#ifndef ODC_C_API_H
#define ODC_C_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ODC_C_API_VERSION "1.0"
/* ------------------------------------------------------------------ */
/*  Version detection                                                 */
/*  If a shared library exports this symbol, the loader treats it     */
/*  as a C API library and creates the appropriate C++ wrapper.       */
/* ------------------------------------------------------------------ */
const char* odc_c_api_version(void);

/* ------------------------------------------------------------------ */
/*  Enums — values pinned to match odc::EventType etc.                */
/* ------------------------------------------------------------------ */
enum C_EventType
{
	C_EventType_Binary                  = 1,
	C_EventType_DoubleBitBinary         = 2,
	C_EventType_Analog                  = 3,
	C_EventType_Counter                 = 4,
	C_EventType_FrozenCounter           = 5,
	C_EventType_BinaryOutputStatus      = 6,
	C_EventType_AnalogOutputStatus      = 7,
	C_EventType_BinaryCommandEvent      = 8,
	C_EventType_AnalogCommandEvent      = 9,
	C_EventType_OctetString             = 10,
	C_EventType_TimeAndInterval         = 11,
	C_EventType_SecurityStat            = 12,
	C_EventType_ControlRelayOutputBlock = 16,
	C_EventType_AnalogOutputInt16       = 17,
	C_EventType_AnalogOutputInt32       = 18,
	C_EventType_AnalogOutputFloat32     = 19,
	C_EventType_AnalogOutputDouble64    = 20,
	C_EventType_TimeSync                = 22,
	C_EventType_BinaryQuality           = 24,
	C_EventType_DoubleBitBinaryQuality  = 25,
	C_EventType_AnalogQuality           = 26,
	C_EventType_CounterQuality          = 27,
	C_EventType_BinaryOutputStatusQuality = 28,
	C_EventType_FrozenCounterQuality    = 29,
	C_EventType_AnalogOutputStatusQuality = 30,
	C_EventType_OctetStringQuality      = 31,
	C_EventType_FileAuth                = 34,
	C_EventType_FileCommand             = 35,
	C_EventType_FileCommandStatus       = 36,
	C_EventType_FileTransport           = 37,
	C_EventType_FileTransportStatus     = 38,
	C_EventType_FileDescriptor          = 39,
	C_EventType_FileSpecString          = 40,
	C_EventType_ConnectState            = 44
};

enum C_CommandStatus
{
	C_CommandStatus_SUCCESS               = 0,
	C_CommandStatus_TIMEOUT               = 1,
	C_CommandStatus_NO_SELECT             = 2,
	C_CommandStatus_FORMAT_ERROR          = 3,
	C_CommandStatus_NOT_SUPPORTED         = 4,
	C_CommandStatus_ALREADY_ACTIVE        = 5,
	C_CommandStatus_HARDWARE_ERROR        = 6,
	C_CommandStatus_LOCAL                 = 7,
	C_CommandStatus_TOO_MANY_OPS          = 8,
	C_CommandStatus_NOT_AUTHORIZED        = 9,
	C_CommandStatus_AUTOMATION_INHIBIT    = 10,
	C_CommandStatus_PROCESSING_LIMITED    = 11,
	C_CommandStatus_OUT_OF_RANGE          = 12,
	C_CommandStatus_DOWNSTREAM_LOCAL      = 13,
	C_CommandStatus_ALREADY_COMPLETE      = 14,
	C_CommandStatus_BLOCKED               = 15,
	C_CommandStatus_CANCELLED             = 16,
	C_CommandStatus_BLOCKED_OTHER_MASTER  = 17,
	C_CommandStatus_DOWNSTREAM_FAIL       = 18,
	C_CommandStatus_NON_PARTICIPATING     = 126,
	C_CommandStatus_UNDEFINED             = 127
};

enum C_ControlCode
{
	C_ControlCode_NUL            = 1,
	C_ControlCode_PULSE_ON       = 3,
	C_ControlCode_PULSE_OFF      = 5,
	C_ControlCode_LATCH_ON       = 7,
	C_ControlCode_LATCH_OFF      = 9,
	C_ControlCode_CLOSE_PULSE_ON = 11,
	C_ControlCode_TRIP_PULSE_ON  = 13,
	C_ControlCode_UNDEFINED      = 15
};

