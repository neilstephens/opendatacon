package main

/*
#cgo CFLAGS: -I${SRCDIR} -I${SRCDIR}/../../../include

#include <stdlib.h>
#include "opendatacon/odc_c_api.h"
#include "gombus_helpers.h"
*/
import "C"
import (
	"time"
	"unsafe"
)

func main() {}

// ---------------------------------------------------------------------------
// odcPort interface — implemented by both GoModbusClientPort and
// GoModbusServerPort so the registry can hold either without type assertions
// in the common dispatch path.
// ---------------------------------------------------------------------------

type odcPort interface {
	build()
	enable()
	disable()
	destroy()
	// handleEvent is called on the ODC strand with ownership of cb.
	// Implementations must invoke the status callback exactly once
	// (directly or via a goroutine).
	handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer)
}

//export odc_library_init
func odc_library_init(odc *C.struct_C_ODC_HostAPI) {
	C.odc = odc
}

//------------------------------------------------------------------------------
// Required C API exports
//------------------------------------------------------------------------------

//export go_c_api_version
func go_c_api_version() *C.char {
	return C.CString(C.ODC_C_API_VERSION)
}

//export go_port_create
func go_port_create(cType *C.char, cName *C.char) unsafe.Pointer {
	name := C.GoString(cName)
	typ := C.GoString(cType)

	var p odcPort
	switch typ {
	case "GoModbusServer":
		p = newGoModbusServerPort(name, typ)
	case "GoModbusClient", "GoModbus": // "GoModbus" kept for backward compatibility
		p = newGoModbusClientPort(name, typ)
	default:
		// Unknown type: log a warning and fall through to client mode.
		// The ODC logger is not yet available at this point (inst is nil),
		// so we can only use a Go stderr write here.  After build() the
		// instance will log via the ODC sink.
		p = newGoModbusClientPort(name, typ)
	}

	inst := registerPort(p)
	switch cp := p.(type) {
	case *GoModbusClientPort:
		cp.inst = inst
	case *GoModbusServerPort:
		cp.inst = inst
	}
	return inst
}

//export go_port_destroy
func go_port_destroy(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p != nil {
		p.destroy()
	}
}

//export go_port_build
func go_port_build(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p == nil {
		return
	}
	p.build()
}

//export go_port_enable
func go_port_enable(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p == nil {
		return
	}
	p.enable()
}

//export go_port_disable
func go_port_disable(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p == nil {
		return
	}
	p.disable()
}

//export go_port_event
func go_port_event(inst unsafe.Pointer, event *C.struct_C_EventInfo, sender *C.char, cb *C.C_StatusCallback) {
	p := lookupPort(inst)
	if p == nil {
		invokeStatusCallback(unsafe.Pointer(cb), C.C_CommandStatus_UNDEFINED)
		return
	}

	s := ""
	if sender != nil {
		s = C.GoString(sender)
	}

	p.handleEvent(event, s, unsafe.Pointer(cb))
}

// ---------------------------------------------------------------------------
// ODC timer callback exports — fired on the strand by the ODC scheduler.
// These are only ever registered by GoModbusClientPort, so type-assert safely.
// ---------------------------------------------------------------------------

//export go_reconnect_timer_cb
func go_reconnect_timer_cb(status C.uint8_t, handle unsafe.Pointer) {
	onReconnectTimer(handle)
}

//export go_connect_ok_cb
func go_connect_ok_cb(status C.uint8_t, handle unsafe.Pointer) {
	onConnectOk(handle)
}

//export go_connect_fail_cb
func go_connect_fail_cb(status C.uint8_t, handle unsafe.Pointer) {
	onConnectFail(handle)
}

//export go_transport_disconnect_cb
func go_transport_disconnect_cb(status C.uint8_t, handle unsafe.Pointer) {
	onTransportDisconnect(handle)
}

//------------------------------------------------------------------------------
// Optional C API exports
//------------------------------------------------------------------------------

//export go_port_stats_json
func go_port_stats_json(inst unsafe.Pointer) *C.char {
	return C.CString("{}")
}

//export go_port_state_json
func go_port_state_json(inst unsafe.Pointer) *C.char {
	return C.CString("{}")
}

//export go_port_status_json
func go_port_status_json(inst unsafe.Pointer) *C.char {
	return C.CString("{}")
}

//export go_port_free_string
func go_port_free_string(s *C.char) {
	C.free(unsafe.Pointer(s))
}

