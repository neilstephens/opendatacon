# Lua-Transport: ODC events over TCP, TLS, HTTP and HTTPS

Four complete, runnable configurations that move ODC point events and controls between two LuaPorts over a real network transport, using the network objects of the Lua scripts (`odc.TCPSocket`, `odc.TLSSocket`, `odc.HTTPServer`, `odc.HTTPClient`; see [Network objects for Lua scripts](../../Code/Libs/LuaWrappers/README.md)).

```
 Source --> Sender ====== transport ======> Receiver --> Sink
 (Lua)     (Lua)                            (Lua)       (Lua)
   ^                                                      |
   +------ CommandStatus <---- control <------------------+
```

Everything runs in ONE opendatacon process on loopback, with synthetic names:

| Port | Script | What it does |
|---|---|---|
| `Source` | `source.lua` | Stands in for a real outstation. Publishes one event of every supported type, then a few changing ones every `PeriodMs`, and answers the controls it receives. |
| `Sender` | `socket_sender.lua` (TCP, TLS) or `http_sender.lua` (HTTP, HTTPS) | Its `Event()` receives what the Source publishes, serialises it and sends it. Gives controls that come back to the Source and sends the CommandStatus back. |
| `Receiver` | `socket_receiver.lua` or `http_receiver.lua` | Receives, deserialises and `odc.PublishEvent`s into ODC again. Its `Event()` receives the controls of the Sink side, sends them to the Sender and returns the status that comes back to the originator. |
| `Sink` | `sink.lua` | Logs every event it receives and, `ControlDelayMs` (6 s) after start, issues six controls and logs their statuses. |

`Source` and `Sender` are joined by the connector `SourceSide`, `Receiver` and `Sink` by `SinkSide`. SimPort is not used for the source: it generates only Analog and Binary points and answers other controls with `NOT_SUPPORTED`, and the point of the examples is every type, so `source.lua` (a Lua port) generates all of them.

| Directory | Transport | Ports | Start with |
|---|---|---|---|
| `TCP` | `odc.TCPSocket`: Sender is the client, Receiver the server | 20150 | `opendatacon` |
| `TLS` | `odc.TLSSocket` with mutual TLS | 20160 | `make-certs`, then `opendatacon` |
| `HTTP` | `odc.HTTPClient` + `odc.HTTPServer` on both sides | 20170 (Receiver), 20171 (Sender) | `opendatacon` |
| `HTTPS` | the same with TLS and mutual TLS | 20180 (Receiver), 20181 (Sender) | `make-certs`, then `opendatacon` |

## Running

Run opendatacon **in the example's directory** (the Lua files are one level up and are named `../x.lua`; the certificate paths are relative to it):

```
cd ExampleConfig/Lua-Transport/TCP
opendatacon
```

For `TLS` and `HTTPS` run `make-certs.ps1` (Windows, PowerShell) or `./make-certs.sh` first. They need `openssl` on the path and write a CA and one certificate and key each for the Sender and the Receiver (synthetic names `odc-sender.example` and `odc-receiver.example`, and `127.0.0.1`) to `certs/`. The example `.gitignore` keeps `certs/` and the logs out of git: **these are throw-away keys for the example, never use them for anything else.** `TLS` needs a build with OpenSSL (`ODC_ASIO_SSL`) and `HTTPS` also Simple-Web-Server with TLS (`SIMPLEWEB_SSL`); otherwise the Sender and Receiver log `no_tls` and do nothing.

## What you will see

In the log (and the console), 2 s after start the Source publishes and the Sink logs, in order, a `ConnectState`, then Binary, DoubleBitBinary, Analog, Counter, FrozenCounter, BinaryOutputStatus, AnalogOutputStatus and OctetString events (with index, quality and time as the Source gave them), then every 2 s a changing Analog, Binary and Counter. 6 s after start the Sink issues six controls and logs each status:

| # | Control | Source answers | The Sink logs |
|---|---|---|---|
| 1 | CROB `PULSE_ON`, index 1 | `SUCCESS` | `SUCCESS` |
| 2 | AnalogOutput Int16, index 2 | `SUCCESS` | `SUCCESS` |
| 3 | AnalogOutput Double64, index 3 | `SUCCESS` | `SUCCESS` |
| 4 | CROB `LATCH_OFF`, index 99 | `NOT_SUPPORTED` | `NOT_SUPPORTED` |
| 5 | AnalogOutput Float32, index 4, value 5000 | `OUT_OF_RANGE` (a value over 1000) | `OUT_OF_RANGE` |
| 6 | CROB `PULSE_ON`, index 98 | no answer | `TIMEOUT` after `ControlTimeoutMs` (5 s), with a warning "control 6 was not answered" |

The Source's answers are in `source.lua` (`Event()`), and an index the Source does not know of is a normal `SUCCESS`: the Source's rule is the example's, not ODC's.

## The wire format

One JSON **message** is `{ "v": 1, "kind": ..., ... }`. `v` is the format version (a reader refuses a version it does not know). The code is `odc_wire.lua`, which every script loads with `require` (`package.path` is set from `odc.GetPath.ScriptDir`).

