//go:build test_stubs

package main

/*
#include <stdint.h>
#include <stddef.h>

// Stubs for test linking — real implementations provided by ODC at runtime.
// Compiled only when "test_stubs" build tag is set (go test -tags test_stubs).
void odc_Log(void* inst, uint8_t level, const char* msg) {}
int  odc_ShouldLog(void* inst, uint8_t level) { return 0; }
void* odc_GetConfigJSON(void* inst) { return NULL; }
void odc_PublishEvent(void* inst, const void* evt, void (*cb)(uint8_t, void*), void* handle) {}
void odc_PublishConnectState(void* inst, int state) {}
void odc_InvokeStatusCallback(void** cb, uint8_t status) {}
void* odc_msTimerCallback(void* inst, uint64_t ms, void (*cb)(uint8_t, void*), void* handle) { return NULL; }
void odc_cancelTimer(void* timer) {}
*/
import "C"
