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
 * main.go — required C API exports and the odcPort interface.
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
	"unsafe"
)

func main() {}

// ---------------------------------------------------------------------------
// odcPort interface — implemented by GoModbusClientPort and GoModbusServerPort.
// All methods are called on the ODC strand except destroy(), which is called
// off-strand from ~C_Port() after the strand has been fully drained.
// ---------------------------------------------------------------------------

type odcPort interface {
	build()
	enable()
	disable()
	destroy()
	// handleEvent is called on the ODC strand.  Implementations send the work
	// onto an internal channel and return immediately; a goroutine does the
	// blocking Modbus I/O and invokes the status callback exactly once.
	handleEvent(event *C.struct_C_EventInfo, sender string, cb unsafe.Pointer)
}

//export odc_library_init
func odc_library_init(hostAPI *C.struct_C_ODC_HostAPI) {
	C.odc = hostAPI
}

//export go_port_create
func go_port_create(cType *C.char, cName *C.char) unsafe.Pointer {
	name := C.GoString(cName)
	typ := C.GoString(cType)

	var p odcPort
	switch typ {
	case "GoModbusServer":
		p = newGoModbusServerPort(name, typ)
	case "GoModbusClient":
		p = newGoModbusClientPort(name, typ)
	default:
		p = newGoModbusClientPort(name, typ)
	}

	inst := registerPort(p)
	switch cp := p.(type) {
	case *GoModbusClientPort:
		cp.inst = inst
		if typ != "GoModbusClient" {
			logError(inst, "unknown port type %q — only \"GoModbusClient\" and \"GoModbusServer\" are valid; treating as GoModbusClient", typ)
		}
	case *GoModbusServerPort:
		cp.inst = inst
	}
	return inst
}

//export go_port_destroy
func go_port_destroy(inst unsafe.Pointer) {
	if p := lookupPort(inst); p != nil {
		p.destroy()
	}
}

//export go_port_build
func go_port_build(inst unsafe.Pointer) {
	if p := lookupPort(inst); p != nil {
		p.build()
	}
}

//export go_port_enable
func go_port_enable(inst unsafe.Pointer) {
	if p := lookupPort(inst); p != nil {
		p.enable()
	}
}

//export go_port_disable
func go_port_disable(inst unsafe.Pointer) {
	if p := lookupPort(inst); p != nil {
		p.disable()
	}
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

//export go_port_stats_json
func go_port_stats_json(inst unsafe.Pointer) *C.char {
	switch p := lookupPort(inst).(type) {
	case *GoModbusClientPort:
		if ps := p.pollStatsVal.Load(); ps != nil {
			s := ps.Stats()
			return C.CString(fmt.Sprintf(
				`{"PollsScheduled":%d,"PollsDropped":%d,"PollsRunning":%d,"MaxConcurrentPolls":%d}`,
				s.PollsScheduled, s.PollsDropped, s.PollsRunning, s.MaxConcurrent))
		}
	case *GoModbusServerPort:
		return C.CString(p.StatsJSON())
	}
	return C.CString("{}")
}

//export go_port_state_json
func go_port_state_json(inst unsafe.Pointer) *C.char {
	switch p := lookupPort(inst).(type) {
	case *GoModbusServerPort:
		return C.CString(p.StateJSON())
	}
	return C.CString("{}")
}

//export go_port_status_json
func go_port_status_json(inst unsafe.Pointer) *C.char {
	switch p := lookupPort(inst).(type) {
	case *GoModbusServerPort:
		return C.CString(p.StatusJSON())
	}
	return C.CString("{}")
}

//export go_port_free_string
func go_port_free_string(s *C.char) {
	C.free(unsafe.Pointer(s))
}
