--opendatacon mandates an Enable() function
function Enable()
	odc.log.info("Enable()");
  open_for_business_example();
	return;
end

--opendatacon mandates a Disable() function
function Disable()
	odc.log.info("Disable()");
  closed_for_business_example();
	return;
end

--opendatacon mandates a Build() function
--when called, it's a good time to process config and initialise things etc.
function Build()
  -- The port config from the opendatacon conf file is available as a Lua table
	odc.log.info("Build()");
  process_config_example();
	return;
end

--opendatacon mandates an Event() function so you can receive events
function Event(EventInfo, SenderName, StatusCallback)
	odc.log.info("Event(): EventType ".. odc.ToString.EventType(EventInfo.EventType)..
            ", Index "..EventInfo.Index..
            ", Timestamp "..odc.msSinceEpochToDateTime(EventInfo.Timestamp)..
            ", QualityFlags "..odc.ToString.QualityFlags(EventInfo.QualityFlags)..
            ", Payload "..odc.EncodeJSON(EventInfo.Payload)..
            ", SenderName "..SenderName);
         
  if EventInfo.EventType == odc.EventType.ConnectState and EventInfo.Payload == odc.ConnectState.PORT_UP then
    -- there's another port out there
    do_example_stuff();
  end
  
  StatusCallback(odc.CommandStatus.SUCCESS);
  return;
end

--------------------- Functions below are just for the purposes of this demo LuaPort
--------------------- Only the four functions above are needed by the opendatacon port interface

