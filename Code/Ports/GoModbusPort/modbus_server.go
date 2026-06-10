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
 * modbus_server.go
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
	"sync"
	"sync/atomic"
	"unsafe"

	"github.com/simonvetter/modbus"
)

// ---------------------------------------------------------------------------
// Data store
// ---------------------------------------------------------------------------

// dataStore is a thread-safe in-memory Modbus register store.
// The server pre-populates it with zero values for all configured addresses;
// subsequent ODC events and Modbus client writes update it in place.
type dataStore struct {
	coils    map[uint16]bool
	di       map[uint16]bool   // discrete inputs (read-only by Modbus clients)
	hr       map[uint16]uint16 // holding registers
	ir       map[uint16]uint16 // input registers (read-only by Modbus clients)
	mu       sync.RWMutex
}

func newDataStore() *dataStore {
	return &dataStore{
		coils: make(map[uint16]bool),
		di:    make(map[uint16]bool),
		hr:    make(map[uint16]uint16),
		ir:    make(map[uint16]uint16),
	}
}

// ---------------------------------------------------------------------------
// Lookup types used to build the reverse maps
// ---------------------------------------------------------------------------

// serverReadTarget is stored in the ODC→store map.  When an ODC event arrives
// with the matching index, its value is encoded and written to the store.
type serverReadTarget struct {
	modbusType string
	modbusAddr uint16
	count      uint16
	scale      float64
	offset     float64
	endian     string
	dataType   string
}

// serverWriteTarget is stored in the store→ODC map.  When a Modbus client
// writes to the base address, the server publishes an ODC event.
type serverWriteTarget struct {
	odcIndex uint64
	odcType  uint8  // C_EventType_Binary or one of the AnalogOutput* types
	count    uint16 // number of registers (HR only; always 1 for coils)
	endian   string
	dataType string
}

// ---------------------------------------------------------------------------
// GoModbusServerPort
// ---------------------------------------------------------------------------

// GoModbusServerPort implements a Modbus TCP/RTU server (slave).
//
// Data-flow summary:
//
//	Incoming ODC events (from connectors)
//	    → handleEvent()
//	    → data store update
//	    → Modbus client reads back the new value
//
//	Modbus client writes (coil / holding-register)
//	    → handler callback
//	    → data store update
//	    → ODC event published
//
// The server never initiates connections; it listens passively and accepts
// any number of simultaneous Modbus TCP client connections.
type GoModbusServerPort struct {
	name string
	typ  string
	inst unsafe.Pointer

	config *ServerPortConfig

	store *dataStore

	// ODC index → store update info.
	// Keyed by ODC index; multiple ODC event types can update the same entry.
	// Separate maps for binary and analog to avoid type ambiguity.
	binaryUpdateMap map[uint64]serverReadTarget // Binary / BinaryOutputStatus / CROB
	analogUpdateMap map[uint64]serverReadTarget // Analog / AnalogOutputStatus / AO*
	octetUpdateMap  map[uint64]serverReadTarget // OctetString

	// Modbus address → ODC event info.
	coilWriteMap map[uint16]serverWriteTarget // coil addr → Binary publish
	hrWriteMap   map[uint16]serverWriteTarget // HR base addr → AnalogOutput* publish

	server *modbus.ModbusServer

	built   bool
	enabled atomic.Bool

	eventWg sync.WaitGroup
}

func newGoModbusServerPort(name, typ string) *GoModbusServerPort {
	return &GoModbusServerPort{
		name: name,
		typ:  typ,
	}
}

