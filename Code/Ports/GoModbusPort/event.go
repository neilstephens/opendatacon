package main

/*
#cgo CFLAGS: -I${SRCDIR} -I${SRCDIR}/../../../include

#include "opendatacon/odc_c_api.h"
#include "gombus_helpers.h"
*/
import "C"
import "unsafe"

func invokeStatusCallback(cb unsafe.Pointer, status uint8) {
	if cb == nil {
		return
	}
	C.odc_InvokeStatusCallbackSafe(cb, C.uint8_t(status))
}
