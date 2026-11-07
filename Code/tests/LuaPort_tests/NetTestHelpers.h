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
 * NetTestHelpers.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Helpers for the tests of the Lua network objects: a LuaPort running a script, a port that collects what the script reports,
 *  and blocking loopback peers. Synthetic names and loopback addresses only; TCP ports 20600 to 20699.
 */
#ifndef NETTESTHELPERS_H
#define NETTESTHELPERS_H

#include "Helpers.h"
#include "../PortLoader.h"
#include <Lua/Wrappers.h>
#include <catch.hpp>
#include <opendatacon/DataPort.h>
#include <opendatacon/asio.h>
#include <spdlog/sinks/base_sink.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace nettest
{
using namespace std::chrono_literals;

//Lua code every test script starts with: report() sends a tag and a value to the test as an OctetString event
inline const char* Prelude = R"lua(
function report(tag, value)
	odc.PublishEvent({EventType = odc.EventType.OctetString, Index = 0, Payload = tag.."\t"..tostring(value)})
end
function Build() end
function Enable() end
function Disable() end
function Event(e, sender, cb) end
)lua";

//Collects the OctetString events a script publishes ("tag<TAB>value")
class SinkPort: public odc::DataPort
{
public:
	SinkPort(): odc::DataPort("Sink","",Json::Value()) {}
	void Enable() override {enabled = true;}
	void Disable() override {enabled = false;}
	void Build() override {}
	void ProcessElements(const Json::Value&) override {}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t pStatusCallback) override
	{
		if(event->GetEventType() == odc::EventType::OctetString)
		{
			auto b = event->GetPayload<odc::EventType::OctetString>();
			std::string s(static_cast<const char*>(b.data()),b.size());
			std::lock_guard<std::mutex> lock(mtx);
			reports.push_back(s);
			cv.notify_all();
		}
		(*pStatusCallback)(odc::CommandStatus::SUCCESS);
	}

	//Waits for a report with this tag that has not been taken yet; returns its value
	bool Take(const std::string& tag, std::string& value, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(mtx);
		const std::string prefix = tag+"\t";
		auto find = [&]()
				{
					for(auto it = reports.begin(); it != reports.end(); ++it)
						if(it->rfind(prefix,0) == 0)
						{
							value = it->substr(prefix.size());
							reports.erase(it);
							return true;
						}
					return false;
				};
		return cv.wait_for(lock,timeout,find);
	}
	size_t Count(const std::string& tag)
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(const auto& r : reports)
			if(r.rfind(tag+"\t",0) == 0)
				++n;
		return n;
	}
	//everything reported and not taken, in order, as "tag<TAB>value"
	std::vector<std::string> All()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return reports;
	}
private:
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<std::string> reports;
};

//Keeps what a logger is given
class CaptureSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
	std::vector<std::string> Messages()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return messages;
	}
	bool Has(const std::string& text)
	{
		for(const auto& m : Messages())
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
	std::vector<std::string> messages;
};

//Polls until the condition holds
//For checking that a port is free again: on POSIX the closed connection on that port lingers in TIME_WAIT, which SO_REUSEADDR binds over, but
//not over a listener; on Windows SO_REUSEADDR would bind over a listener as well
#ifdef _WIN32
constexpr bool ReuseToCheckPortFree = false;
#else
constexpr bool ReuseToCheckPortFree = true;
#endif

//Wakes a thread blocked in a synchronous accept() on the acceptor, which then sees it is to stop. (Closing the acceptor from another thread wakes
//it on Windows, but not on Linux.) Call it with the stop flag set, before closing the acceptor.
inline void WakeAcceptor(asio::io_context& io, asio::ip::tcp::acceptor& acceptor)
{
	std::error_code ec;
	const auto ep = acceptor.local_endpoint(ec);
	if(ec)
		return;
	asio::ip::tcp::socket poke(io);
	poke.connect(ep,ec);
}

inline bool WaitFor(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 15000ms)
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

//A LuaPort running the script (after the prelude), with a port subscribed to it to collect what the script reports
class LuaHarness
{
public:
	LuaHarness(const std::string& script, Json::Value conf = Json::Value(Json::objectValue))
	{
		TestSetup();
		auto log = odc::spdlog_get("LuaPort");
		capture = std::make_shared<CaptureSink>();
		log->sinks().push_back(capture);
		log->sinks()[0]->set_level(log_level); //the console only as the test run asks for
		log->set_level(spdlog::level::trace);

		static std::atomic<int> script_number{0};
		script_file = "NetTestScript_"+std::to_string(++script_number)+".lua";
		{
			std::ofstream f(script_file);
			REQUIRE_FALSE(f.fail());
			f << Prelude << script;
		}
		conf["LuaFile"] = script_file;

		portlib = LoadModule(GetLibFileName("LuaPort"));
		REQUIRE(portlib);
		newptr newPort = GetPortCreator(portlib,"Lua");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib,"Lua");
		REQUIRE(deletePort);
		PUT = std::shared_ptr<odc::DataPort>(newPort("PortUnderTest","",conf),deletePort);
		PUT->Subscribe(&sink,"Sink");

