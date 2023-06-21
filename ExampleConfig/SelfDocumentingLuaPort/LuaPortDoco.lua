--opendatacon mandates an Enable() function
function Enable()
	log.info("Enable()");
	return;
end

--opendatacon mandates a Disable() function
function Disable()
	log.info("Disable()");
	return;
end

--opendatacon mandates a Build() function
--when called, it's a good time to process config and initialise things etc.
function Build()
	log.info("Build()");
	return;
end

--this isn't required
--just a little function for debugging/illustration
--stolen from: https://stackoverflow.com/questions/9168058/how-to-dump-a-table-to-console
function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end


--opendatacon mandates an Event() function so you can receive events
function Event(EventInfo, SenderName, StatusCallback)
	log.info("Event(): EventType ".. ToString.EventType(EventInfo.EventType)..
           ", Index "..EventInfo.Index..
           ", Timestamp "..msSinceEpochToDateTime(EventInfo.Timestamp)..
           ", QualityFlags "..ToString.QualityFlags(EventInfo.QualityFlags)..
           ", Payload "..dump(EventInfo.Payload)..
           ", SenderName "..SenderName);
  StatusCallback(CommandStatus.SUCCESS);
  
  --echo the same event back as a test, but make a new callback
  CB = function(cmd_stat)
      log.info("Publish callback called with "..ToString.CommandStatus(cmd_stat));
  end
  
  PublishEvent(EventInfo,CB);
	
  return;
end

-- There's a bunch of enum values you can access, and logging functions
log.info("EventType.Analog : " .. EventType.Analog);
log.info("EventType.Binary : " .. EventType.Binary);
log.info("CommandStatus.SUCCESS : " .. CommandStatus.SUCCESS);
log.info("QualityFlag.COMM_LOST : " .. QualityFlag.COMM_LOST);
log.info("QualityFlag.RESTART : " .. QualityFlag.RESTART);

--Quality flags can be combined (it's just bitwise OR convenience function)
Q = QualityFlags(QualityFlag.COMM_LOST,QualityFlag.RESTART);
log.info("QualityFlags() returns " .. Q .. ": " .. ToString.QualityFlags(Q));

-- You can get ms since epoch, and convert to something readable
DT = msSinceEpochToDateTime(msSinceEpoch());
log.info("msSinceEpochToDateTime() returns "..DT);

--optionally change the default OctetString formatting
--Can also set this in the Port JSON config
OctetStringFormat = DataToStringMethod.Hex;

--little helpers if you're using Hex OctetStrings
MyHexString = String2Hex("raw string");
MyRawString = Hex2String(MyHexString);
log.info("'raw string' =>String2Hex=> '"..MyHexString.."' =>Hex2String=> '"..MyRawString.."'");
--including binary
MyBinHexString = String2Hex(string.char(0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55));
log.info("String2Hex(string.char(0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55)) returns "..MyBinHexString);
NotHex = "abcdefgh";
TryAnyway = Hex2String(NotHex);
if TryAnyway == nil then log.error(string.format("'%s' is not a hex string",NotHex)) end


--example publish a Binary event
MyEventInfo = {};
MyEventInfo.EventType = EventType.Binary;
MyEventInfo.Index = 345;
--let SenderName, QualityFlags and Timestamp default
MyEventInfo.Payload = false;
PublishEvent(MyEventInfo); --leaving out the optional second parameter

--example publish a DoubleBitBinary event
DBBEvt =
{
  EventType = EventType.DoubleBitBinary,
  Index = 999,
  SenderName = "Spoof",
  QualityFlags = QualityFlags(QualityFlag.ONLINE,QualityFlag.LOCAL_FORCED),
  Timestamp = "2023-06-19 19:22:27.645", --either ms since epoch (system clock epoch) or %Y-%m-%d %H:%M:%S.%e
  --DoubleBitBinary payload needs First and Second
  Payload =
  {
    First = true,
    Second = true
  }
};
--If you want a callback from the destination
MyStatusCB = function(cmd_stat) log.trace("DBBEvt callback result "..ToString.CommandStatus(cmd_stat)) end;
PublishEvent(DBBEvt, MyStatusCB);

--for a more complex event type, use MakePayload to get a default, then just change what you want:
MyControlEvent =
{
  EventType = EventType.ControlRelayOutputBlock,
  Index = 555,
  Payload = MakePayload.ControlRelayOutputBlock()
};

MyControlEvent.Payload.ControlCode = ControlCode.LATCH_OFF;
log.info("Payload constructed: "..dump(MyControlEvent.Payload));
PublishEvent(MyControlEvent);











