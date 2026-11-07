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
 * NetTcpTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Tests of odc.TCPSocket: real Lua scripts in a LuaPort, against loopback peers. TCP ports 20600 to 20699.
 */
#include "NetTestHelpers.h"
#include <future>
#include <sstream>

#define SUITE(name) "LuaNetTcpTestSuite - " name

using namespace nettest;

namespace
{
std::string Join(const std::vector<std::string>& v, const std::string& sep)
{
	std::string s;
	for(const auto& e : v)
		s += (s.empty() ? "" : sep)+e;
	return s;
}
//a result string "a,b,c" split on commas
std::vector<std::string> Split(const std::string& s, char sep)
{
	std::vector<std::string> v;
	std::stringstream ss(s);
	std::string item;
	while(std::getline(ss,item,sep))
		v.push_back(item);
	return v;
}
}

TEST_CASE(SUITE("Echo, and the order of the callbacks"))
{
	Peer peer(20600,Peer::Echo);
	Json::Value conf;
	conf["port"] = 20600;
	LuaHarness h(R"lua(
local order = {}
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
		on_open = function(sock)
			order[#order+1] = "open"
			report("open", tostring(sock == s)..","..sock:serial()..","..tostring(sock:is_open()))
			assert(sock:write("hello"))
		end,
		on_data = function(sock, d)
			got = (got or "")..d
			if #got == 5 then
				report("echo", got)
				order[#order+1] = "before_close"
				assert(sock:close())
				order[#order+1] = "after_close"
				report("closed_returned", tostring(sock:is_open())..","..sock:serial())
			end
		end,
		on_close = function(sock, reason)
			order[#order+1] = "on_close"
			report("close", reason..","..tostring(sock:is_open()))
			report("order", table.concat(order, ","))
		end }
	assert(s:open())
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("open") == "true,1,true");
	CHECK(h.Expect("echo") == "hello");
	CHECK(h.Expect("closed_returned") == "false,0");
	CHECK(h.Expect("close") == "closed,false");
	//on_close is called after close() has returned, never from inside it
	CHECK(h.Expect("order") == "open,before_close,after_close,on_close");
}

TEST_CASE(SUITE("Callbacks set with on()"))
{
	Peer peer(20620,Peer::Echo);
	Json::Value conf;
	conf["port"] = 20620;
	LuaHarness h(R"lua(
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port }
	assert(s:on("open", function(sock) sock:write("abc") end))
	assert(s:on("data", function(sock, d) report("data", d); sock:close() end))
	assert(s:on("on_close", function(sock, reason) report("close", reason) end))
	assert(s:open())
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("data") == "abc");
	CHECK(h.Expect("close") == "closed");
}

TEST_CASE(SUITE("A large write is delivered whole and in order, and close() flushes it"))
{
	std::mutex mtx;
	std::string received;
	std::promise<void> done;
	Peer peer(20601,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r);
			{
				std::lock_guard<std::mutex> lock(mtx);
				received = std::move(r);
			}
			done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20601;
	LuaHarness h(R"lua(
function Enable()
	local t = {}
	for j = 0, 255 do t[#t+1] = string.char(j) end
	local blk = table.concat(t):rep(4096) --1MiB of every byte value
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
		on_open = function(sock)
			for i = 1, 8 do
				local ok, err = sock:write(string.char(i)..blk:sub(2))
				if not ok then report("write_failed", err) end
			end
			sock:close()
		end,
		on_close = function(sock, reason) report("close", reason) end }
	s:open()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("close") == "closed");
	REQUIRE(done.get_future().wait_for(20s) == std::future_status::ready);

	std::string block;
	for(int j = 0; j < 256; j++)
		block += static_cast<char>(j);
	std::string mib;
	for(int i = 0; i < 4096; i++)
		mib += block;
	std::string expected;
	for(int i = 1; i <= 8; i++)
	{
		expected += static_cast<char>(i);
		expected.append(mib,1,std::string::npos);
	}
	std::lock_guard<std::mutex> lock(mtx);
	REQUIRE(received.size() == expected.size());
	CHECK(received == expected);
}

TEST_CASE(SUITE("Data arrives complete whatever the splits and coalescing"))
{
	const size_t N = 8*1024*1024+5000;
	Peer peer(20602,[&](Peer::Socket& s, int)
		{
			const std::string data = Pattern(N);
			std::error_code ec;
			size_t pos = 0;
			//a lot of tiny writes (which the stack may coalesce), then pieces of awkward sizes
			for(int i = 0; i < 1000 && pos < N; i++)
			{
				asio::write(s,asio::buffer(data.data()+pos,10),ec);
				pos += 10;
			}
			const size_t sizes[] = {1,7,1000,65537,3,200000,13};
			for(size_t i = 0; pos < N && !ec; i++)
			{
				const size_t n = std::min(sizes[i%7],N-pos);
				asio::write(s,asio::buffer(data.data()+pos,n),ec);
				pos += n;
			}
			char c;
			s.read_some(asio::buffer(&c,1),ec); //until the script closes
		});
	Json::Value conf;
	conf["port"] = 20602;
	conf["n"] = static_cast<Json::UInt64>(N);
	LuaHarness h(R"lua(
function Enable()
	local N = odc.Config.n
	local pat = {}
	for j = 0, 250 do pat[#pat+1] = string.char(j) end
	local expected = table.concat(pat):rep(N // 251 + 1):sub(1, N)
	local parts, total, chunks = {}, 0, 0
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
		on_data = function(sock, d)
			parts[#parts+1] = d
			total = total + #d
			chunks = chunks + 1
			if total == N then
				report("result", tostring(table.concat(parts) == expected)..","..tostring(chunks > 1))
				sock:close()
			end
		end,
		on_close = function(sock, reason) report("close", reason) end }
	s:open()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("result") == "true,true");
	CHECK(h.Expect("close") == "closed");
}

TEST_CASE(SUITE("auto_reopen after the peer closes, with a new serial"))
{
	std::mutex mtx;
	std::string second_received;
	std::promise<void> done;
	Peer peer(20603,[&](Peer::Socket& s, int index)
		{
			std::error_code ec;
			if(index == 0)
			{
				asio::write(s,asio::buffer(std::string("one")),ec);
				return; //closes
			}
			std::string r;
			Peer::ReadAll(s,r);
			std::lock_guard<std::mutex> lock(mtx);
			second_received = r;
			done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20603;
	LuaHarness h(R"lua(
local serials, reasons, data = {}, {}, {}
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, auto_reopen = true, retry_ms = 100,
		on_open = function(sock)
			serials[#serials+1] = sock:serial()
			if sock:serial() == 2 then
				sock:write("two")
				odc.msTimerCallback(200, function() sock:close() end)
			end
		end,
		on_data = function(sock, d) data[#data+1] = d end,
		on_close = function(sock, reason)
			reasons[#reasons+1] = reason
			if #reasons == 2 then
				report("result", table.concat(serials, ",").." "..table.concat(reasons, ",").." "..table.concat(data, ","))
			end
		end }
	s:open()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("result") == "1,2 closed,closed one");
	REQUIRE(done.get_future().wait_for(10s) == std::future_status::ready);
	std::lock_guard<std::mutex> lock(mtx);
	CHECK(second_received == "two");
}

TEST_CASE(SUITE("write_to with a stale serial writes nothing"))
{
	std::mutex mtx;
	std::string second_received;
	std::promise<void> done;
	Peer peer(20604,[&](Peer::Socket& s, int index)
		{
			if(index == 0)
				return; //closes at once
			std::string r;
			Peer::ReadAll(s,r);
			std::lock_guard<std::mutex> lock(mtx);
			second_received = r;
			done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20604;
	LuaHarness h(R"lua(
local first
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, auto_reopen = true, retry_ms = 100,
		on_open = function(sock)
			if not first then first = sock:serial() return end
			local ok, err = sock:write_to(first, "stale")
			report("stale", tostring(ok)..","..tostring(err))
			report("fresh", tostring(sock:write_to(sock:serial(), "fresh")))
			local bad, berr = sock:write_to(-1, "x")
			report("bad_serial", tostring(bad)..","..tostring(berr))
			odc.msTimerCallback(200, function() sock:close() end)
		end }
	s:open()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("stale") == "nil,closed");
	CHECK(h.Expect("fresh") == "true");
	CHECK(h.Expect("bad_serial") == "nil,invalid_options: serial");
	REQUIRE(done.get_future().wait_for(10s) == std::future_status::ready);
	std::lock_guard<std::mutex> lock(mtx);
	CHECK(second_received == "fresh");
}

TEST_CASE(SUITE("connect_timeout_ms ends an attempt that does not complete"))
{
	//192.0.2.0/24 is reserved for documentation: nothing answers, unless this host has no route at all, when the error is immediate
	LuaHarness h(R"lua(
function Enable()
	local t0 = odc.msSinceEpoch()
	s = odc.TCPSocket{ host = "192.0.2.1", port = 9, connect_timeout_ms = 300,
		on_open = function() report("open", "") end,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail)..","..(odc.msSinceEpoch()-t0)) end }
	s:open()
end
)lua");
	h.Enable();
	auto parts = Split(h.Expect("close",30s),',');
	REQUIRE(parts.size() == 3);
	if(parts[0] == "timeout")
	{
		CHECK(parts[1] == "connect_timeout_ms");
		CHECK(std::stoi(parts[2]) >= 290);
		CHECK(std::stoi(parts[2]) < 5000);
	}
	else
		WARN("This host has no route to the documentation network, so the connect failed at once with '"+parts[0]+"': the timeout is not exercised.");
	CHECK(h.NoReport("open",200ms));
}

TEST_CASE(SUITE("connect_timeout_ms with auto_reopen reports each timeout and retries"))
{
	LuaHarness h(R"lua(
local errors = {}
function Enable()
	s = odc.TCPSocket{ host = "192.0.2.1", port = 9, connect_timeout_ms = 300, auto_reopen = true, retry_ms = 50,
		on_close = function() report("unexpected_close", "") end,
		on_error = function(sock, reason, detail)
			errors[#errors+1] = reason
			if #errors == 3 then
				report("errors", table.concat(errors, ","))
				sock:close()
			end
		end }
	s:open()
end
)lua");
	h.Enable();
	auto errors = Split(h.Expect("errors",40s),',');
	REQUIRE(errors.size() == 3);
	if(errors[0] == "timeout")
	{
		CHECK(errors[1] == "timeout");
		CHECK(errors[2] == "timeout");
	}
	else
		WARN("This host has no route to the documentation network, so the connect failed at once with '"+errors[0]+"': the timeout is not exercised.");
	//a close() with no connection up is not an event
	CHECK(h.NoReport("unexpected_close",500ms));
}

TEST_CASE(SUITE("idle_timeout_ms closes a connection with no traffic"))
{
	Peer peer(20605,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r);
		});
	Json::Value conf;
	conf["port"] = 20605;
	LuaHarness h(R"lua(
function Enable()
	local t0 = odc.msSinceEpoch()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, idle_timeout_ms = 300,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail)..","..(odc.msSinceEpoch()-t0)) end }
	s:open()
end
)lua",conf);
	h.Enable();
	auto parts = Split(h.Expect("close"),',');
	REQUIRE(parts.size() == 3);
	CHECK(parts[0] == "timeout");
	CHECK(parts[1] == "idle_timeout_ms");
	CHECK(std::stoi(parts[2]) >= 290);
	CHECK(std::stoi(parts[2]) < 5000);
}

TEST_CASE(SUITE("idle_timeout_ms is reset by data written"))
{
	std::mutex mtx;
	size_t received = 0;
	Peer peer(20606,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r);
			std::lock_guard<std::mutex> lock(mtx);
			received = r.size();
		});
	Json::Value conf;
	conf["port"] = 20606;
	LuaHarness h(R"lua(
function Enable()
	local t0, writes = odc.msSinceEpoch(), 0
	local function tick(sock)
		if writes < 8 and sock:is_open() then
			writes = writes + 1
			sock:write("x")
			odc.msTimerCallback(100, function() tick(sock) end)
		end
	end
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, idle_timeout_ms = 400,
		on_open = function(sock) tick(sock) end,
		on_close = function(sock, reason) report("close", reason..","..writes..","..(odc.msSinceEpoch()-t0)) end }
	s:open()
end
)lua",conf);
	h.Enable();
	auto parts = Split(h.Expect("close"),',');
	REQUIRE(parts.size() == 3);
	CHECK(parts[0] == "timeout");
	CHECK(parts[1] == "8"); //the writes kept the connection open
	CHECK(std::stoi(parts[2]) >= 7*100+350);
	REQUIRE(WaitFor([&](){ std::lock_guard<std::mutex> lock(mtx); return received == 8; }));
}

