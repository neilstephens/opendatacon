/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * NetHttpServerTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Tests of odc.HTTPServer: real Lua scripts in a LuaPort, a raw socket client in the test (nothing here depends on odc.HTTPClient), and with
 *  Simple-Web-Server built with TLS, an asio TLS client and certificates from the test PKI. TCP ports 20800 to 20899.
 */
#include "NetTestHelpers.h"
#include <future>
#include <map>
#include <sstream>

#ifdef ODC_HTTPS_TESTS
#include "TlsTestPki.h"
#include <client_https.hpp> //the SWS HTTPS client brings in asio::ssl (a direct asio include is forbidden outside libODC); opendatacon/asio.h is included first by NetTestHelpers.h
#endif

#define SUITE(name) "LuaNetHttpServerTestSuite - " name

using namespace nettest;

namespace
{
struct HttpReply
{
	bool ok = false; //a complete response was read
	int status = 0;
	std::map<std::string,std::string> headers; //lower-case names
	std::string body;
	std::chrono::milliseconds elapsed{0};
};

std::string LowerCase(std::string s)
{
	std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return s;
}

//Parses what was read: the status line, the headers, and the body (to the end of the data: the request said Connection: close)
HttpReply ParseReply(const std::string& raw)
{
	HttpReply r;
	const auto split = raw.find("\r\n\r\n");
	if(split == std::string::npos || raw.rfind("HTTP/1.",0) != 0)
		return r;
	std::istringstream head(raw.substr(0,split));
	std::string line;
	std::getline(head,line);
	r.status = std::atoi(line.substr(line.find(' ')+1).c_str());
	while(std::getline(head,line))
	{
		if(!line.empty() && line.back() == '\r')
			line.pop_back();
		const auto colon = line.find(':');
		if(colon != std::string::npos)
		{
			auto value = line.substr(colon+1);
			value.erase(0,value.find_first_not_of(' '));
			r.headers[LowerCase(line.substr(0,colon))] = value;
		}
	}
	r.body = raw.substr(split+4);
	r.ok = true;
	return r;
}

std::string BuildRequest(const std::string& method, const std::string& target, const std::string& body, const std::vector<std::pair<std::string,std::string>>& headers, bool close, bool with_length = true, const std::string& length_override = "")
{
	std::string req = method+" "+target+" HTTP/1.1\r\nHost: localhost\r\n";
	if(close)
		req += "Connection: close\r\n";
	for(const auto& h : headers)
		req += h.first+": "+h.second+"\r\n";
	if(with_length && (!body.empty() || method == "POST" || method == "PUT"))
		req += "Content-Length: "+(length_override.empty() ? std::to_string(body.size()) : length_override)+"\r\n";
	return req+"\r\n"+body;
}

//One request on its own connection, read to the end of the connection
template<typename Socket>
HttpReply ExchangeOn(asio::io_context& io, Socket& sock, const std::string& request, std::chrono::milliseconds timeout)
{
	const auto start = std::chrono::steady_clock::now();
	HttpReply r;
	std::string raw;
	bool done = false;
	asio::async_write(sock,asio::buffer(request),[&](const std::error_code& wec, size_t)
		{
			if(wec)
			{
				done = true;
				return;
			}
			asio::async_read(sock,asio::dynamic_buffer(raw),[&](const std::error_code&, size_t)
				{
					done = true;
				});
		});
	io.restart();
	io.run_for(timeout);
	if(done)
		r = ParseReply(raw);
	r.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
	return r;
}

HttpReply Http(uint16_t port, const std::string& method, const std::string& target, const std::string& body = "",
	const std::vector<std::pair<std::string,std::string>>& headers = {}, std::chrono::milliseconds timeout = 15000ms, const std::string& length_override = "")
{
	asio::io_context io;
	asio::ip::tcp::socket sock(io);
	if(!ConnectTo(io,sock,port,10000ms))
		return HttpReply();
	auto r = ExchangeOn(io,sock,BuildRequest(method,target,body,headers,true,true,length_override),timeout);
	std::error_code ec;
	sock.close(ec);
	return r;
}

//the response to one request read from a connection that stays open (by Content-Length)
bool ReadOne(asio::ip::tcp::socket& sock, std::string& pending, HttpReply& out)
{
	std::error_code ec;
	while(pending.find("\r\n\r\n") == std::string::npos)
	{
		char buf[4096];
		const auto n = sock.read_some(asio::buffer(buf),ec);
		if(ec)
			return false;
		pending.append(buf,n);
	}
	const auto split = pending.find("\r\n\r\n");
	auto head = ParseReply(pending.substr(0,split+4));
	const size_t length = head.headers.count("content-length") ? std::stoul(head.headers["content-length"]) : 0;
	while(pending.size() < split+4+length)
	{
		char buf[4096];
		const auto n = sock.read_some(asio::buffer(buf),ec);
		if(ec)
			return false;
		pending.append(buf,n);
	}
	out = head;
	out.body = pending.substr(split+4,length);
	pending.erase(0,split+4+length);
	return true;
}

//Starts a script that makes a server on the port in odc.Config.port and reports "started"
Json::Value PortConf(uint16_t port)
{
	Json::Value conf;
	conf["port"] = port;
	return conf;
}
}

