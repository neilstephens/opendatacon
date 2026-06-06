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

/* Convenience macros — no magic numbers needed in C code.                */
#define odc_LogTrace(inst, msg)    odc_Log((inst), C_LOG_LEVEL_TRACE, (msg))
#define odc_LogDebug(inst, msg)    odc_Log((inst), C_LOG_LEVEL_DEBUG, (msg))
#define odc_LogInfo(inst, msg)     odc_Log((inst), C_LOG_LEVEL_INFO, (msg))
#define odc_LogWarn(inst, msg)     odc_Log((inst), C_LOG_LEVEL_WARN, (msg))
#define odc_LogError(inst, msg)    odc_Log((inst), C_LOG_LEVEL_ERROR, (msg))
#define odc_LogCritical(inst, msg) odc_Log((inst), C_LOG_LEVEL_CRITICAL, (msg))

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

/* ------------------------------------------------------------------- */
/*  Helper functions - implemented by the C++ wrapper, callable from C */
/* ------------------------------------------------------------------- */

/* Invoke a status callback exactly once.
   Pass the ADDRESS of the callback pointer so it can be zeroed: e.g.
   odc_InvokeStatusCallback(&cb, C_CommandStatus_SUCCESS); */
void odc_InvokeStatusCallback(C_StatusCallback** cb, uint8_t status);

/* Publish an event from this port to all subscribers.
   inst is the void* returned by odc_port_create().
   callback is an optional function pointer invoked once with the downstream
   CommandStatus result. handle is passed back to callback unchanged. */
void odc_PublishEvent(void* inst, const struct C_EventInfo* event,
	C_StatusCallbackFunc_t callback, void* handle);

/* Publish a connection state change. */
void odc_PublishConnectState(void* inst, int state);

/* Retrieve the full resolved config JSON for a port instance.
   The returned string is owned by the C++ wrapper and valid for the
   lifetime of the port. Valid from odc_port_build() onwards. */
const char* odc_GetConfigJSON(void* inst);

/* Log a message using the port's logger.
   level: C_LOG_LEVEL_TRACE etc. Use the odc_LogTrace/Debug/... convenience
   macros to avoid magic numbers. */
void odc_Log(void* inst, uint8_t level, const char* message);

/* Check whether the given log level will produce output.
   Returns non-zero if a message at this level would be logged, zero otherwise.
   Use to avoid expensive string formatting when logging is disabled. */
int odc_ShouldLog(void* inst, uint8_t level);

/* Schedule a one-shot timer.
   inst is the void* returned by odc_port_create() (used for strand dispatch).
   After ms milliseconds, callback is invoked with C_CommandStatus_SUCCESS
   (or C_CommandStatus_UNDEFINED if cancelled). handle is passed back.
   Returns an opaque timer handle, or NULL on error.
   The timer can be cancelled with odc_cancelTimer(). */
void* odc_msTimerCallback(void* inst, uint64_t ms,
	C_StatusCallbackFunc_t callback, void* handle);

/* Cancel a timer created by odc_msTimerCallback().
   timer_handle is the opaque pointer returned by odc_msTimerCallback().
   After this call, the timer's callback will not fire.
   The handle is freed by this function and must not be used again. */
void odc_cancelTimer(void* timer_handle);

/* Callback for repeating timers — return ms to wait before next call, or
   a negative value to stop the repetition. */
typedef int64_t (*C_RepeatingCallbackFunc_t)(void* handle);

/* ------------------------------------------------------------------ */
/*  Port introspection                                                */
/* ------------------------------------------------------------------ */

/* Returns non-zero if the port has at least one event subscriber.
   Returns 0 for non-port instances (transforms, UIs) or unknown inst. */
int odc_InDemand(void* inst);

/* ------------------------------------------------------------------ */
/*  Date/time utilities (no inst needed)                              */
/* ------------------------------------------------------------------ */

/* Current time as milliseconds since the Unix epoch. */
uint64_t odc_msSinceEpoch(void);

/* Convert ms-since-epoch to a datetime string.
   format: strftime-style with %e for millisecond sub-field;
           NULL uses the default "%Y-%m-%d %H:%M:%S.%e".
   Writes a NUL-terminated string to buf.
   Returns 0 on success, non-zero on error. */
int odc_msSinceEpochToDateTime(uint64_t ms, const char* format,
	char* buf, size_t buflen);

