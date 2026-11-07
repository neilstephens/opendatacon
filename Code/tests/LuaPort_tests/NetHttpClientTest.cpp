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
 * NetHttpClientTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Tests of odc.HTTPClient and the odc.HTTP helpers: real Lua scripts in a LuaPort against raw loopback servers written in the test (so the
 *  bytes on the wire are exact), against odc.HTTPServer in the same script, and with Simple-Web-Server built with TLS, against HTTPS servers
 *  with certificates from the test PKI. TCP ports 20900 to 20999.
 */
#include "NetTestHelpers.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

#ifdef ODC_HTTPS_TESTS
#include "TlsTestPki.h"
#endif

#define SUITE(name) "LuaNetHttpClientTestSuite - " name

using namespace nettest;

namespace
{
//A loopback HTTP server that answers what the test says, byte for byte, on a thread for each connection
class RawServer
{
public:
	using Socket = asio::ip::tcp::socket;
	using Fn = std::function<void (Socket&, const std::string&)>;

	explicit RawServer(uint16_t port):
		acceptor(io,asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port))
	{
		accept_thread = std::thread([this](){ AcceptLoop(); });
	}
	~RawServer()
	{
		Stop();
	}
	void Stop()
	{
		if(!stopping.exchange(true))
			WakeAcceptor(io,acceptor);
		std::vector<std::shared_ptr<Socket>> open;
		{
			std::lock_guard<std::mutex> lock(mtx);
			std::error_code ec;
			acceptor.close(ec);
			open = sockets;
		}
		for(auto& s : open)
		{
			std::error_code ec;
			s->shutdown(asio::socket_base::shutdown_both,ec);
			s->close(ec);
		}
		if(accept_thread.joinable())
			accept_thread.join();
		std::vector<std::thread> w;
		{
			std::lock_guard<std::mutex> lock(mtx);
			w.swap(workers);
		}
		for(auto& t : w)
			t.join();
	}

	//what to do for a request target (the path and query as sent)
	void On(const std::string& target, Fn fn)
	{
		std::lock_guard<std::mutex> lock(mtx);
		routes[target] = std::move(fn);
	}
	//a canned response, sent in one write; the connection is closed after it unless keep_open
	void Respond(const std::string& target, const std::string& raw, bool keep_open = false)
	{
		On(target,[raw,keep_open,this](Socket& s, const std::string&)
			{
				Send(s,raw);
				if(keep_open)
					WaitClose(s);
			});
	}
	//reads the request and then nothing, until the client closes
	void Hang(const std::string& target)
	{
		On(target,[this](Socket& s, const std::string&)
			{
				++hanging;
				WaitClose(s);
			});
	}
	std::string Request(const std::string& target)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return seen[target];
	}
	int Connections() const
	{
		return connections.load();
	}
	int ClosedByClient() const
	{
		return closed_by_client.load();
	}
	//the requests that were read and are waiting for an answer that does not come (Hang)
	int Hanging() const
	{
		return hanging.load();
	}

	static void Send(Socket& s, const std::string& data)
	{
		std::error_code ec;
		asio::write(s,asio::buffer(data),ec);
	}
	//blocks until the client closes the connection (or the server stops)
	void WaitClose(Socket& s)
	{
		std::error_code ec;
		char buf[4096];
		while(!ec)
			s.read_some(asio::buffer(buf),ec);
		++closed_by_client;
	}
	static std::string Chunk(const std::string& data)
	{
		std::ostringstream o;
		o << std::hex << data.size() << "\r\n" << data << "\r\n";
		return o.str();
	}

private:
	void AcceptLoop()
	{
		while(!stopping)
		{
			auto s = std::make_shared<Socket>(io);
			std::error_code ec;
			acceptor.accept(*s,ec);
			if(ec || stopping)
				return;
			++connections;
			std::lock_guard<std::mutex> lock(mtx);
			sockets.push_back(s);
			workers.emplace_back([this,s](){ Serve(*s); });
		}
	}

	//reads one request (the head, and a body by its Content-Length)
	static bool ReadRequest(Socket& s, std::string& req)
	{
		std::error_code ec;
		char buf[65536];
		while(req.find("\r\n\r\n") == std::string::npos)
		{
			const auto n = s.read_some(asio::buffer(buf),ec);
			if(ec)
				return false;
			req.append(buf,n);
		}
		const auto split = req.find("\r\n\r\n")+4;
		auto lower = req.substr(0,split);
		std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
		size_t length = 0;
		const auto cl = lower.find("content-length:");
		if(cl != std::string::npos)
			length = std::stoul(lower.substr(cl+15));
		while(req.size() < split+length)
		{
			const auto n = s.read_some(asio::buffer(buf),ec);
			if(ec)
				return false;
			req.append(buf,n);
		}
		return true;
	}

	void Serve(Socket& s)
	{
		std::string req;
		if(!ReadRequest(s,req))
			return;
		const auto a = req.find(' ');
		const auto b = req.find(' ',a+1);
		const std::string target = req.substr(a+1,b-a-1);
		Fn fn;
		{
			std::lock_guard<std::mutex> lock(mtx);
			seen[target] = req;
			auto it = routes.find(target);
			if(it != routes.end())
				fn = it->second;
		}
		if(fn)
			fn(s,req);
		std::error_code ec;
		s.shutdown(asio::socket_base::shutdown_both,ec);
	}

	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	std::mutex mtx;
	std::map<std::string,Fn> routes;
	std::map<std::string,std::string> seen;
	std::vector<std::shared_ptr<Socket>> sockets;
	std::vector<std::thread> workers;
	std::thread accept_thread;
	std::atomic<bool> stopping{false};
	std::atomic<int> connections{0};
	std::atomic<int> closed_by_client{0};
	std::atomic<int> hanging{0};
};

Json::Value PortConf(uint16_t port)
{
	Json::Value conf;
	conf["port"] = port;
	return conf;
}

//Lua every client script starts with: show() puts a response or an error in one line, call() makes a client and one request and reports it, and
//done is reported once as many results as were announced with expect(n) have come
const char* Common = R"lua(
local remaining = 0
function expect(n) remaining = n end
local function finished()
	remaining = remaining - 1
	if remaining == 0 then report("done", "") end
end
function show(resp, err, detail)
	if err then return "err="..err end
	local keys = {}
	for k in pairs(resp.headers) do keys[#keys+1] = k end
	table.sort(keys)
	local hs = {}
	for _, k in ipairs(keys) do
		local v = resp.headers[k]
		if type(v) == "table" then
			table.sort(v)
			v = "["..table.concat(v, ";").."]"
		end
		hs[#hs+1] = k.."="..v
	end
	return "status="..resp.status.."|reason="..resp.reason.."|ver="..resp.http_version.."|len="..#resp.body.."|body="..resp.body.."|"..table.concat(hs, ",")
end
function url() return "http://127.0.0.1:"..odc.Config.port end
function call(name, copts, ropts)
	copts = copts or {}
	if copts.url == nil then copts.url = url() end
	local c, cerr = odc.HTTPClient(copts)
	if not c then
		report(name, "ctor="..tostring(cerr))
		finished()
		return
	end
	ropts = ropts or {}
	ropts.method = ropts.method or "GET"
	ropts.path = ropts.path or "/"
	ropts.on_response = function(resp, err, detail)
		report(name, show(resp, err, detail))
		finished()
	end
	local ok, err = c:request(ropts)
	if not ok then
		report(name, "request="..tostring(err))
		finished()
	end
	return c
end
)lua";

std::map<std::string,std::string> Results(LuaHarness& h)
{
	std::map<std::string,std::string> r;
	for(const auto& e : h.sink.All())
	{
		const auto tab = e.find('\t');
		r[e.substr(0,tab)] = e.substr(tab+1);
	}
	return r;
}

bool Contains(const std::string& text, const std::string& part)
{
	return text.find(part) != std::string::npos;
}
}

TEST_CASE(SUITE("Status headers and bodies and what the client sends"))
{
	RawServer server(20900);
	server.Respond("/a/b?x=1","HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\nX-A: 1\r\nX-A: 2\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nnope");
	for(const char* t : {"/post","/post_empty","/put","/delete","/patch"})
		server.Respond(t,"HTTP/1.1 201 Created\r\nContent-Length: 2\r\nLocation: /new\r\n\r\nok");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(6)
	call("get", nil, { path = "/a/b?x=1", headers = { ["X-Custom"] = "v", Accept = "text/plain", ["X-Num"] = 42 } })
	call("post", nil, { method = "POST", path = "/post", body = "hello" })
	call("post_empty", nil, { method = "POST", path = "/post_empty" })
	call("put", nil, { method = "PUT", path = "/put", body = "" })
	call("delete", nil, { method = "DELETE", path = "/delete" })
	call("patch", nil, { method = "PATCH", path = "/patch", body = "x" })
end
)lua",PortConf(20900));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);

	//(the order of a repeated header is the order of SWS's map: the values are what is checked)
	const auto& g = r["get"];
	CHECK(g.rfind("status=404|reason=Not Found|ver=1.1|len=4|body=nope|",0) == 0);
	CHECK(Contains(g,"connection=close,content-length=4,content-type=text/plain,set-cookie=[a=1;b=2],x-a="));
	CHECK((Contains(g,"x-a=1, 2") || Contains(g,"x-a=2, 1")));
	CHECK(r["post"] == "status=201|reason=Created|ver=1.1|len=2|body=ok|content-length=2,location=/new");

	const auto req = server.Request("/a/b?x=1");
	CHECK(req.rfind("GET /a/b?x=1 HTTP/1.1\r\n",0) == 0);
	CHECK(Contains(req,"\r\nHost: 127.0.0.1:20900\r\n"));
	CHECK(Contains(req,"\r\nConnection: close\r\n"));
	CHECK(Contains(req,"\r\nX-Custom: v\r\n"));
	CHECK(Contains(req,"\r\nAccept: text/plain\r\n"));
	CHECK(Contains(req,"\r\nX-Num: 42\r\n"));
	CHECK_FALSE(Contains(req,"Content-Length"));
	const auto post = server.Request("/post");
	CHECK(Contains(post,"\r\nContent-Length: 5\r\n"));
	CHECK(post.size() > 5);
	CHECK(post.substr(post.size()-5) == "hello");
	CHECK(Contains(server.Request("/post_empty"),"\r\nContent-Length: 0\r\n"));
	CHECK(Contains(server.Request("/put"),"\r\nContent-Length: 0\r\n"));
	CHECK(Contains(server.Request("/patch"),"\r\nContent-Length: 1\r\n"));
	CHECK_FALSE(Contains(server.Request("/delete"),"Content-Length"));
	for(const char* t : {"/post","/delete"})
		CHECK(server.Request(t).rfind(std::string(t) == "/post" ? "POST " : "DELETE ",0) == 0);
}

