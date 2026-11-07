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
 * CurrentStateTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  DataPort::GetCurrentState() of the ICCP port (the UI command DataPorts CurrentState): the server side only. The last known value, quality and time of each point the
 *  port serves, and the state of the devices it serves; a port that only has a client side returns the default. Ports with a TCP listener on ports 21100 to 21119,
 *  driven through Event() (nothing connects to them). Synthetic names only.
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include <algorithm>
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <thread>

#define SUITE(name) "ICCP current state - " name

namespace
{
void CsSetupLoggers()
{
	static bool done = false;
	if(done)
		return;
	done = true;
	auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	sink->set_level(spdlog::level::warn);
	for(const char* name : {"ICCPPort", "opendatacon"})
	{
		if(odc::spdlog_get(name)) //another test file made it first
			continue;
		auto logger = std::make_shared<spdlog::logger>(name, sink);
		logger->set_level(spdlog::level::warn);
		odc::spdlog_register_logger(logger);
	}
}

template<typename Pred>
bool CsWaitFor(Pred&& pred, int timeoutMs = 10000)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while(!pred())
	{
		if(std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

//The configuration: a Server (no Direction on the points), or a Peer (Serve points and devices, and one Read point that only the client side has)
std::string CsConf(const std::string& role, int port, bool withPoints = true)
{
	const bool peer = role == "Peer";
	auto dir = [&](const char* d) { return peer ? std::string(", \"Direction\": \"") + d + "\"" : std::string(); };
	std::string conf = "{\"ServiceRole\": \"" + role + "\", \"IP\": \"127.0.0.1\", \"Port\": \"" + std::to_string(port) + "\", \"TASE2Edition\": \"2000-08\","
	                   " \"TCPConnectRetryPeriodms\": 200,"
	                   " \"LocalTSelector\": \"0001\", \"RemoteTSelector\": \"0002\", \"LocalSSelector\": \"0001\", \"RemoteSSelector\": \"0002\","
	                   " \"LocalPSelector\": \"00000001\", \"RemotePSelector\": \"00000002\","
	                   " \"LocalAPTitle\": \"1.3.9999.1.1\", \"RemoteAPTitle\": \"1.3.9999.1.2\", \"LocalAEQualifier\": 1, \"RemoteAEQualifier\": 2,"
	                   " \"BilateralTableID\": \"BT_TEST_A\", \"RemoteBilateralTableID\": \"BT_TEST_B\","
	                   " \"LocalDomain\": \"DOMAIN_A\", \"RemoteDomain\": \"DOMAIN_B\"";
	if(withPoints)
	{
		conf += ", \"Binaries\": [{\"Index\": 0, \"Name\": \"POINT_ST_01\", \"Type\": \"StateQ\"" + dir("Serve") + "},"
		        "{\"Index\": 1, \"Name\": \"POINT_ST_02\", \"Type\": \"StateQTimeTag\"" + dir("Serve") + "}";
		if(peer)
			conf += ",{\"Index\": 5, \"Name\": \"POINT_ST_READ\", \"Direction\": \"Read\"}";
		conf += "], \"Analogs\": [{\"Index\": 0, \"Name\": \"POINT_MW_01\"" + dir("Serve") + "},"
		        "{\"Index\": 1, \"Name\": \"POINT_MW_02\", \"ScaleFactor\": 0.5, \"Offset\": 10" + dir("Serve") + "},"
		        "{\"Index\": 2, \"Name\": \"POINT_MW_VCC\", \"Scope\": \"VCC\"" + dir("Serve") + "}";
		if(peer)
			conf += ",{\"Index\": 6, \"Name\": \"POINT_MW_READ\", \"Direction\": \"Read\"}";
		conf += "], \"Counters\": [{\"Index\": 0, \"Name\": \"POINT_CNT_01\"" + dir("Serve") + "}],"
		        " \"BinaryControls\": [{\"Index\": 0, \"Name\": \"DEV_DIR\", \"SelectRequired\": false" + std::string(peer || role == "Server" ? ", \"Taggable\": true" : "") + dir("Serve") + "},"
		        "{\"Index\": 1, \"Name\": \"DEV_PLAIN\", \"SelectRequired\": false" + dir("Serve") + "}";
		if(peer)
			conf += ",{\"Index\": 7, \"Name\": \"DEV_READ_SIDE\", \"Direction\": \"Operate\"}";
		conf += "], \"AnalogControls\": [{\"Index\": 0, \"Name\": \"DEV_SP\", \"SelectRequired\": false, \"Scope\": \"VCC\"" + dir("Serve") + "}]";
	}
	return conf + "}";
}

//an ODC data event into a port
std::shared_ptr<odc::EventInfo> CsBinary(size_t index, bool value, odc::QualityFlags q, odc::msSinceEpoch_t t)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::Binary, index, "SourcePort_X", q, t);
	e->SetPayload<odc::EventType::Binary>(std::move(value));
	return e;
}
std::shared_ptr<odc::EventInfo> CsAnalog(size_t index, double value, odc::QualityFlags q, odc::msSinceEpoch_t t)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::Analog, index, "SourcePort_X", q, t);
	e->SetPayload<odc::EventType::Analog>(std::move(value));
	return e;
}
std::shared_ptr<odc::EventInfo> CsCounter(size_t index, uint32_t value, odc::QualityFlags q, odc::msSinceEpoch_t t)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::Counter, index, "SourcePort_X", q, t);
	e->SetPayload<odc::EventType::Counter>(std::move(value));
	return e;
}

