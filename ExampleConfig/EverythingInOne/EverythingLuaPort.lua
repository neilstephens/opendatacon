local running = false
local cancel_loop = nil
local counter = 0

function Build()
     odc.log.info("EverythingLuaPort Build()")
end

function Enable()
     running = true
     cancel_loop = odc.msCoroutineLoop(coroutine.wrap(function()
          while running do
               counter = counter + 1
               odc.PublishEvent({
                    EventType = odc.EventType.Analog,
                    Index = 1,
                    QualityFlags = odc.QualityFlags(odc.QualityFlag.ONLINE),
                    Payload = counter
               })
               coroutine.yield(2000)
          end
     end))
end

function Disable()
     running = false
     if cancel_loop then
          cancel_loop()
          cancel_loop = nil
     end
end

function Event(EventInfo, SenderName, StatusCallback)
     if odc.log.level.ShouldLog(odc.log.level.debug) then
          odc.log.debug("EverythingLuaPort Event from " .. SenderName .. " idx=" .. tostring(EventInfo.Index))
     end
     StatusCallback(odc.CommandStatus.SUCCESS)
end