TEST_CASE(SUITE("idle_timeout_ms is reset by data read"))
{
	Peer peer(20607,[&](Peer::Socket& s, int)
		{
			std::error_code ec;
			for(int i = 0; i < 8 && !ec; i++)
			{
				asio::write(s,asio::buffer(std::string("y")),ec);
				std::this_thread::sleep_for(100ms);
			}
			std::string r;
			Peer::ReadAll(s,r);
		});
	Json::Value conf;
	conf["port"] = 20607;
	LuaHarness h(R"lua(
function Enable()
	local t0, bytes = odc.msSinceEpoch(), 0
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, idle_timeout_ms = 400,
		on_data = function(sock, d) bytes = bytes + #d end,
		on_close = function(sock, reason) report("close", reason..","..bytes..","..(odc.msSinceEpoch()-t0)) end }
	s:open()
end
)lua",conf);
	h.Enable();
	auto parts = Split(h.Expect("close"),',');
	REQUIRE(parts.size() == 3);
	CHECK(parts[0] == "timeout");
	CHECK(parts[1] == "8");
	CHECK(std::stoi(parts[2]) >= 7*100+350);
}

TEST_CASE(SUITE("Server mode takes a second client only after the first has ended"))
{
	Json::Value conf;
	conf["port"] = 20608;
	LuaHarness h(R"lua(
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, server = true, auto_reopen = true, retry_ms = 50,
		on_open = function(sock) report("open", sock:serial()) end,
		on_data = function(sock, d) sock:write(d) end,
		on_close = function(sock, reason) report("close", sock:serial()..","..reason) end }
	assert(s:open())
end
)lua",conf);
	h.Enable();
	asio::io_context io;
	asio::ip::tcp::socket c1(io);
	REQUIRE(ConnectTo(io,c1,20608));
	CHECK(h.Expect("open") == "1");
	std::error_code ec;
	asio::write(c1,asio::buffer(std::string("abc")),ec);
	char buf[3];
	asio::read(c1,asio::buffer(buf),ec);
	REQUIRE_FALSE(ec);
	CHECK(std::string(buf,3) == "abc");

	//while the first connection is up nothing listens
	asio::ip::tcp::socket c2(io);
	c2.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),20608),ec);
	CHECK(ec);

	c1.close();
	CHECK(h.Expect("close") == "0,closed");
	asio::ip::tcp::socket c3(io);
	REQUIRE(ConnectTo(io,c3,20608));
	CHECK(h.Expect("open") == "2");
	asio::write(c3,asio::buffer(std::string("xyz")),ec);
	asio::read(c3,asio::buffer(buf),ec);
	REQUIRE_FALSE(ec);
	CHECK(std::string(buf,3) == "xyz");
}

