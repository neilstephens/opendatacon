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
 * modbus_client.go — GoModbusClientPort: lifecycle, select loop, polling,
 *                    and outbound control handling.
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
	"context"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/simonvetter/modbus"
)

// eventChanBufSize is the capacity of the inbound-event channel.
// When full, handleEvent() returns HARDWARE_ERROR immediately to avoid
// blocking the ODC strand.
const eventChanBufSize = 32

// ---------------------------------------------------------------------------
// GoModbusClientPort
// ---------------------------------------------------------------------------

// GoModbusClientPort holds all per-port state for a Modbus TCP/RTU client.
//
// Concurrency model
// ─────────────────
// The ODC strand serialises create(), build(), enable(), disable(), and
// event() at the C++ level (via pSyncStrand->post()).  Go code inside these
// calls never needs its own mutex.
//
// enable() is the last call that touches mutable Go state directly: it
// creates the channels, stores cancel, and spawns the selectLoop goroutine.
//
// After that, all mutable state (reconnect timer, connected flag, poll
// scheduler) is owned exclusively by selectLoop.  External callers interact
// with the loop only through:
//   - cancel()              — context cancellation (disable/destroy)
//   - client.Swap(nil).Close() — unblocks any blocking Modbus read (disable)
//   - eventChan             — inbound ODC events (event())
//   - transportDisconnChan  — transport-error notifications from poll goroutines
//
// Stats reads (go_port_stats_json) use pollStatsVal (atomic.Pointer).
type GoModbusClientPort struct {
	name string
	typ  string
	inst unsafe.Pointer

	// Immutable after build().
	config             *PortConfig
	polled             []PolledPoint
	controls           []ControlPoint
	pollMinMs          int
	maxConcurrentPolls int
	unitID             uint8
	built              bool

	// Thread-safe: written on ODC strand, read atomically from goroutines.
	client  atomic.Pointer[modbus.ModbusClient]
	enabled atomic.Bool

	// Written once by enable() on the ODC strand; stable for all later
	// reads from disable(), destroy(), handleEvent(), and goroutines.
	cancel               context.CancelFunc
	eventChan            chan eventWork
	transportDisconnChan chan struct{}
	wg                   sync.WaitGroup // counts the selectLoop goroutine

	// Written by selectLoop; read atomically by go_port_stats_json.
	pollStatsVal atomic.Pointer[pollScheduler]
}

// initial and maximum reconnect back-off intervals
const (
	initialReconnectDelay = time.Second
	maxReconnectDelay     = 30 * time.Second
)

// connectResult carries the outcome of a tryConnect goroutine back to
// the select loop via connectResultChan.
type connectResult struct {
	ok     bool
	client *modbus.ModbusClient
}

func newGoModbusClientPort(name, typ string) *GoModbusClientPort {
	return &GoModbusClientPort{name: name, typ: typ}
}

// ---------------------------------------------------------------------------
// odcPort interface wrappers — all called on the ODC strand
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) build()   { p.doBuild() }
func (p *GoModbusClientPort) enable()  { p.doEnable() }
func (p *GoModbusClientPort) disable() { p.doDisable() }

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

	// Validate the connection configuration by creating a client without
	// opening it.  The actual connect is deferred to tryConnect() so that
	// p.client is nil until a successful Open() — handleEventWorker uses
	// the nil check as the "not connected yet" guard.
	if _, err := newModbusClient(cfg); err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
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

	ctx, cancel := context.WithCancel(context.Background())
	p.cancel = cancel
	p.eventChan = make(chan eventWork, eventChanBufSize)
	p.transportDisconnChan = make(chan struct{}, 1)

	p.wg.Add(1)
	go p.selectLoop(ctx)

	p.enabled.Store(true)
	logDebug(p.inst, "doEnable() finished")
}

// doDisable cancels the context and unblocks the goroutines; wg.Wait() is
// the hard boundary — after it returns nothing is in-flight.
// publishConnectState(DISCONNECTED) is issued from the goroutine's ctx.Done()
// handler, where the transport actually closes, guarded by the connected flag.
func (p *GoModbusClientPort) doDisable() {
	if !p.enabled.Load() {
		return
	}
	p.enabled.Store(false)

	// Close the active client FIRST to interrupt any blocking Modbus reads
	// immediately, then cancel the context so goroutines see ctx.Done().
	if c := p.client.Swap(nil); c != nil {
		func() { defer func() { recover() }(); c.Close() }()
	}

	if p.cancel != nil {
		p.cancel()
		p.cancel = nil
	}

	p.wg.Wait() // boundary: nothing in-flight after this returns
}