//one port on a running io service
struct CsFixture
{
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::string file;
	std::shared_ptr<ICCPPortBase> port;

	CsFixture(const std::string& role, int portNumber, bool server = true, bool build = true, bool withPoints = true)
	{
		CsSetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		file = "iccp_currentstate_" + std::to_string(portNumber) + ".conf";
		{
			std::ofstream ofs(file);
			REQUIRE(ofs.good());
			ofs << CsConf(role, portNumber, withPoints);
		}
		if(server)
			port = std::make_shared<ICCPServerPort>("NodeA", file, Json::Value(Json::objectValue));
		else
			port = std::make_shared<ICCPClientPort>("NodeA", file, Json::Value(Json::objectValue));
		if(build)
			port->Build();
	}
	~CsFixture()
	{
		if(port)
			port->Disable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		port.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(file.c_str());
	}
	//an ODC event into the port, and what it answers
	odc::CommandStatus Send(std::shared_ptr<odc::EventInfo> event)
	{
		auto promise = std::make_shared<std::promise<odc::CommandStatus>>();
		auto future = promise->get_future();
		auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([promise](odc::CommandStatus s) { promise->set_value(s); });
		port->Event(event, "TestHarness", cb);
		if(future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
			return odc::CommandStatus::TIMEOUT;
		return future.get();
	}
};

//the one section of a result: { <time>: { ... } }
Json::Value CsSection(const Json::Value& state)
{
	REQUIRE(state.isObject());
	REQUIRE(state.size() == 1);
	return state[state.getMemberNames()[0]];
}

//the entry of an array by its Name (a null value if there is none)
Json::Value CsFind(const Json::Value& section, const char* list, const std::string& name)
{
	for(const auto& e : section[list])
		if(e["Name"].asString() == name)
			return e;
	return Json::Value();
}

std::vector<std::string> CsNames(const Json::Value& section, const char* list)
{
	std::vector<std::string> names;
	for(const auto& e : section[list])
		names.push_back(e["Name"].asString());
	std::sort(names.begin(), names.end());
	return names;
}

std::string CsTime(odc::msSinceEpoch_t t)
{
	return odc::since_epoch_to_datetime(t);
}

const std::string NoValueQuality = odc::ToString(odc::QualityFlags::RESTART); //what the DNP3 port reports for a point it has not heard of yet

//the value written out and read back
Json::Value CsRoundTrip(const Json::Value& v)
{
	Json::StreamWriterBuilder wb;
	const std::string text = Json::writeString(wb, v);
	Json::CharReaderBuilder rb;
	Json::Value out;
	std::string errors;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(rb, in, &out, &errors));
	return out;
}
}