TEST_CASE(SUITE("A host name that resolves to several addresses connects to the one that works"))
{
	Peer peer(20609,Peer::Echo);
	Json::Value conf;
	conf["port"] = 20609;
	LuaHarness h(R"lua(
function Enable()
	s = odc.TCPSocket{ host = "localhost", port = odc.Config.port,
		on_open = function(sock) report("open", "") ; sock:close() end,
		on_close = function(sock, reason) report("close", reason) end }
	s:open()
end
)lua",conf);
	h.Enable();
	h.Expect("open");
	CHECK(h.Expect("close") == "closed");
}

TEST_CASE(SUITE("A connect that fails (refused, or a name that doesn't resolve) ends at connect_timeout_ms; a server that can't listen is logged"))
{
	Json::Value conf;
	conf["port"] = 20630;
	LuaHarness h(R"lua(
function Enable()
	local function failing(name, opts)
		opts.on_open = function() report(name.."_open", "") end
		opts.on_close = function(sock, reason, detail) report(name, reason..","..tostring(detail)) end
		local o = assert(odc.TCPSocket(opts))
		assert(o:open())
	end
	failing("refused", { host = "127.0.0.1", port = odc.Config.port, connect_timeout_ms = 300 })
	failing("resolve", { host = "no-such-host.invalid", port = odc.Config.port, connect_timeout_ms = 300 })
	failing("bind", { host = "192.0.2.77", port = odc.Config.port, server = true })
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("refused") == "timeout,connect_timeout_ms");
	CHECK(h.Expect("resolve",60s) == "timeout,connect_timeout_ms");
	CHECK(h.NoReport("refused_open",100ms));
	//(the details are in the log; a server has no connect timeout: it stays open)
	REQUIRE(WaitFor([&](){ return h.capture->Has("Failed to start server on 192.0.2.77"); }));
	CHECK(h.NoReport("bind",100ms));
}

