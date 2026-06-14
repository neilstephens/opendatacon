function LogSink(Time, LoggerName, Level, Message)
     local warn_level = 3
     if odc and odc.log and odc.log.level and type(odc.log.level.warn) == "number" then
          warn_level = odc.log.level.warn
     end
     if type(Level) == "number" and Level >= warn_level then
          local stamp = odc.msSinceEpochToDateTime(Time)
          io.stderr:write("[LuaLogSink] " .. stamp .. " " .. LoggerName .. " " .. Message .. "\n")
     end
end