TEST_CASE(SUITE("Chunked and length delimited and close delimited bodies"))
{
	RawServer server(20901);
	server.On("/chunked",[](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nX-Kind: chunked\r\n\r\n");
			std::this_thread::sleep_for(20ms);
			RawServer::Send(s,RawServer::Chunk("hello "));
			std::this_thread::sleep_for(20ms);
			//a chunk size line split across two writes, and the chunk data in a third
			const auto big = RawServer::Chunk(std::string(300,'y'));
			RawServer::Send(s,big.substr(0,2));
			std::this_thread::sleep_for(20ms);
			RawServer::Send(s,big.substr(2,50));
			std::this_thread::sleep_for(20ms);
			RawServer::Send(s,big.substr(52)+RawServer::Chunk("world")+"0\r\n\r\n");
		});
	server.On("/length",[](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nhello ");
			std::this_thread::sleep_for(30ms);
			RawServer::Send(s,"world");
		});
	server.On("/length_extra",[](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabcdefgh"); //more than the length: ignored
		});
	server.On("/close_delimited",[](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nabc");
			std::this_thread::sleep_for(30ms);
			RawServer::Send(s,"def");
		});
	server.On("/no_length",[](RawServer::Socket& s, const std::string&)
		{
			//HTTP/1.1 without a length or a Connection header: the end of the stream is the end of the body, as we asked for the connection to close
			RawServer::Send(s,"HTTP/1.1 200 OK\r\n\r\nabc");
			std::this_thread::sleep_for(30ms);
			RawServer::Send(s,"def");
		});
	server.On("/http10",[](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.0 200 OK\r\n\r\nold school");
		});
	server.Respond("/empty","HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
	server.On("/big",[](RawServer::Socket& s, const std::string&)
		{
			const auto body = Pattern(4*1024*1024);
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n");
			RawServer::Send(s,body);
		});
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(9)
	for _, name in ipairs({ "chunked", "length", "length_extra", "close_delimited", "no_length", "http10", "empty" }) do
		call(name, nil, { path = "/"..name })
	end
	call("big_default", nil, { path = "/big" })
	call("big", { max_response_bytes = 0, timeout_ms = 30000 }, { path = "/big" })
end
)lua",PortConf(20901));
	h.Enable();
	h.Expect("done",60000ms);
	auto r = Results(h);
	CHECK(Contains(r["chunked"],"status=200|reason=OK|ver=1.1|len="+std::to_string(6+300+5)+"|body=hello "+std::string(300,'y')+"world|"));
	CHECK(Contains(r["chunked"],"x-kind=chunked"));
	CHECK(Contains(r["length"],"len=11|body=hello world|"));
	CHECK(Contains(r["length_extra"],"len=3|body=abc|"));
	CHECK(Contains(r["close_delimited"],"len=6|body=abcdef|"));
	CHECK(Contains(r["no_length"],"len=6|body=abcdef|"));
	CHECK(Contains(r["http10"],"ver=1.0|len=10|body=old school|"));
	CHECK(Contains(r["empty"],"status=200|reason=OK|ver=1.1|len=0|body=|"));
	CHECK(r["big_default"] == "err=message_size"); //over the default 1 MiB
	CHECK(r["big"].rfind("status=200|reason=OK|ver=1.1|len=4194304|body=",0) == 0);
	const auto expected = Pattern(4*1024*1024);
	CHECK(Contains(r["big"],"|body="+expected+"|"));
}

TEST_CASE(SUITE("max_response_bytes: message_size for headers and each kind of body"))
{
	RawServer server(20902);
	const std::string body(2000,'x');
	server.Respond("/length","HTTP/1.1 200 OK\r\nContent-Length: 2000\r\n\r\n"+body);
	server.Respond("/exact","HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n"+std::string(1000,'e'));
	server.Respond("/chunked","HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"+RawServer::Chunk(body.substr(0,1000))+RawServer::Chunk(body.substr(1000))+"0\r\n\r\n");
	server.Respond("/close","HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n"+body);
	server.Respond("/headers","HTTP/1.1 200 OK\r\nX-Pad: "+std::string(1500,'p')+"\r\nContent-Length: 2\r\n\r\nok");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(5)
	for _, name in ipairs({ "length", "exact", "chunked", "close", "headers" }) do
		call(name, { max_response_bytes = 1000 }, { path = "/"..name })
	end
end
)lua",PortConf(20902));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	CHECK(r["length"] == "err=message_size");
	CHECK(Contains(r["exact"],"status=200|reason=OK|ver=1.1|len=1000|"));
	CHECK(r["chunked"] == "err=message_size");
	CHECK(r["close"] == "err=message_size");
	CHECK(r["headers"] == "err=message_size");
}

TEST_CASE(SUITE("HEAD 204 and 304 have no body and do not wait for one"))
{
	RawServer server(20903);
	//the connection stays open and the length is stated: a client that waited for the body would hang
	server.Respond("/head","HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Kind: head\r\n\r\n",true);
	server.Respond("/204","HTTP/1.1 204 No Content\r\nContent-Length: 5\r\n\r\n",true);
	server.Respond("/304","HTTP/1.1 304 Not Modified\r\nETag: \"abc\"\r\n\r\n",true);
	server.Respond("/get","HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(4)
	call("head", { timeout_ms = 10000 }, { method = "HEAD", path = "/head" })
	call("no_content", { timeout_ms = 10000 }, { method = "DELETE", path = "/204" })
	call("not_modified", { timeout_ms = 10000 }, { path = "/304" })
	call("get", { timeout_ms = 10000 }, { path = "/get" })
end
)lua",PortConf(20903));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	CHECK(r["head"] == "status=200|reason=OK|ver=1.1|len=0|body=|content-length=5,x-kind=head");
	CHECK(Contains(r["no_content"],"status=204|reason=No Content|ver=1.1|len=0|body=|"));
	CHECK(Contains(r["not_modified"],"status=304|reason=Not Modified|ver=1.1|len=0|body=|etag=\"abc\""));
	CHECK(Contains(r["get"],"len=5|body=hello|"));
	CHECK(WaitFor([&](){ return server.ClosedByClient() == 3; })); //the client closed the connections it was finished with
}

TEST_CASE(SUITE("A redirect is not followed"))
{
	RawServer server(20904);
	server.Respond("/old","HTTP/1.1 302 Found\r\nLocation: /new\r\nContent-Length: 0\r\n\r\n");
	server.Respond("/new","HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nnew");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(1)
	call("old", nil, { path = "/old" })
end
)lua",PortConf(20904));
	h.Enable();
	h.Expect("done");
	CHECK(Results(h)["old"] == "status=302|reason=Found|ver=1.1|len=0|body=|content-length=0,location=/new");
	CHECK(server.Request("/new").empty());
}

TEST_CASE(SUITE("A response that does not arrive: timeout_ms is exact and covers the whole request"))
{
	RawServer server(20905);
	server.Hang("/hang");
	server.On("/stall",[&](RawServer::Socket& s, const std::string&)
		{
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nonly a little");
			server.WaitClose(s);
		});
	server.Respond("/ok","HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(3)
	call("hang", { timeout_ms = 400 }, { path = "/hang" })
	call("stall", { timeout_ms = 400 }, { path = "/stall" })
	call("fine", { timeout_ms = 400 }, { path = "/ok" })
end
)lua",PortConf(20905));
	const auto start = std::chrono::steady_clock::now();
	h.Enable();
	h.Expect("done");
	const auto elapsed = std::chrono::steady_clock::now()-start;
	auto r = Results(h);
	CHECK(r["hang"] == "err=timeout");
	CHECK(r["stall"] == "err=timeout");
	CHECK(Contains(r["fine"],"status=200"));
	CHECK(elapsed >= 350ms);
	CHECK(elapsed < 3000ms);                                       //(SWS alone would have waited a whole second)
	CHECK(WaitFor([&](){ return server.ClosedByClient() == 2; })); //the connections were closed
}

TEST_CASE(SUITE("The connect timeout and refused and resolve_failed and a callback never inside the call"))
{
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(4)
	in_call = true
	call("refused", { url = "http://127.0.0.1:20998" }, { path = "/" })
	call("resolve", { url = "http://no-such-host.invalid:20997" }, { path = "/" })
	call("connect", { url = "http://192.0.2.1:81", connect_timeout_ms = 300, timeout_ms = 0 }, { path = "/" })
	in_call = false
	report("returned", "")
	--(the callbacks of the three above are not called before this function returns: they come after "returned")
	call("refused_again", { url = "http://127.0.0.1:20998" }, { path = "/" })
end
)lua",Json::Value());
	h.Enable();
	h.Expect("done",60000ms);
	const auto all = h.sink.All();
	auto index = [&](const std::string& tag)
			 {
				 for(size_t i = 0; i < all.size(); ++i)
					 if(all[i].rfind(tag+"\t",0) == 0)
						 return static_cast<int>(i);
				 return -1;
			 };
	CHECK(index("returned") >= 0);
	CHECK(index("returned") < index("refused"));
	CHECK(index("returned") < index("resolve"));
	auto r = Results(h);
	CHECK(r["refused"] == "err=refused");
	CHECK(r["refused_again"] == "err=refused");
	CHECK(r["resolve"] == "err=resolve_failed");
	//a documentation address: no answer (timeout), or this network says at once that there is no route
	INFO("connect: "+r["connect"]);
	CHECK((r["connect"] == "err=timeout" || r["connect"] == "err=unreachable" || r["connect"] == "err=refused"));
}

