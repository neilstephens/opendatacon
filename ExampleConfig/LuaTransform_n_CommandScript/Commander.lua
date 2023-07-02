
--required function for sending UI commands
-- you could use a plain function called SendCommands
-- but wrapping a coroutine is a nice option so you can yeild and continue where you left off
SendCommands = coroutine.wrap(
  function(...)
    --arguments will be passed on the first invocation from the UI
    --  subsequent calls won't be passed arguments
    local args = {...};
    
    -- you can log messages
    odc.log.info("SendCommands() called with args: "..odc.EncodeJSON(args));
    
    -- send a command:
    --     arguments are the responder (collection) name (or blank for root command), the command, and any command arguments  
    odc.UICommand("DataPorts", "List", ".*");
    
    -- commands return a table of results
    local add_result = odc.UICommand("", "add_logsink", "test", "error", "SYSLOG", "127.0.0.1");
    local sinks = odc.UICommand("", "ls_logsink");
    
    -- you can also send messages back to the UI
    odc.UIMessage("Tried adding a log sink: "..odc.EncodeJSON(add_result));
    odc.UIMessage("Log sink listing: "..odc.EncodeJSON(sinks));
    
    -- return, or yeild in this case, the number of milliseconds until this function should be called/resumed again
    coroutine.yield(5000);
    
    odc.UICommand("", "del_logsink", "test");
    sinks = odc.UICommand("", "ls_logsink");
    odc.UIMessage("Log sink deleted again: "..odc.EncodeJSON(sinks));
    
    -- don't perform long running tasks without yeilding
    -- you can even yeild for zero millisconds to get called again immediately
    -- yeilding avoids hogging a thread from the odc threadpool for too long
    local x = 0;
    while x <= 10 do
      odc.UICommand("SimControl", "SendEvent", ".*", "Analog", tostring(x), tostring(x), "ONLINE");
      coroutine.yield(300);
      x = x+1;
    end
    
    odc.log.info("Command script finished");
    
  end
);

