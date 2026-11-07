# ICCPPort: Protocol Implementation Conformance Statement (PICS)

This document is the PICS / ISPICS statement of the `ICCPPort` library of opendatacon, written in the form of the
requirements lists of IEC 60870-6-702 (the TASE.2 functional profile), Annex A (tables A.1 to A.61).
It lists, for each item of those tables, what the standard asks of a TASE.2 client and server and what this implementation does.
It describes the source tree of the library as documented in [README.md](README.md), including the Block 3 (blocked transfers) code.

* How the library is used and configured: [README.md](README.md).
* The wire encodings that were checked and those that were not: [Supported implementation quirks](README.md#supported-implementation-quirks)
  and [Unverified items](README.md#unverified-items).
* Date of this statement: 2026-10-29.

## Contents

1. [How to read this statement](#1-how-to-read-this-statement)
2. [Product identification](#2-product-identification)
3. [Roles](#3-roles)
4. [Conformance building blocks](#4-conformance-building-blocks)
5. [TASE.2 requirements lists (A.3)](#5-tase2-requirements-lists-a3)
6. [TASE.2 objects and data types](#6-tase2-objects-and-data-types)
7. [Transfer set conditions and parameters](#7-transfer-set-conditions-and-parameters)
8. [MMS requirements lists (A.4)](#8-mms-requirements-lists-a4)
9. [ACSE (A.5)](#9-acse-a5)
10. [Presentation (A.6)](#10-presentation-a6)
11. [Session (A.7)](#11-session-a7)
12. [Transport and lower layers](#12-transport-and-lower-layers)
13. [Authentication and security](#13-authentication-and-security)
14. [Limits and defaults](#14-limits-and-defaults)
15. [Protocol-level options](#15-protocol-level-options)
16. [Known deviations and unverified items](#16-known-deviations-and-unverified-items)
17. [Test evidence](#17-test-evidence)
18. [Items to be confirmed](#18-items-to-be-confirmed)

## 1. How to read this statement

**Supported** column: `Yes` = implemented in the code and covered by a unit test (or stated as verified in the README);
`Partial` = implemented with the limitation given in the note; `No` = not implemented (the reason is given where it is known).

**702 status columns** are the "F/S" (functional standard) entries of the 702 tables, shown as `Client-CR / Server-CR`:

| Code | Meaning in 702 |
|------|----------------|
| m | mandatory (`m1`, `m2`, ... mandatory for conformance block n) |
| o | optional |
| c | conditional |
| i | outside the scope of the standard (may be ignored, but a receiver shall still be able to decode the PDU) |
| x | excluded: shall not be implemented |
| - | not applicable |

Where 702 gives a sending and a receiving entry, they are shown as `snd,rcv`.

**Basis tags** used in the notes (the same as in the README):

| Tag | Meaning |
|-----|---------|
| V | seen in packet captures of a real peer's association |
| T | taken from the text of the standard (IEC 60870-6-503, -802, 62351-4), not seen in real traffic |
| S | an assumption, a design choice or a deliberate deviation; not confirmed by a peer or the standard |

"Quirk n" refers to the numbered table of [Supported implementation quirks](README.md#supported-implementation-quirks) in the README.

**Client and server.** 702 distinguishes the client control centre (Client-CR) and the server control centre (Server-CR).
In this implementation the two are *service roles* of one association (section 3). "Client side" below means the code that runs when the
service role is `Client` or `Peer`; "server side" the code that runs when it is `Server` or `Peer`.

No names of real products, vendors, sites or points appear in this document: "a real peer" is a third-party ICCP implementation
that the port was run against.

## 2. Product identification

| Item | Value |
|------|-------|
| Product name | ICCPPort |
| Part of | opendatacon (Apache License 2.0), `Code/Ports/ICCPPort` |
| Delivered as | the port library `ICCPPort` (a shared module loaded by opendatacon: `ICCPPort.dll` on Windows) |
| Port types in the library | `ICCPClientPort` (type name `ICCPClient`, the TCP initiator) and `ICCPServerPort` (type name `ICCPServer`, the TCP listener); both in the one library, same configuration schema |
| Library entry points | `new_ICCPClientPort`, `delete_ICCPClientPort`, `new_ICCPServerPort`, `delete_ICCPServerPort` (the unit tests are a separate executable, section 17) |
| Build option | CMake `ICCPPORT=ON` (also enabled by `FULL`); C++20, CMake 3.21 or later; TLS and MACE need `ODC_ASIO_SSL=ON` (OpenSSL 1.1.1 or later) |
| Platforms | built with MSVC; gcc and clang builds are untried (README, Known limitations) |
| Source baseline | opendatacon 1.11.x (the exact release or commit of a delivered binary is to be confirmed at delivery) |
| Protocol stack | written from scratch: BER, TPKT/COTP, session, presentation, ACSE, MMS subset, TASE.2; no third-party ICCP/MMS/OSI library. Crypto (TLS, MACE) uses OpenSSL, gated on `ODC_ASIO_SSL` |
| TASE.2 editions | 1996-08 (`TASE2_Version` 1996/8) and 2000-08 (`TASE2_Version` 2000/8); option `TASE2Edition`, default 2000-08. A peer's edition is read and, by default, must equal ours (`RequireMatchingEdition`) |
| MMS | ISO 9506-1/-2 subset (sections 8.3 to 8.5); MMS version number 1 is proposed and negotiated; parameter CBBs `str1, str2, vnam, vlis`; nesting level 4 by default |
| Transport | RFC 1006 (TPKT, ISO 8073 class 0) over TCP; optionally TLS (IEC 62351-3) |
| `Identify` response | vendor `opendatacon`, model `ICCP`, revision `1.0` (options `VendorName`, `ModelName`, `Revision`) |
| Interworking verified | one real third-party ICCP implementation, in both roles, with the earlier design in which the role followed the port type, plain TCP, TLS and MACE; the later role split (`ServiceRole`, `Peer`) is verified by the loopback tests only (README, Status) |

## 3. Roles

| Aspect | Value |
|--------|-------|
| ICCP client | Yes: `ServiceRole` `Client` (reads the peer's association objects, defines data sets, starts transfer sets, polls, consumes reports, operates the peer's devices) |
| ICCP server | Yes: `ServiceRole` `Server` (serves association objects, points, data sets, transfer sets and devices; sends reports) |
| Both | Yes: `ServiceRole` `Peer`: both at once on the one association, with separate invoke id spaces per direction (quirks 15, 35). `Peer` is verified by the loopback tests only |
| Role per association | one role per port, fixed by configuration. One port has one association at a time; to talk to several peers use several ports |
| Who initiates the association | the TCP initiator (`ICCPClient` port type) sends COTP CR, session CN, ACSE AARQ and MMS Initiate, whatever the service role (quirk 34) |
| Port type default of `ServiceRole` | `ICCPClient`: `Client`; `ICCPServer`: `Server`; an explicit `ServiceRole` overrides it for either port type |

| Port type | TCP | ACSE initiator | A.48 initiator / responder |
|-----------|-----|----------------|----------------------------|
| `ICCPClient` | initiator (connects, retries) | this port | Yes / Yes (a role depends on who connects) |
| `ICCPServer` | listener (one connection at a time) | the peer | Yes / Yes |

### A.1 Client/Server capability

| 702 item | Capability | 702 | Supported | Note |
|----------|------------|-----|-----------|------|
| I./1 | Client control centre | - / - (o.1: at least one of the two) | Yes | `ServiceRole` `Client` or `Peer`. Tests: `ICCPLoopbackTest` (roles) |
| I./2 | Server control centre | - / - (o.1) | Yes | `ServiceRole` `Server` or `Peer`. Tests: `ICCPLoopbackTest` (roles) |

## 4. Conformance building blocks

### A.2 TASE.2 CBBs

| 702 item | Block | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|-------|----------------------------|-----------|------|
| I./1 | 1 Basic Services | m / m | Yes | Data values, data sets, association management. Advertised by default (`SupportedBlocks` default `[1, 2]`) |
| I./2 | 2 Extended Conditions | o / o | Yes | DS transfer sets: both sides. Advertised by default |
| I./3 | 3 Blocked Transfers | o / o | Yes | Both sides. Advertised, and `BlockData` honoured by the server, **only when 3 is in `SupportedBlocks`**; the client sends `BlockData` when a transfer set asks for it and decodes blocked reports whether or not 3 was advertised. Rules 0, 1 and 2. Verified `[V]`: the report structure, rule 0 and rule 1 as sent by a real peer, and this port's rule 0/1/2 reports accepted, acknowledged and applied by a real peer. Not seen `[S]`: a rule 2 report sent by a real peer, the byte order of a non zero `Event_Code_Detected`, every point type but `RealQ` and `StateQ`. See [Blocked transfers](README.md#blocked-transfers-block-3), quirks 66 to 70. Tests: `BlockedTransferTest` |
| I./4 | 4 Information Message | o / o | No | Not implemented (no `InformationBuffer`, no information message transfer set). On the roadmap |
| I./5 | 5 SBO Device Control | o / o | Partial | Select before operate and direct operate, both directions, ODC `ControlRelayOutputBlock` and `AnalogOutput*`; optional device tags (Get Tag, Set Tag) and the state effect of Local Reset. Not implemented: the event notifications of Timeout, Success, Failure and Local Reset actions (Block 7), the readable `SBO` and `ControlConfig` variables, cancel select, ICCP command and setpoint structures with a time stamp. See [Device control](README.md#device-control-tase2-block-5), quirks 39 to 43, 51, 54, 60 to 65. Tests: `Block5Test`, `TagTest`, `ICCPLoopbackTest` |
| I./6 | 6 Programs | i / i | No | Out of scope in 702 and not implemented |
| I./7 | 7 Events | i / i | No | Out of scope in 702 and not implemented (MMS event management is excluded, tables A.26) |
| I./8 | 8 Accounts | i / i | No | Out of scope in 702 and not implemented |
| I./9 | 9 Time Series | i / i | No | Out of scope in 702 and not implemented |

### Supported_Features, `SupportedBlocks` and `RequiredRemoteBlocks`

| Item | Behaviour |
|------|-----------|
| Object | `Supported_Features`, a VMD-specific (VCC) variable, read by the peer as part of the association objects |
| Encoding | a BIT STRING of 12 bits; bit `n-1` (bit 0 is the most significant bit of the first octet) is set for block `n`. Bits for blocks 10 to 12 are never set. A received string of any length is accepted (quirk 11) |
| Advertised | the blocks in option `SupportedBlocks` (array of 1 to 9), default `[1, 2]` |
| Added automatically | Block 5 is added when the port serves control devices and `SupportedBlocks` is not given; if `SupportedBlocks` is given without 5 and devices are served, the configuration is rejected |
| Not added automatically | Block 3 is never added: it must be listed explicitly to enable blocked transfers |
| What the parser allows | any of 1 to 9. Listing 4, 6, 7, 8 or 9 would claim a block that is not implemented; the configuration does not stop it, do not list them |
| Required of the peer (client side) | the blocks in `RequiredRemoteBlocks` (array of 1 to 9), default `[1]`, plus 2 when transfer sets are started and plus 5 when the peer's devices are operated (unless the option is given). A missing block ends the association with an error and, on an initiating port, a retry after at least one second |
| Block 3 requested of a peer that lacks it | a warning at association; list 3 in `RequiredRemoteBlocks` to end the association instead |
| Tests | `Tase2Test` (association objects), `ICCPPortConfTest`, `ICCPLoopbackTest` |

## 5. TASE.2 requirements lists (A.3)

### A.3 Association management

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | Supported Features | m1 / m1 | Yes | Read (client) and served (server). Quirk 11. Tests: `Tase2Test`, `ICCPLoopbackTest` |
| I./2 | QOS | o / o | No | No priority class or quality of service is conveyed to the transport (702 clause 6.4) |
| I./3 | Associate operation | m1 / m1 | Yes | COTP CR/CC, session CN/AC, presentation CP/CPA, ACSE AARQ/AARE, MMS Initiate. The client then reads `Bilateral_Table_ID` (checked against `RemoteBilateralTableID`), `TASE2_Version` and `Supported_Features`. Tests: `IccpStackTest`, `ICCPLoopbackTest` |
| I./4 | Conclude operation | m1 / m1 | Yes | MMS Conclude, then session FINISH with ACSE RLRQ, DISCONNECT with RLRE (quirks 23, 45). Tests: `IccpStackTest` (release) |
| I./5 | Abort operation | m1 / m1 | Yes | Session ABORT carrying presentation ARU with an ACSE ABRT; a peer's abort is answered with ABORT ACCEPT. Tests: `IccpStackTest` (Abort) |
| I./6 | Security per IEC/TS 62351-4 | o / o | No | The HMAC based authentication of 62351-4 is not implemented. The certificate based MACE authentication, which a real peer uses, is (Partial, see section 13) |

### A.4 Data value

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | Data Value Model | m1 / m1 | Yes | Points are configured statically (`Binaries`, `Analogs`, `Counters`); no discovery on the server |
| I./2 | VCC-specific scope | m1 / o | Yes | Point option `Scope` `"VCC"` (an MMS VMD-specific name). Both client and server. See [ICC and VCC scope explained](README.md#icc-and-vcc-scope-explained) |
| I./3 | ICC-specific scope | m1 / o | Yes | Point option `Scope` `"ICC"` (the default, an MMS domain-specific name; `LocalDomain` / `RemoteDomain` / `Domain`) |
| I./4 | Get Data Value operation | o / m1 | Partial | Server: Yes (MMS Read of one or several variables, and of data sets). Client: no single-point Read of data values is issued; values are obtained from transfer set reports and from Reads of data sets (`PollIntervalms`), and a Read of the variables is used by the missing-member probe (quirk 36). Tests: `Tase2Test`, `Block5Test`, `TagTest`, `DiagnosticsTest` |
| I./5 | Set Data Value operation | o / m1 | No | Not supported (the profile this port was written for excludes it). A Write to a data value is answered with the data access error object-access-denied |
| I./6 | Get Data Value Names operation | o / m1 | Partial | Server: Yes (MMS GetNameList, class named variable, VMD or domain scope, paged by `NamePageSize`). Client: not issued by the port (the request builder and the paging exist in the codec). Tests: `Tase2Test`, `Block5Test` |
| I./7 | Get Data Value Type operation | o / m1 | Partial | Server: Yes (MMS GetVariableAccessAttributes for points, devices, tags and `Bilateral_Table_ID`; the type descriptions are `[S]`, quirk 55). Client: not issued by the port. Tests: `Tase2Test`, `Block5Test` |
| I./8 | IndicationPoint object | m1 / m1 | Yes | The 12 `Data_*` types (section 6). Mapped to ODC Binary, Analog and Counter events |
| I./9 | ControlPoint object | m5 / m5 | Partial | Block 5 devices, see I./5 of A.2 and table A.12 |
| I./10 | ProtectionEquipmentEvent object | o / o | No | Not implemented |

### A.5 Data sets

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | Data Set Model | m1 / m1 | Yes | Configured data sets (`DataSets`, direction `Serve` or `Read`), data sets created by the peer, data sets this port defines on the peer |
| I./2 | VCC-specific scope | m1 / o | No | Data sets are domain-specific only (in `LocalDomain` on the server, `RemoteDomain` on the client). A VMD-specific data set is refused by the server (a definition class service error) and a GetNameList of VMD-specific data sets returns an empty list. The client-side row is therefore not met (702 makes it m1 for a client) |
| I./3 | ICC-specific scope | m1 / o | Yes | Domain-specific data sets |
| I./4 | Create Data Set operation | o / o | Yes | Client: MMS DefineNamedVariableList for the transfer sets it starts (delete first, then define; members the peer lacks can be pruned, quirks 16, 36). Server: for the peer's data sets in `LocalDomain`, limited by `MaxClientDataSets` and `MaxDataSetMembers`. Tests: `ICCPLoopbackTest`, `TagTest`, `DiagnosticsTest` |
| I./5 | Delete Data Set operation | o / o | Yes | Client and server. A configured (static) data set is matched but not deleted. Deleting an unknown data set is a service error object-undefined (quirk 31) |
| I./6 | Get Data Set Element Values operation | o / m1 | Yes | MMS Read of a variable list name. Server: yes. Client: used for polling (`PollIntervalms`). Tests: `ICCPLoopbackTest`, `TagTest` |
| I./7 | Set Data Set Element Values operation | o / m1 | No | A Write to a variable list name is not supported (the request is rejected as an invalid argument) |
| I./8 | Get Data Set Names operation | o / m1 | Partial | Server: Yes (GetNameList, class named variable list, domain scope). Client: not issued by the port |
| I./9 | Get Data Set Element Names operation | o / m1 | Partial | Server: Yes (MMS GetNamedVariableListAttributes). Client: not issued by the port |

### A.6 Accounts (Block 8)

| 702 item | Item | 702 | Supported |
|----------|------|-----|-----------|
| I./1 | Account Model | i8 | No |
| I./2 | Query Operation | i8 | No |
| I./3 | TransferAccount Object | i8 | No |
| I./4 | TransmissionSegment Object | i8 | No |
| I./5 | ProfileValue Object | i8 | No |
| I./6 | AccountRequest Object | i | No |
| I./7 | DeviceOutage Object | o | No |
| I./8 | AvailabilityReport Object | i | No |
| I./9 | RealTimeStatus Object | i | No |
| I./10 | ForecastSchedule Object | i | No |
| I./12 | Curve Object | i | No |
| I./13 | Power System Dynamic Objects | i | No |

### A.7 DS transfer sets

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| I./1 | Data Set Transfer Set Model | m1 / m1 | Yes | Client starts transfer sets on the peer (`TransferSets`); server offers a pool (`TransferSetPool`). Transfer sets are ICC objects in `LocalDomain` |
| I./2 | Start Transfer operation | m1 / m1 | Yes | MMS Write of the `DSTransferSet` structure with Status TRUE: 12 members on 1996-08, 13 on 2000-08 (quirk 12). `[V]` in both forms as written by real peers; our 13 member write accepted by a peer is `[S]`. Tests: `Tase2Test`, `ICCPLoopbackTest` |
| I./3 | Stop Transfer operation | m1 / m1 | Yes | The same Write with Status FALSE (quirk 25). The form was never seen from a real peer `[S]`. Client sends it on `Disable()`; server honours it. Tests: `Tase2Test`, `ICCPLoopbackTest` |
| I./4 | Get Next DSTransfer Set operation | m1 / m1 | Yes | A Read of `Next_DSTransfer_Set` in the ICC domain `[V]`; with no free transfer set the server answers object-non-existent `[S]` (quirk 31). A transfer set with a fixed `Name` skips the request. Tests: `Tase2Test`, `TagTest` |
| I./5 | IntervalTimeOut | o / m1 | Yes | Section 7 |
| I./6 | ObjectChange | o / m2 | Yes | Section 7 |
| I./7 | OperatorRequest | o / m1 | Yes | Server: raised by the UI command `OperatorRequest`. Section 7 |
| I./8 | IntegrityTimeout | o / m2 | Yes | Section 7 |
| I./9 | OtherExternalEvent | o / m1 | Yes | Server: raised by the UI command `OtherExternalEvent`. Section 7 |
| I./10 | EventCodeRequested | o / m1 | Partial | The `EventCodeRequested` member is sent by the client (`EventCode`, default 0) and stored by the server, which does not use it. The server's `Event_Code_Detected` carries the code given in the `OtherExternalEvent` command (0 for every other report) |
| I./11 | Start Time | m1 / m1 | Yes | Seconds since 1970-01-01 UTC; 0 = at once. The server waits until the time |
| I./12 | Interval | m1 / m1 | Yes | Seconds |
| I./13 | TLE | o / m2 | Partial | The client sends it. The server discards a report handed over later than `TLE` seconds after it was due, as far as the port's own timing shows; there is no transport back-pressure, so a slow TCP link is not seen (quirk 58). Tests: `ReportAckTest` |
| I./14 | Buffer Time | o / m2 | Yes | Seconds. Section 7 |
| I./15 | Integrity Check | o / m2 | Yes | Seconds. The first integrity report is sent at once when the transfer set starts `[S]` (quirk 26) |
| I./16 | DSConditions Requested | o / m1 | Yes | Bit string of 5 bits (section 7) |
| I./17 | Block Data | o / m3 | Yes | Honoured by the server only with 3 in `SupportedBlocks` (A.2, I./3; quirks 66 to 70). Tests: `BlockedTransferTest` |
| I./18 | Critical | o / m2 | Yes | The client acknowledges reports of a critical transfer set with `Transfer_Report_ACK` (`AcknowledgeCriticalReports`); the server tracks the acknowledgements. A real peer's acknowledgement of a report without a time stamp is `[V]`; the two variable form with `Transfer_Set_Time_Stamp` and the NACK are `[S]` (quirks 56, 57, 59). Tests: `ReportAckTest`, `ICCPLoopbackTest` |
| I./19 | RBE | o / m2 | Yes | Report by exception (quirk 32); the report layout `[V]` for the ordinary and blocked forms |

Note of 702: servers must support the processing of these parameters; this does not imply that the application is present.

### A.8 Time series transfer set objects (Block 9)

| 702 item | Item | 702 | Supported |
|----------|------|-----|-----------|
| I./1 | Time Series Transfer Set Model | i9 | No |
| I./2 | Get Next TSTransfer Set Operation | i9 | No |
| I./3 | EndTimeArrived | i9 / i | No |
| I./4 | ReportIntervalTimeOut | i9 / i | No |
| I./5 | OperatorRequest | i9 / i | No |

### A.9 Transfer account transfer set objects (Block 8)

| 702 item | Item | 702 | Supported |
|----------|------|-----|-----------|
| I./1 | Transfer Account Transfer Set Model | i8 | No |
| I./2 | BeforeTheHour | i8 / i | No |
| I./3 | DispatchUpdate | i8 / i | No |
| I./4 | DuringTheHour | i8 / i | No |
| I./5 | AfterTheHour | i8 / i | No |
| I./6 | ActualDataUpdate | i8 / i | No |
| I./7 | PastHours | i8 / i | No |
| I./8 | ObjectChange | i8 / i | No |
| I./9 | OperatorRequest | i8 / i | No |

### A.10 Information message objects (Block 4)

| 702 item | Item | 702 | Supported |
|----------|------|-----|-----------|
| I./1 | Information Message Transfer Set Model | i4 | No |
| I./1 | InformationBuffer Object | i4 | No |

### A.11 Special transfer set objects

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | Transfer Set Name | o / m1 | Yes | The first member of every report (a structure of scope, domain and name) `[V]`. Quirk 14 |
| I./2 | Next DSTransfer Set | o / o | Yes | `Next_DSTransfer_Set`, ICC scope `[V]` |
| I./3 | Next TSTransfer Set | i / i | No | Block 9 |
| I./4 | Event Code | o / m1 | Yes | `Event_Code_Detected`, an integer (2 octets in the blocked form `[V]`) |
| I./5 | DS ConditionsDetected | o / m1 | Yes | `DSConditions_Detected`, a bit string of 5 bits `[V]` |
| I./6 | TS ConditionsDetected | i / i | No | Block 9 |
| I./7 | TA ConditionsDetected | i / i | No | Block 8 |
| I./8 | Transfer Set Time Stamp | o / m1 | Yes | `Transfer_Set_Time_Stamp`, an integer of seconds; echoed by the acknowledgement of a critical report. The value form is `[S]` (quirk 56) |

The objects at the head of a report are configured per transfer set (`SpecialObjects`, default `Transfer_Set_Name` and `DSConditions_Detected`; the first must be `Transfer_Set_Name`).

### A.12 SBO devices

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | Device Model | m5 / m5 | Partial | Device `D` (write only) and `D_SBO` (the select object), per 503 clause 7.2.6 `[T]` and a real peer's operating flows `[V]` (quirks 39, 42, 51). The readable `SBO` and `ControlConfig` variables of 802 are not implemented (quirk 43) |
| I./2 | Select operation | m5 / m5 | Yes | A Read of `D_SBO` returns the configured CheckBackID and arms the device for `SelectTimeoutms` `[V]` (client side and server side) |
| I./3 | Operate operation | m5 / m5 | Yes | A Write of the bare value to `D` `[V]` (quirks 39, 51, 54); direct operate (no select) too |
| I./4 | Get Tag | o / o | Yes | A Read of `D_TAG`. `[T]/[S]`, never seen from a real peer (quirks 60 to 65). Tests: `TagTest` |
| I./5 | Set Tag | o / o | Yes | A Write of `D_TAG`. `[T]/[S]` (quirks 60 to 65). Tests: `TagTest` |
| I./6 | Timeout Action | o / m7 | No | Needs MMS event notification (Block 7). A select lapses at its timeout without telling the peer (quirk 43) |
| I./7 | Local Reset Action | o / o | Partial | The state effect only (ARMED to IDLE, optionally a new tag) as the UI command `LocalReset`; no event notification (quirk 63). No local reset happens by itself |
| I./8 | Success Action | o / m7 | No | Not signalled as an event; the operate's own answer is the result |
| I./9 | Failure Action | o / m7 | No | Not signalled as an event; the operate's own answer is the result |

### A.13 Programs (Block 6)

| 702 item | Item | 702 | Supported |
|----------|------|-----|-----------|
| I./1 | Program Model | i6 | No |
| I./2 | Start Operation | i6 | No |
| I./3 | Stop Operation | i6 | No |
| I./4 | Resume Operation | i6 | No |
| I./5 | Reset Operation | i6 | No |
| I./6 | Reset Operation (second row of the table) | i6 | No |
| I./7 | Kill Operation | i6 | No |
| I./8 | Get Program Attributes Operation | o / i6 | No |

### A.14 Event enrollments and A.15 Event conditions (Block 7)

| Table | 702 item | Item | 702 | Supported |
|-------|----------|------|-----|-----------|
| A.14 | I./1 | Event Enrollment Model | i7 | No |
| A.14 | I./2 | Create Event Enrollment Operation | i7 | No |
| A.14 | I./3 | Delete Event Enrollment Operation | i7 / i | No |
| A.14 | I./5 | Get Event Enrollment Attributes Operation | i7 / i | No |
| A.15 | I./1 | Event Condition Model | i7 | No |
| A.15 | I./2 | Event Notification Action | i7 | No |

### A.16 Object models

| 702 item | Item | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| I./1 | IndicationPoint Object | m1 | Yes | |
| I./2 | ControlPoint Object | m5 | Partial | See A.12 |
| I./3 | ProtectionEquipmentEvent Object | o | No | |
| I./4 | TransferAccount Object | i8 | No | |
| I./5 | TransmissionSegment Object | i8 | No | |
| I./6 | ProfileValue Object | i8 | No | |
| I./7 | AccountRequest Object | i | No | |
| I./8 | DeviceOutage Object | o | No | |
| I./9 | InformationBuffer Object | o4 | No | Block 4 |
| I./10 | AvailabilityReport Object | i | No | |
| I./11 | RealTimeStatus Object | i | No | |
| I./12 | ForecastSchedule Object | i | No | |
| I./13 | Curve Object | i | No | |
| I./14 | Power System Dynamic Objects | i | No | |

## 6. TASE.2 objects and data types

### Data value types (point types)

The point type is taken from the configuration (`Type`), never inferred, because `{integer, flags}` is ambiguous between several types (quirk 19).
Time tags are whole seconds; milliseconds are carried only by the `Extended` types (quirk 22). Defaults: Binaries `StateQ`, Analogs `RealQ`, Counters `DiscreteQ`.

| Object type | Layout on the wire | Supported | Basis |
|-------------|--------------------|-----------|-------|
| Data_Real | float | Yes | S |
| Data_RealQ | structure { float, Data_Flags } | Yes | V |
| Data_RealQTimeTag | structure { TimeStamp, float, Data_Flags } | Yes | S |
| Data_RealExtended | structure { TimeStampExtended, float, Data_Flags, COV } | Yes | S |
| Data_State | bit string (2): state only | Yes | S |
| Data_StateQ | bit string (8): state and flags in one octet | Yes | V |
| Data_StateQTimeTag | structure { TimeStamp, StateQ } | Yes | S |
| Data_StateExtended | structure { TimeStampExtended, StateQ, COV } | Yes | S |
| Data_Discrete | integer | Yes | S |
| Data_DiscreteQ | structure { integer, Data_Flags } | Yes | S |
| Data_DiscreteQTimeTag | structure { TimeStamp, integer, Data_Flags } | Yes | S |
| Data_DiscreteExtended | structure { TimeStampExtended, integer, Data_Flags, COV } | Yes | S |
| Data_Flags | bit string (8): state (2 bits), validity (2), current source (2), normal value (1), time stamp quality (1) | Yes | V |
| TimeStamp | integer, seconds since 1970-01-01 UTC | Yes | S |
| TimeStampExtended | structure { TimeStamp, milliseconds } | Yes | S |
| COV | integer counter of value changes | Yes | S |

Analog values are written as MMS floating point in the 32 bit form `[V]` (the 64 bit form is also decoded). Tests: `Tase2Test` ("Point values: every type encodes, decodes and is recognised"), `BlockedTransferTest` (the blocked octets of every type, which are `[V]` for `RealQ` and `StateQ` only).
Mapping to ODC: State types to Binary events (On is true), Real and Discrete types to Analog or Counter events. The quality mapping to ODC `QualityFlags` is lossy in both directions and the raw flags octet is kept (quirk 20; [Points, data sets and quality](README.md#points-data-sets-and-quality)).
Not mapped: normal-value (abnormal) and time stamp quality flags.

### Association objects

| Object | Scope | Supported | Note |
|--------|-------|-----------|------|
| Bilateral_Table_ID | ICC (domain-specific) | Yes | Visible string of at most 32 characters (type described as a varying length `[S]`, quirk 55). Served only when `LocalDomain` and `BilateralTableID` are set; read from the peer's `RemoteDomain` and compared with `RemoteBilateralTableID`. Quirk 21 `[V]` |
| TASE2_Version | VCC (VMD-specific) | Yes | Structure of two integers: 1996/8 or 2000/8 `[V]`. A difference from `TASE2Edition` refuses the peer unless `RequireMatchingEdition` is false |
| Supported_Features | VCC | Yes | Section 4 |
| Next_DSTransfer_Set | ICC | Yes | A.7, I./4 |
| Transfer_Set_Name, DSConditions_Detected, Event_Code_Detected, Transfer_Set_Time_Stamp | ICC (domain of the transfer set) | Yes | A.11 |
| Transfer_Report_ACK, Transfer_Report_NACK | ICC (domain of the transfer set) | Yes | An unconfirmed InformationReport; the single variable form `[V]`, the form with the time stamp and the NACK `[S]` (quirks 56, 57, 59) |
| DSTransferSet | ICC | Yes | 12 members (1996-08) or 13 members (2000-08, with `AllChangesReported`); both accepted, the form written follows the peer's edition (`DSTransferSetStatusMember`) |
| Device `D`, `D_SBO` | ICC or VCC | Yes | Block 5, A.12 |
| `D_TAG` (Tag_Value) | same as `D` | Yes | Structure { Flags (bit string of 3), Reason (visible string of at most 255) }. The bit order of the tag value is an assumption `[S]` (quirk 60) |
| Device command types | - | Yes | `Command`, `DiscreteCommand` (an integer of width 16, two configured values), `RealSetpoint` (a 32 bit float), `DiscreteSetpoint` (an integer of width 16). Quirks 39, 54 |
| Block_Data (blocked report variable) | VMD-specific | Yes | `[V]`; the id is `BlockedDataName` (default `Block_Data`) |
| Information Message, TS and TA objects, accounts, programs, events | - | No | Not implemented |

### Local reset, tags and local actions (UI commands)

| Command | Side | Supported | Note |
|---------|------|-----------|------|
| `OperatorRequest`, `OtherExternalEvent` | server | Yes | Send a full report from each enabled transfer set that asked for the condition |
| `LocalReset`, `LocalSetTag`, `Tags` | server | Yes | `[T]/[S]` (quirks 60 to 65) |
| `GetTag`, `SetTag` | client | Yes | `[T]/[S]` (quirk 65) |

See [UI commands, status and statistics](README.md#ui-commands-status-and-statistics).

## 7. Transfer set conditions and parameters

Conditions (DSConditions, a bit string of 5 bits; bit `i` is set for condition `i`; the bit numbers are `[V]`, the names from the standard):

| Bit | Condition | Client (requests) | Server (raises) | Note |
|-----|-----------|-------------------|-----------------|------|
| 0 | IntervalTimeOut | Yes | Yes | A report every `Interval` seconds. With `RBE` only what changed (none if nothing changed). `Interval` 0 with this condition is a configuration error |
| 1 | IntegrityTimeOut | Yes | Yes | A complete report every `IntegrityCheck` seconds, whatever `RBE` says; the first one at once `[S]` |
| 2 | ObjectChange | Yes | Yes | After `BufferTime` seconds from the first change; with `RBE` only the changed members |
| 3 | OperatorRequest | Yes | Yes | By the UI command, a full report |
| 4 | OtherExternalEvent | Yes | Yes | By the UI command, a full report with an event code |
| - | EventCodeRequested | Yes (sent) | Partial | See A.7, I./10 |

Default `Conditions` of a client transfer set: `ObjectChange`, plus `IntervalTimeOut` when `Interval` is above 0 and `IntegrityTimeOut` when `IntegrityCheck` is above 0.

| Parameter | Client | Server | Note |
|-----------|--------|--------|------|
| StartTime | Yes | Yes | |
| Interval | Yes | Yes | |
| TLE | Yes | Partial | Quirk 58. Enforced only as far as the port's own timing shows |
| BufferTime | Yes | Yes | |
| IntegrityCheck | Yes | Yes | |
| DSConditionsRequested | Yes | Yes | |
| BlockData | Yes | Yes (with 3 in `SupportedBlocks`) | Block 3 |
| Critical | Yes | Yes | Acknowledgement and tracking. A missing or negative acknowledgement is a logged and counted `Data_failure` (no event condition is raised, Block 7); no resend. `ReportAckTimeoutms` and `MaxUnacknowledgedReports` are not from the standard `[S]` |
| RBE | Yes | Yes | |
| AllChangesReported (2000-08 only) | Yes | Yes | Valid only with 2000-08, `RBE`, `ObjectChange` and `BufferTime` above 0, checked when the configuration is loaded (quirk 50). The server sends consecutive ordinary reports in the order of the changes `[S]` (quirk 53) |
| EventCodeRequested | Yes | stored only | |
| Status (start / stop) | Yes | Yes | |

Reports (unconfirmed MMS InformationReport): the head carries the special objects, then the member values; both the inline variable list and the data set name forms are decoded; the server sends the name form for a complete report when the data set starts with the special objects, and the inline form with only the changed members for RBE (quirk 13). The client acknowledges (`Transfer_Report_ACK`) or negatively acknowledges (`Transfer_Report_NACK`) as described above; a report that is not from a transfer set this port started is discarded and NACKed (`NackUnknownReports`, quirk 59).

Detail: [Transfer set settings explained](README.md#transfer-set-settings-explained), [Critical reports and the time limit TLE](README.md#critical-reports-and-the-time-limit-tle).

## 8. MMS requirements lists (A.4)

### A.4.2 to A.4.4 identifiers, strings and object names

| 702 clause | Requirement | Supported | Note |
|------------|-------------|-----------|------|
| A.4.2 | MMS Identifier: BasicIdentifier, at most 32 characters, not starting with a digit | Partial | The codec and the configuration accept identifiers up to 255 characters and do not check the character set or a leading digit. `Bilateral_Table_ID` is limited to 32 visible ASCII characters. A transfer set name or domain longer than 32 octets makes a blocked report fall back to the ordinary form. ExtendedIdentifier is not used |
| A.4.3 | MMSString: ISO 646 only | Partial | Data values use visible strings; MMSString data are decoded, not produced |
| A.4.4 | ObjectName: BasicIdentifier | Yes | VMD-specific and domain-specific names; AA-specific names are decoded but TASE.2 does not use them |
| 6.3 | A data structure nesting level above 1 | Yes | Proposed `NestingLevel` default 4 (1 to 32); the smaller of both ends is used. Do not configure 1 |

### A.17 Supported MMS PDUs (environment and general management)

Client-CR (A.17a) and Server-CR (A.17b), F/S status as `sending, receiving`.

| 702 item | PDU | Client-CR | Server-CR | Supported | Note |
|----------|-----|-----------|-----------|-----------|------|
| 1 | InitiateRequest | m, - | -, m | Yes | Sent by the ACSE initiator (the TCP initiator), received by the responder. Tests: `MmsTest`, `IccpStackTest` |
| 2 | InitiateResponse | -, m | m, - | Yes | |
| 3 | InitiateError | -, m | m, - | Partial | Received and decoded (the association is reported as refused). Not sent: a refused association is refused at the ACSE level (AARE `rejected-permanent` in a session REFUSE); no Initiate-Error PDU is built |
| 4 | ConcludeRequest | m, m | m, m | Yes | A collision is resolved in favour of the association's initiator (quirk 24 `[S]`) |
| 5 | ConcludeResponse | m, m | m, m | Yes | |
| 6 | ConcludeError | m, m | m, m | Partial | Received and decoded. Sent only to refuse a Conclude that collides with our own (class conclude, no further code) |
| 7 | CancelRequest | i, i | i, i | No | Not decoded; an undecodable PDU other than a confirmed request ends the association with an abort. Tests: `MmsTest` |
| 8 | CancelResponse | i, i | i, i | No | As above |
| 9 | CancelError | i, i | i, i | No | As above |
| 10 | Reject | m, m | m, m | Yes | Sent for: unrecognised service, invalid argument, invalid invoke id, outstanding limit exceeded. The reason numbers follow ISO 9506-2 as remembered `[S]` (quirk 38). Received Rejects complete the matching request. Tests: `IccpStackTest` |

Abort is provided by ACSE (see section 9).

### A.18 MMS modifiers

| 702 item | Modifier | 702 | Supported | Note |
|----------|----------|-----|-----------|------|
| 1 | attachToEventCondition | x / x | No | A request with a list of modifiers is not decoded and is rejected as an invalid argument. Tests: `MmsTest` |
| 2 | attachToSemaphore | x / x | No | As above |

### A.19 Parameter CBBs

The port proposes `str1, str2, vnam, vlis` and answers with the intersection with the peer's proposal.

| 702 item | Parameter CBB | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|---------------|-----------------------------|-----------|------|
| 1 | str1 (arrays) | m / m | Yes | Advertised; arrays are decoded and encoded |
| 2 | str2 (structures) | m / m | Yes | Advertised |
| 3 | vnam (named variables) | m / m | Yes | Advertised |
| 4 | valt (alternate access) | o / o | No | Not advertised. An alternateAccess element is carried as an opaque element by the codec; the server ignores it on a Read and refuses it in a DefineNamedVariableList |
| 5 | vadr (addressed variables) | i / i | No | An Address specification is not decoded |
| 6 | vsca (scattered access) | i / i | No | |
| 7 | tpy (named types) | i / i | No | |
| 8 | vlis (named variable lists) | m / m | Yes | Advertised |
| 9 | real (floating point) | i / i | Partial | The bit is not set in the proposal, although floating point data are used for analog values as real peers do. To be confirmed: whether a peer requires the bit |
| 10 | bit 9 | i / i | No | |
| 11 | cei | i / i | No | |
| 12 | ACO | i / i | No | |
| 13 | SEM | i / i | No | |
| 14 | CSR | i / i | No | |
| 15 | CSNC | i / i | No | |
| 16 | CSPLC | i / i | No | |
| 17 | CSPI | i / i | No | |

### A.20 VMD support

The services supported bit string (85 bits) sets: GetNameList, Identify, Read, Write, GetVariableAccessAttributes, DefineNamedVariableList, GetNamedVariableListAttributes, DeleteNamedVariableList, InformationReport, Conclude.

| 702 item | Service | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|---------|-----------------------------|-----------|------|
| 1 | Status | x / x | No | Answered with a Reject (unrecognised service) |
| 2 | Unsolicited Status | x / x | No | |
| 3 | GetNameList | o / m | Partial | Server: Yes (A.33). Client: not issued by the port |
| 4 | Identify | o / m | Partial | Server: Yes (vendor, model, revision from the configuration). Client: not issued by the port (the stack can send it; `IccpStackTest`) |
| 5 | Rename | x / x | No | |
| 6 | GetCapabilityList | x / x | No | |

### A.21 to A.32 excluded services

All of the following are `x / x` in 702 (excluded) and are not implemented (Supported: No). A confirmed request for one of them is rejected (Reject, unrecognised service).

| Table | Services |
|-------|----------|
| A.21 Domain management | InitiateDownloadSequence, DownloadSegment, TerminateDownloadSequence, InitiateUploadSequence, UploadSegment, TerminateUploadSequence, RequestDomainDownload, RequestDomainUpload, LoadDomainContent, StoreDomainContent, DeleteDomain, GetDomainAttributes (12 items) |
| A.22 Program invocation management | CreateProgramInvocation, DeleteProgramInvocation, Start, Stop, Resume, Reset, Kill, GetProgramInvocationAttributes, ReconfigureProgramInvocation (9 items) |
| A.24 Semaphore management | TakeControl, RelinquishControl, DefineSemaphore, DeleteSemaphore, ReportSemaphoreStatus, ReportPoolSemaphoreStatus, ReportSemaphoreEntryStatus (7 items) |
| A.25 Operator communication | Input, Output (2 items) |
| A.26 Event management | DefineEventCondition, DeleteEventCondition, GetEventConditionAttributes, ReportEventConditionStatus, AlterEventConditionMonitoring, TriggerEvent, DefineEventAction, DeleteEventAction, GetEventActionAttributes, ReportEventActionStatus, DefineEventEnrollment, DeleteEventEnrollment, GetEventEnrollmentAttributes, ReportEventEnrollmentStatus, AlterEventEnrollment, EventNotification, AcknowledgeEventNotification, GetAlarmSummary, GetAlarmEnrollmentSummary (19 items; 702 notes "c3: m if Block 7 is supported, else i") |
| A.27 Journal management | ReadJournal, WriteJournal, InitializeJournal, ReportJournalStatus, CreateJournal, DeleteJournal (6 items) |
| A.28 File access | ObtainFile (1 item) |
| A.29 File management | FileOpen, FileRead, FileClose (3 items) |
| A.30 Data exchange management | GetDataExchangeAttributes, ExchangeData (2 items) |
| A.31 AccessControl | DefineAccessControlList, GetAccessControlListAttributes, ReportAccessControlledObjects, DeleteAccessControlList, AlterAccessControl (5 items) |
| A.32 Additional PDUs | AdditionalPDU (1 item) |

### A.23 Variable access

| 702 item | Service | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|---------|-----------------------------|-----------|------|
| 1 | Read | m / m | Yes | Tests: `MmsTest`, `Tase2Test`, `ICCPLoopbackTest` |
| 2 | Write | m / m | Yes | Server: transfer set start / stop, device operate, device tag. A Write to anything else gives object-access-denied |
| 3 | InformationReport | m / m | Yes | Transfer reports, `Transfer_Report_ACK` / `NACK`, blocked reports; both directions |
| 4 | GetVariableAccessAttributes | m / m | Partial | Server: Yes. Client: not issued by the port |
| 5 | DefineNamedVariable | x / x | No | |
| 6 | DefineScatteredAccess | x / x | No | |
| 7 | GetScatteredAccessAttributes | x / x | No | |
| 8 | DeleteVariableAccess | x / x | No | |
| 9 | DefineNamedVariableList | m / m | Yes | See A.40 |
| 10 | GetNamedVariableListAttributes | m / m | Partial | Server: Yes. Client: not issued by the port |
| 11 | DeleteNamedVariableList | m / m | Yes | See A.42 |
| 12 | DefineNamedType | x / x | No | |
| 13 | GetNamedTypeAttributes | x / x | No | |
| 14 | DeleteNamedType | x / x | No | |

### A.33 GetNameList

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | ObjectClass | m / m | Yes | Served classes: named variable, named variable list, domain (VMD scope). Any other basic class gets an empty list. The extended class (`vmd`, "formerly VMD-specific in MMS V1.0") is not decoded and the request is rejected, as the note of A.33 requires. Tests: `MmsTest` |
| Request | ObjectScope | m / m | Yes | VMD-specific and domain-specific; the aa-specific scope is decoded and answered with an empty list |
| Request | DomainName | o / m | Yes | Server: an unknown domain gives a service error (object-undefined) |
| Request | ContinueAfter | m / m | Yes | Pages of `NamePageSize` names (default 200); an unknown continuation name gives a service error. Tests: `Tase2Test`, `MmsTest` |
| Response+ | ListOfIdentifier | m / m | Yes | |
| Response+ | MoreFollows | m / m | Yes | Omitted when TRUE (its default), written as FALSE on the last page; an absent value is read as TRUE (quirk 10 `[V]`) |
| Response- | Error type | m / m | Yes | |

### A.34 VariableAccessSpecification

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| listOfVariable | | o / m | Yes | |
| variableSpecification | | o / m | Yes | |
| alternateAccess | | o / o | No | Carried opaquely; ignored by a Read, refused in a DefineNamedVariableList |
| variableListName | | o / m | Yes | Data set reads, `InformationReport` by data set name |

### A.35 VariableSpecification

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| name | | o / m | Yes | |
| address | | o / i | No | Not decoded (the request is rejected) |
| variableDescription | | o / i | No | |
| scatteredAccessDescription | | i / i | No | |
| invalidated | | i / i | No | |

### A.36 Read

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | specificationWithResult | o / m | Partial | Server: honoured. Client: not requested (FALSE) |
| Request | variableAccessSpecification | m / m | Yes | |
| Response | variableAccessSpecification | o / m | Yes | Written by the server when asked for; the client reads it when present |
| Response | listOfAccessResult | m / m | Yes | Data access errors per variable |

### A.37 Write

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | variableAccessSpecification | m / m | Yes | A list of variables only; a variable list name is rejected as an invalid argument |
| Request | listOfData | m / m | Yes | One data item per variable, otherwise rejected |
| Response | failure | m / m | Yes | |
| Response | success | m / m | Yes | |

### A.38 InformationReport

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | variableAccessSpecification | m / m | Yes | List of variables or the data set name |
| Request | listOfAccessResult | m / m | Yes | Unconfirmed PDU only |

### A.39 GetVariableAccessAttributes

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | name | o / m | Yes | Server. Client: not issued by the port |
| Request | address | o / i | No | Not decoded |
| Response | mmsDeletable | m / m | Yes | FALSE for points, devices and tags |
| Response | address | i / i | No | |
| Response | typeDescription | m / m | Yes | All type descriptions of the point types and device types are encoded; the type descriptions are `[S]` (quirks 55, 60) |

### A.40 DefineNamedVariableList

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | variableListName | m / m | Yes | Server: domain-specific, in `LocalDomain` only |
| Request | listOfVariable | m / m | Yes | |
| Request | variableSpecification | m / m | Yes | By name. A member that is not a point, a special object or a tag is refused (object-undefined) |
| Request | alternateAccess | i / m | No | Not sent by the client; refused by the server (attribute-inconsistent). 702 asks the server to accept it |
| Response | | m / m | Yes | |

### A.41 GetNamedVariableListAttributes

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | ObjectName | m / m | Yes | Server. Client: not issued by the port |
| Response | mmsDeletable | m / m | Yes | TRUE for a data set the peer created |
| Response | listOfVariable | m / m | Yes | |
| Response | variableSpecification | m / m | Yes | |
| Response | alternateAccess | m / i | Partial | Carried opaquely when decoded; never written |

### A.42 DeleteNamedVariableList

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| Request | Scope | m / m | Yes | Specific, domain and VMD scope are served; aa-specific scope is refused |
| Request | listOfVariableListName | m / m | Yes | |
| Request | domainName | m / m | Yes | |
| Response | numberMatched | m / m | Yes | |
| Response | numberDeleted | m / m | Yes | A configured data set is matched, not deleted |
| Response | DeleteNamedVariableList-Error | m / m | Yes | Object-undefined when nothing matched |

### MMS data and type descriptions

| Item | Supported | Note |
|------|-----------|------|
| Data: array, structure, boolean, bit string, integer, unsigned, floating point (32 and 64 bit), octet string, visible string, generalized time, binary time, BCD, boolean array, object identifier, MMS string, UTC time | Yes | Decoded and encoded; scalars keep their content octets exactly as received (quirks 8, 9). Tests: `MmsTest` |
| TypeDescription: all alternatives | Yes | Every alternative round trips (`MmsTest`) |
| FloatingPoint formats | Partial | Exponent widths 8 (32 bit) and 11 (64 bit) only; another format is a decoding error |
| Decoder limits | - | 100000 list entries, 200000 data nodes per PDU, nesting 32, identifiers of 255 octets (BER nesting 64) |
| Invoke ids | Yes | Per direction; outstanding limit negotiated at Initiate; requests beyond the peer's limit are queued (up to 10000) |

## 9. ACSE (A.5)

### A.43 and A.44 Protocol versions

| 702 item | Version | Supported | Note |
|----------|---------|-----------|------|
| A.A.2/1 | Version 1 | Yes | The only ACSE protocol version; the protocol-version field is always written (quirk 1 `[V]`), reported on receipt and not enforced |
| A.A.2/2 | Version 2 | No | |

A.45 (technical corrigenda implemented): none stated. A.46 (all mandatory features implemented): the mandatory items of the tables below are implemented except where marked No or Partial; the exceptions are listed in section 16.

### A.47 to A.51 Protocol mechanisms and procedures

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| A.A.7/1 | Normal mode | m / m | Yes | |
| A.A.7/2 | X.410-1984 mode | i / i | No | |
| A.A.7/3 | Rules for extensibility | m / m | Yes | Unknown optional fields are skipped (`AcseTest`) |
| A.A.7/4 | Supports operation of SessionV2 | m / m | Yes | |
| A.A.6.1/1 | Association establishment: Initiator | m / i | Yes | The TCP initiator, whatever the service role |
| A.A.6.1/2 | Association establishment: Responder | i / m | Yes | |
| A.A.6.2/1 | Normal release: Initiator | m / i | Yes | RLRQ in a session FINISH after an MMS Conclude (quirk 45) |
| A.A.6.2/2 | Normal release: Responder | i / m | Yes | RLRE in a session DISCONNECT |
| A.A.6.3/1 | Abnormal release: Initiator | m / m | Yes | ABRT in a session ABORT |
| A.A.6.3/2 | Abnormal release: Responder | m / m | Yes | |
| A.A.8/1 | Functional unit: Normal mode | m / m | Yes | |
| A.A.8/2 | Functional unit: Authentication | m / m | Partial | MACE (section 13), when built with `ODC_ASIO_SSL`. Without it the authentication fields are carried and ignored (a warning is logged). 62351-4 HMAC: No |

### A.52 ACSE supported APDUs

| 702 item | APDU | 702 (Client-CR / Server-CR) snd,rcv | Supported | Note |
|----------|------|-------------------------------------|-----------|------|
| A.A.9/1 | A-associate-request (AARQ) | m, i / i, m | Yes | |
| A.A.9/2 | A-associate-response (AARE) | i, m / m, i | Yes | Accepted, or refused (`rejected-permanent`, with a diagnostic, in a session REFUSE; quirk 49) |
| A.A.9/3 | A-release-request (RLRQ) | o, m / o, m | Yes | Reason `normal`; a peer's reason is accepted (quirk 45) |
| A.A.9/4 | A-release-response (RLRE) | m, o / m, o | Yes | |
| A.A.9/5 | A-abort (ABRT) | o, m / o, m | Yes | Source acse-service-user; no diagnostic written |

Per the note of A.52 there is no requirement on either party to terminate an association: a transport that closes after the Conclude, or a peer FINISH without a Conclude, is accepted.

### A.53 AARQ parameters

| 702 item | Parameter | 702 | Supported | Note |
|----------|-----------|-----|-----------|------|
| A.A.10.1/1 | Protocol version | c, m | Yes | Always written; not enforced on receipt |
| A.A.10.1/2 | Application context name | m | Yes | MMS context `1.0.9506.2.3`; any other is refused (diagnostic application-context-name-not-supported) |
| A.A.10.1/3 | Calling AP title | o, m | Yes | `LocalAPTitle`, form 2 (an object identifier), sent when configured |
| A.A.10.1/4 | Calling AE qualifier | o, m | Yes | `LocalAEQualifier`, form 2 (an integer), sent when configured |
| A.A.10.1/5 | Calling AP invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.1/6 | Calling AE invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.1/7 | Called AP title | o, m | Yes | `RemoteAPTitle`, sent when configured; on receipt it is not checked |
| A.A.10.1/8 | Called AE qualifier | o, m | Yes | `RemoteAEQualifier`; not checked on receipt |
| A.A.10.1/9 | Called AP invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.1/10 | Called AE invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.1/11 | Implementation information | o, m | Partial | Decoded; never sent |
| A.A.10.1/12 | User information | o, m | Yes | The MMS Initiate request as an EXTERNAL with an indirect reference and a single-ASN.1-type encoding (quirk 7) |
| A.A.10.1/13 | ACSE user authentication per IEC/TS 62351-4 | o, o | Partial | MACE authentication fields (sender-acse-requirements, mechanism-name, calling-authentication-value); not the 62351-4 HMAC mechanism (section 13) |

### A.54 AARE parameters

| 702 item | Parameter | 702 | Supported | Note |
|----------|-----------|-----|-----------|------|
| A.A.10.2/1 | Protocol version | c, m | Yes | |
| A.A.10.2/2 | Application context name | m | Yes | |
| A.A.10.2/3 | Responding AP title | o, m | Yes | `LocalAPTitle` when configured |
| A.A.10.2/4 | Responding AE qualifier | o, m | Yes | `LocalAEQualifier` when configured |
| A.A.10.2/5 | Responding AP invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.2/6 | Responding AE invocation-id | o, m | Partial | Decoded; never sent |
| A.A.10.2/7 | Result | m | Yes | accepted, rejected-permanent |
| A.A.10.2/8 | Result source diagnostic | m | Yes | acse-service-user diagnostics (see the constants in `Acse.h`, including authentication-required) |
| A.A.10.2/9 | Implementation information | o, m | Partial | Decoded; never sent |
| A.A.10.2/10 | User information | - | Yes | The MMS Initiate response |
| A.A.10.2/11 | ACSE user authentication per IEC/TS 62351-4 | o, o | Partial | MACE only |

### A.55 to A.57 RLRQ, RLRE and ABRT

| 702 item | Parameter | 702 | Supported | Note |
|----------|-----------|-----|-----------|------|
| A.A.10.3/1 | RLRQ reason | o, m | Yes | `normal` sent; any accepted |
| A.A.10.3/2 | RLRQ user information | o, m | Partial | Decoded; never sent |
| A.A.10.4/1 | RLRE reason | o, m | Yes | `normal` sent (a real peer sends not-finished; both accepted) |
| A.A.10.4/2 | RLRE user information | o, m | Partial | Decoded; never sent |
| A.A.10.5/1 | ABRT reason (abort source) | m | Yes | |
| A.A.10.5/2 | ABRT user information | o, m | Partial | Decoded; never sent |

### A.58 AE title syntax name form

| 702 item | Form | Base / F/S sending, receiving | Supported | Note |
|----------|------|-------------------------------|-----------|------|
| A.A.11/1 | Form 1 (Directory Name) | o i, m m | Partial | Received and carried as an uninterpreted BER element; the responder ignores a form 1 title (it is not recorded as the client identity). Never sent |
| A.A.11/2 | Form 2 (Object Id and Integer) | o m, m m | Yes | AP title as an object identifier, AE qualifier as an integer; `LocalAPTitle` and so on are dotted OIDs and numbers |

### A.59 Authentication value form

| 702 item | Form | 702 | Supported | Note |
|----------|------|-----|-----------|------|
| A.A.11.2/1 | GraphicString | o / o | No | Carried as an opaque element by the codec; not produced or interpreted |
| A.A.11.2/2 | BIT STRING | o / o | No | As above |
| A.A.11.2/3 | EXTERNAL | c1 / c1 | Partial | Used by MACE (EXTERNAL holding certificate, signing time and signature). "Shall be per IEC/TS 62351-4 if supported": the MACE structure is what a real peer sends (`[V]`), and it is not claimed to be the 62351-4 form |
| A.A.11.2/4 | Other | o / o | No | |

## 10. Presentation (A.6)

### A.60 Presentation PRL

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| P.A.5.1/1 | X.410 (1984) mode | i / i | No | |
| P.A.5.1/2 | Normal mode | m / m | Yes | |
| P.A.5.2/1 | Kernel functional unit | m / m | Yes | |
| P.A.5.2/2 | Presentation context management functional unit | - | No | |
| P.A.5.2/3 | Presentation context restoration functional unit | - | No | |
| P.A.6.1.1.1/1 | Initiator (presentation connection) | c21 / c21 | Yes | CP carries the ACSE AARQ in a PDV of context 1 |
| P.A.6.1.1.1/2 | Responder (presentation connection) | c22 / c22 | Yes | CPA (or CPR for a refusal) |
| P.A.6.1.1.2/1 | Requestor (normal data) | - | Yes | One single-ASN.1-type PDV per PPDU |
| P.A.6.1.1.2/2 | Acceptor (normal data) | - | Yes | |
| P.A.6.1.1.3/1 | Requestor (orderly release) | c23 / c23 | Yes | Carried by the ACSE RLRQ / RLRE in the session FINISH / DISCONNECT user data |
| P.A.6.1.1.3/2 | Acceptor (orderly release) | c24 / c24 | Yes | |

Other requirements of 702 clause 6.4 (the presentation protocol shall support the default context negotiation, the default context name and the simply encoded data options):

| Option | Supported | Note |
|--------|-----------|------|
| Default context name | No | The field is encoded and decoded by the codec; the stack neither proposes nor uses a default context |
| Default context negotiation | No | Contexts are negotiated explicitly (ACSE and MMS) |
| Simply encoded data | No | A PPDU with simply-encoded or arbitrary-bit-string data is a decoding error |

Contexts, selectors and abstract syntaxes: section 15.

## 11. Session (A.7)

### A.61 Session PRL

| 702 item | Item | 702 (Client-CR / Server-CR) | Supported | Note |
|----------|------|-----------------------------|-----------|------|
| S.A.3.1/2 | Version 2 | m / m | Yes | Version 2 is written in CN and AC; a peer's version is not checked |
| S.A.6.1/4 | Duplex functional unit | m / m | Yes | The only requirement proposed (PI 20 = duplex) |
| S.A.6.2/2 | Reuse of transport connection | i / i | No | |
| S.A.6.2/4 | Extended concatenation (sending) | i / i | No | |
| S.A.6.2/5 | Extended concatenation (receiving) | i / i | No | |
| S.A.7.1.1.1 | Initiator (session connection) | c21 / c21 | Yes | CN |
| S.A.7.1.1.1 | Responder (session connection) | c22 / c22 | Yes | AC, or RF for a refusal (with the presentation data) |
| S.A.7.1.1.2 | Requestor (orderly release) | c23 / c23 | Yes | FN with user data |
| S.A.7.1.1.2 | Acceptor (orderly release) | c24 / c24 | Yes | DN with user data |
| S.A.7.1.1.3 | Requestor (normal data transfer) | m / m | Yes | The GIVE TOKENS + DATA TRANSFER pair `01 00 01 00` |
| S.A.7.1.1.3 | Acceptor (normal data transfer) | m / m | Yes | |
| S.A.7.1.2/2 | Overflow Accept (OA) | i / i | No | |
| S.A.7.1.2/3 | Connect Data Overflow (CDO) | i / i | No | |
| S.A.7.5.1 | Expedited data (requestor, acceptor) | - | No | |
| S.A.7.6.1 | Typed data (requestor, acceptor) | - | No | |
| S.A.7.7.1 | Capability data (requestor, acceptor) | - | No | |
| S.A.7.8.1 | Minor synchronize (requestor, acceptor) | - | No | |
| S.A.7.10.1 | Major synchronize (requestor, acceptor) | - | No | |
| S.A.7.13.1 | Activity start, resume, interrupt, discard, end (requestor, acceptor) | - | No | |
| S.A.7.13.1 | Give tokens confirm (requestor, acceptor) | o / o | No | The GIVE TOKENS SPDU is sent only as the prefix of a data SPDU |
| S.A.8.1.3/4 | Data Overflow item | i / i | No | |
| S.A.8.2/1 | TSDU maximum size (OA) | i / i | No | |
| S.A.8.2/2 | Version number (OA) | i / i | No | |
| S.A.8.3/1 | Enclosure item (CDO) | i / i | No | |
| S.A.8.3/1 | User data (CDO) | i / i | No | |

Session user data in a CN or AC longer than 512 octets uses extended user data (PI 194, up to 10240 octets) in a CN, and PI 193 with a 3 octet length in an AC, as a real peer does (quirk 44 `[V]`); both are accepted in both SPDUs. The calling and called session selectors are written when configured and are not checked on receipt.

## 12. Transport and lower layers

702 clause 6.5 asks for TCP/IP with RFC 2126; IPv4 is mandatory. This implementation uses RFC 1006 (TPKT and ISO 8073 class 0) over TCP.

| Item | Supported | Note |
|------|-----------|------|
| TCP over IPv4 | Yes | Through the opendatacon `TCPSessionManager` (on a `TCPSocketManager`). Client: connects, with retry (`TCPConnectRetryPeriodms`, 0 = exponential back-off, delay at least 1 s); server: listens on `IP` / `Port` (default 102, 3782 with TLS) and accepts one connection at a time |
| TCP over IPv6 | To be confirmed | Not tested and not documented |
| RFC 1006 TPKT | Yes | Version 3, 4 octet header, length 7 to 65535; a stream reassembler accepts arbitrary chunking; a malformed header ends the connection (it cannot be resynchronised). Tests: `TPKTTest` |
| COTP class 0 (ISO 8073) | Yes | CR, CC, DR, DT. Tests: `COTPTest`, `InteropVectorsTest`. Class 2 and above, ER, expedited data and extended formats are not supported (a decoding error) |
| COTP parameters | Yes | Calling TSAP, called TSAP (up to 32 octets) and TPDU size in the CR and CC, in that order (quirk 4 `[V]`) |
| TPDU size | Yes | 128, 256, 512, 1024, 2048, 4096 or 8192 (`TPDUSize`, default 1024); the smaller of both ends is used. A size absent in a CR is read as 128 |
| Segmentation (EOT) | Yes | A TSDU larger than the TPDU is split in DTs, the last with EOT; reassembly is limited to 1 MiB per TSDU |
| RFC 2126 (ITOT) additions | No | Class 0 as in RFC 1006 only; interworking was verified with a real peer using this profile |
| Transport priority (702 clause 6.4) | No | See A.3, I./2 |
| TLS (IEC 62351-3) | Yes (with `ODC_ASIO_SSL`) | Section 13 |

## 13. Authentication and security

| Item | Supported | Note |
|------|-----------|------|
| IEC 62351-3 (TLS on the TCP connection) | Yes, when built with `ODC_ASIO_SSL` | TLS 1.2 and 1.3 (older versions and compression are not offered); mutual authentication (the server requires a client certificate by default); certificate verification against a CA file or directory; optional CRL (fail-closed), `PeerCertAllowList` (CN, SAN or SHA-256 fingerprint), hostname check, optional periodic renegotiation (`RenegotiateIntervalSeconds`, off by default), session resumption off by default. The TLS role follows the TCP direction. The TLS filter is a generic ODC library component (`odc::tls`) and the port's TCP connection with TLS is the library's `odc::TCPSessionManager`; the ICCP defaults (default port 3782, cipher lists, `Legacy12` preset) and what is made of the session's life (status, failure back-off) are the port's own. Default port 3782. A build without OpenSSL rejects a configuration that enables TLS. Verified with one real peer (a stack needing RSA keys of 2048 bits or more, and checking the client certificate), tests between this implementation and itself (`TlsFilterTest`, `TlsLoopbackTest`) and against the OpenSSL command line tools. **Not verified `[S]` against a real IEC 62351-3 peer:** the default cipher suite lists, the renegotiation interval, certificate naming, the default port, and the compatibility options. See [Secure ICCP / TLS](README.md#secure-iccp--tls) |
| MACE: certificate based authentication in the ACSE association (MMS Application Certificate Exchange) | Yes, when built with `ODC_ASIO_SSL` | The authentication fields of the AARQ and AARE carry the sender's X.509 certificate, a signing time and a signature (RSA, 2048 to 8192 bits by default). The wire format and the signature scheme are `[V]` (decoded from a real peer's association, both directions); our request and response were accepted by a real peer, with and without TLS. The signature is not over a hash, as the peer's (quirk 47). Time window `TimeWindowSeconds` (default 600), trust anchors, CRL and allow list as for TLS. The refusal diagnostics are `[S]`. The mechanism-name sent is `MechanismName` (default `1.0.62351.4.3.1`, the 62351-4 value `[T]`); the peer's value (a real peer uses the legacy `1.0.840.0.1.0.1.1`) is accepted whatever it is. See [Secure ICCP: MACE](README.md#secure-iccp-mace), quirks 46 to 49, 52 |
| IEC/TS 62351-4 HMAC based authentication in the ACSE association | No | Not implemented. MACE is not claimed to be IEC 62351-4 |
| A-security presentation context (`1.0.840.0.1.1.4.1`) | Partial | Accepted by a MACE responder when a request offers it; never proposed by this port; never seen on the wire `[S]` (quirk 52) |
| Other | - | Key passwords are only read from a named environment variable, never from the configuration; `TraceHex` and TLS `KeyLogFile` are diagnostics that can expose traffic and secrets and are off by default |

## 14. Limits and defaults

Values from the code (`ICCPPortConf.h`, `ICCPPortConf.cpp`, `IccpStack.h`) and the README configuration tables. Option names are the JSON keys.

### Sizes

| Item | Value |
|------|-------|
| TPDU size | 128 to 8192 octets, default 1024 (`TPDUSize`) |
| TPKT length | 7 to 65535 octets (RFC 1006) |
| Largest reassembled TSDU | 1 MiB (fixed); the TPKT reader buffers at most 1 MiB |
| Session user data (PI 193) | 512 octets; extended user data (PI 194) up to 10240 octets |
| Selectors | TSAP up to 32 octets, session and presentation selectors up to 16 octets; hexadecimal strings |
| MMS decoder | at most 100000 list entries, 200000 data nodes per PDU, nesting 32, identifiers of 255 octets; BER nesting 64 |
| Presentation | at most 32 contexts in a list, 8 transfer syntaxes per context, 16 PDVs per user data |
| Bilateral_Table_ID | 32 visible ASCII characters |
| Configured names | 1 to 255 characters (points, domains, data sets, transfer sets); `Name` + `_SBO` of a device also at most 255 |
| Data set members | `MaxDataSetMembers` default 5000 (1 to 100000) |

### Associations and MMS Initiate

| Item | Value |
|------|-------|
| Associations per port | one at a time (a listening port does not accept a second connection while the first is up) |
| MMS version | 1 |
| Local detail (maximum PDU size proposed) | 32000 |
| `MaxOutstandingCalling`, `MaxOutstandingCalled` | default 10 each (1 to 1000); the smaller of both ends applies; requests beyond the peer's limit wait in a queue of at most 10000 |
| `NestingLevel` | default 4 (1 to 32) |
| Parameter CBB, services supported | section 8 |

### Timers

| Option | Default | Range | Meaning |
|--------|---------|-------|---------|
| `RequestTimeoutms` | 5000 | 1 to 3600000 | A confirmed request |
| `AssociateTimeoutms` | 10000 | 1 to 3600000 | Association set-up |
| `ReleaseTimeoutms` | 5000 | 1 to 3600000 | Release |
| `TCPConnectRetryPeriodms` | 0 (exponential back-off) | 0 to 65535 | Client reconnection; the delay is at least 1 s |
| TLS `HandshakeTimeoutms` | 10000 | 100 to 600000 | TLS handshake |
| Internal tick | 50 ms | fixed | Request timeouts, transfer set intervals, buffer times |

### Server limits and reporting

| Option | Default | Range | Meaning |
|--------|---------|-------|---------|
| `MaxClientDataSets` | 16 | 0 to 100000 | Data sets the peer may create per association |
| `NamePageSize` | 200 | 1 to 100000 | Names per GetNameList response |
| `MaxBufferedChanges` | 1000 | 1 to 100000 | Changes buffered per transfer set with `AllChangesReported`; beyond it the last value of a point wins |
| `ReportAckTimeoutms` | 10000 | 0 to 3600000 | Critical reports: time allowed for `Transfer_Report_ACK`; 0 = never |
| `MaxUnacknowledgedReports` | 100 | 1 to 100000 | Critical reports waiting for an acknowledgement per transfer set; beyond it the oldest is a `Data_failure` |
| `TransferSetPool` | empty | - | Transfer set names offered to the peer; no fixed upper limit |
| `BlockedRule2` | true | - | Send rule 2 (2 octet positions) when a position above 255 is needed |
| `BlockedDataName` | `Block_Data` | 1 to 255 characters | Item id of the blocked octet string variable |

### Client and diagnostics

| Option | Default | Meaning |
|--------|---------|---------|
| `AcknowledgeCriticalReports` | true | Acknowledge the reports of a critical transfer set |
| `NackUnknownReports` | true | NACK a report not from a transfer set this port started |
| `MissingMemberPolicy` | `Prune` | Define the data set without the members the peer lacks, flag their points |
| `MissingMemberQuality` | `COMM_LOST` | ODC quality of the points of missing members |
| `ProbeOnDefineFailure`, `ProbeChunkSize` | true, 50 (1 to 500) | The member probe |
| `AbortOnBilateralTableMismatch` | false | Server-only ports: end the association on a mismatch |
| `RequireMatchingEdition` | true | The peer's `TASE2_Version` must equal `TASE2Edition` |
| `TraceHex`, `TraceHexMaxBytes` | false, 512 (16 to 65536) | Hex dump of the TCP stream (debug level) |

### TLS and MACE defaults (when built with `ODC_ASIO_SSL`)

| Item | Default |
|------|---------|
| TLS versions | 1.2 to 1.3 |
| TLS `VerifyPeer`, `RequireClientCert` | true, true |
| TLS TCP port | 3782 |
| TLS certificate expiry warning | 30 days |
| MACE `TimeWindowSeconds` | 600 (1 to 86400) |
| MACE `MinKeyBits`, `MaxKeyBits` | 2048, 8192 (RSA only) |
| MACE `RequireAuthentication`, `RequireResponderAuthentication` | true, true |

## 15. Protocol-level options

### Addresses and titles

| Option | Meaning | Default |
|--------|---------|---------|
| `LocalTSelector`, `RemoteTSelector` | Transport selectors (TSAP), hexadecimal, up to 32 octets | empty (not sent) |
| `LocalSSelector`, `RemoteSSelector` | Session selectors, up to 16 octets | empty (not sent) |
| `LocalPSelector`, `RemotePSelector` | Presentation selectors, up to 16 octets | empty (not sent); a responder echoes the called selector of the request when it has none configured |
| `LocalAPTitle`, `RemoteAPTitle` | ACSE AP titles, dotted object identifiers (form 2) | none (not sent) |
| `LocalAEQualifier`, `RemoteAEQualifier` | ACSE AE qualifiers, integers 0 to 2147483647 (form 2) | none (not sent) |

"Local" is this port and "Remote" the peer, whichever way the TCP connection goes. On the wire the calling and called addresses swap with the TCP direction. A responder does not enforce the calling or called selectors, titles or qualifiers of the peer; it records the calling AP title and AE qualifier as the identity of the client (used for the owner of a VCC device tag, quirk 64).

### Application context, presentation contexts and versions

| Item | Value |
|------|-------|
| Application context name | MMS `1.0.9506.2.3` (the only one accepted) |
| ACSE protocol version | 1 (always written) |
| Presentation context 1 | abstract syntax ACSE `2.2.1.0.1`, transfer syntax BER `2.1.1` |
| Presentation context 3 | abstract syntax MMS `1.0.9506.2.1`, transfer syntax BER `2.1.1` |
| Context identifiers | 1 and 3 are proposed (as peers do), but the identifiers used are always those of the negotiated result (quirk 3 `[V]`) |
| A-security abstract syntax | `1.0.840.0.1.1.4.1`, never proposed; accepted by a MACE responder when offered (quirk 52) |
| Presentation requirements | 0 (kernel only); protocol version 1 implied |
| Session | version 2, protocol options 0, requirements duplex, no token setting, no TSDU maximum size parameter |
| Transfer syntax | BER only; indefinite lengths and non-minimal integers are accepted on receipt (quirk 9) |
| MMS | version 1; services and parameter CBBs as in section 8 |
| TASE.2 version | 1996/8 or 2000/8 (`TASE2Edition`) |

## 16. Known deviations and unverified items

Everything marked `S` in the README quirks table is unverified. The README sections
[Known limitations](README.md#known-limitations) and [Unverified items](README.md#unverified-items) are the authoritative lists; the points that matter for conformance are:

### Deviations from the 702 tables

| Area | Deviation | Where |
|------|-----------|-------|
| A.4 I./5, A.5 I./7 | Set Data Value and Set Data Set Element Values are not supported (the profile this port was written for excludes them) | section 5 |
| A.5 I./2 | No VCC-scope (VMD-specific) data sets | section 5, A.5 |
| A.3 I./2, 702 clause 6.4 | No QOS / transport priority | section 5, A.3 |
| A.3 I./6, A.51 | IEC 62351-4 HMAC authentication not implemented; MACE instead | section 13 |
| A.40 | The server refuses alternateAccess in DefineNamedVariableList | A.40 |
| A.7 I./10, I./13 | `EventCodeRequested` is stored, not used; `TLE` is enforced only against the port's own timing | quirk 58 |
| A.12 | No Block 7 event notifications for Timeout, Success, Failure, Local Reset; no readable `SBO` or `ControlConfig` variables; no cancel of a select | quirks 43, 63 |
| A.17 | No InitiateError PDU is sent (the refusal is an AARE); ConcludeError only for a collision | A.17 |
| A.19 | The `real` parameter CBB bit is not set | A.19 |
| A.4.2 | Identifiers are not limited to 32 characters and the character set is not checked | A.4.2 |
| A.58, A.A.10 | Form 1 AP titles and AE qualifiers are carried, not interpreted; invocation ids and implementation information are never sent | A.52 to A.58 |
| 6.4 Presentation | No default context name, no default context negotiation, no simply encoded data | section 10 |
| Block 4, 6 to 9 | Not implemented | section 4 |

### Items seen on the wire with a real peer (V) and items not (S), in brief

| Area | Verified `[V]` | Not verified `[S]` (to be confirmed with a peer) |
|------|----------------|--------------------------------------------------|
| Association | Initiate fields, selectors and titles, release sequence of a real peer (quirks 1 to 7, 23, 45) | That a peer accepts our reason values; the Conclude collision rule (quirks 24, 45); the service roles `Peer` and the role / TCP direction combinations (quirks 15, 34, 35) |
| Block 1 | Point types `RealQ` and `StateQ`, GetNameList paging, association objects | Every other point type and the quality mapping details (quirks 20, 22); type descriptions |
| Block 2 | `DSTransferSet` of 12 and 13 members as written by real peers, `Next_DSTransfer_Set`, report layouts, critical report acknowledgement without a time stamp | Stop Transfer, report by exception as sent by our server, the first integrity report at once, `AllChangesReported` on the server (quirks 25, 26, 32, 53), the two variable acknowledgement and the NACK, the acknowledgement timeout, `TLE` (quirks 56 to 59) |
| Block 3 | Report structure, rule 0 and rule 1 as sent by a real peer, our rule 0, 1 and 2 accepted and applied by a real peer, 2 octet positions above 255 | A rule 2 report sent by a real peer, the byte order and sign of a non zero event code, every point type but `RealQ` and `StateQ` (quirks 66 to 70) |
| Block 5 | Read of `D_SBO` then Write of the bare value, direct operate, floating point setpoints, CheckBackID behaviour (quirks 39, 42, 51) | Tags and Local Reset (never seen), the data access error codes other than the standard names, the width 16 range handling, leniency of a device without select (quirks 40, 54, 60 to 65) |
| TLS | One real peer | Cipher lists, renegotiation, naming, default port against an IEC 62351-3 peer |
| MACE | A real peer's wire format and signature scheme, both directions, with and without TLS | The refusal diagnostics, the A-security abstract syntax, the scope of the signature |

### Other limitations

* One association at a time per listening port.
* The library has only been built with MSVC; gcc and clang builds are untested.
* Control events are handled only for configured control devices (`BinaryControls`, `AnalogControls`); other ODC control event types are answered `NOT_SUPPORTED`.
* Server data sets and transfer sets belong to the association; nothing persists across associations (quirk 33).
* Time tags are whole seconds on the types that have them; the local time is used for types without one.
* Server-side state (data sets, transfer sets, tags) is lost when the port restarts.

## 17. Test evidence

The unit tests are in `Code/tests/ICCPPort_tests` (Catch) and run in the `ICCPPort_tests` executable; see [Unit tests](README.md#unit-tests). Each codec layer has fixed byte vectors, truncation tests and random mutation tests that check corrupted input only produces a decoding error.

| Layer or feature | Test files |
|------------------|------------|
| BER | `BERTest` |
| TPKT, COTP | `TPKTTest`, `COTPTest`, `InteropVectorsTest` |
| Session, presentation, ACSE | `SessionTest`, `PresentationTest`, `AcseTest`, `InteropVectorsTest` |
| MMS | `MmsTest` |
| TASE.2 Blocks 1 and 2 | `Tase2Test`, `TransferSetAllChangesTest`, `DiagnosticsTest` |
| Critical reports and TLE | `ReportAckTest` |
| Block 3 | `BlockedTransferTest` |
| Block 5, tags, Local Reset | `Block5Test`, `TagTest` |
| Association state machine, release, abort, limits | `IccpStackTest`, `StaleTransportTest` |
| Configuration | `ICCPPortConfTest`, `TlsConfTest` (the ICCP defaults and presets of the TLS section; the generic parser is tested in `ODC_tests`) |
| Client against server over TCP (all service roles, reconnect, edition mismatch, transfer sets, reports, devices) | `ICCPLoopbackTest` |
| TLS | `TlsLoopbackTest` (this port over TLS); the TLS filter itself is a generic ODC library component (`odc::tls`) with its tests in `ODC_tests`: `TlsFilterTest`, `TlsConfTest`, `TCPSessionManagerTest` |
| MACE | `MaceTest`, `MaceCryptoTest`, `MaceStackTest`, `MaceLoopbackTest` |

The TLS and MACE tests are built only with `ODC_ASIO_SSL` (the TLS configuration tests and the trace parser tests run in every build).
Interoperability beyond these tests is limited to one real third-party peer (section 2) and is not repeated by the unit tests.

## 18. Items to be confirmed

These could not be determined from the code, the tests or the README, and are left open:

* The exact opendatacon release (or commit) of a delivered binary (section 2).
* Behaviour over IPv6 (section 12).
* Whether peers require the `real` parameter CBB bit, which this port does not set (A.19, I./9).
* Whether a peer that proposes only session version 1 is refused (the session version of the peer is not checked).
* Behaviour of other platforms and compilers than MSVC.
* Conformance of the MACE and TLS profiles to a particular edition of IEC 62351-3 and -4 (section 13): not claimed.
