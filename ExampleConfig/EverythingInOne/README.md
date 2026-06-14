# EverythingInOne Example

This example is a comprehensive topology that includes all currently supported built-in
port/plugin/transform types in a single directory.

## Included component types

- Plugins: `ConsoleUI`, `WebUI`, `LuaUICommander`
- Ports: `Null`, `Sim`, `JSONClient`, `JSONServer`, `DNP3Master`, `DNP3Outstation`,
  `ModbusMaster`, `ModbusOutstation`, `MD3Master`, `MD3Outstation`, `CBMaster`,
  `CBOutstation`, `FileTransfer`, `Lua`, `Py`, `KafkaProducer`, `KafkaConsumer`,
  `GoModbusClient`, `GoModbusServer`
- Built-in transforms: `IndexOffset`, `IndexMap`, `Threshold`, `Rand`, `RateLimit`,
  `LogicInv`, `BlackHole`, `AnalogScaling`
- Dynamic transform: `Lua`

## Structure

- `opendatacon.conf` contains all high-level objects.
- Each port has its own config file (`*Port.conf` or `*Port.conf.json`).
- Shared defaults are factored with `Inherits`:
  - `CommonPoints.conf`
  - `DNP3Common.conf`
  - `MD3Common.conf`
  - `CBCommon.conf`
  - `FileTransferCommon.conf`
  - `GoModbusCommon.conf.json`
- All connectors are configured with `ConfOverrides`.

## Notes

- This example is intended as a broad syntax/reference showcase and uses loopback addresses.
- Some protocol pairs (especially CB/MD3) may require protocol-specific tuning for practical
  field communication, but configs are valid for loader/parsing and topology demonstration.
- Create `TX/` and `RX/` directories under this folder when testing file transfer behavior.

## Run

From repository root:

```sh
./build/build/opendatacon -p ExampleConfig/EverythingInOne/
```