TEST_CASE(SUITE("Routes with captures and query parsing and the default route"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/api/state$", function(req, res)
		res:write(200, odc.EncodeJSON({ ok = true, n = 3 }), { ["Content-Type"] = "application/json" })
	end))
	assert(srv:route("POST", "^/api/items/(\\d+)$", function(req, res)
		res:write(201, "item "..req.captures[1].." got "..req.body.." len "..#req.body, { ["X-Item"] = req.captures[1] })
	end))
	assert(srv:route("GET", "^/q$", function(req, res)
		local q = req.query
		res:write(200, table.concat({ tostring(q.a), tostring(q.b), tostring(q.c), #req.query_all.a, req.query_all.a[1], req.query_all.a[2],
			req.query_string, tostring(req.headers["x-test"]), req.method, req.http_version, req.remote_addr, type(req.remote_port), req.path }, "|"))
	end))
	assert(srv:route("GET", "^/captures/([a-z]+)-(\\d*)$", function(req, res)
		res:write(200, #req.captures..":"..req.captures[1]..":"..req.captures[2])
	end))
	assert(srv:route("GET", "^/dup$", function(req, res) res:write(200, req.headers["x-multi"]) end))
	assert(srv:default_route(function(req, res) res:write(404, "no route for "..req.method.." "..req.path) end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20800));
	h.Enable();
	CHECK(h.Expect("started") == "20800");

	auto r = Http(20800,"GET","/api/state");
	REQUIRE(r.ok);
	CHECK(r.status == 200);
	CHECK(r.headers["content-type"] == "application/json");
	CHECK(r.body == "{\"n\":3,\"ok\":true}");

	r = Http(20800,"POST","/api/items/42","hello");
	CHECK(r.status == 201);
	CHECK(r.body == "item 42 got hello len 5");
	CHECK(r.headers["x-item"] == "42");

	r = Http(20800,"GET","/q?a=1&b=x%20y&a=2&c","",{
		{"X-Test","abc"}
	});
	CHECK(r.status == 200);
	CHECK(r.body == "2|x y||2|1|2|a=1&b=x%20y&a=2&c|abc|GET|1.1|127.0.0.1|number|/q");

	r = Http(20800,"GET","/captures/abc-7");
	CHECK(r.body == "2:abc:7");
	r = Http(20800,"GET","/captures/abc-");
	CHECK(r.body == "2:abc:");

	//a repeated header is joined
	r = Http(20800,"GET","/dup","",{
		{"X-Multi","one"},{"X-Multi","two"}
	});
	CHECK(r.status == 200);
	CHECK(r.body.find("one") != std::string::npos);
	CHECK(r.body.find("two") != std::string::npos);

	r = Http(20800,"GET","/nothing");
	CHECK(r.status == 404);
	CHECK(r.body == "no route for GET /nothing");
	r = Http(20800,"PUT","/api/state","x");
	CHECK(r.status == 404); //the route is GET only
	r = Http(20800,"GET","/p/hello%20world%2Fx");
	CHECK(r.body == "no route for GET /p/hello world/x"); //the path is percent-decoded
	r = Http(20800,"GET","/api/state/");
	CHECK(r.status == 404); //the pattern is anchored by SWS
}

TEST_CASE(SUITE("Without a default route an unmatched request is answered 404"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/only$", function(req, res) res:write(200, "ok") end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20801));
	h.Enable();
	h.Expect("started");
	CHECK(Http(20801,"GET","/only").status == 200);
	auto r = Http(20801,"GET","/other");
	CHECK(r.status == 404);
	CHECK(r.body == "Not Found");
}

TEST_CASE(SUITE("A server on port 0 picks a port and reports it"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{})
	report("before", srv:port())
	assert(srv:route("GET", "^/$", function(req, res) res:write(200, "picked") end))
	assert(srv:start())
	report("after", srv:port())
end
)lua");
	h.Enable();
	CHECK(h.Expect("before") == "0");
	const auto port = static_cast<uint16_t>(std::stoi(h.Expect("after")));
	CHECK(port != 0);
	CHECK(Http(port,"GET","/").body == "picked");
}

TEST_CASE(SUITE("An answer after a timer and the 504 backstop for a request that is not answered"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port, response_timeout_ms = 400 })
	assert(srv:route("GET", "^/slow$", function(req, res)
		odc.msTimerCallback(150, function() report("slow_write", tostring(res:write(200, "late answer"))) end)
	end))
	assert(srv:route("GET", "^/never$", function(req, res) held = res end))
	assert(srv:route("GET", "^/toolate$", function(req, res)
		odc.msTimerCallback(1200, function()
			local ok, err = res:write(200, "too late")
			report("toolate", tostring(ok)..","..tostring(err))
		end)
	end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20802));
	h.Enable();
	h.Expect("started");
	auto r = Http(20802,"GET","/slow");
	CHECK(r.status == 200);
	CHECK(r.body == "late answer");
	CHECK(r.elapsed >= 100ms);
	CHECK(h.Expect("slow_write") == "true");

	r = Http(20802,"GET","/never");
	CHECK(r.status == 504);
	CHECK(r.body == "Gateway Timeout");
	CHECK(r.elapsed >= 350ms);
	CHECK(r.elapsed < 5000ms);
	CHECK(h.capture->Has("PortUnderTest: HTTP GET /never was not answered within 400 ms: answering 504."));

	r = Http(20802,"GET","/toolate");
	CHECK(r.status == 504);
	CHECK(h.Expect("toolate") == "nil,already_written"); //the script's answer arrived after the backstop's
}

