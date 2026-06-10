/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * port.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

/*
#cgo CFLAGS: -I${SRCDIR} -I${SRCDIR}/../../../include

#include <stdlib.h>
#include "opendatacon/odc_c_api.h"
#include "gombus_helpers.h"
*/
import "C"
import (
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/simonvetter/modbus"
)

// ---------------------------------------------------------------------------
// Port struct
// ---------------------------------------------------------------------------

// GoModbusClientPort holds all per-port state for a Modbus client (polling)
// port.
//
// Strand-owned state is accessed only from the go_port_* exports below and
// ODC timer callbacks, which all fire on the strand.
//
// Thread-safe fields (client, enabled) use sync/atomic and may be read by
// goroutines without strand serialisation.
//
// WaitGroups (connectWg, eventWg) are used purely for lifetime safety in
// destroy(): goroutines must finish before p.inst is freed by removePort().
type GoModbusClientPort struct {
	name string
	typ  string
	inst unsafe.Pointer

	// Strand-owned state.
	config               *PortConfig
	polled               []PolledPoint
	controls             []ControlPoint
	pollMinMs            int
	maxConcurrentPolls   int
	unitID               uint8
	built                bool
	connected            bool
	reconnectDelayMs     int
	reconnectTimerHandle unsafe.Pointer // ODC one-shot timer handle; nil = none pending

	// Thread-safe atomics — written on strand, read from goroutines.
	client  atomic.Pointer[modbus.ModbusClient]
	enabled atomic.Bool

	// Polling — strand-owned; poll goroutines only call the poll function.
	pollScheduler     PollScheduler
	oldPollSchedulers []PollScheduler // dead schedulers pending Wait() in destroy()

	// Lifetime guards: goroutines use p.inst (logging, ODC schedule calls);
	// destroy() waits on these before removePort() frees p.inst.
	connectWg sync.WaitGroup
	eventWg   sync.WaitGroup
}

// initial and maximum reconnect back-off
const (
	initialReconnectDelayMs = 1000
	maxReconnectDelayMs     = 30000
)

var (
	portMu sync.Mutex
	ports  = make(map[unsafe.Pointer]odcPort)
)

func lookupPort(inst unsafe.Pointer) odcPort {
	portMu.Lock()
	defer portMu.Unlock()
	return ports[inst]
}

// lookupClientPort returns the GoModbusClientPort for inst.  Used by the
// client-specific ODC timer callbacks (reconnect, connect-ok/fail,
// transport-disconnect) which are only ever registered by client ports.
func lookupClientPort(inst unsafe.Pointer) *GoModbusClientPort {
	portMu.Lock()
	defer portMu.Unlock()
	p, _ := ports[inst].(*GoModbusClientPort)
	return p
}

func removePort(inst unsafe.Pointer) {
	portMu.Lock()
	delete(ports, inst)
	portMu.Unlock()
	C.free(inst)
}

func registerPort(p odcPort) unsafe.Pointer {
	// Allocate a unique C-memory sentinel as the opaque handle.
	// C memory never moves, avoiding GC/vet concerns.
	key := C.malloc(C.size_t(1))
	if key == nil {
		panic("C.malloc failed")
	}
	portMu.Lock()
	ports[key] = p
	portMu.Unlock()
	return key
}

func newGoModbusClientPort(name, typ string) *GoModbusClientPort {
	return &GoModbusClientPort{
		name: name,
		typ:  typ,
	}
}

// ---------------------------------------------------------------------------
// odcPort interface implementation wrappers
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) build()   { p.doBuild() }
func (p *GoModbusClientPort) enable()  { p.doEnable() }
func (p *GoModbusClientPort) disable() { p.doDisable() }

