function Event(EventInfo, AllowFn)
     if EventInfo.EventType == odc.EventType.Analog then
          EventInfo.Payload = EventInfo.Payload + 0.25
     elseif EventInfo.EventType == odc.EventType.Binary then
          EventInfo.Payload = not EventInfo.Payload
     end
     AllowFn(EventInfo)
end