enum C_ConnectState
{
	C_ConnectState_PORT_UP      = 0,
	C_ConnectState_CONNECTED    = 1,
	C_ConnectState_DISCONNECTED = 2,
	C_ConnectState_PORT_DOWN    = 3,
	C_ConnectState_UNDEFINED    = 4
};

/* Log levels — integer values match spdlog::level_enum                     */
#define C_LOG_LEVEL_TRACE     0
#define C_LOG_LEVEL_DEBUG     1
#define C_LOG_LEVEL_INFO      2
#define C_LOG_LEVEL_WARN      3
#define C_LOG_LEVEL_ERROR     4
#define C_LOG_LEVEL_CRITICAL  5
#define C_LOG_LEVEL_OFF       6

/* Convenience log macros for native C ports.
 * These assume the port stores its host API vtable pointer as 'odc'
 * (the conventional name set by odc_library_init).  A port that uses a
 * different variable name can define its own equivalents. */
#define odc_LogTrace(inst, msg)    odc->log((inst), C_LOG_LEVEL_TRACE,    (msg))
#define odc_LogDebug(inst, msg)    odc->log((inst), C_LOG_LEVEL_DEBUG,    (msg))
#define odc_LogInfo(inst, msg)     odc->log((inst), C_LOG_LEVEL_INFO,     (msg))
#define odc_LogWarn(inst, msg)     odc->log((inst), C_LOG_LEVEL_WARN,     (msg))
#define odc_LogError(inst, msg)    odc->log((inst), C_LOG_LEVEL_ERROR,    (msg))
#define odc_LogCritical(inst, msg) odc->log((inst), C_LOG_LEVEL_CRITICAL, (msg))

/* Bit flags — values match odc::QualityFlags */
enum C_QualityFlags
{
	C_QualityFlags_NONE          = 0,
	C_QualityFlags_ONLINE        = 1,
	C_QualityFlags_RESTART       = 2,
	C_QualityFlags_COMM_LOST     = 4,
	C_QualityFlags_REMOTE_FORCED = 8,
	C_QualityFlags_LOCAL_FORCED  = 16,
	C_QualityFlags_OVERRANGE     = 32,
	C_QualityFlags_REFERENCE_ERR = 64,
	C_QualityFlags_ROLLOVER      = 128,
	C_QualityFlags_DISCONTINUITY = 256,
	C_QualityFlags_CHATTER_FILTER = 512
};

/* ------------------------------------------------------------------ */
/*  Structs                                                           */
/* ------------------------------------------------------------------ */

struct C_ControlRelayOutputBlock
{
	uint8_t function_code; /* C_ControlCode */
	uint8_t count;
	uint32_t on_time_ms;
	uint32_t off_time_ms;
	uint8_t status; /* C_CommandStatus */
};

/* Tagged union covering every EventType payload.
   The tag is the C_EventType stored in C_EventInfo.event_type. */
union C_Payload
{
	/* Binary, BinaryOutputStatus */
	uint8_t binary_val;

	/* DoubleBitBinary */
	struct { uint8_t a; uint8_t b; } dbb_val;

	/* Analog, AnalogOutputStatus */
	double analog_val;

	/* Counter, FrozenCounter */
	uint32_t counter_val;

	/* BinaryCommandEvent, AnalogCommandEvent */
	uint8_t cmd_status; /* C_CommandStatus */

	/* OctetString — borrowed pointer, valid only during callback */
	struct { const uint8_t* data; size_t size; } octet_string;

	/* TimeAndInterval */
	struct { uint64_t time; uint32_t interval; uint8_t sequence; } tai;

	/* SecurityStat */
	struct { uint16_t assoc_id; uint32_t stat; } security_stat;

	/* ControlRelayOutputBlock */
	struct C_ControlRelayOutputBlock crob;

	/* AnalogOutputInt16 */
	struct { int16_t value; uint8_t status; } ao16;

	/* AnalogOutputInt32 */
	struct { int32_t value; uint8_t status; } ao32;

	/* AnalogOutputFloat32 */
	struct { float value; uint8_t status; } aof32;

	/* AnalogOutputDouble64 */
	struct { double value; uint8_t status; } aod64;