		ios = odc::asio_service::Get();
		work = ios->make_work();
		for(int i = 0; i < 4; i++)
			threads.emplace_back([ios{ios}](){ios->run();});

		PUT->Build();
	}

	~LuaHarness()
	{
		Destroy();
		work.reset();
		for(auto& t : threads)
			t.join();
		threads.clear();
		ios.reset();
		TestTearDown();
		UnLoadModule(portlib);
		std::remove(script_file.c_str());
	}

	void Enable()
	{
		PUT->Enable();
	}
	void Disable()
	{
		PUT->Disable();
	}
	//Sends a command to the script's Event(): an OctetString event with the command as the payload
	void Send(const std::string& cmd)
	{
		auto event = std::make_shared<odc::EventInfo>(odc::EventType::OctetString,0);
		event->SetPayload<odc::EventType::OctetString>(odc::OctetStringBuffer(std::string(cmd)));
		PUT->Event(event,"test_harness",std::make_shared<std::function<void (odc::CommandStatus)>>([](odc::CommandStatus){}));
	}
	//Stops the io service as ODC does at shutdown (the work goes, the threads end): the port is left alive, and nothing runs any more
	void StopIo()
	{
		work.reset();
		for(auto& t : threads)
			t.join();
		threads.clear();
	}
	//Destroys the port (as when ODC shuts down) and checks that every network object it made has been destroyed
	void Destroy()
	{
		if(!PUT)
			return;
		PUT.reset();
		CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	}

	std::string Expect(const std::string& tag, std::chrono::milliseconds timeout = 20000ms)
	{
		std::string value;
		INFO("waiting for the script to report '"+tag+"'");
		REQUIRE(sink.Take(tag,value,timeout));
		return value;
	}
	//true if nothing with this tag is reported for this long
	bool NoReport(const std::string& tag, std::chrono::milliseconds wait)
	{
		std::string value;
		return !sink.Take(tag,value,wait);
	}

	SinkPort sink;
	std::shared_ptr<CaptureSink> capture;
	std::shared_ptr<odc::DataPort> PUT;
	std::shared_ptr<odc::asio_service> ios;

private:
	module_ptr portlib;
	std::string script_file;
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
};

//A blocking loopback server on its own thread: the handler is called, one connection at a time, for each connection accepted
class Peer
{
public:
	using Socket = asio::ip::tcp::socket;
	using Handler = std::function<void (Socket&, int)>;

	Peer(uint16_t port, Handler handler):
		acceptor(io,asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port)),
		handler(std::move(handler))
	{
		thread = std::thread([this](){ Loop(); });
	}
	~Peer()
	{
		Stop();
	}
	void Stop()
	{
		if(!stopping.exchange(true))
			WakeAcceptor(io,acceptor);
		{
			std::lock_guard<std::mutex> lock(mtx);
			std::error_code ec;
			acceptor.close(ec);
			if(current)
				current->close(ec);
		}
		if(thread.joinable())
			thread.join();
	}
	int Accepted() const
	{
		return accepted.load();
	}

	static void ReadAll(Socket& s, std::string& out)
	{
		std::error_code ec;
		char buf[65536];
		while(true)
		{
			auto n = s.read_some(asio::buffer(buf),ec);
			out.append(buf,n);
			if(ec)
				return;
		}
	}
	static void Echo(Socket& s, int)
	{
		std::error_code ec;
		char buf[65536];
		while(true)
		{
			auto n = s.read_some(asio::buffer(buf),ec);
			if(ec)
				return;
			asio::write(s,asio::buffer(buf,n),ec);
			if(ec)
				return;
		}
	}
private:
	void Loop()
	{
		while(!stopping)
		{
			Socket s(io);
			std::error_code ec;
			acceptor.accept(s,ec);
			if(ec || stopping)
				return;
			{
				std::lock_guard<std::mutex> lock(mtx);
				current = &s;
			}
			s.set_option(asio::ip::tcp::no_delay(true),ec);
			handler(s,accepted++);
			std::lock_guard<std::mutex> lock(mtx);
			current = nullptr;
		}
	}

	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	Handler handler;
	std::mutex mtx;
	Socket* current = nullptr;
	std::atomic<bool> stopping{false};
	std::atomic<int> accepted{0};
	std::thread thread;
};

//Connects (retrying until it works or the time is up) to a loopback listener
inline bool ConnectTo(asio::io_context& io, asio::ip::tcp::socket& s, uint16_t port, std::chrono::milliseconds timeout = 15000ms)
{
	return WaitFor([&]()
		{
			std::error_code ec;
			s.close(ec);
			s.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port),ec);
			return !ec;
		},timeout);
}

//data that is easy to check and not aligned with any buffer size: byte i is i mod 251
inline std::string Pattern(size_t n)
{
	std::string s(n,'\0');
	for(size_t i = 0; i < n; i++)
		s[i] = static_cast<char>(i%251);
	return s;
}
}

#endif // NETTESTHELPERS_H
