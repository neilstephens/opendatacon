--opendatacon mandates an Event() function so you can receive events to transform
function Event(EventInfo,AllowFn)
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
  
  --call the Allow function with nil if you want to drop the event
  --  but you must call it either way
  AllowFn(EventInfo);
end 
