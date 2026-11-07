## Table of contents

* [Introduction](#introduction)
* [Status](#status)
* [Scope](#scope)
* [Architecture](#architecture)
* [Behaviour](#behaviour)
* [Configuration files and syntax](#configuration-files-and-syntax)
* [Secure ICCP / TLS](#secure-iccp--tls)
* [Secure ICCP: MACE](#secure-iccp-mace)
* [Migration from the previous schema](#migration-from-the-previous-schema)
* [Points, data sets and quality](#points-data-sets-and-quality)
* [UI commands, status and statistics](#ui-commands-status-and-statistics)
* [Device control (TASE.2 Block 5)](#device-control-tase2-block-5)
* [Troubleshooting a data set the peer refuses](#troubleshooting-a-data-set-the-peer-refuses)
* [Supported implementation quirks](#supported-implementation-quirks)
* [Known limitations](#known-limitations)
* [Unverified items](#unverified-items)
* [Example configuration](#example-configuration)
* [Unit tests](#unit-tests)
* [Building](#building)
* [Roadmap and remaining work](#roadmap-and-remaining-work)


## Introduction

ICCPPort is an ICCP (Inter-Control Centre Communications Protocol, IEC 60870-6 TASE.2) port library for OpenDataCon.

The library provides two port types, both built on an OSI protocol stack written from scratch (no third party ICCP/MMS/OSI libraries):

* `ICCPClientPort` - the **TCP initiator**: it connects (with retry) and, as the TCP initiator, initiates the association.
* `ICCPServerPort` - the **TCP listener**: it accepts the connection and responds to the association.

The two port types differ **only** in the TCP direction. What a port does on the association is a separate setting, the **service role** (`ServiceRole`: `Client`, `Server` or `Peer`),
and it is valid for both port types. The configuration schema, its validation, the points, data sets, transfer sets and all TASE.2 behaviour are identical for both port types:
the same configuration file works under either, provided it sets `ServiceRole` explicitly (without it the role defaults from the port type, see below).

* Service role `Client`: only issues requests: reads the peer's association objects, defines data sets on the peer, starts transfer sets, polls, consumes reports and publishes the values as ODC events.
* Service role `Server`: only serves: the association objects, the configured points from a value cache fed by ODC events, data sets and transfer sets, and it sends reports.
* Service role `Peer`: both at once on the one association (what real-world ICCP peers do).

The protocol implementation conformance statement (PICS), in the form of the IEC 60870-6-702 Annex A requirements lists, is in [PICS.md](PICS.md).

### TCP direction, service role and who initiates

| Port type (`Type`) | TCP | `ServiceRole` default | Association initiated by | Notes |
|--------------------|-----|-----------------------|--------------------------|-------|
| `ICCPClient` | initiator (connects, retries) | `Client` | this port: sends COTP CR, session CN, ACSE AARQ, MMS Initiate | `IP`/`Port` = the peer's address |
| `ICCPServer` | listener (accepts one connection) | `Server` | the peer | `IP` = the address to bind (listen on) |

The association initiator always follows the TCP direction (the TCP initiator sends the AARQ), whatever the service role. So, for example, an `ICCPServer` port with `ServiceRole` `Peer` waits for the
peer's AARQ, then reads the peer's points and serves its own on that association; an `ICCPClient` port with `ServiceRole` `Server` sends the AARQ and then only answers requests.
Both ends of one association must of course agree: one connects, the other listens, and the service roles must be compatible (`Client`-`Server`, `Server`-`Client`, `Peer` with anything).

## Status

Both port types work. They have been run against each other over TCP (the loopback unit tests, and the example configurations in `opendatacon`),
and both the client and the server role were tested successfully as a live peer of a real-world ICCP implementation **with the previous design** (role tied to the port type).
The role / TCP direction split described in this README (`ServiceRole`, `Peer`, `Direction`) is newer: it is covered by the loopback unit tests over real TCP but not yet verified against a real-world peer, so treat it as `[S]`.
The wire encodings were also checked against packet captures of real associations (see [Supported implementation quirks](#supported-implementation-quirks), marked `[V]`).
The live test did not necessarily exercise every `[S]` item below, so those remain unverified until confirmed individually.

| Layer | Standard | Status |
|-------|----------|--------|
| BER | ITU-T X.690 | Done (`BER.h`) |
| TPKT / COTP class 0 | RFC 1006 / ISO 8073 | Done (`TPKT.h`, `COTP.h`) |
| Session | ISO 8327 / X.225 | Done (`Session.h`) |
| Presentation | ISO 8823 / X.226 | Done (`Presentation.h`) |
| ACSE | ISO 8650 / X.227 | Done (`Acse.h`) |
| MMS subset | ISO 9506 | Done (`Mms.h`) |
| TASE.2 Blocks 1 and 2 | IEC 60870-6-503 / -702 | Done (`Tase2.h`, `Tase2Quality.h`, `Tase2Points.h`) |
| TASE.2 Block 3 (blocked transfers, `BlockData`) | IEC 60870-6-503 clause 7.2.5.5.3 | Implemented on both sides. **Seen from a real peer `[V]`** (captures of a real peer as the server, `BlockData` true): the report structure (the special objects as domain-specific variables, then one VMD-specific `Block_Data` octet string result); **rule 0** for every complete report (`RBE` false, and the integrity reports of an `RBE` true transfer set), with the special objects stored inside the octet string in data set order (65 octet name, 4 octet big endian time stamp, 1 octet conditions, 2 octet `Event_Code_Detected` when the data set has it) and the 5 octet `RealQ` and 1 octet `StateQ` records; **rule 1** for the partial (object change) reports of an `RBE` true transfer set: pairs of a 1 octet position and a value, only the changed points (one or several, in ascending position order), no special objects in the string, the position counting the special objects. **Seen from a real peer `[V]` as sent by this port:** rule 2 reports (see below) were accepted, acknowledged **and applied correctly** (the values of 10 points above position 255, on data sets of about 500 and 390 points, matched what this port sent: the 2 octet big endian position, the positions counting the special objects, the value mapping above position 255). **Not seen `[S]`:** a rule 2 report sent BY a real peer (the capture's peer sent none), the byte order and sign of an `Event_Code_Detected` other than 0, every point type but `RealQ` and `StateQ`, and a real peer asking this port's server for reports in other layouts. The server sends rule 2 for a report with a position above 255 unless `BlockedRule2` is false (then in the ordinary form). The server honours `BlockData` only with 3 in `SupportedBlocks`; with the default `[1, 2]` nothing changes. A decode failure is logged with a hex dump (`Tase2Blocked.h`, quirks 66 to 70, see [Blocked transfers (Block 3)](#blocked-transfers-block-3)) |
| TASE.2 Block 5 (device control) | IEC 60870-6-503 / -702 | Done: the device model is per the standard text `[T]` and a real peer's operating flows `[V]` (quirks 39 to 42 and 51), with the optional device tags (Get Tag, Set Tag, `<Device>_TAG`, enforcement on select and operate) and the Local Reset action (state only), both `[T]`/`[S]` and never seen from a real peer (quirks 60 to 65); the event conditions (Block 7) and the SBO / ControlConfig status variables are not implemented (quirk 43) (`Tase2Control.h`, see [Device control](#device-control-tase2-block-5)) |
| Association state machine | | Done (`IccpStack.h`): one `Connection` per TCP connection, no I/O of its own |
| Server and client services | | Done (`Tase2Responder.h`, `Tase2Client.h`), including the acknowledgement of `Critical` reports (client) and their tracking (server), and `TLE` as far as the stack allows: the wire layout of the acknowledgement is `[S]`, see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle) |
| Port glue and configuration | | Done (`ICCPPortBase.h`, `ICCPPortConf.h`, `ICCPServerPort.h`, `ICCPClientPort.h`) |
| Secure ICCP: TLS | IEC 62351-3 | Done (needs `ODC_ASIO_SSL`; the generic TLS filter and session manager of the ODC library, `odc::tls` and `odc::TCPSessionManager`: `opendatacon/TLSConf.h`, `opendatacon/TLSFilter.h`, `opendatacon/TCPSessionManager.h`; the port's own part is `IccpTlsDefaults.h`; see [Secure ICCP / TLS](#secure-iccp--tls)). Used successfully with one real-world peer; the IEC 62351-3 profile details remain `[S]` |
| Secure ICCP: authentication in ACSE (MACE) | MMS Application Certificate Exchange (a real peer's scheme) | Done (needs `ODC_ASIO_SSL`; `Mace.h`, `MaceConf.h`, `Acse.h`; see [Secure ICCP: MACE](#secure-iccp-mace)). The wire format and the signature scheme are `[V]` (decoded from a real peer's association, both directions); our association request, and the peer's response, were accepted by a real peer in our initiator role, and our response was accepted in our responder role, with and without TLS (`[V]`); the failure diagnostics are `[S]` |
| Secure ICCP: authentication in ACSE | IEC 62351-4 | Not started |

## Scope

Implemented:

* TASE.2 Block 1 (basic data) and Block 2 (extended conditions: DS transfer sets).
* TASE.2 Block 3 (blocked transfers: the `BlockData` attribute of a transfer set), on both sides: rule 0 and rule 1 (one or several points per report), the special objects (including `Event_Code_Detected`) and `RealQ`/`StateQ` points as a real peer sends them `[V]`; rule 2, sent by this port's server, was accepted, acknowledged and applied correctly by a real peer `[V]` (2 octet big endian position, positions counting the special objects, values above position 255 checked in the peer); a rule 2 report sent by a real peer has not been seen `[S]`, nor have the other point types and the byte order of a non zero event code (quirks 66 to 70). See [Blocked transfers (Block 3)](#blocked-transfers-block-3).
* TASE.2 Block 5 (device control), select before operate and direct operate, in both directions, mapped to ODC control events. The select is a Read of the device's `_SBO` object and the operate a Write of the bare value, as the standard
  defines `[T]` and a real peer does `[V]` (quirks 39 to 42 and 51). Of the optional parts, the device tags (Get Tag, Set Tag, the variable `<Device>_TAG`, enforcement on select and operate) and the state effect of the Local Reset action are implemented
  (quirks 60 to 65, `[T]`/`[S]` only: no real peer has been seen using them); the event conditions of the device actions (Block 7) are not. See [Device control](#device-control-tase2-block-5) for exactly what is and is not implemented.
* Plain TCP transport (RFC 1006), and TLS over it (IEC 62351-3) when built with `ODC_ASIO_SSL`.
* Authentication in the ACSE association with certificates (MACE), when built with `ODC_ASIO_SSL`: see [Secure ICCP: MACE](#secure-iccp-mace).
* Static point configuration (matching the bilateral table), as with the other ports. No dynamic discovery on the server.
* TASE.2 edition (1996-08 or 2000-08) as a configuration option, default 2000-08.

Later phases: see the [Roadmap](#roadmap-and-remaining-work) (the IEC 62351-4 HMAC authentication, the Block 5 remainder (cancelling a select, Block 7 event conditions), and more). The port must still build and work without OpenSSL (`ODC_ASIO_SSL` off).

Out of scope so far: TASE.2 Blocks 4 and 6 to 9 (Block 4 is on the roadmap).

## Architecture

All layers are in the `iccp` namespace, one sub-namespace per layer. Every decoder treats its input as untrusted: it bounds-checks everything, caps sizes and nesting,
and throws `iccp::ber::DecodeError` on malformed input.

```
ICCPClientPort / ICCPServerPort            thin ODC port classes: only the TCPSessionManager mode (initiator / listener) differs
ICCPPortBase                               strand, timers, TCPSessionManager, ODC events; composes, per the ServiceRole:
  |-- ClientSession  (Tase2Client)         (Client, Peer) verify the peer, provision data sets and transfer sets, poll, turn reports into events
  |-- ResponderSession (Tase2Responder)    (Server, Peer; empty for Client) serve the peer's requests from a ResponderModel, send transfer set reports
  '-- stack::Connection (IccpStack)        association state machine: bytes in, bytes out, timers by Tick(), invoke ids, release
        iccp::tase2   TASE.2 objects, Block 1 and 2 helpers          Tase2.h
        iccp::tase2::blocked  Block 3 (blocked transfers)           Tase2Blocked.h
        iccp::mms     MMS PDUs, services, data, types                Mms.h
        iccp::acse    AARQ/AARE/RLRQ/RLRE/ABRT                       Acse.h
        iccp::presentation  CP/CPA/CPR, PDV wrapper                  Presentation.h
        iccp::session  CN/AC/RF/FN/DN/AB/AA, DT                      Session.h
        iccp::cotp / iccp::tpkt  CR/CC/DR/DT, segmentation; framing  COTP.h, TPKT.h
        iccp::ber                                                    BER.h
```

* Every layer below the ports is pure encode/decode or a pure state machine over byte buffers: no sockets, no timers of its own. That is what lets the unit tests drive two
  `Connection`s back to back in memory.
* All work for a port is serialised on one strand. Timers (a 50 ms tick drives request timeouts, transfer set intervals and buffer times) use the ODC io service.
* The `TCPSessionManager` (`opendatacon/TCPSessionManager.h`, a generic component of the ODC library, built on the `TCPSocketManager`) provides the TCP connection: client mode with retry, server mode listening for one association at a time.
* With TLS enabled, the `TCPSessionManager` runs a `tls::Filter` (`opendatacon/TLSFilter.h`, `odc::tls`, aliased as `iccp::tls` in the port) on each TCP connection, between the socket and `stack::Connection`: see [Secure ICCP / TLS](#secure-iccp--tls).
* Presentation context ids are taken from the negotiated result, never hardcoded.

## Behaviour

The behaviour depends on the service role, not on the port type. The *client side* runs for `Client` and `Peer`, the *server side* for `Server` and `Peer`. With `Peer` both run on the one association
and concurrently; their invoke id spaces are separate per direction (the requests this end issues are numbered by this end, the peer's by the peer), and the peer's requests are answered
while this end's own provisioning (verify, define, start) is in progress. The TCP direction only decides who sends the first PDUs (see the table above).

### Client side (service role Client or Peer)

1. Once the association is up (the TCP initiator sent the AARQ; on a listener the peer did), reads the peer's association objects: `Bilateral_Table_ID` (checked against `RemoteBilateralTableID` if given), `TASE2_Version` (logged; a difference from `TASE2Edition` refuses the peer unless `RequireMatchingEdition` is false) and `Supported_Features`
   (the blocks in `RequiredRemoteBlocks` must be present). A mismatch is logged as an error and the association is dropped; an initiating port retries after a delay of at least one second.
2. Publishes `CONNECTED`.
3. For each configured transfer set (`TransferSets`): `Next_DSTransfer_Set` (unless the transfer set has a fixed `Name`), delete then define the data set on the peer (the special objects first, then the points),
   then write the `DSTransferSet` structure to start the transfer. If the peer refuses the definition because it lacks some members, the data set is probed and defined again without them
   (see [Troubleshooting](#troubleshooting-a-data-set-the-peer-refuses)); the points of the missing members are published once with a quality flag instead of values.
4. Reports (`InformationReport`, unconfirmed) are decoded, converted using the configured `Read` point types and published through `PublishEvent` as Binary, Analog or Counter events with quality and time stamp.
   Data sets with a `PollIntervalms` are instead defined and read by a Read of the data set name every interval.
   A report of a transfer set started with `Critical` true is acknowledged (`Transfer_Report_ACK`), and a report that is not from a transfer set this port started is discarded and answered with `Transfer_Report_NACK`
   (see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle)).
5. On `Disable()`, stops the transfer sets (Stop Transfer), then concludes the association (MMS Conclude, then session release). `DISCONNECTED` is published when the association ends,
   `PORT_DOWN` when the port is down.
6. When the association is lost it is re-established: an initiating (`ICCPClient`) port retries the TCP connection; a listening port accepts the next one.

### Server side (service role Server or Peer)

1. Answers the Initiate (when this end is the TCP listener; as the TCP initiator it sends it), `Identify` (vendor, model and revision from the configuration) and reads of the association objects.
   If `RemoteBilateralTableID` is configured the peer's `Bilateral_Table_ID` is read and compared (a `Server`-only port does this itself; a `Peer` does it as part of its client side);
   a mismatch is logged and, with `AbortOnBilateralTableMismatch`, ends the association. A `Server`-only port publishes `CONNECTED` as soon as the association is up.
2. Serves `GetNameList` (VMD and domain scope, variables and data sets, paged), `GetVariableAccessAttributes`, `Read` of the configured `Serve` points from the value cache, `Read` of data sets,
   and `DefineNamedVariableList`, `DeleteNamedVariableList`, `GetNamedVariableListAttributes` for data sets the peer creates (limited by `MaxClientDataSets` and `MaxDataSetMembers`).
   The `Serve` data sets configured under `DataSets` are served too.
3. `Next_DSTransfer_Set` hands out the next free transfer set from the pool (`TransferSetPool`). A write of the `DSTransferSet` structure starts or stops a transfer set.
4. Reports: `IntegrityTimeOut` (every `IntegrityCheck` seconds; the first one is sent at once when the transfer set starts), `IntervalTimeOut` (every `Interval` seconds), `ObjectChange`
   (when an ODC event changes a value or quality, buffered for `BufferTime` seconds; with `RBE` only the changed members are sent), `OperatorRequest` and `OtherExternalEvent` (UI commands).
   With `RBE` set, an `IntervalTimeOut` report carries only the members that changed since the last report (and none is sent if nothing changed); integrity reports are always complete.
   With `RBE`, `ObjectChange` and a `BufferTime` above 0, a transfer set whose `AllChangesReported` is true reports every change of a point in the buffer time, in order, as several reports (see [Transfer set settings explained](#transfer-set-settings-explained)).
   Reports stop once the peer has concluded.
   Reports of a `Critical` transfer set wait for the client's `Transfer_Report_ACK` (a missing or negative one is a logged and counted `Data_failure`), and a report that is handed over more than `TLE` seconds after it was due is discarded
   (see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle)).
5. `Event()`: ODC Binary, Analog and Counter events update the value cache and drive `ObjectChange`. ODC control events (`ControlRelayOutputBlock`, `AnalogOutput*`) are commands for the devices this
   port operates on the peer (see [Device control](#device-control-tase2-block-5)). Other event types reply `NOT_SUPPORTED`.
6. Anything unsupported is answered with an MMS Reject or Confirmed-Error, never silence.
7. The data sets and transfer set state a client creates belong to the association and are cleared when it ends.

### Release and errors

* Release is an MMS Conclude followed by the session FINISH/DISCONNECT exchange (ACSE RLRQ/RLRE). If both ends conclude at once the association's initiator wins and the other end gives way.
* A transport that closes after the Conclude, or a peer that sends a session FINISH without a Conclude, is also accepted.
* Malformed input never brings the port down: an undecodable confirmed request with a readable invoke id is rejected, a malformed unconfirmed PDU is logged and ignored, anything else aborts the association.
* Requests have a timeout (`RequestTimeoutms`); association set-up and release have their own (`AssociateTimeoutms`, `ReleaseTimeoutms`).

## Configuration files and syntax

The ports are configured as the other ODC ports are: a JSON file named by `ConfFilename`, optionally overridden by `ConfOverrides` (the keys of the overrides replace the same top-level keys of the file).
The configuration is validated strictly; every problem is logged as a `configuration error` and a port with an invalid configuration does not enable.
See `ExampleConfig/ICCP-ClientServer` for working files and `templates` there for generic ones.

In `opendatacon.conf`:

```json
{"Name" : "ICCPClient", "Type" : "ICCPClient", "Library" : "ICCPPort", "ConfFilename" : "ICCPClient.conf", "ConfOverrides" : {}}
{"Name" : "ICCPServer", "Type" : "ICCPServer", "Library" : "ICCPPort", "ConfFilename" : "ICCPServer.conf", "ConfOverrides" : {}}
```

**One schema for both port types.** `Type` chooses only the TCP direction; every key below is valid and validated identically for both. A file with an explicit `ServiceRole` (and explicit
point `Direction`s) works unchanged under either type; the identity keys (`Local*`/`Remote*`) are relative to this port, not to the TCP direction.

### Service role

| Name | Type | Default | Description |
|------|------|---------|-------------|
| ServiceRole | string | `ICCPServer`: "Server"; `ICCPClient`: "Client" | `"Client"`, `"Server"` or `"Peer"`. An explicit value overrides the default, for either port type. |

* `Client`: points are `Read` points (default `Direction`), `DataSets` are defined on the peer, `TransferSets` are started on the peer. `Serve` points/data sets and `TransferSetPool` are errors.
* `Server`: points are `Serve` points (default `Direction`); data sets are served; `TransferSetPool` lists the offered transfer sets. `Read` points, `Read` data sets and `TransferSets` with a `DataSet` are errors.
* `Peer`: both. `Direction` is **required** on every point (there is no sensible default), and data sets are either served or defined on the peer (see below).

### Connection

| Name | Type | Default | Description |
|------|------|---------|-------------|
| IP | string | 127.0.0.1 | `ICCPClient`: the peer's address. `ICCPServer`: the local address to bind and listen on. |
| Port | number or string | 102 (3782 when `TLS` is enabled) | `ICCPClient`: the peer's TCP port. `ICCPServer`: the port to listen on. |
| TCPConnectRetryPeriodms | number | 0 | `ICCPClient`: fixed retry period in ms; 0 = exponential back-off. Accepted and ignored by a listener. |
| TCPThrottleBitrate, TCPThrottleChunksize, TCPThrottleWriteDelayms | number | 0 | Optional write throttling, as in the other TCP ports. |
| TLS | object | absent (plain TCP) | Secure ICCP: TLS (IEC 62351-3) on the TCP connection, see [Secure ICCP / TLS](#secure-iccp--tls). |

### Addressing

Selectors are hexadecimal strings (an even number of digits). AP-titles are dotted object identifiers. "Local" is this port, "Remote" is the peer, whichever way the TCP connection goes
(on the wire the calling and called addresses swap with the TCP direction: the TCP initiator's Local is the calling address, the listener's Local is the called address).

| Name | Type | Default | Description |
|------|------|---------|-------------|
| LocalTSelector, RemoteTSelector | hex string | empty | Transport selectors (TSAP), up to 32 octets. |
| LocalSSelector, RemoteSSelector | hex string | empty | Session selectors. |
| LocalPSelector, RemotePSelector | hex string | empty | Presentation selectors. |
| LocalAPTitle, RemoteAPTitle | OID string | none | ACSE AP-titles (for example `1.3.9999.1.1`). |
| LocalAEQualifier, RemoteAEQualifier | number | none | ACSE AE-qualifiers. |
| TPDUSize | number | 1024 | Proposed TPDU size: 128, 256, 512, 1024, 2048, 4096 or 8192. The smaller of both ends is used. |

### TASE.2

| Name | Type | Default | Description |
|------|------|---------|-------------|
| TASE2Edition | string | "2000-08" | `"2000-08"` or `"1996-08"`. Sets the `TASE2_Version` advertised. Peers seen in captures use 1996-08. |
| BilateralTableID | string | empty | The `Bilateral_Table_ID` this port reports when read. A visible string of at most 32 characters (type `Visible-String-32`, IEC 60870-6-503 clause 8.3.3 `[T]`): at most 32 characters, each visible ASCII (0x20 to 0x7E), else a configuration error that names the rule. |
| RemoteBilateralTableID | string | empty | The `Bilateral_Table_ID` expected from the peer; empty = not checked. Same limits as `BilateralTableID` (32 characters, visible ASCII). Client side: a mismatch drops the association. Server-only: logged (see `AbortOnBilateralTableMismatch`). A peer that returns a longer string than 32 characters is not dropped for that alone: a warning is logged once per association and the value is compared as it is `[S]`. |
| LocalDomain | string | empty | The ICC domain this port serves: the default `Domain` of `Serve` points and data sets. Required if ICC `Serve` points exist. See [ICC and VCC scope explained](#icc-and-vcc-scope-explained). |
| RemoteDomain | string | empty | The peer's ICC domain: the default `Domain` of `Read` points and data sets. Required if anything is read from the peer. See [ICC and VCC scope explained](#icc-and-vcc-scope-explained). |
| SupportedBlocks | array of 1-9 | [1, 2] | Blocks advertised in `Supported_Features`. Server side: a transfer set that a peer starts with `BlockData` true is reported in the blocked form (Block 3) only when this contains 3; without it `BlockData` is accepted and ignored, as before. See [Blocked transfers (Block 3)](#blocked-transfers-block-3). |
| RequiredRemoteBlocks | array of 1-9 | [1] (plus 2 if transfer sets are started) | Client side: blocks the peer must advertise. |
| DSTransferSetStatusMember | boolean | follows the peer | Client side: write the 13 member `DSTransferSet` (the 2000-08 form, which adds `AllChangesReported` before `Status`) instead of the 12 member 1996-08 form. Unless given, the form follows the peer's `TASE2_Version` once it is read (13 members for 2000-08, 12 for 1996-08), and `TASE2Edition` until then. |
| RequireMatchingEdition | boolean | true | The peer's `TASE2_Version` must equal `TASE2Edition`: a client drops the association with an error, a server aborts it (the log names both versions). With `false` a mismatch is only logged as a warning. A peer that does not report its version is not checked. |
| VendorName, ModelName, Revision | string | opendatacon, ICCP, 1.0 | Server side: returned by `Identify`. |

### Timers, MMS parameters and server limits

| Name | Type | Default | Description |
|------|------|---------|-------------|
| RequestTimeoutms | number | 5000 | Timeout of a confirmed request. |
| AssociateTimeoutms | number | 10000 | Timeout of the association set-up. |
| ReleaseTimeoutms | number | 5000 | Timeout of the release. |
| MaxOutstandingCalling, MaxOutstandingCalled | number | 10 | MMS Initiate: requests this end allows the peer to have outstanding / this end may have outstanding. Requests beyond the peer's limit are queued. |
| NestingLevel | number | 4 | MMS Initiate: proposed nesting level. |
| AbortOnBilateralTableMismatch | boolean | false | Server-only: end the association if the peer's `Bilateral_Table_ID` does not match `RemoteBilateralTableID`. |
| MaxClientDataSets | number | 16 | Server side: data sets the peer may create per association. |
| MaxDataSetMembers | number | 5000 | Members of a data set. |
| NamePageSize | number | 200 | Server side: names per `GetNameList` response. |
| MaxBufferedChanges | number | 1000 | Server side: the most changes buffered per transfer set that has `AllChangesReported` (1 to 100000, see [Transfer set settings explained](#transfer-set-settings-explained)). When it is reached a warning is logged (once per start of the transfer set; later overflows are logged at trace level) and from then on only the last value of a point that already has a change in the buffer is kept. |
| ReportAckTimeoutms | number | 10000 | Server side, critical transfer sets: how long the client's `Transfer_Report_ACK` for a report may take (0 to 3600000). A report not acknowledged by then is a `Data_failure`: logged as a warning and counted, once per report. 0 = never time out (the oldest outstanding report is then given up only by `MaxUnacknowledgedReports`). The standard sets no timeout `[S]`: see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle). |
| MaxUnacknowledgedReports | number | 100 | Server side, critical transfer sets: the most reports per transfer set waiting for an acknowledgement (1 to 100000). Beyond it the oldest is given up as a `Data_failure`. |
| AcknowledgeCriticalReports | boolean | true | Client side: send `Transfer_Report_ACK` for the reports of a transfer set started with `Critical` true. `false` is a safety valve for a peer that cannot take the acknowledgement (its layout is `[S]`): the peer then counts every report as unacknowledged. |
| NackUnknownReports | boolean | true | Client side: a transfer report that is not from a transfer set this port started (the transfer set name, in its domain, or the data set it names, is not one of ours) is discarded and answered with `Transfer_Report_NACK`, as the standard says. `false` keeps the previous behaviour: such a report is processed as if it were ours, and never answered. (A blocked report from a transfer set that is not ours is NACKed too; with `false` it is counted as undecodable, since its values cannot be typed.) |
| BlockedDataName | string | "Block_Data" | Server side, Block 3 (only with 3 in `SupportedBlocks`): the item id of the **VMD-specific** MMS variable (no domain) that carries the octet string of a blocked report. A real peer uses `Block_Data` `[V]`. An MMS identifier (1 to 255 characters) that is not the name of a special transfer object. A client takes the octet string whatever the variable is called and in whatever scope, so this only matters to a peer that checks the name. See [Blocked transfers (Block 3)](#blocked-transfers-block-3). |
| BlockedRule2 | boolean | true | Server side, Block 3: send a partial report (what changed, `RBE` true) of a blocked transfer set that needs a position above 255 with **rule 2** (rule octet 2, then pairs with 2 octet unsigned positions; every position of that report is then 2 octets, as the rule octet is one per string). The layout is from the standard's text (IEC 60870-6-503 clause 7.2.5.5.3: relative position, 2 octets, unsigned, counted from 0; the byte order is not stated and is taken as big endian). **Seen `[V]`:** a real peer (the client that started the transfer sets, `BlockData`, `RBE` and `Critical` true) accepted this port's rule 2 reports for data sets of about 390 and 500 points and acknowledged every one with `Transfer_Report_ACK`, the association staying up. **Still `[S]`:** a rule 2 report sent *by* a real peer has not been seen (the peer sent none for a big set), and another peer may not implement rule 2. The same real peer was then asked (checked in the peer) about the values of 10 points above position 255, on data sets of about 500 and 390 points with four special objects: **all matched what this port sent** `[V]`, so the 2 octet big endian position, the positions counting the special objects and the value mapping above position 255 are verified for what this port sends. Another peer may not implement rule 2. `false` restores the earlier behaviour: such a report is sent in the ordinary form, with one warning per start of the transfer set (the real peer above did not acknowledge ordinary reports of a `BlockData` transfer set). A position above 65535 fits no rule: that report is always sent in the ordinary form, with one warning per start, whatever this option says. |
| TraceHex | boolean | false | Log every chunk of bytes sent and received on the TCP connection, as hex, at debug level (`ICCPPort` logger). For diagnosing an interoperability problem; the bytes are the unencrypted ICCP traffic, so the log file may contain operational data. |
| TraceHexMaxBytes | number | 512 | With `TraceHex`: bytes shown per log line (16 to 65536); longer chunks are cut. |

**The two bilateral table IDs.** A bilateral table belongs to one direction: it is held by the server for one particular client centre, and its version is the MMS variable `Bilateral_Table_ID` in that client's domain (IEC 60870-6-503 clauses 5.3.4 and 8.3.3 `[T]`;
a visible string of at most 32 characters, and a new value whenever the table changes). The client reads it after the association is up and compares it with the version in the bilateral agreement; if they differ it concludes the association (clause 5.3.3.3 `[T]`).
Each direction therefore has **its own version**, and the standard does not make the two equal. That is why there are two options:
`BilateralTableID` is the version of the table **this port serves** (what the peer reads and checks), and `RemoteBilateralTableID` is the version this port **expects to read** from the peer's table for us. Many agreements use one value for both and then
the two options are set to the same string; they may differ. Both must satisfy the type: at most 32 characters, visible ASCII only (checked when the configuration is loaded). The type description this port gives for `Bilateral_Table_ID` (a `GetVariableAccessAttributes`
on it, in the domain it serves) is a visible string of at most 32 characters (the MMS size is written as -32, a varying length: the value is usually shorter) `[S]`: the standard names the type `Visible-String-32` with a size of 32 characters and does not say whether it is fixed or varying.

### Missing members and diagnostics (client side)

A peer may refuse to define a data set because it lacks some of the members (the point is not in its bilateral table). These keys decide what happens; they have an effect on the client side only
(`Client` or `Peer`, for the `Read` data sets this port defines on the peer), and are valid, with the same meaning, for both port types.

| Name | Type | Default | Description |
|------|------|---------|-------------|
| MissingMemberPolicy | string | "Prune" | `"Prune"`: find the members the peer lacks (a probe, see Troubleshooting), define the data set again with only the members that exist, then carry on as normal (start the transfer set, handle reports, poll); the points of the missing members are flagged. `"Fail"`: the previous behaviour: log the error, skip the data set (the probe still runs, for the diagnostics). |
| MissingMemberQuality | string | "COMM_LOST" | The ODC quality published, once, for the point of each missing member: ODC quality flag names separated by `\|`, for example `"COMM_LOST"` or `"COMM_LOST\|REFERENCE_ERR"`. `ONLINE` is only accepted together with `COMM_LOST` or `REFERENCE_ERR`: a flagged point is never published as good. |
| ProbeOnDefineFailure | boolean | true | Run the probe after a refused definition even with `MissingMemberPolicy` `"Fail"` (with `"Prune"` it always runs). |
| ProbeChunkSize | number | 50 | Variables per probe Read (1 to 500). |

### Points

`Binaries`, `Analogs` and `Counters` are arrays of point objects.

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Index | number | required | The ODC event index. |
| Name | string | required | The TASE.2 object name (at most 255 characters). |
| Direction | string | by `ServiceRole`: Server = "Serve", Client = "Read"; **required** for Peer | `"Serve"`: the value comes from ODC `Event()` into the cache this port serves to the peer. `"Read"`: the value is read from / reported by the peer and published to ODC with `PublishEvent`. |
| Scope | string | "ICC" | `"ICC"` (a variable of a domain, named for one partner) or `"VCC"` (a VMD-specific variable of the control centre as a whole). See [ICC and VCC scope explained](#icc-and-vcc-scope-explained). |
| Domain | string | `LocalDomain` for Serve, `RemoteDomain` for Read | The ICC domain of the point. |
| Type | string | StateQ (Binaries), RealQ (Analogs), DiscreteQ (Counters) | The TASE.2 point type; taken from here, never inferred. See below. |
| ScaleFactor | number | 1.0 | ODC value = TASE.2 value * ScaleFactor + Offset. Must not be 0. |
| Offset | number | 0.0 | See ScaleFactor. |

Direction rules (each violation is a configuration error that names the rule and the fix):

* `Direction` is required on every point when `ServiceRole` is `Peer`.
* `"Serve"` needs `ServiceRole` `Server` or `Peer`; `"Read"` needs `Client` or `Peer`.
* An ODC `Index` may be used once per kind (Binaries, Analogs, Counters) **per direction**: a Peer may have a served binary 0 and a read binary 0. Events *to* the port (`Event()`) address the `Serve` points;
  events published *by* the port come from the `Read` points. If the other ODC port on the Connector cares about the difference, give the two directions distinct index ranges (the examples use 100 and up for the `Read` points).
* A point `Name` may be used once per direction (the served and the read point may share a name, e.g. when the two peers use the same naming, since they live in different domains).
* `Event()` routing: ODC Binary, Analog and Counter events are applied to the `Serve` point with that kind and index; events for an index with no `Serve` point are accepted and ignored. A `Client`-role port
  replies `NOT_SUPPORTED` (it has nothing to serve).

### ICC and VCC scope explained

Every object a TASE.2 server shows to its partner has a **scope**, ICC or VCC. The setting is `Scope` on points and on control devices, and `LocalDomain` / `RemoteDomain` / `Domain` say which domain an ICC object is in. The names are
easy to confuse, so here is what they mean (IEC 60870-6-503, clauses 5.3.1 and 6.3 `[T]`; the wire form is in quirk 21).

* **VCC** is the *virtual control centre*: everything a control centre makes available for inter-control-centre exchange, seen as one server. It maps to the MMS **VMD** (virtual manufacturing device), the top level of an MMS server.
* **ICC** is *inter-control-centre*: the part of that server that is specific to **one partner**. Each partner control centre has its own **domain** (an MMS domain) inside the VCC, named in the bilateral agreement. The objects in that domain
  are the ones agreed with that partner and no other.

| | `Scope` `"VCC"` | `Scope` `"ICC"` (the default) |
|--|-----------------|-------------------------------|
| Belongs to | the control centre as a whole: not tied to a partner | the one partner whose domain it is in |
| Name on the wire | an MMS **VMD-specific** name: just the item name | an MMS **domain-specific** name: the domain name and the item name |
| `Domain` | not used | required: `LocalDomain` for what this port serves, `RemoteDomain` for what it reads (or the point's own `Domain`) |
| Same name twice | one object per name | the same name may exist in several domains, and may mean a different object in each |
| Typical use | data that is the same whoever asks | data prepared for one partner, so that two partners can have different names, values or access for the same real device |

Notes:

* The scope only says *whom an object is relevant to*. It is not access control: what a partner may read or operate is decided by the bilateral table, whatever the scope.
* **Which scope a given point uses is agreed between the two control centres**, so a client has to cope with both, and each point is configured here with the scope the peer uses. When a point cannot be found on the peer (a data set that will not define,
  `object-non-existent` on a read), the wrong scope or domain is the first thing to check: browse the peer with `GetNameList` both at VMD scope and in its domain, and compare with `Scope`, `Domain` and `RemoteDomain`.
* **Not every kind of object can have both scopes.** Points (data values) and control devices can be either. Transfer sets, and the transfer set names handed out by `Next_DSTransfer_Set`, are ICC objects: this port keeps them in `LocalDomain`.
  Data sets that this port serves or defines have a `Domain` and are domain-specific here (there is no `Scope` for a data set).
* **The association objects are split as well:** `Bilateral_Table_ID` is domain-specific (it is read in the ICC domain, and each domain has its own bilateral table and so its own version), while `TASE2_Version` and `Supported_Features` are VMD-specific (VCC scope).
  So this port serves `Bilateral_Table_ID` only when both `LocalDomain` and `BilateralTableID` are set, and reads the peer's only when `RemoteDomain` is set (without a `RemoteDomain` it reads just the two VMD-specific objects); see quirk 21 and the
  note on the two bilateral table IDs under the general options table.
* `LocalDomain` is the domain **this** port serves, the one the peer will look in: it is the name the peer's bilateral agreement uses for us. `RemoteDomain` is the domain on the **peer** that holds what we read and operate: the name in our agreement for it.
  They are normally different names. Each is required only when the configuration has ICC objects that need it (served ICC points, devices or data sets need `LocalDomain`; read ones, or operated devices, need `RemoteDomain`): a port that
  serves and reads only VCC points needs neither, unless it also uses the `Bilateral_Table_ID` as above.

### Data sets

`DataSets`. A data set is either **served** by this port (the peer may read it, or define its own) or **defined on the peer** by this port so that it can be reported or polled. This is its `Direction`:
`"Serve"` (all members are `Serve` points) or `"Read"` (all members are `Read` points). It is inferred from the members; a data set whose members are a mix is an error (split it in two),
and `Direction` can be given explicitly when a name exists in both directions.

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | string | required | The data set name. |
| Direction | string | inferred from the members | `"Serve"` or `"Read"`, as above. |
| Domain | string | `LocalDomain` (Serve) / `RemoteDomain` (Read) | The domain of the data set. |
| Members | array of strings | required | Names of configured points of the data set's direction, in order. |
| PollIntervalms | number | 0 | `Read` data sets only: read the data set every this many ms instead of (or as well as) a transfer set. |

### Transfer sets

Two separate keys, for the two sides:

* `TransferSets` = the transfer sets **this port starts on the peer** (client side: `Client` or `Peer`). `DataSet` is required and must name a `Read` data set.
* `TransferSetPool` = the transfer set names **this port offers** to the peer (server side: `Server` or `Peer`): an array of names (strings), `["TS_1", "TS_2"]`. The peer takes them with `Next_DSTransfer_Set`.

Backward compatibility: an old server configuration with `"TransferSets": [{"Name": "TS_1"}, ...]` still works when the effective role is `Server` (the entries are taken as the pool);
under any other role the entries are start-up transfer sets and need a `DataSet`.

`TransferSets` entries (the transfer sets this port starts on the peer):

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Name | string | empty | A fixed transfer set name on the peer. Empty (the normal case) = ask the peer for a free one with `Next_DSTransfer_Set`. |
| DataSet | string | required | The configured `Read` data set whose members the peer is to report. This port defines it on the peer (in `RemoteDomain`) and then starts the transfer set on it. |
| Conditions | array of strings | ObjectChange, plus IntervalTimeOut if `Interval` > 0, plus IntegrityTimeOut if `IntegrityCheck` > 0 | The events that make the peer send a report: `IntervalTimeOut` (a periodic report), `IntegrityTimeOut` (a periodic complete report), `ObjectChange` (a member changed), `OperatorRequest` (an operator asked for a report), `OtherExternalEvent` (an external event happened). See [Transfer set settings explained](#transfer-set-settings-explained). |
| StartTime | number | 0 | When the transfer set becomes active, as seconds since 1970-01-01 UTC. 0 = at once. |
| Interval | number | 0 | Seconds between the periodic reports of the `IntervalTimeOut` condition. Needed (> 0) when `IntervalTimeOut` is requested, unused otherwise. |
| IntegrityCheck | number | 0 | Seconds between the complete reports of the `IntegrityTimeOut` condition. Needed (> 0) when `IntegrityTimeOut` is requested, unused otherwise. |
| BufferTime | number | 0 | Seconds the peer waits after the first change before it sends an `ObjectChange` report, so that changes arriving close together share one report. 0 = report each change at once. |
| TLE | number | 0 | The transfer set's time limit for execution, in seconds: the longest the peer's report may take to be generated and sent, after which the client no longer wants it. 0 = no limit. Sent to the peer. This port's server discards a report it hands over later than this after it was due: see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle). |
| RBE | boolean | false | Report by exception. `true`: a report carries only the members that changed. `false`: a report carries every member of the data set. |
| BlockData | boolean | false | Ask the peer to send the values in the blocked form of TASE.2 Block 3: the special objects as variables and the members of the data set (the special objects included) as one octet string. Rule 0 with `RealQ` points matches a capture of a real peer `[V]`; the rest is `[S]`: see [Blocked transfers (Block 3)](#blocked-transfers-block-3). A configuration warning says so, and a warning at association is logged if the peer does not advertise Block 3 in its `Supported_Features` (add 3 to `RequiredRemoteBlocks` to end the association instead). A blocked report that cannot be decoded is logged with a hex dump and not applied. |
| Critical | boolean | false | Ask for critical reports: the client acknowledges each report it receives from this transfer set with a `Transfer_Report_ACK`, and the server watches for the acknowledgements. This port's client sends them (unless `AcknowledgeCriticalReports` is false) and its server tracks them: see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle). The layout of the acknowledgement has not been checked against a peer `[S]`. A critical transfer set should have `Transfer_Set_Time_Stamp` in `SpecialObjects`, which the acknowledgement echoes (the configuration warns if not). |
| AllChangesReported | boolean | false | `true`: every qualifying change is reported, so a change must not disappear because another one followed it before the report was sent. `false`: changes may be consolidated, and the receiver only needs the latest value. `true` is only valid with TASE2Edition 2000-08, `RBE` true, the `ObjectChange` condition and a `BufferTime` above 0: otherwise it must be `false`, and a configuration that says `true` is rejected (see below). |
| EventCode | number | 0 | The event code requested with the transfer set. Sent as is; peers seen send 0. |
| SpecialObjects | array of strings | Transfer_Set_Name, DSConditions_Detected | The objects at the head of the data set, ahead of the points, in order. They carry the per-report information described below. Must start with `Transfer_Set_Name`; the others are `DSConditions_Detected`, `Event_Code_Detected` and `Transfer_Set_Time_Stamp`. |

### Transfer set settings explained

A transfer set is how this port asks a peer to send it values without polling. The settings fall into four groups: **when** a report is sent, **what** it contains, **how** changes are collected, and **what is carried with each report**. The times are all in seconds. Unless noted otherwise, the settings are sent to the peer in the `DSTransferSet` write that starts the transfer set, and **the peer** decides when to report: what this port's own server does with them when a peer starts a transfer set on it is given under "What this port's server does".

**When a report is sent (`Conditions`, `StartTime`, `Interval`, `IntegrityCheck`)**

* `StartTime`: the transfer set does nothing before it. 0 starts it immediately. A non-zero value is a UTC time, in seconds since 1970-01-01.
* `IntervalTimeOut` with `Interval`: a report every `Interval` seconds, whether or not anything changed. Requesting the condition with `Interval` 0 is a configuration error. With `RBE` true the periodic report carries only what changed since the last report.
* `IntegrityTimeOut` with `IntegrityCheck`: a **complete** report of every member every `IntegrityCheck` seconds, whatever `RBE` says. It exists so that a client that missed a change, or has just connected, gets back in step. Requesting the condition with `IntegrityCheck` 0 is a configuration error.
* `ObjectChange`: a report when a member of the data set changes (after `BufferTime`, see below). Without it a `RBE` transfer set reports nothing between the periodic reports.
* `OperatorRequest` and `OtherExternalEvent`: a report when an operator, or some other event on the peer, asks for one. They do not depend on the data changing; peers commonly request both on every transfer set. Without them such reports are not sent to this transfer set.

If `Conditions` is not given, it defaults to `ObjectChange`, plus the periodic condition for each of `Interval` and `IntegrityCheck` that is above 0.

**What a report contains (`RBE`)**

* `RBE` false: every report carries all members of the data set, so a report is a complete picture.
* `RBE` true: a report carries only the members that changed since the last report. This is much smaller for a large data set, and is usually combined with `ObjectChange` for the changes and `IntegrityTimeOut` for a periodic complete report.

**How changes are collected (`BufferTime`, `AllChangesReported`, `TLE`)**

* `BufferTime`: after the first change the peer waits `BufferTime` seconds before it reports, and then sends one report for everything that changed in that time. 0 sends a report for each change at once, which is the quickest, and the busiest on the link.
* `AllChangesReported` says what happens to changes that arrive while the peer is collecting them in the buffer time, so it goes with `BufferTime`:
  * `true`: every qualifying change is to be reported. Intermediate changes must not simply disappear because another change occurred before transmission.
  * `false`: changes may be consolidated. The receiver needs the current, latest value of what changed, not necessarily every intermediate transition.
  * **`true` is only valid when all of these hold; otherwise it must be `false`:** the edition is 2000-08 (the 1996-08 `DSTransferSet` has no such member), `RBE` is true, the `ObjectChange` (send on change) condition is requested, and `BufferTime` is above 0 (with no buffer time there is nothing to collect changes in). A peer refuses a transfer set that breaks the rule (data access error `object-attribute-inconsistent` [8] on the write), so the configuration is checked when it is loaded: `true` with an unmet condition is a configuration error that names what is missing, except for 1996-08, where it is ignored (sent as nothing) with a warning.
  * The default is `false`, which is always valid. A real 2000-08 peer writes `false` on all its own transfer sets.
  * Use `true` where the history matters (for example a state that flips and flips back, which must be seen as two events). Use `false` where only the present value does (an analog that is polled by the control system anyway).
  * `[S]`: the rule above is the peer's, as reported; the refusal of `true` with `BufferTime` 0 was seen on the wire, that a `true` with all the conditions met is accepted has not been checked against a peer.
  * This port's own server honours the flag when a peer starts a transfer set on it: see "What this port's server does" below, and quirk 53 for how the several changes of one point are laid out.
* `TLE`: the time limit for execution (seconds, 0 = none): the time within which the peer should generate and send a report, which tells it how long a report is still of use to this client (IEC 60870-6-503 `[T]`). A server that cannot get a report out within it discards the report instead of sending it late. The peers seen send 0. What this port's server does with it is under [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle). `[S]`: the meaning of `EventCode` is taken from the specification and has not been checked against a peer; `BlockData` is Block 3, see [Blocked transfers (Block 3)](#blocked-transfers-block-3) (rule 0 with `RealQ` points seen from a real peer, the rest not).

**What is carried with each report (`SpecialObjects`, `EventCode`)**

The first members of the data set are not points: they are the special objects, and each report carries their current values ahead of the points, in the order given:

* `Transfer_Set_Name` (always first): the name of the transfer set that produced the report. It is how a client with several transfer sets tells which report belongs to which.
* `DSConditions_Detected`: which conditions caused this report (interval, integrity, object change, operator request, other external event). It lets the receiver recognise a complete integrity report, or the reason for an unexpected one.
* `Event_Code_Detected`: the event code of the operator request or other external event that caused the report (0 for a periodic or change report).
* `Transfer_Set_Time_Stamp`: when the peer generated the report.

The report layout is the data set's layout, and this port defines the data set on the peer, so the special objects are your choice. Whether a given peer accepts a given choice is up to that peer (the probe does not check it: quirk 37). A real 2000-08 peer uses `Transfer_Set_Name` alone on its periodic data sets and `Transfer_Set_Name` followed by `Event_Code_Detected` on its report-by-exception ones, and a 1996-08 peer used `Transfer_Set_Name` and `DSConditions_Detected`: copying the values of the peer you talk to is the safest.

`EventCode` is the event code requested with the transfer set (the specification's `Event_Code_Requested`). It is sent as is, and peers seen send 0.

**Values a real 2000-08 peer uses for its own transfer sets** (a good starting point against such a peer):

| | Periodic set | Report by exception set |
|---|---|---|
| `SpecialObjects` | `["Transfer_Set_Name"]` | `["Transfer_Set_Name", "Event_Code_Detected"]` |
| `Conditions` | `IntervalTimeOut`, `OperatorRequest`, `OtherExternalEvent` | `IntegrityTimeOut`, `ObjectChange`, `OperatorRequest`, `OtherExternalEvent` |
| `Interval` / `IntegrityCheck` / `BufferTime` | 5 / 0 / 0 | 0 / 30 / 2 |
| `RBE` / `AllChangesReported` | false / false | true / false |
| `StartTime`, `TLE`, `BlockData`, `Critical`, `EventCode` | 0, 0, false, false, 0 | the same |

**What this port's server does with a transfer set that a peer starts on it**

| Setting | Server side |
|---------|-------------|
| `StartTime` | Honoured: nothing is sent before that time. |
| `Interval`, `IntegrityCheck`, `Conditions` | Honoured. The first integrity report goes out at once when the transfer set starts. |
| `RBE`, `BufferTime` | Honoured. |
| `AllChangesReported` | Honoured when the rule above is met (`RBE` true, `ObjectChange` requested, `BufferTime` above 0), in the 13 member form: **every change** of a point within the buffer time is reported, in the order it happened, each with the value, quality and time stamp it had. The layout is not defined by the standard: this port sends **consecutive reports, each an ordinary report by exception that lists a point at most once** `[S]` (see below and quirk 53). Otherwise (or with `false`) accepted, and the report carries the latest value of each changed member (an info line at the start of the transfer set says which). |
| `TLE` | Honoured as far as the stack allows: a report handed over more than `TLE` seconds after it was due is discarded (see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle)). |
| `Critical` | Honoured: each report is remembered until the client's `Transfer_Report_ACK` arrives (see the same section). |
| `BlockData` | Honoured when this port advertises Block 3 (3 in `SupportedBlocks`): every report of the transfer set is sent in the blocked form (see [Blocked transfers (Block 3)](#blocked-transfers-block-3)): rule 0 for a complete report (also of an `RBE` true set), rule 1 for a partial report of an `RBE` true set. The layout is a real peer's `[V]` for rule 0 and for rule 1 with one or several points, `[S]` for the rest (the other point types' octets, an event code other than 0). A real peer has **accepted and acknowledged this port's blocked reports**, rule 0 and rule 2 included `[V]` (the 2 octet big endian position, the positions counting the special objects and the values above position 255 were checked in the peer and matched). A partial report that needs a position above 255 is sent with rule 2 (`BlockedRule2`, default true; with `BlockedRule2` false such a report is sent in the ordinary form, with one warning per start; another peer may not implement rule 2). Without 3 in `SupportedBlocks` it is accepted and ignored: the reports are ordinary and byte for byte what they were before Block 3 existed. |
| `EventCode` | Accepted and kept, not acted on. |

**How the server reports every change (`AllChangesReported` true) `[S]`.** IEC 60870-6-503 clause 5.3.10.2.2 says that with `RBE` true and a non-zero buffer time, `true` means all value changes of an object within the buffer period are reported and `false` that only the last is;
clause 7.2.5.5.3 says a report is an `InformationReport` whose list of variables is the changed members and whose data holds their values, and that it must fit one MMS PDU. Neither says how several values of the *same* object are laid out, so this is this port's choice, **not verified against a
real peer**: the changes of the buffer period are cut into reports **in the order they happened**, a new report starting wherever a point would otherwise appear in the report twice. Each report is therefore an ordinary report by exception (the special objects of the data set, then
the changed members, each once, in the order of their changes) that any client understands, and each member carries the value, quality and time stamp of its own change. An example, points A and B changing A1, B1, A2, A3, B2 within one buffer time: reports `[A1 B1] [A2] [A3 B2]`. The reasons for this
over listing the point several times in one report: a client that keeps one value per name per report (a map) would lose all but the last value from the repeated entries, while a sequence of ordinary reports loses nothing; no report is ever larger than a report of all changed members, so the single PDU rule is not made worse (the reports of a
buffer period are many PDUs); and the timers and the report sequencing stay as they are. The price: more reports than changes in the worst case (one per change when one point changes repeatedly), sent back to back when the buffer time ends. How the buffer behaves:

* The buffer time starts with the first change, as ever; when it ends the reports are sent (the condition detected is `ObjectChange`) and the buffer is empty.
* An `IntegrityTimeOut` report (complete, the current state), or an `OperatorRequest` or `OtherExternalEvent` report, that comes while changes are buffered is preceded by the buffered changes, so no change is lost to it and the order is kept. An `IntervalTimeOut` report with `RBE`
  is the buffered changes (condition `IntervalTimeOut`), as the standard has the interval report all changes too. `[S]`
* The buffer holds at most `MaxBufferedChanges` changes per transfer set (default 1000). Beyond that a warning is logged, once per start of the transfer set (each later change that finds the buffer full is logged at trace level), and a new change of a point that has a change in the buffer replaces the latest of them (the last value wins), so the memory is bounded by the cap plus the points of the data set;
  the order of the other changes is kept.
* Stopping the transfer set, deleting its data set and the end of the association clear the buffer (the buffered changes belong to the association).
* A transfer set started with `AllChangesReported` true but without `RBE`, `ObjectChange` or a buffer time behaves as with `false` (the info line at its start says so); `BufferTime` 0 reports each change at once, as before.

### Critical reports and the time limit TLE

Both are parts of the `DSTransmissionPars` that the client writes to start a transfer set. This section says what this port does with them on each side, and what is the standard (`[T]`, IEC 60870-6-503 clauses 5.3.10, 7.2.5.5.2 for the acknowledgement and 7.2.5.5.3 for `TLE`) and what is this port's own choice (`[S]`). Quirks 56 to 59 have the same in table form.

**Critical reports: what the standard says `[T]`.** With `Critical` true the client acknowledges each report it receives from the transfer set, with an unconfirmed `InformationReport` that carries two named variables: `Transfer_Report_ACK` (an object name: the transfer set name as in the report received, with the domain of the variable name equal to that of the transfer set) and `Transfer_Set_Time_Stamp` (the time stamp of the report received).
A client that receives a report with a data set or transfer set name that is not correct discards the data and sends a `Transfer_Report_NACK` instead (the same transfer set name and domain as the report). The server, if the acknowledgement is not correct or does not come, or a NACK comes, triggers the MMS event condition `Data_failure` if it implements event conditions; anything else, such as telling the operator, is a local matter.
The standard does not define a timeout, a resend, or any holding back of further reports while one is unacknowledged.

**Client side (`Client`, `Peer`).**

* After it has processed a report of a transfer set that it started with `Critical` true, the client sends the acknowledgement above: `Transfer_Report_ACK` (value: the transfer set name from the report, as received) and `Transfer_Set_Time_Stamp` (the value from the report), both named in the domain of the transfer set. If the report has no `Transfer_Set_Time_Stamp` (its data set does not have that special object) the time stamp variable is left out; give a critical transfer set `Transfer_Set_Time_Stamp` in `SpecialObjects` (the configuration warns if it is missing). `[S]` for the exact layout: the order of the two variables, leaving the time stamp out, and the encoding of the object name value (the same structure as `Transfer_Set_Name` in a report, quirk 56).
* A transfer report whose transfer set name or data set is not one this port started is discarded, and answered with `Transfer_Report_NACK` carrying the transfer set name and domain of the report. A report is ours if its transfer set name (in its domain) is that of a transfer set this port started on the current association (also one that has since been stopped, so that a report that was already on its way is not refused); if it names a data set, that data set must be one this port defined and its transfer set the one started for it. The first such report of each transfer set is logged as a warning, the later ones at debug level. A report that cannot be decoded at all has no name to answer and is only logged. `NackUnknownReports` `false` keeps the behaviour before this existed: the report is processed and never answered. `[S]`: what counts as a correct name, and the default of `true`, which rejects reports that were previously published (quirk 59).
* `AcknowledgeCriticalReports` `false` sends no acknowledgements at all. A peer that answers an acknowledgement it does not understand with a reject or an abort would otherwise cost the association, so this is the way out, until a real peer has accepted the two variable form (a real peer's own acknowledgement layout matches this port's, see quirk 56).

**Server side (`Server`, `Peer`).**

* A transfer set that the peer started with `Critical` true is *critical*. Each report this port sends from it is remembered with its `Transfer_Set_Time_Stamp` (if the layout of its data set carries one) and the time it was sent, until the acknowledgement comes. A transfer set that is not critical is not tracked at all: its reports are sent exactly as before and nothing is waited for.
* The client's `Transfer_Report_ACK` clears the oldest remembered report it can be for. When both have a time stamp they must be equal (a wrong time stamp clears nothing); a report or an acknowledgement without one matches the oldest report. Two reports in the same second share a time stamp and are cleared oldest first. The server is lenient in what it accepts: the variables may come in any order, other variables are ignored, a time stamp that cannot be read counts as none, and the scope and domain of the variable names are not checked (the transfer set value must be one of this port's transfer sets, in `LocalDomain`). An acknowledgement that matches nothing (late, repeated, or for another time stamp) is only logged at debug level.
* **`Data_failure`.** A `Transfer_Report_NACK` for a critical transfer set, a report that has not been acknowledged after `ReportAckTimeoutms`, and the oldest report when more than `MaxUnacknowledgedReports` are waiting, are each a `Data_failure`. This port has no MMS event conditions, so it **logs a warning** (`transfer set TS_1: Data_failure: the critical report with time stamp ... was not acknowledged: ...`), once per report, and **counts it** in `DataFailures` (with `ReportAckTimeouts`, `ReportNacksReceived` and `ReportAckOverflows` for the causes). Nothing else is done: the report is **not resent**, further reports are **not held back**, and the association is never touched, so a missing acknowledgement can neither stop the data nor break the association. A NACK for a transfer set that is not critical is logged as a warning and counted as a NACK, not as a `Data_failure`.
* The remembered reports of a transfer set are dropped, with no failure counted, when it is stopped or started again, when its data set is deleted and when the association ends.
* `[S]` the timeout `ReportAckTimeoutms` (default 10 s; the standard sets none; 0 switches the check off), the cap `MaxUnacknowledgedReports` (default 100), the matching rule above, the leniency, the absence of resend and holding back, and logging and counting instead of the event condition are this port's choices.
* The counters are in `GetStatistics`: `CriticalReportsSent`, `CriticalReportsAcknowledged`, `CriticalReportsOutstanding` (now, not cumulative), `ReportNacksReceived`, `ReportAckTimeouts`, `ReportAckOverflows`, `DataFailures` and `ReportsDiscardedTLE` for a port with a server side, and `ReportAcksSent` and `ReportNacksSent` for a port with a client side. They are cumulative over the associations, except the outstanding count.

**The time limit `TLE`.** `[T]` `TLE` is the time (seconds) within which the server should generate and send a report; it tells the server how long the client considers a report valid. A server that expects to take longer discards the report (or does not generate it). If `TLE` was exceeded for a previous report, the next report is generated whether or not it would exceed `TLE` too. 0 is ignored by the server. This port's server does this, `[S]` as follows:

* A report is **due** when its trigger happens: the end of the buffer time for an `ObjectChange` report, the interval or integrity time for a periodic one. It is **handed over** when it is passed to the transport, which is when the port's tick (every 50 ms) finds it due. If handing over is more than `TLE` seconds after it was due, the report is discarded: a warning is logged (`a report was due ... ms before it could be sent, more than its time limit TLE`), it is counted in `ReportsDiscardedTLE`, the transfer set carries on, and the next report of that transfer set is sent whatever its age (logged at debug level). Exactly `TLE` late is still in time.
* Reports that are sent as soon as they arise (no buffer time, an operator request or other external event) are never late. The buffer time is a delay that was asked for, so the time is not counted from the first change.
* The discarded report is simply not sent. With `RBE` the changes in it are lost until the next report that carries them (the next integrity report); with `AllChangesReported`, where one buffer period is sent as several consecutive reports, each is judged by itself, so a discarded report drops only the changes in that report and the next one is then sent regardless. A discarded report is not a critical report: nothing waits for its acknowledgement.
* **What is not measured.** The stack has no back-pressure from the TCP connection (a write is handed to the socket at once and nothing tells when it left), so the time a report waits in the operating system or on a slow link is not seen, and `TLE` does not discard a report because of that. What it sees is the lateness of the port's own tick, which means it protects against stale data after the port itself was held up (a stalled host, a long pause of the process), not against a slow network. This is as much of the rule as the information in the stack supports.

**What a capture of a real peer has confirmed, and what it has not.** A capture of this port against a real peer, both directions, with `Critical` true on the transfer sets on each side, confirmed `[V]` the layout of the acknowledgement of a report without a time stamp (an unconfirmed `InformationReport`, one variable `Transfer_Report_ACK` in the domain of the transfer set, the value the `{scope, domain, name}` structure `Transfer_Set_Name` has, no reply) and that the peer sends one per report. It also showed that the peer's own critical reports carry a time stamp, which this port echoes in the variable `Transfer_Set_Time_Stamp`.
Still to be confirmed: the two variable form (the peer sent none, because the data sets it defined on this port, which decide what a report holds, have no `Transfer_Set_Time_Stamp` member: a capture with the time stamp enabled in the peer's own configuration would show whether it then adds the variable, and in which order), whether the peer accepts our acknowledgement (it sent no `Transfer_Report_NACK` and kept the association, but its own log is not visible), what a real server does when it is not acknowledged (which would show whether a timeout or a resend is expected), whether a real peer sends `Transfer_Report_NACK` and for what, and how a real peer reacts to an acknowledgement it does not expect.

### Blocked transfers (Block 3)

Block 3 is the `BlockData` attribute of a transfer set: instead of one MMS variable per value, a report carries the values of the data set as **one octet string**, besides the special objects as separate variables (IEC 60870-6-503 clause 7.2.5.5.3; the value types are in IEC 60870-6-802 clause 6.2.1).
**What has been seen on the wire `[V]`:** captures of a real peer acting as the *server*, to which this port's client asked for `BlockData` true: with `RBE` false (transfer sets with `ObjectChange`, `Interval` and `IntegrityTimeOut`, `Critical` true, a data set of `Transfer_Set_Name`, `Transfer_Set_Time_Stamp` and `DSConditions_Detected` followed by 126 to 231 `RealQ` points), with `RBE` true (the same kind of data set with 126 `RealQ` points), and with a data set of **four** special objects (`Transfer_Set_Name`, `Transfer_Set_Time_Stamp`, `DSConditions_Detected` and `Event_Code_Detected`) followed by 50 `StateQ` points.
Every complete report is **rule 0** (also the integrity reports of the `RBE` true transfer set, every 10 s: 701 octets, `1 + 65 + 4 + 1 + 126 * 5`; and for the four special objects and 50 `StateQ` points 123 octets, `1 + 65 + 4 + 1 + 2 + 50 * 1`), and the partial (object change) reports of the `RBE` true transfer set are **rule 1** with the changed points, one or several. The layout below marked `[V]` is what they hold, to the last octet. **Not seen `[S]`:** a rule 2 report sent *by* a real peer, the octets of every point type but `RealQ` and `StateQ`, the byte order and sign of an `Event_Code_Detected` other than 0 (only the value 0 was seen), and a real peer asking this port's server for one
(this port's server follows the same layout: a real peer, as the client, accepted and acknowledged its rule 0, rule 1 and rule 2 reports and applied the values of the rule 2 report above position 255 correctly `[V]`). The code has every `[S]` choice in one place so that a later capture can correct it, and a report that cannot be decoded is logged with the diagnostics a correction needs (see [How to capture a real peer's blocked report to correct this](#how-to-capture-a-real-peers-blocked-report-to-correct-this)). Quirks 66 to 70 have the same in table form.

**What is implemented.**

* **Server** (`Server`, `Peer`): a transfer set that the peer starts with `BlockData` true is reported in the blocked form **only when 3 is in `SupportedBlocks`** (the port then advertises Block 3 in its `Supported_Features` and honours the flag): **rule 0 for a complete report** (the start-up report, integrity, interval of a set without `RBE`, operator request, other external event; also for an `RBE` true set), **rule 1 for a partial report of an `RBE` true set** (see below). With the default `[1, 2]` `BlockData` stays accepted and ignored, and the reports are byte for byte what they were before Block 3 was implemented.
  It covers every report of the transfer set: the integrity report sent at the start, the periodic ones, the object change (buffer time) reports, the operator request and other external event reports, and the consecutive reports of `AllChangesReported` (each is encoded blocked on its own). `Critical` acknowledgements, `TLE`, the buffer and the statistics are those of any report: the blocked form is a different *encoding* of the same report content.
* **Client** (`Client`, `Peer`): a report of the blocked form is split by the members of the data set it defined (the special objects and the points), and its points are published exactly as an ordinary report's. The report's time stamp, conditions and `Critical` acknowledgement come from the separate variables, as for an ordinary report. A block that cannot be decoded is not applied at all (see below). `BlockData` true in a `TransferSets` entry asks the peer for this form.

**The report `[V]`.** An `InformationReport` whose variable list has the special objects of the data set plus one more (**four** items in the capture, with three special objects): the special objects (`Transfer_Set_Name`, `Transfer_Set_Time_Stamp`, `DSConditions_Detected`, and `Event_Code_Detected` if the data set has it) as **domain-specific** variables in the domain of the transfer set, in data set order, then **one VMD-specific variable** (no domain) with the item id `Block_Data`. The results are the special objects' values (the name structure, the time stamp integer, the conditions bit string) and **one octet string**.
So the special objects appear **both as separate variables and inside the octet string**. (With `BlockData` false a report is as it always was: RBE true, the variables that changed; RBE false, the name of the data set.) The octet string starts with the **RBE blocking rule**, one octet:

| Rule | When `[T]` | Then |
|------|-----------|------|
| 0 `[V]` | `RBE` false, or any complete report | **every member of the data set, in data set order, the special transfer objects included**, each in a fixed width storage form (below) |
| 1 (Offset1) `[V]` for one point or several | `RBE` true | repeated pairs of the **position** of a point in the data set's member list (1 octet unsigned, counted from 0, **the special objects counted**: with three of them the first point is position 3) and its value. **Only the points reported, no special objects in the string** |
| 2 (Offset2) `[V]` as sent by this port: accepted, acknowledged and applied correctly by a real peer | `RBE` true | the same with a **2 octet unsigned big endian** position (the standard's text: "relative position (2 octets, unsigned integer)", its byte order not stated there; big endian `[V]`, positions counting the special objects `[V]`), the rule octet 2. **One rule per string:** when one position of a report is above 255 every position of that report is 2 octets. A real peer acknowledged this port's rule 2 reports of 71 to 106 pairs (`1 + n * 7` octets for `RealQ`: 2 octet position, 4 octet float, flags), the largest positions 396 to 495; a rule 2 report sent by a real peer has not been seen |

**Which rule `[V]`:** a **complete** report (the start-up report, the integrity reports, the report of an operator request or other external event) is **rule 0, also for an `RBE` true transfer set** (the standard's rule 0 is the one without RBE: a complete report lists every member in order); a **partial** report of an `RBE` true transfer set (an object change after the buffer time, the `Interval` report of what changed, the consecutive reports of `AllChangesReported`) is **rule 1**.
Three real rule 1 strings, each 7 octets: `01 05 C2 A0 00 00 08`, `01 05 42 C8 00 01 08` and `01 05 C2 BA 14 7B 00`: rule 1, position 5 (the third point of a data set with three special objects), then a `RealQ` value: a big endian float (-80.0, 100.00007 and -93.04) and the flags octet (`08`, `08`, `00`).
**Several changed points `[V]`** make several pairs in one report, in ascending position order, the positions counting every special object. A real 13 octet string of a data set with three special objects (the first point, position 3, did not change): `01 | 04 xx xx xx xx 00 | 05 yy yy yy yy 00`: rule 1, the point at position 4 (a `RealQ`: a float and the flags octet `00`), the point at position 5 (a float and `00`). With synthetic values (1.5 valid, then -2.0 with flags `10`) it reads `01 04 3F C0 00 00 00 05 C0 00 00 00 10`. With **four** special objects (`Event_Code_Detected` as well) every position is one higher: the first point is position 4.
**This port's server** sends, for a partial report, rule 1 with **only the changed points** as (position, value) pairs, **in ascending position order** `[V]` (a plain `Offset1` report; with `AllChangesReported` the order of the changes `[S]`), when the largest position in the report is at most 255. A report that needs a position above 255 is **rule 2** (rule octet 2, every pair with a 2 octet position, the positions still counting the special objects as for rule 1 `[V]`): the server sends it **by default** (`BlockedRule2` true). **Seen `[V]`: a real peer accepted and acknowledged it** (four such reports of 71 to 106 changed `RealQ` points, on data sets of about 390 and 500 points with four special objects, each followed by the peer's `Transfer_Report_ACK`, the association staying up; in the same session 62 of 62 blocked reports were acknowledged). **Still `[S]`:** that a rule 2 report sent *by* a real peer has not been seen (the peer sent none for such a set); another peer may not implement rule 2. With `BlockedRule2` false the server sends **that report in the ordinary form** instead and logs **one warning per start of the transfer set**; the real peer did not acknowledge ordinary reports of a `BlockData` transfer set. A position above 65535 fits no rule (two octets): that report is **always** sent in the ordinary form, with one warning per start (`the blocked form cannot be built`), the other reports of the transfer set staying blocked. The same ordinary-form fallback, with one warning per start, applies to a data set with a member that is not a point (quirk 60). The fallback condition is one small function, `ResponderSession::BlockedUnsendable` (`Tase2Responder.cpp`), and the rule 2 test is in `BuildBlockedValues`: lift them when a capture settles what they guard.
**The client** decodes rules 0, 1 and 2 (it only reads): rule 1 and the pairs of rule 2 are repeated pairs as above (`[V]` for rule 1; the client's reading of rule 2 is the standard's text only `[S]`, as no real peer has sent one). A position below the number of special objects is read leniently: the storage form of that special object is taken from the string and its value ignored (the separate variables carry the real ones). A position beyond the member list, a truncated value or a leftover octet makes the block undecodable (see below).
**What would settle the rest:** a rule 2 report sent by a real peer (a data set of more than 256 members that the peer itself reports with rule 2, if it ever does), and, for the other `[S]` items, the captures listed in the capture how-to.

**The storage forms of the special objects.**

| Object | Octets | Form |
|--------|-------:|------|
| `Transfer_Set_Name` `[V]` | 65 | the scope (1 octet: 1 for a domain-specific name, 0 for a VMD-specific one), the domain id (32 octets) and the item id (32 octets), ASCII, zero padded |
| `Transfer_Set_Time_Stamp` `[V]` | 4 | unsigned seconds since 1970, big endian (the integer of the separate variable) |
| `DSConditions_Detected` `[V]` | 1 | the content octet of the bit string, bit 0 (`IntervalTimeOut`) the most significant: `0x80` is `IntervalTimeOut`, `0x40` `IntegrityTimeOut`, `0x20` `ObjectChange`, `0x10` `OperatorRequest`, `0x08` `OtherExternalEvent` |
| `Event_Code_Detected` `[V]` | 2 | two octets, the standard types it Integer16 (IEC 60870-6-802 G.3.6); the value 0 (`00 00`) was seen. Byte order and sign of other values `[S]`: big endian, two's complement. A data set with it is reported in the blocked form like any other; a code outside -32768 to 32767 is cut to 16 bits |

A transfer set name, or the domain of the transfer set, longer than 32 octets has no storage form: this port's server then sends that transfer set's reports in the ordinary form and logs one warning per start of the transfer set (the configuration warns when 3 is in `SupportedBlocks` and `LocalDomain` or a name in `TransferSetPool` is longer than 32 characters).

**The octets of a point `[V]` for `RealQ` and `StateQ`, `[S]` for every other type.** The captures have `RealQ` points (**5 octets, a big endian IEEE 754 single and the flags octet**) and `StateQ` points (**1 octet**, the content octet of its bit string, the same octet as in an ordinary report). For the other types this port's reading is the plain one: fixed width, big endian numbers, the components of a structure one after the other in the order this port already uses on the wire for the type
(time stamp, [milliseconds,] value, flags, [COV]; see the point types under [Points, data sets and quality](#points-data-sets-and-quality)), the flags and the state as the one octet of the 8 bit string (the state in the top two bits). A time stamp is seconds since 1970 as an unsigned 32 bit number, milliseconds and COV unsigned 16 bit numbers, a real value an IEEE 754 single, a discrete value a two's complement 32 bit number.

| `Type` | Octets | In this order |
|--------|-------:|---------------|
| `Real` `[S]` | 4 | value |
| `RealQ` `[V]` | 5 | value, flags |
| `RealQTimeTag` `[S]` | 9 | time stamp, value, flags |
| `RealExtended` `[S]` | 13 | time stamp, milliseconds, value, flags, COV |
| `State` `[S]` | 1 | state in the two top bits, the other six bits 0 |
| `StateQ` `[V]` | 1 | flags (state in the top two bits) |
| `StateQTimeTag` `[S]` | 5 | time stamp, flags |
| `StateExtended` `[S]` | 9 | time stamp, milliseconds, flags, COV |
| `Discrete` `[S]` | 4 | value |
| `DiscreteQ` `[S]` | 5 | value, flags |
| `DiscreteQTimeTag` `[S]` | 9 | time stamp, value, flags |
| `DiscreteExtended` `[S]` | 13 | time stamp, milliseconds, value, flags, COV |

The total of a rule 0 block is `1 + 65 + 4 + 1 + 5 N` octets for the three special objects and `N` `RealQ` points (821 for 150 points), with no octet left over `[V]`. A worked example (synthetic names and values, the layout of the capture): rule 0, the name `DOMAIN_A` / `TS_POOL_001`, the time stamp 1700000000, `IntervalTimeOut`, and two `RealQ` points, 1.5 valid and -2.0:
`00` | `01` `44 4F 4D 41 49 4E 5F 41` + 24 zeros `54 53 5F 50 4F 4F 4C 5F 30 30 31` + 21 zeros | `65 53 F1 00` | `80` | `3F C0 00 00 10` | `C0 00 00 00 00` (81 octets).

**Everything neither the standard nor the capture says, as this port chose it `[S]`** (each is in one place in the code, named below):

1. **Rule 2** (a position above 255: the rule octet 2, 2 octet positions whose byte order the standard does not state, and one rule, so 2 octet positions, for every pair of the report) and the choice of it. A real peer accepted and acknowledged this port's rule 2 reports and applied the values above position 255 correctly `[V]` (checked in the peer: the 2 octet big endian position and the position counting the special objects included); what stays `[S]` is the client's reading of a rule 2 report from a real peer (none seen), and the choice of rule 2 for a peer that does not implement it. The server sends it unless `BlockedRule2` is false, when such a report goes in the ordinary form (`ChooseRule`, `Encode`, `Decode` in `Tase2Blocked.cpp`; `ResponderSession::BuildBlockedValues`).
2. **The octets of every point type but `RealQ` and `StateQ`**, and the byte order and sign of an `Event_Code_Detected` other than 0 (`ValueWidth`, `AppendValue`, `ReadValue`, `SpecialWidth`, `AppendSpecial`, `ReadSpecial`).
3. **What the real peer does as a client** asking this port's server for blocked reports (`SupportedBlocks` with 3): it accepted and acknowledged this port's blocked reports of rules 0, 1 and 2, and applied the rule 2 values above position 255 correctly `[V]`; only this one peer has been seen.
4. **Which values go in for RBE**: the same ones an ordinary RBE report would list (the points that changed, in the order of the data set; with `AllChangesReported` the changes in the order they happened, as consecutive reports each listing a point at most once, quirk 53, each blocked on its own), as (position, value) pairs: the pairs and their ascending order are `[V]`, the order with `AllChangesReported` is `[S]`.
5. **A data set with a member that is not a point** (a device tag object, quirk 60) has no octet layout: its reports are sent in the ordinary form, with one warning per start of the transfer set.
6. **Receipt**: what is accepted and refused (below).

**What the client accepts, and what it does with a block it cannot decode.**

* A report is **recognised as blocked by its shape**: it lists its variables, and exactly one of them is not a special transfer object (by item name, in any scope) and has an octet string as its value. The name and the scope of that variable are not checked (a real peer's is VMD-specific `Block_Data`; the name seen is logged at debug level), nor its position, nor the order of the special objects. A report that names its data set instead of listing its variables is not recognised `[S]`.
* The block is read with the members of the data set the client defined on the peer (the special objects and the points; after pruning missing members, the data set as it is on the peer now). Rule 0 must be exactly the whole data set `[V]`; rules 1 and 2 are (position, value) pairs until the end, no special objects expected (one met is read and ignored), with at most as many values as the data set has members; a block that is only the rule octet (RBE with nothing reported) is accepted and carries no value `[S]`.
* **The copy of the special objects inside the octet string is only compared** with the separate variables `[S]`: a different `Transfer_Set_Name`, time stamp or conditions is a note at debug level and no refusal, and the report's time stamp, conditions and acknowledgement come from the separate variables, as for an ordinary report.
* **A block that cannot be decoded** (no octets, an unknown rule octet, a truncated value, a name storage or time stamp cut short, a name storage with a scope octet that is neither 0 nor 1, leftover octets after the last member of rule 0, a position outside the data set, more values than members, a `State` value with bits set below the state, or a report with no `Transfer_Set_Name` or an unreadable special object) is **not applied at all** (no value of it is published), is **counted** in `BlockedReportsUndecodable`, and is logged **once at warning level** with the transfer set, the reason, the rule octet, the length, how many members the data set defines (and how many of them are special objects), the names of the variables seen, and a hex dump of the first 96 octets of the blocked data.
  It is **not answered** (no `Transfer_Report_NACK`, and no `Transfer_Report_ACK` even for a critical transfer set: nothing was understood) and **the association is not touched**: the next report is handled as ever. A blocked report from a transfer set this client did not start is answered like any report from a transfer set that is not ours (`NackUnknownReports`).
* **`BlockData` false but the report arrives blocked** anyway: it is decoded all the same, with one warning per transfer set that the peer sent the blocked form unasked, since discarding values that are clearly all there would be the worse choice. Ordinary reports of a transfer set started with `BlockData` true are handled as ever (a peer without Block 3 ignores the flag).
* **At association** a warning is logged for each transfer set with `BlockData` true if the peer does not advertise Block 3 in its `Supported_Features` (or does not return them); `RequiredRemoteBlocks` with 3 ends the association instead.

**A diagnostic line** (synthetic names; a rule 0 block cut short inside the name storage of `Transfer_Set_Name`):

```
transfer set TS_1: a blocked report could not be decoded and none of its values was applied: TASE.2 blocked data: truncated, the value of member 0 needs 65 octets and 40 are left. Rule byte 0, 41 octets of blocked data, the data set defines 6 members (3 of them special objects). Variables seen: DOMAIN_A/Transfer_Set_Name, DOMAIN_A/Transfer_Set_Time_Stamp, DOMAIN_A/DSConditions_Detected, Block_Data. Blocked data: 00 01 44 4F 4D 41 49 4E 5F 41 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 54 53 5F 50 4F 4F 4C
```

**Statistics** (`GetStatistics`): `BlockedReportsSent` (a port with a server side: reports sent in the blocked form, counted among `ReportsSent`), `BlockedReportsReceived` (a port with a client side: reports received in the blocked form, decodable or not) and `BlockedReportsUndecodable` (of those, the ones that were not applied). They are cumulative over the associations.

#### How to capture a real peer's blocked report to correct this

1. **If you can, ask for more than the first captures showed.** The captures so far are of a real peer as the server, `BlockData` true: `RealQ` points with `RBE` false (rule 0) and with `RBE` true (rule 0 for the integrity reports, rule 1 with one or several changed points per report), and a data set of four special objects (with `Event_Code_Detected`, the value 0) and `StateQ` points. What is still needed, in this order:
   * **(a) a rule 2 report sent by the peer itself** (a data set of more than 256 members that the peer reports with rule 2, if it ever does): this port's rule 2 reports are verified `[V]` (accepted, acknowledged and the values above position 255 matched), what is open is only what a peer sends itself.
   * **(b) a blocked report with discrete points and with state points of other types than `StateQ`** (and with time tag or extended types, if the peer has them), to see their octets; and an **`Event_Code_Detected` other than 0** (a code raised by an operator request or other external event), to see its byte order and sign.
   * **(c) the peer as the client** asking this port's server for blocked reports: `SupportedBlocks` with 3 on this port and a transfer set started by the peer with `BlockData` true (`RBE` false, and `RBE` true), to see whether it accepts this port's rule 0 and rule 1 reports (one real peer does, accepts and acknowledges rule 2 reports too, and applied their values correctly; the server's variable is VMD-specific `Block_Data`, `BlockedDataName` changes the id). A report that needs a position above 255 (rule 2) is sent with rule 2 by default; `BlockedRule2` false sends it in the ordinary form, which the real peer did not acknowledge.
   To set one up: take a **small data set** on the peer with a few points of known types and known values and configure one client transfer set on it with `"BlockData": true` and the `"RBE"` wanted. Do not put 3 in `RequiredRemoteBlocks` (so that the association is not ended if the peer does not advertise Block 3). Use `Conditions` that make the peer report soon (an `Interval` of a few seconds, or `IntegrityTimeOut` with a small `IntegrityCheck`).
2. **Capture the TCP traffic** of the association (Wireshark / tshark, decode as MMS; it is plain TCP unless TLS is on, in which case use `KeyLogFile`, see [Decrypting a capture](#decrypting-a-capture-keylogfile)) while the peer reports that transfer set. The port's own `TraceHex` option logs the same bytes as hex. Keep the Wireshark MMS dissection of the `unconfirmed-PDU` `informationReport` of that transfer set, and the data set definition and the configured types of its points.
3. **Look at**:
   * **the variable names** in the `listOfVariable`: which special objects it lists, the scope and domain of each (the capture: the special objects domain-specific in the domain of the transfer set, then the VMD-specific `Block_Data`), and where the data variable sits;
   * **the first octet of the octet string** (the rule: 0 for a complete report, 1 or 2 for a partial report of an `RBE` true set);
   * **the layout of the members**: the storage form of each special object (the capture: 65, 4 and 1 octets), the width of each point type (compare the table above), the byte order of the numbers, the order of the components of a structure, whether the flags and the state are one octet, the width of the time stamp, milliseconds and COV;
   * **what a position counts** (the special objects count, from 0 `[V]`), and for rule 1 with several points `[V]` and for rule 2 the pairs, their order and the 2 octet big endian position (`[V]` when sent by this port: acknowledged and the values checked in a peer; `[S]` when a peer sends one).
4. Write down the hex of the octet string against the values the points had. The warning line above gives the first 96 octets of any block this port could not decode, with the variable names, so for a peer whose block this port cannot read the log may already be enough.
5. **To change the code**: the id of the variable is `DefaultVariableName` (and the option `BlockedDataName`); the octets are `ValueWidth`, `AppendValue`, `ReadValue`, `SpecialWidth`, `AppendSpecial`, `ReadSpecial`, `ChooseRule`, `Encode` and `Decode` in `Tase2Blocked.cpp`; what a position counts, the order of the points and the rule 2 fallback are `ResponderSession::BuildBlockedValues` (`Tase2Responder.cpp`); the conditions for the ordinary fallback are `ResponderSession::BlockedUnsendable`; what a client accepts is `RecognizeBlockedReport` and `ClientSession::OnBlockedReport`.
   The tests pin the layout byte for byte (`Cases()` and the hand built vector of the real layout in `BlockedTransferTest.cpp`): change the vectors with the code. Then move the items that the capture confirmed from `[S]` to `[V]` in this README (quirks 66 to 70).

## Points, data sets and quality

Point types (`Type`): `Real`, `RealQ`, `RealQTimeTag`, `RealExtended`; `State`, `StateQ`, `StateQTimeTag`, `StateExtended`; `Discrete`, `DiscreteQ`, `DiscreteQTimeTag`, `DiscreteExtended`.
Binaries take the State types; Counters the Discrete types; Analogs the Real or Discrete types. The type must be configured because the wire structure is ambiguous
(`{integer, flags}` is both a `DiscreteQ` and a `StateQTimeTag`, for example). Time tags are whole seconds; the `Extended` types carry milliseconds. A point whose type has no time tag is stamped with the local time on receipt.

Quality is mapped between the TASE.2 data flags and ODC `QualityFlags` (the mapping is lossy; the raw flags octet is kept with the value):

| TASE.2 | ODC |
|--------|-----|
| Validity Valid | ONLINE |
| Validity Held | ONLINE + COMM_LOST |
| Validity Suspect | ONLINE + REFERENCE_ERR |
| Validity NotValid (and state Invalid) | COMM_LOST |
| Current source Entered | + LOCAL_FORCED |
| Current source Estimated | + REMOTE_FORCED |
| State Between | + DISCONTINUITY |
| Normal value (abnormal), time stamp quality | not mapped |

From ODC: not ONLINE = NotValid; COMM_LOST = Held; REFERENCE_ERR = Suspect; otherwise Valid. LOCAL_FORCED = Entered, REMOTE_FORCED = Estimated. RESTART, OVERRANGE, ROLLOVER, DISCONTINUITY and CHATTER_FILTER have no TASE.2 equivalent.
For State points On is `true`, any other state `false`; sending, `true` is On and `false` Off.

## UI commands, status and statistics

Ports whose service role includes the server side (`Server`, `Peer`), and ports that operate devices of the peer (a `Client` or `Peer` with `Operate` controls), register the `ICCPControl` responder with these commands (the first argument is the port name or a regular expression; a command is run only on the matching ports that can do it, and is a success if it was started on at least one):

| Command | Syntax | Description |
|---------|--------|-------------|
| OperatorRequest | `OperatorRequest <port> [<TransferSet>]` | Sends a full report with the OperatorRequest condition from each enabled transfer set that asked for it. (Ports with a server side only.) |
| OtherExternalEvent | `OtherExternalEvent <port> [<EventCode> [<TransferSet>]]` | The same for OtherExternalEvent, with an event code. (Ports with a server side only.) |
| LocalReset | `LocalReset <port> <control> [<tag> [<reason...>]]` | The Local Reset action of a device this port serves (a `Serve` control): the device goes from ARMED to IDLE as if by a local action; with a `<tag>` (`NO_TAG`, `OPEN_AND_CLOSE_INHIBIT` or `CLOSE_ONLY_INHIBIT`, in any case, `-` or `_`) the tag is set too (a taggable device only). The MMS event notification of the action is not sent. See [Device tags and the Local Reset action](#device-tags-and-the-local-reset-action). |
| LocalSetTag | `LocalSetTag <port> <control> <tag> [<reason...>]` | Sets the tag of a taggable device this port serves, locally: no check of the device state or of the owner of the tag. |
| Tags | `Tags <port>` | Lists the tags of the taggable devices this port serves: `Control`, `Scope`, `Domain`, `Tag`, `Reason`, `SetBy` (`Client`, `Local` or empty for NO_TAG), under `Tags.<port>`. |
| GetTag | `GetTag <port> <control>` | Get Tag of a device of the peer that this port operates (an `Operate` control): a Read of its tag object. The tag, the reason and the state (IDLE or ARMED) are logged (info), or why not (warning or error). |
| SetTag | `SetTag <port> <control> <tag> [<reason...>]` | Set Tag of a device of the peer that this port operates: a Write of its tag object. The peer's answer is logged. |

`<control>` is the TASE.2 device name (`Name` of the control). The reason is the rest of the line. A bad control, a bad tag, a control that is not taggable, a reason that is not at most 255 characters of visible ASCII, or a client command with the association down is answered at once with the reason (the result is `Success` only when the action was started; what the peer answered to `GetTag` and `SetTag` comes later, in the log and the statistics).

`GetStatus` reports `Association` (`UP` or `DOWN`), `ServiceRole`, `TCP` (`Initiator` or `Listener`), `TLS` (`Enabled`, and with TLS the `State`, `Protocol`, `Cipher`, `PeerSubject`, `PeerNotAfter`, see [Secure ICCP / TLS](#secure-iccp--tls)) and, for a port with a client side, `Provisioning`: one entry per data set this port defines on the peer, with
`DataSet`, `Domain`, `TransferSet` (once known), `State` (`pending`, `defined`, `defined-partial`, `define-failed`, `transfer-started`, `transfer-failed`), `Members` (as configured), `LastError` (if any) and,
after a refused definition, `Probe` with `Complete`, `Probed`, `MissingMembers`, `PresentMembers`, `PresentButNotReadable`, `Failure` (if the probe could not finish) and `Missing`: **every** missing member
as `{Position (0-based in the data set), Name, Kind, Index}`. A data set defined without the members the peer lacks has `Partial: true` and `MembersDefined`; its `State` is `defined-partial` until a transfer set is started
and `transfer-started` after (the `Partial` flag stays). The entries describe the current association, or the last one while the port is down.
`GetStatistics` reports `Associations`, `ReportsSent` (the server side's reports), `ReportsReceived`, `ValuesPublished` (the client side's) and `UnavailablePoints` (points flagged as not available on the current association).
A port with a server side also reports the critical report counters (`CriticalReportsSent`, `CriticalReportsAcknowledged`, `CriticalReportsOutstanding`, `ReportNacksReceived`, `ReportAckTimeouts`, `ReportAckOverflows`, `DataFailures`), `ReportsDiscardedTLE` and `BlockedReportsSent`, and a port with a client side `ReportAcksSent`, `ReportNacksSent`, `BlockedReportsReceived` and `BlockedReportsUndecodable`: see [Critical reports and the time limit TLE](#critical-reports-and-the-time-limit-tle) and [Blocked transfers (Block 3)](#blocked-transfers-block-3).
For a `Peer` all of them are meaningful, one set per direction. A port with `BinaryControls` / `AnalogControls` also reports `ControlSelectsSent`, `ControlOperatesSent`, `ControlFailures` (commands this port sent to the
peer's devices: selects accepted, operates that succeeded, commands that did not) and `ControlSelectsReceived`, `ControlOperatesReceived`, `ControlRefused` (what the peer did to this port's devices; the selects and operates refused because of a tag are in it). They are cumulative over the associations.
A port that serves a `Taggable` device also reports `TagsSet` (the peer's Set Tag requests accepted), `LocalTagsSet` (tags set by `LocalSetTag` or `LocalReset`), `TagRequestsRefused` (the peer's Get Tag and Set Tag refused), `ControlSelectsRefusedByTag`, `ControlOperatesRefusedByTag` and `LocalResets` (devices taken from ARMED to IDLE by a local reset); and a port with `Operate` controls `TagsRead` (Get Tag answered with a tag), `TagsWritten` (Set Tag accepted by the peer) and `TagRequestFailures`. The tag counters are cumulative like the others (those of the served devices belong to the devices, not to an association). All of them are `[S]`.

**`CurrentState`** (the UI command `DataPorts CurrentState`, `DataPort::GetCurrentState()`) reports the last known value of each point this port **serves**, the values that other ODC ports gave it by events and that a peer reads from it, and the state of the devices it serves. It is the server side only: a port with a server side (`Server`, `Peer`) answers it, and a `Client` returns the default (an empty value), as it did before; the points a `Peer` reads from its peer (`Direction` `Read`) and the devices it operates (`Operate`) are not in it. The shape is that of the DNP3 port (`{ <date and time>: { <lists> } }`, with the same key names where they apply) with the ICCP object name added:

```json
{
	"2026-01-02 03:04:05.678": {
		"Binaries": [
			{"Index": 0, "Name": "POINT_ST_01", "Scope": "ICC", "Domain": "DOMAIN_A", "Value": true, "Quality": "|ONLINE|", "Timestamp": "2026-01-02 03:04:00.123"},
			{"Index": 1, "Name": "POINT_ST_02", "Scope": "ICC", "Domain": "DOMAIN_A", "Quality": "|RESTART|", "Timestamp": "2026-01-02 02:00:00.000"}
		],
		"Analogs": [
			{"Index": 0, "Name": "POINT_MW_01", "Scope": "VCC", "Value": 12.5, "Quality": "|ONLINE|OVERRANGE|", "Timestamp": "2026-01-02 03:04:01.456"}
		],
		"Counters": [
			{"Index": 0, "Name": "POINT_CNT_01", "Scope": "ICC", "Domain": "DOMAIN_A", "Value": 4000000000, "Quality": "|ONLINE|", "Timestamp": "2026-01-02 03:04:02.789"}
		],
		"Controls": [
			{"Index": 0, "Name": "DEV_DIR", "Kind": "Binary", "Scope": "ICC", "Domain": "DOMAIN_A", "Taggable": true, "Tag": "OPEN_AND_CLOSE_INHIBIT", "Reason": "WORK ON FEEDER 7", "SetBy": "Local"},
			{"Index": 0, "Name": "DEV_SP", "Kind": "Analog", "Scope": "VCC", "Taggable": false}
		]
	}
}
```

* The outer key is the time of the call. `Analogs`, `Binaries` and `Counters` list every served point, in the configuration order, as `Index` (the ODC index), `Name` (the TASE.2 object name), `Scope` (`ICC` or `VCC`) and, for an ICC point, `Domain`; then the value as the last ODC event for the point gave it: `Value` (a number for an analog, true or false for a binary, an unsigned integer for a counter; **not** scaled by `ScaleFactor` and `Offset`, which apply on the wire only), `Quality` (the ODC quality, as `ToString` of the quality flags writes it in the other ports: `|ONLINE|OVERRANGE|`) and `Timestamp` (the time of that event, in the local time zone like the other ports).
* A point that has not been given a value since the port was built has no `Value`, the quality `|RESTART|` and the time the port was built: as an empty point of the DNP3 port does.
* `SourcePort` of the DNP3 port is not given: the port keeps the value, quality and time of a point, not the name of the port that sent it.
* `Controls` lists the devices this port serves (`BinaryControls` and `AnalogControls` with `Direction` `Serve`): `Index`, `Name`, `Kind` (`Binary` or `Analog`), `Scope`, `Domain` (ICC), `Taggable` and, for a taggable device, the tag the device has now: `Tag` (`NO_TAG`, `OPEN_AND_CLOSE_INHIBIT` or `CLOSE_ONLY_INHIBIT`), `Reason` and `SetBy` (`Client`, `Local`, or empty without a tag), the same as the `Tags` command gives. Whether a select is outstanding (the device's state ARMED) belongs to the association and is not reported: the model holds it on the port's strand only.
* Thread safety: it may be called from any thread (it is the UI's). It reads the configuration (read-only once loaded), takes a copy of the point states under the lock of the model that serves them (the updates from ODC events take the same lock for one point at a time, so each entry is one update: its value, quality and time belong together; different points are not read at one instant), and reads the tags under their own lock. It does not use the association or wait for the port's strand, and it never throws (it returns the default value if something went wrong). A port that is not built yet, not enabled or disabled again answers with the last values it has, or none.

## Device control (TASE.2 Block 5)

**Basis.** The device model follows IEC 60870-6-503:2014 (clauses 5.3.12 and 7.2.6) and IEC 60870-6-802:2014 (clauses 5.2.3, 6.2.2 and 7.1.2), and was checked against a capture of how a real peer operates devices. In this section
`[V]` marks what was seen on the wire, `[T]` what is taken from the text of the standard (the clause is cited; not yet seen in real traffic), `[S]` an assumption or a deliberate deviation. The two flows of a real peer in the capture are both
supported: *automatic* commanding (a Read of the select object, then a Write of the bare value) and *direct* commanding (the Write alone). It works between two of these ports; interoperability with other implementations beyond that one is not yet tested.

### What is implemented

* Control devices of two kinds of ODC control: **command devices** (ODC `ControlRelayOutputBlock`; Type `Command` or `DiscreteCommand`) and **setpoint devices** (ODC `AnalogOutputInt16`, `Int32`, `Float32`,
  `Double64`; Type `RealSetpoint` or `DiscreteSetpoint`).
* **Select before operate** (the select is a Read of the select object `[V]` `[T]`) and **direct operate** (the Write alone `[V]` `[T]`), with a configured CheckBackID, a select timeout and one operation at a time per device.
* **Both directions**: the *client side* operates the peer's devices from ODC control events and returns an ODC `CommandStatus`; the *server side* serves devices, accepts the peer's select and operate, publishes the ODC
  control event downstream and completes the peer's Write from the downstream command status. A `Peer` does both on the one association.
* Discovery: the devices and their select objects appear in `GetNameList`, and `GetVariableAccessAttributes` returns their types `[T]`.
* The Block 5 bit of `Supported_Features` (bit 4, the fifth): advertised automatically by a port that serves devices, required automatically of the peer by a port that operates them (see below).
* **Device tags** (optional in the standard, Table A.12 of IEC 60870-6-702: Get Tag, Set Tag and Local Reset are optional): a served device can have the Tag attribute (`Taggable`), which gives it the variable `<Device>_TAG`; Get Tag and Set Tag, enforcement of the tag on select and operate, and on the client side
  `GetTag` / `SetTag` of the peer's devices. The **Local Reset action** has its state effect (ARMED to IDLE, optionally with a new tag) as a UI command. See [Device tags and the Local Reset action](#device-tags-and-the-local-reset-action). A real peer has never been seen using tags or the Local Reset: everything about them is `[T]` or `[S]`, never `[V]`.

Not implemented (and why): the MMS **Event Condition / Event Notification mapping** of the device actions Timeout, Success, Failure and Local Reset (`<device>_time`, `_success`, `_fail`, `_reset`): the actions belong to MMS event management, which is Block 7 of TASE.2 and not supported, and the event enrollment and condition models are informative only
in the standard, so they are not built (the state effect of the Local Reset is, the notification is not: quirk 63); the readable **`SBO`** structure variable (State, Timeout) and the **`ControlConfig`** variable (ControlPointType, SetPointType, DeviceClass, TagClass) of IEC 60870-6-802 (clauses 5.2.3, 6.2.2 and 7.1.2), because the standard gives no MMS variable names for them (the variable `<Device>_SBO` is the CheckBackID, as a real peer uses it: quirk 39);
**cancelling** a select (a select just lapses), the **ICCP command / setpoint structures with a time stamp** and any **Block 5 reporting** (a transfer set with device objects: the tag object can be a member of a data set, but a change of a tag is never reported). There are no per-client access control specifications in this port (any associated client may Get Tag and Set Tag, as it may select and operate): the standard's object-access-denied for a missing permission does not arise.
`DiscreteCommand` differs from `Command` only in the name: both carry an integer that is one of two configured values.

### The objects and the flows

A control device `D` of an ICC domain (or the VCC) is two MMS named variables, in the device's domain (a third, `D_TAG`, if it is taggable: see [Device tags and the Local Reset action](#device-tags-and-the-local-reset-action)):

| Object | MMS type | Read | Write |
|--------|----------|------|-------|
| `D` | An integer of width 16 for `Command`, `DiscreteCommand` and `DiscreteSetpoint`; a floating point of 32 bits with an exponent width of 8 for `RealSetpoint` (`GetVariableAccessAttributes` answers this) `[T]` (802 clauses 6.2.2, 7.1.2) | **not defined: refused** with `object-access-denied` (the ControlPoint is write only) `[T]` (802 clause 5.2.3) | **Operate**: the data is the **bare value** and nothing else: an integer that fits the width 16 (read as integer or unsigned: -32768 to 65535 is accepted, see below), or a 32 bit floating point (a 64 bit one is also accepted on read) `[V]` `[T]` (503 clause 7.2.6.3.2) |
| `D_SBO` | An integer of width 16 `[T]` (802 clause 6.2.2) | **Select**: the device goes from idle to armed for `SelectTimeoutms` and the answer is the device's configured CheckBackID `[V]` `[T]` (503 clause 7.2.6.2) | **not defined: refused** with `object-access-denied`. There is no Write of the select object `[V]` `[T]` |

The select is the **Read** of `D_SBO`, not a Write: the server arms the device and returns the CheckBackID that the bilateral table defines for the device, which is a **fixed, configured value** (option `CheckBackID`, default 1), the same on
every select and every association. The client only uses it to check that it selected the right object; what it means to the client is a local matter `[T]` (503 clause 7.2.6.2.2). The operate Write carries no CheckBackID and no structure.
The wire form of an operate of a real setpoint is, for example, 100.0 as the data `87 05 08 42 C8 00 00` (FloatingPoint, exponent width 8, IEEE 754 single) `[V]`.

Select before operate (what the client side does for a device with `SelectRequired` true, the default; this is the *automatic* mode of a real peer):

1. Read `D_SBO` -> the CheckBackID. The device is armed for `SelectTimeoutms` (default 10 s). The client side compares the CheckBackID with its configured one if it has one, and does not go on if they differ.
2. Write `D` <- the bare value: the operate. The select is used up.

Direct operate (`SelectRequired` false on the operating side, the *direct* mode of a real peer): the Write of `D` alone `[V]`.

The server side keeps, per device and association: idle, armed (the select, with its expiry) or operating (a downstream command in flight, with its own timeout, `Timeoutms`, default 4 s). The checks of an operate, in this order `[T]` (503 clause 7.2.6.3.3):
a device that is operating is busy; a device with `SelectRequired` true that is not armed **by this association** is refused; then the data is checked against the device's type, an integer that does not fit the width 16 is refused (below), a command value that is neither ON nor OFF is refused, and a setpoint is scaled
and range checked; then, for a taggable device, the tag is checked against the command (below); then the value goes downstream and the Write is answered when downstream answers. An operate refused for its data leaves an armed device armed until its timeout. A select of a device that is already armed or operating (or, if it is taggable, tagged `OPEN_AND_CLOSE_INHIBIT`) is refused. The select lapses
after its timeout, and the whole state goes with the association (a new association starts with every device idle: the select of the previous one is gone, and an operate there is refused; the tag of a taggable device is not part of that state, see below). Only one association is up at a time, so a device is never armed by two of them.

**The width of the values `[T]`.** IEC 60870-6-802 clause 6.2.2 defines `Control_Command`, `Control_Setpoint_Discrete` and `SBO_CheckBackName` as integers of width 16 and `Control_Setpoint_Real` as a 32 bit floating point, and the type this port advertises is exactly that. The integer values are held to it
(before this, any 32 or 64 bit integer was accepted and written):

* Server side: the value of an operate of a command or discrete setpoint device must be in **-32768 to 65535**: the width 16 as a signed value, or as an unsigned one, because a client may encode the integer either way `[S]` (the standard does not say which a client uses; both are decoded). Any other integer
  (including an unsigned one too big for 63 bits) is refused with the data access error `object-attribute-inconsistent` [8] (`OUT_OF_RANGE` on our client side), the device stays as it was, and a warning names the value and the range. The check comes before the ON / OFF comparison (so 70000 is out of range, not "neither ON nor OFF").
  A setpoint that passes it is then scaled, and published downstream as the device's `AnalogOutputType`: `Int16` refuses (with the downstream status `OUT_OF_RANGE`, so the same data access error [8]) a scaled value that does not fit 16 bits, and `Int32` (the default of a `DiscreteSetpoint`) takes it. Only one of the two checks ever refuses a given operate, so it is
  reported once. A real setpoint (`RealSetpoint`) is a float and has no integer width.
* Client side (the peer's devices that we operate): a `DiscreteSetpoint` value, after the scaling and the rounding to the nearest integer, and a command value must be in **-32768 to 32767**, because this port writes a signed integer. Anything else is refused **before** anything is sent, with `CommandStatus` `OUT_OF_RANGE` and an error line that
  names the value and the range. `CommandValueOn` and `CommandValueOff` are held to the same range when the configuration is loaded.
* `CheckBackID` is not changed: its range is -32768 to 65535 (the width 16 as signed or unsigned), deliberately.

**Deliberate deviation `[S]`:** the standard says a Select of a non-SBO device fails (503 clause 5.3.12.3). A device with `SelectRequired` false is more lenient here: an operate is always accepted (armed or not), the `D_SBO` object still exists, and a Read of it still arms the
device (so that a client that works in select mode also works with it); an armed device is consumed by the operate. This avoids refusing a client that selects first, at the price of a device that the standard would call non-SBO answering a select.

### Device tags and the Local Reset action

These are the optional parts of Block 5 that the standard defines precisely (IEC 60870-6-503:2014 clauses 5.3.12.2 to 5.3.12.8, 7.2.6.2.3, 7.2.6.3.3, 7.2.6.8 and 7.2.6.9; IEC 60870-6-802:2014 clauses 5.2.3, 6.2.2 and 7.1.2; Table A.12 of IEC 60870-6-702 lists Get Tag, Set Tag and Local Reset as optional).
**A real peer has never been seen using tags or the Local Reset, so everything in this subsection is `[T]` (taken from the text) or `[S]` (a choice), never `[V]`**, and only our own client and server have been run against each other. A device without the new `Taggable` option behaves exactly as before.

**The Tag attribute and the variable `D_TAG` `[T]`.** A taggable device has the Tag attribute: `NO_TAG`, `OPEN_AND_CLOSE_INHIBIT` or `CLOSE_ONLY_INHIBIT`, and a reason. It is the MMS named variable `<Device>_TAG` in the same scope and domain as the device (a domain-specific variable for an ICC device, a VMD-specific one for a VCC device), of the type
`Tag_Value`, a structure of `Flags` (a bit string with the named bits `tag_hi` [0], `tag_lo` [1], `tag_state` [2]) and `Reason` (a visible string of at most 255 characters). The tag is in bits 0 and 1 (`NO_TAG` 0, `OPEN_AND_CLOSE_INHIBIT` 1, `CLOSE_ONLY_INHIBIT` 2), the state in bit 2 (`IDLE` 0, `ARMED` 1), the reason in `Reason`. The text does not say whether `tag_hi` is the high or the low bit of the
two bit tag value: this port takes `tag_hi` as the **most significant** bit, so tag value 1 is bits `01` and 2 is bits `10` `[S]`. On the wire, `OPEN_AND_CLOSE_INHIBIT` with the reason `AB` and an idle device is `A2 08 84 02 05 40 8A 02 41 42` (the flags a bit string of 3 bits: 5 unused bits). On receipt a bit string shorter or longer than 3 bits is accepted (missing bits are 0, bits beyond the third are ignored `[S]`); the tag value 3 (both bits set), a reason longer
than 255 characters or any other shape is refused with `type-inconsistent` `[S]`. `GetVariableAccessAttributes` of `D_TAG` returns the structure type (components `Flags` and `Reason`, the flags a bit string of 3 bits, the reason a visible string with a size of -255). `D_TAG` is listed by `GetNameList`, and is a valid member of a data set (the standard says Get Tag can also be done by including the object in a data set, clause 5.3.12.6), which a client defines with `DefineNamedVariableList` and reads
like any other; it appears in the integrity and other full reports of a transfer set that has it, but **a change of a tag is never reported** (by Set Tag or locally: there is no change reporting for tags, only a point's change is tracked) `[S]`. A data set configured in `DataSets` has points only.

**Get Tag `[T]`** is a Read of `D_TAG`: the current tag and reason, and the `tag_state` bit says whether the device is ARMED now. A device that is not taggable (or does not exist as a taggable device) answers `object-non-existent` [10]. The standard's `object-access-denied` for a missing Get Tag permission does not arise: there are no per-client access control specifications in this port, so every associated client may Get Tag and Set Tag.

**Set Tag `[T]`** is a Write of `D_TAG` with a `Tag_Value`: the tag and the reason are set (the state bit written is ignored) and the answer is a positive response. The refusals, in the order of the standard (clause 7.2.6.8.3):

| Cause | Data access error |
|-------|-------------------|
| the device is not taggable (no Tag attribute) | `object-non-existent` [10] `[T]` |
| the data is not a `Tag_Value` | `type-inconsistent` [7] `[S]` (checked after the existence) |
| the device is ARMED, or busy with a select or an operate | `temporarily-unavailable` [2] `[T]` |
| the device is a VCC device, its tag is not `NO_TAG` and the client is not the one that set it | `temporarily-unavailable` [2] `[T]` |

**The owner of a VCC device's tag `[T]` / `[S]`.** The standard says that for a VCC-specific device with a tag other than `NO_TAG` only the client that set the tag may change it. This port serves one association at a time and has no other notion of a client than the association request, so "the same client" is the **identity** taken from the association request: the calling AP title and AE qualifier (for example `1.3.9999.1.2/2`), or the empty identity when the request has none
(and for an association this port initiated itself, where it learns nothing: the empty identity is one client too). The identity of the client that set the tag is kept with the tag, which belongs to the device, so a client that reconnects with the same identity is still the owner. A tag set locally (the configuration's `InitialTag`, `LocalSetTag` or `LocalReset`) has **no** client owner: no client can change it on a VCC device, only the local operator (`LocalSetTag`, `LocalReset`) can. An ICC device has no owner rule: any client can change its tag. `NO_TAG` has no owner. The rule is not enforced between two clients that give the same identity.

**Enforcement `[T]` / `[S]`.** The tag is checked at two places: a **select** (the Read of `D_SBO`) is refused with `temporarily-unavailable` if the tag is `OPEN_AND_CLOSE_INHIBIT` (clauses 5.3.12.3 and 7.2.6.2.3), after the other checks of the select (already armed, busy, no downstream) and without arming the device; an **operate** is refused with `temporarily-unavailable` if the tag conflicts with the command (clauses 5.3.12.4 and 7.2.6.3.3): `NO_TAG` never prevents, `OPEN_AND_CLOSE_INHIBIT` always
does, `CLOSE_ONLY_INHIBIT` prevents only a Close or Raise command. The standard does not say how those map onto this port's command values `[S]`: for a command device **ON** (the configured `CommandValueOn`) is the Close / Raise and **OFF** the Open / Lower, and a **setpoint** device is never prevented by `CLOSE_ONLY_INHIBIT` (a setpoint has no direction), but is by `OPEN_AND_CLOSE_INHIBIT`. The operate check comes after the device state and the checks of the data (the command has to be known), and
a refused operate leaves an armed device armed until its timeout (as an operate refused for its data does) `[S]`. A device that is not taggable is never refused for a tag. The refusals are logged (warning, with the tag and the reason) and counted (`ControlSelectsRefusedByTag`, `ControlOperatesRefusedByTag`, and in `ControlRefused`); the client side maps `temporarily-unavailable` to `ALREADY_ACTIVE` as for any busy device.

**The tag belongs to the device `[S]`.** It is held with the device, not with the association: it is kept when the association ends and is there, with its reason and owner, on the next one. It is lost when the port restarts, unless `InitialTag` / `InitialTagReason` set it again.

**The Local Reset action `[T]` / `[S]`.** The standard's Local Reset action is how the server signals that a device moved from ARMED to IDLE by a local action (emergency pre-emption of device control, clause 5.3.12.8), and the tag may also be set to a different value as a result. The signal on the wire is an MMS event notification of the event condition `<device>_reset` (clause 7.2.6.5), which is Block 7 and not supported (see above), so **only the state effect is implemented, the event notification is not sent**:
the `LocalReset` UI command takes the device from ARMED to IDLE (a later operate by the client that had selected it is refused as not selected with `temporarily-unavailable`), optionally sets a tag and reason, logs it (info) and counts it (`LocalResets` counts the ARMED to IDLE transitions); a device that is not ARMED (idle, operating, or whose select has lapsed) is left as it is, and a device that is operating is not interrupted. The client is not told: it finds out when its operate is refused.
A reset with no association (every device is idle then) only sets the tag.

**The client side.** `GetTag` and `SetTag` (UI commands) send a Read and a Write of `<Device>_TAG` for a configured `Operate` control, of either scope (a VCC control addresses the VMD-specific tag object), through the request and response plumbing of select and operate: the request timeout applies, and the association ending completes it. The outcome is logged, and the data access errors are given readable explanations: `object-non-existent`: "the peer has no such device, or the device has no Tag attribute";
`object-access-denied`: "the peer does not permit Get Tag (Set Tag) to this client"; `temporarily-unavailable` (Set Tag): "the device is ARMED or busy, or its tag was set by another client"; `type-inconsistent`: "the peer did not accept the Tag_Value". A peer's answer that is not a `Tag_Value`, has the wrong number of results, or is a service error, reject or timeout is reported too. A reason that is not at most 255 characters of visible ASCII is not sent.
The operate and select of a peer's tagged device work as before: the new refusal is `temporarily-unavailable`, which is already mapped (`ALREADY_ACTIVE`).

Example of the logs:

```
[info]    ICCPServer: control DEV_0001_CMD: tag set by the peer: OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12)
[warning] ICCPServer: control DEV_0001_CMD: select refused: tagged: the device is tagged OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12) (data access error temporarily-unavailable [2])
[warning] ICCPServer: control DEV_0002_CMD: operate refused: tagged: the device is tagged CLOSE_ONLY_INHIBIT, which prevents ON (Close) (data access error temporarily-unavailable [2])
[warning] ICCPServer: control DEV_0003_CMD: Set Tag refused: busy: the device is ARMED (selected) (data access error temporarily-unavailable [2])
[info]    ICCPServer: control DEV_0001_CMD: local reset: the device went from ARMED to IDLE (the MMS event notification of the action is not sent)
[info]    ICCPClient: control DEV_0001_CMD: Get Tag: the peer's tag is OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12), the device is IDLE
[warning] ICCPClient: control DEV_0009_CMD: Set Tag failed: data access error object-non-existent [10]: the peer has no such device, or the device has no Tag attribute
```

### Configuration

`BinaryControls` and `AnalogControls` are arrays of control objects, valid with every `ServiceRole` and both port types, with the validation style of the points (errors name the rule and the fix).

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Index | number | required | The ODC event index. May be used once per kind (`BinaryControls`, `AnalogControls`) per direction. |
| Name | string | required | The TASE.2 device name (at most 251 characters: the select object is the name plus `_SBO`, the tag object of a taggable device the name plus `_TAG`). Unique per direction; must not equal another device's select object name or a point's name on the same side, and for a taggable device its tag object name must not be another device's or a point's name. |
| Direction | string | by `ServiceRole`: Server = "Serve", Client = "Operate"; **required** for Peer | `"Serve"`: the device is ours, the peer operates it, and each operate is published to ODC as a control event with this Index. `"Operate"`: the device is the peer's, and we operate it from ODC control events with this Index. ("Read" is not a control direction.) |
| Scope, Domain | string | "ICC"; `LocalDomain` for Serve, `RemoteDomain` for Operate | As for points. |
| Type | string | Command (BinaryControls), RealSetpoint (AnalogControls) | `Command` or `DiscreteCommand` in `BinaryControls`; `RealSetpoint` or `DiscreteSetpoint` in `AnalogControls`. |
| SelectRequired | boolean | true | Serve: the device is an SBO device: an operate needs the device to be armed by a select (a Read of `D_SBO`) first, else it is refused with `temporarily-unavailable`. With `false` an operate is accepted without a select, and the select object still arms the device (a deliberate deviation, see above). Operate: do the select (the Read of `D_SBO`) before the operate; with `false`, the operate Write alone. |
| SelectTimeoutms | number | 10000 | Serve: how long a select (the armed state) lasts. Operate: not used. (`SelectTimeoutMs` is accepted too.) |
| CheckBackID | integer, -32768 to 65535 | Serve: 1; Operate: none | Serve: the CheckBackID that a select returns for this device, the value the bilateral table defines for it (a fixed value, the same on every select; no range is imposed within the width 16 of the type, so values above 32767 are fine). Operate: the CheckBackID the peer must return for the select; if it returns another one the operate is not sent and the command ends with `FORMAT_ERROR` and a log line that names both values (the device stays armed on the peer until its select timeout). When absent, any CheckBackID is accepted. |
| Timeoutms | number | Serve: 4000; Operate: 15000 | Serve: how long to wait for downstream to answer an operate (then the peer's Write is answered with a hardware fault). Operate: the limit of the whole select (Read) + operate (Write) sequence (the MMS request timeout, `RequestTimeoutms`, applies to each request as well). (`TimeoutMs` is accepted too.) |
| CommandValueOn, CommandValueOff | integer, -32768 to 32767 | 1, 0 | Command types: the integers that mean ON and OFF on the wire. Must differ. A command is an integer of width 16 and this port writes it signed, so a value outside -32768 to 32767 is a configuration error `[T]` (802 clause 6.2.2). A served device accepts an operate in -32768 to 65535 (see [the width of the values](#the-objects-and-the-flows)). |
| ControlCodeOn, ControlCodeOff | string | LATCH_ON, LATCH_OFF | Serve: the ODC control code published for ON and for OFF. |
| AnalogOutputType | string | Double64 (Int32 for a DiscreteSetpoint) | Serve setpoints: the ODC event published: `Int16`, `Int32`, `Float32` or `Double64`. |
| ScaleFactor, Offset | number | 1.0, 0.0 | Setpoints: ODC value = TASE.2 value * ScaleFactor + Offset. Must not be 0. |
| Taggable | boolean | false | Serve: the device has the Tag attribute: the variable `D_TAG` exists (Get Tag, Set Tag), the tag is enforced on select and operate, and `D_TAG` is listed and can be a data set member. Without it the device behaves exactly as before and `D_TAG` answers `object-non-existent`. An error with `Operate` (a peer's device needs no setting: the `GetTag` / `SetTag` commands try its tag object). `[S]` |
| InitialTag | string | NO_TAG | Serve, taggable: the tag the device has when the port starts: `NO_TAG`, `OPEN_AND_CLOSE_INHIBIT` or `CLOSE_ONLY_INHIBIT` (in any case, with `-` or `_`). It counts as set locally: on a VCC device only the local operator can change it. An error without `Taggable`. |
| InitialTagReason | string | empty | Serve, taggable: its reason: at most 255 characters of visible ASCII (a TASE.2 VisibleString). An error without `Taggable`. |

Block 5 and `Supported_Features`: a port with `Serve` controls advertises block 5 (it adds 5 to `SupportedBlocks` unless that is given, and then it must contain 5); a port with `Operate` controls requires the peer to
advertise it (it adds 5 to `RequiredRemoteBlocks` unless that is given, and then it must contain 5). If the peer does not, the client side logs `the peer does not support TASE.2 block 5` and drops the association, as for blocks 1 and 2.
`LocalDomain` is required for ICC devices that are served and `RemoteDomain` for ICC devices that are operated.

### ODC events and TASE.2

Operating the peer's devices (client side, `Event()`): the event index selects the control of that kind with Direction `Operate`; no such control gives `NOT_SUPPORTED`.

| ODC event | TASE.2 |
|-----------|--------|
| `ControlRelayOutputBlock` with `LATCH_ON`, `PULSE_ON` or `CLOSE_PULSE_ON` | Command value `CommandValueOn` |
| `ControlRelayOutputBlock` with `LATCH_OFF`, `PULSE_OFF` or `TRIP_PULSE_ON` | Command value `CommandValueOff` |
| `ControlRelayOutputBlock` with `NUL` or `UNDEFINED` | not sent: `NOT_SUPPORTED`. (Count, on time and off time are ignored: a TASE.2 command is a value.) |
| `AnalogOutputInt16`, `Int32`, `Float32`, `Double64` (any of them for any setpoint) | Setpoint value (ODC - Offset) / ScaleFactor: a 32 bit float for `RealSetpoint`, the nearest integer for `DiscreteSetpoint`, which must be in -32768 to 32767 (an integer of width 16, written signed). Values that do not fit give `OUT_OF_RANGE`, a NaN `FORMAT_ERROR`, before anything is sent. |

Serving devices (server side): a validated operate is published with `PublishEvent` (source = the port name) and the Write is completed with the status the downstream ports answer. A `ControlRelayOutputBlock`
event with the `ControlCodeOn` / `ControlCodeOff` code (count 1, default times); an `AnalogOutput` event of `AnalogOutputType` with the value TASE.2 value * ScaleFactor + Offset (`OUT_OF_RANGE` if it does not fit `Int16` / `Int32`).
If nothing is subscribed the event is never answered: the operate ends after `Timeoutms` with a hardware fault.

### Results: the data access error of the Read (select) or the Write (operate) and the ODC `CommandStatus`

The errors are the positive responses with an access result of failure that IEC 60870-6-503 clauses 7.2.6.2.3 (select) and 7.2.6.3.3 (operate) define `[T]`. Server side: what is refused or fails, and the data access error the peer's Read or Write gets (each refusal is logged by name):

| Cause | Data access error | Client side `CommandStatus` |
|-------|-------------------|-----------------------------|
| select: the device is already armed, or an operation is in flight (busy), or the device is tagged `OPEN_AND_CLOSE_INHIBIT` | temporarily-unavailable [2] `[T]` | `ALREADY_ACTIVE` |
| select: the device cannot be selected (nothing downstream to carry an operate out) | hardware-fault [1] `[T]` | `HARDWARE_ERROR` |
| operate of a device with `SelectRequired` true that is not armed by this association (never selected, lapsed, used up, selected on an earlier association); or downstream `NO_SELECT` | temporarily-unavailable [2] `[T]` | `ALREADY_ACTIVE` |
| operate of a device that is busy (operating); downstream `ALREADY_ACTIVE`, `TOO_MANY_OPS`, `PROCESSING_LIMITED`; a tag that prevents the command (see [Device tags](#device-tags-and-the-local-reset-action)) | temporarily-unavailable [2] `[T]` | `ALREADY_ACTIVE` |
| operation failed in the device or downstream: `HARDWARE_ERROR`, `DOWNSTREAM_FAIL`, `TIMEOUT`, `CANCELLED`, `UNDEFINED`, `ALREADY_COMPLETE`, or downstream did not answer in `Timeoutms` | hardware-fault [1] `[T]` | `HARDWARE_ERROR` |
| refused by downstream: `BLOCKED`, `BLOCKED_OTHER_MASTER`, `LOCAL`, `DOWNSTREAM_LOCAL`, `NOT_AUTHORIZED`, `AUTOMATION_INHIBIT`, `NON_PARTICIPATING` | object-access-denied [3] `[T]` | `BLOCKED` |
| a Read of the device `D`, or a Write of the select object `D_SBO`: not defined | object-access-denied [3] `[S]` | `BLOCKED` |
| a device that is not served: a Read of its select object | object-non-existent [10] `[T]` | `NOT_SUPPORTED` |
| a device that is not served: a Write | refused like any unknown object: object-access-denied [3] `[S]` | `BLOCKED` |
| a Read or Write of the tag object `D_TAG` of a served device that is not taggable (Get Tag, Set Tag) | object-non-existent [10] `[T]` (for the Write this is the one change of the wire behaviour of an existing configuration; no real peer's flow writes a tag object) | `NOT_SUPPORTED` |
| Set Tag: the data is not a `Tag_Value`; the device is ARMED or busy; a VCC device tagged by another client | type-inconsistent [7] `[S]`; temporarily-unavailable [2] `[T]`; temporarily-unavailable [2] `[T]` | (not an ODC command: see the client side's `GetTag` / `SetTag`) |
| data that is not the bare value of the device's kind (a structure, a string, a float for an integer device, an integer for a real setpoint, a value that is not finite); downstream `FORMAT_ERROR` | type-inconsistent [7] `[S]` | `FORMAT_ERROR` |
| command value that is neither ON nor OFF; no downstream handler; downstream `NOT_SUPPORTED` | type-unsupported [6] `[S]` | `NOT_SUPPORTED` |
| setpoint out of range (not finite); an integer that does not fit the width 16 (outside -32768 to 65535); downstream `OUT_OF_RANGE` (for example a scaled value that does not fit `AnalogOutputType` `Int16`) | object-attribute-inconsistent [8] `[S]` (the width rule `[T]`, 802 clause 6.2.2) | `OUT_OF_RANGE` |

Client side, other outcomes: a CheckBackID returned by the select that is not the configured `CheckBackID` gives `FORMAT_ERROR` and nothing is written; a service error from the peer of class definition or access, or a Reject, gives `NOT_SUPPORTED`; any other service error `HARDWARE_ERROR`; no answer in time `TIMEOUT`;
a malformed or wrong kind of answer `FORMAT_ERROR`; the association not up or ending, or the port being disabled, `UNDEFINED`; a second command for a device with one in flight `ALREADY_ACTIVE`. The data access errors object-undefined, invalid-address,
object-access-unsupported and object-non-existent also give `NOT_SUPPORTED`, and a peer's object-invalidated gives `NO_SELECT` and its object-value-invalid `FORMAT_ERROR`. A command that cannot be started (no such control, not ready, a value that does not fit)
is answered at once.

### Logs and troubleshooting

Each select and operate is an info line on both sides, each failure an error (a refusal by the peer's rules a warning) with the data access error by name:

```
[info]    ICCPServer: control DEVICE_0001_CMD selected (CheckBackID 1, for 10000 ms)
[info]    ICCPServer: control DEVICE_0001_CMD operated by the peer: ON
[info]    ICCPClient: control DEVICE_0001_CMD operated on the peer: ON (select before operate)
[warning] ICCPServer: control DEVICE_0001_CMD: operate refused: not selected: the device is not selected (armed) by this association (data access error temporarily-unavailable [2])
[warning] ICCPServer: control DEVICE_0001_CMD: select refused: busy: already armed (data access error temporarily-unavailable [2])
[warning] ICCPServer: control DEVICE_0001_SP: operate refused: wrong type of value: got a visible string, expected a bare value (a floating point) (data access error type-inconsistent [7])
[error]   ICCPClient: control DEVICE_0001_CMD (ON) failed: the select returned CheckBackID 5 but CheckBackID 1 was expected: the wrong device or object may have been selected, so the operate was not sent (FORMAT_ERROR)
[error]   ICCPClient: control DEVICE_0001_CMD (ON) failed: operate: data access error temporarily-unavailable [2] (ALREADY_ACTIVE)
[error]   ICCPServer: control DEVICE_0001_SP: downstream answered the operate with NOT_SUPPORTED: value not supported (data access error type-unsupported [6])
```

A command answered `ALREADY_ACTIVE` with `SelectRequired` false on the operating side, and `temporarily-unavailable` in the log, usually means the peer's device requires a select: set `SelectRequired` true (it also means the device is busy or armed by an earlier
command, which lapses after the peer's select timeout). `FORMAT_ERROR` with `the select returned CheckBackID ... but CheckBackID ... was expected` means the peer's CheckBackID for the device is not the one configured: correct `CheckBackID`, or remove it to accept any.
`BLOCKED` is a downstream refusal or a device the peer does not serve (check `Name` and `Domain` against the peer's: `GetNameList` of the peer's domain lists its devices and their `_SBO` objects). `NOT_SUPPORTED` with a service error or reject in the log means the peer does not know
these objects: see the quirks table.

**The port does not connect, or is refused at once**

* `Failed to parse configuration from '...' : Line N, Column M Syntax error` followed by `configuration error: a configuration file could not be read ...` and `not enabled: the configuration is invalid`: the file is not valid JSON (comments are allowed). Two easy ways to get there when commenting out the elements of an array such as `TransferSets`: a trailing comma left after the last remaining element, and an array whose only content is comments (`[ // comment ]`: the JSON parser does not accept a comment directly before the closing bracket of an array). Leave the array empty (`[ ]`) or keep one element. The port stays disabled until the file is fixed; it no longer runs on default settings.
* `association ended (refused): transport disconnect request from the peer (address unknown) in answer to our connection request (calling TSAP ..., called TSAP ...)`: the peer does not accept the transport selectors we sent (`none` means the configuration has no `LocalTSelector`/`RemoteTSelector`). Set the selectors the way the peer expects, and the session and presentation selectors with them. Refused, timed-out and aborted associations are logged as warnings.

## Secure ICCP / TLS

Secure ICCP (IEC 62351-3) protects the TCP connection with TLS. It is available when OpenDataCon is built with OpenSSL (`-DODC_ASIO_SSL=ON`, see [Building](#building)).
Without it the port builds and works as before over plain TCP, and a configuration that enables TLS is rejected with
`"TLS": this build has no TLS support: rebuild with ODC_ASIO_SSL=ON`. Plain TCP stays the default: TLS is enabled per port.
IEC 62351-4 (the HMAC based authentication in the ACSE association request) is **not implemented**; what a real-world peer uses instead, certificate based authentication in the association, is [MACE](#secure-iccp-mace), which works with or without TLS.

**The TLS role follows the TCP direction**, like the association initiator does, and is independent of the `ServiceRole`: the TCP initiator (`ICCPClient`) is the TLS client, the TCP listener (`ICCPServer`) is the TLS server.
So a `Peer` or a `Server` role on an `ICCPClient` port is a TLS client as well. Both ends must enable TLS, with certificates that the other trusts; a plain port cannot talk to a TLS port.

How it fits in: the port's TCP connection is an `odc::TCPSessionManager` (`opendatacon/TCPSessionManager.h`); with TLS enabled it runs a TLS filter (`opendatacon/TLSFilter.h`, on OpenSSL memory BIOs, no I/O and no timers of its own, like `stack::Connection`) on each TCP connection and sits between the socket and the TPKT/COTP layer.
The filter and the manager are generic components of the ODC library (namespace `odc::tls`: `TLSFilter.h`, `TLSConf.h`; and `TCPSessionManager.h`), not specific to this port, and other ports can use them; the port refers to the filter's namespace as `tls::` (`namespace tls = odc::tls`, in `IccpTlsDefaults.h`).
What the manager does, once for every port that uses it: it creates, starts and destroys the filter for each TCP connection, feeds it the bytes of that connection, writes its records to that connection and to no other, calls its `Tick()`, reloads a changed certificate file for the next connection (failing closed if the new files do not load), sends `close_notify` before it closes a connection, and ends just that connection when the session fails or the peer closes it.
What stays in the port is what is ICCP specific: what it makes of the session's life (`ICCPPortBase::OnTlsEstablished`, `OnTlsDown`, `OnTlsSessionEnded`, `SetTlsStatus`: the status text `Handshaking`, `Established`, `Failed`, `Closed`, `Down`, the failure count and the back-off of the reconnects after failures, the log lines, starting the association when the handshake has completed), the connection epochs, the default port (3782), the default cipher lists, the `Legacy12` preset and the TLS session id context `opendatacon-iccp`
(the last four in `IccpTlsDefaults.h`; the `TLS` section parser of the library is given them as its starting values and its table of `Compatibility` presets, so the keys, the defaults and the messages are the same as always).
Bytes from the socket go into the filter; decrypted bytes come out of the manager and go to the stack; the stack's output goes into the manager, which encrypts it and writes the TLS records to the socket. The manager is built on a `TCPSocketManager` (it runs on that manager's strand), so its throttling and retry settings apply to the TLS records as they do to anything else;
what is written is tied to the serial of its TCP connection, so that nothing written for one TCP connection can ever reach the next
(see [Nothing carries over from one connection to the next](#nothing-carries-over-from-one-connection-to-the-next)). The TCP initiator starts the TLS handshake as soon as the TCP connection is up and the COTP connection request
is only sent when the handshake has completed and the peer's certificate was accepted; the listener starts the stack at the same moment. Nothing is ever written to the socket in plaintext on a TLS port.
After a failed handshake, or a `close_notify` from the peer, the port (not the manager) decides when to connect again, with a back-off for the initiator; a TCP connection that is lost is reconnected by the manager itself.
Release closes the session with a TLS `close_notify` before the TCP connection is closed; a `close_notify` from the peer ends the association.
`TraceHex` logs the **plaintext** (after decryption, before encryption): it exposes the application data in the log, so it is for diagnosing only.

### TLS configuration

The section is a nested object, `"TLS"`, valid for both port types and all roles. Every key is checked strictly (an unknown key, a wrong type or a bad value names the key), and, when `Enabled` is true, the files are read at configuration time:
a missing or unreadable file, a PEM that does not parse, a key that does not match the certificate, a CA, CRL or cipher list that does not load are configuration errors and the port does not enable.

```json
"TLS" :
{
	"Enabled" : true,
	"CertFile" : "certs/iccp-node.crt", "KeyFile" : "certs/iccp-node.key",
	"CAFile" : "certs/ca.crt"
}
```

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Enabled | bool | false | TLS on this port. When true and `Port` is not given, the port is **3782**, the TCP port of secure ICCP (an explicit `Port` always wins; without TLS the default stays 102). |
| CertFile | string | none | Our certificate, PEM; intermediate CA certificates may follow it. Required for the TLS server (the TCP listener); optional for the client, but then the client presents no certificate and a server that requires one (the default) refuses it. |
| KeyFile | string | none | Our private key, PEM. Required with `CertFile`. Must match the certificate. |
| KeyPasswordEnv | string | none | The **name** of an environment variable that holds the key's password, for an encrypted key. A password in the configuration (`KeyPassword`) is refused on purpose. The library never prompts. |
| CAFile, CAPath | string | none | The CA certificates the peer's certificate must chain to: a PEM bundle and/or a directory of hashed certificates (`c_rehash`). One of them is required while `VerifyPeer` is true. |
| CRLFile | string | none | PEM CRL(s). Enables revocation checking of the peer certificate, **fail-closed**: a certificate whose issuer has no CRL here, or whose CRL has expired, is refused. The file is re-read when it changes (see below). |
| CRLCheckChain | bool | false | Check the CRLs of the intermediate CAs too, not only the peer certificate's (then every CA of the chain needs a CRL in `CRLFile`). |
| VerifyPeer | bool | true | Verify the peer's certificate. `false` accepts any certificate (and logs a warning when the port is built): for testing only. |
| RequireClientCert | bool | true | TLS server: a client without a valid certificate is refused (mutual authentication, as IEC 62351-3 requires). |
| MinVersion, MaxVersion | string | "1.2", "1.3" | `"1.2"` or `"1.3"`. Older protocol versions are not supported. Compression is never used. |
| Ciphers | string | ECDHE with AES-GCM (ECDSA and RSA), DHE-RSA with AES-GCM | TLS 1.2 cipher list, OpenSSL syntax. To talk to an older stack that needs `AES128-SHA` and the like, name it here. |
| CipherSuites13 | string | `TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256` | TLS 1.3 suites, OpenSSL syntax. |
| PeerCertAllowList | array of strings | empty | If not empty, the peer's certificate must also match one entry: `"CN:name"` (a subject common name), `"SAN:name"` (a DNS name or IP address among the subject alternative names, exact match, no wildcards) or `"SHA256:hex"` (the certificate's SHA-256 fingerprint, colons optional). Needs `VerifyPeer`. A certificate that passed the CA check but is not listed is refused with an alert. |
| VerifyHostname | bool | false | TLS client: the server's certificate must carry `ServerName` (default: `IP`) as a subject alternative name (DNS name or IP address). The subject CN is not used. Needs `VerifyPeer`. |
| ServerName | string | `IP` | TLS client: the name verified by `VerifyHostname`, and sent as the SNI when it is a host name. |
| HandshakeTimeoutms | number | 10000 | The TLS handshake must complete within this time of the TCP connection being made (100 to 600000). |
| RenegotiateIntervalSeconds | number | 0 | 0: no renegotiation at all (a peer's attempt is refused). Otherwise the TLS client renegotiates (TLS 1.2) or updates its keys (TLS 1.3) this often, and the TLS server accepts it from the peer, but no more often than every half interval (else it ends the session). See the notes below. |
| SessionResumption | bool | false | Allow session resumption (abbreviated handshakes on reconnect). **Off by default**: an ICCP association is long lived, so a reconnect is rare and an abbreviated handshake saves nothing worth having; a resumed session is not checked against the CA, the CRL or the expiry again (`PeerCertAllowList` still is); and a stack that cannot resume would fail only on a reconnect, after working at start-up. With it on, a session is only kept after a clean close (`close_notify`). |
| CertExpiryWarningDays | number | 30 | Log a warning when the peer's (or our own) certificate expires within this many days. An expired one is refused unless `VerifyPeer` is false. |
| Compatibility | string | "Default" | `"Default"`, or the preset `"Legacy12"` for an older stack: TLS 1.2 only, no session tickets and no resumption, no SNI, groups P-256, P-384 and P-521 only (no hybrid or X25519 key share), and a conservative RSA-based AES-GCM then AES-CBC cipher list. Every key you write yourself overrides the preset. The ClientHello it produces is shown below. `[S]` |
| Groups | string | the library's | The key exchange groups offered, `SSL_CTX_set1_groups_list` syntax, for example `"P-256:P-384"`. OpenSSL 4 offers a hybrid post-quantum group (X25519MLKEM768) first and sends its key share (about 1.2 kB): a ClientHello of about 1.5 kB. Naming only classic groups removes that. `[S]` |
| SignatureAlgorithms | string | the library's | The signature algorithms offered (and, for a server, asked of the client), `SSL_CTX_set1_sigalgs_list` syntax, also used for client authentication, for example `"RSA+SHA256:RSA+SHA384:ECDSA+SHA256"`. `[S]` |
| SendSNI | bool | true | TLS client: `false` never sends the server_name extension. (It is only sent for a host name, never for an IP address, whatever this says.) |
| ExtendedMasterSecret | bool | true | TLS 1.2: `false` does not offer or accept extended_master_secret (RFC 7627). |
| SessionTickets | bool | true | `false` leaves out the session_ticket extension (TLS 1.2) and sends no tickets (TLS 1.3); session id resumption stays if `SessionResumption` is true. With the default `SessionResumption` false the extension is already left out (checked in the unit tests on the ClientHello). With TLS 1.3 resumption needs tickets, so it is off with this. |
| SecurityLevel | number | the library's (1) | The OpenSSL security level, 0 to 5. Level 0 allows SHA-1 signatures, small keys and small DH groups; use it only if an old peer cannot do without, and look at the warning it deserves. Levels 4 and 5 refuse 256 bit EC keys. |
| KeyLogFile | string | empty (off) | **Diagnostics only.** Append the TLS secrets in the NSS key log format to this file, so that Wireshark can decrypt a capture. Off unless set. A warning is logged at start-up and on every connection; the secrets are never in the normal log. The file is created for its owner alone (Linux mode 0600, a Windows access control list granting the owner only) if it does not exist; an existing file keeps its rights. See [Decrypting a capture](#decrypting-a-capture-keylogfile). Anyone who can read the file can decrypt the traffic. |

Examples in `ExampleConfig/ICCP-TLS`: an `ICCPServer` and an `ICCPClient` over TLS in one `opendatacon`, with a script (`make-certs.sh`, `make-certs.ps1`) that makes throw-away certificates with the `openssl` command line tool.
No certificate or key is ever committed; the tests make theirs at run time.

Notes:

* **Certificate files are re-read** when their modification time changes, on the next connection (certificate, key, CA file and CRL file): a renewed certificate or a new CRL needs no restart.
  A changed file that cannot be loaded makes the connection fail (fail closed), with the reason in the log. The CA directory (`CAPath`) is read by OpenSSL on demand.
* **Handshake failures never leave the port stuck.** The TCP connection is closed with a clear error in the log and the port tries again: the initiator after at least a second, backing off up to 30 s while the handshake keeps failing
  (a wrong certificate does not get better by itself); the listener is ready for the next connection after 100 ms. The listener also names a peer that does not speak TLS (below).
* **Renegotiation** is off unless asked for, and is risky: it works between two of these ports, but a peer that refuses it (an OpenSSL server without it enabled, most embedded TLS stacks) answers with a `no_renegotiation` alert,
  which OpenSSL treats as fatal in the middle of a renegotiation, and the association is lost at every interval. Enable it only when the peer is known to allow it. With TLS 1.3 the key update is requested instead,
  which is accepted by any TLS 1.3 peer. Only the TLS client starts them, so that the two ends never collide.
* **Session resumption** is off by default (see `SessionResumption`). When it is on it is per port and in memory; a restart starts with full handshakes. A resumed session still goes through `PeerCertAllowList`, but not through the CA, CRL and expiry checks again.
* The TLS handshake runs on the port's strand like everything else; a stalled peer cannot block other ports (the handshake timer is the 50 ms tick of the port).

### Building with TLS support

TLS needs OpenSSL 1.1.1 or later (the code uses only interfaces that exist from 1.1.1 to 4.0). Enable `ODC_ASIO_SSL`; it links OpenSSL into `libODC` (which holds the TLS filter, and already held asio's SSL support) and `ICCPPort` gets the `ODC_ASIO_SSL` define from its own `CMakeLists.txt` (`Mace` still calls OpenSSL from the port itself). Without it the filter is compiled as stubs and a configuration that enables TLS is refused with a clear message.

**Linux**: install the development package (`libssl-dev`, `openssl-devel`) and configure normally; OpenSSL is linked dynamically.

```
cmake -S . -B build -DODC_ASIO_SSL=ON -DICCPPORT=ON -DTESTS=ON
cmake --build build --target ICCPPort_tests
```

**Windows** (MSVC): the `Code/Libs/ODC/CMakeLists.txt` asks for static OpenSSL libraries linked with the static runtime (`OPENSSL_USE_STATIC_LIBS`, `OPENSSL_MSVC_STATIC_RT`), and the Shining Light OpenSSL 4.0 installer lays its libraries out as
`lib\VC\x64\{MD,MDd,MT,MTd}\libssl_static.lib` / `libcrypto_static.lib`, which CMake 3.28's `FindOpenSSL` does not recognise. Name the libraries explicitly (the `MD`/`MDd` pair matches the project's `/MD` and `/MDd` runtime);
this configuration is known to work with OpenSSL 4.0.3 installed in `D:\OpenSSL-Win64`:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DODC_ASIO_SSL=ON ^
  -DOPENSSL_ROOT_DIR=D:/OpenSSL-Win64 -DOPENSSL_INCLUDE_DIR=D:/OpenSSL-Win64/include ^
  -DLIB_EAY_RELEASE=D:/OpenSSL-Win64/lib/VC/x64/MD/libcrypto_static.lib -DLIB_EAY_DEBUG=D:/OpenSSL-Win64/lib/VC/x64/MDd/libcrypto_static.lib ^
  -DSSL_EAY_RELEASE=D:/OpenSSL-Win64/lib/VC/x64/MD/libssl_static.lib -DSSL_EAY_DEBUG=D:/OpenSSL-Win64/lib/VC/x64/MDd/libssl_static.lib ^
  -DICCPPORT=ON -DTESTS=ON
cmake --build build --config Release --target ICCPPort_tests
```

(Open point: the ODC CMake is unchanged; making `FindOpenSSL` cope with this layout, or not forcing the static runtime, would remove the need for the explicit paths.)
With an older OpenSSL layout (`lib\VC\static\libssl64MD.lib`) `-DOPENSSL_ROOT_DIR` alone is enough.

### Making certificates

Throw-away certificates for trying it out, with the `openssl` command line tool (synthetic names; `ExampleConfig/ICCP-TLS/make-certs.sh` and `make-certs.ps1` do exactly this). A real installation uses its own PKI.

**Key algorithm.** The scripts make RSA keys, 2048 bits by default (`-KeyBits` / `KEYBITS`; they refuse anything below 2048), with SHA-256 signatures. RSA is the choice because some ICCP stacks do not accept other key algorithms (ECDSA, for instance), and a key of at least 2048 bits is a common minimum. The port itself takes RSA and EC certificates alike; its default `Ciphers` list covers `ECDHE-RSA` and `DHE-RSA`, and a stack that needs RSA key exchange or CBC suites is accommodated with `Ciphers` (see the table above). Node certificates carry `keyEncipherment` as well as `digitalSignature` for the same reason.

Every file is PEM (`-----BEGIN CERTIFICATE-----`, keys as `-----BEGIN PRIVATE KEY-----`), except the PKCS#12 packages. Besides `ca.crt` and each node's `.crt` and `.key`, the scripts write, per node:

| File | Contents | Use |
|------|----------|-----|
| `<node>-chain.pem` | the node certificate followed by the CA certificate | `CertFile` (the port loads it with `SSL_CTX_use_certificate_chain_file`, so the CA is sent with the node certificate) |
| `<node>-cert-key.pem` | the node certificate and its private key in one file | `CertFile` and `KeyFile` may both name it; **keep it private, it holds the key** |
| `<node>.p12` | PKCS#12 package: the node certificate, its private key and the CA certificate | Importing into the Windows certificate store (below). Protected with the password `example` (`-P12Password` / `P12PASS`); **keep it private, it holds the key** |

The plain `.crt` and `.key` files stay the ones the example configurations use. The port reads PEM only; it does not read the `.p12`.

**Windows (PKCS#12).** By default the `.p12` files use the older 3DES / SHA-1 protection, which every Windows version can import. OpenSSL 3 and later would otherwise write AES-256 / SHA-256, which only Windows 11 and Windows Server 2022 or later can open; `-P12Modern` (`P12MODERN=1`) selects that. To import:

```
# the node certificate with its key, into the machine's Personal store (or use -CertStoreLocation Cert:\CurrentUser\My)
Import-PfxCertificate -FilePath iccp-node-a.p12 -CertStoreLocation Cert:\LocalMachine\My -Password (ConvertTo-SecureString 'example' -AsPlainText -Force)
# the CA, so the node certificates chain to a trusted root
Import-Certificate -FilePath ca.crt -CertStoreLocation Cert:\LocalMachine\Root
```

`certutil -dump -p example iccp-node-a.p12` shows what a package holds. Windows decides which store the CA certificate from the package goes into, so import `ca.crt` as above to make it trusted. (These import commands are the standard ones; they have not been run here, since they change the machine's certificate stores.)

```
# the CA (the subject is a full distinguished name: country, state, organisation, organisational unit, common name)
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out ca.key
openssl req -x509 -new -key ca.key -sha256 -days 825 \
  -subj "/C=AU/ST=New South Wales/O=Example Utility/OU=Certificate Authority/CN=Example ICCP CA" \
  -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out ca.crt

# a node certificate, usable as a TLS server and as a TLS client; ext.cnf holds:
#   basicConstraints=critical,CA:FALSE
#   keyUsage=critical,digitalSignature,keyEncipherment
#   extendedKeyUsage=serverAuth,clientAuth
#   subjectAltName=DNS:iccp-node-a.example,IP:127.0.0.1
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out node-a.key
openssl req -new -key node-a.key -subj "/C=AU/ST=New South Wales/O=Example Utility/OU=ICCP Nodes/CN=iccp-node-a.example" -out node-a.csr
openssl x509 -req -in node-a.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 825 -sha256 -extfile ext.cnf -out node-a.crt

# a PKCS#12 package for Windows (certificate + key + CA); the -certpbe/-keypbe/-macalg options keep it importable on every Windows version
openssl pkcs12 -export -inkey node-a.key -in node-a.crt -certfile ca.crt -name iccp-node-a.example \
  -certpbe PBE-SHA1-3DES -keypbe PBE-SHA1-3DES -macalg sha1 -out node-a.p12

# show the subject and issuer of a certificate in a readable form
openssl x509 -in node-a.crt -noout -subject -issuer -nameopt multiline

# a CRL revoking a certificate (revocation checking needs `CRLFile`): made with `openssl ca -gencrl` from a CA set up with `openssl ca` (see its documentation)
```

To protect a key with a password, add `-aes256` when generating it (`openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -aes256 -out node-a.key`) and name an environment variable in `KeyPasswordEnv`.

### What the log and status show

At `info`, a completed handshake is logged with what was negotiated and who the peer is (nothing secret):

```
NodeA: TLS established: TLSv1.3 TLS_AES_256_GCM_SHA384, peer certificate subject "CN=iccp-node-b.example,OU=ICCP Nodes,O=Example Utility,ST=New South Wales,C=AU" issuer "CN=Example ICCP CA,OU=Certificate Authority,O=Example Utility,ST=New South Wales,C=AU" serial 0b SHA-256 7c3f...e1 valid until 2028-02-01T10:00:00Z
NodeA: association established
```

A certificate that expires soon is a `warn` (`TLS: the peer certificate "CN=..." expires in 12 days (2028-02-01T10:00:00Z)`). `GetStatus` has a `TLS` object: `Enabled`, `Role` (`Client` or `Server`), `State` (`Idle`, `Handshaking`, `Established`,
`Failed`, `Closed`, `Down`) and, once established, `Protocol`, `Cipher`, `Resumed`, `PeerSubject` and `PeerNotAfter`.

### Troubleshooting

All TLS messages start with `TLS:` and are `error` level when the connection is lost because of TLS. The text names the OpenSSL reason, the certificate verification result with the depth and subject of the certificate it concerns, and the alert exchanged.

| Log line (abbreviated) | Meaning |
|------------------------|---------|
| `TLS: TLS handshake failed: ...certificate verify failed [certificate verification failed: unable to get local issuer certificate (depth 0, subject CN=...)] [alert sent: fatal unknown CA]` | We do not trust the peer's issuer: the CA is not in `CAFile`/`CAPath`, or the peer did not send the intermediate certificates. |
| `...self-signed certificate in certificate chain (depth 1, subject CN=...)` | The chain ends in a root that is not in our CA bundle: the other CA. |
| `...certificate has expired` / `...certificate is not yet valid` (`depth N`) | The peer's certificate is outside its validity period, or the clocks differ. |
| `...certificate revoked` / `...unable to get certificate CRL` / `...CRL has expired` | `CRLFile` is set: the peer's certificate is revoked, or there is no (current) CRL for its issuer: fail-closed. |
| `...Hostname mismatch` | `VerifyHostname`: the server's certificate has no SAN equal to `ServerName` (or `IP`). |
| `TLS: TLS handshake failed: ...tlsv1 alert unknown ca [alert received: fatal unknown CA]` | The **peer** refused our certificate (its CA does not include ours). Look in the peer's log. |
| `TLS: TLS handshake failed: ...peer did not return a certificate` | The server requires a client certificate and this client has none (`CertFile`). |
| `TLS: TLS handshake failed: ...[the peer certificate "CN=..." is not in PeerCertAllowList]` | The certificate verifies but is not on the allow list. |
| `TLS: TLS handshake failed: ...protocol version` / `...no shared cipher` | `MinVersion`/`MaxVersion` or `Ciphers` leave nothing in common. Both ends' logs name it. |
| `TLS: the first byte received (0x03) is not a TLS handshake record: the peer is not using TLS (it looks like a plain RFC 1006 / TPKT connection)` | A plain-TCP peer connected to a TLS listener: enable TLS on the peer or disable it here. |
| `TLS: the peer closed the TCP connection during the TLS handshake (is the peer using TLS on this port?)` | A TLS initiator connected to a plain listener (or something that dropped the connection). |
| `TLS: the TLS handshake did not complete within 10000 ms` | The peer accepted the TCP connection and said nothing (a firewall or a proxy), or the network is too slow for `HandshakeTimeoutms`. |
| `TLS established: ...` and then, a few milliseconds later, the peer closes the TCP connection with no TLS alert and no `close_notify` (no ICCP message was sent by us) | The TLS layer of the **peer** accepted the handshake, and the peer's application then refused the connection. Seen with a real-world peer that checks the client certificate even though it did not say so: the client certificate (its subject, thumbprint or issuing CA) was not configured on the peer for this node. Register the certificate our port presents (`CertFile`) with the peer. Capture the exchange with `KeyLogFile` to confirm: the decrypted capture shows a complete handshake, the peer's session tickets, and a TCP FIN with nothing from our side after the handshake. |
| `configuration error: "TLS": "KeyFile" "...": not a valid PEM private key (if it is encrypted, ...)` | An encrypted key with no `KeyPasswordEnv`, a wrong password, or not a key. |
| `configuration error: "TLS": "CertFile" "..." and "KeyFile" "..." do not belong together` | Certificate and key are from different pairs. |

To see the exchange on the wire, a packet capture shows only TLS records; for the plaintext use `TraceHex` (see above), and see the next sections.

### Diagnosing a handshake that fails

**Every handshake failure line says what we sent and what came back.** It ends with a summary of the handshake so far, for example:

```
ICCPClient: TLS: TLS handshake failed: error:0A0003F2:SSL routines::tls alert unexpected message [alert received: fatal unexpected message]
  [we are the TLS client; sent: ClientHello (1505 bytes); received: alert fatal unexpected message after 0 handshake messages from the peer]
```

That reads: we were the client, we sent a ClientHello of 1505 bytes, and the peer answered with a fatal `unexpected_message` alert before sending a single handshake message of its own.
The same summary is added when the peer closes the TCP connection during the handshake.

**What the alerts mean, and what to try.** An alert is the peer's reaction to something we sent (or, when we sent it, ours to something received). The decision procedure:

1. Capture the handshake with Wireshark (the hellos are not encrypted; with [KeyLogFile](#decrypting-a-capture-keylogfile) the rest of it is readable too). Look at what we sent first (the ClientHello: versions, suites, groups, signature algorithms, size) and at what, if anything, came back.
2. If the peer answers the ClientHello with `unexpected_message`, `decode_error`, `illegal_parameter`, `handshake_failure`, `protocol_version` or `insufficient_security`, or just closes the connection, it did not like the hello. OpenSSL 4 sends a larger and newer hello than older stacks expect:
   a hybrid post-quantum key share (X25519MLKEM768, 1216 octets) so the ClientHello is about 1.5 kB (it may not fit one TCP segment or one fixed-size buffer in a simple stack), TLS 1.3 and 1.2 together (with `supported_versions`, `psk_key_exchange_modes`, `key_share`),
   `extended_master_secret`, `encrypt_then_mac`, `renegotiation_info`, `ec_point_formats`, a long `supported_groups` list, a long `signature_algorithms` list with RSA-PSS, EdDSA and ML-DSA entries,
   and, when a host name is configured, an SNI. (With `"SessionResumption": true` it also sends an empty `session_ticket`, and a resumed attempt carries the stored ticket, about 2.5 kB.) As a server it also sends a CertificateRequest with the same long list. `[S]`
3. Try `"Compatibility": "Legacy12"`. If that works, find which ingredient the peer needs by turning the preset's parts off one at a time (put `"Compatibility": "Default"` back and set one key at a time):
   `"MaxVersion": "1.2"`, then `"Groups": "P-256:P-384"` (drops the hybrid key share and so the size), then `"SignatureAlgorithms": "RSA+SHA256:RSA+SHA384:ECDSA+SHA256"`, `"SendSNI": false`, `"ExtendedMasterSecret": false`,
   `"SessionResumption": false` (off by default: if you turned it on, it adds the session_ticket extension and, on a reconnect, a stored ticket) and finally `"Ciphers"` with the suites the peer's documentation names.
4. If the peer answers well but our certificate is refused (`bad_certificate`, `unknown_ca`, `certificate_expired`, `certificate_unknown`, `unsupported_certificate`, `certificate_required`, `access_denied`), the problem is the certificate, not the hello: check the key size (some stacks need RSA of 2048 bits or more), the chain (send the intermediate certificates in `CertFile`),
   the extended key usage (`clientAuth` for a client certificate), the names, the validity period, and that the peer trusts our CA.
5. If the failure comes after a handshake that looked complete (the TCP connection is closed or reset right after the tickets or the Finished messages, with no alert), the peer is closing on its own policy: decrypt a capture with [KeyLogFile](#decrypting-a-capture-keylogfile) and look at what was exchanged, and at the peer's log.

| Alert (received) | What it usually means | What to try |
|------------------|-----------------------|-------------|
| `unexpected_message` (10) | The peer received a record or handshake message it did not expect at that point: a stray record before the ClientHello (a bug of a version of this port, see [below](#nothing-carries-over-from-one-connection-to-the-next)), a hello it could not take, or a TLS 1.3 message it does not know. | In a capture: was the **first** record we sent a handshake record (`0x16`)? Then `Compatibility` `Legacy12`, then the one-by-one list above. |
| `handshake_failure` (40) | No common cipher suite, group or signature algorithm (or the peer needs a client certificate we did not send). | `Ciphers`, `Groups`, `SignatureAlgorithms`; the peer's supported suites. |
| `protocol_version` (70) | No common protocol version: the peer wants TLS 1.2 or 1.0/1.1, or only 1.3. | `MinVersion`/`MaxVersion`; this port does not offer anything below TLS 1.2. |
| `insufficient_security` (71) | The peer finds the offer too weak (a small DH group, SHA-1, a small key in our certificate). | A stronger certificate (RSA at least 2048 bits), `Ciphers`. |
| `bad_certificate` (42), `unsupported_certificate` (43), `certificate_unknown` (46) | The peer could not accept our certificate: format, key type or size, chain, extensions. | Check the certificate as in step 4. |
| `unknown_ca` (48) | The peer does not trust the issuer of our certificate (or did not get the intermediate certificates). | Put the chain in `CertFile`; add our CA to the peer's trust. |
| `certificate_expired` (45), `certificate_revoked` (44) | As named. | Renew; check the clocks. |
| `decode_error` (50), `illegal_parameter` (47), `unsupported_extension` (110), `missing_extension` (109) | The peer could not parse, or did not accept, something in our hello. | As for `unexpected_message`. |
| `internal_error` (80) | The peer failed internally. | The peer's log. |
| `no_renegotiation` (100) | The peer refuses renegotiation. | `RenegotiateIntervalSeconds` 0. |

**The ClientHello of `"Compatibility": "Legacy12"`** (as a capture shows it, a TLS client with RSA certificates; with the defaults it is about 1505 bytes):

```
ClientHello 158 bytes, legacy_version TLS1.2, 18 cipher suites [ECDHE-RSA-AES256-GCM-SHA384 ECDHE-RSA-AES128-GCM-SHA256 DHE-RSA-AES256-GCM-SHA384 DHE-RSA-AES128-GCM-SHA256 AES256-GCM-SHA384 AES128-GCM-SHA256
  ECDHE-RSA-AES256-SHA384 ECDHE-RSA-AES128-SHA256 DHE-RSA-AES256-SHA256 DHE-RSA-AES128-SHA256 AES256-SHA256 AES128-SHA256 ECDHE-RSA-AES256-SHA ECDHE-RSA-AES128-SHA DHE-RSA-AES256-SHA DHE-RSA-AES128-SHA AES256-SHA AES128-SHA],
  compression [null], 6 extensions [renegotiation_info ec_point_formats supported_groups encrypt_then_mac extended_master_secret signature_algorithms], supported_groups [P-256 P-384 P-521],
  signature_algorithms [ecdsa_secp256r1_sha256 ecdsa_secp384r1_sha384 ecdsa_secp521r1_sha512 ed25519 ed448 rsa_pss_pss_sha256 ... rsa_pkcs1_sha512 ecdsa_sha224 rsa_pkcs1_sha224 dsa_sha224 dsa_sha256 ...], ec_point_formats [uncompressed], session_id 0 bytes
```

It is TLS 1.2 only (no `supported_versions`), has no `key_share`, no `session_ticket` and no `server_name`, offers only the NIST curves, and is 158 bytes. The `signature_algorithms` list is still the library's: if a stack chokes on it, set `"SignatureAlgorithms"` too.
`ec_point_formats` cannot be switched off by itself: OpenSSL leaves it out when no suite needs it (a `Ciphers` list without ECDHE and ECDSA suites, which then also offers finite field groups instead of the curves); with any ECDHE suite it is sent. `[S]`

### Decrypting a capture (KeyLogFile)

A capture of a TLS session shows only encrypted records after the hello. To see inside it, `"KeyLogFile": "keys.log"` in the `TLS` section makes the port append the session secrets to that file in the NSS key log format (`CLIENT_RANDOM`, or for TLS 1.3 `CLIENT_HANDSHAKE_TRAFFIC_SECRET`,
`SERVER_HANDSHAKE_TRAFFIC_SECRET`, `CLIENT_TRAFFIC_SECRET_0`, `SERVER_TRAFFIC_SECRET_0`, `EXPORTER_SECRET`). **This is for diagnostics only: anyone who can read the file can decrypt the traffic**, so the port warns at start-up and on every connection, never writes the file unless asked, creates it readable by its owner alone,
and never puts a secret in the normal log. Turn it off and delete the file when you are done.

To use it with Wireshark: capture the traffic (`tcp.port == <port>`; on Windows a connection to the same machine needs the Npcap loopback adapter), then in Wireshark `Edit > Preferences > Protocols > TLS > (Pre)-Master-Secret log filename` and choose the file (older versions call the protocol SSL). The capture then shows the handshake messages (`tls.handshake`) in the clear and, after them, the decrypted application data (TPKT, COTP, MMS). What to look for: the first record the client sends must be a handshake record (`tls.record.content_type == 22`, the ClientHello);
the ClientHello's size and `tls.handshake.extensions_*`; the alert, if any (`tls.alert_message`); who sends the first TCP FIN, and after which message.

### Nothing carries over from one connection to the next

A TLS record encrypted for one connection is garbage on the next, and a peer answers it with a fatal `unexpected_message` alert; a stale COTP disconnect on a plain connection would make a peer drop the new connection. Earlier versions of this port could do that: bytes written for a connection that had just gone away
(the abort PDUs of an association whose TCP connection was closed by the peer, or a connection request still buffered) were held by the socket manager and sent as the **first bytes of the next connection**, before its ClientHello. A capture of such a failure shows a TLS record of type 23 (application data, `17 03 03 ...`) in the first TCP segment after the
three-way handshake, then the peer's alert `15 03 03 00 02 02 0a` and the connection reset, and only then the client's real ClientHello. That is fixed in this version, in three places:

* every connection has a number (the transport **epoch**; the socket manager also numbers its connections), the association remembers the one it was made for, and what it produces for any other is dropped (with a debug line) instead of written; the `TCPSessionManager` also keeps the session (plain or TLS) of every connection to that connection (by its serial), and a write that is for another one is dropped there;
* a write is bound to its connection in the socket manager as well: if the socket has moved on to a newer connection by the time the write runs, it is dropped there, and nothing buffered or half written when a connection ends survives into the next;
* when a new connection comes up, the old TLS session is discarded by the `TCPSessionManager` **without writing anything** and the old association is discarded by the port the same way (an abort is only for a live association on a live connection); on a port without TLS the first bytes of every connection are checked: a plain initiator starts with a COTP connection request, a plain listener with a connection confirm
  (anything else is refused with an error line, "internal error: refusing to write ... as the first bytes of a connection"). On a TLS port the manager starts every connection with the handshake of its own session, so that check is not needed there (and the plain TCP unit test that shows the bug with the guards off has no TLS counterpart: the TLS test shows that switching the port's guards off changes nothing).

If you still see `alert unexpected_message` at connect, capture with Wireshark: the first record the client sends on a connection must be `0x16`.
The fix was checked against a real-world peer: the capture that showed the stray record, repeated with a build that has the fix, shows every connection starting with the ClientHello. (The stray record was 44 bytes, which is the size of the client's COTP connection request as a TLS record, so that is the likely content, but it was not confirmed.)
For the associations that end because the peer closes the connection right after the handshake (the peer's own policy: it dislikes the client certificate, the key size, the extensions of the chain, a missing first message), see step 5 above.

### TLS items that are not verified `[S]`

TLS has been used successfully with one real-world peer: a third-party stack that needs RSA keys of at least 2048 bits and checks the client certificate. That is a single peer, and which TLS version and cipher it negotiated is not recorded here. None of the following has been checked against a real IEC 62351-3 peer (the TLS tests are between this implementation and itself, and against what OpenSSL does):

* the cipher suites of the default list: IEC 62351-3 has differed between editions (the older editions named `TLS_RSA_WITH_AES_128_CBC_SHA` and the DHE variants; the newer ones TLS 1.2 with ECDHE and AES-GCM, and TLS 1.3). The defaults are conservative; a peer that needs another suite is accommodated with `Ciphers`;
* the periodic renegotiation / session resumption interval that 62351-3 asks for: implemented as `RenegotiateIntervalSeconds` (off by default), only between two of these ports;
* whether a particular peer verifies the subject alternative name or the common name, and what it puts in them: `PeerCertAllowList` and `VerifyHostname` use the SAN, and the allow list can use the CN;
* the TCP port 3782 as the default (it is the port registered for secure ICCP/TASE.2 as far as is known);
* that a peer does not insist on an SSLv3/TLS 1.0/1.1 hello or compression: those are not offered;
* the compatibility options (`Compatibility`, `Groups`, `SignatureAlgorithms`, `SendSNI`, `ExtendedMasterSecret`, `SessionTickets`, `SecurityLevel`) and what a particular older stack needs of them: they change the ClientHello exactly as documented (unit tested on the bytes the port emits, and against the OpenSSL command line `s_server` and `s_client`),
  but which combination a given real peer accepts is not known.

## Secure ICCP: MACE

MACE (MMS Application Certificate Exchange) is the authentication that some ICCP peers carry in the **ACSE association**: each end sends its X.509 certificate with a signature in the AARQ (the initiator) and the AARE (the responder), so
the two applications authenticate each other whatever the transport is. It is independent of TLS (either, both or neither can be on) and of the `ServiceRole`, and valid for both port types.
It needs OpenDataCon built with OpenSSL (`-DODC_ASIO_SSL=ON`, see [Building](#building)); without it the configuration `"MACE": {"Enabled": true}` is rejected with
`"MACE": this build has no MACE support: rebuild with ODC_ASIO_SSL=ON (and OpenSSL) to use "Enabled": true`, and everything else works as before. (The encoding of the authentication fields, `Acse.h`, is in every build.)
This is not IEC 62351-4; that stays unimplemented (see [Status](#status)).

**Who authenticates whom.** The association initiator (the TCP initiator) authenticates in its AARQ. A MACE responder checks it and, when it is valid, authenticates in its AARE with its own certificate; the initiator checks that. Both
directions are checked and both can be required. One `MACE` section (one certificate and key, one trust configuration) serves both roles.

### MACE configuration

The section is a nested object, `"MACE"`. Every key is checked strictly (an unknown key, a wrong type or a bad value names the key), and, when `Enabled` is true, the files are read at configuration time: a missing or unreadable file, a PEM that does not parse,
a key that does not match the certificate, a key that is not RSA or is outside the size limits, a CA or CRL that does not load are configuration errors and the port does not enable.

```json
"MACE" :
{
	"Enabled" : true,
	"CertFile" : "certs/iccp-node.crt", "KeyFile" : "certs/iccp-node.key",
	"CAFile" : "certs/ca.crt"
}
```

`MechanismName` is optional and is left out here: the default is the mechanism-name IEC 62351-4 defines. Set it only to talk to a peer that uses another value, for example `"MechanismName" : "1.0.840.0.1.0.1.1"` (see [The mechanism-name](#the-mechanism-name)).

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Enabled | bool | false | MACE on this port. |
| MechanismName | string | `1.0.62351.4.3.1` | The authentication mechanism-name, a dotted object identifier, that **we send** in the ACSE APDU. Optional; when given it must be a valid dotted OID. The default is `version1-mechanism` of IEC 62351-4 (clause 11.1.4.3); the other value to know is `1.0.840.0.1.0.1.1`, which implementations of IEC TS 62351-4:2007 use. A value that is not the default gets a **configuration warning**. What the peer sends is accepted whatever it is, see [The mechanism-name](#the-mechanism-name). |
| CertFile | string | none, **required** | Our certificate, PEM. The DER of the **first** certificate in the file is what is sent (the leaf only, no chain). |
| KeyFile | string | none, **required** | Our private key, PEM, **RSA**. Must match the certificate. |
| KeyPasswordEnv | string | none | The **name** of an environment variable that holds the key's password, for an encrypted key. A password in the configuration (`KeyPassword`) is refused on purpose. The library never prompts. |
| CAFile, CAPath | string | none | The trust anchors for the **peer's** certificate: a PEM bundle and/or a directory of hashed certificates (`c_rehash`). One of them is required while `VerifyPeer` is true. The peer sends its leaf certificate only, so **put its intermediate CAs here too**. |
| CRLFile | string | none | PEM CRL(s): revocation checking of the peer certificate, **fail-closed** (a certificate whose issuer has no current CRL here is refused). Read at configuration time (a changed file needs a restart). |
| CRLCheckChain | bool | false | Check the CRLs of the intermediate CAs too. |
| PeerCertAllowList | array of strings | empty | If not empty, the peer's certificate must also match one entry: `"CN:name"`, `"SAN:name"` or `"SHA256:hex"` (the same syntax and rules as the TLS list). Needs `VerifyPeer`. |
| TimeWindowSeconds | number | 600 | The peer's signing time must be within this many seconds of our UTC clock (1 to 86400). See [The time window](#the-time-window-and-clocks). |
| MinKeyBits | number | 2048 | The smallest RSA key accepted, ours and the peer's (1024 to 8192). |
| MaxKeyBits | number | 8192 | The largest RSA key accepted (up to 8192, never more: a real-world peer documents 8192 bits as its maximum for MACE). |
| RequireAuthentication | bool | true | Responder side: an association request without an authentication value is refused (ACSE diagnostic `authentication-required` [14]). With `false` it is accepted, with a warning, unauthenticated, and our response then carries no authentication. |
| RequireResponderAuthentication | bool | true | Initiator side: an association response without an authentication value (or with an invalid one) makes the initiator abort the association (an ACSE ABRT, abort-source service-user) after logging why. A real-world peer behaves this way, so it is the default. With `false` a response without one is accepted with a warning; an invalid one still aborts. |
| VerifyPeer | bool | true | Validate the peer's certificate chain, validity period and CRL. `false` skips that and the allow list (a warning is logged when the port is built): the signature and the time window are still checked. For testing only. |

### The wire format `[V]`

The authentication fields are the optional ACSE fields of ISO 8650 (the AARQ [10] sender-acse-requirements, [11] mechanism-name, [12] calling-authentication-value; the AARE [8], [9], [10] responder-acse-requirements, mechanism-name,
responding-authentication-value), in their place between the AE-qualifiers and the user information (the MMS Initiate):

```
AARQ [APPLICATION 0]                                        AARE [APPLICATION 1]
  [0] protocol-version        07 80                           [0] protocol-version        07 80
  [1] application-context-name (MMS)                          [1] application-context-name (MMS)
  [2] called-AP-title, [3] called-AE-qualifier                [2] result, [3] result-source-diagnostic
  [6] calling-AP-title, [7] calling-AE-qualifier              [4] responding-AP-title, [5] responding-AE-qualifier
  [10] sender-acse-requirements     'authentication' 07 80    [8] responder-acse-requirements  'authentication' 07 80
  [11] mechanism-name (OBJECT IDENTIFIER, implicit, 8B)       [9] mechanism-name (89; see The mechanism-name)
  [12] calling-authentication-value (explicit, AC)            [10] responding-authentication-value (explicit, AA)
  [30] user-information (the MMS Initiate)                    [30] user-information (the MMS Initiate response)
```

The authentication value is the same in both directions: the CHOICE alternative `external [2]` (an IMPLICIT EXTERNAL with no direct-reference, indirect-reference or data-value-descriptor) whose single-ASN1-type `[0]` holds one constructed `[0]` with three primitive members:

```
[2] { [0] { [0] { [0] certificate    the raw DER X.509 certificate of the sender: the leaf only, no chain (994 octets for an RSA 2048 certificate)
                  [1] signing time   ASCII GeneralizedTime "YYYYMMDDHHMMSSZ", UTC, 15 octets, whole seconds
                  [2] signature      the raw RSA output (256 octets for a 2048 bit key); no BIT STRING unused-bits octet } } }
```

This was decoded from a real peer's association (that peer authenticating to itself over plain TCP, both directions, and `[V]` below means checked against that). It is a standard ISO 8650 authentication-value structure, so the layout is `Acse.h`'s (`EncodeMaceAuthValue`, `DecodeMaceAuthValue`),
and what is decoded is re-encoded **byte for byte**.

### The mechanism-name

IEC 62351-4:2018+AMD1:2020 clause 11.1.4.3 says the mechanism-name component must be **present** in the AARQ and the AARE that carry the authentication, with the value `version1-mechanism = { auth-mechanism 1 }`, where
`auth-mechanism = { iso(1) standard(0) iec62351(62351) part4(4) auth-mechanism(3) }`. That is the dotted OID `1.0.62351.4.3.1`, DER `28 83 E7 0F 04 03 01` `[T]`. The same clause says the value does **not affect the procedure** and that implementations
**should be tolerant** of it in a received AARQ, because implementations of IEC TS 62351-4:2007 are known to use `{ 1 0 840 0 1 0 1 1 }` (`1.0.840.0.1.0.1.1`, DER `28 86 48 00 01 00 01 01`), which the standard calls an illegal object identifier but allows an implementation to use when talking to such a system `[T]`.
A real peer we interoperate with sends exactly that legacy value `[V]`. What this port does:

| | |
|--|--|
| What we send | The configured `MechanismName` (default `1.0.62351.4.3.1`) in our AARQ. Our AARE carries the mechanism-name **we received** when it differs from ours (an echo, the most compatible answer to a peer on the legacy value), else our own, which is the same thing `[S]` (a peer on the legacy value taking our echoed response has not been checked for the case where the values differ). |
| What we accept | Any value, in the AARQ (responder) and in the AARE (initiator). A different value is **not** a refusal; the diagnostic `authentication-mechanism-name-not-recognized` (11) is defined but no longer sent for this. A **missing** mechanism-name with an authentication value is still refused with `authentication-mechanism-name-required` (12) (an initiator aborts with the same diagnostic). |
| Warning, configuration | A configured `MechanismName` other than `1.0.62351.4.3.1` gives one warning when the port is built: `"MACE": the configured "MechanismName" 1.0.840.0.1.0.1.1 is not the mechanism-name IEC 62351-4 defines (1.0.62351.4.3.1); use it only for peers that use that value (it is the legacy value of IEC TS 62351-4:2007)`. |
| Warning, association | When the value the **peer** sent is not `1.0.62351.4.3.1`, one warning per association: `MACE: the peer's mechanism-name 1.0.840.0.1.0.1.1 is not the one IEC 62351-4 defines (1.0.62351.4.3.1): it is the legacy value of IEC TS 62351-4:2007; it also differs from ours 1.0.62351.4.3.1: accepted, because the standard asks implementations to be tolerant of it` (the two middle parts appear when they apply). A peer that sends the official value which differs from ours (our configuration is not the default) gives one info line `... differs from ours ...: accepted, as IEC 62351-4 asks implementations to be tolerant`. |

Both warnings are **expected and harmless** with a peer that uses the legacy value: the association is authenticated as usual (the mechanism-name is not part of the signature, see below).

**The A-security abstract syntax.** IEC 62351-4 clause 11.1.3 defines an additional presentation-context abstract syntax for the authentication, `mmsAuthenticationAS = { 1 0 840 0 1 1 4 1 }` (DER `28 86 48 00 01 01 04 01`) `[T]`, which a request may carry next to the ACSE and MMS contexts;
an implementation must accept a request without it, and one that receives it must include it in the accept. The authentication value then carries the context's identifier as an `identification` INTEGER in front of its `[0]` (clause 11.1.4.4). What this port does `[S]` (not seen on the wire):

* Finding: before this change a context with an abstract syntax the responder does not know (this one included) was answered with `provider-rejection` (reason `abstract-syntax-not-supported`) for that context alone, and the association went on. The responder still does that for any other unknown abstract syntax.
* A **MACE** responder answers `acceptance` with the BER transfer syntax for the A-security context when the request offers it, and then writes the `identification` (that context's identifier) into its authentication value. The decoder accepts the value with or without the `identification`, so a peer that includes it can be authenticated. Nothing is ever sent in that context.
  A responder without MACE does not support it and rejects it as before.
* Our own requests do **not** offer it (the standard makes that optional).

### The signature is not a hash `[V]`

The signature is **not** a signature over a digest of anything. It is an RSA private-key operation with PKCS#1 v1.5 type 1 padding over a fixed 30 octet block: the DigestInfo header of SHA-1 followed directly by the 15 octets of the signing time text.
The RSA public operation applied to the signature gives `00 01 FF..FF 00` and then

```
30 21 30 09 06 05 2b 0e 03 02 1a 05 00 04 14        the SHA-1 DigestInfo header (it announces a 20 octet digest)
32 30 32 36 30 31 30 32 30 33 30 34 30 35 5a        "20260102030405Z", the signing time as sent (15 octets, not 20: the lengths in the header do not match)
```

so there is no hash: the "digest" is the time string. (It is made with an EVP signing context with PKCS#1 padding and **no digest set**, which signs the data as given, and verified with `EVP_PKEY_verify_recover`; the pair works with OpenSSL 1.1.1 to 4.0.)
Consequences: the signature covers **only the signing time**. It proves that the sender holds the private key at that time. It is not bound to the association, to the peer or to anything else the APDU carries (the certificate is checked by the chain, the mechanism-name and the rest of the APDU are not signed at all),
so the protection against replay is only the receiver's time window `[S: the scope of the signature is what the verified captures show; that it is all the peer signs is an inference]`. PKCS#1 v1.5 is deterministic: two signatures of the same second are the same octets. We sign a **fresh** value for every association from the current time.

### What is checked, and the refusal diagnostics

A responder (and an initiator, for the response) checks, in this order, and refuses on the first failure; every failure is logged here with the detail (`MACE: ...`), and the ACSE diagnostic that goes to the peer carries no more than the number `[S]`:

| Check | ACSE diagnostic sent (responder) |
|-------|----------------------------------|
| No authentication value (and `RequireAuthentication`) | `authentication-required` (14) |
| A value but no mechanism-name | `authentication-mechanism-name-required` (12) |
| A mechanism-name that is not ours or not the official one | **not a refusal**: accepted, with one warning per association (see [The mechanism-name](#the-mechanism-name)); diagnostic 11 is no longer sent |
| The value is malformed, the certificate is not DER X.509, the signing time is not `YYYYMMDDHHMMSSZ` | `authentication-failure` (13) |
| The key is not RSA, or has fewer bits than `MinKeyBits` or more than `MaxKeyBits` | `authentication-failure` (13) |
| The certificate: chain to `CAFile`/`CAPath`, validity period at our clock, `CRLFile`, `PeerCertAllowList` (all skipped with `VerifyPeer` false) | `authentication-failure` (13) |
| The signature does not match the certificate and the time | `authentication-failure` (13) |
| The signing time is more than `TimeWindowSeconds` from our clock | `authentication-failure` (13) |

On a refusal the responder sends an AARE `rejected-permanent` with that diagnostic (in a session REFUSE) and closes. The initiator logs it, for example:
`association refused by the peer (ACSE result rejected-permanent, diagnostic from the ACSE service user: authentication-failure (13))`. A failure of the **response** (the initiator's check) aborts the association with an ACSE ABRT (abort-source service-user, the same as a real peer does).
Which diagnostic a given peer sends or expects for which failure is `[S]`: the real-world peer was seen to answer an association request that carried **no** authentication with the diagnostic `called-AE-qualifier-not-recognized` (9) (decoded since the refusal-decoding fix) rather than 14,
so a refusal with an unexpected diagnostic from such a peer may mean "not authenticated" or "not recognised"; the text of its own log is the authority.

If MACE is **not** enabled and an association request carries authentication fields, they are ignored (as before) and one warning is logged per association: `the association request carries authentication fields, which are ignored because MACE is not enabled`.

### The time window and clocks

The signing time is UTC, whole seconds, taken from the system clock at the moment the association request (or response) is made. The receiver compares it with its own UTC clock and refuses a value more than `TimeWindowSeconds` (600 s by default, which is also the
value a real-world peer was seen configured with) in the past **or the future**. So **the two clocks must agree within the window** (NTP or equivalent; the time zone does not matter, only UTC does). The log says so:

`MACE: signing time outside the window (the peer's clock differs by 700 s; the signing time is 2026-10-21T04:05:47Z, the window is 600 s)` (the difference is our clock minus the signing time; negative: the peer's clock is ahead).

The certificate's validity period is judged at the same clock. The signature does not protect against a replay inside the window: someone who recorded an authenticated association request can present it again for `TimeWindowSeconds` (no secret is exposed, but the replay authenticates as the sender
until the window passes). Use a short window and TLS (which a replayed AARQ cannot get through) where that matters.

### Key sizes and certificates

Only **RSA** keys are supported (the signature is PKCS#1 v1.5 RSA): an EC key, ours or the peer's, is refused with `... is not an RSA key: MACE supports RSA keys only`. Keys of `MinKeyBits` (2048) to `MaxKeyBits` (8192) are accepted;
a real-world peer documents 8192 bits as its maximum for MACE. The certificate is sent as DER, leaf only (a 2048 bit RSA certificate is about 1000 octets), so the AARQ and the AARE are far larger than the usual ones, about 1.4 KB: the session
CN and AC carry that much user data (see quirk 44 for the form of the accept). Certificates are made as for TLS (`ExampleConfig/ICCP-MACE/make-certs.sh` or `.ps1`; the key usage the peers expect is not known, none is required here).

### What the log and status show

At `info`:

```
NodeA: MACE: authenticating with certificate CN=iccp-client.example,OU=ICCP Clients,O=Example Utility,ST=New South Wales,C=AU, signing time 2026-10-21T04:05:47Z
NodeA: MACE: peer authenticated: CN=iccp-server.example,OU=ICCP Servers,O=Example Utility,ST=New South Wales,C=AU, signing time 2026-10-21T04:05:47Z
NodeA: association established
```

Key material is never logged. `GetStatus` has a `MACE` object: `Enabled`, `State` (`None`: no authentication done; `Sent`: ours sent, the peer's not verified; `Verified`; `Failed`), `PeerSubject`, `PeerNotAfter` (the peer certificate's expiry), `SigningTime` (ours) and
`PeerSigningTime`, and `LastError` (the reason of the last failure). It shows the current or last association.

### Troubleshooting

| Log line (abbreviated) | Meaning |
|------------------------|---------|
| `refusing the association: MACE: the association request carries no authentication ("RequireAuthentication" is true)` | The peer sent an unauthenticated AARQ to our MACE responder. Enable MACE on the peer, or set `RequireAuthentication` false. |
| `association refused by the peer (... authentication-required (14))` (or `called-AE-qualifier-not-recognized (9)` from the one real-world peer seen) | We sent no authentication and the peer requires it: enable MACE here. |
| `MACE: the association response carries no authentication ("RequireResponderAuthentication" is true)` | We (the initiator) authenticated, and the peer's response does not. Peers that do not authenticate their responses need `RequireResponderAuthentication` false. The association is aborted. |
| `MACE: the peer's certificate "CN=..." is not acceptable: unable to get local issuer certificate (depth 0)` | The peer's issuer is not in `CAFile`/`CAPath`. The peer sends the leaf only: add its intermediate CAs to the bundle as well. |
| `...certificate has expired` / `...certificate is not yet valid` / `...certificate revoked` | The certificate is outside its validity at our clock, or is on the CRL. |
| `MACE: signing time outside the window (the peer's clock differs by N s ...)` | The clocks differ by more than `TimeWindowSeconds`: synchronise them (UTC), or widen the window. |
| `MACE: the peer's signature is not valid for its certificate "CN=..." and signing time` | The signature does not verify with the certificate: the peer signs something else (another scheme), the key does not match the certificate, or the APDU was altered. |
| `MACE: the peer's mechanism-name X is not the one IEC 62351-4 defines (1.0.62351.4.3.1): ... accepted, because the standard asks implementations to be tolerant of it` (warning, once per association) | The peer uses another mechanism-name, usually the legacy `1.0.840.0.1.0.1.1` of IEC TS 62351-4:2007. Expected with such a peer and harmless: the association is accepted and authenticated. Set `MechanismName` to the peer's value to send the same value too (that gives the configuration warning below). |
| `"MACE": the configured "MechanismName" X is not the mechanism-name IEC 62351-4 defines (1.0.62351.4.3.1); use it only for peers that use that value` (warning when the port is built) | `MechanismName` is set to something other than the default. Right only for a peer that uses that value; leave the key out otherwise. |
| `association refused by the peer (... authentication-mechanism-name-required (12))` / `MACE: the peer sent an authentication value without a mechanism-name` | An authentication value without the mechanism-name component, which the standard requires to be present. |
| `configuration error: "MACE": "MechanismName" must be a dotted object identifier ...` | `MechanismName` is given but is not a dotted OID (for example `1.0.62351.4.3.1`). Remove it to use the default. |
| `MACE: the peer's ... RSA key has 1024 bits, outside the accepted 2048 to 8192` / `... is not an RSA key` | `MinKeyBits`/`MaxKeyBits`, or an EC certificate. |
| `MACE: the peer's certificate "CN=..." is not in PeerCertAllowList` | The certificate verifies but is not listed. |
| `configuration error: "MACE": this build has no MACE support: rebuild with ODC_ASIO_SSL=ON ...` | The binary was built without OpenSSL. |

### What is verified `[V]` and what is not `[S]`

`[V]`, decoded from a real peer's own association (it authenticating to itself, plain TCP, both directions, RSA 2048, mutual authentication, then data both ways and a clean release) and checked by tests that run against that capture locally (not committed):
the field numbers and tags of the AARQ and the AARE (above), the authentication value's nesting, tags and lengths, that the responder's AARE carries the same kind of value with the same mechanism-name, the signature scheme (the public operation gives the PKCS#1 type 1 block with the SHA-1 DigestInfo
header and the time text) **for both directions**, that the signing time is UTC, that the encoder given the same sizes produces the same nesting and lengths (a skeleton comparison), and that the APDUs the stack decodes re-encode byte for byte.
The same peer's initiator **requires** the responder to authenticate (it aborts with an ACSE ABRT when the response has none), which is why `RequireResponderAuthentication` defaults to true.

`[S]`, not verified against a peer other than that capture:

* that a peer takes our failure diagnostics (12, 13, 14) as we mean them, and the refusal's transport (session REFUSE). A real peer has accepted our association **request** (our initiator role) and our **response** (our responder role), with and without TLS, `[V]`; a request carrying a mechanism-name that peer did not expect was refused with diagnostic 9 before the mechanism-name was made tolerant (we now accept any value, see [The mechanism-name](#the-mechanism-name));
* the mechanism-name: that the official value is `1.0.62351.4.3.1` and that the legacy `1.0.840.0.1.0.1.1` is tolerated is `[T]` (IEC 62351-4 clause 11.1.4.3), and that a real peer sends the legacy value `[V]`; that a peer on the legacy value accepts our response echoing it, and one on the official value accepts our response when we are configured with another, is `[S]`;
* the A-security abstract syntax (`1.0.840.0.1.1.4.1`, clause 11.1.3): the responder's acceptance of it and the `identification` in the authentication value are `[S]` (nothing like it has been seen on the wire);
* that the signature covers only the signing time and nothing else (it is what the captures show);
* that `Verified` on one end means the same to the other (no acknowledgement exists);
* which key usages and extensions a real peer puts in its certificates or insists on (no purpose is checked here), and a CRL or the allow list against a real peer's PKI;
* a clock check against a peer in another time zone (the signing time is always UTC);
* MACE together with TLS against a real peer: both roles work, `[V]` (the TLS/MACE, TLS/None, TLS/SSL, None/MACE and None/None combinations were tried with our port as the server).

## Troubleshooting a data set the peer refuses


MMS errors and rejects are logged by name with the numbers kept in brackets (`definition [2] / object-undefined [1]`); the names are ISO 9506-2 as remembered, a few are marked unverified in `MmsNames.cpp`.
When the peer refuses to define a data set, the client logs **one** error line naming the operation, the data set and domain, the member count, the first and last member and the error:

```
[error] ICCP: DefineNamedVariableList failed for data set DS_X (domain DOMAIN_A), 129 members, first "Transfer_Set_Name", last "POINT_0129_MW": service error: definition [2] / object-undefined [1]
```

The same line shape is used for `Next_DSTransfer_Set` and for starting the transfer set (`start transfer (Write of the DSTransferSet) failed for transfer set TS_1 (domain DOMAIN_A) for data set DS_X ...`).
A failed delete of a data set that does not exist is the normal case on a fresh peer and only a debug line.

After a *definition class* error a **probe** finds out which members are to blame: one MMS Read of the data set's points as a list of variables, in chunks of `ProbeChunkSize` (one Read outstanding at a time,
at most once per data set per association, in parallel with the provisioning of the other data sets). The special objects at the head of the data set (`Transfer_Set_Name` and so on) are not read:
a peer need not serve them as variables, so they are counted as present. A member whose Read gives `object-non-existent` or `object-undefined` is *missing*; any other data access error means it
exists but is not readable (it is kept in the data set).

With `MissingMemberPolicy` `"Prune"` (the default) the data set is then defined again without the missing members (if the peer says `object-exists`, it is deleted and defined once more), and everything carries on
with the pruned definition: reports and polled reads are mapped to the points through the pruned member list, so the shifted positions do no harm. Every missing member is logged, individually, as a warning:

```
[warning] ICCP: DefineNamedVariableList failed for data set DS_X (domain DOMAIN_A), 129 members, first "Transfer_Set_Name", last "POINT_0129_MW": service error: definition [2] / object-undefined [1]
[warning] ICCP: probing the members of data set DS_X (domain DOMAIN_A) on the peer to find which ones it rejects: 127 points in Reads of up to 50
[warning] ICCP: data set DS_X: member POINT_0012_MW (ICC, domain DOMAIN_A; ODC Analog index 11) does not exist on the peer; flagged COMM_LOST
[warning] ICCP: data set DS_X: member POINT_0020_MW (ICC, domain DOMAIN_A; ODC Analog index 19) does not exist on the peer; flagged COMM_LOST
[warning] ICCP: probe of data set DS_X (domain DOMAIN_A): 129 of 129 members probed, 2 missing, 127 present
[info]    ICCP: data set DS_X defined with 127 of 129 members (2 missing on the peer left out)
[info]    ICCP: transfer set TS_1 started for data set DS_X
[warning] ICCP: provisioning finished: 3 data sets ok (1 partial), 0 failed; 2 points flagged COMM_LOST (not available on the peer)
```

All missing members are listed: there is no cap. The point of each missing member is published **once** per association, after the provisioning finished, as an event of its configured kind and ODC index with a neutral value
(binary false, analog 0, counter 0), the quality `MissingMemberQuality` and the current time, and is never published with a value afterwards (a peer that reports it anyway is ignored for that point).
`COMM_LOST` without `ONLINE` was chosen because it means "no data from the source", and is what `Tase2Quality.h` maps to `NotValid` when the event is forwarded to another ICCP port; it can be changed with `MissingMemberQuality`.
The port does not publish per point events when the association is lost (as for all points): the `DISCONNECTED` connection state tells ODC. The probe is not repeated while the association lasts; a new
association starts the whole provisioning again, so a point that has since appeared on the peer is picked up then.

If the probe cannot finish (the peer rejects it, answers with an error or garbage, times out, or the association ends), if the pruned definition is refused too, or if no point of the data set exists on the peer,
the data set is skipped as before (state `define-failed`, an error line with the reason) and **all its points** that no successfully provisioned data set carries are flagged the same way, one warning each:

```
[error]   ICCP: data set DS_X (domain DOMAIN_A) is skipped: every point of the data set is missing on the peer
[warning] ICCP: data set DS_X: member POINT_0001_MW (ICC, domain DOMAIN_A; ODC Analog index 0) is not available because the data set could not be provisioned; flagged COMM_LOST
[error]   ICCP: provisioning finished: 2 data sets ok, 1 failed: DS_X; 129 points flagged COMM_LOST (not available on the peer)
```

With `MissingMemberPolicy` `"Fail"` nothing is flagged and nothing is defined again; the probe lines are errors, one per missing member with its position:

```
[error] ICCP: missing member 19 of 129 (position 18, 0-based) in data set DS_X (domain DOMAIN_A): POINT_0012_MW (ICC, domain DOMAIN_A; ODC Analog index 11) - object-non-existent [10]
```

A probe whose result is "every member exists" means the definition was refused for another reason (the data set name may already be in use on the peer, or a limit was reached); the same applies if the peer
refuses a data set because it lacks a special object, which the probe does not test. `TraceHex` shows the exchange.

## Migration from the previous schema

The previous design tied the role to the port type: `ICCPServer` was the TASE.2 server with `TransferSets: [{"Name": ...}]` as its pool, and `ICCPClient` the TASE.2 client with points that were always read.
Existing configurations keep working unchanged: with no `ServiceRole`, `ICCPServer` is a `Server` and `ICCPClient` a `Client`, points default to `Serve` / `Read` accordingly, and `TransferSets` of a `Server`
are still the pool. What changed:

* New `ServiceRole` key, valid for both port types; `Peer` is new.
* New per-point and per-data set `Direction` (`Serve` / `Read`); required for `Peer`, defaulted otherwise.
* New `TransferSetPool` for the names a port offers; `TransferSets` means "start on the peer" (needs `DataSet`) except in the legacy `Server` form above.
* A server configuration run under `ICCPClient` (or the reverse) used to fail with schema errors such as `TransferSets[0]: "DataSet" must name a configured data set`. The schema is now the same for both:
  set `"ServiceRole": "Server"` (and keep `TransferSets` as the pool, or move it to `TransferSetPool`) and the file works under either port type. Config errors now say which role or direction rule was violated and how to fix it.
* A `Server` file moved to `ICCPClient` without `ServiceRole` becomes a `Client` and its `Serve`-style content is rejected with a message pointing at `ServiceRole`.
* `Domain` defaults now follow the point's direction (`LocalDomain` for `Serve`, `RemoteDomain` for `Read`), which is what the old server/client defaults were.
* A data set the peer refused because it lacks members used to be skipped with an error. The default `MissingMemberPolicy` is now `"Prune"`: the data set is defined without the missing members and their points are
  flagged `COMM_LOST` (see Troubleshooting). Set `"MissingMemberPolicy": "Fail"` for the old behaviour.

## Supported implementation quirks

Real-world ICCP peers differ from the standards in several ways, and the standards leave some choices open. This table lists what the implementation accommodates or reproduces.

* **Mode**: *decode* = tolerated when received; *emit* = also produced by this implementation; *both*.
* **V** = seen in real traffic (packet captures of real associations); **T** = taken from the text of the standard (IEC 60870-6-503 / -802, the clause is cited), not yet seen in real traffic; **S** = an assumption, a design choice or a deliberate deviation, not confirmed by a peer or the standard text.

| # | Quirk (one line) | Layer | Mode | Configurable | |
|---|------------------|-------|------|--------------|---|
| 1 | The ACSE protocol-version field is always written, although it carries the default value. | ACSE | emit (decode accepts both) | No | V |
| 2 | Presentation-requirements is always written as a 2 bit string, though decoders accept the short and the bit string forms. | Presentation | emit (decode accepts both) | No | V |
| 3 | Context ids 1 (ACSE) and 3 (MMS) are proposed as peers do, but the ids are taken from the negotiated result, never hardcoded. | Presentation | both | No | V |
| 4 | COTP CR/CC parameters are in the order calling TSAP, called TSAP, TPDU size; the TPDU size proposed is 1024 and the smaller of the two ends is used (class 0 only). | COTP | emit | `TPDUSize` | V |
| 5 | The session CN carries protocol options and version 2 in one parameter group, requirements = duplex, then the selectors, then the user data, in that order. | Session | emit | No | V |
| 6 | On decode the placement of version, requirements and user data in the session CN/AC is accepted leniently. | Session | decode | No | V |
| 7 | The ACSE user-information EXTERNAL uses an indirect-reference only (no direct-reference) and a single-ASN1-type encoding. | ACSE | both | No | V |
| 8 | MMS booleans are sent as `0x01` for true; `0xFF` and any non-zero value are accepted. Received data is re-encoded exactly as it arrived. | MMS | both | No | V |
| 9 | Non-minimal integers and indefinite-length encodings are accepted. | BER / MMS | decode | No | V |
| 10 | In a GetNameList response `moreFollows` is omitted when TRUE (its default); an absent value is read as TRUE. | MMS | both | No | V |
| 11 | `Supported_Features` is sent as a 12 bit string, as peers do; a string of any length is accepted. | TASE.2 | both | `SupportedBlocks` | V |
| 12 | The `DSTransferSet` has 12 members on the 1996-08 edition and 13 on 2000-08, which adds `AllChangesReported` between `RBE` and `Status`. The fourth boolean of the 12 member form is `Status` (TRUE to start): it is TRUE in every 1996-08 capture, and the 1996-08 edition has no `AllChangesReported` (it must be false). Both forms are accepted. A 2000-08 peer refused the 12 member form with data access error `type-inconsistent` (7), and writes the 13 member form itself. | TASE.2 | both | `DSTransferSetStatusMember` (follows the peer's `TASE2_Version` unless given), `RequireMatchingEdition` | V (12 on 1996-08, 13 on 2000-08 as the peer writes it) / S (our 13 member write accepted by a peer; the member assignment of the 12 member form is inferred from the captures and the rule that `AllChangesReported` is false in 1996-08) |
| 13 | Reports name their members either inline (a list of variables) or by data set name; both are parsed. Full reports from the server name the data set when its definition starts with the special objects; RBE reports are inline with only the changed members. | TASE.2 | both | `RBE` | V (both parsed) / S (RBE inline) |
| 14 | The head of a report varies with the data set definition (`Transfer_Set_Name` first, then `DSConditions_Detected`, `Event_Code_Detected` or a time stamp); the layout is taken from the data set. | TASE.2 | both | `SpecialObjects` | V |
| 15 | A peer acts as client and server on one association: with `ServiceRole` `Peer` both sides issue and serve requests at once (a `Client` serves an empty model, answering with an error where there is nothing to serve). The previous design always did this on both port types and was tested that way against a real-world peer. | Stack / TASE.2 | both | `ServiceRole` | V (previous design) / S (the new role split) |
| 16 | A data set is deleted before it is defined, and any answer to the delete (including "does not exist") is accepted. | TASE.2 (client) | emit | No | V |
| 17 | Peers seen use the 1996-08 edition (version 1996/8) and, in a later capture, 2000-08 (2000/8, read from `TASE2_Version`); the default here is 2000-08. The edition sets the version advertised; both are read. | TASE.2 | both | `TASE2Edition` | V (1996 and the peer's 2000 version) / S (our 2000 behaviour beyond quirk 12) |
| 18 | Analog values are MMS floating point in the 32 bit form (`87 05 08` + IEEE 754); the general MMS format is also decoded. | MMS | both | No | V |
| 19 | The point type comes from the configuration, because `{integer, flags}` is ambiguous between several types. | TASE.2 | both | `Type` | V |
| 20 | The quality mapping is lossy in both directions; the raw flags octet is kept with the value. | TASE.2 | both | No | S |
| 21 | `Bilateral_Table_ID` is read in the peer's ICC domain, `TASE2_Version` and `Supported_Features` at VCC (VMD) scope. | TASE.2 | both | `RemoteDomain` | V |
| 22 | Time tags (`...QTimeTag`) are whole seconds; milliseconds only in the `Extended` types. | TASE.2 | both | `Type` | S |
| 23 | Release is an MMS Conclude, then session FINISH (ACSE RLRQ) and DISCONNECT (RLRE). A peer FINISH without a Conclude, or a transport close after the Conclude, is also accepted. The sequence is the one a real peer was seen to use (see 45). | Stack | both | `ReleaseTimeoutms` | V (Conclude, FINISH with RLRQ, DISCONNECT with RLRE) / S (the rest) |
| 24 | If both ends conclude at once, the association's initiator (the TCP initiator) wins and the responder gives way (a Conclude collision). | Stack | both | No | S |
| 25 | Stop Transfer is a write of the `DSTransferSet` with `Status` false (in the 12 or 13 member form, as for the start). | TASE.2 (client) | emit | No | S |
| 26 | The first `IntegrityTimeOut` report of a transfer set is sent at once when it starts. A transfer set that does not request `IntegrityTimeOut` sends nothing until its conditions occur. | TASE.2 (server) | emit | `IntegrityCheck` | S |
| 27 | `Critical` is honoured on both sides: the client acknowledges the reports of a transfer set it started with `Critical` true, the server tracks the acknowledgements. (This row used to say that no acknowledgement was implemented.) See quirks 56 to 58. | TASE.2 | both | `Critical`, `AcknowledgeCriticalReports`, `ReportAckTimeoutms`, `MaxUnacknowledgedReports` | T / S (see 56 to 58) |
| 28 | `BlockData` (TASE.2 Block 3, blocked transfers) is sent by the client when configured, and honoured by the server **only when 3 is in `SupportedBlocks`**: then the reports of that transfer set are sent in the blocked form (quirks 66 to 70); with the default `[1, 2]` the flag is accepted and ignored and the reports are the ordinary ones, unchanged. The client decodes blocked reports (quirk 70). Rule 0 and rule 1 with `RealQ` points are as a real peer sends them; see 66 to 70 for what is and is not seen. (`TLE` is no longer ignored: quirk 58.) | TASE.2 | both | `BlockData`, `SupportedBlocks`, `BlockedDataName` | V / S (see 66 to 70) |
| 29 | MMS Initiate: the smaller of both ends' outstanding-request limits applies; requests beyond the peer's limit are queued, not sent. | Stack | both | `MaxOutstanding*` | V (Initiate fields) / S (queuing) |
| 30 | A malformed unconfirmed PDU is logged and ignored; a malformed confirmed request with a readable invoke id is rejected; other malformed input aborts the association. | Stack | decode | No | S |
| 31 | `Next_DSTransfer_Set` with no free transfer set is answered with a data access error (object non-existent). Deleting an unknown data set is a service error (object undefined). | TASE.2 (server) | emit | `TransferSets` pool | S |
| 32 | With RBE set, the report at an `IntervalTimeOut` carries only the members changed since the last report, named inline; an interval with no change sends nothing. Integrity reports stay complete. | TASE.2 (server) | emit | `RBE` | V (inline subsets at the interval) / S (nothing when unchanged) |
| 33 | Data sets a client creates are kept only for the association; a peer that expects them to still exist on the next association (and to delete and redefine them) gets "object undefined" on the delete. | TASE.2 (server) | emit | No | V (peers keep them) |
| 34 | The association initiator follows the TCP direction, not the service role: the TCP initiator sends the AARQ, even if its service role is `Server`. | Stack | both | `Type` (port type) | S |
| 35 | `Peer`: invoke ids are allocated per direction and both sides are active at once from the moment the association is up; the peer's requests are answered while this end verifies the peer and provisions its data sets. | Stack / TASE.2 | both | `ServiceRole` | S |
| 36 | A data set the peer refuses to define because it lacks members is defined again without them (probe: a Read of the points as a list of variables; `object-non-existent` / `object-undefined` = missing), and the points of the missing members are flagged with a quality instead of values. | TASE.2 (client) | emit | `MissingMemberPolicy`, `MissingMemberQuality` | S |
| 37 | The special objects (`Transfer_Set_Name`, `DSConditions_Detected`, ...) are not read by the probe: a peer is assumed to define them as data set members without serving them as variables. A peer that refuses a data set for lack of one of them is not diagnosed by the probe. | TASE.2 (client) | emit | No | S |
| 38 | The MMS Reject reason numbers follow ISO 9506-2 as remembered (confirmed-requestPDU: unrecognized-service 1, invalid-invokeID 3, invalid-argument 4, max-serv-outstanding-exceeded 6, ...); earlier versions of this port sent other numbers for the last three. | MMS | emit | No | S |
| 39 | Block 5: a device `D` is the write only variable `D` (an integer of width 16, or a 32 bit float with exponent width 8 for a real setpoint) and the variable `D_SBO` (an integer of width 16), both in the device's domain. The **select is a Read of `D_SBO`** (the answer is the device's configured CheckBackID, the same every time); the **operate is a Write of `D` whose data is the bare value**, with no structure and no CheckBackID. There is no Write of `D_SBO` and no Read of `D` (both are refused as `object-access-denied`). A real peer does exactly this in its automatic mode (Read of `D_SBO`, then the Write of `D`) and in its direct mode (the Write of `D`), and writes a 32 bit float even to a device configured as discrete (a 32 or 64 bit float is accepted for a real setpoint, an integer or unsigned for the others, provided it fits the width 16: -32768 to 65535, else `object-attribute-inconsistent`; the client side writes a signed integer, -32768 to 32767). | TASE.2 (Block 5) | both | `BinaryControls`, `AnalogControls` | V (the Read then Write flow, the bare values, a float for a setpoint) / T (names, types and the rest of the model: 503 clauses 5.3.12, 7.2.6; 802 clauses 5.2.3, 6.2.2, 7.1.2; the width 16 of the integers: 802 clause 6.2.2) / S (the unsigned half of the range accepted, to -32768 to 65535) |
| 40 | Block 5 results are MMS data access errors of the Read (select) or the Write (operate) (never service errors), mapped as in the table in [Device control](#device-control-tase2-block-5): the select fails with `temporarily-unavailable` (armed, busy) or `hardware-fault`; the operate with `temporarily-unavailable` (not armed by this association, or busy) or `hardware-fault` (the device did not carry the command out); the others are our choice. A peer that uses other codes is mapped by the client side's table, and one that answers with a service error or a Reject is `NOT_SUPPORTED`. | TASE.2 (Block 5) | both | No | T (the codes of 503 clauses 7.2.6.2.3 and 7.2.6.3.3) / S (type-inconsistent, type-unsupported, object-attribute-inconsistent (which is also the answer to an integer outside the width 16, -32768 to 65535) and the access denied of an undefined access) |
| 41 | Block 5 is the fifth bit of `Supported_Features` (bit 4, bit 0 being the first): advertised when devices are served, required of the peer when devices are operated. | TASE.2 (Block 5) | both | `SupportedBlocks`, `RequiredRemoteBlocks` | S |
| 42 | The select (the Read of `D_SBO`) arms the device (idle to armed) for `SelectTimeoutms`; an operate uses it up and the timeout lapses it; a second select of an armed or operating device is `temporarily-unavailable`; an operate of a device that needs a select and is not armed **by this association** is `temporarily-unavailable` too (the state is per association). The CheckBackID returned by a select is the device's configured `CheckBackID` (default 1), fixed, with no range imposed; the client side can require a given value (`CheckBackID` of an `Operate` control) and then does not write if another one comes back. A device with `SelectRequired` false is lenient: it takes an operate with or without a select, and a select still arms it (the standard says a select of a non-SBO device fails). | TASE.2 (Block 5) | both | `SelectRequired`, `SelectTimeoutms`, `CheckBackID` | T (the state machine, the fixed CheckBackID of the bilateral table) / V (a real peer refuses to write when the CheckBackID read is not one it accepts) / S (the leniency; the rule of a real client for accepting a CheckBackID is not known, so none is imposed) |
| 43 | Block 5, what is **still not implemented**: the MMS Event Condition / Event Notification mapping of the device actions Timeout, Success, Failure and Local Reset (`<device>_time`, `_success`, `_fail`, `_reset`; they belong to MMS event management, i.e. Block 7, which this port does not support, and the event enrollment and condition models are informative only in the standard), so none of these four actions is signalled to the peer (a select still lapses at its timeout, and the Local Reset has its state effect, quirk 63; Success and Failure are the operate's own answer); and the readable `SBO` structure variable (State, Timeout) and `ControlConfig` variable (ControlPointType, SetPointType, DeviceClass, TagClass) of IEC 60870-6-802 clauses 5.2.3, 6.2.2 and 7.1.2, because the standard gives no MMS variable names for them. (`<Device>_SBO` is the CheckBackID, as a real peer uses it: quirk 39. The device tags, formerly listed here, are implemented: quirks 60 to 65.) | TASE.2 (Block 5) | - | No | T (the actions are defined by 503 clauses 5.3.12.7 to 5.3.12.10 and 7.2.6.4 to 7.2.6.7 and the variables by 802 clause 5.2.3, but not built) |
| 44 | A session ACCEPT whose user data is longer than 512 octets (a MACE response is about 1.4 KB) carries it in PI 193 (user data) with the 3 octet length form (`c1 ff xx xx`), as a real peer does, not in PI 194 (extended user data), which our CONNECT uses for the same case (and the peer does too). The decoder accepts both in both SPDUs. | Session | emit (decode accepts both) | No | V |
| 45 | The release exchange of a real peer: the initiator sends an MMS Conclude request, the responder the response; the initiator sends a session FINISH whose user data is a presentation PDV (context 1, ACSE) with an RLRQ, reason `urgent` (1); the responder answers a DISCONNECT with an RLRE, reason `not-finished` (1); the initiator closes the TCP connection first. We send the same SPDUs and PDVs with reason `normal` (0) in both and close both ends after the DISCONNECT; any reason is accepted. | Stack | both | No | V (the peer's sequence and reasons, accepted) / S (that a peer accepts our reason values) |
| 46 | MACE: the AARQ carries sender-acse-requirements [10] (`authentication`), mechanism-name [11] and calling-authentication-value [12], the AARE [8], [9] and [10], each value the nested `external [2]` structure of the certificate (DER), signing time (GeneralizedTime, UTC) and signature (raw RSA). The mechanism-name is the configured value (default the official `1.0.62351.4.3.1`, IEC 62351-4 clause 11.1.4.3); a received value that differs is accepted with a warning when it is not the official one, and the AARE echoes the value received when it differs from ours. A real peer sends the legacy `1.0.840.0.1.0.1.1`. | ACSE | both | `MACE` (`MechanismName`) | V (the structure, the peer's legacy value) / T (the two values, the tolerance) / S (the echo) |
| 47 | MACE: the signature is RSA with PKCS#1 v1.5 type 1 padding over the 15 octet SHA-1 DigestInfo header and the 15 octets of the time text: **no hash**, the lengths in the header do not match what follows. Made with no digest set, verified with the public operation. | MACE | both | No | V |
| 48 | MACE: the initiator requires the responder to authenticate (a real peer aborts with an ACSE ABRT, source service-user, when the response has none); a MACE responder authenticates in its response when the request authenticated. | ACSE | both | `RequireResponderAuthentication`, `RequireAuthentication` | V (the peer's initiator requires it) / S (our responder side) |
| 49 | A real peer in MACE mode refused an association request without authentication with a session REFUSE carrying an AARE `rejected-permanent`, diagnostic `called-AE-qualifier-not-recognized` (9), although the AE-qualifier was right; we send `authentication-required` (14) for that and read both. | ACSE | decode | No | V (9) / S (14) |
| 50 | A 2000-08 peer refuses a `DSTransferSet` with `AllChangesReported` TRUE unless RBE is TRUE, the `ObjectChange` condition is requested and `BufferTime` is above 0 (data access error `object-attribute-inconsistent` [8]); it writes `AllChangesReported` FALSE itself, with the 13 member form (quirk 12). The 1996-08 form has no such member. The default is FALSE, the rule is checked when the configuration is loaded, and the failure line names the values sent. | TASE.2 (client) | emit | `AllChangesReported`, `BufferTime`, `RBE`, `Conditions` | V (refused with `BufferTime` 0) / S (the rest of the rule, as reported; accepted when it is met) |
| 51 | Block 5: what a real peer does to a device, as captured: its *automatic* commanding mode is a Read of `D_SBO` (an integer comes back) and, only if the client accepts that integer, a Write of `D` (domain-specific, the item id is the device name) whose data is a **bare value**; its *direct* mode is the Write alone. A command is an integer, a real setpoint a 32 bit float (FloatingPoint [7], `87 05 08` + IEEE 754 single; 100.0 is `87 05 08 42 C8 00 00`), and that client wrote a float even to a device that was configured as discrete. It never writes `D_SBO`. This port's server side takes exactly this (see quirk 39: a 32 or 64 bit float for a real setpoint, an integer or unsigned of the width 16, -32768 to 65535, for the others; a float to a discrete device is `type-inconsistent`, an integer outside the width `object-attribute-inconsistent`), and its client side does the same Read then Write, or the Write alone. That client refused to write for some of the CheckBackIDs it read (and, for one device, for one that was not the value it expected) and took others; its rule is not known, so a served device returns its configured `CheckBackID` without any range being imposed. | TASE.2 (Block 5) | both | `BinaryControls`, `AnalogControls` (`SelectRequired`, `CheckBackID`) | V (the flows, the bare values, the CheckBackID refusals) / S (that a float to a discrete device is refused here; the rule of the client for a CheckBackID) |
| 52 | MACE: the A-security presentation context abstract syntax `1.0.840.0.1.1.4.1` (IEC 62351-4 clause 11.1.3) is answered with `acceptance` (BER) by a MACE responder when a request offers it, and the responder then writes the context identifier as the `identification` INTEGER of its authentication value (clause 11.1.4.4); the decoder reads the value with or without it. Any other unknown abstract syntax, and this one on a responder without MACE, is rejected for that context alone (`provider-rejection`, `abstract-syntax-not-supported`), as before. Our own requests do not offer it. | Presentation, ACSE | responder (decode accepts both) | No | T (the clauses) / S (never seen on the wire) |
| 53 | Server: a transfer set whose `DSTransferSet` has `AllChangesReported` TRUE (with `RBE`, `ObjectChange` and a `BufferTime` above 0) reports every change of a point in the buffer time. IEC 60870-6-503 clauses 5.3.10.2.2 and 7.2.5.5.3 do not say how the several values of one object are laid out, so this port sends **consecutive reports in the order of the changes, each an ordinary report by exception that lists a point at most once** (a new report starts where a point would repeat), rather than one report that lists the object several times: a client that keeps one value per name per report cannot lose a value, every report is one a client already understands, and none is larger than a report of all changed members. Each value carries the quality and time stamp it had when it changed. Buffered changes go out before an integrity, operator request or other external event report, and with an interval report; the buffer is capped by `MaxBufferedChanges` (last value wins beyond it) and is cleared with the transfer set and the association. With `false`, `BufferTime` 0 or `RBE` false nothing changes. | TASE.2 (server) | emit | `MaxBufferedChanges` (and the transfer set's `AllChangesReported`) | T (that all changes of an object are reported, 503 clause 5.3.10.2.2) / S (the layout, the order with the other report kinds, the cap: not verified against a peer; whether a real client prefers the object repeated inside one report is not known) |
| 54 | The integers of a control device are of width 16 (`Control_Command`, `Control_Setpoint_Discrete`, `SBO_CheckBackName`: 802 clause 6.2.2; `Control_Setpoint_Real` is a 32 bit float). Server: an operate whose integer is outside -32768 to 65535 (signed or unsigned encoding of the width) is refused with `object-attribute-inconsistent` [8]. Client: a command value or a `DiscreteSetpoint` value (scaled, rounded) outside -32768 to 32767 (the integer is written signed) is refused before it is sent with `OUT_OF_RANGE`; `CommandValueOn` and `CommandValueOff` are validated to that range. `CheckBackID` keeps -32768 to 65535. | TASE.2 (Block 5) | both | `CommandValueOn`, `CommandValueOff` | T (the width: 802 clause 6.2.2) / S (the data access error, the unsigned half accepted by the server, the signed range written by the client) |
| 55 | `Bilateral_Table_ID` is of the type `Visible-String-32` (503 clauses 8.2.2 and 8.3.3): `BilateralTableID` and `RemoteBilateralTableID` are validated to at most 32 characters of visible ASCII (0x20 to 0x7E); the type advertised for it is a visible string with the size -32 (varying, at most 32); a longer string returned by a peer is logged as a warning (once per association) and compared as it is, without dropping the association. | TASE.2 | both | `BilateralTableID`, `RemoteBilateralTableID` | T (the type and size) / S (varying rather than fixed length in the advertised type; the tolerance of a longer string) |
| 56 | Client: the acknowledgement of a report of a transfer set started with `Critical` true is an unconfirmed `InformationReport` whose variable access specification lists the domain-specific variables `Transfer_Report_ACK` (the transfer set's object name, as in the report, the structure `{scope, domain, name}` that `Transfer_Set_Name` has) and `Transfer_Set_Time_Stamp` (the report's time stamp, an integer), both in the domain of the transfer set; with no time stamp in the report that variable is left out. The negative acknowledgement is the same with the one variable `Transfer_Report_NACK`. | TASE.2 (client) | emit | `AcknowledgeCriticalReports` | T (the variable names, what they hold, and when each is sent: 503 clauses 5.3.10, 7.2.5.5.2) / V (a real peer's acknowledgement of a report without a time stamp: an unconfirmed `InformationReport` with the one variable `Transfer_Report_ACK` in the domain of the transfer set, whose value is the `{scope, domain, name}` structure of `Transfer_Set_Name`; no reply; this port's acknowledgement of that peer's critical reports, with the time stamp variable added, was followed by no `Transfer_Report_NACK` and no association loss over 25 s) / S (the two variable form with `Transfer_Set_Time_Stamp` has not been sent by a real peer, and whether the peer treats ours as correct is not visible in the capture; the negative acknowledgement has not been seen) |
| 57 | Server: for a critical transfer set each report sent is remembered (transfer set and `Transfer_Set_Time_Stamp`) until a matching `Transfer_Report_ACK` arrives. A match is the oldest report with an equal time stamp (a report or an acknowledgement without one matches the oldest). Receipt is lenient (any variable order, other variables ignored, an unreadable time stamp counts as none, the scope and domain of the variable names not checked). A `Transfer_Report_NACK`, no acknowledgement within `ReportAckTimeoutms` (default 10 s, 0 = never), or more than `MaxUnacknowledgedReports` (default 100) outstanding reports (the oldest goes) is a `Data_failure`: logged as a warning once per report and counted in `DataFailures`, as this port has no event conditions. A failure never resends a report, holds reports back or touches the association. The remembered reports are dropped, uncounted, when the transfer set is stopped, started again or loses its data set, and when the association ends. Transfer sets that are not critical are not tracked and send exactly what they did before. | TASE.2 (server) | both | `ReportAckTimeoutms`, `MaxUnacknowledgedReports` | T (the acknowledgement and `Data_failure` rule: a missing or negative acknowledgement) / V (a real peer acknowledges each report of a critical transfer set, one acknowledgement per report, within a few milliseconds, and its acknowledgement of a report that has no time stamp has none either, which the matching rule handles) / S (the timeout and its default, the cap, the leniency, no resend or holding back, logging and counting instead of the event condition, a NACK for a non-critical transfer set being only logged and counted; what a real peer does when a report is not acknowledged) |
| 58 | Server: `TLE` (seconds, 0 = off) is enforced as far as the stack allows. A report handed over more than `TLE` after it was due (the end of the buffer time, the interval or integrity time) is discarded, logged as a warning and counted (`ReportsDiscardedTLE`), and the next report of that transfer set is sent whatever its age. Reports sent as soon as they arise are never late. The stack has no transport back-pressure, so only the lateness of the port's own tick is measured; a slow TCP link is not seen. With `RBE` the changes of a discarded report are lost until the next report that carries them, and with `AllChangesReported` each of the consecutive reports of a buffer period is judged by itself (a discarded one drops only its own changes). | TASE.2 (server) | emit | `TLE` | T (what `TLE` means, the discard, and the next report regardless: 503 clause 7.2.5.5.3) / S (due-to-handed-over as the measure and what it can and cannot see, the exact-`TLE` boundary, the treatment of `RBE` and `AllChangesReported` changes) |
| 59 | Client: a transfer report from a transfer set this port did not start (the transfer set name, or the domain it is in, differs from the started one, or the report names a data set this port did not define or one whose transfer set is another) is discarded and answered with `Transfer_Report_NACK` (same name and domain); the warning is logged for the first such report of a transfer set, the later ones at debug level. Before this was implemented such reports were processed as if they were ours. A transfer set that was started on this association and has since been stopped still counts as ours. | TASE.2 (client) | both | `NackUnknownReports` | T (a report with an incorrect data set or transfer set name is discarded and NACKed) / S (what counts as correct, that it is on by default, that a stopped transfer set is still ours) |
| 60 | Block 5 tags: a taggable device `D` (option `Taggable`) has the MMS variable `D_TAG`, in the scope and domain of `D`, of the type `Tag_Value`: a structure of `Flags` (a bit string of 3 bits: `tag_hi` bit 0, `tag_lo` bit 1, `tag_state` bit 2) and `Reason` (a visible string of at most 255 characters). The tag is in bits 0 and 1 (NO_TAG 0, OPEN_AND_CLOSE_INHIBIT 1, CLOSE_ONLY_INHIBIT 2), the state in bit 2 (IDLE 0, ARMED 1). **The text does not say whether `tag_hi` is the most or the least significant bit of the tag value: this port takes `tag_hi` as the most significant, so value 1 is bits `01` and value 2 is bits `10`.** A bit string of any length is accepted on receipt (missing bits are 0, extra bits ignored); the value 3, a reason of more than 255 characters or another shape is `type-inconsistent`. Get Tag is a Read of `D_TAG` (the `tag_state` bit is whether the device is ARMED now) and Set Tag a Write of it (the written state bit is ignored); `D_TAG` is in `GetNameList`, has the type described for `GetVariableAccessAttributes`, and can be a member of a data set (read in full reports, never reported as changed). | TASE.2 (Block 5) | both | `Taggable`, `InitialTag`, `InitialTagReason` | T (the variable name, its structure and the meaning of the bits: 503 clauses 5.3.12.5, 5.3.12.6, 7.2.6.8, 7.2.6.9; 802 clauses 6.2.2, 7.1.2) / S (the bit order of the tag value, the leniency on receipt, `type-inconsistent` for a bad value, the size -255 of the advertised reason type, tags as data set members, no change reporting; **never seen on the wire**) |
| 61 | Block 5 tags, the errors of Get Tag and Set Tag (IEC 60870-6-503 clauses 7.2.6.8.3 and 7.2.6.9.3): a device or Tag attribute that does not exist is `object-non-existent` (a Write of the tag object of a served device that is not taggable, which was `object-access-denied` as any unknown object before, is the one change of the wire behaviour of an existing configuration: no real peer's flow does it); Set Tag of a device that is ARMED or busy is `temporarily-unavailable`; Set Tag of a VCC device whose tag is not NO_TAG by a client other than the one that set it is `temporarily-unavailable`; `object-access-denied` for a missing permission does not arise (this port has no per-client access control specifications: every associated client may Get Tag and Set Tag). The data is checked after the existence. | TASE.2 (Block 5) | both | No | T (the errors and their order) / S (`type-inconsistent` for bad data and its place in the order, no access control specifications) |
| 62 | Block 5 tags, enforcement: a select (the Read of `D_SBO`) is refused with `temporarily-unavailable` when the tag is OPEN_AND_CLOSE_INHIBIT, after the other select checks and without arming the device; an operate is refused with `temporarily-unavailable` when the tag conflicts with the command: NO_TAG never prevents, OPEN_AND_CLOSE_INHIBIT always, CLOSE_ONLY_INHIBIT only a Close or Raise command. **The standard does not say how Close / Raise map onto this port's command values: a command device's configured ON value is the Close / Raise and OFF the Open / Lower; a setpoint device (real or discrete) is never prevented by CLOSE_ONLY_INHIBIT, and by OPEN_AND_CLOSE_INHIBIT it is.** The operate's tag check comes after the device state and the data checks (the command has to be known) and a refused operate leaves an armed device armed until its timeout. A device that is not taggable is never refused for a tag, and with no tag set nothing changes. | TASE.2 (Block 5) | both | `Taggable`, `CommandValueOn` | T (the three tag values and what each prevents: 503 clauses 5.3.12.3, 5.3.12.4, 7.2.6.2.3, 7.2.6.3.3) / S (the Close / Raise mapping, setpoints, the place of the check, the armed device left armed) |
| 63 | Block 5, the Local Reset action: only its state effect is implemented. The `LocalReset` UI command takes a device from ARMED to IDLE (a later operate by the client that selected it is refused as not selected, `temporarily-unavailable`), optionally sets a tag and reason, logs it (info) and counts it (`LocalResets`, the ARMED to IDLE transitions only); a device that is not ARMED, or is operating, is left as it is. **The MMS event notification of the action (the event condition `<device>_reset`, Block 7) is not sent**, and the client is not told: it finds out when its operate is refused. No local reset happens by itself (nothing in this port detects an emergency pre-emption). | TASE.2 (Block 5) | emit | No | T (the action: 503 clauses 5.3.12.8, 7.2.6.2.3 and 7.2.6.3.3; its mapping to an event condition, clause 7.2.6.5, is not built) / S (the state effect as the whole of it, the UI command, the counting) |
| 64 | Block 5 tags, the owner of a VCC device's tag: the tag belongs to the device, not to the association (it survives the association's end and is lost when the port restarts), and for a VCC device whose tag is not NO_TAG only "the client that set it" may change it. The client is the **identity** taken from the association request (the calling AP title and AE qualifier, or the empty identity when the request has none or this port initiated the association): a client that reconnects with the same identity is still the owner, one with another identity is not, and the empty identity is one client. A tag set locally (`InitialTag`, `LocalSetTag`, `LocalReset`) has no client owner, so on a VCC device only the local operator can change it. An ICC device has no owner rule. | TASE.2 (Block 5) | both | No | T (the owner rule for a VCC device: 503 clause 7.2.6.8.3, 5.3.12.5) / S (everything about how a client is recognised, the survival over associations, the local tags having no owner) |
| 65 | Block 5 tags on the client side: `GetTag` and `SetTag` (UI commands) send a Read and a Write of `<Device>_TAG` for an `Operate` control, of either scope; the outcome is logged with readable explanations of the data access errors, a reason that is not at most 255 characters of visible ASCII is not sent, and the request timeout and the end of the association complete the request. Nothing else on the client side changes (`temporarily-unavailable` is already `ALREADY_ACTIVE`). | TASE.2 (Block 5, client) | emit | No | T (the requests) / S (the UI commands, the logging, the explanations) |
| 66 | Block 3, the report and the rule: a transfer set started with `BlockData` true on a server with 3 in `SupportedBlocks` is reported as an `InformationReport` whose variable list has the special objects of the data set (domain-specific, in the domain of the transfer set, data set order) then one VMD-specific variable, and whose data is the special objects' values plus **one octet string** that starts with the RBE blocking rule octet: **rule 0 (every member of the data set in order, the special objects included) for a complete report** (start-up, integrity, interval of a set without `RBE`, operator request, other external event; also of an `RBE` true transfer set) and, for a **partial report of an `RBE` true transfer set** (object change after the buffer time, the interval report of what changed, the consecutive reports of `AllChangesReported`), **rule 1: only the points reported as (1 octet position, value) pairs, no special objects in the string**, in the order of the data set (ascending position; with `AllChangesReported` the order of the changes `[S]`); a partial report that needs a position above 255 is **rule 2** (rule octet 2, 2 octet positions for every pair of the report; `BlockedRule2`, default true; **accepted, acknowledged and applied correctly by a real peer `[V]`**, four reports of 71 to 106 `RealQ` pairs on data sets of about 390 and 500 points, positions up to 495, the values of 10 points above position 255 checked in the peer; the 2 octet big endian position and the position counting the special objects are verified) and, with `BlockedRule2` false, is sent in the ordinary form with one warning per start (the real peer did not acknowledge ordinary reports of a `BlockData` transfer set); a position above 65535 fits no rule and is always sent in the ordinary form with one warning per start. All report kinds are blocked alike (start, interval, integrity, object change after the buffer time, operator request, other external event, the consecutive reports of `AllChangesReported`); `Critical` acknowledgements, `TLE` and the buffer are unchanged. Without 3 in `SupportedBlocks` `BlockData` stays ignored and the reports are unchanged. A data set with a member that is not a point, or a transfer set name or domain longer than 32 octets, is reported in the ordinary form, with one warning per start. | TASE.2 (Block 3, server) | emit | `BlockData`, `SupportedBlocks` | V (the structure of the report; rule 0 for every complete report, `RBE` false or true, also with `Event_Code_Detected` among the special objects; rule 1 with one or several points for the partial reports of an `RBE` true set, from a real peer as the server) / T (the three rules and the freedom to choose between 1 and 2: 503 clause 7.2.5.5.3) / S (the order with `AllChangesReported`, the ordinary fallbacks, honouring only with 3 advertised; a rule 2 report sent by a real peer has not been seen). A real peer, as the client of this port's server, accepted and acknowledged this port's blocked reports, rule 0, rule 1 and rule 2 `[V]`. What would settle the rest: a rule 2 report sent by a real peer. |
| 67 | Block 3, the variable that carries the octet string: a **VMD-specific** variable (no domain) with the item id `Block_Data` (`DefaultVariableName`, option `BlockedDataName` for the id), listed **after the special objects**, which are domain-specific variables in the domain of the transfer set and keep the order of the data set definition. A client accepts the octet string whatever the variable is called and in whatever scope. | TASE.2 (Block 3) | both | `BlockedDataName` | V (a real peer's variable list: three domain-specific special objects then the VMD-specific `Block_Data`) |
| 68 | Block 3, the octets of a member: the special objects are in the octet string, in data set order **[V]**: `Transfer_Set_Name` 65 octets (the scope, 1 for domain-specific; the domain id, 32 octets; the item id, 32 octets; ASCII, zero padded), `Transfer_Set_Time_Stamp` 4 octets (unsigned seconds, big endian), `DSConditions_Detected` 1 octet (the content octet of the bit string, bit 0 the most significant), `Event_Code_Detected` 2 octets (the standard types it Integer16; the width and the value 0, `00 00`, are **[V]**, the byte order and sign of other values big endian two's complement **[S]**). A `RealQ` point is 5 octets, a big endian IEEE 754 single and the flags octet **[V]**. A `StateQ` point is 1 octet, the content octet of its bit string, the same octet as in an ordinary report **[V]** (a rule 0 block of three special objects and 50 `StateQ` points is 121 octets, and of four special objects, `Event_Code_Detected` included, and 50 `StateQ` points 123 octets, `1 + 65 + 4 + 1 + 2 + 50`, from a real peer). The other point types are this port's plain reading **[S]**: fixed width, big endian, the components in the order this port uses on the wire for the type (time stamp, milliseconds, value, flags, COV, those the type has), the flags and the state as the one octet of the 8 bit string, a time stamp an unsigned 32 bit number of seconds, milliseconds and COV unsigned 16 bit, a discrete value a two's complement 32 bit number; the widths `Real` 4, `RealQ` 5, `RealQTimeTag` 9, `RealExtended` 13, `State` 1, `StateQ` 1, `StateQTimeTag` 5, `StateExtended` 9, `Discrete` 4, `DiscreteQ` 5, `DiscreteQTimeTag` 9, `DiscreteExtended` 13 (the tables in [Blocked transfers (Block 3)](#blocked-transfers-block-3)). A value that does not fit its field is cut to it when sending. A rule 0 block of three special objects and `N` `RealQ` points is `1 + 65 + 4 + 1 + 5 N` octets with none left over **[V]**. | TASE.2 (Block 3) | both | No | V (the special objects' storage forms including the 2 octet event code, `RealQ`, `StateQ`) / T (the types of 802 clause 6.2.1; `Event_Code_Detected` is Integer16 in G.3.6) / S (the byte order and sign of an event code other than 0, every other point type's octets) |
| 69 | Block 3, positions (rules 1 and 2): the position is the 0-based index in the data set's **full** member list, **the special objects counted** (with three special objects the first point is position 3), 1 octet (rule 1) or 2 octets big endian (rule 2) before the value. A rule 1 or 2 string holds **only the points reported**, no special objects; with RBE the points are those an ordinary RBE report would list (the changed points, in ascending order of position; with `AllChangesReported` in the order of the changes `[S]`, consecutive reports each listing a point at most once, quirk 53), several as repeated pairs **[V]**. Rule 0 has no positions. The client reads a position below the number of special objects as that object's storage form and ignores the value; a position beyond the member list, a truncated value or a leftover octet is undecodable (quirk 70). | TASE.2 (Block 3) | both | No | V (rule 1, one point: `01 05 C2 A0 00 00 08`, `01 05 42 C8 00 01 08`, `01 05 C2 BA 14 7B 00` = rule 1, position 5, a `RealQ` value of a big endian float and the flags octet, for the third point of a data set with three special objects; no special objects in the string; and rule 1 with two points, 13 octets: the rule, position 4, a `RealQ` (float, flags), position 5, a `RealQ`, ascending, in a data set of three special objects) / T (positions start at 0 and are the position in the data set's variable list, 1 or 2 octets) / S (the client's reading of a rule 2 report from a real peer, none seen); a real peer accepted and acknowledged this port's rule 2 reports (a position above 255, 2 octet big endian positions) and applied their values correctly `[V]` |
| 70 | Block 3, receipt and diagnostics (client): a report is blocked if it lists its variables and exactly one of them is not a special transfer object (by item name, any scope, any position) and has an octet string as its value; the name and scope of that variable are not checked (a real peer's is the VMD-specific `Block_Data`). It is decoded with the members of the data set the client defined (the special objects and the points), whether or not the transfer set was started with `BlockData` (one warning per transfer set if it was not). The report's time stamp, conditions and `Critical` acknowledgement come from the separate variables, as for an ordinary report; the copy of `Transfer_Set_Name`, time stamp and conditions inside the octet string is only compared (a difference is a note at debug level, not a refusal). A block that cannot be decoded (no octets, unknown rule, truncated value, a name storage or time stamp cut short, a bad scope octet, leftover octets in rule 0, position outside the data set, more values than members, a `State` value with other bits set, no `Transfer_Set_Name`, an unreadable special object) is **not applied at all**, counted in `BlockedReportsUndecodable`, and logged once at warning level with the transfer set, the reason, the rule octet, the length, the member counts, the variable names and a hex dump of the first 96 octets; it is **not answered** (no NACK, no ACK) and the association is untouched. A block of the rule octet alone is accepted as no values. A report that names its data set instead of listing its variables is not recognised as blocked. A warning is logged at association when a transfer set has `BlockData` true and the peer does not advertise Block 3. | TASE.2 (Block 3, client) | decode | `BlockData`, `RequiredRemoteBlocks`, `NackUnknownReports` | V (decoding a real peer's reports: rule 0, no octet left over; this port's client decoded 41 of a real peer's rule 0 reports of `RealQ` points in a run, none undecodable, and published their values) / S (the recognition, the leniency, the comparison of the copies, the refusals, the counting and the diagnostics are this port's design) |

## Known limitations

* One association at a time per listening port (`ICCPServer`). A second connection is not accepted while the first is up.
* An initiating (`ICCPClient`) port's reconnect delay is at least one second.
* Blocks 4 and 6 to 9, and IEC 62351-4 (the HMAC based ACSE authentication) are not implemented. Block 3 (blocked transfers) is implemented on both sides; rule 0 and rule 1 (one or several points) with `RealQ` and `StateQ` points and the four special objects have been seen from a real peer, the rest has not (quirks 66 to 70). TLS and MACE need a build with `ODC_ASIO_SSL`. Of Block 5 the device event conditions (Block 7: the actions Timeout, Success, Failure and Local Reset are not signalled with MMS event notifications), the readable `SBO` and `ControlConfig` variables and cancelling a select are not (see [Device control](#device-control-tase2-block-5)); the device tags and the state effect of the Local Reset are, but a change of a tag is never reported and a tag is lost when the port restarts.
* `BlockData` is honoured by the server only with 3 in `SupportedBlocks` (a data set with a member that is not a point, a name longer than 32 octets, a report with a position above 65535, or, with `BlockedRule2` false, a report that needs a position above 255 is reported in the ordinary form), and a blocked report must list its variables to be recognised by the client. `Critical` reports are acknowledged and tracked, and `TLE` is enforced only as far as the port's own timing shows (no transport back-pressure, no event conditions: quirks 57 and 58). `Critical` reports are acknowledged and tracked, and `TLE` is enforced only as far as the port's own timing shows (no transport back-pressure, no event conditions: quirks 57 and 58).
* Control integers are of width 16: a client that writes a wider integer to a device of ours has it refused (`object-attribute-inconsistent`), and this port never writes one (quirk 54).
* Control events are only handled for configured control devices (`BinaryControls`, `AnalogControls`); any other control event type (for example `AnalogOutputStatus`) is answered `NOT_SUPPORTED`.
* Server data sets and transfer sets are per association; nothing persists across associations.
* The ports have only been built with MSVC; gcc and clang builds are untested.
* Time tags are whole seconds on the types that have them; the local time is used for types without one.

## Unverified items

Everything marked `[S]` in the quirks table, in particular:

* the Stop Transfer write form (the transfer set stop);
* how a `Critical` report is acknowledged, and a `Transfer_Report_NACK` sent (quirks 56 and 59): the acknowledgement of a report without a time stamp is confirmed (quirk 56), the two variable form with the time stamp, the NACK and whether a real peer accepts ours are not; the acknowledgement timeout (10 s) and its cap, which the standard does not define (quirk 57); and the interpretation of `TLE` (quirk 58);
* the session FINISH/DISCONNECT release exchange is now `[V]` as far as a real peer's sequence goes (quirk 45); that a peer accepts our reason values (`normal`) and the Conclude collision rule are not;
* 2000-08 edition behaviour (the peers observed use 1996-08), including the 13 member `DSTransferSet`;
* report by exception layout as sent by the server;
* `AllChangesReported` on the server (quirk 53): the standard says all changes are reported but not how several values of one object are laid out; the choice here (consecutive ordinary reports in the order of the changes) has not been tried against a real client, which sets the flag TRUE on its transfer sets;
* **Block 3, blocked transfers** (quirks 66 to 70): captures of a real peer as the server (`BlockData` true, `RealQ` and `StateQ` points) settled the report structure, rule 0 for every complete report (also of an `RBE` true set), rule 1 with one or several changed points (position counting the special objects, ascending, no special objects in the string) and the storage forms of the four special objects (`Event_Code_Detected` is 2 octets) and of `RealQ` and `StateQ` `[V]`. **Still `[S]`:** a rule 2 report sent by a real peer has not been seen (this port's rule 2 reports were accepted, acknowledged and applied correctly by a real peer `[V]`, the values above position 255 checked in the peer; `BlockedRule2` false sends such a report in the ordinary form, which that peer did not acknowledge; another peer may not implement rule 2), the byte order and sign of an `Event_Code_Detected` other than 0 (only 0 was seen), the octets of every other point type (`State`, `Discrete`, the time tag and extended types), the ordinary fallbacks, and **what a real peer does as the client** asking this port's server for blocked reports (it accepted and acknowledged rule 0, 1 and 2 reports and applied the rule 2 values above position 255 correctly `[V]`). See [How to capture a real peer's blocked report to correct this](#how-to-capture-a-real-peers-blocked-report-to-correct-this) for the captures still wanted;
* the width 16 of the control integers (quirk 54): the width is `[T]`, but the data access error [8] for a value outside it, the unsigned half of the range accepted by the server and the signed range written by the client are `[S]`; and the type description of `Bilateral_Table_ID` (quirk 55), whose varying length (-32) is `[S]`;
* the first-integrity-report-at-once behaviour;
* the member probe and the prune-and-flag recovery (quirks 36 and 37), and the MMS error, data access error and Reject names and numbers (`MmsNames.cpp`), none of which has been seen against a real peer;
* how a third party peer reacts to the client's delete-then-define of a data set that is in use;
* behaviour against other ICCP implementations (one real-world peer has been tested in both roles, with the previous design in which the role followed the port type);
* the service roles as such: `Peer` and every `ServiceRole` / TCP direction combination are verified only by the loopback unit tests and the examples, not yet against a real-world peer;
* the association initiator following the TCP direction when the service role is the other way round (a `Server`-role `ICCPClient`, a `Client`-role `ICCPServer`);
* **Block 5** (quirks 39 to 42 and 51): the select / operate flows and the bare values are `[V]` against one real peer acting as a client, the rest of the device model `[T]` (from the text of the standard), but only the loopback tests exercise our server side against our client side and
  no third party client has operated our server side: in particular the data access errors other than those the standard names (type-inconsistent, type-unsupported, object-attribute-inconsistent), what a real client does with a CheckBackID it does not accept (its rule is not known), the answer
  to a float written to a discrete device, the leniency of a device with `SelectRequired` false, and the Supported_Features bit are `[S]`.
* **Device tags and the Local Reset** (quirks 60 to 65): **no real peer has been seen using them, so all of it is `[T]` or `[S]`, none `[V]`.** In particular the bit order of the tag value in the `Flags` (`tag_hi` taken as the most significant bit), the mapping of Close / Raise onto this port's ON / OFF (and setpoints never being prevented by CLOSE_ONLY_INHIBIT), the place of the tag check in the operate, the leniency on the length of the flags, the data access errors other than those the standard names,
  the identity used for "the client that set the tag" of a VCC device, tags as data set members and the absence of change reporting, and the Local Reset without its event notification.
* TLS against a real IEC 62351-3 peer (see the list in [Secure ICCP / TLS](#secure-iccp--tls)): cipher suites, renegotiation interval, certificate naming.
* MACE: that a peer takes the diagnostics we send, and the scope of the signature (see [What is verified](#what-is-verified-v-and-what-is-not-s)).

## Example configuration

Both run inside one `opendatacon` over 127.0.0.1, with Sim ports as the source and the sink of values. They need the Sim port and a UI plugin (ConsoleUI); build with `-DICCPPORT=ON -DSIMPORT=ON -DCONSOLEUI=ON`.

* `ExampleConfig/ICCP-ClientServer`: an `ICCPClient` (default role `Client`) and an `ICCPServer` (default role `Server`). `templates/` has generic client, server and peer configurations with placeholders.
* `ExampleConfig/ICCP-Peer`: two ports with `ServiceRole` `Peer`, one the TCP initiator (`ICCPClient`) and the other the TCP listener (`ICCPServer`); each serves its own points and reads the other's on the one association.
* `ExampleConfig/ICCP-TLS`: the client/server example over TLS (needs a build with `ODC_ASIO_SSL`); `make-certs.sh` / `make-certs.ps1` make the throw-away certificates it uses (none are committed).
* `ExampleConfig/ICCP-MACE`: the client/server example with MACE authentication in the association (needs `ODC_ASIO_SSL`), with its own `make-certs` scripts; `ICCPClient-WrongCA.conf` shows a refusal.

Both examples also have control devices (TASE.2 Block 5): a console `spoof_event` of a `ControlRelayOutputBlock` to the operating port is selected and operated on the other port, which publishes it to its Sim port
(whose `BinaryControls` accept it; `http://127.0.0.1:9100/SimSource?type=control&index=0` shows what arrived). The commands to type are in the comment at the top of each `opendatacon.conf`.

```
cd ExampleConfig/ICCP-ClientServer
opendatacon -p .
```

## Unit tests

The unit tests are in [`Code/tests/ICCPPort_tests`](../../tests/ICCPPort_tests) and use Catch.
They need the port's internals, so they are compiled together with the port's own objects (the `ICCPPort_objects` object library) into the `ICCPPort_tests` executable, which has its own Catch `main`
(`TestICCPPort.cpp`). The `ICCPPort` library is the same objects plus the plugin entry points (`main.cpp`) and carries no test code, whether or not `TESTS` is on.

Test files are named `*Test.cpp` and picked up by the test directory's `CMakeLists.txt`:

* the codec layers (BER, TPKT, COTP, Session, Presentation, ACSE, MMS, TASE.2): fixed byte vectors, truncation tests, and random mutation tests that check corrupted input only ever produces a `DecodeError`;
* `InteropVectorsTest.cpp`: synthetic vectors that follow the structure of real-world captures;
* `IccpStackTest.cpp`: two `Connection`s back to back in memory (association, release, abort, malformed bytes, segmentation, concurrent requests in both directions, timeouts);
* `ICCPPortConfTest.cpp`: configuration parsing and validation, including the unified schema (one configuration under both port types, each `ServiceRole`, the direction rules and their messages), `MaxBufferedChanges`, `ReportAckTimeoutms`, `MaxUnacknowledgedReports`, `AcknowledgeCriticalReports` and `NackUnknownReports` with the warnings of a `Critical` transfer set, and the 32 character / visible ASCII rule of `BilateralTableID` and `RemoteBilateralTableID`;
* `Block5Test.cpp`: Block 5: the device and select objects and their types, the requests, the bare value of an operate (including the wire bytes of a real peer's float), the result mapping tables; the server and client sides against each other in memory with a fake clock (the Read of
  the select object returns the configured CheckBackID, the same every time and association, arming and the timeout lapse, a second select refused, an operate without a select or by another association refused, the lenient device without a select requirement,
  a bare float / integer / unsigned operate of each device type, wrong kinds, ON / OFF and neither, a Write of the select object and a Read of the device refused, every downstream status, busy devices, concurrent operates on different devices, downstream and peer timeouts,
  errors and rejects, malformed answers, the client's CheckBackID check (match, mismatch, none expected), the association ending with a select or an operate pending, discovery, the block 5 feature check) and the control configuration (defaults per role, one file under both
  port types, the direction rules, `CheckBackID`, validation, the blocks), the width 16 of the integers (the server's refusal and the accepted range -32768 to 65535 with its boundaries, signed and unsigned encodings, the client's refusal before sending with -32768 / 32767 and rounding, the `CommandValueOn` / `CommandValueOff` validation),
  and the type advertised for `Bilateral_Table_ID` with a peer's longer string;
* `TagTest.cpp`: the optional parts of Block 5, the device tags and the Local Reset. The `Tag_Value` encoding (the bytes of every tag value, the state bit, an empty reason and one of 255 characters, bit strings of 0 to 16 bits on receipt, the undefined tag value 3, every wrong shape, truncation and mutation), the tag names and the reason rule, what each tag prevents; the server side against a scripted peer in memory with a fake clock
  (Get Tag and Set Tag of every device kind and both scopes, the initial tag, a device that is not taggable, every error of Set Tag: bad data, ARMED, busy, the VCC owner rule with several client identities and a locally set tag, the tag state bit through select, operate, timeout and reset, the enforcement matrix of tag x select / operate x ON / OFF / setpoint for devices with and without a select and with the tag set at the select and between select and operate, a refused operate leaving the device armed,
  the tag surviving the association, Local Reset (armed, idle, lapsed, operating, with a tag change, the later operate refused), the tag object in `GetNameList`, `GetVariableAccessAttributes` and a data set and in an integrity report with no report of a change of a tag, the counters, a point or device named like a tag object keeping its own); the client side against the server in memory (Get Tag and Set Tag, both scopes,
  the readable errors, an invalid reason, a timeout, the association ending with a request in flight, answers that are not right, a service error and a reject, the counters); the configuration (`Taggable`, `InitialTag`, `InitialTagReason`, their defaults and every message, the name clashes); and two ICCP ports over real TCP (ports 20430 to 20439) driven through the UI commands `SetTag`, `GetTag`, `Tags`, `LocalSetTag` and `LocalReset` (enforcement end to end, the device ARMED by a select whose CheckBackID the client does not accept then taken to IDLE by `LocalReset`,
  a VCC device and its owner across a reconnect, the statistics, every error result of every command, and a configuration without `Taggable` unchanged);
* `DiagnosticsTest.cpp`: the MMS error names, and the client's provisioning diagnostics and prune-and-flag recovery against a scripted peer;
* `TransferSetAllChangesTest.cpp`: the server side of a transfer set with `AllChangesReported`, a `ResponderSession` against a scripted client in memory with a fake clock: several changes of one point in the buffer time all reported in order with their own time stamps (and only the last with `false`, in the 1996-08 form, with `BufferTime` 0 or with `RBE` false), two points changing in turn, a state that flips and flips back, the buffer timer, the cap `MaxBufferedChanges` and its overflow, the buffer cleared by a new association and by stopping the transfer set, an integrity, interval and operator request report in the middle of a buffer period, and the log lines;
* `ReportAckTest.cpp`: `Critical` reports and `TLE`. The encoding of `Transfer_Report_ACK` and `Transfer_Report_NACK` (fixed bytes, round trips, lenient reading, what is not an acknowledgement, truncation and mutation); the server side against a scripted client in memory with a fake clock (a report outstanding until acknowledged, a wrong time stamp, matching by time stamp in any order, the same second, no time stamp, a NACK, the timeout logged once per report, `ReportAckTimeoutms` 0, the cap `MaxUnacknowledgedReports`, separate transfer sets, cleared by stop, restart, deleting the data set and the end of the association, a transfer set that is not critical untouched and byte for byte the same reports, unknown or broken acknowledgements) and `TLE` (off, exceeded, the next report regardless, the exact boundary, interval and integrity reports, reports sent at once, `AllChangesReported`, not tracked as critical); and the client side against a `ResponderSession` in memory (every report of a critical transfer set acknowledged with the report's time stamp and the server's list ending empty, with and without a time stamp object, a non-critical set not acknowledged, `AcknowledgeCriticalReports` false and the timeouts it causes, the NACK of a report from a transfer set not started or of an unknown data set in both report forms, `NackUnknownReports` false);
* `BlockedTransferTest.cpp`: TASE.2 Block 3 (blocked transfers); synthetic data in the layout a real peer's capture showed (rule 0, `RealQ`), and a regression pin of this port's reading for the rest. The octets of every one of the 12 point types (byte for byte, round trip, equal to the ordinary form's value, a wrong length refused, cut fields), a vector built by hand from the capture's facts (rule 0, scope 1, zero padded 32 octet domain and item, 4 octet time stamp, 1 octet conditions, `RealQ` records, 821 octets for 150 points), the several-points rule 1 layout (`01 04 .. 05 ..`, with three and with four special objects, and the 84 octet rule 0 block with a 2 octet event code), the storage form of every special object (names of 32 octets and one more refused, conditions octets, the 2 octet event code and a 4 octet one refused), all three rules for every type, a data set of all twelve types whole and in part, rule 1 or 2 chosen by the largest position (a data set of 300 members, the 255 / 256 boundary, refused when a position does not fit), the malformed input table (no octets, unknown rule, truncated, leftover, name storage too short, time stamp truncated, a bad scope octet, position out of range, more values than members, a `State` value with other bits), truncation and mutation, the recognition of a blocked report (VMD-specific and any other name, scope and position, what is not blocked, a report that cannot be attributed) and the hex dump (96 octets); the server side against a scripted client in memory with a fake clock (the VMD-specific variable and the special objects inside the octet string, RBE false and RBE true (rule 0 for the complete reports, rule 1 with only the changed points and the positions counting the special objects for the partial ones: buffer time, interval, integrity, `AllChangesReported` consecutive reports each blocked in the order of the changes, `Critical` acknowledged with the time stamp of the blocked report and timing out, `TLE`; operator request and other external event; a report needing a position above 255 as rule 2 (2 octet positions for every pair, a mixed report with one position above 255, the exact octets) and, with `BlockedRule2` false, in the ordinary form with one warning per start; a position above 65535 (a data set of 66000 points) always in the ordinary form with one warning, the others of the transfer set staying blocked; a data set with `Event_Code_Detected` sent in the blocked form (rule 0 with the 2 octet code after the conditions, rule 1 with positions counting all four special objects, several points as several ascending pairs, no warning, and the client decoding it, also from the server), a data set without special objects, a default `SupportedBlocks` leaving the reports ordinary and identical to those of `BlockData` false, `BlockedDataName`, the fallbacks for a member that is not a point and for a name longer than 32 octets, a data set of 302 members); the client side against a `ResponderSession` and against scripted reports in memory (the same events as ordinary reports with RBE and without, every rule decoded including the three real 7 octet rule 1 strings and the 701 octet rule 0 report of an `RBE` true set, any variable name, the comparison of the copies of the special objects, every failure of the table with its diagnostics and no value applied, no answer and the association untouched, the 96 octet cap of the hex dump, a blocked report with `BlockData` false, from a transfer set that is not ours, with no `Transfer_Set_Name`, the warning at association, `Critical` acknowledged); and two ICCP ports over real TCP (ports 20450 to 20461) with every point type, RBE on and off, `BlockData` on and off and Block 3 advertised or not, a `Critical` blocked transfer set and `AllChangesReported`. The configuration tests of `BlockData` (silent at load), `SupportedBlocks` 3 (and 7 and 8, advertised like any block), `BlockedRule2` and `BlockedDataName` are in `ICCPPortConfTest.cpp`.
* `ICCPLoopbackTest.cpp`: ICCP ports over real TCP on 127.0.0.1: association, bilateral table check, data sets, transfer sets, integrity and
  object change reports, polling, Stop Transfer, reconnect, release collision; and every TCP direction x service role pairing (Client-Server, Server-Client, Peer-Peer with either end listening, Peer-Server,
  Server-Peer, Client-Peer, Peer-Client), checking the values, quality and time stamps in each direction (both directions at once for Peer-Peer), release and reconnect. The same pairings with Block 5: an ODC control event
  into the operating port comes out as an ODC control event at the serving port and the `CommandStatus` comes back (select before operate and direct, scaled setpoints, every status, an unselected operate refused,
  the CheckBackID match and mismatch, both directions at once for Peer-Peer, a reconnect, the counters). A transfer set with `AllChangesReported` between our client and server ports: every change inside the buffer time arrives as its own ODC event, in order, with its own time stamp. A `Critical` transfer set between our ports (with and without a `Transfer_Set_Time_Stamp` object): every report acknowledged, the server's outstanding count ending at 0 and no failure; a client that does not acknowledge (`AcknowledgeCriticalReports` false) giving a timeout `Data_failure` per report while the data and the association carry on; and a transfer set that is not critical never acknowledged and never timing out.
* `TlsConfTest.cpp` (every build): what the port passes to the generic `TLS` section parser (the ICCP default cipher lists, the `Legacy12` preset, the session id context, the messages), the default port, and the clear error when the build has no TLS support; with `ODC_ASIO_SSL` the required keys and files.
  The generic parser, the TLS filter and the session manager are tests of the ODC library (`Code/tests/ODC_tests`: `TlsConfTest.cpp`, `TlsFilterTest.cpp`, `TCPSessionManagerTest.cpp`, with the throw-away PKI `TlsTestPki.h`, which the tests of this port include as well); the descriptions below are of what they cover.
* With `ODC_ASIO_SSL` only: `TlsFilterTest.cpp` (`ODC_tests`) wires two TLS filters back to back in memory over a throw-away PKI made at run time (`TlsTestPki.h`: CA, node certificates, expired, not yet valid, other CA,
  CRLs, an encrypted key; nothing is committed): mutual authentication under TLS 1.2 and 1.3, large messages and one octet at a time, a missing, untrusted, expired, not yet valid or revoked certificate,
  the allow list, host name verification, version and cipher mismatches, garbage, truncated and mutated handshakes and records, close_notify, resumption, renegotiation and key update, and every configuration error.
  `TlsLoopbackTest.cpp` runs the ports over TLS on real TCP (ports 20400 to 20499): the role pairings, reconnect, renegotiation, renewed certificate files, the wrong CA, a refused client certificate, a plain peer on either end,   a silent peer.
  `StaleTransportTest.cpp` (plain and TLS, ports 20500 to 20599): the ports talk through a recording TCP proxy that is also a strict peer; a stale association or session writing while the connection is replaced, a new transport event with the association still up, 50 clean and abrupt drops and reconnects,
  connections cut at every moment of the handshake; the first bytes of every connection are what the protocol starts with, and with the guards switched off the bug is reproduced.

  All the fixtures use synthetic names and values.

* MACE: `MaceTest.cpp` (every build): the authentication value and the AARQ/AARE fields byte for byte (a hand built vector, long forms, truncation, mutation, unknown members, the skeleton of a real peer's APDUs), GeneralizedTime, the `MACE` section and its validation messages,
  and the clear error when the build has no MACE support. With `ODC_ASIO_SSL`: `MaceCryptoTest.cpp` (sign and verify, the exact 30 octet signed block, a wrong key, a tampered time or signature, a signature for another time, the window and its edges, the mechanism-names (official and legacy in both directions, the tolerance, the warnings, the echo, the A-security abstract syntax), malformed values,
  expired / not yet valid / untrusted / revoked certificates, `VerifyPeer` false, the allow list, key size limits, EC keys), `MaceStackTest.cpp` (two `Connection`s back to back with a fake UTC clock: mutual authentication, every refusal and the diagnostic the initiator logs, the
  responder's authentication checked by the initiator, `RequireAuthentication` / `RequireResponderAuthentication`), and `MaceLoopbackTest.cpp` (the ports over real TCP, ports 20600 to 20699: the role pairings, with and without TLS, a reconnect, the wrong CA).
  `MaceCryptoTest.cpp` also holds a check against a real peer's capture that is **off by default**: set `ICCP_MACE_CAPTURE_DIR` to a directory holding the two ACSE APDUs (`aarq.bin`, `aare.bin`); it decodes them, re-encodes them byte for byte, compares the skeleton and
  verifies both signatures with the captured certificates, and prints only counts. The capture is never part of the repository.

Build with `-DTESTS=ON`, then run the `ICCPPort_tests` executable from the build output directory (or `ctest`).

## Building

The port is optional. Enable it with the `ICCPPORT` CMake option (it is also enabled by `FULL`):

```
cmake -S . -B build -DICCPPORT=ON -DTESTS=ON
cmake --build build --target ICCPPort_tests
```

A C++20 compiler and CMake 3.21 or later are required. For TLS and MACE, add `-DODC_ASIO_SSL=ON` (OpenSSL 1.1.1 or later; see [Building with TLS support](#building-with-tls-support)).

## Roadmap and remaining work

What is left, in rough order of value. Everything not listed here is done (see [Status](#status)); the unverified (`[S]`) details are in [Unverified items](#unverified-items) and the [quirks table](#supported-implementation-quirks).

1. **Interoperability with further peers.** The port has been run against one real-world ICCP implementation (both roles, plain, TLS and MACE) and, for Block 5, one real peer operating devices. Further implementations should be tried, in particular the service roles and TCP-direction
   combinations that only the loopback tests exercise (`Peer`, a `Server`-role `ICCPClient`, a `Client`-role `ICCPServer`), the 2000-08 edition behaviour, and the unverified items.
2. **Block 5 against a real peer in its automatic (select) mode.** The flow (a Read of `<Device>_SBO`, then a Write of the bare value) is `[V]`, but the rule a real client uses to accept the CheckBackID that the select returns is not known (it refused ids below 32768 when none was
   configured on its side, and an id that differed from a configured one), so the server imposes none and the id is configurable per device (`CheckBackID`).
3. **TASE.2 Block 4.** The original target profile listed Blocks 1 to 5 as supported; Block 4 (operator stations) is not implemented. Block 3 (blocked transfers) is implemented on both sides and matches a real peer's rule 0 and rule 1 (one or several points); this port's rule 0, 1 and 2 reports were accepted and acknowledged by a real peer, and the values of its rule 2 reports above position 255 were checked and correct. Still open: a rule 2 report sent by a real peer, the other point types and a non zero event code (quirks 66 to 70; see [How to capture a real peer's blocked report to correct this](#how-to-capture-a-real-peers-blocked-report-to-correct-this)).
4. **Block 5 completeness.** What remains: **cancelling** a select, the **MMS event notifications** of the device actions Timeout, Success, Failure and Local Reset (the event conditions `<device>_time`, `_success`, `_fail`, `_reset`: Block 7, with MMS event enrollment, not supported; quirk 43) and the readable **`SBO`** and **`ControlConfig`** status variables of IEC 60870-6-802 (the standard gives no MMS names for them). The device tags and the state effect of the Local Reset are done (quirks 60 to 65). The tests of Block 5 also need a third-party client to operate our server side (only our own client has), and tags have never been seen on the wire.
5. **IEC 62351-4 HMAC based ACSE authentication**, gated on `ODC_ASIO_SSL` like the rest of secure ICCP. Not started: the certificate based MACE authentication, which a real-world peer uses, is done.
6. **Stop Transfer and the other `[S]` encodings.** The Stop Transfer write form (quirk 25), the parts of the `Critical` acknowledgement that a real peer has not confirmed (quirks 56 to 59) and the other items under [Unverified items](#unverified-items) need a real peer to confirm them.
7. **Other toolchains.** The port has only been built with MSVC. gcc and clang builds are untried (the CMake is portable, the code uses C++20 and the standard library).
8. **OpenSSL is linked twice (size).** On Windows OpenSSL is linked statically, and both `libODC` (which holds the TLS filter) and the `ICCPPort` module (for `Mace`) carry a copy, and the process holds two OpenSSL states. The sizes then measured (about 4.5 MB of the 5.7 MB Release `ICCPPort.dll` was OpenSSL, the port's own code about 1.1 MB) are from before the TLS filter moved into `libODC`; the port's share is expected to be smaller now that only the crypto half of `Mace` calls OpenSSL from the port (not re-measured).
   Options: a small X.509 / signing interface in `libODC` that the crypto half of `Mace` uses too (`asio.cpp` in `libODC` already shares ASIO's SSL implementation this way, but the port bypasses it), or dynamic OpenSSL on Windows
   (two extra DLLs, a change of a repo-wide choice). Not decided.

### Constraints for further work

* Licence: opendatacon is Apache-2.0. Do not link GPL code (libiec61850, lib60870, Wireshark libraries).
* The core stack (BER through TASE.2) uses only the C++ standard library and asio (through `TCPSessionManager`). Crypto uses OpenSSL, gated on `ODC_ASIO_SSL`; the port must still build and work without it.
* Byte-level encodings come from captures of real associations and from the standards (IEC 60870-6-503, -702 and -802, IEC 62351-3 and -4, ISO 8650, 8823, 8327, 9506). The standards are paywalled: put facts, not their text, in the repository. Wireshark / tshark is a development tool only, never a runtime dependency.
* Names, addresses, AP-titles and other values from a real peer's captures are never put in the repository: fixtures and examples use synthetic values.
* Every layer is a pure encode / decode unit over byte buffers (no I/O) with fixed-vector, truncation and mutation tests; input is untrusted (bounds-checked, `DecodeError`, capped nesting and sizes).
* Tests are in `Code/tests/ICCPPort_tests` (Catch), built into their own executable together with the port's objects (see [Unit tests](#unit-tests)). Format C++ with `uncrustify.cfg`; the commit hook checks it.