/* Parse a datetime string to ms-since-epoch.
   format: NULL uses the default "%Y-%m-%d %H:%M:%S.%e".
   Sets *out_ms on success.
   Returns 0 on success, non-zero on error. */
int odc_DateTimeToMsSinceEpoch(const char* datetime, const char* format,
	uint64_t* out_ms);

/* ------------------------------------------------------------------ */
/*  Hex encode / decode (no inst needed)                              */
/* ------------------------------------------------------------------ */

/* Convert binary data to a lowercase hex string.
   Returns the number of hex characters that would be written (excluding
   the NUL terminator).  If buf is non-NULL and buflen is sufficient the
   NUL-terminated hex string is written.  Safe to call with buf==NULL to
   query the required buffer size. */
size_t odc_String2Hex(const uint8_t* data, size_t len,
	char* buf, size_t buflen);

/* Convert a hex string to binary data.
   Returns the number of bytes written, or -1 on error (odd length or
   non-hex characters).  If buf is NULL returns the number of bytes that
   would be written without performing the conversion. */
int odc_Hex2String(const char* hex, uint8_t* buf, size_t buflen);

/* ------------------------------------------------------------------ */
/*  Path resolution (no inst needed)                                  */
/* ------------------------------------------------------------------ */

/* Write the canonical working-directory path into buf.
   Returns 0 on success, non-zero on error. */
int odc_GetWorkingDir(char* buf, size_t buflen);

/* Write the directory that contains the opendatacon executable into buf.
   Returns 0 on success, non-zero on error. */
int odc_GetExecutableDir(char* buf, size_t buflen);

/* ------------------------------------------------------------------ */
/*  Process spawning (no inst needed)                                 */
/* ------------------------------------------------------------------ */

/* Spawn a detached process (no pipes; fire-and-forget).
   argv: NULL-terminated array of argument strings passed to the process.
         argv[0] is the program path (same as cmd) and is ignored — cmd
         is always used as the executable.
   Returns the PID on success, or -1 on error. */
int64_t odc_SpawnDetached(const char* cmd, const char* const* argv);

/* Spawn a process with stdin/stdout/stderr pipes.
   Sets *stdin_file, *stdout_file, *stderr_file to open FILE* handles
   owned by the caller (call fclose() when finished with each).
   On error all file pointers are set to NULL and -1 is returned. */
int64_t odc_SpawnAttached(const char* cmd, const char* const* argv,
	FILE** stdin_file, FILE** stdout_file, FILE** stderr_file);

/* Send signal sig to process pid.
   Use 0 (SIGEXIT) to check liveness without killing.
   Returns 0 on success, non-zero on error. */
int odc_KillPid(int64_t pid, int sig);

/* Wait for a process to exit.
   nohang non-zero: return immediately if the process has not yet exited.
   Returns 1 if the process has exited (sets *exit_code),
           0 if it is still running (nohang only),
          -1 on error. */
int odc_WaitPid(int64_t pid, int nohang, int* exit_code);

/* ------------------------------------------------------------------ */
/*  Repeating timer                                                   */
/* ------------------------------------------------------------------ */

/* Schedule a repeating timer on the port's sync strand.
   callback(handle) fires after initial_ms milliseconds.  The value it
   returns is the delay (ms) before the next call.  Returning a negative
   value stops the repetition.
   Returns an opaque timer handle; cancel (and free) it with
   odc_cancelTimer(). */
void* odc_msRepeatingCallback(void* inst, uint64_t initial_ms,
	C_RepeatingCallbackFunc_t callback, void* handle);

/* ------------------------------------------------------------------ */
/*  Enum -> string helpers (no inst needed)                           */
/* ------------------------------------------------------------------ */

/* Return a static, NUL-terminated name string for the given enum value.
   Unknown values return "UNKNOWN".  The returned pointer is never NULL
   and the string has static lifetime — do not free it. */
const char* odc_EventTypeToString(uint8_t event_type);
const char* odc_CommandStatusToString(uint8_t status);
const char* odc_ControlCodeToString(uint8_t code);
const char* odc_ConnectStateToString(uint8_t state);

/* Write a human-readable QualityFlags string (e.g. "ONLINE|COMM_LOST")
   into buf.  Returns 0 on success, non-zero if the buffer was too small
   (buf will still be NUL-terminated and contain a truncated value). */
int odc_QualityFlagsToString(uint16_t flags, char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* ODC_C_API_H */
