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
 * NetHostsTest.cpp
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  The network objects in the Lua hosts other than LuaPort (which has its own tests): LuaTransform, LuaLogSink and LuaUICommander, each loaded from
 *  its library as opendatacon loads it. A script opens objects; the host is torn down with them open (no hang, the peer sees the connection end,
 *  nothing leaked), and NetEnabled decides whether the script has them at all. TCP ports 21050 to 21069.
 */
#include "../ThreadPool.h"
#include <LuaLogSink.h>
#include <LuaTransform.h>
#include <LuaUICommander.h>
#include <NetWrappers.h>
#include <catch.hpp>
#include <opendatacon/util.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#define SUITE(name) "NetHosts - " name

extern spdlog::level::level_enum log_level;

namespace
{
using namespace std::chrono_literals;
namespace fs = std::filesystem;

//every log line, to look for what the scripts and hosts said
class CaptureSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
	bool Has(const std::string& text)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& m : messages)
			if(m.find(text) != std::string::npos)
				return true;
		return false;
	}
protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		messages.emplace_back(msg.payload.data(),msg.payload.size());
	}
	void flush_() override {}
private:
	std::mutex mtx;
	std::deque<std::string> messages;
};

//the loggers the hosts use, with a capture of what they log
struct Logging
{
	std::shared_ptr<CaptureSink> capture = std::make_shared<CaptureSink>();
	std::shared_ptr<spdlog::sinks::sink> console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	Logging()
	{
		console->set_level(log_level);
		for(const char* name : {"opendatacon","LuaTransform","LuaUICommander"})
			Register(name);
	}
	//(re)register a logger, with extra sinks as well: replacing a logger is thread safe, changing the sinks of one in use is not
	void Register(const std::string& name, const std::vector<spdlog::sink_ptr>& extra = {})
	{
		std::vector<spdlog::sink_ptr> sinks{console,capture};
		sinks.insert(sinks.end(),extra.begin(),extra.end());
		auto logger = std::make_shared<spdlog::logger>(name,sinks.begin(),sinks.end());
		logger->set_level(spdlog::level::trace);
		odc::spdlog_drop(name);
		odc::spdlog_register_logger(logger);
	}
	~Logging()
	{
		odc::spdlog_flush_all();
		odc::spdlog_drop_all();
	}
};

bool WaitFor(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 15000ms)
{
	const auto end = std::chrono::steady_clock::now()+timeout;
	while(!cond())
	{
		if(std::chrono::steady_clock::now() > end)
			return false;
		std::this_thread::sleep_for(5ms);
	}
	return true;
}

//A TCP listener on loopback: takes connections, keeps what they send, and counts those the other end has closed
class Listener
{
public:
	explicit Listener(uint16_t port): acceptor(io,asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port))
	{
		Accept();
		thread = std::thread([this]{ io.run(); });
	}
	~Listener()
	{
		asio::post(io,[this]{ std::error_code ec; acceptor.close(ec); for(auto& c : conns) c->sock.close(ec);});
		guard.reset();
		thread.join();
	}
	size_t Accepted() const { return accepted.load(); }
	size_t Ended() const { return ended.load(); }
	std::string Data()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return data;
	}
private:
	struct Conn
	{
		explicit Conn(asio::io_context& io): sock(io) {}
		asio::ip::tcp::socket sock;
		char buf[4096];
	};
	void Accept()
	{
		auto c = std::make_shared<Conn>(io);
		acceptor.async_accept(c->sock,[this,c](const std::error_code& ec)
			{
				if(ec)
					return;
				conns.push_back(c);
				++accepted;
				Read(c);
				Accept();
			});
	}
	void Read(const std::shared_ptr<Conn>& c)
	{
		c->sock.async_read_some(asio::buffer(c->buf),[this,c](const std::error_code& ec, size_t n)
			{
				{
					std::lock_guard<std::mutex> lock(mtx);
					data.append(c->buf,n);
				}
				if(ec)
				{
					++ended;
					return;
				}
				Read(c);
			});
	}
	asio::io_context io;
	asio::executor_work_guard<asio::io_context::executor_type> guard = asio::make_work_guard(io);
	asio::ip::tcp::acceptor acceptor;
	std::vector<std::shared_ptr<Conn>> conns;
	std::atomic<size_t> accepted{0}, ended{0};
	std::mutex mtx;
	std::string data;
	std::thread thread;
};