TEST_CASE(SUITE("Many connections at once"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/n/(\\d+)$", function(req, res)
		local n = req.captures[1]
		odc.msTimerCallback(300, function() res:write(200, "n="..n) end)
	end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20803));
	h.Enable();
	h.Expect("started");
	const int N = 50;
	std::vector<std::future<HttpReply>> replies;
	const auto start = std::chrono::steady_clock::now();
	for(int i = 0; i < N; i++)
		replies.push_back(std::async(std::launch::async,[i](){ return Http(20803,"GET","/n/"+std::to_string(i)); }));
	int good = 0;
	for(int i = 0; i < N; i++)
	{
		auto r = replies[i].get();
		if(r.status == 200 && r.body == "n="+std::to_string(i))
			++good;
	}
	CHECK(good == N);
	//answered together, not one after another (50 * 300 ms)
	CHECK(std::chrono::steady_clock::now()-start < 10s);
}

TEST_CASE(SUITE("Connections stay open for more requests and close_after ends one"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/a$", function(req, res) res:write(200, "first") end))
	assert(srv:route("GET", "^/b$", function(req, res) res:write(200, "second") end))
	assert(srv:route("GET", "^/bye$", function(req, res) res:close_after(); res:write(200, "closing") end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20804));
	h.Enable();
	h.Expect("started");
	asio::io_context io;
	asio::ip::tcp::socket sock(io);
	REQUIRE(ConnectTo(io,sock,20804));
	std::string pending;
	HttpReply a, b, c;
	asio::write(sock,asio::buffer(BuildRequest("GET","/a","",{},false)));
	REQUIRE(ReadOne(sock,pending,a));
	asio::write(sock,asio::buffer(BuildRequest("GET","/b","",{},false)));
	REQUIRE(ReadOne(sock,pending,b));
	CHECK(a.body == "first");
	CHECK(b.body == "second");
	asio::write(sock,asio::buffer(BuildRequest("GET","/bye","",{},false)));
	REQUIRE(ReadOne(sock,pending,c));
	CHECK(c.body == "closing");
	CHECK(c.headers["connection"] == "close");
	//and the server closed the connection
	char ch;
	std::error_code ec;
	sock.read_some(asio::buffer(&ch,1),ec);
	CHECK(ec == asio::error::eof);
}

TEST_CASE(SUITE("A request over max_request_bytes is refused and not given to the script"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port, max_request_bytes = 1000 })
	assert(srv:route("POST", "^/up$", function(req, res) report("handled", #req.body); res:write(200, "got "..#req.body) end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20805));
	h.Enable();
	h.Expect("started");
	auto r = Http(20805,"POST","/up",std::string(500,'x'));
	CHECK(r.status == 200);
	CHECK(r.body == "got 500");
	CHECK(h.Expect("handled") == "500");
	//a declared length over the limit is refused before the body is read (Simple-Web-Server means to answer 413, but it never sends it: the connection is closed)
	r = Http(20805,"POST","/up",std::string(10,'x'),{},15000ms,"5000");
	CHECK((r.status == 413 || !r.ok));
	//and so is a body that is over it (if the connection lasts long enough to say so)
	r = Http(20805,"POST","/up",std::string(5000,'x'));
	CHECK(r.status != 200);
	CHECK(h.NoReport("handled",300ms));
}

TEST_CASE(SUITE("start() reports address_in_use and bind_failed"))
{
	Peer occupant(20806,[](Peer::Socket&, int){});
	Json::Value conf = PortConf(20807);
	conf["taken"] = 20806;
	LuaHarness h(R"lua(
function Enable()
	local first = assert(odc.HTTPServer{ port = odc.Config.port })
	report("first", tostring(first:start()))
	local second = assert(odc.HTTPServer{ port = odc.Config.port })
	local ok, err, detail = second:start()
	report("second", tostring(ok)..","..tostring(err)..","..tostring(detail ~= nil))
	local third = assert(odc.HTTPServer{ port = odc.Config.taken })
	local ok3, err3 = third:start()
	report("occupied", tostring(ok3)..","..tostring(err3))
	local fourth = assert(odc.HTTPServer{ address = "192.0.2.55", port = odc.Config.port + 1 })
	local ok4, err4 = fourth:start()
	report("address", tostring(ok4)..","..tostring(err4))
	--the second can start once the first has stopped
	first:stop()
	report("after_stop", tostring(second:start()))
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("first") == "true");
	CHECK(h.Expect("second") == "nil,address_in_use,true");
	CHECK(h.Expect("occupied") == "nil,address_in_use");
	CHECK(h.Expect("address") == "nil,bind_failed");
	CHECK(h.Expect("after_stop") == "true");
}

TEST_CASE(SUITE("stop() with a request in flight then a late answer and a restart"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/hang$", function(req, res) held = res; report("hanging", "") end))
	assert(srv:route("GET", "^/ok$", function(req, res) res:write(200, "ok") end))
	assert(srv:start())
	report("started", srv:port())
end
function Event(e, sender, cb)
	if e.Payload == "stop" then
		report("stop", tostring(srv:stop())..","..tostring(srv:stop()))
	elseif e.Payload == "late" then
		local ok, err = held:write(200, "after the stop")
		report("late", tostring(ok)..","..tostring(err))
	elseif e.Payload == "restart" then
		report("restart", tostring(srv:start())..","..srv:port())
	end
end
)lua",PortConf(20808));
	h.Enable();
	h.Expect("started");
	auto pending = std::async(std::launch::async,[](){ return Http(20808,"GET","/hang","",{},10000ms); });
	h.Expect("hanging");
	h.Send("stop");
	CHECK(h.Expect("stop") == "true,true"); //stopping twice is fine
	auto r = pending.get();
	CHECK_FALSE(r.ok); //the connection was closed with no answer
	CHECK(r.elapsed < 5000ms);
	h.Send("late");
	CHECK(h.Expect("late") == "nil,closed");
	//nobody listens now
	CHECK_FALSE(Http(20808,"GET","/ok","",{},2000ms).ok);
	h.Send("restart");
	CHECK(h.Expect("restart") == "true,20808");
	CHECK(Http(20808,"GET","/ok").body == "ok");
}

TEST_CASE(SUITE("An error in a handler answers 500 and the other routes carry on"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/boom$", function(req, res) error("handler boom") end))
	assert(srv:route("GET", "^/object$", function(req, res) error(setmetatable({}, {})) end))
	assert(srv:route("GET", "^/after$", function(req, res) res:write(200, "written"); error("after boom") end))
	assert(srv:route("GET", "^/ok$", function(req, res) res:write(200, "ok") end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20809));
	h.Enable();
	h.Expect("started");
	auto r = Http(20809,"GET","/boom");
	CHECK(r.status == 500);
	CHECK(r.body == "Internal Server Error"); //(the error text stays in the log)
	CHECK(Http(20809,"GET","/object").status == 500);
	r = Http(20809,"GET","/after");
	CHECK(r.status == 200);
	CHECK(r.body == "written");
	CHECK(Http(20809,"GET","/ok").body == "ok");
	REQUIRE(WaitFor([&](){ return h.capture->Has("handler boom") && h.capture->Has("after boom"); }));
	CHECK(h.capture->Has("PortUnderTest: Error in HTTPServer 127.0.0.1:20809 handler for GET /boom:"));
	CHECK(h.capture->Has("(error object is not a string)"));
}

TEST_CASE(SUITE("res:write once with known status codes and safe headers"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/twice$", function(req, res)
		local a = res:write(200, "first")
		local b, err = res:write(200, "second")
		report("twice", tostring(a)..","..tostring(b)..","..tostring(err))
	end))
	assert(srv:route("GET", "^/status$", function(req, res)
		local a, ea = res:write(299, "x")
		local b, eb = res:write(0, "x")
		local c, ec = res:write("200", "x")
		local d, ed = res:write(nil, "x")
		report("status", tostring(a)..","..tostring(ea).."|"..tostring(b)..","..tostring(eb).."|"..tostring(c)..","..tostring(ec).."|"..tostring(d)..","..tostring(ed))
		res:write(418, "teapot")
	end))
	assert(srv:route("GET", "^/headers$", function(req, res)
		local r = {}
		local function try(name, ...) local ok, err = res:write(...); r[#r+1] = name.."="..tostring(ok)..","..tostring(err) end
		try("inject_value", 200, "x", { ["X-Bad"] = "a\r\nInjected: 1" })
		try("inject_name", 200, "x", { ["X-Bad\r\nInjected"] = "1" })
		try("content_length", 200, "x", { ["Content-Length"] = "3" })
		try("transfer_encoding", 200, "x", { ["Transfer-Encoding"] = "chunked" })
		try("value_type", 200, "x", { ["X-Bad"] = {} })
		try("headers_type", 200, "x", "no")
		try("body_type", 200, 5)
		report("headers", table.concat(r, "|"))
		res:write(200, "x", { ["X-Text"] = "one", ["X-Number"] = 5 })
	end))
	assert(srv:route("GET", "^/empty$", function(req, res) res:write(204) end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20810));
	h.Enable();
	h.Expect("started");
	auto r = Http(20810,"GET","/twice");
	CHECK(r.body == "first");
	CHECK(h.Expect("twice") == "true,nil,already_written");

	r = Http(20810,"GET","/status");
	CHECK(r.status == 418);
	CHECK(r.body == "teapot");
	CHECK(h.Expect("status") == "nil,invalid_options|nil,invalid_options|nil,invalid_options|nil,invalid_options");

	r = Http(20810,"GET","/headers");
	CHECK(r.status == 200);
	CHECK(r.headers["x-text"] == "one");
	CHECK(r.headers["x-number"] == "5");
	CHECK(r.headers.count("injected") == 0);
	CHECK(h.Expect("headers") == "inject_value=nil,invalid_options|inject_name=nil,invalid_options|content_length=nil,invalid_options|"
		"transfer_encoding=nil,invalid_options|value_type=nil,invalid_options|headers_type=nil,invalid_options|body_type=nil,invalid_options");

	r = Http(20810,"GET","/empty");
	CHECK(r.status == 204);
	CHECK(r.body == "");
}

TEST_CASE(SUITE("Options and the methods in the wrong state"))
{
	LuaHarness h(R"lua(
local function show(a, b)
	local s = (type(a) == "boolean" or a == nil) and tostring(a) or "object"
	return s..":"..tostring(b)
end
function Enable()
	local cases =
	{
		{ "none",         function() return odc.HTTPServer() end },
		{ "empty",        function() return odc.HTTPServer{} end },
		{ "typo",         function() return odc.HTTPServer{ prot = 80 } end },
		{ "host_name",    function() return odc.HTTPServer{ address = "localhost" } end },
		{ "address_num",  function() return odc.HTTPServer{ address = 5 } end },
		{ "ipv6",         function() return odc.HTTPServer{ address = "::1" } end },
		{ "port_big",     function() return odc.HTTPServer{ port = 70000 } end },
		{ "port_text",    function() return odc.HTTPServer{ port = "80" } end },
		{ "request_neg",  function() return odc.HTTPServer{ request_timeout_ms = -1 } end },
		{ "content_text", function() return odc.HTTPServer{ content_timeout_ms = "1" } end },
		{ "response_f",   function() return odc.HTTPServer{ response_timeout_ms = 1.5 } end },
		{ "max_zero",     function() return odc.HTTPServer{ max_request_bytes = 0 } end },
		{ "unbounded",    function() return odc.HTTPServer{ request_timeout_ms = 0, content_timeout_ms = 0, response_timeout_ms = 0 } end },
		{ "full",         function() return odc.HTTPServer{ address = "127.0.0.1", port = 0, request_timeout_ms = 2000, content_timeout_ms = 3000,
			max_request_bytes = 4096, response_timeout_ms = 5000 } end },
	}
	local out = {}
	for _, c in ipairs(cases) do out[#out+1] = c[1].."="..show(c[2]()) end
	report("options", table.concat(out, "|"))

	local s = assert(odc.HTTPServer{ port = odc.Config.port })
	local r = {}
	local function add(name, ...) r[#r+1] = name.."="..show(...) end
	add("method_lower", s:route("get", "^/$", function() end))
	add("method_bad", s:route("GE T", "^/$", function() end))
	add("method_type", s:route(5, "^/$", function() end))
	add("pattern_empty", s:route("GET", "", function() end))
	local ok, err = s:route("GET", "(", function() end)
	r[#r+1] = "pattern_regex="..tostring(ok)..","..tostring(err):sub(1, 32)
	add("handler_type", s:route("GET", "^/$", "x"))
	add("default_type", s:default_route(5))
	add("route", s:route("GET", "^/$", function(req, res) res:write(200, "again") end))
	add("route_replace", s:route("GET", "^/$", function(req, res) res:write(200, "replaced") end))
	add("start", s:start())
	add("start_again", s:start())
	add("route_started", s:route("GET", "^/x$", function() end))
	add("default_started", s:default_route(function() end))
	add("port", s:port() == odc.Config.port)
	add("stop", s:stop())
	add("stop_again", s:stop())
	add("route_stopped", s:route("GET", "^/x$", function() end))
	report("methods", table.concat(r, "|"))
end
)lua",PortConf(20811));
	h.Enable();
	const std::vector<std::string> options =
	{
		"none=nil:invalid_options: options",
		"empty=object:nil",
		"typo=nil:invalid_options: prot",
		"host_name=nil:invalid_options: address",
		"address_num=nil:invalid_options: address",
		"ipv6=object:nil",
		"port_big=nil:invalid_options: port",
		"port_text=nil:invalid_options: port",
		"request_neg=nil:invalid_options: request_timeout_ms",
		"content_text=nil:invalid_options: content_timeout_ms",
		"response_f=nil:invalid_options: response_timeout_ms",
		"max_zero=nil:invalid_options: max_request_bytes",
		"unbounded=object:nil",
		"full=object:nil"
	};
	std::string expected;
	for(const auto& o : options)
		expected += (expected.empty() ? "" : "|")+o;
	CHECK(h.Expect("options") == expected);   const auto methods = h.Expect("methods");
	INFO(methods);
	CHECK(methods.find("method_lower=nil:invalid_options: method|") != std::string::npos);
	CHECK(methods.find("method_bad=nil:invalid_options: method|") != std::string::npos);
	CHECK(methods.find("method_type=nil:invalid_options: method|") != std::string::npos);
	CHECK(methods.find("pattern_empty=nil:invalid_options: pattern|") != std::string::npos);
	CHECK(methods.find("pattern_regex=nil,invalid_options: pattern (") != std::string::npos);
	CHECK(methods.find("handler_type=nil:invalid_options: handler|") != std::string::npos);
	CHECK(methods.find("default_type=nil:invalid_options: handler|") != std::string::npos);
	CHECK(methods.find("route=true:nil|route_replace=true:nil|start=true:nil|start_again=true:nil|") != std::string::npos);
	CHECK(methods.find("route_started=nil:already_started|default_started=nil:already_started|port=true:nil|stop=true:nil|stop_again=true:nil|") != std::string::npos);
	CHECK(methods.find("route_stopped=true:nil") != std::string::npos);
}

TEST_CASE(SUITE("A script that stops its server in Disable(): no handler afterwards, port released, nothing leaked"))
{
	Json::Value conf = PortConf(20812);
	LuaHarness h(R"lua(
local disabled = false
function Enable()
	disabled = false
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/hang$", function(req, res)
		if disabled then report("late", "handler") end
		held = res
		report("hanging", "")
	end))
	assert(srv:route("GET", "^/ok$", function(req, res) if disabled then report("late", "handler") end; res:write(200, "ok") end))
	assert(srv:start())
	report("started", srv:port())
end
function Disable()
	disabled = true
	srv:stop()
	srv = nil --the script lets go of it: it is collected once stopped
	report("disabled", "")
end
)lua",conf);
	h.Enable();
	h.Expect("started");
	CHECK(Http(20812,"GET","/ok").status == 200);
	auto pending = std::async(std::launch::async,[](){ return Http(20812,"GET","/hang","",{},10000ms); });
	h.Expect("hanging");
	h.Disable();
	h.Expect("disabled");
	auto r = pending.get();
	CHECK_FALSE(r.ok);
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	//the port is free again, and nothing answers
	CHECK(WaitFor([&]()
		{
			asio::io_context io;
			asio::ip::tcp::acceptor a(io);
			std::error_code e;
			a.open(asio::ip::tcp::v4(),e);
			a.set_option(asio::socket_base::reuse_address(ReuseToCheckPortFree),e);
			a.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),20812),e);
			return !e;
		}));
	CHECK_FALSE(Http(20812,"GET","/ok","",{},1000ms).ok); //nothing is listening to be given a request
	CHECK(h.NoReport("late",500ms));
}

TEST_CASE(SUITE("A script that never calls stop() has its server stopped when the port is destroyed"))
{
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/ok$", function(req, res) res:write(200, "ok") end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",PortConf(20813));
	h.Enable();
	h.Expect("started");
	CHECK(Http(20813,"GET","/ok").status == 200);
	h.Destroy(); //checks that every object has been destroyed
	CHECK_FALSE(Http(20813,"GET","/ok","",{},1000ms).ok);
}

TEST_CASE(SUITE("Collecting the last reference to a started server does not stop it"))
{
	LuaHarness h(R"lua(
function Enable()
	do
		local tmp = assert(odc.HTTPServer{ port = odc.Config.port })
		assert(tmp:route("GET", "^/ok$", function(req, res) res:write(200, "still serving") end))
		assert(tmp:start())
	end
	for i = 1, 3 do local unused = odc.HTTPServer{} end
	report("started", "")
end
function Event(e, sender, cb)
	collectgarbage()
	collectgarbage()
	report("collected", "")
end
)lua",PortConf(20814));
	h.Enable();
	h.Expect("started");
	h.Send("gc");
	h.Expect("collected");
	CHECK(WaitFor([](){ return NetObjectCount() == 1; })); //the three that were never started are gone
	CHECK(Http(20814,"GET","/ok").body == "still serving");
}

TEST_CASE(SUITE("The example in the network objects documentation runs"))
{
	std::ifstream readme(ODC_NETWORK_README);
	REQUIRE_FALSE(readme.fail());
	std::stringstream ss;
	ss << readme.rdbuf();
	const std::string text = ss.str();
	const auto marker = text.find("<!-- example:http-server -->");
	REQUIRE(marker != std::string::npos);
	const auto begin = text.find("```lua",marker);
	REQUIRE(begin != std::string::npos);
	const auto code_begin = text.find('\n',begin)+1;
	const auto code_end = text.find("```",code_begin);
	REQUIRE(code_end != std::string::npos);

	Json::Value conf;
	conf["Port"] = 20815;
	LuaHarness h(text.substr(code_begin,code_end-code_begin),conf);
	h.Enable();
	REQUIRE(WaitFor([&](){ return Http(20815,"GET","/").ok; }));
	auto page = Http(20815,"GET","/");
	CHECK(page.status == 200);
	CHECK(page.headers["content-type"].rfind("text/html",0) == 0);
	CHECK(page.body.find("opendatacon") != std::string::npos);
	auto r = Http(20815,"POST","/api/counter","{\"add\": 5}",{
		{"Content-Type","application/json"}
	});
	CHECK(r.status == 200);
	CHECK(r.headers["content-type"] == "application/json");
	CHECK(r.body == "{\"counter\":5}");
	r = Http(20815,"POST","/api/counter","{\"add\": 2}");
	CHECK(r.body == "{\"counter\":7}");
	r = Http(20815,"GET","/api/counter");
	CHECK(r.body == "{\"counter\":7}");
	r = Http(20815,"POST","/api/counter","not json");
	CHECK(r.status == 400);
	r = Http(20815,"GET","/nowhere");
	CHECK(r.status == 404);
	h.Disable();
}

#ifndef ODC_HTTPS_TESTS
TEST_CASE(SUITE("Without TLS support a server with a tls option returns no_tls"))
{
	LuaHarness h(R"lua(
function Enable()
	local s, err = odc.HTTPServer{ port = odc.Config.port, tls = { CertFile = "none.pem", KeyFile = "none.key" } }
	report("result", tostring(s)..","..tostring(err))
	local t = odc.HTTPServer{ port = odc.Config.port }
	report("plain", tostring(t ~= nil))
end
)lua",PortConf(20899));
	h.Enable();
	CHECK(h.Expect("result") == "nil,no_tls");
	CHECK(h.Expect("plain") == "true"); //plain HTTP is not affected
}
#endif

#ifdef ODC_HTTPS_TESTS

namespace
{
struct Files
{
	std::string cert;
	std::string key;
};

struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa, server, serverEncrypted, client, otherClient;
	Files serverF, serverEncryptedF, clientF, otherClientF;
	std::string caFile, otherCaFile;

	Pki()
	{
		const std::vector<std::string> sans = {"DNS:tls-server.test", "IP:127.0.0.1"};
		ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Test CA", {}, -3600, 86400LL * 3650, true, 1});
		server = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 100}, &ca);
		serverEncrypted = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 103, "correct-horse"}, &ca);
		client = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 200}, &ca);
		otherClient = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 300}, &otherCa);
		serverF = {dir.Write("server.crt", server.certPem), dir.Write("server.key", server.keyPem)};
		serverEncryptedF = {dir.Write("encrypted.crt", serverEncrypted.certPem), dir.Write("encrypted.key", serverEncrypted.keyPem)};
		clientF = {dir.Write("client.crt", client.certPem), dir.Write("client.key", client.keyPem)};
		otherClientF = {dir.Write("otherclient.crt", otherClient.certPem), dir.Write("otherclient.key", otherClient.keyPem)};
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
	}
};

