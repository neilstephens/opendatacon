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
 * common.go — port registry, shared types, logging helpers, ODC publish
 *              helpers, and status-callback wrapper.
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
	"sync"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Port registry
// ---------------------------------------------------------------------------

var (
	portMu sync.Mutex
	ports  = make(map[unsafe.Pointer]odcPort)
)

func lookupPort(inst unsafe.Pointer) odcPort {
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

func registerPort(p odcPort) unsafe.Pointer {
	// Allocate a unique C-memory sentinel as the opaque handle.
	// C memory never moves, avoiding GC/vet concerns with unsafe.Pointer.
	key := C.malloc(C.size_t(1))
	if key == nil {
		panic("C.malloc failed")
	}
	portMu.Lock()
	ports[key] = p
	portMu.Unlock()
	return key
}

// ---------------------------------------------------------------------------
// Shared event type
// ---------------------------------------------------------------------------

// eventWork carries a deep-copied ODC event across the eventChan channel
// boundary.  source_port is cleared on copy because it is a borrowed C
// pointer that may be freed before the worker goroutine reads it.
// octetData carries a Go-owned copy of the OctetString payload for the same
// reason: octet_string.data is a borrowed pointer valid only during the
// odc_port_event() callback; any goroutine access must use this copy instead.
type eventWork struct {
	event     C.struct_C_EventInfo // deep-copied value
	sender    string
	cb        unsafe.Pointer
	octetData []byte // non-nil for OctetString events; copied while pointer is valid
}

// ---------------------------------------------------------------------------
// Status callback
// ---------------------------------------------------------------------------

func invokeStatusCallback(cb unsafe.Pointer, status uint8) {
	if cb == nil {
		return
	}
	C.odc_InvokeStatusCallbackSafe(cb, C.uint8_t(status))
}

// ---------------------------------------------------------------------------
// Logging helpers — called from goroutines; thread-safe via spdlog
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

// ---------------------------------------------------------------------------
// ODC publish helpers — all are goroutine-safe; they route through the
// DataConnector event fabric, not through the ODC strand.
// ---------------------------------------------------------------------------

func odcNow() C.uint64_t {
	return C.uint64_t(C.odc_ms_since_epoch())
}

func publishBinary(inst unsafe.Pointer, index uint64, value bool, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  odcNow(),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	var v C.uint8_t = 0
	if value {
		v = 1
	}
	C.odc_SetPayloadBinary(&evt, v)
	C.odc_publish_event(inst, &evt, C.odc_get_publish_log_cb(), inst)
}

func publishAnalog(inst unsafe.Pointer, index uint64, value float64, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  odcNow(),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	C.odc_SetPayloadAnalog(&evt, C.double(value))
	C.odc_publish_event(inst, &evt, C.odc_get_publish_log_cb(), inst)
}

func publishAnalogOutputEvent(inst unsafe.Pointer, index uint64, value float64, odcType uint8) {
	evt := C.struct_C_EventInfo{
		event_type: C.uint8_t(odcType),
		index:      C.size_t(index),
		timestamp:  odcNow(),
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
	C.odc_publish_event(inst, &evt, C.odc_get_publish_log_cb(), inst)
}

func publishOctetString(inst unsafe.Pointer, index uint64, data []byte) {
	if len(data) == 0 {
		return
	}
	evt := C.struct_C_EventInfo{
		event_type: C.C_EventType_OctetString,
		index:      C.size_t(index),
		timestamp:  odcNow(),
		quality:    C.uint16_t(C.C_QualityFlags_ONLINE),
	}
	cdata := C.CBytes(data)
	C.odc_SetPayloadOctetString(&evt, (*C.uint8_t)(cdata), C.size_t(len(data)))
	C.odc_publish_event(inst, &evt, C.odc_get_publish_log_cb(), inst)
	// odc_publish_event deep-copies the octet payload before returning.
	C.free(cdata)
}

func publishConnectState(inst unsafe.Pointer, state int32) {
	C.odc_publish_connect_state(inst, C.int(state))
}

// ---------------------------------------------------------------------------
// Event value extraction helpers
// ---------------------------------------------------------------------------

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

func isBinaryEventType(et uint8) bool {
	return et == C.C_EventType_Binary ||
		et == C.C_EventType_BinaryOutputStatus ||
		et == C.C_EventType_ControlRelayOutputBlock
}
