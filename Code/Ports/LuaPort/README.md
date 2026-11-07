# LuaPort

LuaPort loads a Lua script as a port. The script must define the global functions `Build()`, `Enable()`, `Disable()` and `Event(event, sender, status_callback)`; see `ExampleConfig/SelfDocumentingLuaPort/LuaPortDoco.lua` for the events, timers, logging and JSON functions of the `odc` table that every Lua script of ODC (port, transform, log sink and UI command script) gets.

A port script (like every other Lua script of opendatacon) can also use the **network objects**: TCP and TLS sockets, an HTTP(S) server and client, and HTTP helpers. They are documented in [Network objects for Lua scripts](../../Libs/LuaWrappers/README.md), with the `NetEnabled` setting that decides which scripts get them (a port's goes in its configuration, next to `LuaFile`; by default a port script has them).

Examples that move ODC events and controls between two Lua ports over TCP, TLS, HTTP and HTTPS are in [`ExampleConfig/Lua-Transport`](../../../ExampleConfig/Lua-Transport), described in [Moving ODC events over a network](../../Libs/LuaWrappers/README.md#moving-odc-events-over-a-network).