Pki& GetPki()
{
	static Pki pki;
	return pki;
}

//One HTTPS request: the server's certificate is verified against ca; the client's certificate is given if id is
HttpReply Https(uint16_t port, const std::string& target, const std::string& ca, const Files* id, std::chrono::milliseconds timeout = 15000ms)
{
	asio::io_context io;
	asio::ssl::context ctx(asio::ssl::context::tls_client);
	std::error_code ec;
	ctx.load_verify_file(ca,ec);
	ctx.set_verify_mode(asio::ssl::verify_peer);
	if(id)
	{
		ctx.use_certificate_chain_file(id->cert,ec);
		ctx.use_private_key_file(id->key,asio::ssl::context::pem,ec);
	}
	asio::ssl::stream<asio::ip::tcp::socket> sock(io,ctx);
	if(!ConnectTo(io,sock.next_layer(),port,10000ms))
		return HttpReply();
	bool shook = false;
	std::error_code hec;
	sock.async_handshake(asio::ssl::stream_base::client,[&](const std::error_code& e){ hec = e; shook = true; });
	io.restart();
	io.run_for(timeout);
	if(!shook || hec)
		return HttpReply();
	return ExchangeOn(io,sock,BuildRequest("GET",target,"",{},true),timeout);
}
}

TEST_CASE(SUITE("HTTPS: a request over TLS with the server's certificate verified"))
{
	auto& pki = GetPki();
	Json::Value conf = PortConf(20850);
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port, tls = { CertFile = odc.Config.cert, KeyFile = odc.Config.key } })
	assert(srv:route("GET", "^/secure$", function(req, res) res:write(200, "over tls from "..req.remote_addr) end))
	assert(srv:start())
	report("started", srv:port())