TEST_CASE(SUITE("cancel() and close() abort what is in flight and the callbacks come after the call"))
{
	RawServer server(20906);
	std::atomic<int> hung{0};
	server.On("/hang",[&](RawServer::Socket& s, const std::string&)
		{
			++hung;
			server.WaitClose(s);
		});
	server.Respond("/ok","HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
	LuaHarness h(std::string(Common)+R"lua(
local c
local phase = 0
local function req(name, path)
	local ok, err = c:request{ method = "GET", path = path, on_response = function(resp, e, detail)
		report(name, show(resp, e, detail))
	end }
	if not ok then report(name, "request="..tostring(err)) end
end
function Enable()
	c = assert(odc.HTTPClient{ url = url(), timeout_ms = 0 })
	req("a", "/hang")
	req("b", "/hang")
	req("c", "/hang")
	report("cancelled", tostring(c:cancel()))
	report("cancel_again", tostring(c:cancel()))
	req("after_cancel", "/ok")
end
function Event(e, sender, cb)
	phase = phase + 1
	if phase == 1 then
		req("d", "/hang")
		req("e", "/hang")
		report("sent", "")
		return
	end
	report("closed", tostring(c:close()))
	req("after_close", "/ok")
	report("close_again", tostring(c:close()))
	local ok, err = c:request{ method = "GET", path = "/ok", on_response = function() report("never", "") end }
	report("late_request", tostring(ok)..","..tostring(err))
end
)lua",PortConf(20906));
	h.Enable();
	REQUIRE(WaitFor([&](){ return h.sink.Count("after_cancel") > 0 && h.sink.Count("c") > 0; }));
	h.Send("go");
	h.Expect("sent");
	REQUIRE(WaitFor([&](){ return hung.load() >= 2; }));
	h.Send("close");
	REQUIRE(WaitFor([&](){ return h.sink.Count("late_request") > 0 && h.sink.Count("e") > 0 && h.sink.Count("after_close") > 0; }));
	auto r = Results(h);
	CHECK(r["a"] == "err=aborted");
	CHECK(r["b"] == "err=aborted");
	CHECK(r["c"] == "err=aborted");
	CHECK(r["cancelled"] == "true");
	CHECK(r["cancel_again"] == "true");
	CHECK(Contains(r["after_cancel"],"status=200")); //the client is usable after cancel()
	CHECK(r["d"] == "err=aborted");
	CHECK(r["e"] == "err=aborted");
	CHECK(r["closed"] == "true");
	CHECK(r["close_again"] == "true");
	CHECK(r["after_close"] == "request=closed");
	CHECK(r["late_request"] == "nil,closed");
	CHECK(h.NoReport("never",200ms));
	//the callbacks of cancel() came after the calls that follow it in the script
	const auto all = h.sink.All();
	auto index = [&](const std::string& tag)
			 {
				 for(size_t i = 0; i < all.size(); ++i)
					 if(all[i].rfind(tag+"\t",0) == 0)
						 return static_cast<int>(i);
				 return -1;
			 };
	CHECK(index("cancelled") < index("a"));
	CHECK(index("cancel_again") < index("a"));
	CHECK(index("close_again") < index("d"));
	CHECK(WaitFor([&](){ return server.ClosedByClient() == hung.load(); })); //every connection that got as far as the server was closed by the client
	CHECK(hung.load() >= 2);
}

TEST_CASE(SUITE("Cancelling at once, before the connection is up, leaves no connection behind"))
{
	RawServer server(20917);
	std::atomic<int> hung{0};
	server.On("/hang",[&](RawServer::Socket& s, const std::string&)
		{
			++hung;
			server.WaitClose(s);
		});
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	local c = assert(odc.HTTPClient{ url = url(), timeout_ms = 0, connect_timeout_ms = 0 })
	local aborted = 0
	for i = 1, 150 do
		assert(c:request{ method = "GET", path = "/hang", on_response = function(resp, err)
			if err == "aborted" then aborted = aborted + 1 end
			if aborted == 150 then report("all_aborted", "") end
		end })
		if i % 3 ~= 0 then c:cancel() end --(the others are cancelled a little later, with more of the connection set up)
	end
	c:cancel()
end
)lua",PortConf(20917));
	h.Enable();
	h.Expect("all_aborted");
	//whatever of these got as far as the server was closed by the client, and the io service is not held by one that is still waiting
	CHECK(WaitFor([&](){ return server.ClosedByClient() == hung.load(); }));
	h.Destroy();
	CHECK(server.ClosedByClient() == hung.load());
}

TEST_CASE(SUITE("Twenty requests at once on one client against odc.HTTPServer, answered together"))
{
	LuaHarness h(std::string(Common)+R"lua(
local waiting = {}
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("GET", "^/n/(\\d+)$", function(req, res)
		waiting[#waiting+1] = { res = res, n = req.captures[1] }
		--no answer until all twenty have arrived: they must have been made at the same time
		if #waiting == 20 then
			for i = #waiting, 1, -1 do waiting[i].res:write(200, "answer "..waiting[i].n) end
			waiting = {}
		end
	end))
	assert(srv:start())
	client = assert(odc.HTTPClient{ url = url(), timeout_ms = 20000 })
	local got = 0
	local bad = 0
	for i = 1, 20 do
		assert(client:request{ method = "GET", path = "/n/"..i, on_response = function(resp, err)
			if err or resp.status ~= 200 or resp.body ~= "answer "..i then bad = bad + 1 end
			got = got + 1
			if got == 20 then report("all", "got="..got..",bad="..bad) end
		end })
	end
end
)lua",PortConf(20907));
	h.Enable();
	CHECK(h.Expect("all") == "got=20,bad=0");
}

TEST_CASE(SUITE("A JSON round trip against odc.HTTPServer with the query and headers"))
{
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	srv = assert(odc.HTTPServer{ port = odc.Config.port })
	assert(srv:route("POST", "^/echo$", function(req, res)
		local body = odc.DecodeJSON(req.body)
		body.seen_query = req.query.tag
		body.seen_auth = req.headers["authorization"]
		res:write(201, odc.EncodeJSON(body), { ["Content-Type"] = "application/json", ["X-Seen-Type"] = req.headers["content-type"] })
	end))
	assert(srv:start())
	client = assert(odc.HTTPClient{ url = url() })
	assert(client:request{
		method = "POST",
		path = "/echo?"..odc.HTTP.query_encode{ tag = "a b&c" },
		headers = { ["Content-Type"] = "application/json", Authorization = "Bearer not-a-real-token" },
		body = odc.EncodeJSON({ name = "x", list = { 1, 2, 3 } }),
		on_response = function(resp, err)
			local d = odc.DecodeJSON(resp.body)
			report("result", resp.status..","..resp.headers["content-type"]..","..resp.headers["x-seen-type"]..","..d.name..","..#d.list..","..d.seen_query..","..d.seen_auth)
		end })
end
)lua",PortConf(20908));
	h.Enable();
	CHECK(h.Expect("result") == "201,application/json,application/json,x,3,a b&c,Bearer not-a-real-token");
	//the token is not in the log
	CHECK_FALSE(h.capture->Has("not-a-real-token"));
}

