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
 * NetExamplesTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Tests of the Lua-Transport examples (ExampleConfig/Lua-Transport): the real Sender and Receiver scripts and the real wire module run
 *  in LuaPorts, with a recording port standing in for the source of the events (which also answers the controls) and another for the sink.
 *  TCP ports 21010 to 21099. The TLS and HTTPS examples need the test PKI (and are compiled out without TLS).
 */
#include "NetTestHelpers.h"
#include <opendatacon/IOTypes.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>

#ifdef ODC_ASIO_SSL
#include "TlsTestPki.h"
#endif

#define SUITE(name) "LuaNetExamplesTestSuite - " name

using namespace nettest;

namespace
{
const std::string ExamplesDir = ODC_LUA_TRANSPORT_DIR;

std::string Absolute(const std::string& path)
{
	return std::filesystem::weakly_canonical(std::filesystem::path(path)).generic_string();
}

Json::Value LoadConf(const std::string& example)
{
	std::ifstream f(ExamplesDir+"/"+example+"/opendatacon.conf");
	REQUIRE_FALSE(f.fail());
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errors;
	REQUIRE(Json::parseFromStream(builder,f,&root,&errors));
	return root;
}

//The ConfOverrides of a port in an example's configuration, with its LuaFile made absolute: the settings the real example has
Json::Value ExamplePort(const std::string& example, const std::string& port)
{
	auto root = LoadConf(example);
	for(const auto& p : root["Ports"])
		if(p["Name"].asString() == port)
		{
			Json::Value conf = p["ConfOverrides"];
			conf["LuaFile"] = Absolute(ExamplesDir+"/"+example+"/"+conf["LuaFile"].asString());
			return conf;
		}
	FAIL("no port "+port+" in the configuration of "+example);
	return Json::Value();
}

//Keeps the events it is given, and answers the controls among them with a status of the test's choice
class RecorderPort: public odc::DataPort
{
public:
	using Chooser = std::function<std::optional<odc::CommandStatus>(const odc::EventInfo&)>;
	explicit RecorderPort(const std::string& name): odc::DataPort(name,"",Json::Value()) {}
	void Enable() override {}
	void Disable() override {}
	void Build() override {}
	void ProcessElements(const Json::Value&) override {}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t pStatusCallback) override
	{
		std::optional<odc::CommandStatus> status = odc::CommandStatus::SUCCESS;
		{
			std::lock_guard<std::mutex> lock(mtx);
			events.push_back(event);
			if(chooser)
				status = chooser(*event);
		}
		cv.notify_all();
		if(status)
			(*pStatusCallback)(*status);
	}
	void SetChooser(Chooser c)
	{
		std::lock_guard<std::mutex> lock(mtx);
		chooser = std::move(c);
	}
	size_t Count()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return events.size();
	}
	std::vector<std::shared_ptr<const odc::EventInfo>> Events()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return events;
	}
	bool WaitForCount(size_t n, std::chrono::milliseconds timeout = 20000ms)
	{
		std::unique_lock<std::mutex> lock(mtx);
		return cv.wait_for(lock,timeout,[&](){ return events.size() >= n; });
	}
private:
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<std::shared_ptr<const odc::EventInfo>> events;
	Chooser chooser;
};

//LuaPorts of the examples (and helper scripts) in one process, on a few io threads, with a log capture
class Rig
{
public:
	Rig()
	{
		TestSetup();
		auto log = odc::spdlog_get("LuaPort");
		capture = std::make_shared<CaptureSink>();
		log->sinks().push_back(capture);
		log->sinks()[0]->set_level(log_level);
		log->set_level(spdlog::level::trace);

		portlib = LoadModule(GetLibFileName("LuaPort"));
		REQUIRE(portlib);
		newPort = GetPortCreator(portlib,"Lua");
		REQUIRE(newPort);
		deletePort = GetPortDestroyer(portlib,"Lua");
		REQUIRE(deletePort);

		ios = odc::asio_service::Get();
		work = ios->make_work();
		for(int i = 0; i < 4; i++)
			threads.emplace_back([ios{ios}](){ ios->run(); });
	}
	~Rig()
	{
		Destroy();
		work.reset();
		for(auto& t : threads)
			t.join();
		threads.clear();
		ios.reset();
		TestTearDown();
		UnLoadModule(portlib);
		for(const auto& f : tool_files)
			std::remove(f.c_str());
	}
	//a port that runs the Lua file in conf["LuaFile"]
	odc::DataPort* Add(const std::string& name, Json::Value conf)
	{
		std::shared_ptr<odc::DataPort> p(newPort(name,"",conf),deletePort);
		ports.push_back(p);
		p->Build();
		return p.get();
	}
	//a port that runs a script of the test's (after the prelude: report(tag, value))
	odc::DataPort* AddTool(const std::string& name, const std::string& script, Json::Value conf)
	{
		static std::atomic<int> number{0};
		const auto file = "NetExTool_"+std::to_string(++number)+".lua";
		{
			std::ofstream f(file);
			REQUIRE_FALSE(f.fail());
			f << Prelude << script;
		}
		tool_files.push_back(file);
		conf["LuaFile"] = file;
		return Add(name,conf);
	}
	void Destroy()
	{
		if(ports.empty())
			return;
		//(as opendatacon does when it shuts down: the scripts stop the timers they re-arm, which the port waits for when it is destroyed)
		for(auto& p : ports)
			p->Disable();
		ports.clear();
		CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	}
	std::shared_ptr<CaptureSink> capture;
private:
	module_ptr portlib;
	newptr newPort = nullptr;
	delptr deletePort = nullptr;
	std::shared_ptr<odc::asio_service> ios;
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::vector<std::shared_ptr<odc::DataPort>> ports;
	std::vector<std::string> tool_files;
};

//One Source -> Sender ==> Receiver -> Sink chain made from an example's real configuration
struct Chain
{
	RecorderPort source{"RecSource"};
	RecorderPort sink{"RecSink"};
	Rig rig; //(destroyed first: the ports use the recorders)
	odc::DataPort* sender = nullptr;
	odc::DataPort* receiver = nullptr;

	Chain(const std::string& example, const Json::Value& sender_conf, const Json::Value& receiver_conf)
	{
		sender = rig.Add("Sender",sender_conf);
		receiver = rig.Add("Receiver",receiver_conf);
		sender->Subscribe(&source,"Source");
		receiver->Subscribe(&sink,"Sink");
		(void)example;
	}
	void Enable()
	{
		receiver->Enable();
		sender->Enable();
	}
	//what the Sender is given by the source
	void Inject(const std::shared_ptr<const odc::EventInfo>& e)
	{
		sender->Event(e,"Source",std::make_shared<std::function<void (odc::CommandStatus)>>([](odc::CommandStatus){}));
	}
	//a control from the sink side: the status it comes back with (nullopt: none within the time)
	std::optional<odc::CommandStatus> Control(const std::shared_ptr<const odc::EventInfo>& e, std::chrono::milliseconds timeout = 10000ms)
	{
		auto promise = std::make_shared<std::promise<odc::CommandStatus>>();
		auto future = promise->get_future();
		receiver->Event(e,"Sink",std::make_shared<std::function<void (odc::CommandStatus)>>([promise](odc::CommandStatus s)
			{
				try
				{
					promise->set_value(s);
				}
				catch(...)
				{}
			}));
		if(future.wait_for(timeout) != std::future_status::ready)
			return std::nullopt;
		return future.get();
	}
};

constexpr odc::msSinceEpoch_t T0 = 1700000000123;

odc::QualityFlags Flags(std::initializer_list<odc::QualityFlags> list)
{
	uint16_t v = 0;
	for(auto f : list)
		v |= static_cast<uint16_t>(f);
	return static_cast<odc::QualityFlags>(v);
}

template<odc::EventType T, typename P>
std::shared_ptr<odc::EventInfo> Make(size_t index, const std::string& source, P payload, odc::QualityFlags q = odc::QualityFlags::ONLINE, odc::msSinceEpoch_t time = T0)
{
	auto e = std::make_shared<odc::EventInfo>(T,index,source,q,time);
	e->SetPayload<T>(std::move(payload));
	return e;
}

std::shared_ptr<odc::EventInfo> OctetString(size_t index, const std::string& source, const std::string& bytes)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::OctetString,index,source,odc::QualityFlags::ONLINE,T0+5);
	e->SetPayload<odc::EventType::OctetString>(odc::OctetStringBuffer(std::string(bytes)));
	return e;
}