//a plain TCP client of a script's server: tells whether the server ended the connection
class Client
{
public:
	explicit Client(uint16_t port): sock(io)
	{
		const auto end = std::chrono::steady_clock::now()+10s;
		std::error_code ec;
		do
		{
			sock.close(ec);
			sock.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port),ec);
			if(ec)
				std::this_thread::sleep_for(10ms);
		} while(ec && std::chrono::steady_clock::now() < end);
		connected = !ec;
	}
	bool Connected() const { return connected; }
	bool Ended(std::chrono::milliseconds timeout = 10000ms)
	{
		std::error_code ec;
		sock.non_blocking(true,ec);
		const auto end = std::chrono::steady_clock::now()+timeout;
		char buf[256];
		while(std::chrono::steady_clock::now() < end)
		{
			sock.read_some(asio::buffer(buf),ec);
			if(ec && ec != asio::error::would_block)
				return true;
			std::this_thread::sleep_for(5ms);
		}
		return false;
	}
private:
	asio::io_context io;
	asio::ip::tcp::socket sock;
	bool connected = false;
};

struct TempDir
{
	fs::path root;
	TempDir()
	{
		static std::atomic<unsigned> n{0};
		root = fs::temp_directory_path()/("odc_nethosts_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"_"+std::to_string(n++));
		fs::create_directories(root);
	}
	~TempDir()
	{
		std::error_code ec;
		fs::remove_all(root,ec);
	}
	std::string Write(const std::string& rel, const std::string& content)
	{
		auto p = root/rel;
		fs::create_directories(p.parent_path());
		std::ofstream(p) << content;
		return p.string();
	}
};

//a host library, loaded as opendatacon loads it
class Library
{
public:
	explicit Library(const std::string& name): handle(LoadModule(GetLibFileName(name)))
	{
		REQUIRE(handle);
	}
	~Library()
	{
		UnLoadModule(handle);
	}
	template<typename F>
	F Symbol(const std::string& name)
	{
		auto f = reinterpret_cast<F>(LoadSymbol(handle,name));
		REQUIRE(f);
		return f;
	}
private:
	module_ptr handle;
};

template<typename F>
long long MsToRun(F&& f)
{
	const auto start = std::chrono::steady_clock::now();
	f();
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
}
}

// ---------------------------------------------------------------------------------------------------------------
// LuaTransform
// ---------------------------------------------------------------------------------------------------------------

namespace
{
const char* const TransformScript = R"lua(
function Enable()
	local P = odc.Parameters
	if odc.TCPSocket == nil then odc.log.info("transform: no network") return end
	odc.log.info("transform: network")
	client = odc.TCPSocket{ host = "127.0.0.1", port = P.Port, auto_reopen = true, retry_ms = 50,
		on_open = function(sock) sock:write("hello from the transform") end }
	assert(client:open())
	server = odc.TCPSocket{ host = "127.0.0.1", port = P.ServerPort, server = true, auto_reopen = true }
	assert(server:open())
end
function Event(event, allow) allow(event) end
)lua";
}

TEST_CASE(SUITE("LuaTransform: a transform destroyed with its objects open tears them down"))
{
	Logging logging;
	ThreadPool pool(2);
	TempDir dir;
	Library lib("LuaTransform");
	auto make = lib.Symbol<odc::Transform*(*)(const std::string&, const Json::Value&)>("new_LuaTransform");
	auto destroy = lib.Symbol<void (*)(odc::Transform*)>("delete_LuaTransform");

	Listener peer(21050);
	Json::Value params;
	params["LuaFile"] = dir.Write("tx.lua",TransformScript);
	params["Port"] = 21050;
	params["ServerPort"] = 21051;
	odc::Transform* tx = make("NetTransform",params);
	REQUIRE(tx);
	tx->Enable();
	REQUIRE(WaitFor([&]{ return peer.Data() == "hello from the transform"; }));
	CHECK(logging.capture->Has("transform: network"));
	CHECK(logging.capture->Has("NetTransform: network access on"));
	Client client_of_script(21051);
	REQUIRE(client_of_script.Connected());

	//destroyed without Disable(), the objects still open
	CHECK(MsToRun([&]{ destroy(tx); }) < 10000);
	CHECK(WaitFor([&]{ return peer.Ended() == 1; }));
	CHECK(client_of_script.Ended());
	CHECK(WaitFor([]{ return NetObjectCount() == 0; }));
}

TEST_CASE(SUITE("LuaTransform: NetEnabled false leaves the script without the network objects"))
{
	Logging logging;
	ThreadPool pool(2);
	TempDir dir;
	Library lib("LuaTransform");
	auto make = lib.Symbol<odc::Transform*(*)(const std::string&, const Json::Value&)>("new_LuaTransform");
	auto destroy = lib.Symbol<void (*)(odc::Transform*)>("delete_LuaTransform");

	Json::Value params;
	params["LuaFile"] = dir.Write("tx.lua",TransformScript);
	params["Port"] = 21052;
	params["ServerPort"] = 21053;
	params["NetEnabled"] = false;
	odc::Transform* tx = make("NoNetTransform",params);
	REQUIRE(tx);
	tx->Enable();
	CHECK(WaitFor([&]{ return logging.capture->Has("transform: no network"); }));
	CHECK(logging.capture->Has("NoNetTransform: network access off"));
	destroy(tx);

	//and a NetEnabled that isn't valid is an error, and no network
	params["NetEnabled"] = "sometimes";
	tx = make("BadNetTransform",params);
	REQUIRE(tx);
	tx->Enable();
	CHECK(WaitFor([&]{ return logging.capture->Has("BadNetTransform: \"NetEnabled\" must be"); }));
	destroy(tx);
}