end
)lua",conf);
	h.Enable();
	h.Expect("started");
	auto r = Https(20850,"/secure",pki.caFile,nullptr);
	REQUIRE(r.ok);
	CHECK(r.status == 200);
	CHECK(r.body == "over tls from 127.0.0.1");
	//a client that does not trust the CA does not get an answer
	CHECK_FALSE(Https(20850,"/secure",pki.otherCaFile,nullptr).ok);
	//plain HTTP to an HTTPS port gets nothing
	CHECK_FALSE(Http(20850,"GET","/secure","",{},2000ms).ok);
}

TEST_CASE(SUITE("HTTPS with CAFile asks the client for a certificate"))
{
	auto& pki = GetPki();
	Json::Value conf = PortConf(20851);
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	conf["ca"] = pki.caFile;
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port, tls = { CertFile = odc.Config.cert, KeyFile = odc.Config.key, CAFile = odc.Config.ca } })
	assert(srv:route("GET", "^/secure$", function(req, res) res:write(200, "mutual") end))
	assert(srv:start())
	srv2 = assert(odc.HTTPServer{ port = odc.Config.port + 1, tls = { CertFile = odc.Config.cert, KeyFile = odc.Config.key, CAFile = odc.Config.ca, VerifyPeer = false } })
	assert(srv2:route("GET", "^/secure$", function(req, res) res:write(200, "no client certificate needed") end))
	assert(srv2:start())
	report("started", srv:port())