// destroy is called off-strand from ~C_Port() after the ODC strand has been
// fully drained.  ODC guarantees Disable() was called before Destroy(), so
// all goroutines are already gone.  destroy() only frees the handle.
func (p *GoModbusClientPort) destroy() {
	logDebug(p.inst, "destroy() finished")
	removePort(p.inst)
}

// ---------------------------------------------------------------------------
// handleEvent — called on the ODC strand
// ---------------------------------------------------------------------------

// handleEvent deep-copies the event, then performs a non-blocking send onto
// eventChan so the ODC strand is never blocked.
func (p *GoModbusClientPort) handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer) {
	if !p.enabled.Load() {
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	work := eventWork{
		event:  *event, // deep copy
		sender: sender,
		cb:     cb,
	}
	work.event.source_port = nil // borrowed C pointer — nil after copy

	select {
	case p.eventChan <- work:
	default:
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
	}
}

// ---------------------------------------------------------------------------
// Select loop — the single goroutine that owns all mutable port state
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) selectLoop(ctx context.Context) {
	defer p.wg.Done()

	// All mutable state lives here as local variables, not on the struct.
	var (
		reconnDelay = initialReconnectDelay
		reconnTimer = time.NewTimer(reconnDelay) // arms the first connect
		connected   bool
		pollSched   *pollScheduler
		resultChan  chan connectResult // nil when no connect is in flight
		workerWg    sync.WaitGroup
	)

	for {
		select {

		// ── Shutdown ──────────────────────────────────────────────────
		case <-ctx.Done():
			reconnTimer.Stop()
			if pollSched != nil {
				pollSched.Shutdown()
				workerWg.Add(1)
				go func(ps *pollScheduler) {
					defer workerWg.Done()
					ps.Wait()
				}(pollSched)
				pollSched = nil
				p.pollStatsVal.Store(nil)
			}
			if c := p.client.Swap(nil); c != nil {
				func() { defer func() { recover() }(); c.Close() }()
			}
			workerWg.Wait()

			// Drain any events that arrived in eventChan before disable fired
			// and were never dispatched.  Their status callbacks must be invoked
			// exactly once; the WG is still alive here so the callbacks are safe.
		DRAIN:
			for {
				select {
				case w := <-p.eventChan:
					invokeStatusCallback(w.cb, C.C_CommandStatus_HARDWARE_ERROR)
				default:
					break DRAIN
				}
			}

			// Publish DISCONNECTED when the transport actually closes — here,
			// guarded by the connected flag so we don't double-publish if a
			// transport disconnect already fired before disable was called.
			if connected {
				connected = false
				publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
			}
			return

		// ── Reconnect timer fired — start a connect goroutine ─────────
		// resultChan is nil while no connect is in flight; selecting on a
		// nil channel blocks forever, so only one connect goroutine runs
		// at a time.
		case <-reconnTimer.C:
			resultChan = make(chan connectResult, 1)
			workerWg.Add(1)
			go func() {
				defer workerWg.Done()
				p.tryConnect(ctx, resultChan)
			}()

		// ── Connect goroutine result ───────────────────────────────────
		case r := <-resultChan:
			resultChan = nil
			if r.ok {
				p.client.Store(r.client)
				connected = true
				reconnDelay = initialReconnectDelay

				if len(p.polled) > 0 && p.pollMinMs > 0 {
					ps := newPollScheduler(
						time.Duration(p.pollMinMs)*time.Millisecond,
						p.doPoll,
						p.maxConcurrentPolls,
					)
					pollSched = ps
					p.pollStatsVal.Store(ps)
				}
				publishConnectState(p.inst, C.C_ConnectState_CONNECTED)
				logDebug(p.inst, "connected")
			} else {
				reconnDelay *= 2
				if reconnDelay > maxReconnectDelay {
					reconnDelay = maxReconnectDelay
				}
				reconnTimer = time.NewTimer(reconnDelay)
				logWarn(p.inst, "connect failed, retrying in %v", reconnDelay)
			}

		// ── Transport disconnect reported by a poll/event goroutine ───
		case <-p.transportDisconnChan:
			if !connected {
				continue // de-duplicate: a second goroutine may race
			}
			connected = false

			if c := p.client.Swap(nil); c != nil {
				func() { defer func() { recover() }(); c.Close() }()
			}
			if pollSched != nil {
				pollSched.Shutdown()
				workerWg.Add(1)
				go func(ps *pollScheduler) {
					defer workerWg.Done()
					ps.Wait()
				}(pollSched)
				pollSched = nil
				p.pollStatsVal.Store(nil)
			}

			publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
			logWarn(p.inst, "transport disconnected, scheduling reconnect")
			reconnDelay = initialReconnectDelay
			reconnTimer = time.NewTimer(reconnDelay)

		// ── Inbound ODC event (control write) ─────────────────────────
		case work := <-p.eventChan:
			workerWg.Add(1)
			go func(w eventWork) {
				defer workerWg.Done()
				p.handleEventWorker(w)
			}(work)
		}
	}
}

