-- source.lua: the "far end" of the Lua-Transport examples. A LuaPort that makes up events of every kind the examples
-- carry, publishes them, and answers the controls that come back with a status of its choice.
--
-- Settings (ConfOverrides), all optional:
--   StartDelayMs   time to wait before the first burst, so the transport is up (default 2000)
--   PeriodMs       time between the periodic Analog / Binary / Counter updates (default 2000, 0 = none)
--
-- The status it answers a control with depends on its index, so every case can be seen:
--   index 98: no answer at all (the requester's control times out)
--   index 99: NOT_SUPPORTED
--   an analog output with a value outside -1000 to 1000: OUT_OF_RANGE
--   anything else: SUCCESS

local Conf = odc.Config
local StartDelayMs = math.floor(Conf.StartDelayMs or 2000)
local PeriodMs = math.floor(Conf.PeriodMs or 2000)

local ET, Q = odc.EventType, odc.QualityFlag
local enabled = false
local tick_count, analog_value, binary_value, counter_value = 0, 230.0, false, 100

local function publish(type_name, index, payload, quality)
	odc.PublishEvent({ EventType = ET[type_name], Index = index, QualityFlags = quality or Q.ONLINE, Timestamp = odc.msSinceEpoch(), Payload = payload })
end

-- one event of every kind the examples carry
local function burst()
	odc.log.info("SOURCE: publishing one event of each kind")
	publish("ConnectState", 0, odc.ConnectState.PORT_UP)
	publish("ConnectState", 0, odc.ConnectState.CONNECTED)
	publish("Binary", 1, true)
	publish("Binary", 2, false, odc.QualityFlags(Q.ONLINE, Q.LOCAL_FORCED))
	publish("DoubleBitBinary", 1, { First = true, Second = false })
	publish("Analog", 1, 230.5)
	publish("Analog", 2, -0.125, odc.QualityFlags(Q.ONLINE, Q.OVERRANGE))
	publish("Counter", 1, 12345)
	publish("FrozenCounter", 1, 12000)
	publish("BinaryOutputStatus", 1, true)
	publish("AnalogOutputStatus", 1, 42.75)
	publish("OctetString", 1, "hello\0\255\r\n{\"json\":true}")
end

local function tick()
	if not enabled or PeriodMs <= 0 then return end
	tick_count = tick_count + 1
	analog_value = analog_value + ((tick_count % 2 == 0) and 0.25 or -0.25)
	binary_value = not binary_value
	counter_value = counter_value + 1
	publish("Analog", 3, analog_value)
	publish("Binary", 3, binary_value)
	publish("Counter", 3, counter_value)
	odc.msTimerCallback(PeriodMs, tick)
end

function Build() end

function Enable()
	enabled = true
	odc.msTimerCallback(StartDelayMs, function()
		if not enabled then return end
		burst()
		odc.msTimerCallback(PeriodMs > 0 and PeriodMs or 1000, tick)
	end)
end

function Disable()
	enabled = false
end

local function describe(e)
	local what = odc.ToString.EventType(e.EventType).." index "..e.Index
	if e.EventType == ET.ControlRelayOutputBlock then
		return what.." "..odc.ToString.ControlCode(e.Payload.ControlCode)..", count "..e.Payload.Count..", on "..e.Payload.msOnTime.." ms, off "..e.Payload.msOffTime.." ms"
	end
	return what.." value "..tostring(e.Payload.Value)
end

local function status_for(e)
	if e.Index == 98 then return nil end
	if e.Index == 99 then return odc.CommandStatus.NOT_SUPPORTED end
	if e.EventType ~= ET.ControlRelayOutputBlock and math.abs(e.Payload.Value) > 1000 then return odc.CommandStatus.OUT_OF_RANGE end
	return odc.CommandStatus.SUCCESS
end

function Event(EventInfo, SenderName, StatusCallback)
	local t = EventInfo.EventType
	if t == ET.ControlRelayOutputBlock or t == ET.AnalogOutputInt16 or t == ET.AnalogOutputInt32 or t == ET.AnalogOutputFloat32 or t == ET.AnalogOutputDouble64 then
		local status = status_for(EventInfo)
		if status == nil then
			odc.log.info("SOURCE: control "..describe(EventInfo).." received: not answering")
			return
		end
		odc.log.info("SOURCE: control "..describe(EventInfo).." received: answering "..odc.ToString.CommandStatus(status))
		StatusCallback(status)
		return
	end
	StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
end