// ---------------------------------------------------------------------------
// odcPort interface implementation
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) build()   { p.doBuild() }
func (p *GoModbusServerPort) enable()  { p.doEnable() }
func (p *GoModbusServerPort) disable() { p.doDisable() }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) doBuild() {
	if p.built {
		return
	}

	cfg, err := parseServerConfig(p.inst)
	if err != nil {
		logError(p.inst, "server build failed: %v", err)
		return
	}
	if err := validateServerConfig(p.inst, cfg); err != nil {
		logError(p.inst, "server build failed: %v", err)
		return
	}
	p.config = cfg
	p.store = newDataStore()
	p.binaryUpdateMap = make(map[uint64]serverReadTarget)
	p.analogUpdateMap = make(map[uint64]serverReadTarget)
	p.octetUpdateMap = make(map[uint64]serverReadTarget)
	p.coilWriteMap = make(map[uint16]serverWriteTarget)
	p.hrWriteMap = make(map[uint16]serverWriteTarget)

	p.buildMaps()

	// Create the simonvetter/modbus server with this port as the handler.
	// Note: the simonvetter/modbus library does not yet implement RTU server
	// mode (ReadRequest returns "unimplemented").  TCP server is fully
	// supported.  RTU server support can be added when the library provides
	// it or via an alternative serial-port implementation.
	if cfg.RTU != nil {
		logError(p.inst, "server build failed: RTU server mode is not supported by the underlying modbus library")
		return
	}

	serverConf := &modbus.ServerConfiguration{
		URL: "tcp://" + cfg.TCP.Listen,
	}

	srv, err := modbus.NewServer(serverConf, p)
	if err != nil {
		logError(p.inst, "server build failed: modbus.NewServer: %v", err)
		return
	}
	p.server = srv

	p.built = true
	logDebug(p.inst, "server doBuild() finished")
}

func (p *GoModbusServerPort) doEnable() {
	if p.enabled.Load() {
		return
	}
	if !p.built {
		logError(p.inst, "server enable failed: port not built")
		return
	}

	p.enabled.Store(true)

	if err := p.server.Start(); err != nil {
		logError(p.inst, "server Start() failed: %v", err)
		p.enabled.Store(false)
		return
	}

	publishConnectState(p.inst, C.C_ConnectState_CONNECTED)
	logDebug(p.inst, "server doEnable() finished")
}

func (p *GoModbusServerPort) doDisable() {
	if !p.enabled.Load() {
		return
	}
	p.enabled.Store(false)

	if p.server != nil {
		p.server.Stop()
	}

	publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
	logDebug(p.inst, "server doDisable() finished")
}

func (p *GoModbusServerPort) destroy() {
	p.doDisable()
	p.eventWg.Wait()
	logDebug(p.inst, "server destroy() finished")
	removePort(p.inst)
}

