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
 * modbus_server.go — GoModbusServerPort: Modbus TCP server (slave) mode.
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
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/simonvetter/modbus"
)

// ---------------------------------------------------------------------------
// Data store
// ---------------------------------------------------------------------------

// dataStore is a thread-safe in-memory Modbus register store.
// It is pre-populated with zero values for all configured addresses.
// Concurrent access comes from:
//   - the selectLoop's applyEventToStore goroutines (ODC-event-driven writes)
//   - the simonvetter/modbus server goroutines (Modbus-client-driven reads/writes)
type dataStore struct {
	coils map[uint16]bool
	di    map[uint16]bool   // discrete inputs (read-only by Modbus clients)
	hr    map[uint16]uint16 // holding registers
	ir    map[uint16]uint16 // input registers (read-only by Modbus clients)
	mu    sync.RWMutex
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
// Lookup types
// ---------------------------------------------------------------------------

// serverReadTarget is stored in the ODC→store map.  When an ODC event
// arrives with the matching index the value is encoded and written to store.
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
// writes to the base address the server publishes an ODC event.
type serverWriteTarget struct {
	odcIndex uint64
	odcType  uint8
	count    uint16
	endian   string
	dataType string
}

// ---------------------------------------------------------------------------
// GoModbusServerPort
// ---------------------------------------------------------------------------

// GoModbusServerPort implements a Modbus TCP server (slave).
//
// Concurrency model
// ─────────────────
// Follows the same selectLoop pattern as GoModbusClientPort.
// enable() spawns selectLoop; all mutable state lives there.
// disable() calls cancel() + server.Stop() (non-blocking) and returns
// immediately.  destroy() calls wg.Wait().
//
// The dataStore is protected by its own sync.RWMutex because it is accessed
// concurrently from:
//   - applyEventToStore goroutines (launched by selectLoop)
//   - simonvetter server goroutines (Handle* callbacks)
type GoModbusServerPort struct {
	name string
	typ  string
	inst unsafe.Pointer

	config *ServerPortConfig
	store  *dataStore

	// ODC index → store update info.
	binaryUpdateMap map[uint64]serverReadTarget
	analogUpdateMap map[uint64]serverReadTarget
	octetUpdateMap  map[uint64]serverReadTarget

	// Modbus address → ODC event info.
	coilWriteMap map[uint16]serverWriteTarget
	hrWriteMap   map[uint16]serverWriteTarget

	server *modbus.ModbusServer

	built   bool
	enabled atomic.Bool

	// Set once by enable() on the ODC strand.
	cancel          context.CancelFunc
	eventChan       chan eventWork
	writeNotifyChan chan func() // Modbus-client writes → ODC publish, routed via selectLoop
	wg              sync.WaitGroup // counts the selectLoop goroutine

	// Runtime counters (thread-safe atomics).
	eventsReceived  atomic.Uint64
	eventsApplied   atomic.Uint64
	writesPublished atomic.Uint64
	clientReads     atomic.Uint64
	enableTime      atomic.Int64 // UnixNano when enable() was called
	serverRunning   atomic.Bool  // true while selectLoop has started successfully
}

// writeNotifyChanBufSize is the capacity of the channel used to route Modbus
// client write notifications (coil/HR writes) from the simonvetter server
// goroutines to the selectLoop for publishing as ODC events.
const writeNotifyChanBufSize = 64

func newGoModbusServerPort(name, typ string) *GoModbusServerPort {
	return &GoModbusServerPort{name: name, typ: typ}
}

// ---------------------------------------------------------------------------
// State / status / stats JSON helpers
// ---------------------------------------------------------------------------

// goTimeStr returns the current UTC time formatted to match the C++
// since_epoch_to_datetime output ("YYYY-MM-DD HH:MM:SS.mmm").
func goTimeStr() string {
	return time.Now().UTC().Format("2006-01-02 15:04:05.000")
}

// pointJSON returns a JSON object string for a single point value with the
// given index, value, quality, and timestamp.
func pointJSON(index uint64, val interface{}, ts string) string {
	return fmt.Sprintf(`{"Index":%d,"Value":%v,"Quality":"ONLINE","Timestamp":%q}`,
		index, val, ts)
}

// ---------------------------------------------------------------------------
// State JSON helpers — each builds a sorted array of point-value objects
// for one point category.  They read from p.store and must be called while
// p.store.mu is held (RLock or Lock).
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) buildBinaryStateArrLocked(ts string) string {
	if len(p.binaryUpdateMap) == 0 {
		return ""
	}
	keys := make([]uint64, 0, len(p.binaryUpdateMap))
	for k := range p.binaryUpdateMap {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool { return keys[i] < keys[j] })
	pts := make([]string, 0, len(keys))
	for _, odcIdx := range keys {
		tgt := p.binaryUpdateMap[odcIdx]
		val := false
		switch tgt.modbusType {
		case "Coil":
			val = p.store.coils[tgt.modbusAddr]
		case "DiscreteInput":
			val = p.store.di[tgt.modbusAddr]
		}
		pts = append(pts, pointJSON(odcIdx, val, ts))
	}
	return strings.Join(pts, ",")
}