// handleEvent is the odcPort interface method.  It runs the pre-flight checks
// that previously lived in go_port_event, then dispatches asynchronously.
func (p *GoModbusClientPort) handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer) {
	if !p.enabled.Load() || !p.connected {
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	// Copy event to Go heap; nil source_port (borrowed C pointer).
	evtCopy := *event
	evtCopy.source_port = nil
	client := p.client.Load() // atomic snapshot before leaving strand

	p.eventWg.Add(1)
	go func() {
		defer p.eventWg.Done()
		p.doHandleEventAsync(&evtCopy, sender, cb, client)
	}()
}

// ---------------------------------------------------------------------------
// Lifecycle — called on the ODC strand
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) doBuild() {
	if p.built {
		return
	}

	cfg, err := parseConfig(p.inst)
	if err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
	if err := validateConfig(p.inst, cfg); err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
	p.config = cfg
	p.unitID = cfg.UnitID
	p.maxConcurrentPolls = cfg.MaxConcurrentPolls

	p.expandPoints()

	client, err := newModbusClient(cfg)
	if err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
	p.client.Store(client)

	p.built = true
	logDebug(p.inst, "doBuild() finished")
}

func (p *GoModbusClientPort) doEnable() {
	if p.enabled.Load() {
		return
	}
	if !p.built {
		logError(p.inst, "enable failed: port not built")
		return
	}

	p.enabled.Store(true)
	p.connected = false
	p.reconnectDelayMs = initialReconnectDelayMs
	p.scheduleReconnect()

	logDebug(p.inst, "doEnable() finished")
}

func (p *GoModbusClientPort) doDisable() {
	if !p.enabled.Load() {
		return
	}

	p.enabled.Store(false)
	p.connected = false

	// Cancel any pending reconnect timer before it fires.
	if p.reconnectTimerHandle != nil {
		C.odc_cancel_timer(p.reconnectTimerHandle)
		p.reconnectTimerHandle = nil
	}

	// Close client so in-flight polls and any connect goroutine fail fast.
	if client := p.client.Load(); client != nil {
		func() { defer func() { recover() }(); client.Close() }()
		p.client.Store(nil)
	}

	p.stopPolling()

	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	logDebug(p.inst, "doDisable() finished")
}

// scheduleReconnect cancels any existing ODC timer and arms a new one-shot
// timer for the current reconnect delay.  Called on strand only.
func (p *GoModbusClientPort) scheduleReconnect() {
	if p.reconnectTimerHandle != nil {
		C.odc_cancel_timer(p.reconnectTimerHandle)
		p.reconnectTimerHandle = nil
	}
	p.reconnectTimerHandle = C.odc_schedule_reconnect(p.inst, C.uint64_t(p.reconnectDelayMs))
}

func (p *GoModbusClientPort) scheduleReconnectWithBackoff() {
	p.reconnectDelayMs *= 2
	if p.reconnectDelayMs > maxReconnectDelayMs {
		p.reconnectDelayMs = maxReconnectDelayMs
	}
	p.scheduleReconnect()
}

// stopPolling shuts down the current poll scheduler without waiting.
// The dead scheduler is appended to oldPollSchedulers; destroy() calls Wait().
// Nilling p.pollScheduler allows a fresh one to be created on the next
// successful reconnect (fixes the reconnect-after-disconnect polling bug).
func (p *GoModbusClientPort) stopPolling() {
	if p.pollScheduler != nil {
		p.pollScheduler.Shutdown()
		p.oldPollSchedulers = append(p.oldPollSchedulers, p.pollScheduler)
		p.pollScheduler = nil
	}
}

// ---------------------------------------------------------------------------
// ODC timer callbacks — fired on the strand
// ---------------------------------------------------------------------------

// onReconnectTimer is invoked when the reconnect delay timer expires.
// It starts a goroutine to attempt the (blocking) TCP connect.
func onReconnectTimer(inst unsafe.Pointer) {
	p := lookupClientPort(inst)
	if p == nil {
		return
	}
	p.reconnectTimerHandle = nil
	if !p.enabled.Load() {
		return
	}
	p.connectWg.Add(1)
	go p.tryConnect()
}

