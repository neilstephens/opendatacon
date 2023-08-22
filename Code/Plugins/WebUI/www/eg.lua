
SendCommands = coroutine.wrap(
  function(...)
    local args = {...};
    odc.log.info("SendCommands() called with args: "..odc.EncodeJSON(args));
    odc.UIMessage("Status message.");
    
    local x = 0;
    while x <= 10 do
      odc.UICommand("SimControl", "SendEvent", ".*", "Analog", tostring(x), tostring(x), "ONLINE");
      coroutine.yield(300);
      x = x+1;
    end
    
    odc.log.info("Command script finished");
  end
); 