func (p *GoModbusServerPort) buildAnalogStateArrLocked(ts string) string {
	if len(p.analogUpdateMap) == 0 {
		return ""
	}
	keys := make([]uint64, 0, len(p.analogUpdateMap))
	for k := range p.analogUpdateMap {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool { return keys[i] < keys[j] })
	pts := make([]string, 0, len(keys))
	for _, odcIdx := range keys {
		tgt := p.analogUpdateMap[odcIdx]
		rawVal := p.readAnalogRegsLocked(&tgt)
		val := rawVal*tgt.scale + tgt.offset
		pts = append(pts, pointJSON(odcIdx, val, ts))
	}
	return strings.Join(pts, ",")
}

func (p *GoModbusServerPort) buildOctetStateArrLocked(ts string) string {
	if len(p.octetUpdateMap) == 0 {
		return ""
	}
	keys := make([]uint64, 0, len(p.octetUpdateMap))
	for k := range p.octetUpdateMap {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool { return keys[i] < keys[j] })
	pts := make([]string, 0, len(keys))
	for _, odcIdx := range keys {
		tgt := p.octetUpdateMap[odcIdx]
		regs := p.readRegsLocked(tgt.modbusAddr, tgt.count, tgt.modbusType)
		out := make([]byte, 0, len(regs)*2)
		for _, r := range regs {
			out = append(out, byte(r>>8), byte(r&0xFF))
		}
		pts = append(pts, pointJSON(odcIdx, fmt.Sprintf("%x", out), ts))
	}
	return strings.Join(pts, ",")
}

func (p *GoModbusServerPort) buildControlStateArrLocked(ts string) string {
	if len(p.coilWriteMap) == 0 {
		return ""
	}
	// coilWriteMap is keyed by Modbus address; group by ODC index.
	type entry struct{ idx uint64; val bool }
	seen := make(map[uint64]bool)
	var entries []entry
	for modbusAddr, tgt := range p.coilWriteMap {
		if seen[tgt.odcIndex] {
			continue
		}
		seen[tgt.odcIndex] = true
		entries = append(entries, entry{tgt.odcIndex, p.store.coils[modbusAddr]})
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].idx < entries[j].idx })
	pts := make([]string, 0, len(entries))
	for _, e := range entries {
		pts = append(pts, pointJSON(e.idx, e.val, ts))
	}
	return strings.Join(pts, ",")
}

func (p *GoModbusServerPort) buildAnalogControlStateArrLocked(ts string) string {
	if len(p.hrWriteMap) == 0 {
		return ""
	}
	// hrWriteMap is keyed by Modbus address; group by ODC index.
	type entry struct{ idx uint64; val float64 }
	seen := make(map[uint64]bool)
	var entries []entry
	for modbusAddr, tgt := range p.hrWriteMap {
		if seen[tgt.odcIndex] {
			continue
		}
		seen[tgt.odcIndex] = true
		regs := p.readRegsLocked(modbusAddr, tgt.count, "HoldingRegister")
		rawVal := decodeAnalogRegs(regs, tgt.count, tgt.endian, tgt.dataType)
		entries = append(entries, entry{tgt.odcIndex, rawVal})
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].idx < entries[j].idx })
	pts := make([]string, 0, len(entries))
	for _, e := range entries {
		pts = append(pts, pointJSON(e.idx, e.val, ts))
	}
	return strings.Join(pts, ",")
}

// readAnalogRegsLocked reads tgt's registers from the store and decodes them.
// Must be called with p.store.mu held (at least RLock).
func (p *GoModbusServerPort) readAnalogRegsLocked(tgt *serverReadTarget) float64 {
	regs := p.readRegsLocked(tgt.modbusAddr, tgt.count, tgt.modbusType)
	return decodeAnalogRegs(regs, tgt.count, tgt.endian, tgt.dataType)
}

