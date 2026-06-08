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

	p := newGoModbusPort(name, typ, nil)
	inst := registerPort(p)
	p.inst = inst
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
	p.doBuild()
}

//export go_port_enable
func go_port_enable(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p == nil {
		return
	}
	p.doEnable()
}

//export go_port_disable
func go_port_disable(inst unsafe.Pointer) {
	p := lookupPort(inst)
	if p == nil {
		return
	}
	p.doDisable()
}

//export go_port_event
func go_port_event(inst unsafe.Pointer, event *C.struct_C_EventInfo, sender *C.char, cb *C.C_StatusCallback) {
	p := lookupPort(inst)
	if p == nil {
		invokeStatusCallback(unsafe.Pointer(cb), C.C_CommandStatus_UNDEFINED)
		return
	}
	// Both checks are on the strand — no mutex needed.
	if !p.enabled.Load() || !p.connected {
		invokeStatusCallback(unsafe.Pointer(cb), C.C_CommandStatus_HARDWARE_ERROR)
		return
	}

	s := ""
	if sender != nil {
		s = C.GoString(sender)
	}

	// Copy event to Go heap; nil source_port (borrowed C pointer).
	evtCopy := *event
	evtCopy.source_port = nil
	cbPtr := unsafe.Pointer(cb)
	client := p.client.Load() // atomic snapshot before leaving strand

	p.eventWg.Add(1)
	go func() {
		defer p.eventWg.Done()
		p.doHandleEventAsync(&evtCopy, s, cbPtr, client)
	}()
}

// ---------------------------------------------------------------------------
// ODC timer callback exports — fired on the strand by the ODC scheduler
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
// Publish helpers
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
