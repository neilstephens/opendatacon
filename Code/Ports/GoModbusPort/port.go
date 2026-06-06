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
// Operation types for the actor channel
// ---------------------------------------------------------------------------

type opType uint8

const (
	opBuild        opType = iota
	opEnable
	opDisable
	opEvent
	opShutdown
	opReconnect // internal: attempt to reconnect
)

type operation struct {
	typ    opType
	event  C.struct_C_EventInfo // copied by value
	sender string
	cb     unsafe.Pointer
}

// ---------------------------------------------------------------------------
// Port struct — state is owned exclusively by run()
// ---------------------------------------------------------------------------

type GoModbusPort struct {
	name string
	typ  string
	inst unsafe.Pointer

	ops  chan operation // buffered — non-blocking post from C API
	done chan struct{}  // closed when run() exits

	// state touched only by run()
	config              *PortConfig
	client              atomic.Pointer[modbus.ModbusClient]
	polled              []PolledPoint
	controls            []ControlPoint
	pollMinMs           int
	maxConcurrentPolls  int
	unitID              uint8
	enabled             bool
	built               bool
	connected           bool         // transport-level connection state
	reconnectScheduled  bool         // prevent duplicate reconnect timers
	reconnectTimer      *time.Timer  // handle for the pending reconnect AfterFunc
	reconnectDelayMs    int          // current reconnect backoff delay

	// polling (owned by pollScheduler goroutine)
	pollScheduler PollScheduler
}

// initial reconnect delay and max delay
const (
	initialReconnectDelayMs = 1000
	maxReconnectDelayMs     = 30000
)

var (
	portMu sync.Mutex
	ports  = make(map[unsafe.Pointer]*GoModbusPort)
)

func lookupPort(inst unsafe.Pointer) *GoModbusPort {
	portMu.Lock()
	defer portMu.Unlock()
	return ports[inst]
}

func removePort(inst unsafe.Pointer) {
	portMu.Lock()
	delete(ports, inst)
	portMu.Unlock()
	C.free(inst)
}

func registerPort(p *GoModbusPort) unsafe.Pointer {
	// Allocate a unique C-memory sentinel as the opaque handle.
	// This avoids any GC or vet concerns since C memory never moves.
	key := C.malloc(C.size_t(1))
	if key == nil {
		panic("C.malloc failed")
	}
	portMu.Lock()
	ports[key] = p
	portMu.Unlock()
	return key
}

func newGoModbusPort(name, typ string, inst unsafe.Pointer) *GoModbusPort {
	p := &GoModbusPort{
		name: name,
		typ:  typ,
		inst: inst,
		ops:  make(chan operation, 100),
		done: make(chan struct{}),
	}
	go p.run()
	return p
}

// ---------------------------------------------------------------------------
// Actor event loop — single goroutine owns all state
// ---------------------------------------------------------------------------

func (p *GoModbusPort) run() {
	defer close(p.done)

	for {
		select {
		case op := <-p.ops:
			switch op.typ {
			case opBuild:
				p.doBuild()
			case opEnable:
				p.doEnable()
			case opDisable:
				p.doDisable()
			case opEvent:
				p.doHandleEvent(&op.event, op.sender, op.cb)
			case opReconnect:
				p.doReconnect()
			case opShutdown:
				p.doDisable()
				return
			}
		}
	}
}

func (p *GoModbusPort) stopPolling() {
	if p.pollScheduler != nil {
		p.pollScheduler.Shutdown()
		// Don't Wait() here — in-flight polls will fail fast on the closed client.
		// destroy() calls Wait() after <-p.done so the goroutines are joined before
		// dlclose.  Do NOT nil p.pollScheduler here; destroy() needs the reference.
	}
}

// ---------------------------------------------------------------------------
// Lifecycle operations — called only from run()
// ---------------------------------------------------------------------------