// ---------------------------------------------------------------------------
// Map construction (called once in doBuild)
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) buildMaps() {
	cfg := p.config

	// Binaries → coil / discrete-input store, updated by ODC Binary events.
	for _, pt := range cfg.Binaries {
		pts := expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_Binary, 0)
		for _, pp := range pts {
			p.binaryUpdateMap[pp.ODCIndex] = serverReadTarget{
				modbusType: pp.ModbusType,
				modbusAddr: pp.ModbusAddr,
				count:      pp.Count,
			}
			p.preAllocStore(pp.ModbusType, pp.ModbusAddr, pp.Count)
		}
	}

	// BinaryOutputStatuses → same semantics as Binaries for the server.
	for _, pt := range cfg.BinaryOutputStatuses {
		pts := expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_BinaryOutputStatus, 0)
		for _, pp := range pts {
			// Only register if not already mapped (Binaries takes priority).
			if _, exists := p.binaryUpdateMap[pp.ODCIndex]; !exists {
				p.binaryUpdateMap[pp.ODCIndex] = serverReadTarget{
					modbusType: pp.ModbusType,
					modbusAddr: pp.ModbusAddr,
					count:      pp.Count,
				}
			}
			p.preAllocStore(pp.ModbusType, pp.ModbusAddr, pp.Count)
		}
	}

	// Analogs → holding-register / input-register store, updated by ODC Analog.
	for _, pt := range cfg.Analogs {
		pts := expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_Analog, 0)
		for _, pp := range pts {
			p.analogUpdateMap[pp.ODCIndex] = serverReadTarget{
				modbusType: pp.ModbusType,
				modbusAddr: pp.ModbusAddr,
				count:      pp.Count,
				scale:      pp.Scale,
				offset:     pp.Offset,
				endian:     pp.Endian,
				dataType:   pp.DataType,
			}
			p.preAllocStore(pp.ModbusType, pp.ModbusAddr, pp.Count)
		}
	}

	// AnalogOutputStatuses → same semantics as Analogs for the server.
	for _, pt := range cfg.AnalogOutputStatuses {
		pts := expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_AnalogOutputStatus, 0)
		for _, pp := range pts {
			if _, exists := p.analogUpdateMap[pp.ODCIndex]; !exists {
				p.analogUpdateMap[pp.ODCIndex] = serverReadTarget{
					modbusType: pp.ModbusType,
					modbusAddr: pp.ModbusAddr,
					count:      pp.Count,
					scale:      pp.Scale,
					offset:     pp.Offset,
					endian:     pp.Endian,
					dataType:   pp.DataType,
				}
			}
			p.preAllocStore(pp.ModbusType, pp.ModbusAddr, pp.Count)
		}
	}

	// OctetStrings → holding-register block, updated by ODC OctetString.
	for _, pt := range cfg.OctetStrings {
		pts := expandPolledPoint(pt.PointConfig, 0, 0, "", "", C.C_EventType_OctetString, 0)
		for _, pp := range pts {
			p.octetUpdateMap[pp.ODCIndex] = serverReadTarget{
				modbusType: pp.ModbusType,
				modbusAddr: pp.ModbusAddr,
				count:      pp.Count,
			}
			p.preAllocStore(pp.ModbusType, pp.ModbusAddr, pp.Count)
		}
	}

	// BinaryControls → coil writes trigger ODC Binary events.
	for _, ct := range cfg.BinaryControls {
		pts := expandControls(ct, C.C_EventType_Binary)
		for _, cp := range pts {
			p.coilWriteMap[cp.ModbusAddr] = serverWriteTarget{
				odcIndex: cp.ODCIndex,
				odcType:  C.C_EventType_Binary,
				count:    1,
			}
			p.preAllocStore("Coil", cp.ModbusAddr, 1)
		}
	}

	// AnalogControls → holding-register writes trigger ODC AnalogOutput* events.
	for _, ct := range cfg.AnalogControls {
		pts := expandServerAnalogControls(ct)
		for _, cp := range pts {
			p.hrWriteMap[cp.ModbusAddr] = serverWriteTarget{
				odcIndex: cp.ODCIndex,
				odcType:  cp.ODCType,
				count:    cp.Count,
				endian:   cp.Endian,
				dataType: cp.DataType,
			}
			p.preAllocStore("HoldingRegister", cp.ModbusAddr, cp.Count)
		}
	}
}

// preAllocStore ensures all registers in [addr, addr+count) exist in the store.
func (p *GoModbusServerPort) preAllocStore(modbusType string, addr, count uint16) {
	for i := uint16(0); i < count; i++ {
		a := addr + i
		switch modbusType {
		case "Coil":
			if _, ok := p.store.coils[a]; !ok {
				p.store.coils[a] = false
			}
		case "DiscreteInput":
			if _, ok := p.store.di[a]; !ok {
				p.store.di[a] = false
			}
		case "HoldingRegister":
			if _, ok := p.store.hr[a]; !ok {
				p.store.hr[a] = 0
			}
		case "InputRegister":
			if _, ok := p.store.ir[a]; !ok {
				p.store.ir[a] = 0
			}
		}
	}
}

// ---------------------------------------------------------------------------
// handleEvent — ODC strand callback
// ---------------------------------------------------------------------------