// readRegsLocked reads count consecutive registers of the given type starting
// at addr from the store.  Must be called with p.store.mu held (at least RLock).
func (p *GoModbusServerPort) readRegsLocked(addr uint16, count uint16, modbusType string) []uint16 {
	regs := make([]uint16, 0, count)
	var m map[uint16]uint16
	switch modbusType {
	case "HoldingRegister":
		m = p.store.hr
	case "InputRegister":
		m = p.store.ir
	default:
		return regs
	}
	for i := uint16(0); i < count; i++ {
		regs = append(regs, m[addr+i])
	}
	return regs
}

// ---------------------------------------------------------------------------
// State / status / stats JSON (called on the ODC strand, thread-safe reads)
// ---------------------------------------------------------------------------

// StateJSON returns the current point values and operational state.
// Follows the DNP3/JSON port convention: point values wrapped in a UTC
// timestamp key, grouped by type.
func (p *GoModbusServerPort) StateJSON() string {
	if !p.built || p.store == nil || !p.enabled.Load() {
		return "{}"
	}
	ts := goTimeStr()

	p.store.mu.RLock()
	binaryArr := p.buildBinaryStateArrLocked(ts)
	analogArr := p.buildAnalogStateArrLocked(ts)
	octetArr := p.buildOctetStateArrLocked(ts)
	controlArr := p.buildControlStateArrLocked(ts)
	analogControlArr := p.buildAnalogControlStateArrLocked(ts)
	p.store.mu.RUnlock()

	var buf strings.Builder
	buf.WriteString(fmt.Sprintf(`{%q:{`, ts))

	// Demand flag
	buf.WriteString(`"InDemand":true`)

	writeArr(&buf, "Binaries", binaryArr)
	writeArr(&buf, "Analogs", analogArr)
	writeArr(&buf, "OctetStrings", octetArr)
	writeArr(&buf, "BinaryControls", controlArr)
	writeArr(&buf, "AnalogControls", analogControlArr)

	buf.WriteString(`}}`)
	return buf.String()
}

// writeArr appends a JSON key:value pair (with preceding comma) to buf if the
// value is non-empty.
func writeArr(buf *strings.Builder, key, arr string) {
	if arr == "" {
		return
	}
	buf.WriteString(fmt.Sprintf(`,%q:[%s]`, key, arr))
}

// StatusJSON returns the operational health and runtime metrics.
func (p *GoModbusServerPort) StatusJSON() string {
	uptime := 0
	if t := p.enableTime.Load(); t != 0 {
		uptime = int((time.Now().UnixNano() - t) / 1_000_000)
	}
	addr := ""
	if p.config != nil && p.config.TCP != nil {
		addr = p.config.TCP.Listen
	}
	unitID := 0
	if p.config != nil {
		unitID = p.config.UnitID
	}

	storeCoils := 0
	storeDI := 0
	storeHR := 0
	storeIR := 0
	if p.store != nil {
		p.store.mu.RLock()
		storeCoils = len(p.store.coils)
		storeDI = len(p.store.di)
		storeHR = len(p.store.hr)
		storeIR = len(p.store.ir)
		p.store.mu.RUnlock()
	}
	return fmt.Sprintf(
		`{"Enabled":%t,"Built":%t,"Running":%t,"ListenAddress":%q,"UnitID":%d,"UptimeMs":%d,`+
			`"EventsReceived":%d,"EventsApplied":%d,"WritesPublished":%d,"ModbusClientReads":%d,`+
			`"StoreCoils":%d,"StoreDiscreteInputs":%d,"StoreHoldingRegisters":%d,"StoreInputRegisters":%d}`,
		p.enabled.Load(), p.built, p.serverRunning.Load(), addr, unitID, uptime,
		p.eventsReceived.Load(), p.eventsApplied.Load(), p.writesPublished.Load(), p.clientReads.Load(),
		storeCoils, storeDI, storeHR, storeIR)
}

// StatsJSON returns performance-counter snapshots.
func (p *GoModbusServerPort) StatsJSON() string {
	return fmt.Sprintf(
		`{"EventsReceived":%d,"EventsApplied":%d,"WritesPublished":%d,"ModbusClientReads":%d}`,
		p.eventsReceived.Load(), p.eventsApplied.Load(), p.writesPublished.Load(), p.clientReads.Load())
}

