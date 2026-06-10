--[[
  DataSink.lua  –  GoModbus-LuaPort-E2E example

  Receives events from two sources:
    Client2Sink connector: polled values from GoModbusClient (Analog indices
      shifted +1000 by IndexShiftTransform, ConnectState dropped).
    Server2Sink connector: events published by GoModbusServer when the Modbus
      client writes a mapped coil or holding register.

  After ControlDelayMs the sink fires a one-shot timer and sends control events
  (CROB, AO16, AOF32, AOD64) to GoModbusClient via the Sink2Client connector.
  The client writes them to the server; the server publishes them back.

  ODC host-API functions exercised (via LuaPort's odc.* wrappers):
    in_demand               – checked before expensive per-event processing
    should_log              – guards trace-level message formatting
    ms_since_epoch          – current wall-clock time
    ms_since_epoch_to_datetime – formatted timestamp for logging
    datetime_to_ms_since_epoch – parse a known reference date string
    string2hex              – hex-dump OctetString payloads
    hex2string              – round-trip verification of string2hex
    get_working_dir         – logged in Build
    get_executable_dir      – logged in Build
    spawn_attached          – run 'date' and read its stdout
    spawn_detached          – run 'sleep 1' to demonstrate detached process API
    wait_pid                – block until the spawned date command finishes
    kill_pid (signal 0)     – check whether the sleep process is still running
    kill_pid (SIGTERM)      – terminate the sleep process
    event_type_to_string    – every event logged with its type string
    command_status_to_string – control callback results
    control_code_to_string  – CROB control code logged
    connect_state_to_string – connect-state transitions
    quality_flags_to_string – per-event quality logged at trace level
--]]

local controlTimerCancel  = nil
local statsLoopCancel     = nil
local eventCount          = { total = 0, byType = {} }
local controlsSent        = false
local running             = false   -- set true in Enable(), false first in Disable()

-- ── Mandatory lifecycle functions ─────────────────────────────────────────────

function Build()
  odc.log.info("DataSink Build()")

  -- get_working_dir / get_executable_dir vtable functions
  odc.log.info("WorkingDir:    " .. odc.GetPath.WorkingDir())
  odc.log.info("ExecutableDir: " .. odc.GetPath.ExecutableDir())

  -- datetime_to_ms_since_epoch vtable function
  -- Parse a known reference time to demonstrate the API.
  local ref_ms = odc.msSinceEpoch("2000-01-01 00:00:00", "%Y-%m-%d %H:%M:%S")
  odc.log.info("ms-since-epoch for Y2K: " .. ref_ms)

  -- spawn_attached vtable function: run 'date' and capture stdout
  local pid, _, stdout, _ = odc.SpawnAttached("date")
  if pid then
    -- wait_pid vtable function
    local _, exit_code = odc.WaitPid(pid, false)
    local date_str = stdout:read("*a") or ""
    odc.log.info(string.format("date: %s (exit %d)", date_str:gsub("\n",""), exit_code))
  else
    odc.log.warning("DataSink: SpawnAttached('date') failed")
  end

  -- spawn_detached / kill_pid vtable functions
  local dpid = odc.SpawnDetached("sleep", "5")
  if dpid then
    odc.log.info("SpawnDetached('sleep 5') PID=" .. dpid)
    -- kill_pid with signal 0 = check if running (vtable function)
    local proc_running = odc.KillPid(dpid, 0)
    if proc_running then
      -- kill_pid with SIGTERM (vtable function)
      odc.KillPid(dpid, odc.Kill.SIGTERM)
      odc.log.info("Sent SIGTERM to sleep (PID=" .. dpid .. ")")
    end
  else
    odc.log.warning("DataSink: SpawnDetached('sleep') failed")
  end
end

function Enable()
  odc.log.info("DataSink Enable()")

  running = true

  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.PORT_UP })
  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.CONNECTED })

  -- Periodic stats logger every 10 s (exercises ms_repeating_callback via msCoroutineLoop)
  statsLoopCancel = odc.msCoroutineLoop(coroutine.wrap(function()
    while running do
      -- in_demand vtable function
      local demand = odc.InDemand()
      odc.log.info(string.format(
        "DataSink stats: total=%d  InDemand=%s",
        eventCount.total, tostring(demand)))
      coroutine.yield(10000)
    end
  end))

  -- One-shot timer to send controls after the client has had time to connect
  local delay = odc.Config.ControlDelayMs or 3000
  controlTimerCancel = odc.msTimerCallback(delay, function(err)
    if err == odc.CommandStatus.SUCCESS then
      sendControls()
    end
  end)
end

function Disable()
  running = false

  odc.log.info("DataSink Disable()")

  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.DISCONNECTED })
  odc.PublishEvent({ EventType = odc.EventType.ConnectState,
                     Payload   = odc.ConnectState.PORT_DOWN })

  if statsLoopCancel then
    statsLoopCancel()
    statsLoopCancel = nil
  end
  if controlTimerCancel then
    controlTimerCancel()
    controlTimerCancel = nil
  end
  controlsSent = false
  eventCount   = { total = 0, byType = {} }
end

-- ── Incoming event handler ─────────────────────────────────────────────────────