TEST_CASE(SUITE("a Server: every served point and device is listed, with no value until one is given"))
{
	CsFixture f("Server", 21100);
	REQUIRE(f.port->ConfigValid());
	f.port->Enable();
	const Json::Value state = f.port->GetCurrentState();
	const Json::Value s = CsSection(state);
	CHECK(CsNames(s, "Binaries") == std::vector<std::string>{"POINT_ST_01", "POINT_ST_02"});
	CHECK(CsNames(s, "Analogs") == std::vector<std::string>{"POINT_MW_01", "POINT_MW_02", "POINT_MW_VCC"});
	CHECK(CsNames(s, "Counters") == std::vector<std::string>{"POINT_CNT_01"});
	CHECK(CsNames(s, "Controls") == std::vector<std::string>{"DEV_DIR", "DEV_PLAIN", "DEV_SP"});
	//the key of the outer object is a date and time, as the other ports' is
	CHECK(state.getMemberNames()[0].size() == CsTime(0).size());
	//no value yet: as a point of the DNP3 port that was never set: no Value, the RESTART quality, and the time the port was built
	for(const char* list : {"Binaries", "Analogs", "Counters"})
		for(const auto& e : s[list])
		{
			INFO(list << " " << e["Name"].asString());
			CHECK_FALSE(e.isMember("Value"));
			CHECK(e["Quality"].asString() == NoValueQuality);
			CHECK(e["Quality"].asString() == "|RESTART|");
			CHECK(e["Timestamp"].isString());
			CHECK(e["Timestamp"].asString().size() == CsTime(0).size());
			CHECK_FALSE(e.isMember("SourcePort"));
		}
	const auto mw = CsFind(s, "Analogs", "POINT_MW_01");
	CHECK(mw["Index"].asUInt64() == 0);
	CHECK(mw["Scope"].asString() == "ICC");
	CHECK(mw["Domain"].asString() == "DOMAIN_A");
	const auto vcc = CsFind(s, "Analogs", "POINT_MW_VCC");
	CHECK(vcc["Index"].asUInt64() == 2);
	CHECK(vcc["Scope"].asString() == "VCC");
	CHECK_FALSE(vcc.isMember("Domain"));
	//the devices: the kind, the scope, whether the device has a tag (and, if so, it has none)
	const auto dir = CsFind(s, "Controls", "DEV_DIR");
	CHECK(dir["Index"].asUInt64() == 0);
	CHECK(dir["Kind"].asString() == "Binary");
	CHECK(dir["Scope"].asString() == "ICC");
	CHECK(dir["Domain"].asString() == "DOMAIN_A");
	CHECK(dir["Taggable"].asBool());
	CHECK(dir["Tag"].asString() == "NO_TAG");
	CHECK(dir["Reason"].asString() == "");
	CHECK(dir["SetBy"].asString() == "");
	const auto plain = CsFind(s, "Controls", "DEV_PLAIN");
	CHECK_FALSE(plain["Taggable"].asBool());
	CHECK_FALSE(plain.isMember("Tag"));
	const auto sp = CsFind(s, "Controls", "DEV_SP");
	CHECK(sp["Kind"].asString() == "Analog");
	CHECK(sp["Scope"].asString() == "VCC");
	CHECK_FALSE(sp.isMember("Domain"));
}