//one event of every type the examples carry, with values that are not round numbers and qualities and times that differ
std::vector<std::shared_ptr<odc::EventInfo>> AllTypes(const std::string& source)
{
	using ET = odc::EventType;
	using QF = odc::QualityFlags;
	std::string all_bytes;
	for(int i = 0; i < 256; i++)
		all_bytes += static_cast<char>(i);
	std::vector<std::shared_ptr<odc::EventInfo>> v;
	v.push_back(Make<ET::ConnectState>(0,source,odc::ConnectState::PORT_UP,QF::ONLINE,T0));
	v.push_back(Make<ET::ConnectState>(0,source,odc::ConnectState::CONNECTED,QF::ONLINE,T0+1));
	v.push_back(Make<ET::Binary>(7,source,true,QF::ONLINE,T0+2));
	v.push_back(Make<ET::Binary>(8,source,false,Flags({QF::ONLINE,QF::LOCAL_FORCED}),T0+3));
	v.push_back(Make<ET::DoubleBitBinary>(3,source,odc::DBB(true,false),QF::ONLINE,T0+4));
	v.push_back(Make<ET::DoubleBitBinary>(4,source,odc::DBB(false,true),QF::COMM_LOST,T0+4));
	v.push_back(Make<ET::Analog>(4,source,-123.456789,QF::ONLINE,T0+5));
	v.push_back(Make<ET::Analog>(5,source,1.0/3.0,Flags({QF::ONLINE,QF::OVERRANGE}),T0+6));
	v.push_back(Make<ET::Analog>(6,source,1e-9,QF::RESTART,T0+7));
	v.push_back(Make<ET::Analog>(7,source,6.02214076e23,QF::ONLINE,T0+8));
	v.push_back(Make<ET::Analog>(8,source,0.0,QF::ONLINE,T0+9));
	v.push_back(Make<ET::Counter>(1,source,uint32_t(4294967295u),QF::ONLINE,T0+10));
	v.push_back(Make<ET::Counter>(2,source,uint32_t(0),Flags({QF::ONLINE,QF::ROLLOVER}),T0+11));
	v.push_back(Make<ET::FrozenCounter>(1,source,uint32_t(12000),QF::ONLINE,T0+12));
	v.push_back(Make<ET::BinaryOutputStatus>(1,source,true,QF::ONLINE,T0+13));
	v.push_back(Make<ET::AnalogOutputStatus>(1,source,42.75,QF::ONLINE,T0+14));
	v.push_back(OctetString(1,source,all_bytes));
	v.push_back(OctetString(2,source,std::string("{\"json\":true}\r\n\0end",18)));
	v.push_back(OctetString(3,source,""));
	v.push_back(Make<ET::Analog>(4294967295u,source,1.5,QF::ONLINE,T0+15)); //the largest index
	return v;
}

std::shared_ptr<odc::EventInfo> Crob(size_t index, odc::ControlCode code, uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
	odc::ControlRelayOutputBlock crob;
	crob.functionCode = code;
	crob.count = count;
	crob.onTimeMS = on_ms;
	crob.offTimeMS = off_ms;
	crob.status = odc::CommandStatus::SUCCESS;
	return Make<odc::EventType::ControlRelayOutputBlock>(index,"Sink",crob,odc::QualityFlags::ONLINE,T0);
}

//The source's choice: by index, like the example's source.lua
std::optional<odc::CommandStatus> ChooseByIndex(const odc::EventInfo& e)
{
	switch(e.GetIndex())
	{
		case 98: return std::nullopt; //no answer
		case 99: return odc::CommandStatus::NOT_SUPPORTED;
		case 97: return odc::CommandStatus::HARDWARE_ERROR;
		case 96: return odc::CommandStatus::BLOCKED;
		default: return odc::CommandStatus::SUCCESS;
	}
}

//Every kind of control, with its answer, from the sink to the source and back
void CheckControls(Chain& c)
{
	using ET = odc::EventType;
	using CS = odc::CommandStatus;
	c.source.SetChooser(ChooseByIndex);

	auto crob = Crob(1,odc::ControlCode::PULSE_ON,3,250,750);
	auto r = c.Control(crob);
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);
	auto ao16 = Make<ET::AnalogOutputInt16>(2,"Sink",odc::AO16(-321,CS::SUCCESS));
	r = c.Control(ao16);
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);
	r = c.Control(Make<ET::AnalogOutputInt32>(3,"Sink",odc::AO32(-2000000000,CS::SUCCESS)));
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);
	r = c.Control(Make<ET::AnalogOutputFloat32>(4,"Sink",odc::AOF(1.5f,CS::SUCCESS)));
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);
	r = c.Control(Make<ET::AnalogOutputDouble64>(5,"Sink",odc::AOD(2.25,CS::SUCCESS)));
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);

	//the source's own statuses come back as they are
	r = c.Control(Crob(99,odc::ControlCode::LATCH_OFF,1,100,100));
	REQUIRE(r);
	CHECK(*r == CS::NOT_SUPPORTED);
	r = c.Control(Crob(97,odc::ControlCode::LATCH_ON,1,100,100));
	REQUIRE(r);
	CHECK(*r == CS::HARDWARE_ERROR);
	r = c.Control(Make<ET::AnalogOutputInt16>(96,"Sink",odc::AO16(5,CS::SUCCESS)));
	REQUIRE(r);
	CHECK(*r == CS::BLOCKED);

	//and what the source received is what the sink issued
	const auto received = c.source.Events();
	REQUIRE(received.size() == 8);
	CHECK(received[0]->GetEventType() == ET::ControlRelayOutputBlock);
	CHECK(received[0]->GetIndex() == 1);
	CHECK(received[0]->GetPayload<ET::ControlRelayOutputBlock>() == crob->GetPayload<ET::ControlRelayOutputBlock>());
	CHECK(received[0]->GetSourcePort() == "Sender");
	CHECK(received[1]->GetPayload<ET::AnalogOutputInt16>() == ao16->GetPayload<ET::AnalogOutputInt16>());
	CHECK(received[2]->GetPayload<ET::AnalogOutputInt32>() == odc::AO32(-2000000000,CS::SUCCESS));
	CHECK(received[3]->GetPayload<ET::AnalogOutputFloat32>() == odc::AOF(1.5f,CS::SUCCESS));
	CHECK(received[4]->GetPayload<ET::AnalogOutputDouble64>() == odc::AOD(2.25,CS::SUCCESS));
	CHECK(received[5]->GetIndex() == 99);
	CHECK(received[5]->GetPayload<ET::ControlRelayOutputBlock>().functionCode == odc::ControlCode::LATCH_OFF);

	//no answer from the source: the sink gets TIMEOUT from the Receiver (the examples' ControlTimeoutMs is short in the tests)
	const auto start = std::chrono::steady_clock::now();
	r = c.Control(Crob(98,odc::ControlCode::PULSE_ON,1,100,100),15000ms);
	REQUIRE(r);
	CHECK(*r == CS::TIMEOUT);
	const auto waited = std::chrono::steady_clock::now()-start;
	CHECK(waited >= 500ms);
	CHECK(waited < 10s);
	//and carries on: a control after it is answered
	r = c.Control(Crob(1,odc::ControlCode::PULSE_OFF,1,100,100));
	REQUIRE(r);
	CHECK(*r == CS::SUCCESS);
	//(a late answer to the control that timed out is not mistaken for one of these)
}

//All the types in order, and a run of analogs: what the sink gets is what the source published, with the Receiver as the source port
void CheckStream(Chain& c, size_t first_expected = 0)
{
	const auto expected = AllTypes("Receiver");
	for(const auto& e : AllTypes("Source"))
		c.Inject(e);
	const size_t analogs = 200;
	for(size_t i = 0; i < analogs; i++)
		c.Inject(Make<odc::EventType::Analog>(i,"Source",static_cast<double>(i)*0.1+0.05,odc::QualityFlags::ONLINE,T0+100+static_cast<odc::msSinceEpoch_t>(i)));
	const size_t total = first_expected+expected.size()+analogs;
	REQUIRE(c.sink.WaitForCount(total));
	const auto got = c.sink.Events();
	REQUIRE(got.size() == total);
	for(size_t i = 0; i < expected.size(); i++)
	{
		INFO("event "+std::to_string(i)+": "+odc::ToString(expected[i]->GetEventType()));
		CHECK(*got[first_expected+i] == *expected[i]);
	}
	for(size_t i = 0; i < analogs; i++)
	{
		const auto e = Make<odc::EventType::Analog>(i,"Receiver",static_cast<double>(i)*0.1+0.05,odc::QualityFlags::ONLINE,T0+100+static_cast<odc::msSinceEpoch_t>(i));
		CHECK(*got[first_expected+expected.size()+i] == *e);
	}
}

