-- http_sender.lua: the Sender of the HTTP and HTTPS examples (see ../README.md).
-- A LuaPort next to the source of the events. Its Event() receives the events the source publishes and queues them; a batch
-- of them (every BatchMs, or as soon as BatchMax are queued) is POSTed as a JSON document to the Receiver's /events.
-- It also runs an HTTP(S) server of its own: the Receiver POSTs the controls of the sink side to /control, the Sender
-- publishes each to the source and answers with the CommandStatus the source gives.
--
-- Settings (ConfOverrides), all optional:
--   Host                the address to listen on and to connect to (127.0.0.1)
--   LocalPort           this Sender's server, for /control (20171)
--   PeerPort            the Receiver's server, for /events (20170)
--   BatchMs, BatchMax   how often a batch is sent (1000), and how many events it holds at most (50)
--   QueueLimit          events kept while the Receiver cannot be reached; the oldest is dropped beyond it (1000)
--   MaxBodyBytes        the largest request body this Sender's server accepts (65536)
--   ControlTimeoutMs    how long the source has to answer a control (5000)
--   Scheme              "http", or "https" for mutual TLS (needs CertFile, KeyFile, CAFile)
--   CertFile, KeyFile, CAFile   this side's certificate and key, and the CA that signed the Receiver's (certs/sender.crt, certs/sender.key, certs/ca.crt)

package.path = odc.GetPath.ScriptDir("?.lua")..";"..package.path
local wire = require("odc_wire")

local Conf = odc.Config
local Host = Conf.Host or "127.0.0.1"
local LocalPort = Conf.LocalPort or 20171
local PeerPort = Conf.PeerPort or 20170
local BatchMs = math.floor(Conf.BatchMs or 1000)
local BatchMax = Conf.BatchMax or 50
local QueueLimit = Conf.QueueLimit or 1000
local MaxBodyBytes = Conf.MaxBodyBytes or 65536
local ControlTimeoutMs = math.floor(Conf.ControlTimeoutMs or 5000)
local Scheme = Conf.Scheme or "http"

local JSON = { ["Content-Type"] = "application/json" }
local server, client
local enabled, in_flight, outage_logged = false, false, false
local queue = wire.event_queue(QueueLimit)

local function tls_options()
	if Scheme ~= "https" then return nil end
	return
	{
		CertFile = Conf.CertFile or "certs/sender.crt",
		KeyFile = Conf.KeyFile or "certs/sender.key",
		CAFile = Conf.CAFile or "certs/ca.crt",
	}
end

local send_batch

-- sends the next batch if none is on its way (the batches go one at a time, so their order is kept)
local function pump()
	if enabled and not in_flight and queue.size() > 0 then send_batch() end
end

send_batch = function()
	local batch = queue.take(BatchMax)
	local msg = wire.events_msg(batch)
	local body = wire.encode(msg)
	in_flight = true
	local ok, err = client:request
	{
		method = "POST", path = "/events", headers = JSON, body = body,
		on_response = function(resp, error_string, detail)
			in_flight = false
			if error_string then
				queue.put_back(batch)
				if not outage_logged then
					outage_logged = true
					odc.log.warning("cannot reach the Receiver: "..error_string..(detail and (" ("..detail..")") or "").."; keeping "..queue.size().." events and trying again")
				end
				return
			end
			outage_logged = false
			if resp.status == 200 then
				odc.log.debug("batch of "..#batch.." events delivered")
				pump()
			elseif resp.status >= 400 and resp.status < 500 then
				-- the Receiver will not take it, however often it is sent: drop it
				odc.log.error("the Receiver refused a batch of "..#batch.." events (HTTP "..resp.status.."): dropped")
				pump()
			else
				queue.put_back(batch)
				odc.log.warning("the Receiver answered HTTP "..resp.status.."; keeping "..queue.size().." events and trying again")
			end
		end,
	}
	if not ok then
		in_flight = false
		queue.put_back(batch)
		odc.log.error("cannot make the request: "..tostring(err))
	end
end

local function tick()
	if not enabled then return end
	pump()
	odc.msTimerCallback(BatchMs, tick)
end

-- POST /control: a control from the sink side
local function handle_control(req, res)
	local msg, why = wire.decode(req.body)
	if not msg or msg.kind ~= "control" or math.type(msg.id) ~= "integer" then
		odc.log.error("rejected a control request: "..tostring(why or "not a control with an id"))
		res:write(400, wire.encode({ v = wire.VERSION, error = "not a control message" }), JSON)
		return
	end
	local event, event_why = wire.decode_event(msg.event)
	if not event or not wire.is_control(event.EventType) then
		odc.log.error("rejected control "..msg.id..": "..tostring(event_why or "not a control type"))
		res:write(200, wire.encode(wire.status_msg(msg.id, odc.CommandStatus.FORMAT_ERROR)), JSON)
		return
	end
	odc.log.info("control "..msg.id..": "..odc.ToString.EventType(event.EventType).." index "..event.Index.." published to the source")
	-- answered when the source answers (the server answers 504 if that takes longer than the Receiver waits)
	odc.PublishEvent(event, function(status)
		res:write(200, wire.encode(wire.status_msg(msg.id, status)), JSON)
	end)
end

function Build() end

function Enable()
	enabled, in_flight, outage_logged = true, false, false
	local tls = tls_options()
	local s, err = odc.HTTPServer{ address = Host, port = LocalPort, tls = tls, max_request_bytes = MaxBodyBytes, response_timeout_ms = ControlTimeoutMs + 2000 }
	if not s then
		odc.log.error("cannot make the server: "..tostring(err).." (for https: run make-certs first)")
		return
	end
	server = s
	assert(server:route("POST", "^/control$", handle_control))
	local ok, start_err, detail = server:start()
	if not ok then
		odc.log.error("cannot start the server on "..Host..":"..LocalPort..": "..tostring(start_err)..(detail and (" ("..detail..")") or ""))
		return
	end
	local c, client_err = odc.HTTPClient{ url = Scheme.."://"..Host..":"..PeerPort, tls = tls, timeout_ms = 10000, max_response_bytes = 65536 }
	if not c then
		odc.log.error("cannot make the client: "..tostring(client_err).." (for https: run make-certs first)")
		return
	end
	client = c
	odc.log.info("sending to "..Scheme.."://"..Host..":"..PeerPort.."/events, listening on "..Scheme.."://"..Host..":"..LocalPort.."/control")
	odc.msTimerCallback(BatchMs, tick)
end

function Disable()
	enabled = false
	if client then client:close() end
	if server then server:stop() end
	client, server = nil, nil
end

-- an event from the source: queued for the next batch
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
	if client and queue.size() >= BatchMax then pump() end
	StatusCallback(odc.CommandStatus.SUCCESS)
end