TEST_CASE(SUITE("Connect timeouts with auto_reopen are reported to on_error and the object keeps trying"))
{
	Json::Value conf;
	conf["port"] = 20631;
	LuaHarness h(R"lua(
function Enable()
	local errors = {}
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, auto_reopen = true, retry_ms = 50, connect_timeout_ms = 100,
		on_close = function() report("unexpected_close", "") end,
		on_error = function(sock, reason, detail)
			errors[#errors+1] = reason
			if #errors == 3 then
				report("errors", table.concat(errors, ","))
				report("closed", tostring(sock:close()))
			end
		end }
	s:open()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("errors") == "timeout,timeout,timeout");
	CHECK(h.Expect("closed") == "true");
	CHECK(h.NoReport("unexpected_close",500ms));
}

TEST_CASE(SUITE("NetEnabled false leaves the port script without the network objects; a value that isn't valid does too"))
{
	for(const auto& value : {Json::Value(false),Json::Value("maybe")})
	{
		INFO(value.toStyledString());
		Json::Value conf;
		conf["NetEnabled"] = value;
		LuaHarness h(R"lua(
function Enable()
	report("objects", tostring(odc.TCPSocket == nil and odc.TLSSocket == nil and odc.HTTPServer == nil and odc.HTTPClient == nil and odc.HTTP == nil))
end
)lua",conf);
		h.Enable();
		CHECK(h.Expect("objects") == "true");
	}
	//(and by default, as every other test here shows, it has them)
}