end
)lua",conf);
	h.Enable();
	h.Expect("started");
	auto r = Https(20851,"/secure",pki.caFile,&pki.clientF);
	REQUIRE(r.ok);
	CHECK(r.body == "mutual");
	CHECK_FALSE(Https(20851,"/secure",pki.caFile,nullptr).ok);           //no client certificate
	CHECK_FALSE(Https(20851,"/secure",pki.caFile,&pki.otherClientF).ok); //one from another CA
	//VerifyPeer = false: the CA file is not used to ask the client for anything
	r = Https(20852,"/secure",pki.caFile,nullptr);
	REQUIRE(r.ok);
	CHECK(r.body == "no client certificate needed");
}

TEST_CASE(SUITE("HTTPS options: only the restricted set and files that can be read and no encrypted key"))
{
	auto& pki = GetPki();
	Json::Value conf = PortConf(20853);
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	conf["enc_cert"] = pki.serverEncryptedF.cert;
	conf["enc_key"] = pki.serverEncryptedF.key;
	conf["ca"] = pki.caFile;
	conf["missing"] = pki.dir.Path("does-not-exist.pem");
	conf["not_a_key"] = pki.dir.Write("not-a-key.pem","this is not a key\n");
	LuaHarness h(R"lua(
local C = odc.Config
local function srv(tls) return odc.HTTPServer{ port = C.port, tls = tls } end
function Enable()
	local cases =
	{
		{ "ok",          function() return srv{ CertFile = C.cert, KeyFile = C.key } end },
		{ "ok_ca",       function() return srv{ CertFile = C.cert, KeyFile = C.key, CAFile = C.ca, VerifyPeer = true, VerifyHostname = false } end },
		{ "not_table",   function() return srv("x") end },
		{ "no_cert",     function() return srv{ KeyFile = C.key } end },
		{ "no_key",      function() return srv{ CertFile = C.cert } end },
		{ "min_version", function() return srv{ CertFile = C.cert, KeyFile = C.key, MinVersion = "1.2" } end },
		{ "password_env",function() return srv{ CertFile = C.cert, KeyFile = C.key, KeyPasswordEnv = "SECRET" } end },
		{ "key_password",function() return srv{ CertFile = C.cert, KeyFile = C.key, KeyPassword = "hunter2" } end },
		{ "key_log",     function() return srv{ CertFile = C.cert, KeyFile = C.key, KeyLogFile = "keys.log" } end },
		{ "ciphers",     function() return srv{ CertFile = C.cert, KeyFile = C.key, Ciphers = "HIGH" } end },
		{ "verify_type", function() return srv{ CertFile = C.cert, KeyFile = C.key, VerifyPeer = "yes" } end },
		{ "missing_cert",function() return srv{ CertFile = C.missing, KeyFile = C.key } end },
		{ "missing_key", function() return srv{ CertFile = C.cert, KeyFile = C.missing } end },
		{ "missing_ca",  function() return srv{ CertFile = C.cert, KeyFile = C.key, CAFile = C.missing } end },
		{ "encrypted",   function() return srv{ CertFile = C.enc_cert, KeyFile = C.enc_key } end },
	}
	for _, c in ipairs(cases) do
		local s, err = c[2]()
		report("case_"..c[1], s and "object" or tostring(err))
	end
	--a certificate and key that do not fit are found when the server starts
	local bad = assert(srv{ CertFile = C.cert, KeyFile = C.not_a_key })
	local ok, err = bad:start()
	report("case_bad_key_start", tostring(ok)..","..tostring(err))
	report("done", "")
end
)lua",conf);
	h.Enable();
	h.Expect("done");
	std::map<std::string,std::string> r;
	for(const auto& e : h.sink.All())
	{
		const auto tab = e.find('\t');
		r[e.substr(0,tab)] = e.substr(tab+1);
	}
	auto Is = [&](const std::string& name, const std::string& value)
		    {
			    INFO(name+" = "+r["case_"+name]);
			    CHECK(r["case_"+name] == value);
		    };
	auto Has = [&](const std::string& name, const std::string& text)
		     {
			     INFO(name+" = "+r["case_"+name]);
			     CHECK(r["case_"+name].find(text) != std::string::npos);
		     };
	Is("ok","object");
	Is("ok_ca","object");
	Is("not_table","invalid_options: tls");
	Is("no_cert","invalid_options: tls.CertFile");
	Is("no_key","invalid_options: tls.KeyFile");
	Is("min_version","invalid_options: MinVersion is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("password_env","invalid_options: KeyPasswordEnv is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("key_password","invalid_options: KeyPassword is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("key_log","invalid_options: KeyLogFile is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("ciphers","invalid_options: Ciphers is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("verify_type","invalid_options: tls.VerifyPeer");
	Is("missing_cert","invalid_options: tls.CertFile cannot be read");
	Is("missing_key","invalid_options: tls.KeyFile cannot be read");
	Is("missing_ca","invalid_options: tls.CAFile cannot be read");
	Has("encrypted","invalid_options: tls.KeyFile is encrypted");
	CHECK(r["case_key_password"].find("hunter2") == std::string::npos);
	Has("bad_key_start","nil,tls config: could not use CertFile and KeyFile");
}

TEST_CASE(SUITE("HTTPS: a script that stops its server in Disable() stops the TLS server too"))
{
	auto& pki = GetPki();
	Json::Value conf = PortConf(20854);
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	LuaHarness h(R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port, tls = { CertFile = odc.Config.cert, KeyFile = odc.Config.key } })
	assert(srv:route("GET", "^/secure$", function(req, res) res:write(200, "secure") end))
	assert(srv:start())
	report("started", srv:port())
end
function Disable() srv:stop(); srv = nil end
)lua",conf);
	h.Enable();
	h.Expect("started");
	CHECK(Https(20854,"/secure",pki.caFile,nullptr).ok);
	h.Disable();
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	CHECK_FALSE(Https(20854,"/secure",pki.caFile,nullptr).ok);
}

#endif //ODC_HTTPS_TESTS