func (p *GoModbusPort) doBuild() {
	if p.built {
		return
	}

	cfg, err := parseConfig(p.inst)
	if err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
	if err := validateConfig(cfg); err != nil {
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
	logInfo(p.inst, "port built: %s/%s", p.name, p.typ)
}

func (p *GoModbusPort) doEnable() {
	if p.enabled {
		return
	}
	if !p.built {
		logError(p.inst, "enable failed: port not built")
		return
	}

	p.enabled = true
	p.connected = false
	p.reconnectDelayMs = initialReconnectDelayMs
	p.reconnectScheduled = false

	// Start the reconnection process asynchronously
	p.scheduleReconnect()

	logInfo(p.inst, "port enabled: %s", p.name)
}

func (p *GoModbusPort) doDisable() {
	if !p.enabled {
		return
	}

	p.enabled = false
	p.connected = false
	p.reconnectScheduled = false
	// Stop any pending reconnect timer so no goroutine fires after dlclose.
	if p.reconnectTimer != nil {
		p.reconnectTimer.Stop()
		p.reconnectTimer = nil
	}

	// Close client FIRST so in-flight polls fail fast
	client := p.client.Load()
	if client != nil {
		func() {
			defer func() {
				recover() // ignore panic if transport was never opened
			}()
			client.Close()
		}()
		p.client.Store(nil)
	}

	// Then stop scheduler (no wait - polls will fail on closed connection)
	p.stopPolling()

	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	logInfo(p.inst, "port disabled: %s", p.name)
}

func (p *GoModbusPort) scheduleReconnect() {
	if p.reconnectScheduled || !p.enabled {
		return
	}
	p.reconnectScheduled = true
	delay := time.Duration(p.reconnectDelayMs) * time.Millisecond
	p.reconnectTimer = time.AfterFunc(delay, func() {
		p.ops <- operation{typ: opReconnect}
	})
}

func (p *GoModbusPort) doReconnect() {
	p.reconnectScheduled = false

	if !p.enabled {
		return
	}

	client := p.client.Load()
	if client == nil {
		// Client was destroyed, recreate it
		if p.config == nil {
			logError(p.inst, "reconnect failed: no config")
			p.scheduleReconnect()
			return
		}
		newClient, err := newModbusClient(p.config)
		if err != nil {
			logError(p.inst, "reconnect failed to create client: %v", err)
			p.scheduleReconnectWithBackoff()
			return
		}
		p.client.Store(newClient)
		client = newClient
	}

	if err := openClient(client, p.unitID); err != nil {
		logWarn(p.inst, "reconnect failed: %v", err)
		p.scheduleReconnectWithBackoff()
		return
	}

	// Connection successful
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
	logInfo(p.inst, "port reconnected: %s", p.name)
}

func (p *GoModbusPort) scheduleReconnectWithBackoff() {
	p.reconnectDelayMs *= 2
	if p.reconnectDelayMs > maxReconnectDelayMs {
		p.reconnectDelayMs = maxReconnectDelayMs
	}
	p.scheduleReconnect()
}

func (p *GoModbusPort) destroy() {
	p.ops <- operation{typ: opShutdown}
	<-p.done // wait for run() to finish

	// Wait for any in-flight polls to complete (they should fail fast on closed connection)
	if p.pollScheduler != nil {
		p.pollScheduler.Wait()
		p.pollScheduler = nil
	}

	// Client is closed by doDisable in the actor loop. Don't close again.
	// Just clear the reference.
	p.client.Store(nil)

	logInfo(p.inst, "port destroyed: %s", p.name)
	removePort(p.inst)
}

// ---------------------------------------------------------------------------
// Config expansion
// ---------------------------------------------------------------------------

func (p *GoModbusPort) expandPoints() {
	defaultRate := p.config.PollRateMs

	for _, pt := range p.config.Binaries {
		p.polled = append(p.polled, expandPolledPoint(pt, 0, 0, "", C.C_EventType_Binary, defaultRate)...)
	}
	for _, pt := range p.config.Analogs {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, C.C_EventType_Analog, defaultRate)...)
	}
	for _, pt := range p.config.BinaryOutputStatuses {
		p.polled = append(p.polled, expandPolledPoint(pt, 0, 0, "", C.C_EventType_BinaryOutputStatus, defaultRate)...)
	}
	for _, pt := range p.config.AnalogOutputStatuses {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, C.C_EventType_AnalogOutputStatus, defaultRate)...)
	}
	for _, pt := range p.config.OctetStrings {
		p.polled = append(p.polled, expandPolledPoint(pt.PointConfig, 0, 0, "", C.C_EventType_OctetString, defaultRate)...)
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
// Polling
// ---------------------------------------------------------------------------

func (p *GoModbusPort) doPoll() error {
	if !p.enabled || !p.connected {
		return nil
	}

	client := p.client.Load()
	if client == nil {
		return nil
	}

	now := time.Now().UnixNano()

	groups := p.groupDuePoints(now)
	var lastErr error
	for i := range groups {
		if err := p.readGroup(&groups[i]); err != nil {
			lastErr = err
			if isTransportDisconnected(err) {
				logWarn(p.inst, "transport disconnected during poll: %v", err)
				p.handleDisconnect()
				return err
			}
		}
	}
	return lastErr
}

func (p *GoModbusPort) handleDisconnect() {
	if !p.connected {
		return
	}
	p.connected = false

	// Close client FIRST so in-flight polls fail fast
	client := p.client.Load()
	if client != nil {
		func() {
			defer func() {
				recover()
			}()
			client.Close()
		}()
		p.client.Store(nil)
	}

	// Then stop scheduler (no wait)
	p.stopPolling()

	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	p.scheduleReconnect()
}

func isTransportDisconnected(err error) bool {
	if err == nil {
		return false
	}
	errStr := err.Error()
	return strings.Contains(errStr, "use of closed network connection") ||
		strings.Contains(errStr, "connection reset") ||
		strings.Contains(errStr, "broken pipe") ||
		strings.Contains(errStr, "EOF") ||
		strings.Contains(errStr, "connection refused") ||
		strings.Contains(errStr, "i/o timeout")
}

type pollGroup struct {
	modbusType string
	startAddr  uint16
	count      uint16
	points     []*PolledPoint
}

func (p *GoModbusPort) groupDuePoints(now int64) []pollGroup {
	type typeAddrKey struct {
		t string
		a uint16
	}
	merged := make(map[typeAddrKey]*pollGroup)
	var ordered []pollGroup

	for i := range p.polled {
		pt := &p.polled[i]
		if !pt.due(now) {
			continue
		}
		pt.lastPollNs = now
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
			ordered = append(ordered, *g)
		}
	}
	return ordered
}