TEST_CASE(SUITE("Requests that are refused before anything is sent"))
{
	RawServer server(20909);
	LuaHarness h(std::string(Common)+R"lua(
local function try(name, opts)
	opts.on_response = function() report("called", name) end
	if opts.method == nil then opts.method = "GET" end
	if opts.path == nil then opts.path = "/" end
	local c = odc.HTTPClient{ url = url() }
	local ok, err = c:request(opts)
	report("case_"..name, tostring(ok)..","..tostring(err))
end
function Enable()
	try("ok_control", { headers = { ["X-Fine"] = "yes" } })
	try("no_method", { method = false })
	try("lower_method", { method = "get" })
	try("space_method", { method = "G ET" })
	try("crlf_method", { method = "GET / HTTP/1.1\r\nHost: evil\r\n\r\nGET" })
	try("path_no_slash", { path = "x" })
	try("path_space", { path = "/a b" })
	try("path_crlf", { path = "/x\r\nHost: evil" })
	try("path_nul", { path = "/x\0y" })
	try("path_type", { path = 5 })
	try("host", { headers = { Host = "evil.test" } })
	try("host_lower", { headers = { host = "evil.test" } })
	try("connection", { headers = { Connection = "keep-alive" } })
	try("content_length", { headers = { ["Content-Length"] = "5" }, body = "hello" })
	try("transfer_encoding", { headers = { ["transfer-encoding"] = "chunked" } })
	try("expect", { headers = { Expect = "100-continue" } })
	try("upgrade", { headers = { Upgrade = "websocket" } })
	try("name_space", { headers = { ["X A"] = "1" } })
	try("name_colon", { headers = { ["X:A"] = "1" } })
	try("name_crlf", { headers = { ["X-A\r\nX-B"] = "1" } })
	try("name_empty", { headers = { [""] = "1" } })
	try("value_crlf", { headers = { ["X-A"] = "1\r\nX-Injected: yes" } })
	try("value_lf", { headers = { ["X-A"] = "1\nX-Injected: yes" } })
	try("value_nul", { headers = { ["X-A"] = "1\0002" } })
	try("value_del", { headers = { ["X-A"] = "1\1272" } })
	try("value_table", { headers = { ["X-A"] = {} } })
	try("value_bool", { headers = { ["X-A"] = true } })
	try("headers_type", { headers = "X-A: 1" })
	try("array_headers", { headers = { "X-A: 1" } })
	try("body_type", { body = 5 })
	try("body_table", { body = {} })
	try("unknown_key", { timeout = 5 })
	local c = odc.HTTPClient{ url = url() }
	local _, e1 = c:request{ method = "GET", path = "/" }
	report("case_no_callback", tostring(e1))
	local _, e2 = c:request{ method = "GET", path = "/", on_response = "x" }
	report("case_callback_type", tostring(e2))
	local _, e3 = c:request("GET")
	report("case_not_table", tostring(e3))
	report("done", "")
end
)lua",PortConf(20909));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	auto Is = [&](const std::string& name, const std::string& value)
		    {
			    INFO(name+" = "+r["case_"+name]);
			    CHECK(r["case_"+name] == value);
		    };
	auto Refused = [&](const std::string& name, const std::string& error)
			   {
				   INFO(name+" = "+r["case_"+name]);
				   CHECK(r["case_"+name].rfind("nil,"+error,0) == 0);
			   };
	Is("ok_control","true,nil");
	Refused("no_method","invalid_options: method");
	Refused("lower_method","invalid_options: method");
	Refused("space_method","invalid_options: method");
	Refused("crlf_method","invalid_options: method");
	Refused("path_no_slash","invalid_options: path");
	Refused("path_space","invalid_options: path");
	Refused("path_crlf","invalid_options: path");
	Refused("path_nul","invalid_options: path");
	Refused("path_type","invalid_options: path");
	Refused("host","invalid_options: headers.Host is set by the binding");
	Refused("host_lower","invalid_options: headers.host is set by the binding");
	Refused("connection","invalid_options: headers.Connection is set by the binding");
	Refused("content_length","invalid_options: headers.Content-Length is set by the binding");
	Refused("transfer_encoding","invalid_options: headers.transfer-encoding is set by the binding");
	Refused("expect","invalid_options: headers.Expect is set by the binding");
	Refused("upgrade","invalid_options: headers.Upgrade is set by the binding");
	Refused("name_space","invalid_options: headers");
	Refused("name_colon","invalid_options: headers");
	Refused("name_crlf","invalid_options: headers");
	Refused("name_empty","invalid_options: headers");
	Refused("value_crlf","invalid_options: headers.X-A has a control character");
	Refused("value_lf","invalid_options: headers.X-A has a control character");
	Refused("value_nul","invalid_options: headers.X-A has a control character");
	Refused("value_del","invalid_options: headers.X-A has a control character");
	Refused("value_table","invalid_options: headers");
	Refused("value_bool","invalid_options: headers");
	Refused("headers_type","invalid_options: headers");
	Refused("array_headers","invalid_options: headers");
	Refused("body_type","invalid_options: body");
	Refused("body_table","invalid_options: body");
	Refused("unknown_key","invalid_options: timeout");
	Is("no_callback","invalid_options: on_response");
	Is("callback_type","invalid_options: on_response");
	Is("not_table","invalid_options: options");
	//only the control case was sent (its callback is the one "called")
	CHECK(WaitFor([&](){ return !server.Request("/").empty(); }));
	CHECK(server.Connections() == 1);
	CHECK(h.sink.Count("called") <= 1);
	const auto req = server.Request("/");
	CHECK(Contains(req,"X-Fine: yes"));
	CHECK_FALSE(Contains(req,"evil"));
	CHECK_FALSE(Contains(req,"Injected"));
}

TEST_CASE(SUITE("Responses that are not HTTP are bad_response, and a closed connection is closed"))
{
	RawServer server(20910);
	server.Respond("/garbage","NOT HTTP AT ALL\r\n\r\n");
	server.Respond("/cl_text","HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\nxyz");
	server.Respond("/cl_huge","HTTP/1.1 200 OK\r\nContent-Length: 99999999999999999999999999\r\n\r\nxyz");
	server.Respond("/cl_negative","HTTP/1.1 200 OK\r\nContent-Length: -5\r\n\r\nxyz");
	server.Respond("/cl_empty","HTTP/1.1 200 OK\r\nContent-Length:\r\n\r\nxyz");
	server.Respond("/cl_differ","HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Length: 4\r\n\r\nxyz");
	server.Respond("/chunk_bad","HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nabc\r\n0\r\n\r\n");
	server.Respond("/status_text","HTTP/1.1 2x0 Odd\r\nContent-Length: 0\r\n\r\n");
	server.Respond("/status_low","HTTP/1.1 99 Low\r\nContent-Length: 0\r\n\r\n");
	server.Respond("/nothing","");
	server.Respond("/short","HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nonly ten b");
	server.Respond("/chunk_cut","HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"+RawServer::Chunk("abc"));
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	local names = { "garbage", "cl_text", "cl_huge", "cl_negative", "cl_empty", "cl_differ", "chunk_bad", "status_text", "status_low", "nothing", "short", "chunk_cut" }
	expect(#names)
	for _, name in ipairs(names) do
		call(name, nil, { path = "/"..name })
	end
end
)lua",PortConf(20910));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	for(const char* name : {"garbage","cl_text","cl_huge","cl_negative","cl_empty","cl_differ","chunk_bad","status_text","status_low"})
	{
		INFO(name);
		CHECK(r[name] == "err=bad_response");
	}
	CHECK(r["nothing"] == "err=closed");
	CHECK(r["short"] == "err=closed");
	CHECK(r["chunk_cut"] == "err=closed");
	//(and opendatacon is still here: SWS alone would have thrown from a thread of the io service on a Content-Length that is not a number)
}

TEST_CASE(SUITE("An http proxy gets the request for the absolute URL"))
{
	RawServer proxy(20911);
	proxy.Respond("http://origin.test:8081/p?q=1","HTTP/1.1 200 OK\r\nContent-Length: 7\r\nX-Via: proxy\r\n\r\nproxied");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	expect(4)
	call("proxied", { url = "http://origin.test:8081", proxy = "127.0.0.1:20911" }, { path = "/p?q=1" })
	call("proxy_https", { url = "https://origin.test", proxy = "127.0.0.1:20911", tls = { CAFile = odc.Config.ca } }, {})
	call("proxy_bad", { url = "http://origin.test", proxy = "no port" }, {})
	call("proxy_type", { url = "http://origin.test", proxy = 5 }, {})
end
)lua",[]()
		{
			auto c = PortConf(20911);
			c["ca"] = "none.pem";
			return c;
		}());
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	CHECK(r["proxied"] == "status=200|reason=OK|ver=1.1|len=7|body=proxied|content-length=7,x-via=proxy");
	const auto req = proxy.Request("http://origin.test:8081/p?q=1");
	CHECK(req.rfind("GET http://origin.test:8081/p?q=1 HTTP/1.1\r\n",0) == 0);
	CHECK(Contains(req,"\r\nHost: origin.test:8081\r\n"));
	#ifdef ODC_HTTPS_TESTS
	CHECK(r["proxy_https"] == "ctor=invalid_options: proxy (not supported with https)");
	#else
	CHECK(r["proxy_https"] == "ctor=no_tls");
	#endif
	CHECK(r["proxy_bad"] == "ctor=invalid_options: proxy (expected host:port)");
	CHECK(r["proxy_type"] == "ctor=invalid_options: proxy (expected host:port)");
}

TEST_CASE(SUITE("The options of the constructor"))
{
	LuaHarness h(R"lua(
function Enable()
	local cases =
	{
		{ "ok", { url = "http://127.0.0.1:8080" } },
		{ "ok_no_port", { url = "http://localhost" } },
		{ "ok_slash", { url = "HTTP://LocalHost:8080/" } },
		{ "ok_all", { url = "http://127.0.0.1:8080", timeout_ms = 0, connect_timeout_ms = 0, max_response_bytes = 0 } },
		{ "no_url", {} },
		{ "url_type", { url = 5 } },
		{ "scheme", { url = "ftp://127.0.0.1" } },
		{ "no_scheme", { url = "127.0.0.1:80" } },
		{ "empty_host", { url = "http://" } },
		{ "path", { url = "http://127.0.0.1/path" } },
		{ "query", { url = "http://127.0.0.1?x=1" } },
		{ "userinfo", { url = "http://user@127.0.0.1" } },
		{ "space", { url = "http://127.0 .0.1" } },
		{ "port_zero", { url = "http://127.0.0.1:0" } },
		{ "port_big", { url = "http://127.0.0.1:65536" } },
		{ "port_text", { url = "http://127.0.0.1:http" } },
		{ "ipv6", { url = "http://[::1]:8080" } },
		{ "timeout_negative", { url = "http://127.0.0.1", timeout_ms = -1 } },
		{ "timeout_float", { url = "http://127.0.0.1", timeout_ms = 1.5 } },
		{ "timeout_text", { url = "http://127.0.0.1", timeout_ms = "100" } },
		{ "timeout_huge", { url = "http://127.0.0.1", timeout_ms = 1e12 } },
		{ "connect_timeout", { url = "http://127.0.0.1", connect_timeout_ms = true } },
		{ "max_bytes", { url = "http://127.0.0.1", max_response_bytes = -2 } },
		{ "unknown", { url = "http://127.0.0.1", retries = 3 } },
		{ "tls_with_http", { url = "http://127.0.0.1", tls = { CAFile = "x.pem" } } },
		{ "roots_with_http", { url = "http://127.0.0.1", system_roots = true } },
		{ "roots_type", { url = "http://127.0.0.1", system_roots = "yes" } },
	}
	for _, c in ipairs(cases) do
		local o, err = odc.HTTPClient(c[2])
		report("case_"..c[1], o and "object" or tostring(err))
	end
	local o, err = odc.HTTPClient("http://127.0.0.1")
	report("case_not_table", tostring(err))
	report("done", "")
end
)lua");
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	auto Is = [&](const std::string& name, const std::string& value)
		    {
			    INFO(name+" = "+r["case_"+name]);
			    CHECK(r["case_"+name] == value);
		    };
	Is("ok","object");
	Is("ok_no_port","object");
	Is("ok_slash","object");
	Is("ok_all","object");
	Is("no_url","invalid_options: url");
	Is("url_type","invalid_options: url");
	Is("scheme","invalid_options: url (the scheme must be http or https)");
	Is("no_scheme","invalid_options: url (the scheme must be http or https)");
	Is("empty_host","invalid_options: url (the scheme must be http or https)");
	for(const char* name : {"path","query","userinfo","space","port_zero","port_big","port_text"})
	{
		INFO(name);
		CHECK(r[std::string("case_")+name] == "invalid_options: url (expected scheme://host[:port]; the path is given per request)");
	}
	Is("ipv6","invalid_options: url (IPv6 literals are not supported)");
	Is("timeout_negative","invalid_options: timeout_ms");
	Is("timeout_float","invalid_options: timeout_ms");
	Is("timeout_text","invalid_options: timeout_ms");
	Is("timeout_huge","invalid_options: timeout_ms");
	Is("connect_timeout","invalid_options: connect_timeout_ms");
	Is("max_bytes","invalid_options: max_response_bytes");
	Is("unknown","invalid_options: retries");
	Is("tls_with_http","invalid_options: tls (only for https urls)");
	Is("roots_with_http","invalid_options: system_roots (only for https urls)");
	Is("roots_type","invalid_options: system_roots");
	Is("not_table","invalid_options: options");
}

