-- odc_wire.lua: the wire format of the Lua-Transport examples (see README.md in this directory).
--
-- Serialises ODC events (as a LuaPort's Event() receives them and odc.PublishEvent() takes them) to JSON-able Lua
-- tables and back, so that a Sender and a Receiver LuaPort can move them over any transport:
--
--   local wire = require("odc_wire")
--   local text = wire.encode(wire.event_msg(EventInfo))      -- a JSON text on one line
--   local msg, err = wire.decode(text)                       -- msg.kind is "event", "events", "control", "status" or "ack"
--   local event, err = wire.decode_event(msg.event)          -- back to an EventInfo table for odc.PublishEvent()
--
-- Messages are { v = 1, kind = ..., ... }; v is the format version (a reader refuses a version it does not know).
-- An event is { type = "Analog", index = 3, quality = 1, quality_text = "ONLINE", time = <ms since epoch>, payload = ... }
-- with a payload that depends on the type (see `codecs` below). SourcePort is not sent: the Receiver's name is the
-- source of what it publishes. Every function that can fail returns nil and a message that says what is wrong.

local M = {}
M.VERSION = 1

local ET = odc.EventType

-- JSON text on one line (odc.EncodeJSON indents only if the global PrettyPrintJSON is true)
local function compact_json(t)
	local saved = PrettyPrintJSON
	PrettyPrintJSON = false
	local ok, text = pcall(odc.EncodeJSON, t)
	PrettyPrintJSON = saved
	if not ok then return nil, "cannot encode: "..tostring(text) end
	return text
end

local function whole(x, lo, hi)
	if math.type(x) == "float" and x == math.floor(x) and x >= -2^53 and x <= 2^53 then x = math.tointeger(x) end
	if math.type(x) ~= "integer" or x < lo or x > hi then return nil end
	return x
end

local function finite(x)
	return type(x) == "number" and x == x and x ~= math.huge and x ~= -math.huge
end

-- enumerations travel as their names (odc.ToString.* gives the name, odc.<Enum>[name] the value)
local function name_of(tostring_fn, value)
	if math.type(value) ~= "integer" then return nil end
	local name = tostring_fn(value)
	if type(name) ~= "string" or name == "" then return nil end
	return name
end

local function status_name(v) return name_of(odc.ToString.CommandStatus, v) end
local function status_value(name) return type(name) == "string" and odc.CommandStatus[name] or nil end
M.status_name = status_name
M.status_value = status_value

-- one codec per event type: encode(payload) -> JSON-able value | nil, why; decode(value) -> payload | nil, why
local codecs = {}

local function boolean_codec()
	return {
		encode = function(p) if type(p) ~= "boolean" then return nil, "payload is not a boolean" end return p end,
		decode = function(v) if type(v) ~= "boolean" then return nil, "payload is not a boolean" end return v end,
	}
end

local function number_codec()
	return {
		encode = function(p) if not finite(p) then return nil, "payload is not a finite number" end return p end,
		decode = function(v) if not finite(v) then return nil, "payload is not a finite number" end return v end,
	}
end

local function counter_codec()
	local function check(v)
		local n = whole(v, 0, 4294967295)
		if not n then return nil, "payload is not an integer from 0 to 4294967295" end
		return n
	end
	return { encode = check, decode = check }
end

codecs.Binary = boolean_codec()
codecs.BinaryOutputStatus = boolean_codec()
codecs.Analog = number_codec()
codecs.AnalogOutputStatus = number_codec()
codecs.Counter = counter_codec()
codecs.FrozenCounter = counter_codec()

codecs.DoubleBitBinary =
{
	encode = function(p)
		if type(p) ~= "table" or type(p.First) ~= "boolean" or type(p.Second) ~= "boolean" then return nil, "payload is not { First, Second } booleans" end
		return { first = p.First, second = p.Second }
	end,
	decode = function(v)
		if type(v) ~= "table" or type(v.first) ~= "boolean" or type(v.second) ~= "boolean" then return nil, "payload is not { first, second } booleans" end
		return { First = v.first, Second = v.second }
	end,
}

-- raw bytes travel as a hex string
codecs.OctetString =
{
	encode = function(p)
		if type(p) ~= "string" then return nil, "payload is not a string" end
		if p == "" then return "" end
		return odc.String2Hex(p)
	end,
	decode = function(v)
		if type(v) ~= "string" then return nil, "payload is not a hex string" end
		if v == "" then return "" end
		local raw = odc.Hex2String(v)
		if raw == nil then return nil, "payload is not a hex string" end
		return raw
	end,
}

codecs.ControlRelayOutputBlock =
{
	encode = function(p)
		if type(p) ~= "table" then return nil, "payload is not a CROB table" end
		local code = name_of(odc.ToString.ControlCode, p.ControlCode)
		local status = status_name(p.CommandStatus)
		if not code or not status or not whole(p.Count, 0, 255) or not whole(p.msOnTime, 0, 4294967295) or not whole(p.msOffTime, 0, 4294967295) then
			return nil, "payload is not a valid CROB (ControlCode, Count, msOnTime, msOffTime, CommandStatus)"
		end
		return { code = code, count = p.Count, on_ms = p.msOnTime, off_ms = p.msOffTime, status = status }
	end,
	decode = function(v)
		if type(v) ~= "table" then return nil, "payload is not a CROB object" end
		local code = type(v.code) == "string" and odc.ControlCode[v.code] or nil
		local status = status_value(v.status)
		local count, on_ms, off_ms = whole(v.count, 0, 255), whole(v.on_ms, 0, 4294967295), whole(v.off_ms, 0, 4294967295)
		if not code or not status or not count or not on_ms or not off_ms then
			return nil, "payload is not a valid CROB (code, count, on_ms, off_ms, status)"
		end
		return { ControlCode = code, Count = count, msOnTime = on_ms, msOffTime = off_ms, CommandStatus = status }
	end,
}

-- an analog output: { value, status }; Int16 and Int32 carry integers, Float32 and Double64 numbers
local function analog_output_codec(lo, hi)
	local function value_ok(x)
		if lo then return whole(x, lo, hi) end
		if finite(x) then return x end
		return nil
	end
	return {
		encode = function(p)
			if type(p) ~= "table" then return nil, "payload is not an analog output table" end
			local value, status = value_ok(p.Value), status_name(p.CommandStatus)
			if value == nil or not status then return nil, "payload is not a valid analog output (Value, CommandStatus)" end
			return { value = value, status = status }
		end,
		decode = function(v)
			if type(v) ~= "table" then return nil, "payload is not an analog output object" end
			local value, status = value_ok(v.value), status_value(v.status)
			if value == nil or not status then return nil, "payload is not a valid analog output (value, status)" end
			return { Value = value, CommandStatus = status }
		end,
	}
end
codecs.AnalogOutputInt16 = analog_output_codec(-32768, 32767)
codecs.AnalogOutputInt32 = analog_output_codec(-2147483648, 2147483647)
codecs.AnalogOutputFloat32 = analog_output_codec(nil)
codecs.AnalogOutputDouble64 = analog_output_codec(nil)

codecs.ConnectState =
{
	encode = function(p)
		local name = name_of(odc.ToString.ConnectState, p)
		if not name then return nil, "payload is not a connect state" end
		return name
	end,
	decode = function(v)
		local value = type(v) == "string" and odc.ConnectState[v] or nil
		if value == nil then return nil, "payload is not a connect state name" end
		return value
	end,
}

-- the control types: what the Receiver forwards from the sink side to the source side
local controls = { ControlRelayOutputBlock = true, AnalogOutputInt16 = true, AnalogOutputInt32 = true, AnalogOutputFloat32 = true, AnalogOutputDouble64 = true }

function M.is_control(event_type_value)
	local name = math.type(event_type_value) == "integer" and odc.ToString.EventType(event_type_value) or nil
	return name ~= nil and controls[name] == true
end

function M.supported_types()
	local list = {}
	for name in pairs(codecs) do list[#list+1] = name end
	table.sort(list)
	return list
end

-- an EventInfo table (from Event()) -> the JSON-able event
function M.encode_event(e)
	if type(e) ~= "table" or math.type(e.EventType) ~= "integer" then return nil, "not an event" end
	local name = odc.ToString.EventType(e.EventType)
	local codec = codecs[name]
	if not codec then return nil, "unsupported event type "..tostring(name).." ("..e.EventType..")" end
	local index = whole(e.Index, 0, 4294967295)
	local quality = whole(e.QualityFlags or 0, 0, 65535)
	local time = whole(e.Timestamp, 0, 2^53)
	if not index or not quality or not time then return nil, name.." event with an invalid index, quality or timestamp" end
	local payload, why = codec.encode(e.Payload)
	if payload == nil then return nil, name.." event: "..why end
	return { type = name, index = index, quality = quality, quality_text = odc.ToString.QualityFlags(quality), time = time, payload = payload }
end

-- a decoded JSON event -> an EventInfo table for odc.PublishEvent()
function M.decode_event(t)
	if type(t) ~= "table" or type(t.type) ~= "string" then return nil, "event has no type" end
	local codec = codecs[t.type]
	if not codec or ET[t.type] == nil then return nil, "unsupported event type '"..t.type.."'" end
	local index, quality, time = whole(t.index, 0, 4294967295), whole(t.quality, 0, 65535), whole(t.time, 0, 2^53)
	if not index or not quality or not time then return nil, t.type.." event with an invalid index, quality or time" end
	local payload, why = codec.decode(t.payload)
	if payload == nil then return nil, t.type.." event: "..why end
	return { EventType = ET[t.type], Index = index, QualityFlags = quality, Timestamp = time, Payload = payload }
end

-- ---- messages: { v, kind, ... }
function M.event_msg(e)
	local ev, why = M.encode_event(e)
	if not ev then return nil, why end
	return { v = M.VERSION, kind = "event", event = ev }
end

-- a batch: the events that cannot be encoded are left out and counted
function M.events_msg(list)
	local events, skipped = {}, 0
	for _, e in ipairs(list) do
		local ev, why = M.encode_event(e)
		if ev then events[#events+1] = ev else
			skipped = skipped + 1
			odc.log.error("event not sent: "..why)
		end
	end
	return { v = M.VERSION, kind = "events", events = events }, skipped
end

function M.control_msg(id, e)
	local ev, why = M.encode_event(e)
	if not ev then return nil, why end
	if not M.is_control(e.EventType) then return nil, ev.type.." is not a control type" end
	return { v = M.VERSION, kind = "control", id = id, event = ev }
end

function M.status_msg(id, status)
	local name = status_name(status)
	if not name then return nil, "not a CommandStatus" end
	return { v = M.VERSION, kind = "status", id = id, status = name }
end

function M.ack_msg(accepted, rejected)
	return { v = M.VERSION, kind = "ack", accepted = accepted, rejected = rejected }
end

M.compact_json = compact_json

function M.encode(msg)
	return compact_json(msg)
end

local kinds = { event = true, events = true, control = true, status = true, ack = true }

-- a JSON text -> a message table, or nil and what is wrong (odc.DecodeJSON logs a parse error itself)
function M.decode(text)
	if type(text) ~= "string" or text == "" then return nil, "empty message" end
	local ok, msg = pcall(odc.DecodeJSON, text)
	if not ok or type(msg) ~= "table" then return nil, "not a JSON object" end
	if math.type(msg.v) ~= "integer" then return nil, "no version (v)" end
	if msg.v ~= M.VERSION then return nil, "unsupported version "..msg.v end
	if not kinds[msg.kind] then return nil, "unknown kind '"..tostring(msg.kind).."'" end
	return msg
end

-- ---- helpers the Senders and Receivers share

-- Cuts a stream into lines, whatever the way the data was split or joined; a line over max_bytes is dropped (and
-- counted) up to its end, and the stream carries on after it.
function M.line_buffer(max_bytes)
	local buffer, discarding, dropped = "", false, 0
	local lb = {}
	function lb.feed(data)
		local lines = {}
		buffer = buffer..data
		while true do
			local nl = buffer:find("\n", 1, true)
			if not nl then
				if #buffer > max_bytes then
					buffer, discarding, dropped = "", true, dropped + 1
					odc.log.error("a line over "..max_bytes.." bytes is being dropped")
				end
				break
			end
			local line = buffer:sub(1, nl - 1)
			buffer = buffer:sub(nl + 1)
			if discarding then
				discarding = false
			elseif #line > max_bytes then
				dropped = dropped + 1
				odc.log.error("a line over "..max_bytes.." bytes was dropped")
			else
				line = line:gsub("\r$", "")
				if line ~= "" then lines[#lines+1] = line end
			end
		end
		return lines
	end
	function lb.reset() buffer, discarding = "", false end
	function lb.dropped() return dropped end
	return lb
end

-- Controls waiting for their answer: add(callback) -> id; resolve(id, status) -> true if it was waiting; fail_all(status).
-- A control that is not answered within timeout_ms gets odc.CommandStatus.TIMEOUT.
function M.pending_controls(timeout_ms)
	local waiting, next_id = {}, 0
	local p = {}
	local function finish(id, status)
		local cb = waiting[id]
		if not cb then return false end
		waiting[id] = nil
		cb(status)
		return true
	end
	function p.add(cb)
		next_id = next_id + 1
		local id = next_id
		waiting[id] = cb
		odc.msTimerCallback(timeout_ms, function()
			if finish(id, odc.CommandStatus.TIMEOUT) then odc.log.warning("control "..id.." was not answered within "..timeout_ms.." ms") end
		end)
		return id
	end
	function p.resolve(id, status) return math.type(id) == "integer" and finish(id, status) end
	function p.fail_all(status)
		local ids = {}
		for id in pairs(waiting) do ids[#ids+1] = id end
		for _, id in ipairs(ids) do finish(id, status) end
	end
	return p
end

-- A bounded first-in first-out queue of events; when it is full the oldest is dropped (and logged).
function M.event_queue(limit)
	local items, first, last, dropped = {}, 1, 0, 0
	local q = {}
	function q.push(e)
		if last - first + 1 >= limit then
			items[first] = nil
			first = first + 1
			dropped = dropped + 1
			odc.log.error("the event queue is full ("..limit.."): the oldest event is dropped")
		end
		last = last + 1
		items[last] = e
	end
	function q.size() return last - first + 1 end
	function q.dropped() return dropped end
	-- up to n events, removed
	function q.take(n)
		local out = {}
		while #out < n and first <= last do
			out[#out+1] = items[first]
			items[first] = nil
			first = first + 1
		end
		return out
	end
	-- put events taken back at the front, in their order
	function q.put_back(list)
		for i = #list, 1, -1 do
			first = first - 1
			items[first] = list[i]
		end
	end
	return q
end

return M
