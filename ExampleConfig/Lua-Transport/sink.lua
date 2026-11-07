-- sink.lua: the "near end" of the Lua-Transport examples. A LuaPort that logs every event that comes out of the
-- transport, and a little after it is enabled issues a series of controls that travel back to the source, logging the
-- CommandStatus each comes back with.
--
-- Settings (ConfOverrides), all optional:
--   ControlDelayMs   time to wait before the controls are issued, so the transport is up and some events have arrived (default 6000)

local Conf = odc.Config
local ControlDelayMs = math.floor(Conf.ControlDelayMs or 6000)

local ET = odc.EventType
local enabled = false

local function describe(e)
	local p = e.Payload
	local text
	if e.EventType == ET.OctetString then
		text = "hex "..odc.String2Hex(p)
	elseif type(p) == "table" then
		text = odc.EncodeJSON(p)
	elseif e.EventType == ET.ConnectState then
		text = odc.ToString.ConnectState(p)
	else
		text = tostring(p)
	end
	return odc.ToString.EventType(e.EventType).." index "..e.Index.." quality "..odc.ToString.QualityFlags(e.QualityFlags)..
		" time "..odc.msSinceEpochToDateTime(e.Timestamp).." payload "..text
end

-- issues a control and logs the status that comes back (the status callback is called once, whatever the answer is)
local function control(text, event)
	odc.PublishEvent(event, function(status)
		odc.log.info("SINK: control "..text.." => "..odc.ToString.CommandStatus(status))
	end)
end

local function crob(index, code)
	local payload = odc.MakePayload.ControlRelayOutputBlock()
	payload.ControlCode, payload.Count, payload.msOnTime, payload.msOffTime = code, 1, 500, 0
	return { EventType = ET.ControlRelayOutputBlock, Index = index, Payload = payload }
end

local function analog_output(type_name, index, value)
	local payload = odc.MakePayload[type_name]()
	payload.Value = value
	return { EventType = ET[type_name], Index = index, Payload = payload }
end

local function issue_controls()
	odc.log.info("SINK: issuing controls")
	control("CROB index 1 PULSE_ON (expect SUCCESS)", crob(1, odc.ControlCode.PULSE_ON))
	control("AnalogOutputInt16 index 2 value 42 (expect SUCCESS)", analog_output("AnalogOutputInt16", 2, 42))
	control("AnalogOutputDouble64 index 3 value 12.5 (expect SUCCESS)", analog_output("AnalogOutputDouble64", 3, 12.5))
	control("CROB index 99 LATCH_OFF (expect NOT_SUPPORTED)", crob(99, odc.ControlCode.LATCH_OFF))
	control("AnalogOutputFloat32 index 4 value 5000 (expect OUT_OF_RANGE)", analog_output("AnalogOutputFloat32", 4, 5000))
	control("CROB index 98 PULSE_ON (the source does not answer: expect TIMEOUT)", crob(98, odc.ControlCode.PULSE_ON))
end

function Build() end

function Enable()
	enabled = true
	odc.msTimerCallback(ControlDelayMs, function()
		if enabled then issue_controls() end
	end)
end

function Disable()
	enabled = false
end

function Event(EventInfo, SenderName, StatusCallback)
	odc.log.info("SINK: "..describe(EventInfo))
	StatusCallback(odc.CommandStatus.SUCCESS)
end