// onConnectOk is invoked (via 0-delay ODC timer) by the connect goroutine
// after a successful TCP connect.
func onConnectOk(inst unsafe.Pointer) {
	p := lookupClientPort(inst)
	if p == nil {
		return
	}
	if !p.enabled.Load() {
		// Port was disabled while the connect was in flight.
		// The tryConnect goroutine may have stored a client; close it.
		if client := p.client.Swap(nil); client != nil {
			func() { defer func() { recover() }(); client.Close() }()
		}
		return
	}

	p.connected = true
	p.reconnectDelayMs = initialReconnectDelayMs

	if len(p.polled) > 0 && p.pollMinMs > 0 && p.pollScheduler == nil {
		p.pollScheduler = newPollScheduler(
			time.Duration(p.pollMinMs)*time.Millisecond,
			p.doPoll,
			p.maxConcurrentPolls,
		)
	}

	publishConnectState(p.inst, C.C_ConnectState_CONNECTED)
	logDebug(p.inst, "onConnectOk() finished")
}

// onConnectFail is invoked (via 0-delay ODC timer) by the connect goroutine
// after a failed TCP connect.
func onConnectFail(inst unsafe.Pointer) {
	p := lookupClientPort(inst)
	if p == nil {
		return
	}
	if !p.enabled.Load() {
		return
	}
	p.scheduleReconnectWithBackoff()
}

// onTransportDisconnect is invoked (via 0-delay ODC timer) by a poll goroutine
// when a transport-level error is detected mid-poll.
func onTransportDisconnect(inst unsafe.Pointer) {
	p := lookupClientPort(inst)
	if p == nil {
		return
	}
	if !p.connected {
		// Guard: multiple concurrent polls may each fire one of these.
		return
	}

	p.connected = false

	if client := p.client.Load(); client != nil {
		func() { defer func() { recover() }(); client.Close() }()
		p.client.Store(nil)
	}

	p.stopPolling()

	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	logWarn(p.inst, "transport disconnected")
	p.reconnectDelayMs = initialReconnectDelayMs
	p.scheduleReconnect()
}

// ---------------------------------------------------------------------------
// Connect goroutine — not on strand
// Uses only atomic fields (enabled, client) and read-only-after-Build fields
// (config, unitID).  Signals result back to strand via 0-delay ODC timers.
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) tryConnect() {
	defer p.connectWg.Done()

	// Create a fresh client (the previous one was nil'd by disable/disconnect).
	client, err := newModbusClient(p.config)
	if err != nil {
		logError(p.inst, "tryConnect: newModbusClient: %v", err)
		if p.enabled.Load() {
			C.odc_schedule_connect_result(p.inst, 0)
		}
		return
	}

	err = openModbusClient(client, p.unitID) // blocking TCP/RTU connect

	if !p.enabled.Load() {
		// Disabled while we were connecting; discard the client.
		func() { defer func() { recover() }(); client.Close() }()
		return
	}

	if err != nil {
		logWarn(p.inst, "connect failed: %v", err)
		func() { defer func() { recover() }(); client.Close() }()
		C.odc_schedule_connect_result(p.inst, 0)
		return
	}

	// Store the connected client atomically before signalling success.
	// onConnectOk will close it if the port was disabled in the meantime.
	p.client.Store(client)
	C.odc_schedule_connect_result(p.inst, 1)
}

// ---------------------------------------------------------------------------
// destroy — called off-strand from ~C_Port after the strand is fully drained
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) destroy() {
	p.doDisable() // no-op if Disable() was already called

	// Wait for in-flight connect goroutines: they call odc_schedule_* with p.inst.
	p.connectWg.Wait()

	// Close any client that tryConnect stored after doDisable already nil'd it.
	// (tryConnect stores the client before posting the connect-result callback;
	// if doDisable raced ahead, that client is never otherwise closed.)
	if client := p.client.Swap(nil); client != nil {
		func() { defer func() { recover() }(); client.Close() }()
	}

	// Wait for in-flight event goroutines: they log with p.inst.
	p.eventWg.Wait()

	// Wait for in-flight poll goroutines: they fail fast on the closed client.
	for _, s := range p.oldPollSchedulers {
		s.Wait()
	}
	p.oldPollSchedulers = nil

	p.client.Store(nil)
	logDebug(p.inst, "destroy() finished")
	removePort(p.inst)
}