// ---------------------------------------------------------------------------
// odcPort interface wrappers — all called on the ODC strand
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) build()   { p.doBuild() }
func (p *GoModbusServerPort) enable()  { p.doEnable() }
func (p *GoModbusServerPort) disable() { p.doDisable() }

// ---------------------------------------------------------------------------
// Lifecycle — called on the ODC strand
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

	if cfg.RTU != nil {
		logError(p.inst, "server build failed: RTU server mode is not supported by the underlying modbus library")
		return
	}

	p.store = newDataStore()
	p.binaryUpdateMap = make(map[uint64]serverReadTarget)
	p.analogUpdateMap = make(map[uint64]serverReadTarget)
	p.octetUpdateMap = make(map[uint64]serverReadTarget)
	p.coilWriteMap = make(map[uint16]serverWriteTarget)
	p.hrWriteMap = make(map[uint16]serverWriteTarget)
	p.buildMaps()

	srv, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL: "tcp://" + cfg.TCP.Listen,
	}, p)
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

	p.enableTime.Store(time.Now().UnixNano())
	ctx, cancel := context.WithCancel(context.Background())
	p.cancel = cancel
	p.eventChan = make(chan eventWork, eventChanBufSize)
	p.writeNotifyChan = make(chan func(), writeNotifyChanBufSize)

	p.wg.Add(1)
	go p.selectLoop(ctx)

	p.enabled.Store(true)
	logDebug(p.inst, "server doEnable() finished")
}

// doDisable cancels the context and stops the server listener; wg.Wait() is
// the hard boundary.  publishConnectState(DISCONNECTED) is issued from
// ctx.Done() where the server actually closes.
func (p *GoModbusServerPort) doDisable() {
	if !p.enabled.Load() {
		return
	}
	p.enabled.Store(false)

	if p.cancel != nil {
		p.cancel()
		p.cancel = nil
	}
	if p.server != nil {
		p.server.Stop() //nolint:errcheck
	}

	p.wg.Wait() // boundary: nothing in-flight after this returns
}

func (p *GoModbusServerPort) destroy() {
	logDebug(p.inst, "server destroy() finished")
	removePort(p.inst)
}

// ---------------------------------------------------------------------------
// handleEvent — called on the ODC strand
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer) {
	if !p.enabled.Load() {
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	work := eventWork{
		event:  *event,
		sender: sender,
		cb:     cb,
	}
	work.event.source_port = nil

	// OctetString payload is a borrowed pointer valid only for the duration of
	// this callback.  Deep-copy the data now so the goroutine can access it safely.
	if uint8(C.odc_GetEventType(event)) == C.C_EventType_OctetString {
		if sz := C.odc_GetOctetStringSize(event); sz > 0 {
			if dp := C.odc_GetOctetStringData(event); dp != nil {
				work.octetData = C.GoBytes(unsafe.Pointer(dp), C.int(sz))
			}
		}
	}

	select {
	case p.eventChan <- work:
	default:
		invokeStatusCallback(cb, C.C_CommandStatus_HARDWARE_ERROR)
	}
}

// ---------------------------------------------------------------------------
// Select loop
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) selectLoop(ctx context.Context) {
	defer p.wg.Done()

	// Start() binds the TCP listener and spawns acceptTCPClients(); it returns
	// immediately and is non-blocking from this goroutine's perspective.
	if err := p.server.Start(); err != nil {
		logError(p.inst, "server Start() failed: %v", err)
		// This CGO call is safe here: we are early in the goroutine's life,
		// destroy() has not been called, and dlclose is far away.
		publishConnectState(p.inst, C.C_ConnectState_PORT_DOWN)
		return
	}
	p.serverRunning.Store(true)
	// Same reasoning: CONNECTED is published at the start of the loop,
	// not on the exit path, so there is no dlclose race.
	publishConnectState(p.inst, C.C_ConnectState_CONNECTED)

	var workerWg sync.WaitGroup
	for {
		select {
		case <-ctx.Done():
			p.serverRunning.Store(false)
			workerWg.Wait()
			// Drain pending ODC events whose callbacks were never dispatched.
		DRAIN:
			for {
				select {
				case w := <-p.eventChan:
					invokeStatusCallback(w.cb, C.C_CommandStatus_HARDWARE_ERROR)
				default:
					break DRAIN
				}
			}
			// Discard any pending Modbus-write publish thunks — port is shutting down.
		DRAIN_WRITES:
			for {
				select {
				case <-p.writeNotifyChan:
				default:
					break DRAIN_WRITES
				}
			}
			publishConnectState(p.inst, C.C_ConnectState_DISCONNECTED)
			return

		// Incoming ODC events update the data store.
		case work := <-p.eventChan:
			workerWg.Add(1)
			go func(w eventWork) {
				defer workerWg.Done()
				// Copy to standalone variable — same reason as client: CGO
				// rejects &w.event because w also contains sender string.
				event := w.event
				p.applyEventToStore(&event, uint8(C.odc_GetEventType(&event)), uint64(event.index), w.cb, w.octetData)
			}(work)

		// Modbus-client writes: publish the resulting ODC event.
		// The thunk was sent by HandleCoils/HandleHoldingRegisters from
		// simonvetter's goroutines.  Executing it here keeps all publish
		// calls on a single controlled goroutine (the selectLoop).
		case fn := <-p.writeNotifyChan:
			fn()
		}
	}
}