Json::Value With(Json::Value conf, const std::map<std::string,Json::Value>& changes)
{
	for(const auto& c : changes)
		conf[c.first] = c.second;
	return conf;
}

bool Logged(Rig& rig, const std::string& text, std::chrono::milliseconds timeout = 10000ms)
{
	return WaitFor([&](){ return rig.capture->Has(text); },timeout);
}

size_t CountLogged(Rig& rig, const std::string& text)
{
	size_t n = 0;
	for(const auto& m : rig.capture->Messages())
		if(m.find(text) != std::string::npos)
			++n;
	return n;
}

//The configuration of an example for the tests: its own settings, with the ports of the test
Json::Value SocketSender(const std::string& example, uint16_t port, const Json::Value& extra = Json::Value(Json::objectValue))
{
	auto c = ExamplePort(example,"Sender");
	c["Port"] = port;
	c["RetryMs"] = 100;
	for(const auto& name : extra.getMemberNames())
		c[name] = extra[name];
	return c;
}
Json::Value SocketReceiver(const std::string& example, uint16_t port, const Json::Value& extra = Json::Value(Json::objectValue))
{
	auto c = ExamplePort(example,"Receiver");
	c["Port"] = port;
	c["ControlTimeoutMs"] = 800;
	for(const auto& name : extra.getMemberNames())
		c[name] = extra[name];
	return c;
}
Json::Value HttpSender(const std::string& example, uint16_t sender_port, uint16_t receiver_port, const Json::Value& extra = Json::Value(Json::objectValue))
{
	auto c = ExamplePort(example,"Sender");
	c["LocalPort"] = sender_port;
	c["PeerPort"] = receiver_port;
	c["BatchMs"] = 50;
	c["ControlTimeoutMs"] = 800;
	for(const auto& name : extra.getMemberNames())
		c[name] = extra[name];
	return c;
}
Json::Value HttpReceiver(const std::string& example, uint16_t sender_port, uint16_t receiver_port, const Json::Value& extra = Json::Value(Json::objectValue))
{
	auto c = ExamplePort(example,"Receiver");
	c["LocalPort"] = receiver_port;
	c["PeerPort"] = sender_port;
	c["ControlTimeoutMs"] = 800;
	for(const auto& name : extra.getMemberNames())
		c[name] = extra[name];
	return c;
}

//a counter event with a sequence number as its index: what the reconnect tests send
std::shared_ptr<odc::EventInfo> Numbered(size_t n)
{
	return Make<odc::EventType::Counter>(n,"Source",static_cast<uint32_t>(n*3),odc::QualityFlags::ONLINE,T0+static_cast<odc::msSinceEpoch_t>(n));
}

void CheckNumbered(Chain& c, size_t from, size_t to)
{
	REQUIRE(c.sink.WaitForCount(to));
	const auto got = c.sink.Events();
	REQUIRE(got.size() == to);
	for(size_t i = from; i < to; i++)
	{
		INFO("event "+std::to_string(i));
		REQUIRE(got[i]->GetEventType() == odc::EventType::Counter);
		CHECK(got[i]->GetIndex() == i);
		CHECK(got[i]->GetPayload<odc::EventType::Counter>() == i*3);
	}
}

//Pieces of text for the tools: the Lua that writes chunks to the Receiver's TCP or TLS port, and the one that POSTs bodies to /events
const char* SocketTool = R"lua(
local C = odc.Config
local chunks = C.Chunks
local gap = C.GapMs or 0
local sock
local i = 0
local done = false
local function step()
	if done then return end
	i = i + 1
	if i > #chunks then
		done = true
		report("sent", #chunks)
		sock:close()
		return
	end
	sock:write(chunks[i])
	if gap > 0 then odc.msTimerCallback(gap, step) else step() end
end
function Enable()
	local options =
	{
		host = "127.0.0.1", port = C.Port, auto_reopen = true, retry_ms = 50,
		on_open = function(s, peer) report("open", peer and peer.protocol or "tcp"); step() end,
		on_close = function(s, reason, detail) report("closed", tostring(reason)); s:close() end,
	}
	local s, err
	if C.Tls then
		options.on_tls_failed = function(s, category, detail) report("tls_failed", tostring(category)) end
		options.tls = C.Tls
		s, err = odc.TLSSocket(options)
	else
		s, err = odc.TCPSocket(options)
	end
	if not s then report("ctor", tostring(err)) return end
	sock = s
	sock:open()
end
)lua";

const char* HttpTool = R"lua(
local C = odc.Config
function Enable()
	local c, err = odc.HTTPClient{ url = C.Url, timeout_ms = 5000, tls = C.Tls }
	if not c then report("ctor", tostring(err)) return end
	for i, body in ipairs(C.Bodies) do
		local ok, rerr = c:request{ method = C.Method or "POST", path = C.Path or "/events", headers = { ["Content-Type"] = "application/json" }, body = body,
			on_response = function(resp, error_string, detail)
				report("r"..i, error_string and ("err="..error_string) or (resp.status..":"..resp.body))
			end }
		if not ok then report("r"..i, "request="..tostring(rerr)) end
	end
end
)lua";

std::string EventLine(const std::string& type, size_t index, const std::string& payload, const std::string& version = "1", const std::string& kind = "event")
{
	return "{\"v\":"+version+",\"kind\":\""+kind+"\",\"event\":{\"type\":\""+type+"\",\"index\":"+std::to_string(index)+",\"quality\":1,\"time\":"+std::to_string(T0)+",\"payload\":"+payload+"}}";
}

//Chunks of the text, each of at most n bytes
Json::Value Split(const std::string& text, size_t n)
{
	Json::Value chunks(Json::arrayValue);
	for(size_t i = 0; i < text.size(); i += n)
		chunks.append(text.substr(i,n));
	return chunks;
}
}

TEST_CASE(SUITE("The configurations parse and the files they use exist and each says how to run it"))
{
	std::set<int> all_ports; //(each example has its own ports)
	const std::regex require_pattern("require\\(\"([A-Za-z0-9_]+)\"\\)");
	for(const std::string example : {"TCP","TLS","HTTP","HTTPS"})
	{
		INFO(example);
		const auto root = LoadConf(example);
		std::set<std::string> names;
		std::set<int> ports;
		REQUIRE(root["Ports"].isArray());
		for(const auto& p : root["Ports"])
		{
			names.insert(p["Name"].asString());
			const auto lua = ExamplesDir+"/"+example+"/"+p["ConfOverrides"]["LuaFile"].asString();
			INFO(lua);
			REQUIRE(std::filesystem::exists(lua));
			std::ifstream f(lua);
			std::stringstream ss;
			ss << f.rdbuf();
			const auto text = ss.str();
			for(std::sregex_iterator it(text.begin(),text.end(),require_pattern); it != std::sregex_iterator(); ++it)
				CHECK(std::filesystem::exists(std::filesystem::path(lua).parent_path()/((*it)[1].str()+".lua")));
			for(const char* key : {"Port","LocalPort","PeerPort"})
				if(p["ConfOverrides"].isMember(key))
					ports.insert(p["ConfOverrides"][key].asInt());
		}
		for(int port : ports)
			CHECK(all_ports.insert(port).second);
		for(const auto& c : root["Connectors"])
			for(const auto& connection : c["ConfOverrides"]["Connections"])
			{
				CHECK(names.count(connection["Port1"].asString()) == 1);
				CHECK(names.count(connection["Port2"].asString()) == 1);
			}
		const bool tls = (example == "TLS" || example == "HTTPS");
		CHECK(std::filesystem::exists(ExamplesDir+"/"+example+"/opendatacon.conf"));
		CHECK(std::filesystem::exists(ExamplesDir+"/"+example+"/make-certs.ps1") == tls);
		CHECK(std::filesystem::exists(ExamplesDir+"/"+example+"/make-certs.sh") == tls);
		if(tls)
		{
			//the certificates are made where the configuration looks for them, and are not committed
			std::ifstream gi(ExamplesDir+"/"+example+"/.gitignore");
			std::stringstream gs;
			gs << gi.rdbuf();
			CHECK(gs.str().find("certs/") != std::string::npos);
			std::ifstream ps(ExamplesDir+"/"+example+"/make-certs.ps1");
			std::stringstream pss;
			pss << ps.rdbuf();
			for(const char* port : {"Sender","Receiver"})
			{
				const auto c = ExamplePort(example,port);
				for(const char* key : {"CertFile","KeyFile","CAFile"})
				{
					REQUIRE(c.isMember(key));
					const auto path = c[key].asString();
					CHECK(path.rfind("certs/",0) == 0);
					CHECK(pss.str().find(path.substr(6)) != std::string::npos);
				}
			}
		}
	}
}