TEST_CASE(SUITE("Invalid options and methods used in the wrong state"))
{
	Peer peer(20610,Peer::Echo);
	Json::Value conf;
	conf["port"] = 20610;
	LuaHarness h(R"lua(
local function show(a, b)
	local s = (type(a) == "boolean" or a == nil) and tostring(a) or "object"
	return s..":"..tostring(b)
end
function Enable()
	local H, P = "127.0.0.1", 7000
	local cases =
	{
		{ "none",         function() return odc.TCPSocket() end },
		{ "empty",        function() return odc.TCPSocket{} end },
		{ "no_port",      function() return odc.TCPSocket{ host = H } end },
		{ "port0",        function() return odc.TCPSocket{ host = H, port = 0 } end },
		{ "port_big",     function() return odc.TCPSocket{ host = H, port = 70000 } end },
		{ "port_text",    function() return odc.TCPSocket{ host = H, port = "abc" } end },
		{ "port_digits",  function() return odc.TCPSocket{ host = H, port = "7000" } end },
		{ "host_number",  function() return odc.TCPSocket{ host = 5, port = P } end },
		{ "host_empty",   function() return odc.TCPSocket{ host = "", port = P } end },
		{ "client_nohost",function() return odc.TCPSocket{ port = P } end },
		{ "server_nohost",function() return odc.TCPSocket{ port = P, server = true } end },
		{ "server_text",  function() return odc.TCPSocket{ host = H, port = P, server = "yes" } end },
		{ "reopen_num",   function() return odc.TCPSocket{ host = H, port = P, auto_reopen = 1 } end },
		{ "retry_neg",    function() return odc.TCPSocket{ host = H, port = P, retry_ms = -1 } end },
		{ "retry_big",    function() return odc.TCPSocket{ host = H, port = P, retry_ms = 70000 } end },
		{ "connect_text", function() return odc.TCPSocket{ host = H, port = P, connect_timeout_ms = "1" } end },
		{ "idle_float",   function() return odc.TCPSocket{ host = H, port = P, idle_timeout_ms = 1.5 } end },
		{ "idle_whole",   function() return odc.TCPSocket{ host = H, port = P, idle_timeout_ms = 2.0 } end },
		{ "wbl_neg",      function() return odc.TCPSocket{ host = H, port = P, write_buffer_limit = -1 } end },
		{ "drop_option",  function() return odc.TCPSocket{ host = H, port = P, drop_when_disconnected = false } end },
		{ "throttle_neg", function() return odc.TCPSocket{ host = H, port = P, throttle_bitrate = -1 } end },
		{ "ka_count",     function() return odc.TCPSocket{ host = H, port = P, keepalive_count = 256 } end },
		{ "cb_not_fn",    function() return odc.TCPSocket{ host = H, port = P, on_open = "x" } end },
		{ "typo",         function() return odc.TCPSocket{ host = H, port = P, auto_reoppen = true } end },
		{ "full",         function() return odc.TCPSocket{ host = H, port = P, server = false, auto_reopen = true, retry_ms = 10,
			connect_timeout_ms = 100, idle_timeout_ms = 100, write_buffer_limit = 10,
			throttle_bitrate = 0, throttle_chunksize = 0, throttle_writedelay_ms = 0, keepalive = true, keepalive_idle_s = 60,
			keepalive_retry_s = 5, keepalive_count = 3, on_open = function() end, on_data = function() end, on_close = function() end,
			on_error = function() end } end },
	}
	local out = {}
	for _, c in ipairs(cases) do out[#out+1] = c[1].."="..show(c[2]()) end
	report("options", table.concat(out, "|"))

	--methods
	local o = assert(odc.TCPSocket{ host = H, port = odc.Config.port, on_close = function() report("unexpected_close", "") end })
	local r = {}
	r[#r+1] = "write_before_open="..show(o:write("x"))
	r[#r+1] = "open="..show(o:open())
	r[#r+1] = "open_again="..show(o:open())
	r[#r+1] = "on_bogus="..show(o:on("bogus", function() end))
	r[#r+1] = "on_not_fn="..show(o:on("open", 5))
	r[#r+1] = "on_clear="..show(o:on("data", nil))
	r[#r+1] = "close="..show(o:close())
	r[#r+1] = "write_after_close="..show(o:write("x"))
	r[#r+1] = "open_after_close="..show(o:open())
	r[#r+1] = "on_after_close="..show(o:on("open", function() end))
	r[#r+1] = "close_again="..show(o:close())
	r[#r+1] = "state="..tostring(o:is_open())..","..o:serial()
	report("methods", table.concat(r, "|"))
end
)lua",conf);
	h.Enable();
	const std::vector<std::string> expected_options =
	{
		"none=nil:invalid_options: options",
		"empty=nil:invalid_options: host",
		"no_port=nil:invalid_options: port",
		"port0=nil:invalid_options: port",
		"port_big=nil:invalid_options: port",
		"port_text=nil:invalid_options: port",
		"port_digits=object:nil",
		"host_number=nil:invalid_options: host",
		"host_empty=nil:invalid_options: host",
		"client_nohost=nil:invalid_options: host",
		"server_nohost=object:nil",
		"server_text=nil:invalid_options: server",
		"reopen_num=nil:invalid_options: auto_reopen",
		"retry_neg=nil:invalid_options: retry_ms",
		"retry_big=nil:invalid_options: retry_ms",
		"connect_text=nil:invalid_options: connect_timeout_ms",
		"idle_float=nil:invalid_options: idle_timeout_ms",
		"idle_whole=object:nil",
		"wbl_neg=nil:invalid_options: write_buffer_limit",
		"drop_option=nil:invalid_options: drop_when_disconnected",
		"throttle_neg=nil:invalid_options: throttle_bitrate",
		"ka_count=nil:invalid_options: keepalive_count",
		"cb_not_fn=nil:invalid_options: on_open",
		"typo=nil:invalid_options: auto_reoppen",
		"full=object:nil"
	};
	CHECK(h.Expect("options") == Join(expected_options,"|"));
	const std::vector<std::string> expected_methods =
	{
		"write_before_open=nil:closed",
		"open=true:nil",
		"open_again=true:nil",
		"on_bogus=nil:invalid_options: bogus",
		"on_not_fn=nil:invalid_options: open",
		"on_clear=true:nil",
		"close=true:nil",
		"write_after_close=nil:closed",
		"open_after_close=nil:closed",
		"on_after_close=nil:closed",
		"close_again=true:nil",
		"state=false,0"
	};
	CHECK(h.Expect("methods") == Join(expected_methods,"|"));
	//closing an object with no connection up is the script's own doing: not reported
	CHECK(h.NoReport("unexpected_close",300ms));
}

TEST_CASE(SUITE("A write while not connected is refused, and never sent on a later connection"))
{
	std::mutex mtx;
	std::string received;
	std::promise<void> done;
	Peer peer(20611,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r);
			std::lock_guard<std::mutex> lock(mtx);
			received = r;
			done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20611;
	LuaHarness h(R"lua(
function Enable()
	s = assert(odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
		on_open = function(sock) sock:write("after"); sock:close() end })
	assert(s:open())
	local a, b = s:write("too early")
	report("early", tostring(a)..","..tostring(b))
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("early") == "nil,closed");
	REQUIRE(done.get_future().wait_for(10s) == std::future_status::ready);
	std::lock_guard<std::mutex> lock(mtx);
	CHECK(received == "after");
}

TEST_CASE(SUITE("An error in a callback is logged with the port name and does not stop the others"))
{
	Peer peer(20614,[&](Peer::Socket& s, int)
		{
			std::error_code ec;
			asio::write(s,asio::buffer(std::string("1")),ec);
			std::this_thread::sleep_for(150ms);
			asio::write(s,asio::buffer(std::string("2")),ec);
			std::this_thread::sleep_for(150ms);
		});
	Json::Value conf;
	conf["port"] = 20614;
	conf["refused_port"] = 20632;
	LuaHarness h(R"lua(
function Enable()
	local got = ""
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
		on_open = function(sock) report("open", "") error("open boom") end,
		on_data = function(sock, d)
			got = got..d
			report("data", d)
			error("data boom")
		end,
		on_close = function(sock, reason) report("close", reason) error("close boom") end }
	s:open()

	local errors = 0
	e = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.refused_port, auto_reopen = true, retry_ms = 50, connect_timeout_ms = 100,
		on_error = function(sock, reason)
			errors = errors + 1
			if errors == 3 then
				report("errors_continue", reason)
				sock:close()
			end
			error("error boom")
		end }
	e:open()
	f = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.refused_port, connect_timeout_ms = 100, on_close = function() error(setmetatable({}, {})) end }
	f:open()
end
)lua",conf);
	h.Enable();
	h.Expect("open");
	std::string data;
	while(data.size() < 2)
		data += h.Expect("data");
	CHECK(data == "12"); //the second chunk arrived although the first callback raised an error
	CHECK(h.Expect("close") == "closed");
	CHECK(h.Expect("errors_continue") == "timeout");
	REQUIRE(WaitFor([&](){ return h.capture->Has("close boom") && h.capture->Has("error boom") && h.capture->Has("(error object is not a string)"); }));
	CHECK(h.capture->Has("open boom"));
	CHECK(h.capture->Has("data boom"));
	CHECK(h.capture->Has("PortUnderTest: Error calling TCPSocket 127.0.0.1:20614 on_data callback"));
	CHECK(h.capture->Has("(error object is not a string)"));
}

TEST_CASE(SUITE("Collecting the last reference to an open object does not close it"))
{
	std::promise<void> release;
	auto released = release.get_future().share();
	std::promise<void> done;
	Peer peer(20615,[&](Peer::Socket& s, int)
		{
			released.wait();
			std::error_code ec;
			asio::write(s,asio::buffer(std::string("still here")),ec);
			std::string r;
			Peer::ReadAll(s,r);
			done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20615;
	LuaHarness h(R"lua(
function Enable()
	do
		local tmp = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port,
			on_open = function(sock) report("open", "") end,
			on_data = function(sock, d) report("data", d..","..tostring(sock:is_open())); sock:close() end,
			on_close = function(sock, reason) report("close", reason) end }
		tmp:open()
	end
	for i = 1, 5 do
		local never_opened = odc.TCPSocket{ host = "127.0.0.1", port = 1 }
	end
end
function Event(e, sender, cb)
	collectgarbage()
	collectgarbage()
	report("collected", "")
end
)lua",conf);
	h.Enable();
	h.Expect("open");
	h.Send("gc");
	h.Expect("collected");
	//the objects that were never opened are gone; the open one is not
	CHECK(WaitFor([](){ return NetObjectCount() == 1; }));
	release.set_value();
	CHECK(h.Expect("data") == "still here,true");
	CHECK(h.Expect("close") == "closed");
	REQUIRE(done.get_future().wait_for(10s) == std::future_status::ready);
	//closed, and now collectable
	h.Send("gc");
	h.Expect("collected");
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
}

TEST_CASE(SUITE("A script that closes its objects in Disable(): callbacks stop, sockets released, nothing leaked"))
{
	std::promise<void> client_peer_done;
	Peer peer(20616,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r); //ends when the script's socket is closed
			client_peer_done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20616;
	conf["server_port"] = 20617;
	LuaHarness h(R"lua(
local disabled = false
local function watch(name, opts)
	opts.on_open = function(sock) if disabled then report("late", name.." open") else report(name.."_open", "") end end
	opts.on_data = function(sock, d) if disabled then report("late", name.." data") end end
	--(the on_close that answers the script's own close() is not late)
	opts.on_error = function(sock, r) if disabled then report("late", name.." error") end end
	return assert(odc.TCPSocket(opts))
end
function Enable()
	disabled = false
	c = watch("client", { host = "127.0.0.1", port = odc.Config.port, auto_reopen = true, retry_ms = 50 })
	srv = watch("server", { host = "127.0.0.1", port = odc.Config.server_port, server = true, auto_reopen = true })
	hang = watch("hang", { host = "192.0.2.1", port = 9, auto_reopen = true, connect_timeout_ms = 100 })
	c:open()
	srv:open()
	hang:open()
end
function Disable()
	disabled = true
	c:close()
	srv:close()
	hang:close()
	c, srv, hang = nil, nil, nil --the script lets go of them: they are collected once closed
	report("disabled", "")
end
)lua",conf);
	h.Enable();
	asio::io_context io;
	asio::ip::tcp::socket client_of_script(io);
	REQUIRE(ConnectTo(io,client_of_script,20617));
	h.Expect("client_open");
	h.Expect("server_open");

	h.Disable();
	h.Expect("disabled");
	//the peers see the connections end
	REQUIRE(client_peer_done.get_future().wait_for(10s) == std::future_status::ready);
	std::error_code ec;
	char c;
	REQUIRE(WaitFor([&]()
		{
			client_of_script.read_some(asio::buffer(&c,1),ec);
			return ec == asio::error::eof || ec == asio::error::connection_reset;
		}));
	//and the script's objects are destroyed
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	//and the listening port is free
	CHECK(WaitFor([&]()
		{
			asio::io_context io2;
			asio::ip::tcp::acceptor a(io2);
			std::error_code e;
			a.open(asio::ip::tcp::v4(),e);
			a.set_option(asio::socket_base::reuse_address(ReuseToCheckPortFree),e);
			a.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),20617),e);
			return !e;
		}));
	CHECK(h.NoReport("late",800ms));
}

TEST_CASE(SUITE("Disabling the port does not close what the script leaves open"))
{
	Peer peer(20619,Peer::Echo);
	Json::Value conf;
	conf["port"] = 20619;
	LuaHarness h(R"lua(
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, auto_reopen = true,
		on_open = function(sock) report("open", "") end,
		on_data = function(sock, d) report("data", d) end,
		on_close = function(sock, r) report("close", r) end }
	s:open()
end
function Disable() report("disabled", "") end
function Event(e, sender, cb) s:write("ping") end
)lua",conf);
	h.Enable();
	h.Expect("open");
	h.Disable();
	h.Expect("disabled");
	//still connected, and still calling back: closing is the script's job. (the destruction of the harness tears it down)
	h.Send("write");
	CHECK(h.Expect("data") == "ping");
	CHECK(h.NoReport("close",300ms));
}

TEST_CASE(SUITE("Destroying the port with objects open closes them without a hang or a callback"))
{
	std::promise<void> peer_done;
	Peer peer(20618,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r);
			peer_done.set_value();
		});
	Json::Value conf;
	conf["port"] = 20618;
	LuaHarness h(R"lua(
local destroyed = false
function Enable()
	s = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port, auto_reopen = true,
		on_open = function(sock) report("open", "") end,
		on_close = function(sock, r) report("close", r) end }
	s:open()
	t = odc.TCPSocket{ host = "192.0.2.1", port = 9, connect_timeout_ms = 200, auto_reopen = true,
		on_error = function() report("error", "") end }
	t:open()
end
)lua",conf);
	h.Enable();
	h.Expect("open");
	h.Destroy(); //checks that every object has been destroyed
	REQUIRE(peer_done.get_future().wait_for(10s) == std::future_status::ready);
	//the script is gone: nothing more is reported
	CHECK(h.NoReport("close",500ms));
	CHECK(h.NoReport("error",10ms));
}

TEST_CASE(SUITE("Destroying the port after the io service has stopped does not hang, and nothing is posted to it"))
{
	//what ODC does at shutdown: the io service stops (its threads end) and only then are the ports destroyed. The destructor must not post to the io service and
	//wait for the handler: it would never run
	LuaHarness h(R"lua(
function Enable() report("enabled", "") end
function Disable() report("disabled", "") end
)lua");
	h.Enable();
	h.Expect("enabled");
	h.Disable(); //queues the garbage collection of Disable_()
	h.Expect("disabled");
	h.StopIo();
	auto destroyed = std::async(std::launch::async,[&h](){ h.PUT.reset(); });
	CHECK(destroyed.wait_for(15s) == std::future_status::ready);
}

TEST_CASE(SUITE("The example in the network objects documentation runs"))
{
	std::ifstream readme(ODC_NETWORK_README);
	REQUIRE_FALSE(readme.fail());
	std::stringstream ss;
	ss << readme.rdbuf();
	const std::string text = ss.str();
	const auto marker = text.find("<!-- example:tcp -->");
	REQUIRE(marker != std::string::npos);
	const std::string fence = "```lua";
	const auto begin = text.find(fence,marker);
	REQUIRE(begin != std::string::npos);
	const auto code_begin = text.find('\n',begin)+1;
	const auto code_end = text.find("```",code_begin);
	REQUIRE(code_end != std::string::npos);

	Json::Value conf;
	conf["Port"] = 20690;
	LuaHarness h(text.substr(code_begin,code_end-code_begin),conf);
	h.Enable();
	REQUIRE(WaitFor([&](){ return h.capture->Has("client closed: closed"); }));
	CHECK(h.capture->Has("PortUnderTest: client received: hello"));
	h.Disable();
}