//------------------------------------------------------------------------------
// Publish helpers — called from goroutines (poll, event, server handlers).
// odc_publish_event is goroutine-safe: it posts through the ASIO event loop.
//------------------------------------------------------------------------------

func publishBinary(inst unsafe.Pointer, index uint64, value bool, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  C.uint64_t(time.Now().UnixMilli()),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	var v C.uint8_t = 0
	if value {
		v = 1
	}
	C.odc_SetPayloadBinary(&evt, v)
	C.odc_publish_event(inst, &evt, nil, nil)
}

func publishAnalog(inst unsafe.Pointer, index uint64, value float64, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  C.uint64_t(time.Now().UnixMilli()),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	C.odc_SetPayloadAnalog(&evt, C.double(value))
	C.odc_publish_event(inst, &evt, nil, nil)
}

// publishAnalogOutputEvent publishes an ODC AnalogOutput* event with the
// correct payload union field for the given odcType.  Used by the server
// when a Modbus client writes a holding register mapped to AnalogControls.
func publishAnalogOutputEvent(inst unsafe.Pointer, index uint64, value float64, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  C.uint64_t(time.Now().UnixMilli()),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	switch odcType {
	case C.C_EventType_AnalogOutputInt16:
		C.odc_SetPayloadAO16(&evt, C.int16_t(int16(value)))
	case C.C_EventType_AnalogOutputInt32:
		C.odc_SetPayloadAO32(&evt, C.int32_t(int32(value)))
	case C.C_EventType_AnalogOutputFloat32:
		C.odc_SetPayloadAOF32(&evt, C.float(float32(value)))
	case C.C_EventType_AnalogOutputDouble64:
		C.odc_SetPayloadAOD64(&evt, C.double(value))
	default:
		C.odc_SetPayloadAnalog(&evt, C.double(value))
	}
	C.odc_publish_event(inst, &evt, nil, nil)
}

func publishOctetString(inst unsafe.Pointer, index uint64, data []byte) {
	if len(data) == 0 {
		return
	}
	evt := C.struct_C_EventInfo{
		event_type: C.C_EventType_OctetString,
		index:      C.size_t(index),
		timestamp:  C.uint64_t(time.Now().UnixMilli()),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	cdata := C.CBytes(data)
	defer C.free(cdata)
	C.odc_SetPayloadOctetString(&evt, (*C.uint8_t)(cdata), C.size_t(len(data)))
	C.odc_publish_event(inst, &evt, nil, nil)
}

func publishConnectState(inst unsafe.Pointer, state int32) {
	evt := C.struct_C_EventInfo{
		event_type: C.C_EventType_ConnectState,
		index:      0,
		timestamp:  C.uint64_t(time.Now().UnixMilli()),
		quality:    0,
	}
	C.odc_SetPayloadConnectState(&evt, C.uint8_t(state))
	C.odc_publish_event(inst, &evt, nil, nil)
}

// ---------------------------------------------------------------------------
// Event value extraction helpers
// ---------------------------------------------------------------------------

// getAnalogEventValue extracts the numeric payload from any analog-class ODC
// event as a float64.
func getAnalogEventValue(event *C.struct_C_EventInfo) float64 {
	switch uint8(C.odc_GetEventType(event)) {
	case C.C_EventType_AnalogOutputInt16:
		return float64(int16(C.odc_GetAO16Value(event)))
	case C.C_EventType_AnalogOutputInt32:
		return float64(int32(C.odc_GetAO32Value(event)))
	case C.C_EventType_AnalogOutputFloat32:
		return float64(C.odc_GetAOF32Value(event))
	case C.C_EventType_AnalogOutputDouble64:
		return float64(C.odc_GetAOD64Value(event))
	default: // Analog, AnalogOutputStatus
		return float64(C.odc_GetPayloadAnalog(event))
	}
}

// getBinaryEventValue extracts a bool from any binary-class ODC event.
// For CROB events the function code is used to determine on/off intent.
func getBinaryEventValue(event *C.struct_C_EventInfo) bool {
	switch uint8(C.odc_GetEventType(event)) {
	case C.C_EventType_ControlRelayOutputBlock:
		fc := uint8(C.odc_GetCROBFunctionCode(event))
		switch fc {
		case C.C_ControlCode_LATCH_ON, C.C_ControlCode_CLOSE_PULSE_ON, C.C_ControlCode_PULSE_ON:
			return true
		default:
			return false
		}
	default:
		return C.odc_GetPayloadBinary(event) != 0
	}
}