TEST_CASE(SUITE("The wire module: every type round trips and what is wrong is refused"))
{
	SinkPort reports;
	Rig rig;
	Json::Value conf;
	conf["WireDir"] = Absolute(ExamplesDir);
	auto tool = rig.AddTool("Tool",R"lua(
package.path = odc.Config.WireDir.."/?.lua;"..package.path
local wire = require("odc_wire")
local ET, Q, CS = odc.EventType, odc.QualityFlag, odc.CommandStatus
local results = {}
local function check(name, ok, detail) results[#results+1] = name..(ok and "=ok" or ("=FAILED "..tostring(detail))) end
local function same(a, b)
	if type(a) ~= type(b) then return false end
	if type(a) ~= "table" then return a == b end
	for k, v in pairs(a) do if not same(v, b[k]) then return false end end
	for k in pairs(b) do if a[k] == nil then return false end end
	return true
end
local function round_trip(name, e)
	local msg, why = wire.event_msg(e)
	if not msg then check(name, false, why) return end
	local text = wire.encode(msg)
	local back, why2 = wire.decode(text)
	if not back then check(name, false, why2) return end
	local out, why3 = wire.decode_event(back.event)
	if not out then check(name, false, why3) return end
	check(name, same(e, out) and not text:find("\n", 1, true), odc.EncodeJSON(e).." became "..odc.EncodeJSON(out))
end
local function ev(type_name, index, payload, quality, time) return { EventType = ET[type_name], Index = index, QualityFlags = quality or Q.ONLINE, Timestamp = time or 1700000000123, Payload = payload } end
local function crob(code, count, on, off, status) return { ControlCode = odc.ControlCode[code], Count = count, msOnTime = on, msOffTime = off, CommandStatus = CS[status or "SUCCESS"] } end

function Enable()
	round_trip("binary", ev("Binary", 1, true))
	round_trip("binary_false", ev("Binary", 2, false, odc.QualityFlags(Q.ONLINE, Q.LOCAL_FORCED)))
	round_trip("dbb", ev("DoubleBitBinary", 3, { First = true, Second = false }))
	round_trip("analog", ev("Analog", 4, -123.456789))
	round_trip("analog_third", ev("Analog", 4, 1/3))
	round_trip("analog_tiny", ev("Analog", 4, 1e-300))
	round_trip("analog_huge", ev("Analog", 4, 1.7976931348623157e308))
	round_trip("analog_integer_valued", ev("Analog", 4, 5.0))
	round_trip("counter", ev("Counter", 5, 4294967295))
	round_trip("frozen", ev("FrozenCounter", 5, 0))
	round_trip("binary_output_status", ev("BinaryOutputStatus", 6, true))
	round_trip("analog_output_status", ev("AnalogOutputStatus", 6, 2.5))
	round_trip("octets", ev("OctetString", 7, "\0\1\2\255\r\n\"\\ {}"))
	round_trip("octets_empty", ev("OctetString", 7, ""))
	round_trip("connect_state", ev("ConnectState", 0, odc.ConnectState.CONNECTED))
	round_trip("crob", ev("ControlRelayOutputBlock", 8, crob("PULSE_ON", 3, 250, 750)))
	round_trip("crob_status", ev("ControlRelayOutputBlock", 8, crob("TRIP_PULSE_ON", 255, 4294967295, 0, "NOT_SUPPORTED")))
	round_trip("ao16", ev("AnalogOutputInt16", 9, { Value = -32768, CommandStatus = CS.SUCCESS }))
	round_trip("ao32", ev("AnalogOutputInt32", 9, { Value = 2147483647, CommandStatus = CS.SUCCESS }))
	round_trip("aof", ev("AnalogOutputFloat32", 9, { Value = 1.5, CommandStatus = CS.SUCCESS }))
	round_trip("aod", ev("AnalogOutputDouble64", 9, { Value = -2.25, CommandStatus = CS.OUT_OF_RANGE }))
	round_trip("index_max", ev("Binary", 4294967295, true))
	round_trip("time_zero", ev("Binary", 0, true, 0, 0))

	-- what cannot be encoded
	local function refuses_encode(name, e, text)
		local msg, why = wire.event_msg(e)
		check(name, msg == nil and type(why) == "string" and why:find(text, 1, true) ~= nil, tostring(why))
	end
	refuses_encode("encode_unsupported_type", ev("TimeSync", 0, { AbsTime = true, SysOffs = false }), "unsupported event type")
	refuses_encode("encode_nan", ev("Analog", 0, 0/0), "finite")
	refuses_encode("encode_inf", ev("Analog", 0, 1/0), "finite")
	refuses_encode("encode_payload_type", ev("Binary", 0, 5), "boolean")
	refuses_encode("encode_counter_negative", ev("Counter", 0, -1), "integer")
	refuses_encode("encode_not_event", {}, "not an event")
	refuses_encode("encode_crob_bad", ev("ControlRelayOutputBlock", 0, { ControlCode = 1 }), "CROB")
	refuses_encode("encode_ao16_range", ev("AnalogOutputInt16", 0, { Value = 40000, CommandStatus = CS.SUCCESS }), "analog output")
	local non_control, why = wire.control_msg(1, ev("Analog", 0, 1.5))
	check("control_msg_refuses_data", non_control == nil and why:find("not a control", 1, true) ~= nil, why)

	-- what cannot be decoded
	local function refuses_decode(name, text, part)
		local msg, why = wire.decode(text)
		if msg and msg.kind == "event" then msg, why = wire.decode_event(msg.event) end
		check(name, msg == nil and type(why) == "string" and why:find(part, 1, true) ~= nil, tostring(why))
	end
	local function line(type_name, payload, extra) return '{"v":1,"kind":"event","event":{"type":"'..type_name..'","index":1,"quality":1,"time":5,"payload":'..payload..(extra or "")..'}}' end
	refuses_decode("decode_empty", "", "empty")
	refuses_decode("decode_garbage", "not json at all", "not a JSON object")
	refuses_decode("decode_array", "[1,2]", "no version")
	refuses_decode("decode_no_version", '{"kind":"event"}', "no version")
	refuses_decode("decode_future_version", '{"v":2,"kind":"event"}', "unsupported version 2")
	refuses_decode("decode_float_version", '{"v":1.5,"kind":"event"}', "no version")
	refuses_decode("decode_unknown_kind", '{"v":1,"kind":"launch"}', "unknown kind")
	refuses_decode("decode_unknown_type", line("Frobnicate", "1"), "unsupported event type 'Frobnicate'")
	refuses_decode("decode_unsupported_type", line("TimeSync", "1"), "unsupported event type 'TimeSync'")
	refuses_decode("decode_type_not_string", '{"v":1,"kind":"event","event":{"type":5,"index":1,"quality":1,"time":5,"payload":1}}', "no type")
	refuses_decode("decode_no_event", '{"v":1,"kind":"event"}', "no type")
	refuses_decode("decode_bad_payload", line("Binary", '"yes"'), "boolean")
	refuses_decode("decode_analog_string", line("Analog", '"1.5"'), "finite number")
	refuses_decode("decode_counter_float", line("Counter", "1.5"), "integer")
	refuses_decode("decode_counter_big", line("Counter", "4294967296"), "integer")
	refuses_decode("decode_dbb", line("DoubleBitBinary", '{"first":true}'), "booleans")
	refuses_decode("decode_hex", line("OctetString", '"xyz"'), "hex")
	refuses_decode("decode_connect_state", line("ConnectState", '"EXPLODED"'), "connect state")
	refuses_decode("decode_crob_code", line("ControlRelayOutputBlock", '{"code":"EXPLODE","count":1,"on_ms":1,"off_ms":1,"status":"SUCCESS"}'), "CROB")
	refuses_decode("decode_crob_status", line("ControlRelayOutputBlock", '{"code":"PULSE_ON","count":1,"on_ms":1,"off_ms":1,"status":"NOPE"}'), "CROB")
	refuses_decode("decode_crob_count", line("ControlRelayOutputBlock", '{"code":"PULSE_ON","count":256,"on_ms":1,"off_ms":1,"status":"SUCCESS"}'), "CROB")
	refuses_decode("decode_ao_value", line("AnalogOutputInt16", '{"value":1.5,"status":"SUCCESS"}'), "analog output")
	refuses_decode("decode_index_negative", '{"v":1,"kind":"event","event":{"type":"Binary","index":-1,"quality":1,"time":5,"payload":true}}', "invalid index")
	refuses_decode("decode_no_time", '{"v":1,"kind":"event","event":{"type":"Binary","index":1,"quality":1,"payload":true}}', "invalid index, quality or time")
	local ok_msg = wire.decode(line("Binary", "true"))
	check("decode_ignores_extras", ok_msg ~= nil and wire.decode_event(ok_msg.event) ~= nil)
	local with_extra = wire.decode('{"v":1,"kind":"event","future":"field","event":{"type":"Binary","index":1,"quality":1,"quality_text":"ONLINE","time":5,"payload":true,"more":1}}')
	check("decode_ignores_unknown_fields", with_extra ~= nil and wire.decode_event(with_extra.event) ~= nil)

	-- messages
	local status = wire.decode(wire.encode(wire.status_msg(7, CS.HARDWARE_ERROR)))
	check("status_round_trip", status.kind == "status" and status.id == 7 and wire.status_value(status.status) == CS.HARDWARE_ERROR)
	check("status_unknown_name", wire.status_value("NOPE") == nil)
	local ack = wire.decode(wire.encode(wire.ack_msg(3, 1)))
	check("ack_round_trip", ack.kind == "ack" and ack.accepted == 3 and ack.rejected == 1)
	local batch, skipped = wire.events_msg({ ev("Binary", 1, true), ev("TimeSync", 0, { AbsTime = true, SysOffs = false }), ev("Analog", 2, 1.5) })
	check("batch_skips_what_cannot_be_sent", #batch.events == 2 and skipped == 1)
	check("supported_types_has_the_examples_types", #wire.supported_types() == 14)

	-- the line buffer: any split, any join
	local text = {}
	for i = 1, 5 do text[#text+1] = wire.encode(wire.event_msg(ev("Analog", i, i + 0.5))).."\n" end
	local all = table.concat(text)
	for _, size in ipairs({ 1, 2, 3, 7, 64, 1000 }) do
		local lb = wire.line_buffer(100000)
		local got = {}
		for i = 1, #all, size do
			for _, l in ipairs(lb.feed(all:sub(i, i + size - 1))) do got[#got+1] = l end
		end
		local intact = #got == 5
		for i = 1, #got do intact = intact and got[i] .. "\n" == text[i] end
		check("lines_split_"..size, intact, #got)
	end
	local lb = wire.line_buffer(100000)
	local got = lb.feed(all)
	check("lines_coalesced", #got == 5)
	local crlf = wire.line_buffer(100).feed("one\r\ntwo\r\n\r\nthree")
	check("lines_crlf_and_blank", #crlf == 2 and crlf[1] == "one" and crlf[2] == "two")
	local small = wire.line_buffer(20)
	local lines = small.feed("short\n"..string.rep("x", 50).."\nafter\n"..string.rep("y", 30))
	for _, l in ipairs(small.feed("\nlast\n")) do lines[#lines+1] = l end
	check("line_over_limit_dropped_stream_carries_on", small.dropped() == 2 and #lines == 3 and lines[1] == "short" and lines[2] == "after" and lines[3] == "last", table.concat(lines, "|").." dropped "..small.dropped())

	-- the queue: bounded, in order, put_back at the front
	local q = wire.event_queue(3)
	for i = 1, 5 do q.push(i) end
	check("queue_drops_the_oldest", q.size() == 3 and q.dropped() == 2 and q.take(1)[1] == 3)
	q.put_back({ 3 })
	local rest = q.take(10)
	check("queue_put_back_keeps_order", #rest == 3 and rest[1] == 3 and rest[2] == 4 and rest[3] == 5)

	-- pending controls: answered, timed out, failed
	local pending = wire.pending_controls(50)
	local answers = {}
	local a = pending.add(function(s) answers.a = s end)
	local b = pending.add(function(s) answers.b = s end)
	local c = pending.add(function(s) answers.c = s end)
	check("pending_resolve", pending.resolve(a, CS.SUCCESS) and answers.a == CS.SUCCESS)
	check("pending_resolve_twice", pending.resolve(a, CS.SUCCESS) == false)
	check("pending_resolve_unknown", pending.resolve(99, CS.SUCCESS) == false and pending.resolve("x", CS.SUCCESS) == false)
	pending.fail_all(CS.DOWNSTREAM_FAIL)
	check("pending_fail_all", answers.b == CS.DOWNSTREAM_FAIL and answers.c == CS.DOWNSTREAM_FAIL)
	local late
	pending.add(function(s) late = s end)
	odc.msTimerCallback(300, function()
		check("pending_timeout", late == CS.TIMEOUT, late)
		report("done", table.concat(results, "\n"))
	end)
end
)lua",conf);
	tool->Subscribe(&reports,"Reports");
	tool->Enable();
	std::string value;
	REQUIRE(reports.Take("done",value,20000ms));
	size_t ok = 0;
	std::istringstream in(value);
	std::string line;
	while(std::getline(in,line))
	{
		INFO(line);
		CHECK(line.find("=ok") != std::string::npos);
		if(line.find("=ok") != std::string::npos)
			++ok;
	}
	CHECK(ok >= 75);
}

TEST_CASE(SUITE("TCP: every type arrives in order and the controls go back with their status"))
{
	Chain c("TCP",SocketSender("TCP",21010),SocketReceiver("TCP",21010));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: the Sender connected"));
	CheckStream(c);
	CheckControls(c);
	//the source side of every event is the Receiver's name, and the events the example publishes are all of types the Sender was given
	for(const auto& e : c.sink.Events())
		CHECK(e->GetSourcePort() == "Receiver");
}

TEST_CASE(SUITE("TCP: the Sender keeps events while the Receiver is away and sends them in order when it is back"))
{
	Chain c("TCP",SocketSender("TCP",21011),SocketReceiver("TCP",21011));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: the Sender connected"));
	for(size_t i = 0; i < 10; i++)
		c.Inject(Numbered(i));
	CheckNumbered(c,0,10);

	//the Receiver goes away: the connection ends, and what the source publishes meanwhile is kept
	c.receiver->Disable();
	REQUIRE(Logged(c.rig,"connection to the Receiver ended"));
	for(size_t i = 10; i < 20; i++)
		c.Inject(Numbered(i));
	std::this_thread::sleep_for(300ms);
	CHECK(c.sink.Count() == 10);

	c.receiver->Enable();
	CheckNumbered(c,10,20);
	REQUIRE(WaitFor([&](){ return CountLogged(c.rig,"Receiver: the Sender connected") >= 2; }));
	CHECK(CountLogged(c.rig,"Sender: connected to the Receiver") == 2);
	//and everything works again, controls too
	for(size_t i = 20; i < 25; i++)
		c.Inject(Numbered(i));
	CheckNumbered(c,20,25);
	c.source.SetChooser(ChooseByIndex);
	auto r = c.Control(Crob(1,odc::ControlCode::PULSE_ON,1,100,100));
	REQUIRE(r);
	CHECK(*r == odc::CommandStatus::SUCCESS);
}

TEST_CASE(SUITE("TCP: without a Sender a control is answered DOWNSTREAM_FAIL at once"))
{
	Chain c("TCP",SocketSender("TCP",21012),SocketReceiver("TCP",21012));
	c.receiver->Enable(); //(the Sender is not enabled)
	std::this_thread::sleep_for(200ms);
	const auto start = std::chrono::steady_clock::now();
	auto r = c.Control(Crob(1,odc::ControlCode::PULSE_ON,1,100,100));
	REQUIRE(r);
	CHECK(*r == odc::CommandStatus::DOWNSTREAM_FAIL);
	CHECK(std::chrono::steady_clock::now()-start < 2s);
	//a data event sent the wrong way is not supported
	auto bad = Make<odc::EventType::Analog>(1,"Sink",1.5);
	auto p = std::make_shared<std::promise<odc::CommandStatus>>();
	c.receiver->Event(bad,"Sink",std::make_shared<std::function<void (odc::CommandStatus)>>([p](odc::CommandStatus s){ p->set_value(s); }));
	auto f = p->get_future();
	REQUIRE(f.wait_for(5s) == std::future_status::ready);
	CHECK(f.get() == odc::CommandStatus::NOT_SUPPORTED);
}

TEST_CASE(SUITE("TCP: the Receiver takes lines however the bytes are split or joined and carries on after bad ones"))
{
	//the lines: good, unknown type, garbage, wrong version, a kind that does not belong, a good one that follows them all
	const std::vector<std::string> lines =
	{
		EventLine("Analog",1,"2.5"),
		EventLine("Frobnicate",1,"1"),
		"this is not json",
		EventLine("Binary",2,"true","7"),
		EventLine("Binary",3,"true","1","events"),
		EventLine("Binary",4,"\"not a boolean\""),
		EventLine("Counter",5,"123456"),
		"",
		EventLine("Analog",6,"-0.5"),
	};
	std::string all;
	for(const auto& l : lines)
		all += l+"\n";
	struct Variant
	{
		const char* name;
		Json::Value chunks;
		int gap;
	};
	for(const auto& v : {Variant{"byte by byte",Split(all,1),1},Variant{"in sevens",Split(all,7),2},Variant{"all at once",Split(all,all.size()),0}
	    })
	{
		INFO(v.name);
		RecorderPort sink("RecSink");
		SinkPort reports;
		Rig rig;
		auto receiver = rig.Add("Receiver",SocketReceiver("TCP",21013));
		receiver->Subscribe(&sink,"Sink");
		receiver->Enable();
		Json::Value tool_conf;
		tool_conf["Port"] = 21013;
		tool_conf["Chunks"] = v.chunks;
		tool_conf["GapMs"] = v.gap;
		auto tool = rig.AddTool("Tool",SocketTool,tool_conf);
		tool->Subscribe(&reports,"Reports");
		tool->Enable();
		std::string value;
		REQUIRE(reports.Take("sent",value,60000ms));
		//three of the lines are good events
		REQUIRE(sink.WaitForCount(3));
		std::this_thread::sleep_for(300ms);
		const auto got = sink.Events();
		REQUIRE(got.size() == 3);
		CHECK(got[0]->GetEventType() == odc::EventType::Analog);
		CHECK(got[0]->GetPayload<odc::EventType::Analog>() == 2.5);
		CHECK(got[1]->GetEventType() == odc::EventType::Counter);
		CHECK(got[1]->GetPayload<odc::EventType::Counter>() == 123456);
		CHECK(got[2]->GetEventType() == odc::EventType::Analog);
		CHECK(got[2]->GetPayload<odc::EventType::Analog>() == -0.5);
		//each bad line was refused with a logged error, and the stream went on
		CHECK(CountLogged(rig,"Receiver: rejected") >= 5);
		CHECK(rig.capture->Has("unsupported event type 'Frobnicate'"));
		CHECK(rig.capture->Has("unsupported version 7"));
		CHECK(rig.capture->Has("rejected a message of kind 'events'"));
		CHECK(rig.capture->Has("Binary event: payload is not a boolean"));
		CHECK(rig.capture->Has("not a JSON object"));
	}
}

TEST_CASE(SUITE("TCP: a line over the limit is dropped and the stream carries on"))
{
	RecorderPort sink("RecSink");
	SinkPort reports;
	Rig rig;
	auto receiver = rig.Add("Receiver",SocketReceiver("TCP",21014,[](){ Json::Value v; v["MaxLineBytes"] = 300; return v; }()));
	receiver->Subscribe(&sink,"Sink");
	receiver->Enable();
	const std::string huge = EventLine("OctetString",1,"\""+std::string(2000,'a')+"\"");
	std::string all = EventLine("Counter",1,"11")+"\n"+huge+"\n"+EventLine("Counter",2,"22")+"\n"+huge+"\n"+EventLine("Counter",3,"33")+"\n";
	Json::Value tool_conf;
	tool_conf["Port"] = 21014;
	tool_conf["Chunks"] = Split(all,100);
	tool_conf["GapMs"] = 1;
	auto tool = rig.AddTool("Tool",SocketTool,tool_conf);
	tool->Subscribe(&reports,"Reports");
	tool->Enable();
	REQUIRE(sink.WaitForCount(3));
	std::this_thread::sleep_for(300ms);
	const auto got = sink.Events();
	REQUIRE(got.size() == 3);
	CHECK(got[0]->GetPayload<odc::EventType::Counter>() == 11);
	CHECK(got[1]->GetPayload<odc::EventType::Counter>() == 22);
	CHECK(got[2]->GetPayload<odc::EventType::Counter>() == 33);
	CHECK(rig.capture->Has("a line over 300 bytes"));
}

TEST_CASE(SUITE("TCP: stopping the ports with traffic and a control in flight leaves nothing behind and no callback"))
{
	Chain c("TCP",SocketSender("TCP",21015),SocketReceiver("TCP",21015,[](){ Json::Value v; v["ControlTimeoutMs"] = 30000; return v; }()));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: the Sender connected"));
	c.source.SetChooser(ChooseByIndex);
	for(size_t i = 0; i < 50; i++)
		c.Inject(Numbered(i));
	std::atomic<int> answered{0};
	c.receiver->Event(Crob(98,odc::ControlCode::PULSE_ON,1,100,100),"Sink",std::make_shared<std::function<void (odc::CommandStatus)>>([&answered](odc::CommandStatus){ ++answered; }));
	std::this_thread::sleep_for(200ms);
	c.sender->Disable();
	c.receiver->Disable();
	c.rig.Destroy(); //checks that every object is gone
	//(the control that was waiting is not answered after the port is gone)
	CHECK(answered.load() <= 1);
}

TEST_CASE(SUITE("HTTP: every type arrives in order and the controls go back with their status"))
{
	Chain c("HTTP",HttpSender("HTTP",21030,21031),HttpReceiver("HTTP",21030,21031));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: listening on http://127.0.0.1:21031/events"));
	CheckStream(c);
	CheckControls(c);
	for(const auto& e : c.sink.Events())
		CHECK(e->GetSourcePort() == "Receiver");
	//a batch is at most BatchMax events
	CHECK(c.sink.Count() >= 220);
}

TEST_CASE(SUITE("HTTP: the Sender keeps the batch while the Receiver is away and the order is kept"))
{
	Chain c("HTTP",HttpSender("HTTP",21032,21033),HttpReceiver("HTTP",21032,21033));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: listening on http://127.0.0.1:21033/events"));
	for(size_t i = 0; i < 10; i++)
		c.Inject(Numbered(i));
	CheckNumbered(c,0,10);
	//(the Receiver is stopped once the Sender has its answer: stopping it before the answer is out makes the Sender send the batch again)
	REQUIRE(Logged(c.rig,"Sender: HTTPClient http://127.0.0.1:21033: POST /events -> 200"));
	c.receiver->Disable();
	std::this_thread::sleep_for(300ms);
	for(size_t i = 10; i < 130; i++) //(more than a batch)
		c.Inject(Numbered(i));
	REQUIRE(Logged(c.rig,"cannot reach the Receiver"));
	CHECK(c.sink.Count() == 10);
	c.receiver->Enable();
	CheckNumbered(c,10,130);
	c.source.SetChooser(ChooseByIndex);
	auto r = c.Control(Crob(1,odc::ControlCode::PULSE_ON,1,100,100));
	REQUIRE(r);
	CHECK(*r == odc::CommandStatus::SUCCESS);
}

TEST_CASE(SUITE("HTTP: without a Sender a control is answered DOWNSTREAM_FAIL"))
{
	//(a refused connection takes a couple of seconds to be reported on Windows: the control waits longer than that)
	Json::Value longer;
	longer["ControlTimeoutMs"] = 8000;
	Chain c("HTTP",HttpSender("HTTP",21034,21035),HttpReceiver("HTTP",21034,21035,longer));
	c.receiver->Enable(); //(the Sender is not enabled: nothing listens on its port)
	REQUIRE(Logged(c.rig,"Receiver: listening on http://127.0.0.1:21035/events"));
	auto r = c.Control(Crob(1,odc::ControlCode::PULSE_ON,1,100,100),20000ms);
	REQUIRE(r);
	CHECK(*r == odc::CommandStatus::DOWNSTREAM_FAIL);
}

TEST_CASE(SUITE("HTTP: bad requests to the Receiver get an error status the good events in a batch are kept and the stream carries on"))
{
	RecorderPort sink("RecSink");
	SinkPort reports;
	Rig rig;
	auto receiver = rig.Add("Receiver",HttpReceiver("HTTP",21036,21037,[](){ Json::Value v; v["MaxBatchEvents"] = 5; v["MaxBodyBytes"] = 4000; return v; }()));
	receiver->Subscribe(&sink,"Sink");
	receiver->Enable();
	auto batch = [](const std::vector<std::string>& events, const std::string& v = "1", const std::string& kind = "events")
			 {
				 std::string s = "{\"v\":"+v+",\"kind\":\""+kind+"\",\"events\":[";
				 for(size_t i = 0; i < events.size(); i++)
					 s += (i ? "," : "")+events[i];
				 return s+"]}";
			 };
	auto ev = [](const std::string& type, size_t index, const std::string& payload)
		    {
			    return "{\"type\":\""+type+"\",\"index\":"+std::to_string(index)+",\"quality\":1,\"time\":"+std::to_string(T0)+",\"payload\":"+payload+"}";
		    };
	Json::Value bodies(Json::arrayValue);
	bodies.append(batch({ev("Counter",1,"11"),ev("Frobnicate",2,"1"),ev("Counter",3,"33"),ev("Binary",4,"7")}));                                       //r1: two good, two bad
	bodies.append("this is not json");                                                                                                                 //r2
	bodies.append(batch({ev("Counter",5,"55")},"9"));                                                                                                  //r3: a version it does not know
	bodies.append(batch({},"1","control"));                                                                                                            //r4: the wrong kind
	bodies.append(batch({ev("Counter",6,"1"),ev("Counter",7,"1"),ev("Counter",8,"1"),ev("Counter",9,"1"),ev("Counter",10,"1"),ev("Counter",11,"1")})); //r5: over MaxBatchEvents
	bodies.append(batch({ev("OctetString",12,"\""+std::string(5000,'a')+"\"")}));                                                                      //r6: over MaxBodyBytes
	bodies.append(batch({ev("Counter",13,"66")}));                                                                                                     //r7: good, after all of them
	bodies.append(batch({}));                                                                                                                          //r8: empty
	Json::Value conf;
	conf["Url"] = "http://127.0.0.1:21037";
	conf["Bodies"] = bodies;
	auto tool = rig.AddTool("Tool",HttpTool,conf);
	tool->Subscribe(&reports,"Reports");
	tool->Enable();
	std::map<std::string,std::string> r;
	for(int i = 1; i <= 8; i++)
	{
		std::string v;
		REQUIRE(reports.Take("r"+std::to_string(i),v,20000ms));
		r["r"+std::to_string(i)] = v;
	}
	CHECK(r["r1"] == "200:{\"accepted\":2,\"kind\":\"ack\",\"rejected\":2,\"v\":1}");
	CHECK(r["r2"].rfind("400:",0) == 0);
	CHECK(r["r3"].rfind("400:",0) == 0);
	CHECK(r["r4"].rfind("400:",0) == 0);
	CHECK(r["r5"].rfind("400:",0) == 0);
	INFO("r6: "+r["r6"]);
	CHECK(r["r6"].rfind("200:",0) != 0); //refused: an error status or a connection closed
	CHECK(r["r7"] == "200:{\"accepted\":1,\"kind\":\"ack\",\"rejected\":0,\"v\":1}");
	CHECK(r["r8"] == "200:{\"accepted\":0,\"kind\":\"ack\",\"rejected\":0,\"v\":1}");
	REQUIRE(sink.WaitForCount(3));
	const auto got = sink.Events();
	REQUIRE(got.size() == 3);
	std::set<size_t> indexes;
	for(const auto& e : got)
		indexes.insert(e->GetIndex());
	CHECK(indexes == std::set<size_t>{1,3,13});
	CHECK(rig.capture->Has("unsupported event type 'Frobnicate'"));
	CHECK(rig.capture->Has("unsupported version 9"));
	//a GET, and a path that is not a route, are not served
	Json::Value conf2;
	conf2["Url"] = "http://127.0.0.1:21037";
	conf2["Method"] = "GET";
	Json::Value one(Json::arrayValue);
	one.append("");
	conf2["Bodies"] = one;
	auto tool2 = rig.AddTool("Tool2",HttpTool,conf2);
	tool2->Subscribe(&reports,"Reports");
	tool2->Enable();
	std::string v;
	REQUIRE(reports.Take("r1",v,20000ms));
	CHECK(v.rfind("404:",0) == 0);
}

#ifdef ODC_ASIO_SSL
namespace
{
struct ExPki
{
	tlstest::TempDir dir;
	std::string ca, otherCa, senderCrt, senderKey, receiverCrt, receiverKey, otherCrt, otherKey, expiredCrt, expiredKey;
	ExPki()
	{
		const std::vector<std::string> sans = {"DNS:odc.example", "IP:127.0.0.1"};
		auto caCert = tlstest::MakeCert({"Example CA", {}, -3600, 86400LL*3650, true, 1});
		auto otherCaCert = tlstest::MakeCert({"Other Example CA", {}, -3600, 86400LL*3650, true, 1});
		auto sender = tlstest::MakeCert({"odc-sender.example", sans, -3600, 86400LL*365, false, 100}, &caCert);
		auto receiver = tlstest::MakeCert({"odc-receiver.example", sans, -3600, 86400LL*365, false, 101}, &caCert);
		auto other = tlstest::MakeCert({"odc-sender.example", sans, -3600, 86400LL*365, false, 300}, &otherCaCert);
		auto expired = tlstest::MakeCert({"odc-sender.example", sans, -7200, -3600, false, 400}, &caCert);
		ca = dir.Write("ca.crt",caCert.certPem);
		otherCa = dir.Write("otherca.crt",otherCaCert.certPem);
		senderCrt = dir.Write("sender.crt",sender.certPem);
		senderKey = dir.Write("sender.key",sender.keyPem);
		receiverCrt = dir.Write("receiver.crt",receiver.certPem);
		receiverKey = dir.Write("receiver.key",receiver.keyPem);
		otherCrt = dir.Write("other.crt",other.certPem);
		otherKey = dir.Write("other.key",other.keyPem);
		expiredCrt = dir.Write("expired.crt",expired.certPem);
		expiredKey = dir.Write("expired.key",expired.keyPem);
	}
};

ExPki& Pki()
{
	static ExPki pki;
	return pki;
}

Json::Value Certs(const std::string& crt, const std::string& key, const std::string& ca)
{
	Json::Value v;
	v["CertFile"] = crt;
	v["KeyFile"] = key;
	v["CAFile"] = ca;
	return v;
}
}

TEST_CASE(SUITE("TLS: every type arrives in order and the controls go back over mutual TLS"))
{
	auto& pki = Pki();
	Chain c("TLS",SocketSender("TLS",21020,Certs(pki.senderCrt,pki.senderKey,pki.ca)),SocketReceiver("TLS",21020,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: the Sender connected"));
	CHECK(c.rig.capture->Has("Sender: TLS session with CN=odc-receiver.example"));
	CHECK(c.rig.capture->Has("Receiver: TLS session with CN=odc-sender.example"));
	CheckStream(c);
	CheckControls(c);
}

TEST_CASE(SUITE("TLS: the Sender keeps events while the Receiver is away and the session comes back"))
{
	auto& pki = Pki();
	Chain c("TLS",SocketSender("TLS",21021,Certs(pki.senderCrt,pki.senderKey,pki.ca)),SocketReceiver("TLS",21021,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: the Sender connected"));
	for(size_t i = 0; i < 10; i++)
		c.Inject(Numbered(i));
	CheckNumbered(c,0,10);
	c.receiver->Disable();
	REQUIRE(Logged(c.rig,"connection to the Receiver ended"));
	for(size_t i = 10; i < 20; i++)
		c.Inject(Numbered(i));
	c.receiver->Enable();
	CheckNumbered(c,10,20);
	c.source.SetChooser(ChooseByIndex);
	auto r = c.Control(Crob(1,odc::ControlCode::PULSE_ON,1,100,100));
	REQUIRE(r);
	CHECK(*r == odc::CommandStatus::SUCCESS);
}

TEST_CASE(SUITE("TLS: a client without a valid certificate is refused and the real Sender still gets in afterwards"))
{
	auto& pki = Pki();
	RecorderPort source("RecSource");
	RecorderPort sink("RecSink");
	SinkPort reports;
	Rig rig;
	auto receiver = rig.Add("Receiver",SocketReceiver("TLS",21022,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	receiver->Subscribe(&sink,"Sink");
	receiver->Enable();
	const std::string line = EventLine("Counter",1,"5")+"\n";

	struct Attempt
	{
		const char* name;
		Json::Value tls;
	};
	Json::Value no_cert;
	no_cert["CAFile"] = pki.ca;
	Json::Value other_ca = Certs(pki.otherCrt,pki.otherKey,pki.ca);
	Json::Value expired = Certs(pki.expiredCrt,pki.expiredKey,pki.ca);
	Json::Value distrusts_server = Certs(pki.senderCrt,pki.senderKey,pki.otherCa);
	int n = 0;
	for(const auto& attempt : {Attempt{"no certificate",no_cert},Attempt{"a certificate from another CA",other_ca},Attempt{"an expired certificate",expired},Attempt{"does not trust the server",distrusts_server}
	    })
	{
		INFO(attempt.name);
		Json::Value tool_conf;
		tool_conf["Port"] = 21022;
		tool_conf["Tls"] = attempt.tls;
		Json::Value chunks(Json::arrayValue);
		chunks.append(line);
		tool_conf["Chunks"] = chunks;
		auto tool = rig.AddTool("Tool"+std::to_string(++n),SocketTool,tool_conf);
		tool->Subscribe(&reports,"Reports");
		tool->Enable();
		//the connection ends: refused during the handshake, or (TLS 1.3, where the client finishes its handshake first) as soon as the server has looked at the certificate
		std::string closed;
		REQUIRE(reports.Take("closed",closed,20000ms));
		INFO("closed: "+closed);
	}
	std::this_thread::sleep_for(300ms);
	CHECK(sink.Count() == 0); //nothing any of them wrote was taken
	CHECK(rig.capture->Has("Receiver: TLS failed: tls_failed"));
	CHECK(rig.capture->Has("peer did not return a certificate"));

	//the Sender, with the right certificate, is let in (one connection at a time: the refused ones are gone)
	auto sender = rig.Add("Sender",SocketSender("TLS",21022,Certs(pki.senderCrt,pki.senderKey,pki.ca)));
	sender->Subscribe(&source,"Source");
	sender->Enable();
	REQUIRE(Logged(rig,"Receiver: the Sender connected"));
	sender->Event(Numbered(77),"Source",std::make_shared<std::function<void (odc::CommandStatus)>>([](odc::CommandStatus){}));
	REQUIRE(sink.WaitForCount(1));
	CHECK(sink.Events()[0]->GetIndex() == 77);
}

TEST_CASE(SUITE("TLS: a Sender that does not trust the Receiver's certificate does not send"))
{
	auto& pki = Pki();
	Chain c("TLS",SocketSender("TLS",21023,Certs(pki.senderCrt,pki.senderKey,pki.otherCa)),SocketReceiver("TLS",21023,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	c.Enable();
	REQUIRE(Logged(c.rig,"TLS failed: tls_failed"));
	CHECK(c.rig.capture->Has("certificate verification failed"));
	c.Inject(Numbered(1));
	std::this_thread::sleep_for(500ms);
	CHECK(c.sink.Count() == 0);
	CHECK_FALSE(c.rig.capture->Has("Receiver: the Sender connected"));
}
#endif

#ifdef ODC_HTTPS_TESTS
TEST_CASE(SUITE("HTTPS: every type arrives in order and the controls go back over mutual TLS"))
{
	auto& pki = Pki();
	Chain c("HTTPS",HttpSender("HTTPS",21040,21041,Certs(pki.senderCrt,pki.senderKey,pki.ca)),HttpReceiver("HTTPS",21040,21041,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: listening on https://127.0.0.1:21041/events"));
	CheckStream(c);
	CheckControls(c);
}

TEST_CASE(SUITE("HTTPS: the Sender keeps the batch while the Receiver is away"))
{
	auto& pki = Pki();
	Chain c("HTTPS",HttpSender("HTTPS",21042,21043,Certs(pki.senderCrt,pki.senderKey,pki.ca)),HttpReceiver("HTTPS",21042,21043,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	c.Enable();
	REQUIRE(Logged(c.rig,"Receiver: listening on https://127.0.0.1:21043/events"));
	for(size_t i = 0; i < 5; i++)
		c.Inject(Numbered(i));
	CheckNumbered(c,0,5);
	//(the Receiver is stopped once the Sender has its answer: stopping it before the answer is out makes the Sender send the batch again)
	REQUIRE(Logged(c.rig,"Sender: HTTPClient https://127.0.0.1:21043: POST /events -> 200"));
	c.receiver->Disable();
	std::this_thread::sleep_for(300ms);
	for(size_t i = 5; i < 15; i++)
		c.Inject(Numbered(i));
	REQUIRE(Logged(c.rig,"cannot reach the Receiver"));
	c.receiver->Enable();
	CheckNumbered(c,5,15);
}

TEST_CASE(SUITE("HTTPS: a client without a valid certificate is refused and a client that does not trust the server cannot send"))
{
	auto& pki = Pki();
	RecorderPort sink("RecSink");
	SinkPort reports;
	Rig rig;
	auto receiver = rig.Add("Receiver",HttpReceiver("HTTPS",21044,21045,Certs(pki.receiverCrt,pki.receiverKey,pki.ca)));
	receiver->Subscribe(&sink,"Sink");
	receiver->Enable();
	REQUIRE(Logged(rig,"Receiver: listening on https://127.0.0.1:21045/events"));
	Json::Value bodies(Json::arrayValue);
	bodies.append("{\"v\":1,\"kind\":\"events\",\"events\":[{\"type\":\"Counter\",\"index\":1,\"quality\":1,\"time\":1,\"payload\":5}]}");

	Json::Value no_cert;
	no_cert["CAFile"] = pki.ca;
	struct Attempt
	{
		const char* name;
		Json::Value tls;
		std::vector<std::string> accepted; //what the client may see
	};
	const std::vector<std::string> rejected = {"err=tls_client_certificate_rejected","err=closed","err=tls_handshake_failed"};
	int n = 0;
	for(const auto& attempt : {Attempt{"no certificate",no_cert,rejected},Attempt{"a certificate from another CA",Certs(pki.otherCrt,pki.otherKey,pki.ca),rejected},
	                           Attempt{"an expired certificate",Certs(pki.expiredCrt,pki.expiredKey,pki.ca),rejected},
	                           Attempt{"does not trust the server",Certs(pki.senderCrt,pki.senderKey,pki.otherCa),{"err=tls_untrusted"}
					   }
	    })
	{
		INFO(attempt.name);
		Json::Value conf;
		conf["Url"] = "https://127.0.0.1:21045";
		conf["Tls"] = attempt.tls;
		conf["Bodies"] = bodies;
		auto tool = rig.AddTool("Tool"+std::to_string(++n),HttpTool,conf);
		tool->Subscribe(&reports,"Reports");
		tool->Enable();
		std::string r;
		REQUIRE(reports.Take("r1",r,20000ms));
		INFO("the client saw: "+r);
		CHECK(std::find(attempt.accepted.begin(),attempt.accepted.end(),r) != attempt.accepted.end());
	}
	std::this_thread::sleep_for(300ms);
	CHECK(sink.Count() == 0);

	//the right certificate is served
	Json::Value conf;
	conf["Url"] = "https://127.0.0.1:21045";
	conf["Tls"] = Certs(pki.senderCrt,pki.senderKey,pki.ca);
	conf["Bodies"] = bodies;
	auto tool = rig.AddTool("ToolGood",HttpTool,conf);
	tool->Subscribe(&reports,"Reports");
	tool->Enable();
	std::string r;
	REQUIRE(reports.Take("r1",r,20000ms));
	CHECK(r == "200:{\"accepted\":1,\"kind\":\"ack\",\"rejected\":0,\"v\":1}");
	REQUIRE(sink.WaitForCount(1));
}
#endif