	/* Quality event types: BinaryQuality, DoubleBitBinaryQuality, etc. */
	uint16_t quality_val; /* C_QualityFlags */

	/* ConnectState */
	uint8_t connect_state; /* C_ConnectState */

	/* TimeSync */
	struct { uint64_t abs_time_ms; int64_t sys_offset_ms; } time_sync;

	/* Stub types (File*, Reserved*) — not commonly used */
	uint8_t stub;
};

struct C_EventInfo
{
	uint8_t event_type; /* C_EventType */
	size_t index;
	uint64_t timestamp;      /* ms since epoch */
	uint16_t quality;        /* C_QualityFlags bitmask */
	const char* source_port; /* borrowed, valid during callback */
	union C_Payload payload;
};

/* Opaque status callback handle.
   Created by the C++ wrapper, passed to C code in odc_port_event().
   Must be invoked exactly once via odc_InvokeStatusCallback(),
   then it is released and zeroed. */
typedef struct C_StatusCallback C_StatusCallback;

/* Function pointer for status/timer callbacks.
   status: a C_CommandStatus value.
   handle: user-provided context pointer (passed through from the caller). */
typedef void (*C_StatusCallbackFunc_t)(uint8_t status, void* handle);

/* Opaque pass context for transforms.
   Created by the C++ wrapper, passed to C code in odc_transform_event(). */
typedef struct C_PassContext C_PassContext;

/* ------------------------------------------------------------------- */
/*  Host API vtable                                                    */
/*  Populated by the host (CAPI) and passed to each library via       */
/*  odc_library_init().  Port code calls host services through this   */
/*  struct; no link-time dependency on the host binary is required.   */
/* ------------------------------------------------------------------- */

/* Callback for repeating timers — return ms to wait before next call, or
   a negative value to stop the repetition. */
typedef int64_t (*C_RepeatingCallbackFunc_t)(void* handle);

struct C_ODC_HostAPI
{
	/* Invoke a status callback exactly once (then nulls *cb). */
	void (*invoke_status_callback)(C_StatusCallback** cb, uint8_t status);

	/* Publish an event from this port to all subscribers. */
	void (*publish_event)(void* inst, const struct C_EventInfo* event,
		C_StatusCallbackFunc_t callback, void* handle);

	/* Publish a connection-state change. */
	void (*publish_connect_state)(void* inst, int state);

	/* Retrieve the full resolved config JSON for a port instance.
	   Returned string is owned by the host; valid for the port's lifetime. */
	const char* (*get_config_json)(void* inst);

	/* Log a message.  level: C_LOG_LEVEL_TRACE etc. */
	void (*log)(void* inst, uint8_t level, const char* message);

	/* Returns non-zero if the given log level will produce output. */
	int (*should_log)(void* inst, uint8_t level);

	/* Schedule a one-shot timer; returns opaque handle or NULL on error.
	   Cancel with cancel_timer(). */
	void* (*ms_timer_callback)(void* inst, uint64_t ms,
		C_StatusCallbackFunc_t callback, void* handle);

	/* Cancel a timer created by ms_timer_callback(). */
	void (*cancel_timer)(void* timer_handle);

	/* Schedule a repeating timer; returns opaque handle.
	   Cancel with cancel_timer(). */
	void* (*ms_repeating_callback)(void* inst, uint64_t initial_ms,
		C_RepeatingCallbackFunc_t callback, void* handle);

	/* Returns non-zero if the port has at least one event subscriber. */
	int (*in_demand)(void* inst);

	/* Current time as milliseconds since the Unix epoch. */
	uint64_t (*ms_since_epoch)(void);

	/* Convert ms-since-epoch to a datetime string (see odc_msSinceEpochToDateTime). */
	int (*ms_since_epoch_to_datetime)(uint64_t ms, const char* format,
		char* buf, size_t buflen);

	/* Parse a datetime string to ms-since-epoch. */
	int (*datetime_to_ms_since_epoch)(const char* datetime, const char* format,
		uint64_t* out_ms);

	/* Binary → hex string. */
	size_t (*string2hex)(const uint8_t* data, size_t len,
		char* buf, size_t buflen);

	/* Hex string → binary. */
	int (*hex2string)(const char* hex, uint8_t* buf, size_t buflen);