TEST_CASE(SUITE("An error in a callback is logged and does not stop the others"))
{
	RawServer server(20912);
	server.Respond("/ok","HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	local c = assert(odc.HTTPClient{ url = url() })
	local n = 0
	local function cb(name, fail)
		return function(resp, err)
			n = n + 1
			report(name, tostring(err)..","..(resp and resp.status or "-"))
			if n == 3 then report("done", "") end
			if fail then error("script error in "..name) end
		end
	end
	assert(c:request{ method = "GET", path = "/ok", on_response = cb("one", false) })
	assert(c:request{ method = "GET", path = "/ok", on_response = cb("two", true) })
	assert(c:request{ method = "GET", path = "/ok", on_response = cb("three", false) })
	--a failing request too: the error handling is the same
	local d = assert(odc.HTTPClient{ url = "http://127.0.0.1:20996" })
	assert(d:request{ method = "GET", path = "/", on_response = function(resp, err) report("refused", tostring(err)); error("again") end })
end
)lua",PortConf(20912));
	h.Enable();
	h.Expect("done");
	CHECK(h.Expect("refused") == "refused");
	CHECK(h.Expect("one") == "nil,200");
	CHECK(h.Expect("two") == "nil,200");
	CHECK(h.Expect("three") == "nil,200");
	CHECK(WaitFor([&](){ return h.capture->Has("Error in HTTPClient http://127.0.0.1:20912 on_response for GET /ok") && h.capture->Has("script error in two"); }));
	CHECK(WaitFor([&](){ return h.capture->Has("Error in HTTPClient http://127.0.0.1:20996 on_response for GET /"); }));
}

TEST_CASE(SUITE("Collecting the last reference to a client with a request in flight does not cancel it"))
{
	RawServer server(20913);
	std::atomic<bool> release{false};
	server.On("/slow",[&](RawServer::Socket& s, const std::string&)
		{
			WaitFor([&](){ return release.load(); },30000ms);
			RawServer::Send(s,"HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nlate");
		});
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	do
		local c = assert(odc.HTTPClient{ url = url(), timeout_ms = 0 })
		assert(c:request{ method = "GET", path = "/slow", on_response = function(resp, err) report("result", show(resp, err)) end })
	end
	for i = 1, 3 do local unused = odc.HTTPClient{ url = url() } end
	report("made", "")
end
function Event(e, sender, cb)
	collectgarbage()
	collectgarbage()
	report("collected", "")
end
)lua",PortConf(20913));
	h.Enable();
	h.Expect("made");
	CHECK(WaitFor([&](){ return server.Connections() == 1; }));
	h.Send("gc");
	h.Expect("collected");
	CHECK(WaitFor([](){ return NetObjectCount() == 1; })); //the three that had no request are gone; the one in flight is kept
	CHECK(h.NoReport("result",200ms));
	release = true;
	CHECK(Contains(h.Expect("result"),"status=200|reason=OK|ver=1.1|len=4|body=late|"));
	CHECK(WaitFor([](){ return NetObjectCount() == 0; })); //and goes when it is done
}

TEST_CASE(SUITE("A script that closes its clients in Disable() drops the requests in flight: no late callback and nothing is left"))
{
	RawServer server(20914);
	server.Hang("/hang");
	LuaHarness h(std::string(Common)+R"lua(
--(the "aborted" that answers the script's own close() is not late)
local function late(resp, err) if err ~= "aborted" then report("late", tostring(err)) end end
function Enable()
	c = assert(odc.HTTPClient{ url = url(), timeout_ms = 0 })
	d = assert(odc.HTTPClient{ url = url(), timeout_ms = 400 })
	for i = 1, 3 do
		assert(c:request{ method = "GET", path = "/hang", on_response = late })
	end
	assert(d:request{ method = "GET", path = "/hang", on_response = late })
	report("started", "")
end
function Disable()
	c:close()
	d:close()
	c = nil
	d = nil
	collectgarbage()
	collectgarbage()
	report("disabled", "")
end
)lua",PortConf(20914));
	h.Enable();
	h.Expect("started");
	CHECK(WaitFor([&](){ return server.Hanging() == 4; }));
	h.Disable();
	h.Expect("disabled");
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	CHECK(WaitFor([&](){ return server.ClosedByClient() == 4; })); //the connections were closed
	CHECK(h.NoReport("late",800ms));                               //not even the 400 ms timeout of one of them
	//the port can be enabled again and works
	h.Enable();
	h.Expect("started");
	CHECK(WaitFor([&](){ return server.Hanging() == 8; }));
}

TEST_CASE(SUITE("Destroying the port with requests in flight leaves nothing"))
{
	RawServer server(20915);
	server.Hang("/hang");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	c = assert(odc.HTTPClient{ url = url(), timeout_ms = 0 })
	for i = 1, 5 do
		assert(c:request{ method = "GET", path = "/hang", on_response = function(resp, err) report("late", tostring(err)) end })
	end
	report("started", "")
end
)lua",PortConf(20915));
	h.Enable();
	h.Expect("started");
	CHECK(WaitFor([&](){ return server.Hanging() == 5; }));
	h.Destroy(); //checks that every object has been destroyed
	CHECK(WaitFor([&](){ return server.ClosedByClient() == 5; }));
	CHECK(h.NoReport("late",300ms));
}

