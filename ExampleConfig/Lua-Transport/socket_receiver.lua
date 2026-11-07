-- socket_receiver.lua: the Receiver of the TCP and TLS examples (see ../README.md).
-- A LuaPort next to the sink. It listens for the Sender, reads one line of JSON at a time, turns each event back into
-- an ODC event and publishes it. Its Event() receives the controls the sink side issues, sends each to the Sender as a
-- line of JSON and gives the sink the CommandStatus that comes back (TIMEOUT if none does).
--
-- odc.TCPSocket and odc.TLSSocket servers take ONE connection at a time: a second Sender is refused until the first has gone.
--
-- Settings (ConfOverrides), all optional:
--   Host, Port         what to listen on (127.0.0.1, 20150)
--   MaxLineBytes       the longest line accepted from the Sender (65536)
--   ControlTimeoutMs   how long a control waits for its answer (5000)
--   UseTLS             true: odc.TLSSocket with mutual TLS (needs CertFile, KeyFile, CAFile), false: odc.TCPSocket
--   CertFile, KeyFile, CAFile   this side's certificate and key, and the CA that signed the Sender's (certs/receiver.crt, certs/receiver.key, certs/ca.crt)

package.path = odc.GetPath.ScriptDir("?.lua")..";"..package.path
local wire = require("odc_wire")

local Conf = odc.Config
local Host = Conf.Host or "127.0.0.1"
local Port = Conf.Port or 20150
local MaxLineBytes = Conf.MaxLineBytes or 65536
local ControlTimeoutMs = math.floor(Conf.ControlTimeoutMs or 5000)
local UseTLS = Conf.UseTLS == true

local sock
local connected, enabled = false, false
local lines = wire.line_buffer(MaxLineBytes)
local pending = wire.pending_controls(ControlTimeoutMs)
local published, rejected = 0, 0

local function write_line(msg)
	local text, why = wire.encode(msg)
	if not text then
		odc.log.error("cannot encode a message: "..tostring(why))
		return false
	end
	local ok, err = sock:write(text.."\n")
	if not ok then odc.log.warning("cannot write: "..tostring(err)) end
	return ok
end

-- a line from the Sender: an event to publish, or the answer to a control
local function on_line(line)
	local msg, why = wire.decode(line)
	if not msg then
		rejected = rejected + 1
		odc.log.error("rejected a message: "..tostring(why))
		return
	end
	if msg.kind == "event" then
		local event, event_why = wire.decode_event(msg.event)
		if not event then
			rejected = rejected + 1
			odc.log.error("rejected an event: "..tostring(event_why))
			return
		end
		published = published + 1
		odc.PublishEvent(event)
	elseif msg.kind == "status" then
		local status = wire.status_value(msg.status)
		if status == nil then
			odc.log.error("rejected a status that is not a CommandStatus: "..tostring(msg.status))
		elseif not pending.resolve(msg.id, status) then
			odc.log.warning("a status for control "..tostring(msg.id).." that is not waiting (answered too late?)")
		end
	else
		rejected = rejected + 1
		odc.log.error("rejected a message of kind '"..msg.kind.."'")
	end
end

local function make_socket()
	local options =
	{
		host = Host, port = Port, server = true, auto_reopen = true,
		on_open = function(s)
			connected = true
			lines.reset()
			odc.log.info("the Sender connected (connection "..s:serial()..")")
		end,
		on_data = function(s, data)
			for _, line in ipairs(lines.feed(data)) do on_line(line) end
		end,
		on_close = function(s, reason, detail)
			connected = false
			lines.reset()
			odc.log.warning("the Sender's connection ended: "..tostring(reason)..(detail and (" ("..detail..")") or ""))
			pending.fail_all(odc.CommandStatus.DOWNSTREAM_FAIL)
		end,
	}
	if not UseTLS then return odc.TCPSocket(options) end
	options.tls =
	{
		CertFile = Conf.CertFile or "certs/receiver.crt",
		KeyFile = Conf.KeyFile or "certs/receiver.key",
		CAFile = Conf.CAFile or "certs/ca.crt",
		RequireClientCert = true,
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
	pending.fail_all(odc.CommandStatus.DOWNSTREAM_FAIL)
end

-- a control from the sink side: sent to the Sender, and its StatusCallback is called with the answer
function Event(EventInfo, SenderName, StatusCallback)
	if not wire.is_control(EventInfo.EventType) then
		StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
		return
	end
	if not connected then
		odc.log.warning("control not sent: no Sender is connected")
		StatusCallback(odc.CommandStatus.DOWNSTREAM_FAIL)
		return
	end
	local id = pending.add(StatusCallback)
	local msg, why = wire.control_msg(id, EventInfo)
	if not msg then
		odc.log.error("control not sent: "..tostring(why))
		pending.resolve(id, odc.CommandStatus.FORMAT_ERROR)
	elseif not write_line(msg) then
		pending.resolve(id, odc.CommandStatus.DOWNSTREAM_FAIL)
	end
end
