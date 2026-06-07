#ifndef GOMBUS_HELPERS_H
#define GOMBUS_HELPERS_H

#include <stddef.h>
#include <stdint.h>
#include "opendatacon/odc_c_api.h"

/* ------------------------------------------------------------------ */
/*  Host API vtable pointer — defined in api_shim.c, set once by      */
/*  odc_library_init() before any odc_port_create() call.             */
/* ------------------------------------------------------------------ */
extern struct C_ODC_HostAPI* odc;

/* ------------------------------------------------------------------ */
/*  Inline wrappers — odc_foo(...) calls odc->foo(...)                 */
/* ------------------------------------------------------------------ */

static inline void odc_invoke_status_callback(C_StatusCallback** cb, uint8_t status)
{
	if(odc && odc->invoke_status_callback)
		odc->invoke_status_callback(cb, status);
}

/* Safe callback-invocation wrapper: takes the C_StatusCallback* as a
   void* so binding code does not need to form a pointer-to-pointer. */
static inline void odc_InvokeStatusCallbackSafe(void* cb_ptr, uint8_t status)
{
	if(cb_ptr != NULL && odc != NULL && odc->invoke_status_callback != NULL)
	{
		C_StatusCallback* cb = (C_StatusCallback*)cb_ptr;
		odc->invoke_status_callback(&cb, status);
	}
}

static inline void odc_publish_event(void* inst, const struct C_EventInfo* event,
	C_StatusCallbackFunc_t callback, void* handle)
{
	if(odc && odc->publish_event)
		odc->publish_event(inst, event, callback, handle);
}

static inline void odc_publish_connect_state(void* inst, int state)
{
	if(odc && odc->publish_connect_state)
		odc->publish_connect_state(inst, state);
}

static inline const char* odc_get_config_json(void* inst)
{
	if(odc && odc->get_config_json)
		return odc->get_config_json(inst);
	return NULL;
}

static inline void odc_log(void* inst, uint8_t level, const char* message)
{
	if(odc && odc->log)
		odc->log(inst, level, message);
}

static inline int odc_should_log(void* inst, uint8_t level)
{
	if(odc && odc->should_log)
		return odc->should_log(inst, level);
	return 0;
}

static inline void* odc_ms_timer_callback(void* inst, uint64_t ms,
	C_StatusCallbackFunc_t callback, void* handle)
{
	if(odc && odc->ms_timer_callback)
		return odc->ms_timer_callback(inst, ms, callback, handle);
	return NULL;
}

static inline void odc_cancel_timer(void* timer_handle)
{
	if(odc && odc->cancel_timer)
		odc->cancel_timer(timer_handle);
}

static inline void* odc_ms_repeating_callback(void* inst, uint64_t initial_ms,
	C_RepeatingCallbackFunc_t callback, void* handle)
{
	if(odc && odc->ms_repeating_callback)
		return odc->ms_repeating_callback(inst, initial_ms, callback, handle);
	return NULL;
}

static inline int odc_in_demand(void* inst)
{
	if(odc && odc->in_demand)
		return odc->in_demand(inst);
	return 0;
}

static inline uint64_t odc_ms_since_epoch(void)
{
	if(odc && odc->ms_since_epoch)
		return odc->ms_since_epoch();
	return 0;
}

static inline int odc_ms_since_epoch_to_datetime(uint64_t ms, const char* format,
	char* buf, size_t buflen)
{
	if(odc && odc->ms_since_epoch_to_datetime)
		return odc->ms_since_epoch_to_datetime(ms, format, buf, buflen);
	return -1;
}

static inline int odc_datetime_to_ms_since_epoch(const char* datetime,
	const char* format, uint64_t* out_ms)
{
	if(odc && odc->datetime_to_ms_since_epoch)
		return odc->datetime_to_ms_since_epoch(datetime, format, out_ms);
	return -1;
}

static inline size_t odc_string2hex(const uint8_t* data, size_t len,
	char* buf, size_t buflen)
{
	if(odc && odc->string2hex)
		return odc->string2hex(data, len, buf, buflen);
	return 0;
}

static inline int odc_hex2string(const char* hex, uint8_t* buf, size_t buflen)
{
	if(odc && odc->hex2string)
		return odc->hex2string(hex, buf, buflen);
	return -1;
}

