

function Enable()
	print("[Lua] Enable()");
	return;
end

function Disable()
	print("[Lua] Disable()");
	return;
end

function ProcessConfig(Conf)
	print("[Lua] ProcessConfig()");
	return;
end

function EventHandler(EventType, Index, Time, Quality, Payload, Sender)
	print("[Lua] EventHandler( "..EventType..","..Index..")");
	return;
end