// ---------------------------------------------------------------------------
// applyEventToStore — goroutine launched by selectLoop
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) applyEventToStore(event *C.struct_C_EventInfo, et uint8, odcIndex uint64, cb unsafe.Pointer, octetData []byte) {
	p.eventsReceived.Add(1)
	switch {
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
			p.eventsApplied.Add(1)
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}

	case isAnalogEventType(et):
		if tgt, ok := p.analogUpdateMap[odcIndex]; ok {
			rawVal := getAnalogEventValue(event)
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
			p.eventsApplied.Add(1)
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}

	case et == C.C_EventType_OctetString:
		if tgt, ok := p.octetUpdateMap[odcIndex]; ok {
			// octetData was deep-copied in handleEvent() while the borrowed
			// pointer was valid — use it here instead of the stale event pointer.
			data := octetData
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
			p.eventsApplied.Add(1)
			invokeStatusCallback(cb, C.C_CommandStatus_SUCCESS)
			return
		}
	}

	invokeStatusCallback(cb, C.C_CommandStatus_NOT_SUPPORTED)
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
// Map construction — called once in doBuild()
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) buildMaps() {
	cfg := p.config

	for _, pt := range cfg.Binaries {
		pts := expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_Binary, 0)
		for i := range pts {
			p.binaryUpdateMap[pts[i].ODCIndex] = serverReadTarget{
				modbusType: pts[i].ModbusType, modbusAddr: pts[i].ModbusAddr, count: pts[i].Count,
			}
			p.preAllocStore(pts[i].ModbusType, pts[i].ModbusAddr, pts[i].Count)
		}
	}

	for _, pt := range cfg.BinaryOutputStatuses {
		pts := expandPolledPoint(pt, 0, 0, "", "", C.C_EventType_BinaryOutputStatus, 0)
		for i := range pts {
			if _, exists := p.binaryUpdateMap[pts[i].ODCIndex]; !exists {
				p.binaryUpdateMap[pts[i].ODCIndex] = serverReadTarget{
					modbusType: pts[i].ModbusType, modbusAddr: pts[i].ModbusAddr, count: pts[i].Count,
				}
			}
			p.preAllocStore(pts[i].ModbusType, pts[i].ModbusAddr, pts[i].Count)
		}
	}

	for _, pt := range cfg.Analogs {
		pts := expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_Analog, 0)
		for i := range pts {
			p.analogUpdateMap[pts[i].ODCIndex] = serverReadTarget{
				modbusType: pts[i].ModbusType, modbusAddr: pts[i].ModbusAddr, count: pts[i].Count,
				scale: pts[i].Scale, offset: pts[i].Offset, endian: pts[i].Endian, dataType: pts[i].DataType,
			}
			p.preAllocStore(pts[i].ModbusType, pts[i].ModbusAddr, pts[i].Count)
		}
	}

	for _, pt := range cfg.AnalogOutputStatuses {
		pts := expandPolledPoint(pt.PointConfig, pt.Scale, pt.Offset, pt.Endian, pt.DataType, C.C_EventType_AnalogOutputStatus, 0)
		for i := range pts {
			if _, exists := p.analogUpdateMap[pts[i].ODCIndex]; !exists {
				p.analogUpdateMap[pts[i].ODCIndex] = serverReadTarget{
					modbusType: pts[i].ModbusType, modbusAddr: pts[i].ModbusAddr, count: pts[i].Count,
					scale: pts[i].Scale, offset: pts[i].Offset, endian: pts[i].Endian, dataType: pts[i].DataType,
				}
			}
			p.preAllocStore(pts[i].ModbusType, pts[i].ModbusAddr, pts[i].Count)
		}
	}

	for _, pt := range cfg.OctetStrings {
		pts := expandPolledPoint(pt.PointConfig, 0, 0, "", "", C.C_EventType_OctetString, 0)
		for i := range pts {
			p.octetUpdateMap[pts[i].ODCIndex] = serverReadTarget{
				modbusType: pts[i].ModbusType, modbusAddr: pts[i].ModbusAddr, count: pts[i].Count,
			}
			p.preAllocStore(pts[i].ModbusType, pts[i].ModbusAddr, pts[i].Count)
		}
	}

	for _, ct := range cfg.BinaryControls {
		cps := expandControls(ct, C.C_EventType_Binary)
		for i := range cps {
			p.coilWriteMap[cps[i].ModbusAddr] = serverWriteTarget{
				odcIndex: cps[i].ODCIndex, odcType: C.C_EventType_Binary, count: 1,
			}
			p.preAllocStore("Coil", cps[i].ModbusAddr, 1)
		}
	}

	for _, ct := range cfg.AnalogControls {
		cps := expandServerAnalogControls(ct)
		for i := range cps {
			p.hrWriteMap[cps[i].ModbusAddr] = serverWriteTarget{
				odcIndex: cps[i].ODCIndex, odcType: cps[i].ODCType, count: cps[i].Count,
				endian: cps[i].Endian, dataType: cps[i].DataType,
			}
			p.preAllocStore("HoldingRegister", cps[i].ModbusAddr, cps[i].Count)
		}
	}
}

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
// simonvetter/modbus handler interfaces
// These are called from the modbus server's own goroutines and run
// concurrently with applyEventToStore workers — hence the store mutex.
// ---------------------------------------------------------------------------