function do_example_stuff()
  -- There's a bunch of enum values you can access, and logging functions
  odc.log.info("EventType.Analog : " .. odc.EventType.Analog);
  odc.log.info("EventType.Binary : " .. odc.EventType.Binary);
  odc.log.info("CommandStatus.SUCCESS : " .. odc.CommandStatus.SUCCESS);
  odc.log.info("QualityFlag.COMM_LOST : " .. odc.QualityFlag.COMM_LOST);
  odc.log.info("QualityFlag.RESTART : " .. odc.QualityFlag.RESTART);
  
  --Quality flags can be combined (it's just bitwise OR convenience function)
  local Q = odc.QualityFlags(odc.QualityFlag.COMM_LOST,odc.QualityFlag.RESTART);
  odc.log.info("QualityFlags() returns " .. Q .. ": " .. odc.ToString.QualityFlags(Q));
  
  -- You can get ms since epoch, and convert to something readable
  -- msSinceEpoch() with no args gets the time 'now'
  local DT = odc.msSinceEpochToDateTime(odc.msSinceEpoch());
  odc.log.info("msSinceEpochToDateTime() returns "..DT);
  -- You can get fancy and even use formatter strings
  local some_time_fmt1 = "09/02/1983 @ 13:42:01.234";
  local some_time_msse = odc.msSinceEpoch(some_time_fmt1,"%d/%m/%Y @ %H:%M:%S.%e");
  local some_time_fmt2 = odc.msSinceEpochToDateTime(some_time_msse, "%H-%M-%S.%e ON %Y-%m-%d");
  odc.log.info(some_time_fmt1.." reformatted as "..some_time_fmt2);
  
  -- If you need to decode JSON strings,
  -- Just like the port config was converted to a table, the function is here for general usage
  local SomeJSON = '{"this" : ["is", "a" , "json", "string",{"test" : 42}], "empty" : {}, "nada" : null, "float" : 0.12345}';
  local TableFromJSON = odc.DecodeJSON(SomeJSON);
  -- and if you need to encode json (handy for dumping tables for debugging)
  PrettyPrintJSON = true;
  local JSONFromTable = odc.EncodeJSON(TableFromJSON);
  
  odc.log.info("JSON: "..SomeJSON);
  odc.log.info("JSON after decode and re-encode with formatting: "..JSONFromTable);
  
  --optionally change the default OctetString formatting
  --Can also set this in the Port JSON config
  odc.OctetStringFormat = odc.DataToStringMethod.Hex;
  
  --little helpers if you're using Hex formatted OctetString payloads
  local MyHexString = odc.String2Hex("raw string");
  local MyRawString = odc.Hex2String(MyHexString);
  odc.log.info("'raw string' =>String2Hex=> '"..MyHexString.."' =>Hex2String=> '"..MyRawString.."'");
  --including binary
  local MyBinHexString = odc.String2Hex(string.char(0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55));
  odc.log.info("String2Hex(string.char(0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55)) returns "..MyBinHexString);
  local NotHex = "abcdefgh";
  local TryAnyway = odc.Hex2String(NotHex);
  if TryAnyway == nil then odc.log.error(string.format("'%s' is not a hex string",NotHex)) end
  
  --example publish a Binary event
  local MyEventInfo = {};
  MyEventInfo.EventType = odc.EventType.Binary;
  MyEventInfo.Index = 345;
  --let SenderName, QualityFlags and Timestamp default
  MyEventInfo.Payload = false;
  odc.PublishEvent(MyEventInfo); --leaving out the optional second parameter
  
  --example publish a DoubleBitBinary event
  local DBBEvt =
  {
    EventType = odc.EventType.DoubleBitBinary,
    Index = 999,
    SenderName = "Spoof",
    QualityFlags = odc.QualityFlags(odc.QualityFlag.ONLINE,odc.QualityFlag.LOCAL_FORCED),
    Timestamp = "2023-06-19 19:22:27.645", --either ms since epoch (system clock epoch) or %Y-%m-%d %H:%M:%S.%e
    --DoubleBitBinary payload needs First and Second
    Payload =
    {
      First = true,
      Second = true
    }
  };
  --If you want a callback from the destination
  local MyStatusCB = function(cmd_stat) odc.log.trace("DBBEvt callback result "..odc.ToString.CommandStatus(cmd_stat)) end;
  odc.PublishEvent(DBBEvt, MyStatusCB);
  
  --for a more complex event type, use MakePayload to get a default, then just change what you want:
  local MyControlEvent =
  {
    EventType = odc.EventType.ControlRelayOutputBlock,
    Index = 555,
    Payload = odc.MakePayload.ControlRelayOutputBlock()
  };
  
  MyControlEvent.Payload.ControlCode = odc.ControlCode.PULSE_ON;
  odc.log.info("Payload constructed: "..odc.EncodeJSON(MyControlEvent.Payload));
  odc.PublishEvent(MyControlEvent);
  
  odc.log.info("Here's everything under 'odc' for good measure: "..odc.EncodeJSON(odc));
  odc.log.info("...Plus all the default payloads from odc.MakePayload(): "..dump_default_payloads_json());

end
   
function open_for_business_example()
  local PortUpEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_UP };
  local ConnectedEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.CONNECTED };
  odc.PublishEvent(PortUpEvent);
  odc.PublishEvent(Connected);
end

function closed_for_business_example()
  local DisconnectedEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.DISCONNECTED };
  local PortDownEvent = { EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_DOWN };
  odc.PublishEvent(DisconnectedEvent);
  odc.PublishEvent(PortDownEvent);
end

function process_config_example()
  local conf = odc.Config;
  local serialised_as_json = odc.EncodeJSON(conf);
  odc.log.info("Turned my config back into JSON : "..serialised_as_json);
end

function dump_default_payloads_json()
  EventTypeNames = get_sorted_keys(odc.EventType);
  AllPayloads = "";
  for _,ETName in pairs(EventTypeNames) do
    AllPayloads = AllPayloads.."\n"..ETName.." => "..odc.EncodeJSON(odc.MakePayload[ETName]());
  end
  return AllPayloads;
end

function get_sorted_keys(Tab)
  local n=1; local ks = {};
  for k,_ in pairs(Tab) do
    ks[n] = k;
    n=n+1;
  end
  table.sort(ks);
  return ks;
end