// ---------------------------------------------------------------------------------------------------------------
// LuaLogSink
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("LuaLogSink: a sink that sends log lines over TCP, without its own lines coming back, torn down with the socket open"))
{
	Logging logging;
	ThreadPool pool(2);
	TempDir dir;
	Library lib("LuaLogSink");
	auto make = lib.Symbol<spdlog::sinks::sink*(*)(const std::string&, const std::string&, const Json::Value&)>("new_LuaLogSink");
	auto destroy = lib.Symbol<void (*)(spdlog::sinks::sink*)>("delete_LuaLogSink");

	Listener peer(21055);
	Json::Value config;
	config["Port"] = 21055;
	const auto file = dir.Write("sink.lua",R"lua(
local C = odc.Config
client = odc.TCPSocket{ host = "127.0.0.1", port = C.Port, auto_reopen = true, retry_ms = 50 }
assert(client:open())
function LogSink(time, logger, level, msg)
	if client:is_open() then client:write(msg.."\n") end
end
)lua");
	std::shared_ptr<spdlog::sinks::sink> sink(make("NetSink",file,config),destroy);
	REQUIRE(sink);
	REQUIRE(WaitFor([&]{ return peer.Accepted() == 1; }));
	std::this_thread::sleep_for(200ms); //(the script sees the connection up)

	//the sink is on the logger the network objects log to as well (at every level), and on one of the test's own
	auto test_logger = std::make_shared<spdlog::logger>("SinkTest",sink);
	test_logger->set_level(spdlog::level::trace);
	logging.Register("opendatacon",{sink});
	for(int i = 1; i <= 5; i++)
		test_logger->info("line {}",i);
	REQUIRE(WaitFor([&]{ return peer.Data().find("line 5\n") != std::string::npos; }));
	std::this_thread::sleep_for(500ms);
	const auto data = peer.Data();
	CHECK(data.find("line 1\nline 2\nline 3\nline 4\nline 5\n") != std::string::npos);
	CHECK(data.find("NetSink") == std::string::npos); //(what the sink's own objects log is not given to it)
	CHECK(data.size() < 4096);

	logging.Register("opendatacon");
	test_logger.reset();
	CHECK(MsToRun([&]{ sink.reset(); }) < 10000);
	CHECK(WaitFor([&]{ return peer.Ended() == 1; }));
	CHECK(WaitFor([]{ return NetObjectCount() == 0; }));
}

// ---------------------------------------------------------------------------------------------------------------
// LuaUICommander
// ---------------------------------------------------------------------------------------------------------------

namespace
{
//SendCommands(port, mode): opens a client the first time, then every 50 ms. mode "finish": returns -1 at once, leaving it open
const char* const CommandScript = R"lua(
local started = false
function SendCommands(port, mode)
	if not started then
		started = true
		if odc.TCPSocket == nil then odc.UIMessage("no network") return -1 end
		odc.UIMessage("network")
		client = odc.TCPSocket{ host = "127.0.0.1", port = tonumber(port), auto_reopen = true, retry_ms = 50,
			on_open = function(sock) sock:write("hello from the script") end }
		assert(client:open())
		if mode == "finish" then return -1 end
	end
	return 50
end
function Cancel()
	if client then client:close() end
	odc.UIMessage("cancelled")
end
)lua";

struct Commander
{
	Library lib{"LuaUICommander"};
	LuaUICommander* cmdr = nullptr;
	void (*destroy)(LuaUICommander*) = nullptr;
	explicit Commander(const Json::Value& overrides)
	{
		auto make = lib.Symbol<LuaUICommander*(*)(const std::string&, const std::string&, const Json::Value&)>("new_LuaUICommanderPlugin");
		destroy = lib.Symbol<void (*)(LuaUICommander*)>("delete_LuaUICommanderPlugin");
		cmdr = make("Commander","",overrides);
		REQUIRE(cmdr);
	}
	~Commander()
	{
		if(cmdr)
			destroy(cmdr);
	}
	Json::Value Run(const std::string& cmd, const std::vector<std::string>& args)
	{
		ParamCollection params;
		for(size_t i = 0; i < args.size(); i++)
			params[std::to_string(i)] = args[i];
		return static_cast<IUIResponder*>(cmdr)->ExecuteCommand(cmd,params);
	}
	bool File(const std::string& file, const std::string& id, const std::vector<std::string>& args = {})
	{
		std::vector<std::string> a{"\""+file+"\"",id};
		a.insert(a.end(),args.begin(),args.end());
		return Run("ExecuteFile",a)["RESULT"].asString().find("SUCCESS") != std::string::npos;
	}
	bool Code(const std::string& code, const std::string& id, const std::vector<std::string>& args = {})
	{
		std::vector<std::string> a{odc::b64encode(code),id};
		a.insert(a.end(),args.begin(),args.end());
		return Run("Base64Execute",a)["RESULT"].asString().find("SUCCESS") != std::string::npos;
	}
	bool Said(const std::string& id, const std::string& message)
	{
		const auto status = Run("StatExecution",{id});
		for(const auto& m : status["Messages"])
			if(m.asString() == message)
				return true;
		return false;
	}
	bool Running(const std::string& id)
	{
		return Run("StatExecution",{id})["Execution status"].asString() == "Running";
	}
};
}