func (p *GoModbusServerPort) HandleCoils(req *modbus.CoilsRequest) ([]bool, error) {
	if p.config.UnitID != 0 && int(req.UnitId) != p.config.UnitID {
		return nil, modbus.ErrIllegalFunction
	}
	if req.IsWrite {
		p.store.mu.Lock()
		for i := uint16(0); i < req.Quantity; i++ {
			p.store.coils[req.Addr+i] = req.Args[i]
		}
		p.store.mu.Unlock()
		// Route write notifications through writeNotifyChan so that all
		// publishBinary calls happen on the selectLoop goroutine, not on
		// simonvetter's internal goroutines.
		for addr, tgt := range p.coilWriteMap {
			if addr >= req.Addr && addr < req.Addr+req.Quantity {
				val := req.Args[addr-req.Addr]
				odcIdx := tgt.odcIndex
				odcType := tgt.odcType
				select {
				case p.writeNotifyChan <- func() {
					publishBinary(p.inst, odcIdx, val, odcType)
					p.writesPublished.Add(1)
				}:
				default: // channel full or selectLoop shutting down — discard
				}
			}
		}
		return nil, nil
	}
	p.store.mu.RLock()
	res := make([]bool, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.coils[req.Addr+i]
	}
	p.store.mu.RUnlock()
	p.clientReads.Add(1)
	return res, nil
}

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
	p.clientReads.Add(1)
	return res, nil
}

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
		for baseAddr, tgt := range p.hrWriteMap {
			if baseAddr >= req.Addr && baseAddr+tgt.count <= req.Addr+req.Quantity {
				offset := baseAddr - req.Addr
				// Copy the register slice before capturing in the closure —
				// req.Args belongs to the simonvetter library and may be reused.
				regs := make([]uint16, tgt.count)
				copy(regs, req.Args[offset:offset+tgt.count])
				cnt := tgt.count
				endian := tgt.endian
				dataType := tgt.dataType
				odcIdx := tgt.odcIndex
				odcType := tgt.odcType
				select {
				case p.writeNotifyChan <- func() {
					rawVal := decodeAnalogRegs(regs, cnt, endian, dataType)
					publishAnalogOutputEvent(p.inst, odcIdx, rawVal, odcType)
					p.writesPublished.Add(1)
				}:
				default: // channel full or selectLoop shutting down — discard
				}
			}
		}
		return nil, nil
	}
	p.store.mu.RLock()
	res := make([]uint16, req.Quantity)
	for i := uint16(0); i < req.Quantity; i++ {
		res[i] = p.store.hr[req.Addr+i]
	}
	p.store.mu.RUnlock()
	p.clientReads.Add(1)
	return res, nil
}

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
	p.clientReads.Add(1)
	return res, nil
}