function Event(EventInfo, SenderName, StatusCallback)
  eventCount.total = eventCount.total + 1
  local etName = odc.ToString.EventType(EventInfo.EventType)  -- event_type_to_string
  eventCount.byType[etName] = (eventCount.byType[etName] or 0) + 1

  -- ms_since_epoch_to_datetime vtable function with custom format
  local ts = odc.msSinceEpochToDateTime(EventInfo.Timestamp or odc.msSinceEpoch(),
                                        "%H:%M:%S.%e")

  -- should_log vtable function: only build the full message when info is active
  if odc.log.level.ShouldLog(odc.log.level.info) then
    local payloadStr = formatPayload(EventInfo)
    odc.log.info(string.format(
      "[%s] %-26s idx=%-4d  from=%-16s  payload=%s",
      ts, etName, EventInfo.Index, SenderName, payloadStr))
  end

  -- quality_flags_to_string vtable function: only at trace level
  if odc.log.level.ShouldLog(odc.log.level.trace) then
    odc.log.trace("  quality: " .. odc.ToString.QualityFlags(EventInfo.QualityFlags or 0))
  end

  -- Handle specific event types for round-trip verification
  handleEventByType(EventInfo, SenderName)

  StatusCallback(odc.CommandStatus.SUCCESS)
end

-- ── Per-type event handling ────────────────────────────────────────────────────

function handleEventByType(ev, sender)
  local et = ev.EventType

  -- OctetString: hex-dump the bytes (exercises string2hex / hex2string)
  if et == odc.EventType.OctetString then
    local payload = ev.Payload or ""
    -- string2hex vtable function
    local hex = odc.String2Hex(payload)
    odc.log.info("  OctetString hex:   " .. hex)
    -- hex2string vtable function (round-trip check)
    local roundtrip = odc.Hex2String(hex)
    if roundtrip ~= payload then
      odc.log.warning("  OctetString round-trip mismatch!")
    else
      odc.log.debug("  OctetString round-trip OK")
    end

  -- ConnectState from server: log with connect_state_to_string
  elseif et == odc.EventType.ConnectState then
    -- connect_state_to_string vtable function
    odc.log.info("  ConnectState from " .. sender .. ": " ..
      odc.ToString.ConnectState(ev.Payload or 0))

  -- ControlRelayOutputBlock from server (echoed back after client wrote coil 20)
  elseif et == odc.EventType.ControlRelayOutputBlock and sender == "ModbusServer" then
    -- control_code_to_string vtable function
    local cc = (ev.Payload and ev.Payload.ControlCode) or 0
    odc.log.info("  Server CROB echo: ControlCode=" .. odc.ToString.ControlCode(cc))

  -- Analog output events from server (echoed back after client wrote HRs)
  elseif (et == odc.EventType.AnalogOutputInt16 or
          et == odc.EventType.AnalogOutputFloat32 or
          et == odc.EventType.AnalogOutputDouble64) and sender == "ModbusServer" then
    local val = (ev.Payload and ev.Payload.Value) or 0
    odc.log.info(string.format("  Server AO echo: %s idx=%d val=%s",
      etName(ev), ev.Index, tostring(val)))
  end
end

-- ── Send control events to GoModbusClient ─────────────────────────────────────
-- Called once from the one-shot Enable timer after ControlDelayMs.
-- Exercises the full set of ODC control event types.

function sendControls()
  -- Guard: Disable() may have been called before the timer fired.
  if not running then return end
  if controlsSent then return end
  controlsSent = true
  odc.log.info("DataSink: sending control events to ModbusClient")

  -- BinaryControl index 20: CROB LATCH_ON → writes Coil 20 on server
  local crob = odc.MakePayload.ControlRelayOutputBlock()
  crob.ControlCode = odc.ControlCode.LATCH_ON
  crob.Count = 1
  odc.PublishEvent(
    { EventType = odc.EventType.ControlRelayOutputBlock, Index = 20, Payload = crob },
    function(s)
      -- command_status_to_string vtable function
      odc.log.info("CROB(20) cb: " .. odc.ToString.CommandStatus(s))
    end)

  -- AnalogControl index 20: AnalogOutputInt16 → writes HR 20 (1 register)
  odc.PublishEvent(
    { EventType = odc.EventType.AnalogOutputInt16,
      Index     = 20,
      Payload   = { Value = 1234, CommandStatus = odc.CommandStatus.SUCCESS } },
    function(s)
      odc.log.info("AO16(20) cb: " .. odc.ToString.CommandStatus(s))
    end)

  -- AnalogControl index 21: AnalogOutputFloat32 → writes HR 22-23 (2 registers, ABCD)
  odc.PublishEvent(
    { EventType = odc.EventType.AnalogOutputFloat32,
      Index     = 21,
      Payload   = { Value = 3.14159, CommandStatus = odc.CommandStatus.SUCCESS } },
    function(s)
      odc.log.info("AOF32(21) cb: " .. odc.ToString.CommandStatus(s))
    end)

  -- AnalogControl index 22: AnalogOutputDouble64 → writes HR 26-29 (4 registers)
  odc.PublishEvent(
    { EventType = odc.EventType.AnalogOutputDouble64,
      Index     = 22,
      Payload   = { Value = 2.718281828, CommandStatus = odc.CommandStatus.SUCCESS } },
    function(s)
      odc.log.info("AOD64(22) cb: " .. odc.ToString.CommandStatus(s))
    end)
end

-- ── Helpers ────────────────────────────────────────────────────────────────────

function etName(ev)
  return odc.ToString.EventType(ev.EventType)
end

function formatPayload(ev)
  local et = ev.EventType
  if et == odc.EventType.OctetString then
    local hex = odc.String2Hex(ev.Payload or "")
    return "0x" .. hex:sub(1, 24) .. (hex:len() > 24 and "..." or "")
  end
  local ok, encoded = pcall(odc.EncodeJSON, ev.Payload)
  return ok and encoded or tostring(ev.Payload)
end