// ---------------------------------------------------------------------------
// Config expansion
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) expandPoints() {
	defaultRate := p.config.PollRateMs

	for _, pt := range p.config.Binaries {
		p.polled = append(p.polled, expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_Binary, defaultRate)...)
	}
	for _, pt := range p.config.Analogs {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_Analog, defaultRate)...)
	}
	for _, pt := range p.config.BinaryOutputStatuses {
		p.polled = append(p.polled, expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_BinaryOutputStatus, defaultRate)...)
	}
	for _, pt := range p.config.AnalogOutputStatuses {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_AnalogOutputStatus, defaultRate)...)
	}
	for _, pt := range p.config.OctetStrings {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, 0, 0, "", "", C.C_EventType_OctetString, defaultRate)...)
	}
	for _, pt := range p.config.BinaryControls {
		p.controls = append(p.controls, expandControls(pt, C.C_EventType_ControlRelayOutputBlock)...)
	}
	for _, pt := range p.config.AnalogControls {
		p.controls = append(p.controls, expandAnalogControls(pt)...)
	}

	pollMinMs := defaultRate
	for _, pt := range p.polled {
		if pt.PollRateMs > 0 && pt.PollRateMs < pollMinMs {
			pollMinMs = pt.PollRateMs
		}
	}
	p.pollMinMs = pollMinMs
}

// ---------------------------------------------------------------------------
// Polling — goroutine, not on strand
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) doPoll() error {
	if !p.enabled.Load() {
		return nil
	}

	client := p.client.Load()
	if client == nil {
		return nil
	}

	now := time.Now().UnixNano()

	groups := p.groupDuePoints(now)
	var lastErr error
	for _, g := range groups {
		if err := p.readGroup(g); err != nil {
			lastErr = err
			if isTransportDisconnected(err) {
				logWarn(p.inst, "transport disconnected during poll: %v", err)
				if p.enabled.Load() {
					C.odc_schedule_transport_disconnect(p.inst)
				}
				return err
			}
		}
	}
	return lastErr
}

func isTransportDisconnected(err error) bool {
	if err == nil {
		return false
	}
	s := err.Error()
	return strings.Contains(s, "use of closed network connection") ||
		strings.Contains(s, "connection reset") ||
		strings.Contains(s, "broken pipe") ||
		strings.Contains(s, "EOF") ||
		strings.Contains(s, "connection refused") ||
		strings.Contains(s, "i/o timeout")
}

type pollGroup struct {
	modbusType string
	startAddr  uint16
	count      uint16
	points     []*PolledPoint
}

func (p *GoModbusClientPort) groupDuePoints(now int64) []*pollGroup {
	type typeAddrKey struct {
		t string
		a uint16
	}
	merged := make(map[typeAddrKey]*pollGroup)
	var ordered []*pollGroup

	for i := range p.polled {
		pt := &p.polled[i]
		if !pt.due(now) {
			continue
		}
		pt.lastPollNs.Store(now) // atomic: multiple poll goroutines may run concurrently
		key := typeAddrKey{pt.ModbusType, pt.ModbusAddr}

		if g, ok := merged[key]; ok {
			g.points = append(g.points, pt)
		} else {
			g := &pollGroup{
				modbusType: pt.ModbusType,
				startAddr:  pt.ModbusAddr,
				count:      pt.Count,
			}
			g.points = append(g.points, pt)
			merged[key] = g
			ordered = append(ordered, g)
		}
	}
	return ordered
}

func (p *GoModbusClientPort) readGroup(g *pollGroup) error {
	switch g.modbusType {
	case "Coil":
		return p.readCoils(g)
	case "DiscreteInput":
		return p.readDiscreteInputs(g)
	case "HoldingRegister":
		return p.readRegisters(g, modbus.HOLDING_REGISTER)
	case "InputRegister":
		return p.readRegisters(g, modbus.INPUT_REGISTER)
	default:
		logError(p.inst, "unknown modbus type: %s", g.modbusType)
		return nil
	}
}