// ---------------------------------------------------------------------------
// tryConnect goroutine — not on strand; result sent via channel
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) tryConnect(ctx context.Context, resultChan chan<- connectResult) {
	client, err := newModbusClient(p.config)
	if err != nil {
		logError(p.inst, "tryConnect: newModbusClient: %v", err)
		select {
		case resultChan <- connectResult{ok: false}:
		case <-ctx.Done():
		}
		return
	}

	err = openModbusClient(client, p.unitID) // blocking dial

	// Check context before touching shared state or sending result.
	select {
	case <-ctx.Done():
		func() { defer func() { recover() }(); client.Close() }()
		return
	default:
	}

	if err != nil {
		logWarn(p.inst, "connect failed: %v", err)
		func() { defer func() { recover() }(); client.Close() }()
		select {
		case resultChan <- connectResult{ok: false}:
		case <-ctx.Done():
		}
		return
	}

	// Store before sending so the select loop can find the client if ctx
	// is cancelled between the send and the receive.
	p.client.Store(client)
	select {
	case resultChan <- connectResult{ok: true, client: client}:
	case <-ctx.Done():
		// Port was disabled in the tiny window after Store; clean up.
		p.client.Store(nil)
		func() { defer func() { recover() }(); client.Close() }()
	}
}

// ---------------------------------------------------------------------------
// Polling — goroutine spawned by pollScheduler; not on strand
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) doPoll() error {
	if !p.enabled.Load() {
		return nil
	}
	if C.odc_in_demand(p.inst) == 0 {
		return nil // no subscribers — skip poll to avoid wasteful Modbus traffic
	}
	client := p.client.Load()
	if client == nil {
		return nil
	}

	now := time.Now().UnixNano()
	groups := p.groupDuePoints(now)

	var lastErr error
	for _, g := range groups {
		if err := p.readGroup(g, client); err != nil {
			lastErr = err
			if isTransportDisconnected(err) {
				logWarn(p.inst, "transport disconnected during poll: %v", err)
				select {
				case p.transportDisconnChan <- struct{}{}:
				default: // already queued — one notification is enough
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

// ---------------------------------------------------------------------------
// Poll group helpers
// ---------------------------------------------------------------------------

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
		pt.lastPollNs.Store(now)
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

func (p *GoModbusClientPort) readGroup(g *pollGroup, client *modbus.ModbusClient) error {
	switch g.modbusType {
	case "Coil":
		return p.readCoils(g, client)
	case "DiscreteInput":
		return p.readDiscreteInputs(g, client)
	case "HoldingRegister":
		return p.readRegisters(g, client, modbus.HOLDING_REGISTER)
	case "InputRegister":
		return p.readRegisters(g, client, modbus.INPUT_REGISTER)
	default:
		logError(p.inst, "unknown modbus type: %s", g.modbusType)
		return nil
	}
}

func (p *GoModbusClientPort) readCoils(g *pollGroup, client *modbus.ModbusClient) error {
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

func (p *GoModbusClientPort) readDiscreteInputs(g *pollGroup, client *modbus.ModbusClient) error {
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

func (p *GoModbusClientPort) readRegisters(g *pollGroup, client *modbus.ModbusClient, regType modbus.RegType) error {
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
// handleEventWorker — goroutine spawned by the select loop
// ---------------------------------------------------------------------------

func (p *GoModbusClientPort) handleEventWorker(work eventWork) {
	// Copy event to a standalone local variable before any C calls.
	// &work.event is inside an eventWork that also has a sender string
	// (a Go pointer); CGO rejects pointers into mixed Go/C allocations.
	event := work.event

	client := p.client.Load()
	if client == nil {
		logWarn(p.inst, "ignoring event: no client (sender=%s)", work.sender)
		invokeStatusCallback(work.cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	et := C.odc_GetEventType(&event)
	idx := uint64(event.index)

	cp := p.findControl(uint8(et), idx)
	if cp == nil {
		logWarn(p.inst, "no control mapping for event type=%d index=%d (sender=%s)", et, idx, work.sender)
		invokeStatusCallback(work.cb, C.C_CommandStatus_NOT_SUPPORTED)
		return
	}

	var err error
	switch cp.ModbusType {
	case "Coil":
		err = handleCoilWrite(client, cp, uint8(et), &event)
	case "HoldingRegister":
		err = handleRegisterWrite(client, cp, uint8(et), &event)
	default:
		err = fmt.Errorf("unsupported modbus type for control: %s", cp.ModbusType)
	}

	if err != nil {
		logError(p.inst, "control failed: %v", err)
		if isTransportDisconnected(err) {
			select {
			case p.transportDisconnChan <- struct{}{}:
			default:
			}
		}
		invokeStatusCallback(work.cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}
	invokeStatusCallback(work.cb, C.C_CommandStatus_SUCCESS)
}

func (p *GoModbusClientPort) findControl(odcType uint8, odcIndex uint64) *ControlPoint {
	for i := range p.controls {
		if p.controls[i].ODCType == odcType && p.controls[i].ODCIndex == odcIndex {
			return &p.controls[i]
		}
	}
	return nil
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
	for i := range p.polled {
		if p.polled[i].PollRateMs > 0 && p.polled[i].PollRateMs < pollMinMs {
			pollMinMs = p.polled[i].PollRateMs
		}
	}
	p.pollMinMs = pollMinMs
}

// ---------------------------------------------------------------------------
// Coil / register write helpers — called from handleEventWorker goroutine
// ---------------------------------------------------------------------------

// coilActionValue maps an OnAction/OffAction string to a coil boolean.
func coilActionValue(action string) bool {
	return action != "Off"
}

func handleCoilWrite(client *modbus.ModbusClient, cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	switch eventType {
	case C.C_EventType_ControlRelayOutputBlock:
		fc := uint8(C.odc_GetCROBFunctionCode(event))
		var on bool
		switch fc {
		case C.C_ControlCode_LATCH_ON, C.C_ControlCode_CLOSE_PULSE_ON, C.C_ControlCode_PULSE_ON:
			on = coilActionValue(cp.OnAction)
		default:
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

func handleRegisterWrite(client *modbus.ModbusClient, cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	switch eventType {
	case C.C_EventType_AnalogOutputInt16:
		return client.WriteRegister(cp.ModbusAddr, uint16(C.odc_GetAO16Value(event)))
	case C.C_EventType_AnalogOutputInt32:
		return client.WriteUint32(cp.ModbusAddr, uint32(C.odc_GetAO32Value(event)))
	case C.C_EventType_AnalogOutputFloat32:
		return client.WriteFloat32(cp.ModbusAddr, float32(C.odc_GetAOF32Value(event)))
	case C.C_EventType_AnalogOutputDouble64:
		return client.WriteFloat64(cp.ModbusAddr, float64(C.odc_GetAOD64Value(event)))
	case C.C_EventType_Analog:
		return client.WriteRegister(cp.ModbusAddr, uint16(int16(C.odc_GetPayloadAnalog(event))))
	default:
		return fmt.Errorf("unhandled ODC event type for register control: %d", eventType)
	}
}

// ---------------------------------------------------------------------------
// Modbus client factory — shared by client and server (RTU parity conversion)
// ---------------------------------------------------------------------------

func newModbusClient(cfg *PortConfig) (*modbus.ModbusClient, error) {
	var url string
	if cfg.TCP != nil {
		url = "tcp://" + cfg.TCP.Address
	} else if cfg.RTU != nil {
		url = "rtu://" + cfg.RTU.Port
	} else {
		return nil, fmt.Errorf("no TCP or RTU connection config")
	}

	clientConf := &modbus.ClientConfiguration{
		URL:     url,
		Timeout: time.Duration(cfg.TimeoutMs) * time.Millisecond,
	}
	if cfg.RTU != nil {
		clientConf.Speed = uint(cfg.RTU.BaudRate)
		clientConf.DataBits = uint(cfg.RTU.DataBits)
		clientConf.Parity = modbusParityFromString(cfg.RTU.Parity)
		clientConf.StopBits = uint(cfg.RTU.StopBits)
	}

	client, err := modbus.NewClient(clientConf)
	if err != nil {
		return nil, fmt.Errorf("modbus.NewClient: %w", err)
	}
	return client, nil
}

func openModbusClient(client *modbus.ModbusClient, unitID uint8) error {
	if err := client.SetUnitId(unitID); err != nil {
		return fmt.Errorf("SetUnitId: %w", err)
	}
	return client.Open()
}

// modbusParityFromString converts the config parity string ("E"/"O"/"N") to
// the modbus library constant.  Used by both client and server.
func modbusParityFromString(parity string) uint {
	switch parity {
	case "E", "e":
		return modbus.PARITY_EVEN
	case "O", "o":
		return modbus.PARITY_ODD
	default:
		return modbus.PARITY_NONE
	}
}
