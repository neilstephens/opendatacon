## Table of contents

* [Introduction](#introduction)
* [Counters and Frozen Counters](#counters-and-frozen-counters)
* [Freezing](#freezing)
* [Mapping Counters to Analogs](#mapping-counters-to-analogs)
* [Standard compliance and interop notes](#standard-compliance-and-interop-notes)

## Introduction

DNP3Port provides DNP3 Master and Outstation ports for OpenDataCon, built on opendnp3.
This document covers the Counter and Frozen Counter configuration. Other point types (`Analogs`, `Binaries`, etc.) follow the same per-point schema.

## Counters and Frozen Counters

`"Counters"` (Group 20 static / Group 22 events) and `"FrozenCounters"` (Group 21 static / Group 23 events) are configured like `"Analogs"`:

```jsonc
"Counters":
[
  { "Range": { "Start": 0, "Stop": 9 }, "Class": 2 },
  { "Index": 20, "Deadband": 5, "StaticCounterResponse": "Group20Var2", "EventCounterResponse": "Group22Var5" }
],
"FrozenCounters":
[
  { "Range": { "Start": 0, "Stop": 9 } },
  { "Index": 20, "StaticFrozenCounterResponse": "Group21Var9", "EventFrozenCounterResponse": "Group23Var2" }
]
```

| Key | Meaning |
|-----|---------|
| `Index` or `Range: {Start, Stop}` | The point(s) the entry applies to |
| `Class` | Event class (0-3) |
| `Deadband` | Unsigned integer; the change in count required to generate an event |
| `StaticCounterResponse` | `Group20Var1`, `Group20Var2`, `Group20Var5`, `Group20Var6` |
| `EventCounterResponse` | `Group22Var1`, `Group22Var2`, `Group22Var5`, `Group22Var6` |
| `StaticFrozenCounterResponse` | `Group21Var1`, `Group21Var2`, `Group21Var5`, `Group21Var6`, `Group21Var9`, `Group21Var10` |
| `EventFrozenCounterResponse` | `Group23Var1`, `Group23Var2`, `Group23Var5`, `Group23Var6` |
| `StartVal` | `"X"` starts the point as comm-lost with value 0; `"D"` deletes the point; otherwise an unsigned integer |
| `Type` | Optional - `"Analog"` maps the point to an Analog (see below) |

Port-wide defaults (all optional): `StaticCounterResponse` (`Group20Var1`), `EventCounterResponse` (`Group22Var1`), `StaticFrozenCounterResponse` (`Group21Var1`), `EventFrozenCounterResponse` (`Group23Var1`),
`MaxCounterEvents` and `MaxFrozenCounterEvents` (1000 each, the outstation's event buffer sizes).

The port status output includes `Counters` and `FrozenCounters` arrays.

`StartVal` is parsed and stored, but (as with `Analogs` and `Binaries`) it isn't used to initialise the point database.

## Freezing

An outstation with `FrozenCounters` configured at the same indexes as its `Counters` answers `IMMED_FREEZE` and `FREEZE_CLEAR` requests from **any** master. The freeze mechanics are implemented inside opendnp3's outstation database.

opendnp3 offers no API to set a frozen counter value directly, so:
- a `FrozenCounter` event sent to a DNP3 outstation port is stored in its local point database only; the value seen on the wire is whatever the live counter was when it was last frozen.
- quality-only `FrozenCounterQuality` events likewise only affect the local point database.

A DNP3 Master port can trigger freezes itself:

```jsonc
"FreezeScanRatems": 60000,
"FreezeMode": "IMMED_FREEZE"
```

`FreezeScanRatems` (default 0, disabled) is the period in ms between freeze requests, sent while the link is up. `FreezeMode` is `IMMED_FREEZE` (default) or `FREEZE_CLEAR`.
The request is sent to all Group 20 objects (variation 0) with the acknowledged function code. Use an integrity scan (or Class 0 read) to read the frozen values back.
The standard recommends the no-ack variants (`IMMED_FREEZE_NR`/`FREEZE_CLEAR_NR`) for broadcast freezing; that isn't exposed, as this feature is point-to-point only.

## Mapping Counters to Analogs

There are two ways to represent counters as Analogs.

**Per point (DNP3 local):** add `"Type": "Analog"` to a Counters/FrozenCounters entry. On the wire the point stays a Group 20/21 object, but everywhere else in OpenDataCon it is an `Analog` event at that index.

```jsonc
"Counters": [ { "Index": 20, "Type": "Analog" } ]
```

Limitation: a quality-only `CounterQuality`/`FrozenCounterQuality` event sent to a mapped point has no effect. Send `AnalogQuality` instead.
An `Analog` event sent to an outstation point mapped from a FrozenCounter is stored locally only (see Freezing).

**Per connection (any port):** use the `TypeConversion` transform in a connector. `FromType`/`ToType` can be any of `Counter`, `FrozenCounter` and `Analog`; `Indexes` is optional (default: every index).
Analog to Counter conversions round to the nearest integer and clamp to the range of an unsigned 32 bit integer.

```jsonc
{ "Name": "TxCounterToAnalog", "Type": "TypeConversion", "Sender": "SomePort",
  "Parameters": { "FromType": "Counter", "ToType": "Analog", "Indexes": [1,2,3] } }
```

Neither of these is the same as the DNP3 standard's native frozen analog input (Group 31/33), which opendnp3 does not implement.

## Standard compliance and interop notes

* Configuring the same index in both `Counters` and `FrozenCounters` is valid, but the outstation then reports both Group 20 and Group 21 objects for it in a Class 0 response. A warning is logged at startup, as this is usually unintentional.
* The `ROLLOVER` quality flag is obsolete in IEEE 1815-2012. This port doesn't set it; it exists only for pass-through from sources that set it deliberately. `DISCONTINUITY` is current and is passed through.
* "Without flag" static variations (`Group20Var5`/`Var6`, `Group21Var9`/`Var10`) transmit no quality bits. A receiving master must assume `ONLINE` with no exception flags, so quality configured on the OpenDataCon side isn't visible on the wire.
* Counters are unsigned and monotonic. Model bidirectional accumulation as two Counter points, one per polarity.
* Presetting a counter via a `WRITE` request and `FREEZE_AT_TIME` scheduled freezes are not supported. opendnp3's master API has no generic write to Group 20, and `FREEZE_AT_TIME` needs a Group 50 Var 2 object composed in the same request.
* Delta counter variations (`Var3`/`Var4`/`Var7`/`Var8`) aren't offered because opendnp3's enums don't include them.