TEST_CASE(SUITE("values, quality, time and index follow the events; the latest update wins"))
{
	CsFixture f("Server", 21101);
	REQUIRE(f.port->ConfigValid());
	f.port->Enable();
	using Q = odc::QualityFlags;
	const odc::msSinceEpoch_t t0 = 1700000000123; //a fixed time of day
	CHECK(f.Send(CsBinary(0, true, Q::ONLINE, t0)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsBinary(1, false, Q::ONLINE | Q::LOCAL_FORCED, t0 + 1)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsAnalog(0, 12.5, Q::ONLINE, t0 + 2)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsAnalog(1, -3.25, Q::COMM_LOST, t0 + 3)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsCounter(0, 4000000000u, Q::ONLINE | Q::ROLLOVER, t0 + 4)) == odc::CommandStatus::SUCCESS);
	//an event for a point that is not configured is answered and changes nothing
	CHECK(f.Send(CsAnalog(99, 1.0, Q::ONLINE, t0)) == odc::CommandStatus::SUCCESS);

	const auto s = CsSection(f.port->GetCurrentState());
	CHECK(s["Analogs"].size() == 3);
	auto st = CsFind(s, "Binaries", "POINT_ST_01");
	CHECK(st["Index"].asUInt64() == 0);
	CHECK(st["Value"].isBool());
	CHECK(st["Value"].asBool() == true);
	CHECK(st["Quality"].asString() == odc::ToString(Q::ONLINE));
	CHECK(st["Timestamp"].asString() == CsTime(t0));
	st = CsFind(s, "Binaries", "POINT_ST_02");
	CHECK(st["Value"].asBool() == false);
	CHECK(st["Quality"].asString() == odc::ToString(Q::ONLINE | Q::LOCAL_FORCED));
	CHECK(st["Timestamp"].asString() == CsTime(t0 + 1));
	auto an = CsFind(s, "Analogs", "POINT_MW_01");
	CHECK(an["Value"].asDouble() == 12.5); //the ODC value as the event gave it: the scaling is for the wire only
	CHECK(an["Quality"].asString() == odc::ToString(Q::ONLINE));
	CHECK(an["Timestamp"].asString() == CsTime(t0 + 2));
	an = CsFind(s, "Analogs", "POINT_MW_02");
	CHECK(an["Value"].asDouble() == -3.25);
	CHECK(an["Quality"].asString() == odc::ToString(Q::COMM_LOST));
	an = CsFind(s, "Analogs", "POINT_MW_VCC"); //the third is still without a value
	CHECK_FALSE(an.isMember("Value"));
	CHECK(an["Quality"].asString() == NoValueQuality);
	const auto cnt = CsFind(s, "Counters", "POINT_CNT_01");
	CHECK(cnt["Value"].asUInt64() == 4000000000u);
	CHECK(cnt["Quality"].asString() == odc::ToString(Q::ONLINE | Q::ROLLOVER));
	CHECK(cnt["Timestamp"].asString() == CsTime(t0 + 4));

	//a later update replaces the value, the quality and the time; an update that changes nothing but the time still moves the time
	CHECK(f.Send(CsAnalog(0, 99.0, Q::ONLINE | Q::OVERRANGE, t0 + 1000)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsBinary(0, true, Q::ONLINE, t0 + 2000)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsBinary(0, false, Q::ONLINE, t0 + 3000)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsBinary(0, false, Q::ONLINE, t0 + 4000)) == odc::CommandStatus::SUCCESS);
	const auto s2 = CsSection(f.port->GetCurrentState());
	an = CsFind(s2, "Analogs", "POINT_MW_01");
	CHECK(an["Value"].asDouble() == 99.0);
	CHECK(an["Quality"].asString() == odc::ToString(Q::ONLINE | Q::OVERRANGE));
	CHECK(an["Timestamp"].asString() == CsTime(t0 + 1000));
	st = CsFind(s2, "Binaries", "POINT_ST_01");
	CHECK(st["Value"].asBool() == false);
	CHECK(st["Timestamp"].asString() == CsTime(t0 + 4000));
	CHECK(CsFind(s2, "Analogs", "POINT_MW_02")["Value"].asDouble() == -3.25); //the others are as they were
	CHECK(CsFind(s2, "Binaries", "POINT_ST_02")["Value"].asBool() == false);
}

TEST_CASE(SUITE("the result is valid JSON that survives being written and read"))
{
	CsFixture f("Server", 21102);
	f.port->Enable();
	CHECK(f.Send(CsAnalog(0, 0.1, odc::QualityFlags::ONLINE, 1700000000000)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsBinary(1, true, odc::QualityFlags::NONE, 1700000000001)) == odc::CommandStatus::SUCCESS);
	const Json::Value state = f.port->GetCurrentState();
	REQUIRE(state.isObject());
	//(compared as text: the reader makes an int of a small unsigned number, which is a different Json type with the same value)
	Json::StreamWriterBuilder wb;
	CHECK(Json::writeString(wb, CsRoundTrip(state)) == Json::writeString(wb, state));
	const auto s = CsSection(CsRoundTrip(state));
	CHECK(CsFind(s, "Binaries", "POINT_ST_02")["Quality"].asString() == "|NONE|");
	CHECK(CsFind(s, "Analogs", "POINT_MW_01")["Value"].asDouble() == 0.1);
}

