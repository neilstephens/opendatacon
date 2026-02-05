--opendatacon mandates an Enable() function
function Enable()
	odc.log.info("Enable()");
  local PortUpEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_UP };
  odc.PublishEvent(PortUpEvent);
  if not odc.InDemand() then
    PingOn();
  end
end

--opendatacon mandates a Disable() function
function Disable()
	odc.log.debug("Disable()");
  local PortDownEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_DOWN };
  odc.PublishEvent(PortDownEvent);
  PingOff();
end

--opendatacon mandates a Build() function
function Build()
	odc.log.debug("Build()");
  Connected = false;
  PingRunning = false;
end

--opendatacon mandates an Event() function so you can receive events
function Event(EventInfo, SenderName, StatusCallback)
  odc.log.trace("Event()");  
  if EventInfo.EventType == odc.EventType.ConnectState then
    if odc.InDemand() then
      PingOff();
    else
      PingOn();
    end
    StatusCallback(odc.CommandStatus.SUCCESS);
    return;
  end
  StatusCallback(odc.CommandStatus.NOT_SUPPORTED);
end

--------------------- Functions below are just for the purposes of port (not mandated by opendatacon) ---------------------

function PingOn()
  odc.log.debug("PingOn()");
  if PingRunning then
    return
  end
  PingRunning = true;
  DoPing();
end 

function DoPing()
  odc.log.debug("DoPing()");
  local ping_pid, _, ping_out, ping_err = odc.SpawnAttached(table.unpack(odc.Config.PingCommand));
  if(not ping_pid) then
    odc.log.error("Failed to execute ping command");
  else
    PingWaitCancel = odc.msTimerCallback(odc.Config.PingDurationSecs*1000,
      function(err_code)
        if PingRunning then
          CheckPingResult(ping_pid, ping_out, ping_err);
        else
          odc.log.info("Abort Ping.");
          odc.KillPid(ping_pid,odc.Kill.SIGTERM);
          odc.WaitPid(ping_pid);
        end
      end);
  end
  TimerCancel = odc.msTimerCallback(odc.Config.PingPeriodSecs*1000,function(err_code)if PingRunning then DoPing() end end);
end

function CheckPingResult(ping_pid, ping_out, ping_err)
  local exited, status = odc.WaitPid(ping_pid,true); --nohang true : non-blocking
  if not exited then
    odc.KillPid(ping_pid,odc.Kill.SIGINT); --could use TERM, or KILL if system supports it
    _, status = odc.WaitPid(ping_pid); --blocks til exit
  end
  
  local err_str = ping_err:read("*a");
  local out_str = ping_out:read("*a");
  
  if status == 0 then
    odc.log.info("CheckPingResult(): Ping successful: "..out_str);
    if not Connected then
      Connected = true;
      local ConnectedEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.CONNECTED };
      odc.PublishEvent(ConnectedEvent);
    end
  else
    odc.log.error("CheckPingResult(): Ping command failed: status "..status..". STDOUT: "..out_str..". STDERR: "..err_str);
    if Connected then
      Connected = false;
      local DisconnectedEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.DISCONNECTED };
      odc.PublishEvent(DisconnectedEvent);
    end
  end
end

function PingOff()
  odc.log.debug("PingOff()");
  PingRunning = false;
  if TimerCancel then
    TimerCancel();
    TimerCancel = nil;
  end
  if PingWaitCancel then
    PingWaitCancel();
    PingWaitCancel = nil;
  end
end