func (p *GoModbusClientPort) readCoils(g *pollGroup) error {
	client := p.client.Load()
	if client == nil {
		return nil
	}
	values, err := client.ReadCoils(g.startAddr, g.count)
	if err != nil {
		logError(p.inst, "ReadCoils(%d,%d): %v", g.startAddr, g.count, err)
		return err
	}
	for _, pt := range g.points {
		offset := pt.ModbusAddr - g.startAddr
		val := offset < uint16(len(values)) && values[offset]
		publishBinary(p.inst, pt.ODCIndex, val, pt.ODCType)
	}
	return nil
}

func (p *GoModbusClientPort) readDiscreteInputs(g *pollGroup) error {
	client := p.client.Load()
	if client == nil {
		return nil
	}
	values, err := client.ReadDiscreteInputs(g.startAddr, g.count)
	if err != nil {
		logError(p.inst, "ReadDiscreteInputs(%d,%d): %v", g.startAddr, g.count, err)
		return err
	}
	for _, pt := range g.points {
		offset := pt.ModbusAddr - g.startAddr
		val := offset < uint16(len(values)) && values[offset]
		publishBinary(p.inst, pt.ODCIndex, val, pt.ODCType)
	}
	return nil
}

func (p *GoModbusClientPort) readRegisters(g *pollGroup, regType modbus.RegType) error {
	client := p.client.Load()
	if client == nil {
		return nil
	}
	values, err := client.ReadRegisters(g.startAddr, g.count, regType)
	if err != nil {
		logError(p.inst, "ReadRegisters(%d,%d): %v", g.startAddr, g.count, err)
		return err
	}
	for _, pt := range g.points {
		p.publishRegisterValue(pt, values, g.startAddr)
	}
	return nil
}

func (p *GoModbusClientPort) publishRegisterValue(pt *PolledPoint, regs []uint16, startAddr uint16) {
	switch pt.ODCType {
	case C.C_EventType_Binary, C.C_EventType_BinaryOutputStatus:
		regIdx := pt.ModbusAddr - startAddr
		if int(regIdx) < len(regs) {
			val := (regs[regIdx]>>uint(pt.Bit))&1 != 0
			publishBinary(p.inst, pt.ODCIndex, val, pt.ODCType)
		}

	case C.C_EventType_Analog, C.C_EventType_AnalogOutputStatus:
		p.publishAnalogValue(pt, regs)

	case C.C_EventType_OctetString:
		p.publishOctetValue(pt, regs)
	}
}

func (p *GoModbusClientPort) publishAnalogValue(pt *PolledPoint, regs []uint16) {
	if len(regs) == 0 {
		return
	}
	val := decodeAnalogRegs(regs, pt.Count, pt.Endian, pt.DataType)
	if pt.Scale != 0 {
		val *= pt.Scale
	}
	val += pt.Offset
	publishAnalog(p.inst, pt.ODCIndex, val, pt.ODCType)
}

func (p *GoModbusClientPort) publishOctetValue(pt *PolledPoint, regs []uint16) {
	out := make([]byte, 0, len(regs)*2)
	for _, r := range regs {
		out = append(out, byte(r>>8), byte(r&0xFF))
	}
	if len(out) > 0 {
		publishOctetString(p.inst, pt.ODCIndex, out)
	}
}

// ---------------------------------------------------------------------------
// Controls (inbound commands) — doHandleEventAsync runs in a goroutine
// ---------------------------------------------------------------------------