// handleEvent receives an ODC event from a connector and updates the data
// store so that Modbus clients reading the relevant address get the new value.
// The actual store update is dispatched to a short-lived goroutine to keep
// the ODC strand unblocked; the status callback is invoked from that goroutine.
func (p *GoModbusServerPort) handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer) {
	if !p.enabled.Load() {
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	et := uint8(C.odc_GetEventType(event))
	odcIndex := uint64(event.index)

	// Copy event to Go heap (source_port is a borrowed C pointer).
	evtCopy := *event
	evtCopy.source_port = nil

	p.eventWg.Add(1)
	go func() {
		defer p.eventWg.Done()
		p.applyEventToStore(&evtCopy, et, odcIndex, cb)
	}()
}

func (p *GoModbusServerPort) applyEventToStore(event *C.struct_C_EventInfo, et uint8, odcIndex uint64, cb unsafe.Pointer) {
	switch {
	// Binary / CROB → coil or discrete input
	case isBinaryEventType(et):
		if tgt, ok := p.binaryUpdateMap[odcIndex]; ok {
			val := getBinaryEventValue(event)
			p.store.mu.Lock()
			switch tgt.modbusType {
			case "Coil":
				p.store.coils[tgt.modbusAddr] = val
			case "DiscreteInput":
				p.store.di[tgt.modbusAddr] = val
			}
			p.store.mu.Unlock()
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}

	// Analog / AnalogOutput* → holding or input register
	case isAnalogEventType(et):
		if tgt, ok := p.analogUpdateMap[odcIndex]; ok {
			rawVal := getAnalogEventValue(event)
			// Apply inverse scale/offset: raw = (odcVal - offset) / scale
			if tgt.scale != 0 {
				rawVal = (rawVal - tgt.offset) / tgt.scale
			}
			regs := encodeAnalogRegs(rawVal, tgt.count, tgt.endian, tgt.dataType)
			p.store.mu.Lock()
			for i, r := range regs {
				addr := tgt.modbusAddr + uint16(i)
				switch tgt.modbusType {
				case "HoldingRegister":
					p.store.hr[addr] = r
				case "InputRegister":
					p.store.ir[addr] = r
				}
			}
			p.store.mu.Unlock()
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}

	// OctetString → holding or input register block
	case et == C.C_EventType_OctetString:
		if tgt, ok := p.octetUpdateMap[odcIndex]; ok {
			data := C.GoBytes(
				unsafe.Pointer(C.odc_GetOctetStringData(event)),
				C.int(C.odc_GetOctetStringSize(event)),
			)
			p.store.mu.Lock()
			for i := uint16(0); i < tgt.count; i++ {
				hi, lo := byte(0), byte(0)
				if int(i*2) < len(data) {
					hi = data[i*2]
				}
				if int(i*2+1) < len(data) {
					lo = data[i*2+1]
				}
				addr := tgt.modbusAddr + i
				switch tgt.modbusType {
				case "HoldingRegister":
					p.store.hr[addr] = uint16(hi)<<8 | uint16(lo)
				case "InputRegister":
					p.store.ir[addr] = uint16(hi)<<8 | uint16(lo)
				}
			}
			p.store.mu.Unlock()
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}
	}

	// No mapping found.
	invokeStatusCallback(cb, C.C_CommandStatus_NOT_SUPPORTED)
}

// isBinaryEventType returns true for Binary, BinaryOutputStatus, and CROB.
func isBinaryEventType(et uint8) bool {
	return et == C.C_EventType_Binary ||
		et == C.C_EventType_BinaryOutputStatus ||
		et == C.C_EventType_ControlRelayOutputBlock
}

// isAnalogEventType returns true for Analog, AnalogOutputStatus, and all
// AnalogOutput* command types.
func isAnalogEventType(et uint8) bool {
	return et == C.C_EventType_Analog ||
		et == C.C_EventType_AnalogOutputStatus ||
		et == C.C_EventType_AnalogOutputInt16 ||
		et == C.C_EventType_AnalogOutputInt32 ||
		et == C.C_EventType_AnalogOutputFloat32 ||
		et == C.C_EventType_AnalogOutputDouble64
}

// ---------------------------------------------------------------------------
// simonvetter/modbus handler interfaces
// ---------------------------------------------------------------------------

// HandleCoils handles FC1 (read) and FC5/FC15 (write) coil requests.
func (p *GoModbusServerPort) HandleCoils(req *modbus.CoilsRequest) ([]bool, error) {
	if p.config.UnitID != 0 && int(req.UnitId) != p.config.UnitID {
		return nil, modbus.ErrIllegalFunction
	}

	if req.IsWrite {
		p.store.mu.Lock()
		for i := uint16(0); i < req.Quantity; i++ {
			addr := req.Addr + i
			p.store.coils[addr] = req.Args[i]
		}
		p.store.mu.Unlock()

		// Publish ODC Binary events for any mapped write targets fully
		// covered by this write request.
		for addr, tgt := range p.coilWriteMap {
			if addr >= req.Addr && addr < req.Addr+req.Quantity {
				val := req.Args[addr-req.Addr]
				publishBinary(p.inst, tgt.odcIndex, val, tgt.odcType)
			}
		}
		return nil, nil
	}

	// Read.
	p.store.mu.RLock()
	res := make([]bool, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.coils[req.Addr+i]
	}
	p.store.mu.RUnlock()
	return res, nil
}

// HandleDiscreteInputs handles FC2 (read-only) discrete input requests.
func (p *GoModbusServerPort) HandleDiscreteInputs(req *modbus.DiscreteInputsRequest) ([]bool, error) {
	if p.config.UnitID != 0 && int(req.UnitId) != p.config.UnitID {
		return nil, modbus.ErrIllegalFunction
	}

	p.store.mu.RLock()
	res := make([]bool, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.di[req.Addr+i]
	}
	p.store.mu.RUnlock()
	return res, nil
}

// HandleHoldingRegisters handles FC3 (read) and FC6/FC16 (write) holding
// register requests.
func (p *GoModbusServerPort) HandleHoldingRegisters(req *modbus.HoldingRegistersRequest) ([]uint16, error) {
	if p.config.UnitID != 0 && int(req.UnitId) != p.config.UnitID {
		return nil, modbus.ErrIllegalFunction
	}

	if req.IsWrite {
		p.store.mu.Lock()
		for i := uint16(0); i < req.Quantity; i++ {
			p.store.hr[req.Addr+i] = req.Args[i]
		}
		p.store.mu.Unlock()

		// Publish ODC AnalogOutput* events for any mapped write targets
		// whose entire register block is fully covered by this write.
		for baseAddr, tgt := range p.hrWriteMap {
			if baseAddr >= req.Addr && baseAddr+tgt.count <= req.Addr+req.Quantity {
				offset := baseAddr - req.Addr
				regs := req.Args[offset : offset+tgt.count]
				rawVal := decodeAnalogRegs(regs, tgt.count, tgt.endian, tgt.dataType)
				publishAnalogOutputEvent(p.inst, tgt.odcIndex, rawVal, tgt.odcType)
			}
		}
		return nil, nil
	}

	// Read.
	p.store.mu.RLock()
	res := make([]uint16, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.hr[req.Addr+i]
	}
	p.store.mu.RUnlock()
	return res, nil
}

// HandleInputRegisters handles FC4 (read-only) input register requests.
func (p *GoModbusServerPort) HandleInputRegisters(req *modbus.InputRegistersRequest) ([]uint16, error) {
	if p.config.UnitID != 0 && int(req.UnitId) != p.config.UnitID {
		return nil, modbus.ErrIllegalFunction
	}

	p.store.mu.RLock()
	res := make([]uint16, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.ir[req.Addr+i]
	}
	p.store.mu.RUnlock()
	return res, nil
}