TEST_CASE(SUITE("The odc.HTTP helpers"))
{
	LuaHarness h(R"lua(
local function eq(name, got, want)
	if got == want then report(name, "ok") else report(name, "got "..tostring(got).." want "..tostring(want)) end
end
function Enable()
	local H = odc.HTTP
	-- percent encoding
	eq("enc_plain", H.percent_encode("abcXYZ019-._~"), "abcXYZ019-._~")
	eq("enc_space", H.percent_encode("a b"), "a%20b")
	eq("enc_reserved", H.percent_encode("a&b=c/d?e#f+g%h"), "a%26b%3Dc%2Fd%3Fe%23f%2Bg%25h")
	eq("enc_empty", H.percent_encode(""), "")
	eq("enc_bytes", H.percent_encode("\0\1\255 \127"), "%00%01%FF%20%7F")
	eq("enc_utf8", H.percent_encode("\195\188"), "%C3%BC")
	eq("dec_plain", H.percent_decode("abc"), "abc")
	eq("dec_basic", H.percent_decode("a%20b%2Fc%2fd"), "a b/c/d")
	eq("dec_plus", H.percent_decode("a+b"), "a+b")
	eq("dec_bad", H.percent_decode("100%"), "100%")
	eq("dec_bad2", H.percent_decode("%zz%4"), "%zz%4")
	eq("dec_end", H.percent_decode("x%41"), "xA")
	eq("dec_nul", H.percent_decode("a%00b"), "a\0b")
	eq("dec_empty", H.percent_decode(""), "")
	local all = "\0\1\2 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\127\128\255abcXYZ"
	eq("roundtrip", H.percent_decode(H.percent_encode(all)), all)
	local bad = select(2, H.percent_encode(5))
	eq("enc_type", bad, "invalid_options: s")
	eq("dec_type", select(2, H.percent_decode(nil)), "invalid_options: s")
	-- query_encode
	eq("q_sorted", H.query_encode{ b = "x y", a = "1" }, "a=1&b=x%20y")
	eq("q_empty", H.query_encode{}, "")
	eq("q_number", H.query_encode{ n = 10, f = 1.5 }, "f=1.5&n=10")
	eq("q_boolean", H.query_encode{ t = true, f = false }, "f=false&t=true")
	eq("q_list", H.query_encode{ tag = { "x", "y z", 3 } }, "tag=x&tag=y%20z&tag=3")
	eq("q_empty_value", H.query_encode{ a = "" }, "a=")
	eq("q_special", H.query_encode{ ["a b"] = "c&d=e" }, "a%20b=c%26d%3De")
	eq("q_empty_list", H.query_encode{ a = {} }, "")
	eq("q_type", select(2, H.query_encode("a=1")), "invalid_options: query")
	eq("q_key", select(2, H.query_encode{ [1] = "a" }), "invalid_options: query (a key is not a string)")
	eq("q_value", select(2, H.query_encode{ a = print }), "invalid_options: query.a (not a string, number, boolean or list)")
	eq("q_list_value", select(2, H.query_encode{ a = { {} } }), "invalid_options: query.a (a list holds only strings, numbers and booleans)")
	-- query_parse
	local q, lists = H.query_parse("a=1&b=x%20y&c&a=2&d=%2B+e")
	eq("p_a", q.a, "2")
	eq("p_b", q.b, "x y")
	eq("p_c", q.c, "")
	eq("p_d", q.d, "+ e")
	eq("p_lists", table.concat(lists.a, ","), "1,2")
	eq("p_lists_b", #lists.b, 1)
	local q2 = H.query_parse("?x=1")
	eq("p_question", q2.x, "1")
	local q3, l3 = H.query_parse("")
	eq("p_empty", next(q3) == nil and next(l3) == nil, true)
	local q4 = H.query_parse("&&a=1&&=2&")
	eq("p_blanks", q4.a, "1")
	eq("p_type", select(2, H.query_parse(5)), "invalid_options: s")
	local rt = H.query_parse(H.query_encode{ ["k y"] = "v&=+%", z = "" })
	eq("p_roundtrip", rt["k y"].."|"..rt.z, "v&=+%|")
	-- status and reason
	eq("st_ok", H.status.ok, 200)
	eq("st_created", H.status.created, 201)
	eq("st_no_content", H.status.no_content, 204)
	eq("st_found", H.status.found, 302)
	eq("st_not_found", H.status.not_found, 404)
	eq("st_teapot", H.status.im_a_teapot, 418)
	eq("st_ise", H.status.internal_server_error, 500)
	eq("st_gateway", H.status.gateway_timeout, 504)
	eq("st_unknown", H.status.nothing_like_it, nil)
	eq("re_404", H.reason(404), "Not Found")
	eq("re_200", H.reason(200), "OK")
	eq("re_teapot", H.reason(418), "I'm a teapot")
	eq("re_unknown", H.reason(299), nil)
	eq("re_zero", H.reason(0), nil)
	eq("re_type", select(2, H.reason("404")), "invalid_options: code")
	local consistent = true
	for name, code in pairs(H.status) do
		if H.reason(code) == nil then consistent = false end
	end
	eq("st_consistent", consistent, true)
	-- date
	eq("d_epoch", H.date(0), "Thu, 01 Jan 1970 00:00:00 GMT")
	eq("d_rfc", H.date(784111777000), "Sun, 06 Nov 1994 08:49:37 GMT")
	eq("d_leap", H.date(951782400000), "Tue, 29 Feb 2000 00:00:00 GMT")
	eq("d_end_of_year", H.date(1262303999000), "Thu, 31 Dec 2009 23:59:59 GMT")
	eq("d_ms_dropped", H.date(1999), "Thu, 01 Jan 1970 00:00:01 GMT")
	eq("d_far", H.date(253402300799000), "Fri, 31 Dec 9999 23:59:59 GMT")
	eq("d_now", string.match(H.date(), "^%a%a%a, %d%d %a%a%a %d%d%d%d %d%d:%d%d:%d%d GMT$") ~= nil, true)
	eq("d_negative", select(2, H.date(-1)), "invalid_options: ms")
	eq("d_too_far", select(2, H.date(253402300800000)), "invalid_options: ms")
	eq("d_type", select(2, H.date("0")), "invalid_options: ms")
	report("done", "")
end
)lua");
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	int checked = 0;
	for(const auto& e : r)
	{
		if(e.first == "done")
			continue;
		INFO(e.first+": "+e.second);
		CHECK(e.second == "ok");
		++checked;
	}
	CHECK(checked > 60);
}

namespace
{
//Runs the Lua code that follows a marker in the network objects documentation
std::string ReadmeExample(const std::string& marker)
{
	std::ifstream readme(ODC_NETWORK_README);
	REQUIRE_FALSE(readme.fail());
	std::stringstream ss;
	ss << readme.rdbuf();
	const std::string text = ss.str();
	const auto at = text.find("<!-- example:"+marker+" -->");
	REQUIRE(at != std::string::npos);
	const auto begin = text.find("```lua",at);
	REQUIRE(begin != std::string::npos);
	const auto code_begin = text.find('\n',begin)+1;
	const auto code_end = text.find("```",code_begin);
	REQUIRE(code_end != std::string::npos);
	return text.substr(code_begin,code_end-code_begin);
}
}

TEST_CASE(SUITE("The client example in the network objects documentation runs"))
{
	Json::Value conf;
	conf["Port"] = 20916;
	LuaHarness h(ReadmeExample("http-client"),conf);
	h.Enable();
	CHECK(WaitFor([&](){ return h.capture->Has("counter is now 7"); }));
	CHECK(h.capture->Has("counter is now 5"));
	CHECK_FALSE(h.capture->Has("request failed"));
	CHECK_FALSE(h.capture->Has("unexpected status"));
	h.Disable();
}

TEST_CASE(SUITE("The helpers example in the network objects documentation runs"))
{
	LuaHarness h(ReadmeExample("http-helpers"));
	h.Enable();
	CHECK(WaitFor([&](){ return h.capture->Has("date: Sun, 06 Nov 1994 08:49:37 GMT"); }));
	CHECK(h.capture->Has("query: limit=10&name=a%20b%26c&tags=x&tags=y"));
	CHECK(h.capture->Has("name: a b&c tags: 2"));
	CHECK(h.capture->Has("encoded: 100%25%20of%20a%2Fb"));
	CHECK(h.capture->Has("decoded: 100% of a/b"));
	CHECK(h.capture->Has("status: 404 Not Found"));
	h.Disable();
}

namespace
{
//One GET to a loopback server, read to the end of the connection
bool HttpGet(uint16_t port, const std::string& target, int& status, std::string& body)
{
	asio::io_context io;
	asio::ip::tcp::socket sock(io);
	if(!ConnectTo(io,sock,port,5000ms))
		return false;
	const std::string req = "GET "+target+" HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
	std::error_code ec;
	asio::write(sock,asio::buffer(req),ec);
	std::string raw;
	char buf[4096];
	while(true)
	{
		const auto n = sock.read_some(asio::buffer(buf),ec);
		raw.append(buf,n);
		if(ec)
			break;
	}
	const auto split = raw.find("\r\n\r\n");
	if(split == std::string::npos || raw.rfind("HTTP/1.",0) != 0)
		return false;
	status = std::atoi(raw.c_str()+9);
	body = raw.substr(split+4);
	return true;
}

Json::Value ParseJson(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream in(text);
	if(!Json::parseFromStream(builder,in,&v,&errors))
		return Json::Value();
	return v;
}

std::string ReadWholeFile(const std::string& path)
{
	std::ifstream f(path,std::ios::binary);
	REQUIRE_FALSE(f.fail());
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
}

TEST_CASE(SUITE("The script in ExampleConfig/Lua-Network serves its pages and polls"))
{
	Json::Value conf;
	conf["HttpPort"] = 20918;
	conf["PollPeriodSecs"] = 0.3;
	LuaHarness h(ReadWholeFile(ODC_LUA_NETWORK_EXAMPLE),conf);
	h.Enable();
	Json::Value status;
	CHECK(WaitFor([&]()
		{
			int code = 0;
			std::string body;
			if(!HttpGet(20918,"/api/status",code,body) || code != 200)
				return false;
			status = ParseJson(body);
			return status["polls"].asInt() >= 2;
		}));
	CHECK(status["name"].asString() == "PortUnderTest");
	CHECK(status["polling"].asString() == "http://127.0.0.1:20918/api/status");
	CHECK(status["last_status"].asInt() == 200);
	CHECK(status["last_error"].isNull());
	CHECK(status["failures"].asInt() == 0);
	CHECK(status["uptime_s"].asInt() >= 0);

	int code = 0;
	std::string body;
	REQUIRE(HttpGet(20918,"/",code,body));
	CHECK(code == 200);
	CHECK(Contains(body,"<h1>PortUnderTest</h1>"));
	REQUIRE(HttpGet(20918,"/nowhere",code,body));
	CHECK(code == 404);
	CHECK(ParseJson(body)["path"].asString() == "/nowhere");
	CHECK_FALSE(h.capture->Has("api/status failed"));

	h.Disable();
	CHECK(WaitFor([&](){ return !HttpGet(20918,"/api/status",code,body); })); //the server is stopped
}

TEST_CASE(SUITE("The script in ExampleConfig/Lua-Network reports a poll that fails"))
{
	Json::Value conf;
	conf["HttpPort"] = 20919;
	conf["PollURL"] = "http://127.0.0.1:20997";
	conf["PollPeriodSecs"] = 0.2;
	LuaHarness h(ReadWholeFile(ODC_LUA_NETWORK_EXAMPLE),conf);
	h.Enable();
	Json::Value status;
	CHECK(WaitFor([&]()
		{
			int code = 0;
			std::string body;
			if(!HttpGet(20919,"/api/status",code,body) || code != 200)
				return false;
			status = ParseJson(body);
			return status["failures"].asInt() >= 1;
		}));
	CHECK(status["last_error"].asString() == "refused");
	CHECK(status["last_status"].isNull());
	CHECK(WaitFor([&](){ return h.capture->Has("poll of http://127.0.0.1:20997/api/status failed: refused"); }));
	h.Disable();
}

#ifndef ODC_HTTPS_TESTS
TEST_CASE(SUITE("Without TLS support an https client returns no_tls"))
{
	LuaHarness h(R"lua(
function Enable()
	local c, err = odc.HTTPClient{ url = "https://127.0.0.1:20999", tls = { CAFile = "none.pem" } }
	report("with_tls", tostring(c)..","..tostring(err))
	local d, err2 = odc.HTTPClient{ url = "https://127.0.0.1:20999" }
	report("plain_https", tostring(d)..","..tostring(err2))
	local e, err3 = odc.HTTPClient{ url = "https://127.0.0.1:20999", system_roots = true }
	report("roots", tostring(e)..","..tostring(err3))
	local f = odc.HTTPClient{ url = "http://127.0.0.1:20999" }
	report("http", tostring(f ~= nil))
end
)lua");
	h.Enable();
	CHECK(h.Expect("with_tls") == "nil,no_tls");
	CHECK(h.Expect("plain_https") == "nil,no_tls");
	CHECK(h.Expect("roots") == "nil,no_tls");
	CHECK(h.Expect("http") == "true"); //plain HTTP is not affected
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
	tlstest::Cert ca, otherCa, server, serverNoIp, serverExpired, serverEncrypted, client, otherClient;
	Files serverF, serverNoIpF, serverExpiredF, serverEncryptedF, clientF, otherClientF, clientExpiredF;
	std::string caFile, otherCaFile;

	Pki()
	{
		const std::vector<std::string> sans = {"DNS:tls-server.test", "IP:127.0.0.1"};
		ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Test CA", {}, -3600, 86400LL * 3650, true, 1});
		server = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 100}, &ca);
		serverNoIp = tlstest::MakeCert({"tls-server.test", {"DNS:tls-server.test"}, -3600, 86400LL * 365, false, 101}, &ca);
		serverExpired = tlstest::MakeCert({"tls-server.test", sans, -7200, -3600, false, 102}, &ca);
		serverEncrypted = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 103, "correct-horse"}, &ca);
		client = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 200}, &ca);
		otherClient = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 300}, &otherCa);
		serverF = {dir.Write("server.crt", server.certPem), dir.Write("server.key", server.keyPem)};
		serverNoIpF = {dir.Write("noip.crt", serverNoIp.certPem), dir.Write("noip.key", serverNoIp.keyPem)};
		serverExpiredF = {dir.Write("expired.crt", serverExpired.certPem), dir.Write("expired.key", serverExpired.keyPem)};
		serverEncryptedF = {dir.Write("encrypted.crt", serverEncrypted.certPem), dir.Write("encrypted.key", serverEncrypted.keyPem)};
		clientF = {dir.Write("client.crt", client.certPem), dir.Write("client.key", client.keyPem)};
		otherClientF = {dir.Write("otherclient.crt", otherClient.certPem), dir.Write("otherclient.key", otherClient.keyPem)};
		clientExpiredF = {dir.Path("client_renewed.crt"), dir.Path("client_renewed.key")};
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
	}
};