func (p *GoModbusPort) readGroup(g *pollGroup) error {
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

func (p *GoModbusPort) readCoils(g *pollGroup) error {
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

func (p *GoModbusPort) readDiscreteInputs(g *pollGroup) error {
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

func (p *GoModbusPort) readRegisters(g *pollGroup, regType modbus.RegType) error {
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
		p.publishRegisterValue(pt, values)
	}
	return nil
}

func (p *GoModbusPort) publishRegisterValue(pt *PolledPoint, regs []uint16) {
	offset := (pt.ModbusAddr - p.polled[0].ModbusAddr) // wrong if not first group point
	_ = offset

	switch pt.ODCType {
	case C.C_EventType_Binary, C.C_EventType_BinaryOutputStatus:
		// Bit extraction from register value
		regIdx := (pt.ModbusAddr - pt.ModbusAddr) // 0 relative to the point's own address
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

func (p *GoModbusPort) publishAnalogValue(pt *PolledPoint, regs []uint16) {
	if len(regs) == 0 {
		return
	}

	var val float64
	switch pt.Count {
	case 0, 1:
		val = float64(int16(regs[0] & 0xFFFF))
	case 2:
		if len(regs) >= 2 {
			raw := uint32(regs[0])<<16 | uint32(regs[1])
			val = float64(int32(raw))
		} else {
			val = float64(int16(regs[0] & 0xFFFF))
		}
	default:
		val = float64(int16(regs[0] & 0xFFFF))
	}

	if pt.Scale != 0 {
		val *= pt.Scale
	}
	val += pt.Offset
	publishAnalog(p.inst, pt.ODCIndex, val, pt.ODCType)
}

func (p *GoModbusPort) publishOctetValue(pt *PolledPoint, regs []uint16) {
	// Convert []uint16 to []byte, big-endian per register
	out := make([]byte, 0, len(regs)*2)
	for _, r := range regs {
		out = append(out, byte(r>>8), byte(r&0xFF))
	}
	if len(out) > 0 {
		publishOctetString(p.inst, pt.ODCIndex, out)
	}
}

// ---------------------------------------------------------------------------
// Controls (inbound commands)
// ---------------------------------------------------------------------------

func (p *GoModbusPort) doHandleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer) {
	// Copy C struct to stack to avoid cgo "Go pointer to unpinned Go pointer" error
	// (the operation struct lives on Go heap and contains a Go string)
	var cEvent C.struct_C_EventInfo = *event
	event = &cEvent

	if !p.enabled {
		logWarn(p.inst, "ignoring event: port not enabled (sender=%s)", sender)
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	if !p.connected {
		logWarn(p.inst, "ignoring event: port not connected (sender=%s)", sender)
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
		err = p.handleCoilControl(cp, et8, event)
	case "HoldingRegister":
		err = p.handleRegisterControl(cp, et8, event)
	default:
		err = fmt.Errorf("unsupported modbus type for control: %s", cp.ModbusType)
	}

	if err != nil {
		logError(p.inst, "control failed: %v", err)
		if isTransportDisconnected(err) {
			p.handleDisconnect()
		}
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}
	invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
}

func (p *GoModbusPort) findControl(odcType uint8, odcIndex uint64) *ControlPoint {
	for i := range p.controls {
		if p.controls[i].ODCType == odcType && p.controls[i].ODCIndex == odcIndex {
			return &p.controls[i]
		}
	}
	return nil
}

func (p *GoModbusPort) handleCoilControl(cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	// Copy to stack for cgo safety
	var cEvent C.struct_C_EventInfo = *event
	event = &cEvent

	client := p.client.Load()
	if client == nil {
		return fmt.Errorf("client not available")
	}
	switch eventType {
	case C.C_EventType_ControlRelayOutputBlock:
		return client.WriteCoil(cp.ModbusAddr, true)
	case C.C_EventType_Binary:
		return client.WriteCoil(cp.ModbusAddr, false)
	default:
		return client.WriteCoil(cp.ModbusAddr, true)
	}
}

func (p *GoModbusPort) handleRegisterControl(cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	// Copy to stack for cgo safety
	var cEvent C.struct_C_EventInfo = *event
	event = &cEvent

	client := p.client.Load()
	if client == nil {
		return fmt.Errorf("client not available")
	}
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
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_TRACE, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logDebug(inst unsafe.Pointer, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_DEBUG, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logInfo(inst unsafe.Pointer, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_INFO, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logWarn(inst unsafe.Pointer, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_WARN, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logError(inst unsafe.Pointer, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_ERROR, cmsg)
	C.free(unsafe.Pointer(cmsg))
}

func logCritical(inst unsafe.Pointer, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	cmsg := C.CString(msg)
	C.odc_Log(inst, C.C_LOG_LEVEL_CRITICAL, cmsg)
	C.free(unsafe.Pointer(cmsg))
}
