--[[
  DataGenerator.lua  –  GoModbus-LuaPort-E2E example
  
  Periodically publishes ODC events that flow into GoModbusServer's data store
  via the Gen2Server connector.  GoModbusClient then polls those values and
  publishes them to DataSink.
  
  ODC host-API functions exercised (via LuaPort's odc.* wrappers):
    ms_repeating_callback   – via odc.msCoroutineLoop
    ms_since_epoch          – for OctetString timestamp payload
    get_working_dir         – OctetString payload = working directory path
    get_executable_dir      – logged during Build
    should_log              – guards expensive log-message construction
    event_type_to_string    – logged in Build
    connect_state_to_string – logged in Enable/Disable
    quality_flags_to_string – logged when publishing events
    ms_since_epoch_to_datetime – logged with custom format
--]]

local cancelLoop = nil
local tick        = 0
local running     = false   -- set true in Enable(), false first thing in Disable()

-- ── Mandatory lifecycle functions ────────────────────────────────────────────

function Build()
  odc.log.info("DataGenerator Build()")

  -- get_executable_dir vtable function
  local exedir = odc.GetPath.ExecutableDir()
  odc.log.info("ExecutableDir: " .. exedir)

  -- should_log vtable function: skip expensive string builds when not needed
  if odc.log.level.ShouldLog(odc.log.level.debug) then
    local evtNames = {}
    for name, _ in pairs(odc.EventType) do
      evtNames[#evtNames+1] = name
    end
    table.sort(evtNames)
    -- event_type_to_string vtable function (via ToString.EventType)
    for _, name in ipairs(evtNames) do
      odc.log.debug("  EventType." .. name .. " = " ..
        odc.ToString.EventType(odc.EventType[name]))
    end
  end
end

function Enable()
  -- connect_state_to_string vtable function (via ToString.ConnectState)
  odc.log.info("DataGenerator Enable() – state: " ..
    odc.ToString.ConnectState(odc.ConnectState.CONNECTED))

  -- Publish PORT_UP / CONNECTED so subscribers know we're live
  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.PORT_UP })
  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.CONNECTED })

  running = true
  tick    = 0
  cancelLoop = odc.msCoroutineLoop(coroutine.wrap(generatorLoop))
end

function Disable()
  running = false

  odc.log.info("DataGenerator Disable()")

  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.DISCONNECTED })
  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.PORT_DOWN })

  if cancelLoop then
    cancelLoop()
    cancelLoop = nil
  end
end

-- Mandatory event handler – generator does not subscribe to anything meaningful,
-- but the function is required.  Log and acknowledge every event.
function Event(EventInfo, SenderName, StatusCallback)
  odc.log.debug("DataGenerator received event from " .. SenderName ..
    " type=" .. odc.ToString.EventType(EventInfo.EventType) ..
    " idx=" .. EventInfo.Index)
  StatusCallback(odc.CommandStatus.SUCCESS)
end

-- ── Generator loop ────────────────────────────────────────────────────────────

function generatorLoop()
  local period  = odc.Config.PeriodMs or 2000
  local rampMax = odc.Config.RampMax  or 32767

  while running do
    tick = tick + 1
    local qOnline = odc.QualityFlags(odc.QualityFlag.ONLINE)

    -- ms_since_epoch vtable function
    local now = odc.msSinceEpoch()
    -- ms_since_epoch_to_datetime vtable function with custom format
    local ts = odc.msSinceEpochToDateTime(now, "%Y-%m-%d %H:%M:%S.%e")

    -- ── Binary index 0: toggle every other tick ─────────────────────────────
    local bval = (tick % 2 == 0)
    publish("Binary_0", {
      EventType    = odc.EventType.Binary,
      Index        = 0,
      QualityFlags = qOnline,
      Payload      = bval
    })

    -- ── Binary index 1: opposite of index 0 ─────────────────────────────────
    publish("Binary_1", {
      EventType    = odc.EventType.Binary,
      Index        = 1,
      QualityFlags = qOnline,
      Payload      = not bval
    })

    -- ── BinaryOutputStatus index 10: mirrors Binary index 0 ─────────────────
    publish("BinaryOS_10", {
      EventType    = odc.EventType.BinaryOutputStatus,
      Index        = 10,
      QualityFlags = qOnline,
      Payload      = bval
    })

    -- ── Analog index 0: Int16 ramp counter ───────────────────────────────────
    local ramp = (tick * 100) % (rampMax + 1)
    publish("Analog_0_Int16", {
      EventType    = odc.EventType.Analog,
      Index        = 0,
      QualityFlags = qOnline,
      Payload      = ramp
    })

    -- ── Analog index 1: Float32 ABCD – simple floating counter ───────────────
    local fval = tick * 0.25
    publish("Analog_1_F32_ABCD", {
      EventType    = odc.EventType.Analog,
      Index        = 1,
      QualityFlags = qOnline,
      Payload      = fval
    })

    -- ── Analog index 2: Float32 DCBA – exercises fully little-endian decode ──
    -- The server's HR 4-5 uses Endian=DCBA; encodeAnalogRegs stores it in DCBA
    -- register order; the client decodes with DCBA and recovers the float value.
    publish("Analog_2_F32_DCBA", {
      EventType    = odc.EventType.Analog,
      Index        = 2,
      QualityFlags = qOnline,
      Payload      = fval * 2.0   -- different value to distinguish in logs
    })

    -- ── AnalogOutputStatus index 10: Uint16 free-running counter ─────────────
    local uval = tick % 65536
    publish("AnalogOS_10_U16", {
      EventType    = odc.EventType.AnalogOutputStatus,
      Index        = 10,
      QualityFlags = qOnline,
      Payload      = uval
    })

    -- ── OctetString index 0: working dir path as raw bytes ───────────────────
    -- get_working_dir vtable function
    local wd = odc.GetPath.WorkingDir()
    -- Embed the tick number so the sink can verify freshness
    local payload = string.format("[tick=%04d] %s", tick, wd)
    -- Trim or pad to exactly 16 bytes (8 Modbus registers × 2 bytes)
    if #payload > 16 then
      payload = payload:sub(1, 16)
    else
      payload = payload .. string.rep("\0", 16 - #payload)
    end
    publish("OctetString_0", {
      EventType    = odc.EventType.OctetString,
      Index        = 0,
      QualityFlags = qOnline,
      Payload      = payload
    })

    odc.log.debug(string.format(
      "DataGenerator tick=%d at %s  binary=%s  ramp=%d  float=%.3f  uint=%d",
      tick, ts, tostring(bval), ramp, fval, uval))

    coroutine.yield(period)
  end
end

-- ── Publish helper with quality-flags logging ─────────────────────────────────

function publish(tag, evt)
  -- quality_flags_to_string vtable function
  if odc.log.level.ShouldLog(odc.log.level.trace) then
    odc.log.trace("DataGenerator publish " .. tag ..
      " Q=" .. odc.ToString.QualityFlags(evt.QualityFlags or 0))
  end
  odc.PublishEvent(evt, function(status)
    if status ~= odc.CommandStatus.SUCCESS then
      -- command_status_to_string vtable function
      odc.log.warning("DataGenerator " .. tag .. " → " ..
        odc.ToString.CommandStatus(status))
    end
  end)
end