Pki& GetPki()
{
	static Pki pki;
	return pki;
}

//What a client that the server refused sees. With TLS 1.3 the client's handshake is done before the server looks at its certificate, so the alert comes with
//the first read, and the best answer is tls_client_certificate_rejected; but the server closes the connection at once, and if the reset gets to the client
//first (it does, more often on a busy machine, and on Windows a reset throws away what was received and not read) all it knows is that the connection closed.
bool Rejected(const std::string& result)
{
	return result == "err=tls_client_certificate_rejected" || result == "err=closed" || result == "err=tls_handshake_failed";
}

//The conf every HTTPS client test gives its script: the files
Json::Value TlsConf(uint16_t port)
{
	auto& pki = GetPki();
	Json::Value conf = PortConf(port);
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	conf["noip_cert"] = pki.serverNoIpF.cert;
	conf["noip_key"] = pki.serverNoIpF.key;
	conf["expired_cert"] = pki.serverExpiredF.cert;
	conf["expired_key"] = pki.serverExpiredF.key;
	conf["enc_cert"] = pki.serverEncryptedF.cert;
	conf["enc_key"] = pki.serverEncryptedF.key;
	conf["client_cert"] = pki.clientF.cert;
	conf["client_key"] = pki.clientF.key;
	conf["other_client_cert"] = pki.otherClientF.cert;
	conf["other_client_key"] = pki.otherClientF.key;
	conf["renewed_cert"] = pki.clientExpiredF.cert;
	conf["renewed_key"] = pki.clientExpiredF.key;
	conf["ca"] = pki.caFile;
	conf["other_ca"] = pki.otherCaFile;
	return conf;
}
}

TEST_CASE(SUITE("HTTPS: the server's certificate is verified and each failure has its own error"))
{
	auto& pki = GetPki();
	LuaHarness h(std::string(Common)+R"lua(
local C = odc.Config
local function serve(port, cert, key, ca)
	local s = assert(odc.HTTPServer{ port = port, tls = { CertFile = cert, KeyFile = key, CAFile = ca } })
	assert(s:route("GET", "^/secure$", function(req, res) res:write(200, "secure "..req.remote_addr) end))
	assert(s:route("POST", "^/echo$", function(req, res) res:write(200, req.body) end))
	assert(s:start())
	return s
end
function Enable()
	s1 = serve(C.port, C.cert, C.key)
	s2 = serve(C.port + 1, C.noip_cert, C.noip_key)
	s3 = serve(C.port + 2, C.expired_cert, C.expired_key)
	local function u(n) return "https://127.0.0.1:"..(C.port + n) end
	expect(14)
	call("ok", { url = u(0), tls = { CAFile = C.ca } }, { path = "/secure" })
	call("ok_defaults_explicit", { url = u(0), tls = { CAFile = C.ca, VerifyPeer = true, VerifyHostname = true } }, { path = "/secure" })
	call("wrong_ca", { url = u(0), tls = { CAFile = C.other_ca } }, { path = "/secure" })
	call("hostname", { url = u(1), tls = { CAFile = C.ca } }, { path = "/secure" })
	call("hostname_off", { url = u(1), tls = { CAFile = C.ca, VerifyHostname = false } }, { path = "/secure" })
	call("expired", { url = u(2), tls = { CAFile = C.ca } }, { path = "/secure" })
	call("verify_off", { url = u(0), tls = { CAFile = C.other_ca, VerifyPeer = false } }, { path = "/secure" })
	call("system_roots", { url = u(0), system_roots = true }, { path = "/secure" })
	call("system_roots_and_ca", { url = u(0), system_roots = true, tls = { CAFile = C.ca } }, { path = "/secure" })
	call("no_ca", { url = u(0) }, { path = "/secure" })
	call("no_ca_tls", { url = u(0), tls = { VerifyHostname = true } }, { path = "/secure" })
	call("to_plain_port", { url = "https://127.0.0.1:"..C.port + 10, tls = { CAFile = C.ca }, timeout_ms = 5000 }, { path = "/secure" }) -- (a plain server below)
	call("echo", { url = u(0), tls = { CAFile = C.ca }, max_response_bytes = 0 }, { method = "POST", path = "/echo", body = string.rep("0123456789abcdef", 16384) })
	call("name_in_url", { url = "https://localhost:"..C.port, tls = { CAFile = C.ca } }, { path = "/secure" })
end
)lua",TlsConf(20950));
	//a plain HTTP server that answers the TLS hello with an HTTP error at once
	Peer plain(20960,[](Peer::Socket& s, int)
		{
			std::error_code ec;
			asio::write(s,asio::buffer(std::string("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n")),ec);
		});
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	CHECK(r["ok"] == "status=200|reason=OK|ver=1.1|len=16|body=secure 127.0.0.1|content-length=16");
	CHECK(Contains(r["ok_defaults_explicit"],"status=200|reason=OK|"));
	CHECK(r["wrong_ca"] == "err=tls_untrusted");
	CHECK(r["hostname"] == "err=tls_hostname_mismatch");
	CHECK(Contains(r["hostname_off"],"status=200|reason=OK|"));
	CHECK(r["expired"] == "err=tls_certificate_expired");
	CHECK(Contains(r["verify_off"],"status=200|reason=OK|"));
	CHECK(r["system_roots"] == "err=tls_untrusted"); //the test CA is not in the system's store: it was used, and not given
	CHECK(Contains(r["system_roots_and_ca"],"status=200|reason=OK|"));
	CHECK(r["no_ca"] == "ctor=invalid_options: tls.CAFile (required for https unless system_roots = true)");
	CHECK(r["no_ca_tls"] == "ctor=invalid_options: tls.CAFile (required for https unless system_roots = true)");
	INFO("to_plain_port: "+r["to_plain_port"]);
	CHECK((r["to_plain_port"] == "err=tls_handshake_failed" || r["to_plain_port"] == "err=closed"));
	CHECK(Contains(r["echo"],"status=200|reason=OK|ver=1.1|len=262144|"));
	CHECK(r["name_in_url"] == "err=tls_hostname_mismatch"); //localhost is not in the certificate's names (only tls-server.test and 127.0.0.1)
	//a warning for each use of an option that turns verification off, and no more than that
	CHECK(h.capture->Has("VerifyHostname is false, the name in the server's certificate is not checked."));
	CHECK(h.capture->Has("VerifyPeer is false, the server's certificate is not checked."));
	(void)pki;
}

