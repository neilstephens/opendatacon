
local hello = {};

function hello.go()
  odc.log.info("Hello from " .. odc.GetPath.ScriptDir("hello.lua"));
end
  
return hello;
