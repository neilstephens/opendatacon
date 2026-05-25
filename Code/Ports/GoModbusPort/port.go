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
	"sync"
	"time"
	"unsafe"

	"github.com/simonvetter/modbus"
)

// ---------------------------------------------------------------------------
// Operation types for the actor channel
// ---------------------------------------------------------------------------

type opType uint8

const (
	opBuild    opType = iota
	opEnable
	opDisable
	opEvent
	opShutdown
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

	ops  chan operation  // buffered — non-blocking post from C API
	done chan struct{}   // closed when run() exits

	// ↓↓↓ state touched only by run() goroutine ↓↓↓
	config    *PortConfig
	client    *modbus.ModbusClient
	polled    []PolledPoint
	controls  []ControlPoint
	pollTick  *time.Ticker
	pollMinMs int
	unitID    uint8
	enabled   bool
	built     bool
}

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

	// pollC is nil when polling is stopped — the select case is dead.
	var pollC <-chan time.Time

	for {
		select {
		case op := <-p.ops:
			switch op.typ {
			case opBuild:
				p.doBuild()
			case opEnable:
				p.doEnable()
				if p.enabled && len(p.polled) > 0 {
					p.pollTick = time.NewTicker(
						time.Duration(p.pollMinMs) * time.Millisecond)
					pollC = p.pollTick.C
				}
			case opDisable:
				p.stopTicker()
				pollC = nil
				p.doDisable()
			case opEvent:
				p.doHandleEvent(&op.event, op.sender, op.cb)
			case opShutdown:
				p.stopTicker()
				pollC = nil
				p.doDisable()
				return
			}

		case <-pollC:
			p.doPoll()
		}
	}
}

func (p *GoModbusPort) stopTicker() {
	if p.pollTick != nil {
		p.pollTick.Stop()
		p.pollTick = nil
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

	p.expandPoints()

	client, err := newModbusClient(cfg)
	if err != nil {
		logError(p.inst, "build failed: %v", err)
		return
	}
	p.client = client

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

	if err := openClient(p.client, p.unitID); err != nil {
		logError(p.inst, "enable failed: %v", err)
		return
	}

	publishConnectState(p.inst, C.C_ConnectState_CONNECTED)
	p.enabled = true
	logInfo(p.inst, "port enabled: %s", p.name)
}

func (p *GoModbusPort) doDisable() {
	if !p.enabled {
		return
	}

	p.enabled = false
	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	logInfo(p.inst, "port disabled: %s", p.name)
}

func (p *GoModbusPort) destroy() {
	p.ops <- operation{typ: opShutdown}
	<-p.done // wait for run() to finish

	// Close client after all goroutines have drained.
	if p.client != nil {
		p.client.Close()
	}

	removePort(p.inst)
	logInfo(p.inst, "port destroyed: %s", p.name)
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

func (p *GoModbusPort) doPoll() {
	now := time.Now().UnixNano()

	groups := p.groupDuePoints(now)
	for i := range groups {
		p.readGroup(&groups[i])
	}
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

func (p *GoModbusPort) readGroup(g *pollGroup) {
	switch g.modbusType {
	case "Coil":
		p.readCoils(g)
	case "DiscreteInput":
		p.readDiscreteInputs(g)
	case "HoldingRegister":
		p.readRegisters(g, modbus.HOLDING_REGISTER)
	case "InputRegister":
		p.readRegisters(g, modbus.INPUT_REGISTER)
	default:
		logError(p.inst, "unknown modbus type: %s", g.modbusType)
	}
}

func (p *GoModbusPort) readCoils(g *pollGroup) {
	values, err := p.client.ReadCoils(g.startAddr, g.count)
	if err != nil {
		logError(p.inst, "ReadCoils(%d,%d): %v", g.startAddr, g.count, err)
		publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
		return
	}
	for _, pt := range g.points {
		offset := pt.ModbusAddr - g.startAddr
		val := offset < uint16(len(values)) && values[offset]
		publishBinary(p.inst, pt.ODCIndex, val, pt.ODCType)
	}
}

func (p *GoModbusPort) readDiscreteInputs(g *pollGroup) {
	values, err := p.client.ReadDiscreteInputs(g.startAddr, g.count)
	if err != nil {
		logError(p.inst, "ReadDiscreteInputs(%d,%d): %v", g.startAddr, g.count, err)
		publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
		return
	}
	for _, pt := range g.points {
		offset := pt.ModbusAddr - g.startAddr
		val := offset < uint16(len(values)) && values[offset]
		publishBinary(p.inst, pt.ODCIndex, val, pt.ODCType)
	}
}

func (p *GoModbusPort) readRegisters(g *pollGroup, regType modbus.RegType) {
	values, err := p.client.ReadRegisters(g.startAddr, g.count, regType)
	if err != nil {
		logError(p.inst, "ReadRegisters(%d,%d): %v", g.startAddr, g.count, err)
		publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
		return
	}
	for _, pt := range g.points {
		p.publishRegisterValue(pt, values)
	}
}

func (p *GoModbusPort) publishRegisterValue(pt *PolledPoint, regs []uint16) {
	offset := (pt.ModbusAddr - p.polled[0].ModbusAddr) // wrong if not first group point
	_ = offset

	switch pt.ODCType {
	case C.C_EventType_Binary, C.C_EventType_BinaryOutputStatus:
		// Bit extraction from register value
		regIdx := (pt.ModbusAddr - pt.ModbusAddr) // 0 relative to the point's own address
		if int(regIdx) < len(regs) {
			val := (regs[regIdx] >> uint(pt.Bit)) & 1 != 0
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
	if !p.enabled {
		logWarn(p.inst, "ignoring event: port not enabled (sender=%s)", sender)
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
	switch eventType {
	case C.C_EventType_ControlRelayOutputBlock:
		return p.client.WriteCoil(cp.ModbusAddr, true)
	case C.C_EventType_Binary:
		return p.client.WriteCoil(cp.ModbusAddr, false)
	default:
		return p.client.WriteCoil(cp.ModbusAddr, true)
	}
}

func (p *GoModbusPort) handleRegisterControl(cp *ControlPoint, eventType uint8, event *C.struct_C_EventInfo) error {
	switch eventType {
	case C.C_EventType_AnalogOutputInt16:
		return p.client.WriteRegister(cp.ModbusAddr, uint16(C.odc_GetAO16Value(event)))
	case C.C_EventType_AnalogOutputInt32:
		val := uint32(C.odc_GetAO32Value(event))
		return p.client.WriteUint32(cp.ModbusAddr, val)
	case C.C_EventType_AnalogOutputFloat32:
		return p.client.WriteFloat32(cp.ModbusAddr, float32(C.odc_GetAOF32Value(event)))
	case C.C_EventType_AnalogOutputDouble64:
		return p.client.WriteFloat64(cp.ModbusAddr, float64(C.odc_GetAOD64Value(event)))
	case C.C_EventType_Analog:
		val := int16(C.odc_GetPayloadAnalog(event))
		return p.client.WriteRegister(cp.ModbusAddr, uint16(val))
	default:
		return fmt.Errorf("unhandled ODC event type for register control: %d", eventType)
	}
}

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------

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
