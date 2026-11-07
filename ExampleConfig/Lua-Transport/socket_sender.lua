-- socket_sender.lua: the Sender of the TCP and TLS examples (see ../README.md).
-- A LuaPort next to the source of the events. Its Event() receives the events the source publishes, writes each as one
-- line of JSON to a TCP or TLS connection to the Receiver, and publishes the controls that come back (with their
-- answer sent back on the same connection).
--
-- Settings (ConfOverrides), all optional:
--   Host, Port       where the Receiver listens (127.0.0.1, 20150)
--   RetryMs          time between connection attempts (500)
--   MaxLineBytes     the longest line accepted from the Receiver (65536)
--   QueueLimit       events kept while the connection is down; the oldest is dropped beyond it (1000)
--   UseTLS           true: odc.TLSSocket with mutual TLS (needs CertFile, KeyFile, CAFile), false: odc.TCPSocket
--   CertFile, KeyFile, CAFile   this side's certificate and key, and the CA that signed the Receiver's (certs/sender.crt, certs/sender.key, certs/ca.crt)
--   PeerName         the name the Receiver's certificate must have (default: Host)

package.path = odc.GetPath.ScriptDir("?.lua")..";"..package.path
local wire = require("odc_wire")

local Conf = odc.Config
local Host = Conf.Host or "127.0.0.1"
local Port = Conf.Port or 20150
local RetryMs = math.floor(Conf.RetryMs or 500)
local MaxLineBytes = Conf.MaxLineBytes or 65536
local QueueLimit = Conf.QueueLimit or 1000
local UseTLS = Conf.UseTLS == true

local sock
local connected, enabled, outage_logged = false, false, false
local lines = wire.line_buffer(MaxLineBytes)
local queue = wire.event_queue(QueueLimit)

local function write_line(msg, serial)
	local text, why = wire.encode(msg)
	if not text then
		odc.log.error("cannot encode a message: "..tostring(why))
		return false
	end
	local ok, err
	if serial then ok, err = sock:write_to(serial, text.."\n") else ok, err = sock:write(text.."\n") end
	if not ok then
		odc.log.warning("cannot write: "..tostring(err))
		return false
	end
	return true
end

-- sends what is queued, in order; stops at the first that cannot be sent
local function flush()
	while connected and queue.size() > 0 do
		local e = queue.take(1)[1]
		local msg, why = wire.event_msg(e)
		if not msg then
			odc.log.error("event not sent: "..why)
		elseif not write_line(msg) then
			queue.put_back({ e })
			return
		end
	end
end

-- a line from the Receiver: a control to carry out, answered with its status
local function on_line(line, serial)
	local msg, why = wire.decode(line)
	if not msg then
		odc.log.error("rejected a message: "..tostring(why))
		return
	end
	if msg.kind ~= "control" then
		odc.log.error("rejected a message of kind '"..msg.kind.."': only controls come this way")
		return
	end
	local id = msg.id
	if math.type(id) ~= "integer" then
		odc.log.error("rejected a control without an id")
		return
	end
	local event, event_why = wire.decode_event(msg.event)
	local answered = false
	local function answer(status)
		if answered then return end
		answered = true
		write_line(wire.status_msg(id, status), serial)
	end
	if not event or not wire.is_control(event.EventType) then
		odc.log.error("rejected control "..id..": "..tostring(event_why or "not a control type"))
		answer(odc.CommandStatus.FORMAT_ERROR)
		return
	end
	odc.log.info("control "..id..": "..odc.ToString.EventType(event.EventType).." index "..event.Index.." published to the source")
	odc.PublishEvent(event, answer)
end

local function make_socket()
	local options =
	{
		host = Host, port = Port, auto_reopen = true, retry_ms = RetryMs,
		on_open = function(s)
			connected, outage_logged = true, false
			lines.reset()
			odc.log.info("connected to the Receiver at "..Host..":"..Port.." (connection "..s:serial()..")")
			flush()
		end,
		on_data = function(s, data)
			local serial = s:serial()
			for _, line in ipairs(lines.feed(data)) do on_line(line, serial) end
		end,
		on_close = function(s, reason, detail)
			connected = false
			lines.reset()
			odc.log.warning("connection to the Receiver ended: "..tostring(reason)..(detail and (" ("..detail..")") or "")..(enabled and "; reconnecting" or ""))
		end,
		on_error = function(s, reason, detail)
			if not outage_logged then
				outage_logged = true
				odc.log.warning("cannot connect to the Receiver at "..Host..":"..Port..": "..tostring(reason)..(detail and (" ("..detail..")") or "").."; trying again every "..RetryMs.." ms")
			end
		end,
	}
	if not UseTLS then return odc.TCPSocket(options) end
	options.peer_name = Conf.PeerName
	options.tls =
	{
		CertFile = Conf.CertFile or "certs/sender.crt",
		KeyFile = Conf.KeyFile or "certs/sender.key",
		CAFile = Conf.CAFile or "certs/ca.crt",
	}
	options.on_tls_failed = function(s, reason, detail)
		odc.log.error("TLS failed: "..tostring(reason).." ("..tostring(detail)..")")
	end
	local original_open = options.on_open
	options.on_open = function(s, peer)
		odc.log.info("TLS session with "..tostring(peer and peer.subject)..", "..tostring(peer and peer.protocol))
		original_open(s)
	end
	return odc.TLSSocket(options)
end

function Build() end

function Enable()
	enabled = true
	local s, err = make_socket()
	if not s then
		odc.log.error("cannot make the socket: "..tostring(err).." (for TLS: run make-certs first)")
		return
	end
	sock = s
	sock:open()
end

function Disable()
	enabled = false
	connected = false
	if sock then sock:close() end
	sock = nil
end

-- an event from the source: queued, and sent if the connection is up
function Event(EventInfo, SenderName, StatusCallback)
	if wire.is_control(EventInfo.EventType) then
		StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
		return
	end
	local _, why = wire.event_msg(EventInfo)
	if why then
		odc.log.error("event not sent: "..why)
		StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
		return
	end
	queue.push(EventInfo)
	flush()
	StatusCallback(odc.CommandStatus.SUCCESS)
end
