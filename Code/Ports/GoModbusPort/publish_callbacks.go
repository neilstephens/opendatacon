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
 * publish_callbacks.go — retry-capable publish helpers for control events.
 *
 * When a Modbus client writes to a BinaryControl or AnalogControl address,
 * the server publishes the corresponding ODC event.  If the ODC fabric
 * returns a non-SUCCESS status (e.g. because a downstream port is busy),
 * publishEventWithRetry() re-attempts the publish up to MaxPublishRetries
 * additional times.
 *
 * The status callback (go_publish_retry_cb) is an exported Go function so
 * that the ODC pool thread can call it back after processing the event.
 * Retry context is stored in retryCtxMap, keyed by a monotonic integer ID
 * cast to an opaque void* handle.
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

// Forward-declare the exported retry callback.
extern void go_publish_retry_cb(uint8_t status, void* handle);

// CGO cannot use an exported Go function directly as a callback pointer;
// this getter lets Go code obtain the pointer through a normal C call.
static inline C_StatusCallbackFunc_t get_go_retry_cb(void)
{
	return go_publish_retry_cb;
}
*/
import "C"
import (
	"sync"
	"sync/atomic"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Retry context
// ---------------------------------------------------------------------------

// retryCtx holds the state for one in-flight publish-with-retry operation.
// Stored in retryCtxMap until the ODC callback fires.
type retryCtx struct {
	inst    unsafe.Pointer
	evt     C.struct_C_EventInfo // deep copy; no borrowed pointers
	retries int                  // remaining attempts
	wg      *sync.WaitGroup      // kept alive for the lifetime of this chain
	enabled *atomic.Bool         // checked before each retry; nil disables the check
}

var (
	retryCtxMap sync.Map      // int64 → *retryCtx
	retryCtxSeq atomic.Int64  // monotonic counter used as handle ID
)

func storeRetryCtx(ctx *retryCtx) unsafe.Pointer {
	id := retryCtxSeq.Add(1)
	retryCtxMap.Store(id, ctx)
	// Allocate a tiny C buffer to hold the ID.  This avoids the integer-as-
	// pointer anti-pattern (unsafe.Pointer(uintptr(id))) that go vet rejects.
	// The buffer is freed in go_publish_retry_cb when the context is consumed.
	handle := C.malloc(C.size_t(8))
	*(*int64)(handle) = id
	return handle
}

// ---------------------------------------------------------------------------
// Exported retry callback — called by the ODC pool thread
// ---------------------------------------------------------------------------

//export go_publish_retry_cb
func go_publish_retry_cb(status C.uint8_t, handle unsafe.Pointer) {
	id := *(*int64)(handle)
	C.free(handle)

	val, ok := retryCtxMap.LoadAndDelete(id)
	if !ok {
		return
	}
	ctx := val.(*retryCtx)

	statusStr := C.GoString(C.odc_command_status_to_string(status))

	if uint8(status) == uint8(C.C_CommandStatus_SUCCESS) {
		logTrace(ctx.inst, "control publish status: %s", statusStr)
		ctx.wg.Done() // chain complete — release the WG hold
		return
	}

	// Short-circuit: if the port has been disabled, abandon remaining retries.
	if ctx.enabled != nil && !ctx.enabled.Load() {
		logDebug(ctx.inst, "control publish status: %s (port disabled, aborting retries)", statusStr)
		ctx.wg.Done()
		return
	}

	if ctx.retries <= 0 {
		logWarn(ctx.inst, "control publish status: %s (retries exhausted)", statusStr)
		ctx.wg.Done()
		return
	}

	logDebug(ctx.inst, "control publish status: %s (%d retries remaining)", statusStr, ctx.retries)
	ctx.retries--
	newHandle := storeRetryCtx(ctx)
	// Do NOT call wg.Done() here — the chain continues; wg stays elevated.
	C.odc_publish_event(ctx.inst, &ctx.evt, C.get_go_retry_cb(), newHandle)
}

// ---------------------------------------------------------------------------
// publishEventWithRetry — called from the selectLoop via writeNotifyChan
// ---------------------------------------------------------------------------

// publishEventWithRetry publishes an ODC event.  When retries > 0:
//   - wg.Add(1) is called before odc_publish_event, keeping the caller's
//     WaitGroup elevated until the entire retry chain terminates.
//   - enabled is checked in go_publish_retry_cb before each retry; if the
//     port has been disabled the chain is abandoned and wg.Done() is called.
// When retries == 0, the plain log callback is used (fire-and-forget, no WG).
func publishEventWithRetry(inst unsafe.Pointer, evt *C.struct_C_EventInfo,
	retries int, wg *sync.WaitGroup, enabled *atomic.Bool) {
	if retries <= 0 {
		C.odc_publish_event(inst, evt, C.odc_get_publish_log_cb(), inst)
		return
	}
	// Increment before publish so the WG is held for the full chain lifetime.
	wg.Add(1)
	ctx := &retryCtx{inst: inst, evt: *evt, retries: retries, wg: wg, enabled: enabled}
	handle := storeRetryCtx(ctx)
	C.odc_publish_event(inst, evt, C.get_go_retry_cb(), handle)
}

// ---------------------------------------------------------------------------
// Retry-capable wrappers for binary and analog-output control events
// ---------------------------------------------------------------------------

// publishBinaryWithRetry builds a Binary (or BinaryOutputStatus) event and
// publishes it with retry support.  See publishEventWithRetry for WG semantics.
func publishBinaryWithRetry(inst unsafe.Pointer, index uint64, value bool, odcType uint8,
	retries int, wg *sync.WaitGroup, enabled *atomic.Bool) {
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
	publishEventWithRetry(inst, &evt, retries, wg, enabled)
}

// publishAnalogOutputEventWithRetry builds an AnalogOutput* event and
// publishes it with retry support.  See publishEventWithRetry for WG semantics.
func publishAnalogOutputEventWithRetry(inst unsafe.Pointer, index uint64, value float64, odcType uint8,
	retries int, wg *sync.WaitGroup, enabled *atomic.Bool) {
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
	publishEventWithRetry(inst, &evt, retries, wg, enabled)
}