| `kind` | Fields | Sent |
|---|---|---|
| `event` | `event` | Sender to Receiver, TCP/TLS: one per line |
| `events` | `events`: a list of events | Sender to Receiver, HTTP/HTTPS: the body of `POST /events` |
| `control` | `id` (integer), `event` | Receiver to Sender: one per line (TCP/TLS), or the body of `POST /control` (HTTP/HTTPS) |
| `status` | `id`, `status` (a CommandStatus name) | Sender to Receiver: a line (TCP/TLS), or the 200 response of `POST /control` (HTTP/HTTPS) |
| `ack` | `accepted`, `rejected` | the 200 response of `POST /events` |

An **event** is `{ "type": "Analog", "index": 3, "quality": 1, "quality_text": "ONLINE", "time": 1700000000000, "payload": ... }`. `type` is the name from `odc.ToString.EventType`, `quality` the number of `QualityFlags` (what is read back; `quality_text` is for people), `time` milliseconds since 1970. `SourcePort` is not sent (what the Receiver publishes has the Receiver as its source). The payload:

| `type` | `payload` |
|---|---|
| `Binary`, `BinaryOutputStatus` | `true` / `false` |
| `Analog`, `AnalogOutputStatus` | a finite number (JSON has no NaN or infinity: such a value is refused by the sender) |
| `Counter`, `FrozenCounter` | an integer 0 to 4294967295 |
| `DoubleBitBinary` | `{ "first": bool, "second": bool }` |
| `OctetString` | the bytes as a lower-case hex string |
| `ConnectState` | the name from `odc.ToString.ConnectState` |
| `ControlRelayOutputBlock` | `{ "code": "<ControlCode name>", "count": 0 to 255, "on_ms": n, "off_ms": n, "status": "<CommandStatus name>" }` |
| `AnalogOutputInt16`, `AnalogOutputInt32`, `AnalogOutputFloat32`, `AnalogOutputDouble64` | `{ "value": n, "status": "<CommandStatus name>" }` (Int16 and Int32 values are range-checked) |

Anything else (an unknown `type`, a payload of the wrong shape, a number out of range, a bad hex string, a wrong version) is refused with a message that says what is wrong: the receiving side logs an error and carries on with the next message. The stream is not stopped by a bad message.

**Controls.** The Receiver gives each control the next `id`, sends it, and keeps the callback it got from ODC (the "status callback" of `Event()`) until the `status` with that `id` arrives; when the Sender's own `Event()` gets the status from the Source it sends it back. If nothing arrives in `ControlTimeoutMs` the callback gets `TIMEOUT`; if the transport is down when the control is issued, or goes down while it is waiting, the callback gets `DOWNSTREAM_FAIL`. (The HTTP Receiver maps an HTTP error the same way: a client `timeout` or a 504 from the Sender is `TIMEOUT`, any other error `DOWNSTREAM_FAIL`, an answer that is not a valid status `FORMAT_ERROR`.)

## Per transport

**TCP and TLS.** One message per line (`\n`), the line buffer copes with the bytes arriving split and joined in any way, and a line over `MaxLineBytes` (65536) is dropped. The Sender is the client with `auto_reopen` and `RetryMs`; events issued while the connection is down are queued (at most `QueueLimit`, 1000: the oldest is dropped beyond that) and sent when it comes back, in order. The Receiver is the server and, like every `odc.TCPSocket` / `odc.TLSSocket` server, **takes one connection at a time**: a second client waits until the first has gone. Control answers go back on the connection the control was sent on. The TLS example needs a client certificate signed by the CA: a client without one is refused at the handshake.

**HTTP and HTTPS.** The Sender collects events and every `BatchMs` (1000), or at once when `BatchMax` (50) are queued, POSTs one JSON message (`kind: "events"`, a list of events) to the Receiver's `/events`; the Receiver answers 200 `{ accepted, rejected }`, and 400 for a body that is not a valid batch or holds more than `MaxBatchEvents` (1000) events. (A body over `MaxBodyBytes` never reaches the script: Simple-Web-Server closes the connection, and the Sender sees `closed`; the batches are far smaller than the limit.) Batches go one at a time, so their order is kept. A batch is sent again at the next `BatchMs` tick after a transport error or a 5xx (so a Receiver that is away loses nothing until `QueueLimit` is reached) and dropped after a 4xx (it would be refused again). Controls: **each side runs a client and a server**: the Receiver `POST`s the control to the Sender's server at `/control` and the 200 response holds the `status`. Bodies are bounded with `max_request_bytes` and `max_response_bytes`. HTTPS uses the HTTP stack's restricted TLS options (`CertFile`, `KeyFile`, `CAFile` only; both servers require a client certificate signed by the CA, both clients verify the server's certificate and host name against it).

## Settings

Every script lists its settings (`ConfOverrides` of the port, read with `odc.Config`) at the top of the file. The ports and the certificate paths can be changed there; the test of the examples (`Code/tests/LuaPort_tests/NetExamplesTest.cpp`) runs these same scripts with its own ports and certificates that it generates at run time.

## Tests

`NetExamplesTest.cpp` runs the real scripts (Sender, Receiver and the wire module) in LuaPorts: every type round trips with value, index, quality and timestamp, in order, with the bytes split and joined and across a reconnect; the controls return the status the source chose (including `TIMEOUT` and `DOWNSTREAM_FAIL`); a malformed or unknown message is rejected and the stream carries on; a client without a valid certificate is refused; and every `opendatacon.conf` here is parsed and checked for the files it uses.
