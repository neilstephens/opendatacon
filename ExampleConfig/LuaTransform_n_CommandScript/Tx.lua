--opendatacon mandates an Event() function so you can receive events to transform
function Event(EventInfo)
	odc.log.info("Event(): EventType ".. odc.ToString.EventType(EventInfo.EventType)..
            ", Index "..EventInfo.Index..
            ", Timestamp "..odc.msSinceEpochToDateTime(EventInfo.Timestamp)..
            ", QualityFlags "..odc.ToString.QualityFlags(EventInfo.QualityFlags)..
            ", Payload "..odc.EncodeJSON(EventInfo.Payload)..
            ", SourcePort "..EventInfo.SourcePort);
  
  --transform the data
  EventInfo.SourcePort = "Wouldn't you like to know";
  EventInfo.Index = 100*(EventInfo.Index+1);
  EventInfo.QualityFlags = odc.QualityFlags(EventInfo.QualityFlags, odc.QualityFlag.LOCAL_FORCED);
  
  --return nil if you want to drop the event altogether,
  --or return a transformed EventInfo
  return EventInfo;
end 
