#ifndef GOMBUS_HELPERS_H
#define GOMBUS_HELPERS_H

#include <stddef.h>
#include <stdint.h>
#include "opendatacon/odc_c_api.h"

/* Safe callback-invocation wrapper: takes the C_StatusCallback* as a
   void* so binding code does not need to form a pointer-to-pointer. */
static inline void odc_InvokeStatusCallbackSafe(void* cb_ptr,
	uint8_t status)
{
	if(cb_ptr != NULL)
	{
		C_StatusCallback* cb = (C_StatusCallback*)cb_ptr;
		odc_InvokeStatusCallback(&cb, status);
	}
}

/* Payload setters */
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

/* Payload getters */
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