	/* Write the canonical working-directory path into buf. */
	int (*get_working_dir)(char* buf, size_t buflen);

	/* Write the opendatacon executable directory into buf. */
	int (*get_executable_dir)(char* buf, size_t buflen);

	/* Spawn a detached process (no pipes). */
	int64_t (*spawn_detached)(const char* cmd, const char* const* argv);

	/* Spawn a process with stdin/stdout/stderr pipes. */
	int64_t (*spawn_attached)(const char* cmd, const char* const* argv,
		FILE** stdin_file, FILE** stdout_file, FILE** stderr_file);

	/* Send signal sig to process pid. */
	int (*kill_pid)(int64_t pid, int sig);

	/* Wait for process exit. */
	int (*wait_pid)(int64_t pid, int nohang, int* exit_code);

	/* Enum → string helpers. */
	const char* (*event_type_to_string)(uint8_t event_type);
	const char* (*command_status_to_string)(uint8_t status);
	const char* (*control_code_to_string)(uint8_t code);
	const char* (*connect_state_to_string)(uint8_t state);
	int (*quality_flags_to_string)(uint16_t flags, char* buf, size_t buflen);
};

/* ------------------------------------------------------------------ */
/*  Required exports from a C Port library                            */
/* ------------------------------------------------------------------ */

/* Create a port instance. Returns an opaque handle that is the C
   code's own per-instance state. The C++ side uses this same pointer
   as a lookup key for publish helpers.
   type and name are borrowed — copy if needed.
   type is the port type from the config (allows one library to
   handle multiple types). name is the port's unique name. */
void* odc_port_create(const char* type,
	const char* name);

/* Destroy a port instance created by odc_port_create(). */
void odc_port_destroy(void* inst);

/* Build/initialise the port (parse config, open sockets, etc.). */
void odc_port_build(void* inst);

void odc_port_enable(void* inst);
void odc_port_disable(void* inst);

/* Receive an event from other ports.
   cb is an opaque status callback handle. It MUST be invoked exactly
   once via odc_InvokeStatusCallback(&cb, status).
   After invocation, *cb is nulled.
   If C code does not need to report status, invoke with SUCCESS. */
void odc_port_event(void* inst,
	const struct C_EventInfo* event,
	const char* sender,
	C_StatusCallback* cb);

/* Called exactly once by the host immediately after dlopen/LoadLibrary,
   before any odc_port_create() calls.  Provides the host API vtable so
   the library has zero link-time symbol dependencies on the host. */
void odc_library_init(struct C_ODC_HostAPI* odc);

/* ------------------------------------------------------------------ */
/*  Optional exports from a C Port library — return JSON strings      */
/*  allocated by C (malloc/strdup). The C++ side will call            */
/*  odc_port_free_string() to release them.                           */
/* ------------------------------------------------------------------ */
const char* odc_port_stats_json(void* inst);
const char* odc_port_state_json(void* inst);
const char* odc_port_status_json(void* inst);
void        odc_port_free_string(const char* str);

/* ------------------------------------------------------------------ */
/*  Required exports from a C Transform library                       */
/* ------------------------------------------------------------------ */

void* odc_transform_create(const char* name, const char* params_json);
void  odc_transform_destroy(void* inst);
void  odc_transform_enable(void* inst);
void  odc_transform_disable(void* inst);

/* Transform an event. Call pass(pass_ctx, &event) to forward the
   (possibly modified) event downstream, or skip the call to drop it. */
void odc_transform_event(void* inst,
	struct C_EventInfo* event,
	C_PassContext* pass_ctx,
	void (*pass)(C_PassContext* ctx, struct C_EventInfo* evt));

/* ------------------------------------------------------------------ */
/*  Required exports from a C Plugin (IUI) library                    */
/* ------------------------------------------------------------------ */

void* odc_plugin_create(const char* name,
	const char* conf_filename,
	const char* conf_overrides_json);
void  odc_plugin_destroy(void* inst);
void  odc_plugin_build(void* inst);
void  odc_plugin_enable(void* inst);
void  odc_plugin_disable(void* inst);

#ifdef __cplusplus
}
#endif

#endif /* ODC_C_API_H */
