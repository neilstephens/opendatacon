 
-- log sink that stores a rolling buffer of everything,
-- prints the unprinted portion of the buffer when a message breaches the threshold level
-- prints "..." to indicate there were unprinted messages
 
local file = io.open("context.log","w");

local threshold = odc.log.level.error;
local buffer = {};
local before = 10;
local pos = 1;
local not_printed = 0;
local gap_printed = false;

function LogSink(Time,LoggerName,Level,Message)
  local record = odc.msSinceEpochToDateTime(Time).." || "..LoggerName.." || "..odc.log.level.ToString(Level).." || "..Message;
  if Level >= threshold then
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
    file:write(record.."\n");
    not_printed = 0;
    gap_printed = false;
  elseif not_printed < before then
    not_printed = not_printed+1;
  elseif gap_printed == false then
    file:write("...\n");
    gap_printed = true;
  end
  buffer[pos] = record;
  pos = pos+1;
  if pos > before then pos = 1 end
end