TEST_CASE(SUITE("HTTPS: mutual TLS and what a rejected client certificate looks like"))
{
	LuaHarness h(std::string(Common)+R"lua(
local C = odc.Config
function Enable()
	local s = assert(odc.HTTPServer{ port = C.port, tls = { CertFile = C.cert, KeyFile = C.key, CAFile = C.ca } })
	assert(s:route("GET", "^/secure$", function(req, res) res:write(200, "mutual") end))
	assert(s:start())
	srv = s
	local u = "https://127.0.0.1:"..C.port
	expect(5)
	call("with_cert", { url = u, tls = { CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.client_key } }, { path = "/secure" })
	call("no_cert", { url = u, tls = { CAFile = C.ca } }, { path = "/secure" })
	call("other_ca_cert", { url = u, tls = { CAFile = C.ca, CertFile = C.other_client_cert, KeyFile = C.other_client_key } }, { path = "/secure" })
	call("cert_without_key", { url = u, tls = { CAFile = C.ca, CertFile = C.client_cert } }, { path = "/secure" })
	call("key_without_cert", { url = u, tls = { CAFile = C.ca, KeyFile = C.client_key } }, { path = "/secure" })
end
)lua",TlsConf(20951));
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	CHECK(r["with_cert"] == "status=200|reason=OK|ver=1.1|len=6|body=mutual|content-length=6");
	INFO("no_cert: "+r["no_cert"]);
	INFO("other_ca_cert: "+r["other_ca_cert"]);
	CHECK(Rejected(r["no_cert"]));
	CHECK(Rejected(r["other_ca_cert"]));
	CHECK(r["cert_without_key"] == "ctor=invalid_options: tls.KeyFile (CertFile is given without it)");
	CHECK(r["key_without_cert"] == "ctor=invalid_options: tls.CertFile (KeyFile is given without it)");
}

TEST_CASE(SUITE("HTTPS: only the restricted options and files that can be read and no encrypted key"))
{
	auto& pki = GetPki();
	auto conf = TlsConf(20953);
	conf["missing"] = pki.dir.Path("does-not-exist.pem");
	conf["not_a_key"] = pki.dir.Write("not-a-key.pem","this is not a key\n");
	conf["not_a_ca"] = pki.dir.Write("not-a-ca.pem","this is not a certificate\n");
	LuaHarness h(R"lua(
local C = odc.Config
local function client(tls, more)
	local o = { url = "https://127.0.0.1:20953", tls = tls }
	for k, v in pairs(more or {}) do o[k] = v end
	return odc.HTTPClient(o)
end
function Enable()
	local cases =
	{
		{ "ok", function() return client{ CAFile = C.ca } end },
		{ "ok_mutual", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.client_key } end },
		{ "not_table", function() return client("x") end },
		{ "min_version", function() return client{ CAFile = C.ca, MinVersion = "1.2" } end },
		{ "ciphers", function() return client{ CAFile = C.ca, Ciphers = "HIGH" } end },
		{ "password_env", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.client_key, KeyPasswordEnv = "SECRET" } end },
		{ "key_password", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.client_key, KeyPassword = "hunter2" } end },
		{ "key_log", function() return client{ CAFile = C.ca, KeyLogFile = "keys.log" } end },
		{ "crl", function() return client{ CAFile = C.ca, CRLFile = "list.crl" } end },
		{ "allow_list", function() return client{ CAFile = C.ca, PeerCertAllowList = {} } end },
		{ "verify_type", function() return client{ CAFile = C.ca, VerifyPeer = "yes" } end },
		{ "hostname_type", function() return client{ CAFile = C.ca, VerifyHostname = 1 } end },
		{ "ca_type", function() return client{ CAFile = 5 } end },
		{ "missing_ca", function() return client{ CAFile = C.missing } end },
		{ "missing_cert", function() return client{ CAFile = C.ca, CertFile = C.missing, KeyFile = C.client_key } end },
		{ "missing_key", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.missing } end },
		{ "encrypted", function() return client{ CAFile = C.ca, CertFile = C.enc_cert, KeyFile = C.enc_key } end },
		{ "not_a_key", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.not_a_key } end },
		{ "not_a_ca", function() return client{ CAFile = C.not_a_ca } end },
		{ "mismatch", function() return client{ CAFile = C.ca, CertFile = C.client_cert, KeyFile = C.other_client_key } end },
		{ "verify_off_no_ca", function() return client({ VerifyPeer = false }) end },
		{ "roots_only", function() return client({ VerifyHostname = true }, { system_roots = true }) end },
	}
	for _, c in ipairs(cases) do
		local o, err, detail = c[2]()
		report("case_"..c[1], o and "object" or tostring(err))
	end
	report("done", "")
end
)lua",conf);
	h.Enable();
	h.Expect("done");
	auto r = Results(h);
	auto Is = [&](const std::string& name, const std::string& value)
		    {
			    INFO(name+" = "+r["case_"+name]);
			    CHECK(r["case_"+name] == value);
		    };
	auto Has = [&](const std::string& name, const std::string& text)
		     {
			     INFO(name+" = "+r["case_"+name]);
			     CHECK(Contains(r["case_"+name],text));
		     };
	Is("ok","object");
	Is("ok_mutual","object");
	Is("not_table","invalid_options: tls");
	const std::vector<std::pair<std::string,std::string>> unsupported =
	{
		{"min_version","MinVersion"},{"ciphers","Ciphers"},{"password_env","KeyPasswordEnv"},{"key_password","KeyPassword"},
		{"key_log","KeyLogFile"},{"crl","CRLFile"},{"allow_list","PeerCertAllowList"}
	};
	for(const auto& u : unsupported)
		Is(u.first,"invalid_options: "+u.second+" is not supported by the HTTP stack (use odc.TLSSocket)");
	Is("verify_type","invalid_options: tls.VerifyPeer");
	Is("hostname_type","invalid_options: tls.VerifyHostname");
	Is("ca_type","invalid_options: tls.CAFile");
	Is("missing_ca","invalid_options: tls.CAFile cannot be read");
	Is("missing_cert","invalid_options: tls.CertFile cannot be read");
	Is("missing_key","invalid_options: tls.KeyFile cannot be read");
	Has("encrypted","invalid_options: tls.KeyFile is encrypted");
	Is("not_a_key","tls config: could not use CertFile, KeyFile or CAFile");
	Is("not_a_ca","tls config: could not use CertFile, KeyFile or CAFile");
	Is("mismatch","tls config: could not use CertFile, KeyFile or CAFile");
	Is("verify_off_no_ca","object"); //nothing to verify against
	Is("roots_only","object");
	CHECK_FALSE(Contains(r["case_key_password"],"hunter2"));
}

TEST_CASE(SUITE("HTTPS: a renewed certificate is used by the next request and by a new client"))
{
	auto& pki = GetPki();
	//the client certificate in the files starts out expired
	auto expired = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -7200, -3600, false, 201}, &pki.ca);
	pki.dir.Write("client_renewed.crt",expired.certPem);
	pki.dir.Write("client_renewed.key",expired.keyPem);
	LuaHarness h(std::string(Common)+R"lua(
local C = odc.Config
local c
local function request(name, client)
	local ok, err = client:request{ method = "GET", path = "/secure", on_response = function(resp, e, detail) report(name, show(resp, e, detail)) end }
	if not ok then report(name, "request="..tostring(err)) end
end
function Enable()
	srv = assert(odc.HTTPServer{ port = C.port, tls = { CertFile = C.cert, KeyFile = C.key, CAFile = C.ca } })
	assert(srv:route("GET", "^/secure$", function(req, res) res:write(200, "mutual") end))
	assert(srv:start())
	c = assert(odc.HTTPClient{ url = "https://127.0.0.1:"..C.port, tls = { CAFile = C.ca, CertFile = C.renewed_cert, KeyFile = C.renewed_key } })
	request("before", c)
end
function Event(e, sender, cb)
	request("after", c)
	request("new_client", assert(odc.HTTPClient{ url = "https://127.0.0.1:"..C.port, tls = { CAFile = C.ca, CertFile = C.renewed_cert, KeyFile = C.renewed_key } }))
end
)lua",TlsConf(20954));
	h.Enable();
	const auto before = h.Expect("before");
	INFO("before: "+before);
	CHECK(Rejected(before));
	//renewed: the same files hold a valid certificate now
	auto renewed = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 202}, &pki.ca);
	pki.dir.Write("client_renewed.crt",renewed.certPem);
	pki.dir.Write("client_renewed.key",renewed.keyPem);
	h.Send("again");
	CHECK(Contains(h.Expect("after"),"status=200|reason=OK|ver=1.1|len=6|body=mutual|"));
	CHECK(Contains(h.Expect("new_client"),"status=200|reason=OK|"));
}

TEST_CASE(SUITE("HTTPS: a script that closes its clients in Disable() with a request in flight to a silent TLS server"))
{
	auto& pki = GetPki();
	//a server that accepts the connection and says nothing: the handshake never finishes
	RawServer silent(20955);
	silent.Hang("/never");
	LuaHarness h(std::string(Common)+R"lua(
function Enable()
	c = assert(odc.HTTPClient{ url = "https://127.0.0.1:20955", tls = { CAFile = odc.Config.ca }, timeout_ms = 0, connect_timeout_ms = 0 })
	assert(c:request{ method = "GET", path = "/never", on_response = function(resp, err) if err ~= "aborted" then report("late", tostring(err)) end end })
	d = assert(odc.HTTPClient{ url = "https://127.0.0.1:20955", tls = { CAFile = odc.Config.ca }, timeout_ms = 0, connect_timeout_ms = 400 })
	assert(d:request{ method = "GET", path = "/never", on_response = function(resp, err) report("connect_timeout", tostring(err)) end })
	report("started", "")
end
function Disable()
	c:close()
	d:close()
	c = nil
	d = nil
	collectgarbage()
	collectgarbage()
end
)lua",TlsConf(20955));
	h.Enable();
	h.Expect("started");
	//the handshake against a server that says nothing: the connect timeout covers it
	CHECK(h.Expect("connect_timeout") == "timeout");
	h.Disable();
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	CHECK(h.NoReport("late",500ms));
	(void)pki;
}

#endif //ODC_HTTPS_TESTS