TEST_CASE(SUITE("a Peer returns the points and devices it serves, not those of its client side"))
{
	CsFixture f("Peer", 21103);
	REQUIRE(f.port->ConfigValid());
	f.port->Enable();
	CHECK(f.Send(CsBinary(0, true, odc::QualityFlags::ONLINE, 1700000000000)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsAnalog(1, 7.0, odc::QualityFlags::ONLINE, 1700000000001)) == odc::CommandStatus::SUCCESS);
	//ODC events with the indexes of the Read points are not served values: they do not appear
	CHECK(f.Send(CsBinary(5, true, odc::QualityFlags::ONLINE, 1700000000002)) == odc::CommandStatus::SUCCESS);
	CHECK(f.Send(CsAnalog(6, 5.0, odc::QualityFlags::ONLINE, 1700000000003)) == odc::CommandStatus::SUCCESS);
	const auto s = CsSection(f.port->GetCurrentState());
	CHECK(CsNames(s, "Binaries") == std::vector<std::string>{"POINT_ST_01", "POINT_ST_02"});
	CHECK(CsNames(s, "Analogs") == std::vector<std::string>{"POINT_MW_01", "POINT_MW_02", "POINT_MW_VCC"});
	CHECK(CsNames(s, "Counters") == std::vector<std::string>{"POINT_CNT_01"});
	CHECK(CsNames(s, "Controls") == std::vector<std::string>{"DEV_DIR", "DEV_PLAIN", "DEV_SP"}); //DEV_READ_SIDE is operated on the peer: not here
	CHECK(CsFind(s, "Binaries", "POINT_ST_READ").isNull());
	CHECK(CsFind(s, "Analogs", "POINT_MW_READ").isNull());
	CHECK(CsFind(s, "Controls", "DEV_READ_SIDE").isNull());
	CHECK(CsFind(s, "Binaries", "POINT_ST_01")["Value"].asBool() == true);
	CHECK(CsFind(s, "Analogs", "POINT_MW_02")["Value"].asDouble() == 7.0);
}

TEST_CASE(SUITE("a port that is only a client returns the default value, as it did before"))
{
	CsFixture f("Client", 21104, false);
	REQUIRE(f.port->ConfigValid());
	CHECK(f.port->GetCurrentState() == Json::Value());
	CHECK(f.port->GetCurrentState().isNull());
	f.port->Enable();
	CHECK(f.port->GetCurrentState().isNull());
	f.port->Disable();
	CHECK(f.port->GetCurrentState().isNull());
}

TEST_CASE(SUITE("a port that is not built, not enabled or disabled again gives the same, well formed, result"))
{
	{
		//constructed, never built: no model yet
		CsFixture f("Server", 21105, true, false);
		const auto s = CsSection(f.port->GetCurrentState());
		CHECK(s["Binaries"].size() == 2);
		CHECK(s["Analogs"].size() == 3);
		CHECK(s["Counters"].size() == 1);
		CHECK(s["Controls"].size() == 3);
		for(const auto& e : s["Binaries"])
		{
			CHECK_FALSE(e.isMember("Value"));
			CHECK(e["Quality"].asString() == NoValueQuality);
		}
		CHECK(CsFind(s, "Controls", "DEV_DIR")["Tag"].asString() == "NO_TAG");
		//an event to a port that is not enabled is refused, and changes nothing
		CHECK(f.Send(CsAnalog(0, 1.0, odc::QualityFlags::ONLINE, 1700000000000)) == odc::CommandStatus::UNDEFINED);
		CHECK_FALSE(CsFind(CsSection(f.port->GetCurrentState()), "Analogs", "POINT_MW_01").isMember("Value"));
	}
	{
		//built, not enabled
		CsFixture f("Server", 21106);
		const auto s = CsSection(f.port->GetCurrentState());
		CHECK(s["Analogs"].size() == 3);
		CHECK_FALSE(CsFind(s, "Analogs", "POINT_MW_01").isMember("Value"));
		CHECK(f.Send(CsAnalog(0, 1.0, odc::QualityFlags::ONLINE, 1700000000000)) == odc::CommandStatus::UNDEFINED);
		//enabled, given a value, disabled: the last values are still there
		f.port->Enable();
		CHECK(f.Send(CsAnalog(0, 42.0, odc::QualityFlags::ONLINE, 1700000000000)) == odc::CommandStatus::SUCCESS);
		f.port->Disable();
		CHECK(CsFind(CsSection(f.port->GetCurrentState()), "Analogs", "POINT_MW_01")["Value"].asDouble() == 42.0);
		f.port->Enable();
		CHECK(CsFind(CsSection(f.port->GetCurrentState()), "Analogs", "POINT_MW_01")["Value"].asDouble() == 42.0);
	}
	{
		//a Server with no points at all: the structure is there, empty
		CsFixture f("Server", 21107, true, true, false);
		const auto s = CsSection(f.port->GetCurrentState());
		for(const char* list : {"Binaries", "Analogs", "Counters", "Controls"})
		{
			INFO(list);
			CHECK(s[list].isArray());
			CHECK(s[list].empty());
		}
	}
}