// doHandleEventAsync is called from a goroutine started by go_port_event.
// It uses only: p.inst (logging, valid via eventWg), p.controls (read-only
// after Build), and the client snapshot passed in.
func (p *GoModbusClientPort) doHandleEventAsync(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer, client *modbus.ModbusClient) {
	if client == nil {
		logWarn(p.inst, "ignoring event: no client (sender=%s)", sender)
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	et := C.odc_GetEventType(event)
	idx := uint64(event.index)

	cp := p.findControl(uint8(et), idx)
	if cp == nil {
		logWarn(p.inst, "no control mapping for event type=%d index=%d (sender=%s)", et, idx, sender)
		invokeStatusCallback(cb, C.C_CommandStatus_NOT_SUPPORTED)
		return
	}

	var err error
	et8 := uint8(et)
	switch cp.ModbusType {
	case "Coil":
		err = handleCoilWrite(client, cp, et8, event)
	case "HoldingRegister":
		err = handleRegisterWrite(client, cp, et8, event)
	default:
		err = fmt.Errorf("unsupported modbus type for control: %s", cp.ModbusType)
	}

	if err != nil {
		logError(p.inst, "control failed: %v", err)
		if isTransportDisconnected(err) && p.enabled.Load() {
			C.odc_schedule_transport_disconnect(p.inst)
		}
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}
	invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
}

func (p *GoModbusClientPort) findControl(odcType uint8, odcIndex uint64) *ControlPoint {
	for i := range p.controls {
		if p.controls[i].ODCType == odcType && p.controls[i].ODCIndex == odcIndex {
			return &p.controls[i]
		}
	}
	return nil
}

// coilActionValue maps an OnAction/OffAction string to a coil boolean.
// "Off" is the only string that means false; everything else (incl. "On") is true.
func coilActionValue(action string) bool {
	return action != "Off"
}

// handleCoilWrite performs a Modbus coil write.  Called from event goroutine.
// For CROB events the control code determines which action (on/off) to apply.
// For Binary events the payload value selects the action.
// cp.OnAction / cp.OffAction (default "On"/"Off") allow logic inversion.
func handleCoilWrite(client *modbus.ModbusClient, cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	switch eventType {
	case C.C_EventType_ControlRelayOutputBlock:
		fc := uint8(C.odc_GetCROBFunctionCode(event))
		var on bool
		switch fc {
		case C.C_ControlCode_LATCH_ON, C.C_ControlCode_CLOSE_PULSE_ON, C.C_ControlCode_PULSE_ON:
			on = coilActionValue(cp.OnAction)
		default: // LATCH_OFF, TRIP_PULSE_ON, PULSE_OFF, NUL, UNDEFINED
			on = coilActionValue(cp.OffAction)
		}
		return client.WriteCoil(cp.ModbusAddr, on)
	case C.C_EventType_Binary:
		if C.odc_GetPayloadBinary(event) != 0 {
			return client.WriteCoil(cp.ModbusAddr, coilActionValue(cp.OnAction))
		}
		return client.WriteCoil(cp.ModbusAddr, coilActionValue(cp.OffAction))
	default:
		return client.WriteCoil(cp.ModbusAddr, coilActionValue(cp.OnAction))
	}
}

// handleRegisterWrite performs a Modbus register write.  Called from event goroutine.
func handleRegisterWrite(client *modbus.ModbusClient, cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	switch eventType {
	case C.C_EventType_AnalogOutputInt16:
		return client.WriteRegister(cp.ModbusAddr, uint16(C.odc_GetAO16Value(event)))
	case C.C_EventType_AnalogOutputInt32:
		val := uint32(C.odc_GetAO32Value(event))
		return client.WriteUint32(cp.ModbusAddr, val)
	case C.C_EventType_AnalogOutputFloat32:
		return client.WriteFloat32(cp.ModbusAddr, float32(C.odc_GetAOF32Value(event)))
	case C.C_EventType_AnalogOutputDouble64:
		return client.WriteFloat64(cp.ModbusAddr, float64(C.odc_GetAOD64Value(event)))
	case C.C_EventType_Analog:
		val := int16(C.odc_GetPayloadAnalog(event))
		return client.WriteRegister(cp.ModbusAddr, uint16(val))
	default:
		return fmt.Errorf("unhandled ODC event type for register control: %d", eventType)
	}
}

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------


func logTrace(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_TRACE) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_TRACE, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logDebug(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_DEBUG) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_DEBUG, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logInfo(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_INFO) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_INFO, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logWarn(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_WARN) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_WARN, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logError(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_ERROR) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_ERROR, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logCritical(inst unsafe.Pointer, format string, args ...interface{}) {
	if C.odc_should_log(inst, C.C_LOG_LEVEL_CRITICAL) == 0 {
		return
	}
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_log(inst, C.C_LOG_LEVEL_CRITICAL, cmsg)
	C.free(unsafe.Pointer(cmsg))
}
