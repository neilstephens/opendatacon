 
-- provide a function called LogSink opendatacon will call it with args:
--    Time - integer ms since epoch
--    LoggerName - the log 'topic'
--    Level - integer (enumerate with ocd.log.level)
--    Message - string with the actual log message

function LogSink(Time,LoggerName,Level,Message)
  local record = odc.msSinceEpochToDateTime(Time).." || "..LoggerName.." || "..odc.log.level.ToString(Level).." || "..Message;
  checkLevel(Level,record);
end


-- This example log sink stores a rolling buffer of everything,
-- and when a message comes in over the threshold log level,
-- it prints the unprinted portion of the buffer to file.
-- Also prints "..." to indicate if there were unprinted messages

local filename = "context.log";
if type(odc.Config["OutputFile"]) == "string" then
  filename = odc.Config["OutputFile"];
end
local file = io.open(filename,"w");

local threshold = odc.log.level.error;
local buffer = {};
local before = 10;
local pos = 1;
local not_printed = 0;
local gap_printed = false;

function checkLevel(LvL,Rec)
  if LvL >= threshold then
    local n = pos;
    local skip = before - not_printed;
    local first=true;
    while skip > 0 do
      first = false;
      if n > before then n = 1 end
      n = n+1;
      skip = skip-1;
    end
    if n > before then n = 1 end
    while n ~= pos or first == true do
      first = false;
      file:write(buffer[n].."\n");
      n = n+1;
      if n > before then n = 1 end
    end
    file:write(Rec.."\n");
    not_printed = 0;
    gap_printed = false;
  elseif not_printed < before then
    not_printed = not_printed+1;
  elseif gap_printed == false then
    file:write("...\n");
    gap_printed = true;
  end
  buffer[pos] = Rec;
  pos = pos+1;
  if pos > before then pos = 1 end
end