TEST_CASE(SUITE("the tag of a device follows a local tag"))
{
	CsFixture f("Server", 21108);
	f.port->Enable();
	CHECK(f.port->SetLocalTag("DEV_DIR", iccp::tase2::DeviceTag::OpenAndCloseInhibit, "WORK ON FEEDER 7").empty());
	REQUIRE(CsWaitFor([&] { return CsFind(CsSection(f.port->GetCurrentState()), "Controls", "DEV_DIR")["Tag"].asString() == "OPEN_AND_CLOSE_INHIBIT"; }));
	auto dev = CsFind(CsSection(f.port->GetCurrentState()), "Controls", "DEV_DIR");
	CHECK(dev["Reason"].asString() == "WORK ON FEEDER 7");
	CHECK(dev["SetBy"].asString() == "Local");
	CHECK(CsFind(CsSection(f.port->GetCurrentState()), "Controls", "DEV_PLAIN").isMember("Tag") == false);
	//a device that is not taggable cannot be given one, and is still listed without
	CHECK_FALSE(f.port->SetLocalTag("DEV_PLAIN", iccp::tase2::DeviceTag::OpenAndCloseInhibit, "").empty());
	CHECK(f.port->SetLocalTag("DEV_DIR", iccp::tase2::DeviceTag::NoTag, "").empty());
	REQUIRE(CsWaitFor([&] { return CsFind(CsSection(f.port->GetCurrentState()), "Controls", "DEV_DIR")["Tag"].asString() == "NO_TAG"; }));
	dev = CsFind(CsSection(f.port->GetCurrentState()), "Controls", "DEV_DIR");
	CHECK(dev["Reason"].asString() == "");
	CHECK(dev["SetBy"].asString() == "");
}

