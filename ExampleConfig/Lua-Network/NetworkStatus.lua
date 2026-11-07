-- A LuaPort script that uses the network objects (see Code/Libs/LuaWrappers/README.md):
--  * odc.HTTPServer serves a status page and a JSON REST endpoint
--  * odc.HTTPClient polls an HTTP URL every few seconds and keeps the result
-- No keys, certificates or external hosts are needed: by default the script polls its own server.
-- To poll another HTTP server set PollURL, for example "http://192.0.2.10:8000" (a documentation address: it will not answer).
--
-- Settings (ConfOverrides in opendatacon.conf), all optional:
--   HttpPort        the port of this script's server, default 8080 (it listens on 127.0.0.1 only)
--   PollURL         what to poll, default this script's own server
--   PollPath        the path to request, default "/api/status"
--   PollPeriodSecs  seconds between polls, default 5

local Conf = odc.Config
local HttpPort = Conf.HttpPort or 8080
local PollURL = Conf.PollURL or ("http://127.0.0.1:"..HttpPort)
local PollPath = Conf.PollPath or "/api/status"
local PollPeriodMs = math.floor((Conf.PollPeriodSecs or 5) * 1000)
local JSON = { ["Content-Type"] = "application/json" }

local Enabled = false
local Server, Client, CancelTimer
local StartedMs = 0
local Stats = { polls = 0, failures = 0, last_status = nil, last_error = nil, last_poll = nil }

local function StatusTable()
	return {
		name = odc.Name,
		uptime_s = (odc.msSinceEpoch() - StartedMs) // 1000,
		polling = PollURL..PollPath,
		polls = Stats.polls,
		failures = Stats.failures,
		last_poll = Stats.last_poll,
		last_status = Stats.last_status,
		last_error = Stats.last_error,
	}
end

local SchedulePoll

local function Poll()
	if not Enabled then return end
	-- the callback is called once, later, with the response or with an error string (never from inside request())
	local ok, err = Client:request{
		method = "GET",
		path = PollPath,
		headers = { Accept = "application/json" },
		on_response = function(resp, error_string, detail)
			Stats.polls = Stats.polls + 1
			Stats.last_poll = odc.msSinceEpochToDateTime(odc.msSinceEpoch())
			if error_string then
				Stats.failures = Stats.failures + 1
				Stats.last_status = nil
				Stats.last_error = error_string
				odc.log.warning("poll of "..PollURL..PollPath.." failed: "..error_string..(detail and (" ("..detail..")") or ""))
			else
				Stats.last_status = resp.status
				Stats.last_error = nil
				odc.log.info("poll of "..PollURL..PollPath.." answered "..resp.status.." "..resp.reason.." ("..#resp.body.." bytes)")
			end
			SchedulePoll()
		end,
	}
	if not ok then
		Stats.failures = Stats.failures + 1
		Stats.last_error = err
		odc.log.error("could not make the poll request: "..tostring(err))
		SchedulePoll()
	end
end

SchedulePoll = function()
	if not Enabled then return end
	CancelTimer = odc.msTimerCallback(PollPeriodMs, function() Poll() end)
end

--opendatacon mandates a Build() function
function Build() end

--opendatacon mandates an Enable() function
function Enable()
	Enabled = true
	StartedMs = odc.msSinceEpoch()
	Stats = { polls = 0, failures = 0 }

	Server = assert(odc.HTTPServer{ port = HttpPort })
	-- a status page
	assert(Server:route("GET", "^/$", function(req, res)
		local s = StatusTable()
		res:write(200, "<html><body><h1>"..s.name.."</h1><p>up "..s.uptime_s.." s, polls: "..s.polls..", failures: "..s.failures
			.."</p><p>see <a href=\"/api/status\">/api/status</a></p></body></html>", { ["Content-Type"] = "text/html; charset=utf-8" })
	end))
	-- a REST endpoint: the same as JSON
	assert(Server:route("GET", "^/api/status$", function(req, res)
		res:write(200, odc.EncodeJSON(StatusTable()), JSON)
	end))
	assert(Server:default_route(function(req, res)
		res:write(404, odc.EncodeJSON({ error = "no such page", path = req.path }), JSON)
	end))
	assert(Server:start())
	odc.log.info("status page on http://127.0.0.1:"..Server:port().."/")

	Client = assert(odc.HTTPClient{ url = PollURL, timeout_ms = 3000 })
	odc.PublishEvent({ EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_UP })
	-- the first poll shortly after enabling
	CancelTimer = odc.msTimerCallback(500, function() Poll() end)
end

--opendatacon mandates a Disable() function
function Disable()
	Enabled = false
	if CancelTimer then CancelTimer() end
	-- close what Enable() opened and stop what it started: the port does not do it
	if Client then Client:close() end
	if Server then Server:stop() end
	Client, Server = nil, nil
	odc.PublishEvent({ EventType = odc.EventType.ConnectState, Payload = odc.ConnectState.PORT_DOWN })
end

--opendatacon mandates an Event() function so you can receive events
function Event(EventInfo, SenderName, StatusCallback)
	StatusCallback(odc.CommandStatus.NOT_SUPPORTED)
end
