--[[
  IndexShiftTransform.lua  –  GoModbus-LuaPort-E2E example

  Applied to the Client2Sink connector (Sender: "ModbusClient").
  Two transformations:
    1. Analog and AnalogOutputStatus indices are shifted by +1000, so
       polled analog values at the sink are easily distinguishable from
       control-echo events (which arrive from ModbusServer at the original
       indices 20-22).
    2. ConnectState events are dropped so the sink's event log is not
       cluttered with the client's reconnect cycles.

  A Lua transform only needs to define Event(EventInfo, AllowFn).
  AllowFn(nil) drops the event; AllowFn(EventInfo) passes it through.
--]]

function Event(EventInfo, AllowFn)

  -- Drop ConnectState: the sink monitors its own connection state, not the client's.
  if EventInfo.EventType == odc.EventType.ConnectState then
    AllowFn(nil)
    return
  end

  -- Shift Analog and AnalogOutputStatus indices by +1000.
  -- This distinguishes client-polled data (idx ≥ 1000) from control-echo
  -- events coming from the server (idx 20–22).
  if EventInfo.EventType == odc.EventType.Analog or
     EventInfo.EventType == odc.EventType.AnalogOutputStatus then
    EventInfo.Index = EventInfo.Index + 1000
  end

  AllowFn(EventInfo)
end