TEST_CASE(SUITE("GetCurrentState called from other threads while events and tags are applied"))
{
	CsFixture f("Peer", 21109);
	REQUIRE(f.port->ConfigValid());
	f.port->Enable();
	const odc::msSinceEpoch_t base = 1700000000000;
	const int updates = 4000;
	std::atomic_bool finished{false};
	std::atomic<int> problems{0};
	std::atomic<int> reads{0};

	//the writer: the analog 0, the counter 0 and the binary 0 take the value k at the time base + k; the tag of a device changes now and then
	std::thread writer([&]
		{
			auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([](odc::CommandStatus) {});
			for(int k = 1; k <= updates; ++k)
			{
				f.port->Event(CsAnalog(0, k, odc::QualityFlags::ONLINE, base + k), "TestHarness", cb);
				f.port->Event(CsCounter(0, static_cast<uint32_t>(k), odc::QualityFlags::ONLINE, base + k), "TestHarness", cb);
				f.port->Event(CsBinary(0, k % 2 == 0, odc::QualityFlags::ONLINE, base + k), "TestHarness", cb);
				if(k % 50 == 0)
					f.port->SetLocalTag("DEV_DIR", k % 100 == 0 ? iccp::tase2::DeviceTag::NoTag : iccp::tase2::DeviceTag::CloseOnlyInhibit, "R" + std::to_string(k));
				if(k % 20 == 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); //let the port's strand keep up
			}
			finished = true;
		});
	auto reader = [&]
			  {
				  double lastAnalog = 0;
				  double lastCounter = 0;
				  while(!finished.load() || reads.load() < 200)
				  {
					  const Json::Value state = f.port->GetCurrentState();
					  ++reads;
					  if(!state.isObject() || state.size() != 1)
					  {
						  ++problems;
						  continue;
					  }
					  const Json::Value& s = state[state.getMemberNames()[0]];
					  const auto an = CsFind(s, "Analogs", "POINT_MW_01");
					  const auto cn = CsFind(s, "Counters", "POINT_CNT_01");
					  const auto bn = CsFind(s, "Binaries", "POINT_ST_01");
					  if(an.isMember("Value"))
					  {
						  const double k = an["Value"].asDouble();
						  //an entry is one update: the time is that of the value, and a value never goes back
						  if(an["Timestamp"].asString() != CsTime(base + static_cast<odc::msSinceEpoch_t>(k)) || k < lastAnalog)
							  ++problems;
						  lastAnalog = k;
					  }
					  else if(lastAnalog != 0)
						  ++problems;
					  if(cn.isMember("Value"))
					  {
						  const double k = cn["Value"].asDouble();
						  if(cn["Timestamp"].asString() != CsTime(base + static_cast<odc::msSinceEpoch_t>(k)) || k < lastCounter)
							  ++problems;
						  lastCounter = k;
					  }
					  if(bn.isMember("Value"))
					  {
						  //the binary is true for the even k: the time says which k
						  const auto k = odc::datetime_to_since_epoch(bn["Timestamp"].asString()) - base;
						  if(k < 1 || k > updates || bn["Value"].asBool() != (k % 2 == 0))
							  ++problems;
					  }
					  if(CsFind(s, "Controls", "DEV_DIR")["Tag"].asString().empty())
						  ++problems;
				  }
			  };
	std::thread r1(reader), r2(reader);
	writer.join();
	r1.join();
	r2.join();
	CHECK(problems.load() == 0);
	CHECK(reads.load() >= 200);
	//everything the writer sent arrives: the last values are the last updates
	REQUIRE(CsWaitFor([&]
		{
			return CsFind(CsSection(f.port->GetCurrentState()), "Analogs", "POINT_MW_01")["Value"].asDouble() == updates &&
			       CsFind(CsSection(f.port->GetCurrentState()), "Counters", "POINT_CNT_01")["Value"].asUInt64() == static_cast<uint64_t>(updates) &&
			       CsFind(CsSection(f.port->GetCurrentState()), "Binaries", "POINT_ST_01")["Timestamp"].asString() == CsTime(base + updates);
		}, 30000));
	const auto s = CsSection(f.port->GetCurrentState());
	CHECK(CsFind(s, "Binaries", "POINT_ST_01")["Value"].asBool() == true); //4000 is even
	CHECK(CsFind(s, "Controls", "DEV_DIR")["Tag"].isString());

	//and while the port is disabled and enabled again
	std::atomic_bool stop{false};
	std::atomic<int> bad{0};
	std::thread watcher([&]
		{
			while(!stop.load())
			{
				const Json::Value state = f.port->GetCurrentState();
				if(!state.isObject() || state.size() != 1)
					++bad;
			}
		});
	for(int i = 0; i < 5; ++i)
	{
		f.port->Disable();
		f.port->Enable();
	}
	stop = true;
	watcher.join();
	CHECK(bad.load() == 0);
}

TEST_CASE(SUITE("reading while a burst of events is queued and the port is disabled"))
{
	//a reading thread and the port's strand with work still queued when the port is disabled and the last owner lets go
	CsFixture f("Server", 21110);
	f.port->Enable();
	std::atomic_bool stop{false};
	std::atomic<int> bad{0};
	auto port = f.port;
	std::thread reader([&]
		{
			while(!stop.load())
			{
				const Json::Value state = port->GetCurrentState();
				if(!state.isObject())
					++bad;
			}
		});
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([](odc::CommandStatus) {});
	for(int k = 0; k < 500; ++k)
		port->Event(CsAnalog(0, k, odc::QualityFlags::ONLINE, 1700000000000 + k), "TestHarness", cb);
	port->Disable();
	stop = true;
	reader.join();
	CHECK(bad.load() == 0);
	port.reset();
}