TEST_CASE(SUITE("LuaUICommander: Cancel() closes the script's objects; a script that finishes leaves them to the purge; the plugin is torn down with them open"))
{
	Logging logging;
	ThreadPool pool(2);
	TempDir dir;
	const auto file = dir.Write("cmd.lua",CommandScript);
	Listener peer(21060);
	{
		Commander c(Json::Value::nullSingleton());

		//cancelled: Cancel() closes the client, the peer sees it, while the plugin goes on
		REQUIRE(c.File(file,"first",{"21060"}));
		REQUIRE(WaitFor([&]{ return peer.Data() == "hello from the script"; }));
		CHECK(c.Said("first","network"));
		c.Run("CancelExecution",{"first"});
		CHECK(WaitFor([&]{ return peer.Ended() == 1; }));
		CHECK(WaitFor([&]{ return c.Said("first","cancelled"); }));

		//finished without closing: the client stays up until the script is purged, at the next Execute
		REQUIRE(c.File(file,"second",{"21060","finish"}));
		REQUIRE(WaitFor([&]{ return peer.Accepted() == 2; }));
		REQUIRE(WaitFor([&]{ return !c.Running("second"); }));
		std::this_thread::sleep_for(300ms);
		CHECK(peer.Ended() == 1);
		REQUIRE(c.Code("function SendCommands() return -1 end","third"));
		CHECK(WaitFor([&]{ return peer.Ended() == 2; }));

		//running, with its client open, when the plugin is destroyed
		REQUIRE(c.File(file,"fourth",{"21060"}));
		REQUIRE(WaitFor([&]{ return peer.Accepted() == 3; }));
		CHECK(MsToRun([&]{ c.destroy(c.cmdr); c.cmdr = nullptr; }) < 10000);
		CHECK(WaitFor([&]{ return peer.Ended() == 3; }));
	}
	CHECK(WaitFor([]{ return NetObjectCount() == 0; }));
}

TEST_CASE(SUITE("LuaUICommander: NetEnabled decides which scripts get the network objects"))
{
	Logging logging;
	ThreadPool pool(2);
	TempDir dir;
	const auto approved = dir.Write("approved/cmd.lua",CommandScript);
	const auto other = dir.Write("other/cmd.lua",CommandScript);
	Listener peer(21062);

	SECTION("the default, FILE_SCRIPTS_ONLY: a file, not code given directly")
	{
		Commander c(Json::Value::nullSingleton());
		REQUIRE(c.File(other,"file",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("file","network"); }));
		REQUIRE(c.Code(CommandScript,"code",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("code","no network"); }));
	}
	SECTION("true: code given directly as well")
	{
		Json::Value o;
		o["NetEnabled"] = true;
		Commander c(o);
		REQUIRE(c.Code(CommandScript,"code",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("code","network"); }));
	}
	SECTION("false: not even a file")
	{
		Json::Value o;
		o["NetEnabled"] = false;
		Commander c(o);
		REQUIRE(c.File(other,"file",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("file","no network"); }));
	}
	SECTION("an allow list: only the files in it")
	{
		Json::Value o;
		o["NetEnabled"]["Allow"].append((dir.root/"approved").string());
		Commander c(o);
		REQUIRE(c.File(approved,"approved",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("approved","network"); }));
		REQUIRE(c.File(other,"other",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("other","no network"); }));
		REQUIRE(c.Code(CommandScript,"code",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("code","no network"); }));
	}
	SECTION("a value that isn't valid: an error, and no network")
	{
		Json::Value o;
		o["NetEnabled"] = "FILE_SCRIPTS";
		Commander c(o);
		CHECK(logging.capture->Has("Commander: \"NetEnabled\" must be"));
		REQUIRE(c.File(other,"file",{"21062","finish"}));
		CHECK(WaitFor([&]{ return c.Said("file","no network"); }));
	}
	CHECK(WaitFor([]{ return NetObjectCount() == 0; }));
}
