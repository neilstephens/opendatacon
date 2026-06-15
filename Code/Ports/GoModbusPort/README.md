# GoModbusPort — Modbus TCP/RTU Client and Server via Go

A Modbus port plugin for opendatacon that uses [simonvetter/modbus](https://github.com/simonvetter/modbus) (a pure-Go
Modbus library) via Go's `c-shared` build mode.  This port is an alternative to the C++ Modbus port
(`ModbusPort`) and was created to evaluate whether Go can serve as a viable port-development language
within the opendatacon C API framework.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Features](#features)
3. [Configuration Reference](#configuration-reference)
4. [Architecture](#architecture)
5. [Concurrency Model](#concurrency-model)
6. [Point Expansion](#point-expansion)
7. [Endian / DataType Reference](#endian--datatype-reference)
8. [Building](#building)
9. [Testing](#testing)
10. [Code Structure](#code-structure)
11. [Known Limitations](#known-limitations)

---

## Quick Start

### Client mode — polling a Modbus TCP device

```json
{
  "Ports": [
    {
      "Name": "MyPLC",
      "Type": "GoModbusClient",
      "Conf": {
        "TCP": { "Address": "192.168.1.100:502" },
        "UnitID": 1,
        "TimeoutMs": 2000,
        "PollRateMs": 1000,
        "Binaries": [
          { "Index": 0, "Modbus": { "Type": "Coil", "Address": 0 } },
          { "Index": 1, "Modbus": { "Type": "DiscreteInput", "Address": 0 } }
        ],
        "Analogs": [
          { "Index": 0, "Modbus": { "Type": "HoldingRegister", "Address": 100 }, "Scale": 0.1, "DataType": "Int16" },
          { "Index": 1, "Modbus": { "Type": "InputRegister", "Address": 200 }, "Endian": "DCBA", "DataType": "Float32" }
        ]
      }
    }
  ]
}
```

### Server mode — exposing ODC events as a Modbus device

```json
{
  "Ports": [
    {
      "Name": "MyServer",
      "Type": "GoModbusServer",
      "Conf": {
        "TCP": { "Listen": "0.0.0.0:502" },
        "UnitID": 1,
        "Binaries": [
          { "Index": 0, "Modbus": { "Type": "Coil", "Address": 0 } }
        ],
        "Analogs": [
          { "Index": 10, "Modbus": { "Type": "HoldingRegister", "Address": 100, "Count": 2 }, "Endian": "ABCD", "DataType": "Float32" }
        ],
        "BinaryControls": [
          { "Index": 20, "Modbus": { "Type": "Coil", "Address": 50 }, "OnAction": "On", "OffAction": "Off" }
        ],
        "AnalogControls": [
          { "Index": 30, "Modbus": { "Type": "HoldingRegister", "Address": 200, "Count": 2 }, "ControlType": "AnalogOutputFloat32" }
        ]
      }
    }
  ]
}
```

---

## Features

- **Client (polling) mode** — `"Type": "GoModbusClient"`
  - Polls Modbus TCP/RTU devices on a configurable schedule
  - Supports all Modbus data types: Coils, Discrete Inputs, Holding Registers, Input Registers
  - Per-point or global poll rate; `Range` notation for bulk point configuration
  - Configurable endianness and data-type interpretation for register-based values
  - Scale/Offset transformation on analog values
  - Bit extraction from holding registers for binary points
  - OctetString support (register pairs → byte array)
  - Outbound control writes (CROB/Binary/Analog → WriteCoil/WriteRegister)
  - Automatic reconnection with exponential back-off (1–30 s)
  - Transport-disconnect detection triggers reconnect cycle
  - Configurable concurrency via `MaxConcurrentPolls` (default 1)
  - Demand-aware polling: `odc_in_demand()` guard skips polls when no subscriber is connected

- **Server (slave) mode** — `"Type": "GoModbusServer"`
  - Exposes ODC events as a Modbus TCP device (RTU server not supported by the underlying library)
  - Bidirectional mapping:
    - ODC Binary/Analog/OctetString events → update internal data store → readable by Modbus clients
    - Modbus client coil writes → ODC Binary events published into the data concentrator
    - Modbus client holding register writes → ODC AnalogOutput events published
  - Full endian/data-type decoding for analog control writes
  - Concurrent client support (thread-safe `sync.RWMutex`-protected data store)
  - Pre-allocated data store (registers default to zero)

- **Both modes**
  - TCP (IPv4/IPv6) and RTU (serial) transport
  - Configurable unit ID filtering
  - ODC log integration (trace/debug/info/warn/error/critical via `odc_log`)
  - Port stats, state, and status JSON for monitoring
  - Server state JSON follows DNP3/JSON port convention: point-value arrays wrapped in a UTC timestamp key
  - Full C API compliance via `api_shim.c` + `gombus_helpers.h`

---

## Configuration Reference

### Client port (`PortConfig`)

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `TCP` | object | (see note) | — | `{ "Address": "host:port" }` — TCP connection target |
| `RTU` | object | (see note) | — | `{ "Port": "...", "BaudRate": 19200, "DataBits": 8, "StopBits": 1, "Parity": "N" }` |
| `UnitID` | integer | no | 0 | Modbus unit/slave ID |
| `TimeoutMs` | integer | no | 1000 | Per-request timeout |
| `PollRateMs` | integer | no | 1000 | Default poll interval for all points (overridable per point) |
| `MaxConcurrentPolls` | integer | no | 1 | Maximum concurrent poll goroutines (see [Known Limitations](#known-limitations)) |
| `Binaries` | array | no | [] | Coil/DiscreteInput poll points |
| `Analogs` | array | no | [] | HoldingRegister/InputRegister poll points (with optional Scale/Offset/Endian/DataType) |
| `BinaryOutputStatuses` | array | no | [] | Same structure as Binaries; mapped to ODC BinaryOutputStatus type |
| `AnalogOutputStatuses` | array | no | [] | Same structure as Analogs; mapped to ODC AnalogOutputStatus type |
| `OctetStrings` | array | no | [] | HoldingRegister/InputRegister blocks mapped to ODC OctetString |
| `BinaryControls` | array | no | [] | Outbound control mapped to WriteCoil (see [ControlConfig](#controlconfig)) |
| `AnalogControls` | array | no | [] | Outbound control mapped to WriteRegister (see [AnalogControlConfig](#analogcontrolconfig)) |

Exactly one of `TCP` or `RTU` must be specified.

### Server port (`ServerPortConfig`)

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `TCP` | object | yes | — | `{ "Listen": "host:port" }` — TCP listen address |
| `RTU` | object | no | — | Currently not supported (underlying library limitation); use TCP |
| `UnitID` | integer | no | 0 | 0 = respond to all unit IDs |
| `Binaries` | array | no | [] | Readable by Modbus clients (ODC events → store) |
| `Analogs` | array | no | [] | Same; with optional Scale/Offset/Endian/DataType |
| `BinaryOutputStatuses` | array | no | [] | Same as Binaries; ODC type = BinaryOutputStatus |
| `AnalogOutputStatuses` | array | no | [] | Same as Analogs; ODC type = AnalogOutputStatus |
| `OctetStrings` | array | no | [] | Register block exposed as OctetString |
| `BinaryControls` | array | no | [] | Writable by Modbus clients (coil write → ODC event). Only `"Type": "Coil"` is valid. |
| `AnalogControls` | array | no | [] | Writable by Modbus clients (HR write → ODC event). Only `"Type": "HoldingRegister"` is valid. |

### Common point config

```json
{
  "Index": 0,
  "Range": { "Start": 0, "Stop": 9 },           // optional; expands to 10 points
  "Modbus": { "Type": "Coil", "Address": 100, "Count": 1 },
  "PollRateMs": 500,                               // per-point poll rate (client only)
  "Bit": 3                                         // bit extraction from register (client only)
}
```

| Field | Type | Default | Description |
|---|---|---|---|
| `Index` | integer | required | ODC point index |
| `Range` | object | null | `{ "Start": N, "Stop": M }` — expands to M-N+1 points starting at Index=Start, with Modbus address stride = Count |
| `Modbus.Type` | string | required | One of: `Coil`, `DiscreteInput`, `HoldingRegister`, `InputRegister` |
| `Modbus.Address` | integer | required | Modbus register/coil address |
| `Modbus.Count` | integer | 1 | Number of consecutive registers (for multi-register types like Float32=2, Float64=4) |
| `PollRateMs` | integer | global `PollRateMs` | Per-point poll rate (client only) |
| `Bit` | integer | null (sentinel -1) | When set, point is polled as a register and the specified bit is extracted as a binary value (client only) |

### Analog-specific fields (`AnalogPointConfig`)

| Field | Type | Default | Description |
|---|---|---|---|
| `Scale` | float | 0 | `raw_value * Scale + Offset` applied to polled values (client); inverse applied for server store writes |
| `Offset` | float | 0 | See Scale |
| `Endian` | string | `"ABCD"` | Byte/word order for multi-register values (see [Endian reference](#endian--datatype-reference)) |
| `DataType` | string | `"Int16"` (count=1), `"Int32"` (count=2) | Interpretation of raw register(s): `Int16`, `Uint16`, `Int32`, `Uint32`, `Float32`, `Float64` |

### ControlConfig

| Field | Type | Default | Description |
|---|---|---|---|
| `OnAction` | string | `"On"` | Coil value to write for ON/LATCH_ON/CROB-on events. Any value other than "Off" is treated as ON. |
| `OffAction` | string | `"Off"` | Coil value to write for OFF/LATCH_OFF/CROB-off events. `"Off"` = false, anything else = true. |

### AnalogControlConfig

| Field | Type | Default | Description |
|---|---|---|---|
| `ControlType` | string | `"AnalogOutputInt16"` | ODC event type for the published value: `AnalogOutputInt16`, `AnalogOutputInt32`, `AnalogOutputFloat32`, `AnalogOutputDouble64` |
| `Endian` | string | `"ABCD"` | Byte/word order for decoding raw Modbus registers before publishing the ODC event |
| `DataType` | string | `"Int16"` | How to interpret the raw Modbus register(s) |

---

## Port Monitoring

Each port exposes three JSON endpoints accessible via the ODC C API (`go_port_stats_json`,
`go_port_state_json`, `go_port_status_json`).

### Client stats

```json
{
  "PollsScheduled": 42,
  "PollsDropped": 0,
  "PollsRunning": 0,
  "MaxConcurrentPolls": 1
}
```

| Field | Description |
|---|---|
| `PollsScheduled` | Total poll cycles enqueued by the ticker |
| `PollsDropped` | Poll cycles dropped because `MaxConcurrentPolls` limit was reached |
| `PollsRunning` | Currently executing poll goroutines (should normally be 0) |
| `MaxConcurrentPolls` | Configured concurrency limit |

### Server state

Returns current point values and operational metadata. The top-level key is a UTC
timestamp (matching the `since_epoch_to_datetime` format used by DNP3/JSON ports).

```json
{
  "2026-06-13 12:00:00.000": {
    "InDemand": true,
    "Binaries": [
      {"Index":0,"Value":true,"Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"},
      {"Index":1,"Value":false,"Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"}
    ],
    "Analogs": [
      {"Index":10,"Value":123.4,"Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"}
    ],
    "OctetStrings": [
      {"Index":20,"Value":"abcdef0123456789","Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"}
    ],
    "BinaryControls": [
      {"Index":30,"Value":true,"Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"}
    ],
    "AnalogControls": [
      {"Index":40,"Value":567.8,"Quality":"ONLINE","Timestamp":"2026-06-13 12:00:00.000"}
    ]
  }
}
```

| Field | Description |
|---|---|
| `InDemand` | Whether the port has at least one active subscriber |
| `Binaries` | Array of binary point values (Coils, DiscreteInputs) from the data store |
| `Analogs` | Array of analog point values (HoldingRegisters, InputRegisters), Scale/Offset applied |
| `OctetStrings` | Array of octet-string values (register blocks), hex-encoded |
| `BinaryControls` | Array of control-output binary values (simulates a Modbus client reading its own last-written coils) |
| `AnalogControls` | Array of control-output analog values (same for holding registers), DataType/Endian decoded |

Each point object contains `Index`, `Value`, `Quality` (always `"ONLINE"`), and
`Timestamp` — following the DNP3/JSON port snapshot convention.

The response is `{}` (empty object) when the port is not built, disabled, or has
no store.

### Server status

Combines the operational metadata and runtime counters:

```json
{
  "Enabled": true,
  "Built": true,
  "Running": true,
  "ListenAddress": "0.0.0.0:502",
  "UnitID": 1,
  "UptimeMs": 1234567,
  "EventsReceived": 42,
  "EventsApplied": 40,
  "WritesPublished": 15,
  "ModbusClientReads": 128,
  "StoreCoils": 100,
  "StoreDiscreteInputs": 50,
  "StoreHoldingRegisters": 200,
  "StoreInputRegisters": 50
}
```

| Field | Description |
|---|---|
| `Enabled` | Whether the port is enabled and the select-loop goroutine is running |
| `Built` | Whether `build()` completed successfully |
| `Running` | Whether the Modbus TCP server is started and accepting connections |
| `ListenAddress` | The configured TCP listen address |
| `UnitID` | The configured unit ID filter (0 = respond to all) |
| `UptimeMs` | Milliseconds since `enable()`, or 0 if never enabled |
| `EventsReceived` | Total ODC events received (Binary/Analog/OctetString) |
| `EventsApplied` | ODC events successfully matched and written to the data store |
| `WritesPublished` | Modbus client coil/register writes published as ODC events |
| `ModbusClientReads` | Modbus client read requests served (coils, DI, HR, IR) |
| `StoreCoils` | Number of coil addresses configured in the data store |
| `StoreDiscreteInputs` | Number of discrete-input addresses configured |
| `StoreHoldingRegisters` | Number of holding-register addresses configured |
| `StoreInputRegisters` | Number of input-register addresses configured |

### Server stats

Performance-counter subset of status (no operational metadata or store sizes):

```json
{
  "EventsReceived": 42,
  "EventsApplied": 40,
  "WritesPublished": 15,
  "ModbusClientReads": 128
}
```

| Field | Description |
|---|---|
| `EventsReceived` | Total ODC events received (Binary/Analog/OctetString) |
| `EventsApplied` | ODC events successfully matched and written to the data store |
| `WritesPublished` | Modbus client coil/register writes published as ODC events |
| `ModbusClientReads` | Modbus client read requests served (coils, DI, HR, IR) |

---

## Architecture

GoModbusPort implements the opendatacon C API (`odc_c_api.h`) as a `c-shared` Go library.
The C entry points in `api_shim.c` bridge to Go functions through `cgo`, which routes calls
to the appropriate port implementation (client or server) via the `odcPort` interface.

### Execution Model

![Execution Model][ExecutionModel_dot]

The execution model follows a **select-loop goroutine** pattern that owns all mutable port state:

1. **ODC Strand** — C++ `asio::io_context` serialises all `odc_port_*` calls per port.
   `build()`, `enable()`, `disable()`, and `event()` never run concurrently for the same port.
2. **selectLoop goroutine** — spawned by `enable()`. Owns all mutable state (reconnect timer,
   connected flag, poll scheduler, worker WaitGroup). Communication with the outside world
   happens only through channels (`eventChan`, `transportDisconnChan`, `writeNotifyChan`)
   or thread-safe atomics/context cancellation.
3. **Worker goroutines** — spawned by the select loop for blocking I/O (Modbus reads/writes).
   They communicate results back via channels; the select loop serialises those results.
4. **destroy()** — called off-strand from `~C_Port()` after the ODC strand has been fully
   drained. ODC guarantees disable before destroy, so only `removePort()` is needed.

### Server Data Flow

![Server Data Flow][server_data_flow_dot]

The Modbus server exposes a `dataStore` (protected by `sync.RWMutex`) that is written by
ODC events (via `applyEventToStore`) and read/written by Modbus client requests (via
simonvetter's `HandleCoils`/`HandleHoldingRegisters` handlers). Modbus client writes
are routed through `writeNotifyChan` to the `selectLoop` goroutine, which performs the
ODC publish — keeping all publish calls on a single controlled goroutine.

### Config Flow

![Config Flow][config_flow_dot]

JSON config is parsed via standard Go `encoding/json` unmarshalling into typed structs,
then validated. Point configurations (`Range` + `Modbus.Count`) are expanded into individual
`PolledPoint` or `ControlPoint` entries at build time.

---

## Concurrency Model

| Context | Thread safety | Mechanism |
|---|---|---|
| ODC strand calls (`build/enable/disable/event`) | Serialised by C++ strand per port | No locks needed |
| selectLoop goroutine | Single goroutine, owns all mutable state | Local variables, channels |
| Worker goroutines (polls, control writes) | No shared mutable state | Read atomics, write to channels |
| dataStore (server) | RWMutex | `sync.RWMutex` — concurrent reads, exclusive writes |
| Logging (`odc_log`) | Thread-safe | spdlog (C++ side) |
| Publish helpers (`publishBinary`, etc.) | Thread-safe | Route through DataConnector event fabric (C++ side) |
| `go_port_stats_json` | Atomic read | `atomic.Pointer[pollScheduler]` |

### Key invariants

- `event()` copies the `C_EventInfo` struct by value before sending it onto `eventChan`.
  The `source_port` field (a borrowed C pointer) is explicitly set to nil.
- OctetString data is deep-copied in `handleEvent()` while the borrowed C pointer is valid,
  then the copy is used in the worker goroutine.
- `doDisable()` sets `enabled=false`, swaps the client to nil and closes it, then cancels
  the context. The `workerWg.Wait()` in the select loop's `ctx.Done()` handler ensures all
  in-flight workers complete before `doDisable()` returns.
- `wg.Wait()` is the absolute last statement in `doDisable()`. `destroy()` has no
  `doDisable()` safety-net — ODC guarantees disable before destroy.

---

## Point Expansion

### Range notation

```json
{ "Range": { "Start": 0, "Stop": 9 }, "Modbus": { "Address": 100, "Count": 2 } }
```

Expands to 10 points with ODC indices 0–9, mapped to Modbus addresses:
- Index 0 → Address 100
- Index 1 → Address 102
- Index 2 → Address 104
- ...

The stride is `Count × (i - Start)`.

### Scale/Offset (client)

Polled values: `published_value = raw_value * Scale + Offset`

### Scale/Offset (server)

ODC events written to store: `store_value = (odc_value - Offset) / Scale`
(note: inverse transform — so the Modbus client reads the raw register value).

### Bit extraction

When `"Bit": N` is specified on a Binaries/BinaryOutputStatuses point whose Modbus type is
`HoldingRegister` or `InputRegister`, the port extracts bit N from the register value:

```
binary = (regs[offset] >> N) & 1
```

A `Bit` value of 0 extracts the LSB. Absent Bit means the point is treated as an analog
register point (no bit extraction).

---

## Endian / DataType Reference

### Endian notation for multi-register values

| Notation | Description | Example (reg[0]=0x1234, reg[1]=0x5678 → uint32) |
|---|---|---|
| `ABCD` (default) | Big-endian: reg[0]=high word, reg[1]=low word | `0x12345678` |
| `CDAB` | Word-swapped: reg[0]=low word, reg[1]=high word | `0x56781234` |
| `BADC` | Byte-swap within each register, then ABCD combine | `0x34127856` |
| `DCBA` | Complete little-endian (byte-swap all 4 bytes) | `0x78563412` |
| `BA` (count=1 only) | Bytes within single register swapped | `0x3412` |

### DataType

| DataType | Count | Description |
|---|---|---|
| `Int16` | 1 | Signed 16-bit integer (default for count=1) |
| `Uint16` | 1 | Unsigned 16-bit integer |
| `Int32` | 2 | Signed 32-bit integer (default for count=2) |
| `Uint32` | 2 | Unsigned 32-bit integer (clamped to [0, 4294967295]) |
| `Float32` | 2 | IEEE 754 single-precision float |
| `Float64` | 4 | IEEE 754 double-precision float (always big-endian register order) |

---

## Building

### Prerequisites

- Go 1.21+ (install from https://go.dev/dl/ or via your package manager)
- CMake 3.10+
- opendatacon build environment (asio, spdlog, etc.)

### CMake

```sh
cmake . -B build -DGO_MODBUS_PORT=ON
cmake --build build
```

The CMake target is `GoModbusPort`. The build:
1. Finds the Go toolchain (set `GO_HOME` cache variable to override, default `/opt/go`)
2. Runs `go build -buildmode=c-shared`
3. Installs the resulting shared library to the module install directory

The imported target `GoModbusPort_lib` lets test executables and other targets declare a
proper CMake dependency on the `.so` without linking it.

---

## Testing

### Go unit tests (config parsing, codecs, endian round-trips)

```sh
cd Code/Ports/GoModbusPort && go test -v -count=1 ./...
```

These tests validate JSON parsing, point expansion, endian encoding/decoding, and
server point-config validation. They do not require a Modbus device.

### Go integration tests (Modbus TCP client/server via simonvetter)

```sh
cd Code/Ports/GoModbusPort && go test -v -count=1 -tags=integration ./...
```

These tests start real Modbus TCP servers and clients on free ports, testing
coil/register read/write, connection lifecycle, and server-stop behaviour.

### C++ E2E tests (opendatacon integration)

```sh
cmake . -B build -DGO_MODBUS_PORT=ON -DTESTS=ON
cmake --build build
cd build/install/bin && ./GoModbusPort_tests error -d yes
```

These tests use the full opendatacon C API through `C_Port`, exercising:
- Port creation, building, enabling, disabling, destruction
- Poll-based coil/register reads
- Outbound control writes (Binary → Coil, Analog → Register)
- Server mode with bidirectional data flow
- E2E scenarios with two ports (client + JSONPort, or client + server)

---

## Code Structure

| File | Role |
|---|---|
| `api_shim.c` | C entry points (`odc_port_create`, `odc_port_destroy`, etc.) bridging to Go |
| `gombus_helpers.h` | Inline wrappers for the ODC host API vtable; payload getters/setters |
| `main.go` | `odcPort` interface definition, `export`ed Go functions, port registry |
| `common.go` | Port registry, `eventWork` type, logging helpers, ODC publish helpers, event value extractors |
| `config.go` | Config structs, JSON parsing/validation, point expansion, endian/dataType codecs |
| `modbus_client.go` | `GoModbusClientPort` — lifecycle, select loop, polling, control writes |
| `modbus_server.go` | `GoModbusServerPort` — lifecycle, select loop, data store, simonvetter handlers, state/status/stats JSON |
| `poll_scheduler.go` | Ticker-based poll scheduler with semaphore-based concurrency control |
| `config_flow.dot` | Configuration parsing and point expansion flow diagram |
| `server_data_flow.dot` | Server mode data flow diagram |
| `ExecutionModel.dot` | Complete execution/concurrency model diagram |
| `config_test.go` | Unit tests for config parsing and codec functions |
| `modbus_integration_test.go` | Integration tests for raw Modbus TCP client/server |
| `modbus_server_integration_test.go` | Integration tests for GoModbusServerPort against real clients |

### Adding a new feature

1. **Config**: Add fields to `PortConfig` or `ServerPortConfig` in `config.go` with appropriate
   `json:"..."` tags. Add validation in `validateConfig()` or `validateServerConfig()`.
2. **Point expansion**: If the new feature introduces a new data type, add a new array field
   to the config struct and an expansion case in `expandPoints()` (client) or `buildMaps()` (server).
3. **Publishing**: Use the helpers in `common.go` (`publishBinary`, `publishAnalog`, etc.) or
   add a new one if a new ODC event type is needed.
4. **Server handlers**: If the feature involves a new Modbus register type, implement the
   corresponding `Handle*` method in `modbus_server.go`.

---

## Known Limitations

- **RTU server mode**: The simonvetter/modbus library supports RTU client but not RTU server
  (Modbus RTU is master-slave only; there is no standard RTU server listen model).
  RTU client mode works for connecting to RTU devices.
- **`MaxConcurrentPolls > 1`**: Supported without duplicate reads — each point is claimed
  atomically via `CompareAndSwap` in `tryClaimDue`. However, which goroutine claims which point
  first is non-deterministic; use the default value of 1 if strict poll ordering is required.
- **State/status/stats for client mode**: The client port provides poll statistics via
  `go_port_stats_json` but returns `{}` for state and status. State and status reporting
  for client ports is not yet implemented.
- **Server `UnitID = 0`**: Responds to all unit IDs (per Modbus spec). Set to a specific value
  (1–247) to filter.