static inline int odc_get_working_dir(char* buf, size_t buflen)
{
	if(odc && odc->get_working_dir)
		return odc->get_working_dir(buf, buflen);
	return -1;
}

static inline int odc_get_executable_dir(char* buf, size_t buflen)
{
	if(odc && odc->get_executable_dir)
		return odc->get_executable_dir(buf, buflen);
	return -1;
}

static inline int64_t odc_spawn_detached(const char* cmd, const char* const* argv)
{
	if(odc && odc->spawn_detached)
		return odc->spawn_detached(cmd, argv);
	return -1;
}

static inline int64_t odc_spawn_attached(const char* cmd, const char* const* argv,
	FILE** stdin_file, FILE** stdout_file, FILE** stderr_file)
{
	if(odc && odc->spawn_attached)
		return odc->spawn_attached(cmd, argv, stdin_file, stdout_file, stderr_file);
	return -1;
}

static inline int odc_kill_pid(int64_t pid, int sig)
{
	if(odc && odc->kill_pid)
		return odc->kill_pid(pid, sig);
	return -1;
}

static inline int odc_wait_pid(int64_t pid, int nohang, int* exit_code)
{
	if(odc && odc->wait_pid)
		return odc->wait_pid(pid, nohang, exit_code);
	return -1;
}

static inline const char* odc_event_type_to_string(uint8_t event_type)
{
	if(odc && odc->event_type_to_string)
		return odc->event_type_to_string(event_type);
	return "UNKNOWN";
}

static inline const char* odc_command_status_to_string(uint8_t status)
{
	if(odc && odc->command_status_to_string)
		return odc->command_status_to_string(status);
	return "UNKNOWN";
}

static inline const char* odc_control_code_to_string(uint8_t code)
{
	if(odc && odc->control_code_to_string)
		return odc->control_code_to_string(code);
	return "UNKNOWN";
}

static inline const char* odc_connect_state_to_string(uint8_t state)
{
	if(odc && odc->connect_state_to_string)
		return odc->connect_state_to_string(state);
	return "UNKNOWN";
}

static inline int odc_quality_flags_to_string(uint16_t flags, char* buf, size_t buflen)
{
	if(odc && odc->quality_flags_to_string)
		return odc->quality_flags_to_string(flags, buf, buflen);
	return -1;
}

/*  Payload setters (pure inline — no vtable needed)                  */
/* ------------------------------------------------------------------ */
static inline void odc_SetPayloadBinary(struct C_EventInfo* evt,
	uint8_t val)
{ evt->payload.binary_val = val; }

static inline void odc_SetPayloadAnalog(struct C_EventInfo* evt,
	double val)
{ evt->payload.analog_val = val; }

static inline void odc_SetPayloadOctetString(
	struct C_EventInfo* evt, const uint8_t* data, size_t len)
{
	evt->payload.octet_string.data = data;
	evt->payload.octet_string.size = len;
}

static inline void odc_SetPayloadConnectState(
	struct C_EventInfo* evt, uint8_t state)
{ evt->payload.connect_state = state; }

/* ------------------------------------------------------------------ */
/*  Payload getters (pure inline — no vtable needed)                  */
/* ------------------------------------------------------------------ */
static inline uint8_t odc_GetEventType(
	const struct C_EventInfo* evt)
{ return evt->event_type; }

static inline uint8_t odc_GetPayloadBinary(
	const struct C_EventInfo* evt)
{ return evt->payload.binary_val; }

static inline uint8_t odc_GetCROBFunctionCode(
	const struct C_EventInfo* evt)
{ return evt->payload.crob.function_code; }

static inline int16_t odc_GetAO16Value(
	const struct C_EventInfo* evt)
{ return evt->payload.ao16.value; }

static inline int32_t odc_GetAO32Value(
	const struct C_EventInfo* evt)
{ return evt->payload.ao32.value; }

static inline float odc_GetAOF32Value(
	const struct C_EventInfo* evt)
{ return evt->payload.aof32.value; }

static inline double odc_GetAOD64Value(
	const struct C_EventInfo* evt)
{ return evt->payload.aod64.value; }

static inline double odc_GetPayloadAnalog(
	const struct C_EventInfo* evt)
{ return evt->payload.analog_val; }

#endif /* GOMBUS_HELPERS_H */
