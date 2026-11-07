-- http_receiver.lua: the Receiver of the HTTP and HTTPS examples (see ../README.md).
-- A LuaPort next to the sink. It runs an HTTP(S) server: the Sender POSTs batches of events to /events, each event is
-- turned back into an ODC event and published, and the answer (200) says how many were taken. Its Event() receives the
-- controls the sink side issues, POSTs each to the Sender's /control and gives the sink the CommandStatus that comes
-- back in the response (TIMEOUT if the Sender does not answer in time, DOWNSTREAM_FAIL if it cannot be reached).
--
-- Settings (ConfOverrides), all optional:
--   Host                the address to listen on and to connect to (127.0.0.1)
--   LocalPort           this Receiver's server, for /events (20170)
--   PeerPort            the Sender's server, for /control (20171)
--   MaxBodyBytes        the largest batch accepted (1048576); at most MaxBatchEvents events are taken from one (1000)
--   ControlTimeoutMs    how long a control waits for its answer (5000)
--   Scheme              "http", or "https" for mutual TLS (needs CertFile, KeyFile, CAFile)
--   CertFile, KeyFile, CAFile   this side's certificate and key, and the CA that signed the Sender's (certs/receiver.crt, certs/receiver.key, certs/ca.crt)

package.path = odc.GetPath.ScriptDir("?.lua")..";"..package.path
local wire = require("odc_wire")

local Conf = odc.Config
local Host = Conf.Host or "127.0.0.1"
local LocalPort = Conf.LocalPort or 20170
local PeerPort = Conf.PeerPort or 20171
local MaxBodyBytes = Conf.MaxBodyBytes or 1048576
local MaxBatchEvents = Conf.MaxBatchEvents or 1000
local ControlTimeoutMs = math.floor(Conf.ControlTimeoutMs or 5000)
local Scheme = Conf.Scheme or "http"

local JSON = { ["Content-Type"] = "application/json" }
local server, client
local next_id = 0

local function tls_options()
	if Scheme ~= "https" then return nil end
	return
	{
		CertFile = Conf.CertFile or "certs/receiver.crt",
		KeyFile = Conf.KeyFile or "certs/receiver.key",
		CAFile = Conf.CAFile or "certs/ca.crt",
	}
end

-- POST /events: a batch of events to publish
local function handle_events(req, res)
	local msg, why = wire.decode(req.body)
	if not msg or msg.kind ~= "events" or type(msg.events) ~= "table" then
		odc.log.error("rejected a batch: "..tostring(why or "not an events message"))
		res:write(400, wire.encode({ v = wire.VERSION, error = "not an events message" }), JSON)
		return
	end
	if #msg.events > MaxBatchEvents then
		odc.log.error("rejected a batch of "..#msg.events.." events (the limit is "..MaxBatchEvents..")")
		res:write(400, wire.encode({ v = wire.VERSION, error = "too many events" }), JSON)
		return
	end
	local accepted, rejected = 0, 0
	for _, t in ipairs(msg.events) do
		local event, event_why = wire.decode_event(t)
		if event then
			accepted = accepted + 1
			odc.PublishEvent(event)
		else
			rejected = rejected + 1
			odc.log.error("rejected an event: "..tostring(event_why))
		end
	end
	res:write(200, wire.encode(wire.ack_msg(accepted, rejected)), JSON)
end

function Build() end

function Enable()
	local tls = tls_options()
	local s, err = odc.HTTPServer{ address = Host, port = LocalPort, tls = tls, max_request_bytes = MaxBodyBytes }
	if not s then
		odc.log.error("cannot make the server: "..tostring(err).." (for https: run make-certs first)")
		return
	end
	server = s
	assert(server:route("POST", "^/events$", handle_events))
	local ok, start_err, detail = server:start()
	if not ok then
		odc.log.error("cannot start the server on "..Host..":"..LocalPort..": "..tostring(start_err)..(detail and (" ("..detail..")") or ""))
		return
	end
	local c, client_err = odc.HTTPClient{ url = Scheme.."://"..Host..":"..PeerPort, tls = tls, timeout_ms = ControlTimeoutMs, max_response_bytes = 65536 }
	if not c then
		odc.log.error("cannot make the client: "..tostring(client_err).." (for https: run make-certs first)")
		return
	end
	client = c
	odc.log.info("listening on "..Scheme.."://"..Host..":"..LocalPort.."/events, sending controls to "..Scheme.."://"..Host..":"..PeerPort.."/control")
end

function Disable()
	if client then client:close() end
	if server then server:stop() end
	client, server = nil, nil
end

-- a control from the sink side: POSTed to the Sender, and its StatusCallback is called with the answer
function Event(EventInfo, SenderName, StatusCallback)
	if not wire.is_control(EventInfo.EventType) then
		StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
		return
	end
	if not client then
		StatusCallback(odc.CommandStatus.DOWNSTREAM_FAIL)
		return
	end
	next_id = next_id + 1
	local id = next_id
	local msg, why = wire.control_msg(id, EventInfo)
	if not msg then
		odc.log.error("control not sent: "..tostring(why))
		StatusCallback(odc.CommandStatus.FORMAT_ERROR)
		return
	end
	local ok, err = client:request
	{
		method = "POST", path = "/control", headers = JSON, body = wire.encode(msg),
		on_response = function(resp, error_string, detail)
			if error_string == "timeout" then
				odc.log.warning("control "..id.." was not answered within "..ControlTimeoutMs.." ms")
				StatusCallback(odc.CommandStatus.TIMEOUT)
			elseif error_string then
				odc.log.warning("control "..id.." could not be sent: "..error_string..(detail and (" ("..detail..")") or ""))
				StatusCallback(odc.CommandStatus.DOWNSTREAM_FAIL)
			elseif resp.status == 504 then
				StatusCallback(odc.CommandStatus.TIMEOUT)
			elseif resp.status ~= 200 then
				odc.log.warning("control "..id.." was refused: HTTP "..resp.status)
				StatusCallback(odc.CommandStatus.DOWNSTREAM_FAIL)
			else
				local answer, answer_why = wire.decode(resp.body)
				local status = answer and answer.kind == "status" and answer.id == id and wire.status_value(answer.status) or nil
				if status == nil then
					odc.log.error("control "..id..": unusable answer: "..tostring(answer_why or "not the status of this control"))
					StatusCallback(odc.CommandStatus.FORMAT_ERROR)
				else
					StatusCallback(status)
				end
			end
		end,
	}
	if not ok then
		odc.log.error("control not sent: "..tostring(err))
		StatusCallback(odc.CommandStatus.DOWNSTREAM_FAIL)
	end
end
