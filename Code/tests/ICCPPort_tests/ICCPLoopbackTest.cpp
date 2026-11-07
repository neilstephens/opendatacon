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
 * ICCPLoopbackTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  An ICCPServerPort and an ICCPClientPort talking to each other over a real TCP connection on 127.0.0.1.
 *  Synthetic names only.
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <optional>
#include <opendatacon/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP loopback - " name

namespace
{
//ConfigParser caches files by name for the life of the process: every test needs file names of its own
std::string ConfFileName(const char* role, int tag)
{
	return std::string("iccp_loopback_") + role + "_" + std::to_string(tag) + ".conf";
}

void SetupLoggers()
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

void WriteFile(const char* file, const std::string& text)
{
	std::ofstream ofs(file);
	REQUIRE(ofs.good());
	ofs << text;
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
	for(size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size()))
		text.replace(pos, from.size(), to);
	return text;
}

//the server's and client's configuration; %PORT% etc. are substituted
const std::string DefaultTransferSet = R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange", "IntegrityTimeOut"], "IntegrityCheck": 30})";

const char* ServerConf = R"({
	"IP": "127.0.0.1", "Port": "%PORT%", "TASE2Edition": "%EDITION%",
	"LocalTSelector": "0001", "RemoteTSelector": "0002", "LocalSSelector": "0001", "RemoteSSelector": "0002",
	"LocalPSelector": "00000001", "RemotePSelector": "00000002",
	"LocalAPTitle": "1.3.9999.1.1", "RemoteAPTitle": "1.3.9999.1.2", "LocalAEQualifier": 1, "RemoteAEQualifier": 2,
	"BilateralTableID": "BT_TEST_A", "RemoteBilateralTableID": "BT_TEST_B",
	"LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
	"VendorName": "VENDOR_X", "ModelName": "MODEL_Y", "Revision": "REV_Z",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"Counters": [{"Index": 0, "Name": "POINT_0003_CNT"}],
	"TransferSets": [{"Name": "TS_POOL_001"}, {"Name": "TS_POOL_002"}]
})";

const char* ClientConf = R"({
	"IP": "127.0.0.1", "Port": "%PORT%", "TASE2Edition": "%EDITION%", "TCPConnectRetryPeriodms": 200,
	"LocalTSelector": "0002", "RemoteTSelector": "0001", "LocalSSelector": "0002", "RemoteSSelector": "0001",
	"LocalPSelector": "00000002", "RemotePSelector": "00000001",
	"LocalAPTitle": "1.3.9999.1.2", "RemoteAPTitle": "1.3.9999.1.1", "LocalAEQualifier": 2, "RemoteAEQualifier": 1,
	"BilateralTableID": "BT_TEST_B", "RemoteBilateralTableID": "%REMOTE_BT%",
	"LocalDomain": "DOMAIN_B", "RemoteDomain": "DOMAIN_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "ScaleFactor": 0.5, "Offset": 10}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"Counters": [{"Index": 0, "Name": "POINT_0003_CNT"}],
	"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"]},
		{"Name": "DS_TEST_002", "Members": ["POINT_0003_CNT"], "PollIntervalms": 300}],
	"TransferSets": [%CLIENT_TRANSFER_SETS%]
})";

class Recorder: public odc::IOHandler
{
public:
	struct Rec
	{
		odc::EventType type;
		size_t index;
		double value;
		odc::QualityFlags quality;
		odc::msSinceEpoch_t timestamp;
	};
	Recorder(): odc::IOHandler("Recorder")
	{}
	void Event(odc::ConnectState state, const std::string&) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		states.push_back(state);
	}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t cb) override
	{
		//connection state changes arrive here too (as well as through the ConnectState overload): not data
		if(event->GetEventType() != odc::EventType::ConnectState)
		{
			std::lock_guard<std::mutex> lock(mtx);
			Rec r{event->GetEventType(), event->GetIndex(), 0, event->GetQuality(), event->GetTimestamp()};
			switch(r.type)
			{
				case odc::EventType::Binary: r.value = event->GetPayload<odc::EventType::Binary>() ? 1 : 0; break;
				case odc::EventType::Analog: r.value = event->GetPayload<odc::EventType::Analog>(); break;
				case odc::EventType::Counter: r.value = event->GetPayload<odc::EventType::Counter>(); break;
				default: break;
			}
			events.push_back(r);
		}
		(*cb)(odc::CommandStatus::SUCCESS);
	}
	void Enable() override
	{}
	void Disable() override
	{}

	size_t Count(odc::ConnectState s)
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(auto x : states)
			n += (x == s) ? 1 : 0;
		return n;
	}
	size_t EventCount()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return events.size();
	}
	//every point event, in the order it arrived
	std::vector<Rec> Events()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return events;
	}
	//the latest record for a point
	std::optional<Rec> Last(odc::EventType type, size_t index)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(auto it = events.rbegin(); it != events.rend(); ++it)
			if(it->type == type && it->index == index)
				return *it;
		return std::nullopt;
	}
	bool LastIs(odc::EventType type, size_t index, double value)
	{
		const auto r = Last(type, index);
		return r && r->value == value;
	}

private:
	std::mutex mtx;
	std::vector<odc::ConnectState> states;
	std::vector<Rec> events;
};

template<typename Pred>
bool WaitFor(Pred&& pred, int timeoutMs = 10000)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while(!pred())
	{
		if(std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return true;
}

void Feed(ICCPPortBase& server, odc::EventType type, size_t index, double value, odc::msSinceEpoch_t ts,
	odc::QualityFlags q = odc::QualityFlags::ONLINE)
{
	auto event = std::make_shared<odc::EventInfo>(type, index, "TestHarness", q, ts);
	switch(type)
	{
		case odc::EventType::Binary: event->SetPayload<odc::EventType::Binary>(value != 0); break;
		case odc::EventType::Analog: event->SetPayload<odc::EventType::Analog>(double(value)); break;
		case odc::EventType::Counter: event->SetPayload<odc::EventType::Counter>(uint32_t(value)); break;
		default: break;
	}
	std::atomic_bool done{false};
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([&done](odc::CommandStatus) { done = true; });
	server.Event(event, "TestHarness", cb);
	REQUIRE(WaitFor([&] { return done.load(); }));
}

//the ODC threads, and the ports, for one test
struct Harness
{
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::shared_ptr<ICCPServerPort> server;
	std::shared_ptr<ICCPClientPort> client;
	Recorder recorder;

	const std::string serverFile;
	const std::string clientFile;

	Harness(int port, const std::string& remoteBt = "BT_TEST_A", const std::string& clientTransferSets = DefaultTransferSet,
		const std::string& edition = "1996-08", const Json::Value& extraClientOverrides = Json::Value(Json::objectValue),
		const Json::Value& extraServerOverrides = Json::Value(Json::objectValue)):
		serverFile(ConfFileName("server", port)),
		clientFile(ConfFileName("client", port))
	{
		SetupLoggers();
		//a service left over from an earlier test (something kept it alive) would be stopped
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		const std::string p = std::to_string(port);
		WriteFile(serverFile.c_str(), Replace(Replace(ServerConf, "%PORT%", p), "%EDITION%", edition));
		WriteFile(clientFile.c_str(), Replace(Replace(Replace(Replace(ClientConf, "%PORT%", p), "%REMOTE_BT%", "BT_TEST_A"), "%CLIENT_TRANSFER_SETS%", clientTransferSets), "%EDITION%", edition));
		//opendatacon passes "ConfOverrides" : { } (an empty object, not null) for most ports; a non-empty one must replace
		//just the keys it names, whatever the file says
		Json::Value serverOverrides(Json::objectValue);
		Json::Value clientOverrides(Json::objectValue);
		if(remoteBt != "BT_TEST_A")
			clientOverrides["RemoteBilateralTableID"] = remoteBt;
		for(const auto& key : extraClientOverrides.getMemberNames())
			clientOverrides[key] = extraClientOverrides[key];
		for(const auto& key : extraServerOverrides.getMemberNames())
			serverOverrides[key] = extraServerOverrides[key];
		server = std::make_shared<ICCPServerPort>("TestServer", serverFile, serverOverrides);
		client = std::make_shared<ICCPClientPort>("TestClient", clientFile, clientOverrides);
		server->Build();
		client->Build();
		client->Subscribe(&recorder, "Recorder");
	}
	~Harness()
	{
		if(client)
			client->Disable();
		if(server)
			server->Disable();
		WaitFor([&] { return !client->AssociationUp() && !server->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		client.reset();
		server.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(serverFile.c_str());
		std::remove(clientFile.c_str());
	}
};
}

TEST_CASE(SUITE("association, data set, transfer set and reports"))
{
	Harness h(20301);
	REQUIRE(h.server->ConfigValid());
	REQUIRE(h.client->ConfigValid());
	h.server->Enable();
	//values the server has before the client arrives
	constexpr odc::msSinceEpoch_t Ts1 = 1700000000000; //time tags are whole seconds on the wire
	Feed(*h.server, odc::EventType::Binary, 0, 1, Ts1);
	Feed(*h.server, odc::EventType::Analog, 0, 100.5, Ts1);
	Feed(*h.server, odc::EventType::Analog, 1, -7.25, Ts1, odc::QualityFlags::ONLINE | odc::QualityFlags::LOCAL_FORCED);
	Feed(*h.server, odc::EventType::Counter, 0, 42, Ts1);

	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(h.client->AssociationUp());
	REQUIRE(h.server->AssociationUp());

	//the integrity report that follows the start of the transfer set, and the polled data set
	REQUIRE(WaitFor([&]
		{
			return h.recorder.LastIs(odc::EventType::Binary, 0, 1) && h.recorder.LastIs(odc::EventType::Analog, 1, -7.25) &&
			       h.recorder.LastIs(odc::EventType::Counter, 0, 42);
		}));
	//client scaling: value * 0.5 + 10
	REQUIRE(h.recorder.LastIs(odc::EventType::Analog, 0, 100.5 * 0.5 + 10));
	const auto a1 = h.recorder.Last(odc::EventType::Analog, 1);
	CHECK(a1->quality == (odc::QualityFlags::ONLINE | odc::QualityFlags::LOCAL_FORCED));
	CHECK(a1->timestamp == Ts1);
	const auto b = h.recorder.Last(odc::EventType::Binary, 0);
	CHECK(b->quality == odc::QualityFlags::ONLINE);
	CHECK(b->timestamp == Ts1);

	//an object change arrives as an unsolicited report
	constexpr odc::msSinceEpoch_t Ts2 = 1700000005000;
	Feed(*h.server, odc::EventType::Analog, 1, 33.5, Ts2);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 1, 33.5); }));
	CHECK(h.recorder.Last(odc::EventType::Analog, 1)->timestamp == Ts2);
	Feed(*h.server, odc::EventType::Binary, 0, 0, Ts2, odc::QualityFlags::COMM_LOST);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Binary, 0, 0); }));
	CHECK(h.recorder.Last(odc::EventType::Binary, 0)->quality != odc::QualityFlags::ONLINE);

	//the polled data set follows the counter without any report for it
	Feed(*h.server, odc::EventType::Counter, 0, 43, Ts2);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Counter, 0, 43); }));

	//the provisioning state of the client's data sets (GetStatus)
	{
		const Json::Value st = h.client->GetStatus();
		REQUIRE(st["Provisioning"].size() == 2);
		CHECK(st["Provisioning"][0]["DataSet"].asString() == "DS_TEST_001");
		CHECK(st["Provisioning"][0]["State"].asString() == "transfer-started");
		CHECK(st["Provisioning"][0]["Members"].asUInt64() == 5); //two special objects and three points
		CHECK(st["Provisioning"][0]["TransferSet"].asString().rfind("TS_POOL_", 0) == 0);
		CHECK_FALSE(st["Provisioning"][0].isMember("LastError"));
		CHECK_FALSE(st["Provisioning"][0].isMember("Probe"));
		CHECK(st["Provisioning"][1]["DataSet"].asString() == "DS_TEST_002");
		CHECK(st["Provisioning"][1]["State"].asString() == "defined");
		CHECK_FALSE(h.server->GetStatus().isMember("Provisioning"));
	}

	CHECK(h.server->GetStatistics()["Associations"].asUInt64() == 1);

	//disabling the client stops the transfer set and concludes the association
	h.client->Disable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
	REQUIRE(WaitFor([&] { return !h.server->AssociationUp(); }));
}

TEST_CASE(SUITE("a rejected data set definition is diagnosed: the probe names the members the peer lacks (policy Fail)"))
{
	Json::Value overrides(Json::objectValue);
	Json::CharReaderBuilder builder;
	std::string errs;
	const std::string text = R"({
		"MissingMemberPolicy": "Fail",
		"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"},
			{"Index": 5, "Name": "POINT_9001_ST"}, {"Index": 6, "Name": "POINT_9002_ST"}],
		"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_9001_ST", "POINT_0001_MW", "POINT_9002_ST"]}]
	})";
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(builder, in, &overrides, &errs));

	Harness h(20321, "BT_TEST_A", DefaultTransferSet, "1996-08", overrides);
	REQUIRE(h.server->ConfigValid());
	REQUIRE(h.client->ConfigValid());
	h.server->Enable();
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	//the probe is the last step: its result is in the status
	REQUIRE(WaitFor([&]
		{
			const Json::Value st = h.client->GetStatus();
			return st["Provisioning"].size() == 1 && st["Provisioning"][0].isMember("Probe");
		}));
	const Json::Value st = h.client->GetStatus()["Provisioning"][0];
	CHECK(st["DataSet"].asString() == "DS_TEST_001");
	CHECK(st["Domain"].asString() == "DOMAIN_A");
	CHECK(st["State"].asString() == "define-failed");
	CHECK(st["Members"].asUInt64() == 6); //two special objects and four points
	CHECK(st["LastError"].asString().find("definition [2] / object-undefined [1]") != std::string::npos);
	CHECK(st["Probe"]["Complete"].asBool());
	CHECK(st["Probe"]["Probed"].asUInt64() == 6);
	REQUIRE(st["Probe"]["Missing"].size() >= 2);
	//the two points the server does not have, with their position in the data set and their ODC index
	bool found1 = false, found2 = false;
	for(const auto& m : st["Probe"]["Missing"])
	{
		if(m["Name"].asString() == "POINT_9001_ST")
		{
			found1 = true;
			CHECK(m["Position"].asUInt64() == 3);
			CHECK(m["Kind"].asString() == "Binary");
			CHECK(m["Index"].asUInt64() == 5);
		}
		if(m["Name"].asString() == "POINT_9002_ST")
		{
			found2 = true;
			CHECK(m["Position"].asUInt64() == 5);
			CHECK(m["Index"].asUInt64() == 6);
		}
	}
	CHECK(found1);
	CHECK(found2);
	CHECK(st["Probe"]["MissingMembers"].asUInt64() + st["Probe"]["PresentMembers"].asUInt64() + st["Probe"]["PresentButNotReadable"].asUInt64() == 6);
}

TEST_CASE(SUITE("members the peer lacks are pruned from the data set, their points flagged, the rest reported (policy Prune, the default)"))
{
	Json::Value overrides(Json::objectValue);
	Json::CharReaderBuilder builder;
	std::string errs;
	const std::string text = R"({
		"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"},
			{"Index": 5, "Name": "POINT_9001_ST"}, {"Index": 6, "Name": "POINT_9002_ST"}],
		"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_9001_ST", "POINT_0001_MW", "POINT_9002_ST"]}]
	})";
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(builder, in, &overrides, &errs));

	Harness h(20322, "BT_TEST_A", DefaultTransferSet, "1996-08", overrides);
	REQUIRE(h.client->ConfigValid());
	h.server->Enable();
	constexpr odc::msSinceEpoch_t Ts1 = 1700000000000;
	Feed(*h.server, odc::EventType::Binary, 0, 1, Ts1);
	Feed(*h.server, odc::EventType::Analog, 0, 100.5, Ts1);
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));

	//the values of the members the peer has arrive, scaled; the two points it lacks are flagged, once each, never with a value
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Binary, 0, 1) && h.recorder.LastIs(odc::EventType::Analog, 0, 100.5 * 0.5 + 10); }));
	REQUIRE(WaitFor([&] { return h.recorder.Last(odc::EventType::Binary, 5).has_value() && h.recorder.Last(odc::EventType::Binary, 6).has_value(); }));
	for(size_t index : {5u, 6u})
	{
		const auto r = h.recorder.Last(odc::EventType::Binary, index);
		CHECK(r->quality == odc::QualityFlags::COMM_LOST);
		CHECK(r->value == 0);
	}
	//a later report does not turn them into values
	Feed(*h.server, odc::EventType::Analog, 0, 7.5, 1700000005000);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 0, 7.5 * 0.5 + 10); }));
	CHECK(h.recorder.Last(odc::EventType::Binary, 5)->quality == odc::QualityFlags::COMM_LOST);

	const Json::Value st = h.client->GetStatus()["Provisioning"][0];
	CHECK(st["State"].asString() == "transfer-started");
	CHECK(st["Partial"].asBool());
	CHECK(st["Members"].asUInt64() == 6);
	CHECK(st["MembersDefined"].asUInt64() == 4); //the two special objects and the two members the peer has
	CHECK(st["Probe"]["MissingMembers"].asUInt64() == 2);
	CHECK(st["Probe"]["Missing"].size() == 2);
	CHECK(WaitFor([&] { return h.client->GetStatistics()["UnavailablePoints"].asUInt64() == 2; })); //updated by the 50 ms tick
}

TEST_CASE(SUITE("reconnect after the server drops"))
{
	Harness h(20302);
	h.server->Enable();
	Feed(*h.server, odc::EventType::Analog, 0, 1.5, 1700000000000);
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 0, 1.5 * 0.5 + 10); }));

	h.server->Disable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return !h.server->AssociationUp(); }));

	//the server returns; the client re-associates and gets the (changed) values again
	h.server->Enable();
	Feed(*h.server, odc::EventType::Analog, 0, 2.5, 1700000009000);
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 0, 2.5 * 0.5 + 10); }));
	CHECK(h.server->GetStatistics()["Associations"].asUInt64() == 2);
}

TEST_CASE(SUITE("a bilateral table mismatch keeps the client from going up"))
{
	Harness h(20303, "BT_TEST_WRONG");
	h.server->Enable();
	h.client->Enable();
	//the association is made, then the client drops it when the table does not match: never CONNECTED
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["Associations"].asUInt64() >= 1; }));
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK(h.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	CHECK(h.recorder.EventCount() == 0);
	CHECK_FALSE(h.client->AssociationUp());
}

TEST_CASE(SUITE("an edition mismatch drops the association by default and is only a warning with RequireMatchingEdition false"))
{
	Json::Value mismatch(Json::objectValue);
	mismatch["TASE2Edition"] = "1996-08"; //the server runs 2000-08
	{
		Harness h(20392, "BT_TEST_A", DefaultTransferSet, "2000-08", mismatch);
		h.server->Enable();
		h.client->Enable();
		REQUIRE(WaitFor([&] { return h.server->GetStatistics()["Associations"].asUInt64() >= 1; }));
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		CHECK(h.recorder.Count(odc::ConnectState::CONNECTED) == 0);
		CHECK_FALSE(h.client->AssociationUp());
	}
	mismatch["RequireMatchingEdition"] = false;
	Json::Value serverTolerant(Json::objectValue);
	serverTolerant["RequireMatchingEdition"] = false;
	{
		Harness h(20391, "BT_TEST_A", DefaultTransferSet, "2000-08", mismatch, serverTolerant);
		h.server->Enable();
		h.client->Enable();
		REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		CHECK(h.client->AssociationUp());
	}
}

TEST_CASE(SUITE("operator request and other external event reports"))
{
	Harness h(20304, "BT_TEST_A", R"({"DataSet": "DS_TEST_001", "Conditions": ["OperatorRequest", "OtherExternalEvent"]})");
	h.server->Enable();
	Feed(*h.server, odc::EventType::Binary, 0, 1, 1700000000000);
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	//this transfer set reports on neither ObjectChange nor integrity: nothing arrives until an operator request
	std::this_thread::sleep_for(std::chrono::milliseconds(400));
	CHECK_FALSE(h.recorder.Last(odc::EventType::Binary, 0).has_value());
	h.server->RaiseCondition(iccp::tase2::DsCondition::OperatorRequest, 0);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Binary, 0, 1); }));
	//a change is not reported either
	Feed(*h.server, odc::EventType::Binary, 0, 0, 1700000001000);
	std::this_thread::sleep_for(std::chrono::milliseconds(400));
	CHECK(h.recorder.LastIs(odc::EventType::Binary, 0, 1));
	h.server->RaiseCondition(iccp::tase2::DsCondition::OtherExternalEvent, 5);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Binary, 0, 0); }));
}

TEST_CASE(SUITE("edition 2000-08, and both ports shutting down together"))
{
	//the other tests use 1996-08; 2000-08 is the default
	Harness h(20305, "BT_TEST_A", DefaultTransferSet, "2000-08");
	h.server->Enable();
	Feed(*h.server, odc::EventType::Binary, 0, 1, 1700000000000);
	Feed(*h.server, odc::EventType::Analog, 0, 20.0, 1700000000000);
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Binary, 0, 1) && h.recorder.LastIs(odc::EventType::Analog, 0, 20.0 * 0.5 + 10); }));
	//both ends conclude at once (as when opendatacon shuts down): the release must not be left to its 5 s timeout
	const auto start = std::chrono::steady_clock::now();
	h.server->Disable();
	h.client->Disable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }, 4000));
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
	REQUIRE(WaitFor([&] { return !h.server->AssociationUp(); }));
}

TEST_CASE(SUITE("report by exception at the interval: only what changed is reported"))
{
	Harness h(20306, "BT_TEST_A", R"({"DataSet": "DS_TEST_001", "Conditions": ["IntervalTimeOut"], "Interval": 1, "RBE": true})");
	h.server->Enable();
	Feed(*h.server, odc::EventType::Binary, 0, 1, 1700000000000);
	Feed(*h.server, odc::EventType::Analog, 0, 20.0, 1700000000000);
	Feed(*h.server, odc::EventType::Analog, 1, 5.0, 1700000000000);
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	//nothing has changed since the transfer set started: the interval reports are empty, so nothing is sent
	std::this_thread::sleep_for(std::chrono::milliseconds(2500));
	CHECK_FALSE(h.recorder.Last(odc::EventType::Analog, 0).has_value());
	CHECK_FALSE(h.recorder.Last(odc::EventType::Binary, 0).has_value());
	//one point changes: the next interval report has that point only
	Feed(*h.server, odc::EventType::Analog, 0, 30.0, 1700000002000);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 0, 30.0 * 0.5 + 10); }, 5000));
	CHECK_FALSE(h.recorder.Last(odc::EventType::Analog, 1).has_value());
	CHECK_FALSE(h.recorder.Last(odc::EventType::Binary, 0).has_value());
}

TEST_CASE(SUITE("AllChangesReported: every change inside the buffer time arrives as its own ODC event, in order"))
{
	Harness h(20307, "BT_TEST_A", R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"], "RBE": true, "BufferTime": 1, "AllChangesReported": true})", "2000-08");
	REQUIRE(h.server->ConfigValid());
	REQUIRE(h.client->ConfigValid());
	h.server->Enable();
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(WaitFor([&]
		{
			const Json::Value st = h.client->GetStatus();
			return st.isMember("Provisioning") && st["Provisioning"].size() == 2 && st["Provisioning"][0]["State"].asString() == "transfer-started";
		}));

	//one buffer period (1 s): the points change in turn, one of them three times
	constexpr odc::msSinceEpoch_t T = 1700000000000;
	Feed(*h.server, odc::EventType::Analog, 1, 11, T + 1000);
	Feed(*h.server, odc::EventType::Binary, 0, 1, T + 2000);
	Feed(*h.server, odc::EventType::Analog, 1, 12, T + 3000);
	Feed(*h.server, odc::EventType::Binary, 0, 0, T + 4000);
	Feed(*h.server, odc::EventType::Analog, 1, 13, T + 5000);
	auto points = [&]
			  {
				  std::vector<Recorder::Rec> out;
				  for(const auto& e : h.recorder.Events())
					  if(e.type == odc::EventType::Analog || e.type == odc::EventType::Binary)
						  out.push_back(e);
				  return out;
			  };
	REQUIRE(WaitFor([&] { return points().size() >= 5; }));
	std::this_thread::sleep_for(std::chrono::milliseconds(1500)); //and no more than those
	const auto seen = points();
	REQUIRE(seen.size() == 5);
	const double values[] = {11, 1, 12, 0, 13};
	for(size_t i = 0; i < seen.size(); ++i)
	{
		CAPTURE(i);
		CHECK(seen[i].type == (i % 2 == 0 ? odc::EventType::Analog : odc::EventType::Binary));
		CHECK(seen[i].value == values[i]);
		CHECK(seen[i].timestamp == T + 1000 * static_cast<odc::msSinceEpoch_t>(i + 1)); //each keeps the time stamp of its change
	}
	CHECK(h.server->GetStatistics()["ReportsSent"].asUInt64() >= 3);
}

TEST_CASE(SUITE("Critical: the client acknowledges every report, so the server's outstanding list ends empty and nothing fails"))
{
	//with and without a Transfer_Set_Time_Stamp object in the reports (without, the acknowledgement names the transfer set only)
	const std::string withStamp = R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"], "RBE": true, "Critical": true,
		"SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"]})";
	const std::string withoutStamp = R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"], "RBE": true, "Critical": true})";
	int port = 20350;
	for(const std::string& transferSet : {withStamp, withoutStamp})
	{
		Harness h(port++, "BT_TEST_A", transferSet, "2000-08");
		REQUIRE(h.server->ConfigValid());
		REQUIRE(h.client->ConfigValid());
		h.server->Enable();
		h.client->Enable();
		REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		REQUIRE(WaitFor([&]
			{
				const Json::Value st = h.client->GetStatus();
				return st.isMember("Provisioning") && st["Provisioning"].size() == 2 && st["Provisioning"][0]["State"].asString() == "transfer-started";
			}));
		constexpr odc::msSinceEpoch_t T = 1700000000000;
		for(int i = 1; i <= 5; ++i)
		{
			Feed(*h.server, odc::EventType::Analog, 1, 10 + i, T + 1000 * i);
			REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 1, 10 + i); }));
		}
		REQUIRE(WaitFor([&] { return h.server->GetStatistics()["CriticalReportsAcknowledged"].asUInt64() == 5; }));
		const auto s = h.server->GetStatistics();
		CHECK(s["CriticalReportsSent"].asUInt64() == 5);
		CHECK(s["CriticalReportsOutstanding"].asUInt64() == 0);
		CHECK(s["DataFailures"].asUInt64() == 0);
		CHECK(s["ReportAckTimeouts"].asUInt64() == 0);
		CHECK(s["ReportNacksReceived"].asUInt64() == 0);
		CHECK(h.client->GetStatistics()["ReportAcksSent"].asUInt64() == 5);
		CHECK(h.client->GetStatistics()["ReportNacksSent"].asUInt64() == 0);
		//nothing times out later (the default timeout is 10 s: a short wait only shows the list stays empty)
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		CHECK(h.server->GetStatistics()["DataFailures"].asUInt64() == 0);
		CHECK(h.server->AssociationUp());
	}
}

TEST_CASE(SUITE("Critical: a client that does not acknowledge makes each report a Data_failure after the timeout, and the data still flows"))
{
	Json::Value clientOverrides(Json::objectValue);
	clientOverrides["AcknowledgeCriticalReports"] = false;
	Json::Value serverOverrides(Json::objectValue);
	serverOverrides["ReportAckTimeoutms"] = 300;
	const std::string transferSet = R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"], "RBE": true, "Critical": true,
		"SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"]})";
	Harness h(20352, "BT_TEST_A", transferSet, "2000-08", clientOverrides, serverOverrides);
	REQUIRE(h.server->ConfigValid());
	REQUIRE(h.client->ConfigValid());
	h.server->Enable();
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(WaitFor([&]
		{
			const Json::Value st = h.client->GetStatus();
			return st.isMember("Provisioning") && st["Provisioning"].size() == 2 && st["Provisioning"][0]["State"].asString() == "transfer-started";
		}));
	constexpr odc::msSinceEpoch_t T = 1700000000000;
	Feed(*h.server, odc::EventType::Analog, 1, 11, T + 1000);
	Feed(*h.server, odc::EventType::Analog, 1, 12, T + 2000);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 1, 12); })); //the values arrive all the same
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["DataFailures"].asUInt64() == 2; }));
	const auto s = h.server->GetStatistics();
	CHECK(s["CriticalReportsSent"].asUInt64() == 2);
	CHECK(s["CriticalReportsAcknowledged"].asUInt64() == 0);
	CHECK(s["ReportAckTimeouts"].asUInt64() == 2);
	CHECK(s["CriticalReportsOutstanding"].asUInt64() == 0);
	CHECK(h.client->GetStatistics()["ReportAcksSent"].asUInt64() == 0);
	//the association is untouched and the transfer set keeps reporting
	CHECK(h.server->AssociationUp());
	CHECK(h.client->AssociationUp());
	Feed(*h.server, odc::EventType::Analog, 1, 13, T + 3000);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 1, 13); }));
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["DataFailures"].asUInt64() == 3; }));
	CHECK(h.recorder.Count(odc::ConnectState::DISCONNECTED) == 0);
}

TEST_CASE(SUITE("a transfer set that is not critical is never acknowledged and never times out"))
{
	Json::Value serverOverrides(Json::objectValue);
	serverOverrides["ReportAckTimeoutms"] = 200;
	Harness h(20353, "BT_TEST_A", R"({"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"], "RBE": true})", "2000-08", Json::Value(Json::objectValue), serverOverrides);
	h.server->Enable();
	h.client->Enable();
	REQUIRE(WaitFor([&] { return h.recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(WaitFor([&]
		{
			const Json::Value st = h.client->GetStatus();
			return st.isMember("Provisioning") && st["Provisioning"].size() == 2 && st["Provisioning"][0]["State"].asString() == "transfer-started";
		}));
	Feed(*h.server, odc::EventType::Analog, 1, 11, 1700000001000);
	REQUIRE(WaitFor([&] { return h.recorder.LastIs(odc::EventType::Analog, 1, 11); }));
	std::this_thread::sleep_for(std::chrono::milliseconds(600));
	const auto s = h.server->GetStatistics();
	CHECK(s["CriticalReportsSent"].asUInt64() == 0);
	CHECK(s["DataFailures"].asUInt64() == 0);
	CHECK(h.client->GetStatistics()["ReportAcksSent"].asUInt64() == 0);
}

TEST_CASE(SUITE("invalid configuration: the port does not enable"))
{
	SetupLoggers();
	const std::string file = ConfFileName("invalid", 0);
	WriteFile(file.c_str(), R"({"Port": 0})");
	auto ios = odc::asio_service::Get();
	auto work = ios->make_work();
	std::thread t([ios] { ios->run(); });
	{
		auto server = std::make_shared<ICCPServerPort>("TestServer", file, Json::nullValue);
		server->Build();
		CHECK_FALSE(server->ConfigValid());
		CHECK_FALSE(server->ConfigErrors().empty());
		Recorder recorder;
		server->Subscribe(&recorder, "Recorder");
		server->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		CHECK(recorder.Count(odc::ConnectState::PORT_UP) == 0);
	}
	work.reset();
	ios->run();
	t.join();
	std::remove(file.c_str());
}

TEST_CASE(SUITE("a configuration file with a JSON syntax error keeps the port disabled, whatever the overrides"))
{
	SetupLoggers();
	const std::string file = ConfFileName("syntax", 0);
	//a trailing comma in an array: what commenting out the last element leaves behind
	WriteFile(file.c_str(), R"({"Port": "10102", "TransferSets": [ {"DataSet": "DS_TEST_001"}, ]})");
	auto ios = odc::asio_service::Get();
	auto work = ios->make_work();
	std::thread t([ios] { ios->run(); });
	{
		auto server = std::make_shared<ICCPServerPort>("TestServer", file, Json::Value(Json::objectValue));
		server->Build();
		CHECK_FALSE(server->ConfigValid());
		CHECK_FALSE(server->ConfigErrors().empty());
		Recorder recorder;
		server->Subscribe(&recorder, "Recorder");
		server->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		CHECK(recorder.Count(odc::ConnectState::PORT_UP) == 0);
	}
	work.reset();
	ios->run();
	t.join();
	std::remove(file.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the service roles over real TCP, whichever way the TCP connection goes
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
std::string Join(const std::vector<std::string>& v)
{
	std::string s;
	for(size_t i = 0; i < v.size(); ++i)
		s += (i ? ", " : "") + v[i];
	return s;
}

//what node A or B serves (through Event()): the same shapes, different values
struct NodeValues
{
	bool st;
	double mw0;
	double mw1;
};
NodeValues ValuesOf(const std::string& me)
{
	return me == "A" ? NodeValues{true, 100.5, -7.25} : NodeValues{false, 300.5, 8.5};
}
constexpr odc::msSinceEpoch_t PairTs = 1700000000000; //whole seconds: the time tag types carry no milliseconds

//The configuration of node "A" or "B" (the identity: selectors, domain, names), whatever the TCP direction of its port: the same text is used
//under ICCPClientPort and ICCPServerPort. A node that serves has points with Direction "Serve" and a transfer set pool; one that reads has the
//peer's points with Direction "Read", a data set for them and a transfer set to start.
std::string NodeConf(const std::string& me, const std::string& role, int port, bool serves, bool reads)
{
	const std::string peer = me == "A" ? "B" : "A";
	const bool a = me == "A";
	std::vector<std::string> bin, ana, pool;
	auto point = [](int index, const std::string& name, const std::string& type, const std::string& direction, const std::string& extra = "")
			 {
				 return "{\"Index\": " + std::to_string(index) + ", \"Name\": \"" + name + "\"" + (type.empty() ? "" : ", \"Type\": \"" + type + "\"") +
				        ", \"Direction\": \"" + direction + "\"" + extra + "}";
			 };
	if(serves)
	{
		bin.push_back(point(0, me + "_ST", "StateQTimeTag", "Serve"));
		ana.push_back(point(0, me + "_MW0", "", "Serve"));
		ana.push_back(point(1, me + "_MW1", "RealQTimeTag", "Serve"));
		pool = {"\"TS_" + me + "_1\"", "\"TS_" + me + "_2\""};
	}
	if(reads)
	{
		bin.push_back(point(0, peer + "_ST", "StateQTimeTag", "Read"));
		ana.push_back(point(0, peer + "_MW0", "", "Read", ", \"ScaleFactor\": 0.5, \"Offset\": 10"));
		ana.push_back(point(1, peer + "_MW1", "RealQTimeTag", "Read"));
	}
	std::string text = "{\"ServiceRole\": \"" + role + "\", \"IP\": \"127.0.0.1\", \"Port\": \"" + std::to_string(port) + "\", \"TASE2Edition\": \"1996-08\","
	                   " \"TCPConnectRetryPeriodms\": 200,"
	                   " \"LocalTSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteTSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	                   " \"LocalSSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteSSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	                   " \"LocalPSelector\": \"0000000" + std::string(a ? "1" : "2") + "\", \"RemotePSelector\": \"0000000" + std::string(a ? "2" : "1") + "\","
	                   " \"LocalAPTitle\": \"1.3.9999.1." + std::string(a ? "1" : "2") + "\", \"RemoteAPTitle\": \"1.3.9999.1." + std::string(a ? "2" : "1") + "\","
	                   " \"LocalAEQualifier\": " + std::string(a ? "1" : "2") + ", \"RemoteAEQualifier\": " + std::string(a ? "2" : "1") + ","
	                   " \"BilateralTableID\": \"BT_" + me + "\", \"RemoteBilateralTableID\": \"BT_" + peer + "\","
	                   " \"LocalDomain\": \"DOMAIN_" + me + "\", \"RemoteDomain\": \"DOMAIN_" + peer + "\","
	                   " \"Binaries\": [" + Join(bin) + "], \"Analogs\": [" + Join(ana) + "]";
	if(reads)
		text += ", \"DataSets\": [{\"Name\": \"DS_" + peer + "_001\", \"Members\": [\"" + peer + "_ST\", \"" + peer + "_MW0\", \"" + peer + "_MW1\"]}],"
		        " \"TransferSets\": [{\"DataSet\": \"DS_" + peer + "_001\", \"Conditions\": [\"ObjectChange\", \"IntegrityTimeOut\"], \"IntegrityCheck\": 30}]";
	if(serves)
		text += ", \"TransferSetPool\": [" + Join(pool) + "]";
	return text + "}";
}

struct PairHarness
{
	struct Side
	{
		std::string file;
		std::shared_ptr<ICCPPortBase> port;
		Recorder recorder;
		std::string me;
		std::string role;
		bool serves = false;
		bool reads = false;
	};
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	Side a, b;

	//node A and node B with their service roles; aListens: A's port is an ICCPServerPort (TCP listener), else an ICCPClientPort (initiator)
	PairHarness(int port, const std::string& roleA, const std::string& roleB, bool aListens)
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		a.me = "A";
		b.me = "B";
		a.role = roleA;
		b.role = roleB;
		//a node serves unless it is a Client, and reads the peer's points if it is not a Server and the peer serves them
		a.serves = roleA != "Client";
		b.serves = roleB != "Client";
		a.reads = roleA != "Server" && b.serves;
		b.reads = roleB != "Server" && a.serves;
		for(Side* s : {&a, &b})
		{
			const bool isA = s == &a;
			s->file = ConfFileName(("pair" + s->me).c_str(), port);
			WriteFile(s->file.c_str(), NodeConf(s->me, s->role, port, s->serves, s->reads));
			const bool listens = isA ? aListens : !aListens;
			if(listens)
				s->port = std::make_shared<ICCPServerPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			else
				s->port = std::make_shared<ICCPClientPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			s->port->Build();
			s->port->Subscribe(&s->recorder, "Recorder");
		}
	}
	~PairHarness()
	{
		for(Side* s : {&a, &b})
			if(s->port)
				s->port->Disable();
		WaitFor([&] { return !a.port->AssociationUp() && !b.port->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		a.port.reset();
		b.port.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(a.file.c_str());
		std::remove(b.file.c_str());
	}
};

//what a reader has to see of the peer's served values (the read points are scaled: MW0 * 0.5 + 10)
bool SeesValues(PairHarness::Side& reader, const NodeValues& v)
{
	return reader.recorder.LastIs(odc::EventType::Binary, 0, v.st ? 1 : 0) && reader.recorder.LastIs(odc::EventType::Analog, 0, v.mw0 * 0.5 + 10) &&
	       reader.recorder.LastIs(odc::EventType::Analog, 1, v.mw1);
}

void FeedNode(PairHarness::Side& s, const NodeValues& v, odc::msSinceEpoch_t ts)
{
	Feed(*s.port, odc::EventType::Binary, 0, v.st ? 1 : 0, ts);
	Feed(*s.port, odc::EventType::Analog, 0, v.mw0, ts);
	Feed(*s.port, odc::EventType::Analog, 1, v.mw1, ts, odc::QualityFlags::ONLINE | odc::QualityFlags::LOCAL_FORCED);
}

//Both nodes' ports, the TCP direction and the roles as given: association, bilateral table check, data set and transfer set, reports in
//each direction that the roles allow (both at once for a Peer pair), nothing in the directions they do not, release, and optionally a reconnect.
void RunPair(int port, const std::string& roleA, const std::string& roleB, bool aListens, bool reconnect)
{
	PairHarness h(port, roleA, roleB, aListens);
	REQUIRE(h.a.port->ConfigValid());
	REQUIRE(h.b.port->ConfigValid());
	auto& listener = aListens ? h.a : h.b;
	auto& initiator = aListens ? h.b : h.a;
	CHECK(h.a.port->GetStatus()["ServiceRole"].asString() == roleA);
	CHECK(h.b.port->GetStatus()["ServiceRole"].asString() == roleB);
	CHECK(listener.port->GetStatus()["TCP"].asString() == "Listener");
	CHECK(initiator.port->GetStatus()["TCP"].asString() == "Initiator");

	listener.port->Enable();
	initiator.port->Enable();
	//values the serving side has before (or while) the peer arrives
	for(auto* s : {&h.a, &h.b})
		if(s->serves)
			FeedNode(*s, ValuesOf(s->me), PairTs);
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	REQUIRE(h.a.port->AssociationUp());
	REQUIRE(h.b.port->AssociationUp());

	auto checkFlow = [&](const char* when)
			     {
				     INFO(when);
				     for(auto* reader : {&h.a, &h.b})
				     {
					     auto* peer = reader == &h.a ? &h.b : &h.a;
					     if(reader->reads)
					     {
						     REQUIRE(WaitFor([&] { return SeesValues(*reader, ValuesOf(peer->me)); }));
						     const auto st = reader->recorder.Last(odc::EventType::Binary, 0);
						     CHECK(st->quality == odc::QualityFlags::ONLINE);
						     CHECK(st->timestamp == PairTs);
						     const auto mw1 = reader->recorder.Last(odc::EventType::Analog, 1);
						     CHECK(mw1->quality == (odc::QualityFlags::ONLINE | odc::QualityFlags::LOCAL_FORCED));
						     CHECK(mw1->timestamp == PairTs);
					     }
					     else
						     CHECK(reader->recorder.EventCount() == 0); //a Server publishes nothing: it has no points to read
				     }
			     };
	checkFlow("after the association");

	//a change at each serving side reaches the reader as an object change report (both directions at once for a Peer pair)
	constexpr odc::msSinceEpoch_t Ts2 = 1700000005000;
	for(auto* s : {&h.a, &h.b})
		if(s->serves)
		{
			Feed(*s->port, odc::EventType::Analog, 0, ValuesOf(s->me).mw0 + 50, Ts2);
			Feed(*s->port, odc::EventType::Binary, 0, ValuesOf(s->me).st ? 0 : 1, Ts2);
		}
	for(auto* reader : {&h.a, &h.b})
	{
		auto* peer = reader == &h.a ? &h.b : &h.a;
		if(reader->reads)
		{
			const auto v = ValuesOf(peer->me);
			REQUIRE(WaitFor([&] { return reader->recorder.LastIs(odc::EventType::Analog, 0, (v.mw0 + 50) * 0.5 + 10) && reader->recorder.LastIs(odc::EventType::Binary, 0, v.st ? 0 : 1); }));
			CHECK(reader->recorder.Last(odc::EventType::Binary, 0)->timestamp == Ts2);
		}
	}

	if(reconnect)
	{
		//the listening port drops: both ends see it, the initiator re-associates when the listener is back, and the data flows again
		listener.port->Disable();
		REQUIRE(WaitFor([&] { return initiator.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
		REQUIRE(WaitFor([&] { return listener.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
		listener.port->Enable();
		for(auto* s : {&h.a, &h.b})
			if(s->serves)
				FeedNode(*s, ValuesOf(s->me), PairTs);
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
		checkFlow("after the reconnect");
	}

	//release: the initiator first, then the listener
	initiator.port->Disable();
	REQUIRE(WaitFor([&] { return initiator.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
	REQUIRE(WaitFor([&] { return listener.recorder.Count(odc::ConnectState::DISCONNECTED) == (reconnect ? 2u : 1u); }));
	listener.port->Disable();
	REQUIRE(WaitFor([&] { return listener.recorder.Count(odc::ConnectState::PORT_DOWN) == (reconnect ? 2u : 1u); }));
}
}

TEST_CASE(SUITE("roles: Client (TCP initiator) with Server (TCP listener)"))
{
	RunPair(20311, "Client", "Server", false, false);
}

TEST_CASE(SUITE("roles: Server (TCP initiator) with Client (TCP listener)"))
{
	//A is the client but its port is the TCP listener; B serves from the initiating port
	RunPair(20312, "Client", "Server", true, false);
}

TEST_CASE(SUITE("roles: Peer with Peer, A initiating the TCP connection (both directions at once), with a reconnect"))
{
	RunPair(20313, "Peer", "Peer", false, true);
}

TEST_CASE(SUITE("roles: Peer with Peer, A listening: the same configurations under the other port types"))
{
	RunPair(20314, "Peer", "Peer", true, false);
}

TEST_CASE(SUITE("roles: Peer (TCP initiator) with Server (TCP listener)"))
{
	RunPair(20315, "Peer", "Server", false, false);
}

TEST_CASE(SUITE("roles: Peer (TCP listener) with Server (TCP initiator)"))
{
	RunPair(20316, "Peer", "Server", true, false);
}

TEST_CASE(SUITE("roles: Client (TCP initiator) with Peer (TCP listener)"))
{
	RunPair(20317, "Client", "Peer", false, false);
}

TEST_CASE(SUITE("roles: Client (TCP listener) with Peer (TCP initiator), with a reconnect"))
{
	RunPair(20318, "Client", "Peer", true, true);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// TASE.2 Block 5 (device control) [S] over real TCP: ODC control events in at the operating end, ODC control events out at the serving end
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
//the downstream of a node: it records the control events the node publishes (the peer operated one of its devices) and answers them
class ControlSink: public odc::IOHandler
{
public:
	struct Rec
	{
		odc::EventType type;
		size_t index;
		odc::ControlCode code;
		double value;
	};
	ControlSink():
		odc::IOHandler("ControlSink")
	{}
	void Event(odc::ConnectState state, const std::string&) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		states.push_back(state);
	}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t cb) override
	{
		const auto t = event->GetEventType();
		if(t == odc::EventType::ConnectState)
		{
			(*cb)(odc::CommandStatus::SUCCESS);
			return;
		}
		{
			std::lock_guard<std::mutex> lock(mtx);
			Rec r{t, event->GetIndex(), odc::ControlCode::UNDEFINED, 0};
			switch(t)
			{
				case odc::EventType::ControlRelayOutputBlock: r.code = event->GetPayload<odc::EventType::ControlRelayOutputBlock>().functionCode; break;
				case odc::EventType::AnalogOutputInt16: r.value = event->GetPayload<odc::EventType::AnalogOutputInt16>().first; break;
				case odc::EventType::AnalogOutputInt32: r.value = event->GetPayload<odc::EventType::AnalogOutputInt32>().first; break;
				case odc::EventType::AnalogOutputFloat32: r.value = event->GetPayload<odc::EventType::AnalogOutputFloat32>().first; break;
				case odc::EventType::AnalogOutputDouble64: r.value = event->GetPayload<odc::EventType::AnalogOutputDouble64>().first; break;
				default: break;
			}
			recs.push_back(r);
		}
		(*cb)(answer.load());
	}
	void Enable() override
	{}
	void Disable() override
	{}
	size_t Count(odc::ConnectState state)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return static_cast<size_t>(std::count(states.begin(), states.end(), state));
	}
	size_t Count()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return recs.size();
	}
	std::vector<Rec> All()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return recs;
	}
	std::atomic<odc::CommandStatus> answer{odc::CommandStatus::SUCCESS};

private:
	std::mutex mtx;
	std::vector<Rec> recs;
	std::vector<odc::ConnectState> states;
};

//The devices of node X: DEV_X_CMD (select before operate), DEV_X_DIR (direct operate, ON = 2, OFF = 3), DEV_X_SP (a real setpoint, ODC = TASE.2 * 0.5 + 10)
//and DEV_X_DSP (a discrete setpoint published as AnalogOutputInt32). A node serves its own and operates the peer's: Direction says which, so the same text
//works under either port type.
std::string ControlNodeConf(const std::string& me, const std::string& role, int port, bool serves, bool operates, bool operatorDirectCmd, std::optional<int> serveId = std::nullopt,
	std::optional<int> expectId = std::nullopt)
{
	const std::string peer = me == "A" ? "B" : "A";
	const bool a = me == "A";
	auto devices = [](const std::string& owner, const std::string& direction, bool directCmd, std::optional<int> checkBack)
			   {
				   const std::string d = ", \"Direction\": \"" + direction + "\"";
				   return std::make_pair(
					   "{\"Index\": 0, \"Name\": \"DEV_" + owner + "_CMD\"" + d + (directCmd ? ", \"SelectRequired\": false" : "") + (checkBack ? ", \"CheckBackID\": " + std::to_string(*checkBack) : "") + "}, "
					   "{\"Index\": 1, \"Name\": \"DEV_" + owner + "_DIR\"" + d + ", \"SelectRequired\": false, \"CommandValueOn\": 2, \"CommandValueOff\": 3}",
					   "{\"Index\": 0, \"Name\": \"DEV_" + owner + "_SP\"" + d + ", \"ScaleFactor\": 0.5, \"Offset\": 10}, "
					   "{\"Index\": 1, \"Name\": \"DEV_" + owner + "_DSP\"" + d + ", \"Type\": \"DiscreteSetpoint\"}");
			   };
	std::vector<std::string> bin, ana;
	if(serves)
	{
		const auto d = devices(me, "Serve", false, serveId);
		bin.push_back(d.first);
		ana.push_back(d.second);
	}
	if(operates)
	{
		const auto d = devices(peer, "Operate", operatorDirectCmd, expectId);
		bin.push_back(d.first);
		ana.push_back(d.second);
	}
	return "{\"ServiceRole\": \"" + role + "\", \"IP\": \"127.0.0.1\", \"Port\": \"" + std::to_string(port) + "\", \"TASE2Edition\": \"1996-08\","
	       " \"TCPConnectRetryPeriodms\": 200,"
	       " \"LocalTSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteTSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalSSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteSSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalPSelector\": \"0000000" + std::string(a ? "1" : "2") + "\", \"RemotePSelector\": \"0000000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalAPTitle\": \"1.3.9999.1." + std::string(a ? "1" : "2") + "\", \"RemoteAPTitle\": \"1.3.9999.1." + std::string(a ? "2" : "1") + "\","
	       " \"LocalAEQualifier\": " + std::string(a ? "1" : "2") + ", \"RemoteAEQualifier\": " + std::string(a ? "2" : "1") + ","
	       " \"BilateralTableID\": \"BT_" + me + "\", \"RemoteBilateralTableID\": \"BT_" + peer + "\","
	       " \"LocalDomain\": \"DOMAIN_" + me + "\", \"RemoteDomain\": \"DOMAIN_" + peer + "\","
	       " \"BinaryControls\": [" + Join(bin) + "], \"AnalogControls\": [" + Join(ana) + "]}";
}

struct ControlPair
{
	struct Side
	{
		std::string file;
		std::shared_ptr<ICCPPortBase> port;
		ControlSink sink;
		ControlSink& recorder = sink; //the connection states are counted by the sink too (a second subscriber would turn its answers into UNDEFINED)
		std::string me;
		std::string role;
		bool serves = false;
		bool operates = false;
	};
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	Side a, b;

	ControlPair(int port, const std::string& roleA, const std::string& roleB, bool aListens, bool operatorDirectCmd = false, std::optional<int> serveId = std::nullopt,
		std::optional<int> expectId = std::nullopt)
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		a.me = "A";
		b.me = "B";
		a.role = roleA;
		b.role = roleB;
		a.serves = roleA != "Client";
		b.serves = roleB != "Client";
		a.operates = roleA != "Server" && b.serves;
		b.operates = roleB != "Server" && a.serves;
		for(Side* s : {&a, &b})
		{
			const bool isA = s == &a;
			s->file = ConfFileName(("control" + s->me).c_str(), port);
			WriteFile(s->file.c_str(), ControlNodeConf(s->me, s->role, port, s->serves, s->operates, operatorDirectCmd, serveId, expectId));
			const bool listens = isA ? aListens : !aListens;
			if(listens)
				s->port = std::make_shared<ICCPServerPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			else
				s->port = std::make_shared<ICCPClientPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			s->port->Build();
			s->port->Subscribe(&s->sink, "ControlSink");
		}
	}
	~ControlPair()
	{
		for(Side* s : {&a, &b})
			if(s->port)
				s->port->Disable();
		WaitFor([&] { return !a.port->AssociationUp() && !b.port->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		a.port.reset();
		b.port.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(a.file.c_str());
		std::remove(b.file.c_str());
	}
	Side& Peer(Side& s)
	{
		return &s == &a ? b : a;
	}
};

//an ODC control command in at a port, and what it answers
struct PendingCommand
{
	std::atomic_bool done{false};
	std::atomic<odc::CommandStatus> status{odc::CommandStatus::UNDEFINED};
};
std::shared_ptr<PendingCommand> Issue(ICCPPortBase& port, std::shared_ptr<odc::EventInfo> event)
{
	auto p = std::make_shared<PendingCommand>();
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([p](odc::CommandStatus s)
		{
			p->status = s;
			p->done = true;
		});
	port.Event(event, "TestHarness", cb);
	return p;
}
odc::CommandStatus Wait(const std::shared_ptr<PendingCommand>& p)
{
	REQUIRE(WaitFor([&] { return p->done.load(); }));
	return p->status.load();
}
std::shared_ptr<odc::EventInfo> Crob(size_t index, odc::ControlCode code)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, "TestHarness", odc::QualityFlags::ONLINE);
	odc::ControlRelayOutputBlock crob;
	crob.functionCode = code;
	e->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(crob));
	return e;
}
std::shared_ptr<odc::EventInfo> Setpoint(odc::EventType type, size_t index, double value)
{
	auto e = std::make_shared<odc::EventInfo>(type, index, "TestHarness", odc::QualityFlags::ONLINE);
	switch(type)
	{
		case odc::EventType::AnalogOutputInt16: e->SetPayload<odc::EventType::AnalogOutputInt16>(odc::AO16(static_cast<int16_t>(value), odc::CommandStatus::SUCCESS)); break;
		case odc::EventType::AnalogOutputInt32: e->SetPayload<odc::EventType::AnalogOutputInt32>(odc::AO32(static_cast<int32_t>(value), odc::CommandStatus::SUCCESS)); break;
		case odc::EventType::AnalogOutputFloat32: e->SetPayload<odc::EventType::AnalogOutputFloat32>(odc::AOF(static_cast<float>(value), odc::CommandStatus::SUCCESS)); break;
		default: e->SetPayload<odc::EventType::AnalogOutputDouble64>(odc::AOD(value, odc::CommandStatus::SUCCESS)); break;
	}
	return e;
}

//The operating end `from` drives every kind of command at the devices of the serving end `to`, and checks what comes out at the other end.
void OperateAll(ControlPair::Side& from, ControlPair::Side& to)
{
	using CS = odc::CommandStatus;
	using CC = odc::ControlCode;
	INFO("operating from " << from.me << " to " << to.me);
	auto& sink = to.sink;
	size_t n = sink.Count();
	auto expect = [&](odc::EventType type, size_t index, CC code, double value)
			  {
				  REQUIRE(sink.Count() == n + 1);
				  const auto r = sink.All()[n];
				  ++n;
				  CHECK(r.type == type);
				  CHECK(r.index == index);
				  CHECK(r.code == code);
				  CHECK(r.value == Approx(value));
			  };
	//select before operate
	CHECK(Wait(Issue(*from.port, Crob(0, CC::LATCH_ON))) == CS::SUCCESS);
	expect(odc::EventType::ControlRelayOutputBlock, 0, CC::LATCH_ON, 0);
	CHECK(Wait(Issue(*from.port, Crob(0, CC::PULSE_OFF))) == CS::SUCCESS);
	expect(odc::EventType::ControlRelayOutputBlock, 0, CC::LATCH_OFF, 0); //OFF is published as the configured OFF code
	//direct operate, with the device's own ON / OFF numbers
	CHECK(Wait(Issue(*from.port, Crob(1, CC::TRIP_PULSE_ON))) == CS::SUCCESS);
	expect(odc::EventType::ControlRelayOutputBlock, 1, CC::LATCH_OFF, 0);
	CHECK(Wait(Issue(*from.port, Crob(1, CC::CLOSE_PULSE_ON))) == CS::SUCCESS);
	expect(odc::EventType::ControlRelayOutputBlock, 1, CC::LATCH_ON, 0);
	//setpoints: scaled to the wire and back
	CHECK(Wait(Issue(*from.port, Setpoint(odc::EventType::AnalogOutputDouble64, 0, 25))) == CS::SUCCESS);
	expect(odc::EventType::AnalogOutputDouble64, 0, CC::UNDEFINED, 25);
	CHECK(Wait(Issue(*from.port, Setpoint(odc::EventType::AnalogOutputFloat32, 0, -3.5))) == CS::SUCCESS); //whatever ODC type comes in
	expect(odc::EventType::AnalogOutputDouble64, 0, CC::UNDEFINED, -3.5);
	CHECK(Wait(Issue(*from.port, Setpoint(odc::EventType::AnalogOutputInt32, 1, 100))) == CS::SUCCESS);
	expect(odc::EventType::AnalogOutputInt32, 1, CC::UNDEFINED, 100); //a discrete setpoint is published as an integer
	//a command the mapping does not know: nothing goes to the peer
	CHECK(Wait(Issue(*from.port, Crob(0, CC::NUL))) == CS::NOT_SUPPORTED);
	CHECK(Wait(Issue(*from.port, Crob(99, CC::LATCH_ON))) == CS::NOT_SUPPORTED); //not a device of this port
	CHECK(Wait(Issue(*from.port, Setpoint(odc::EventType::AnalogOutputDouble64, 99, 1))) == CS::NOT_SUPPORTED);
	CHECK(sink.Count() == n);
	//downstream blocks: the status comes back
	sink.answer = CS::BLOCKED;
	CHECK(Wait(Issue(*from.port, Crob(0, CC::LATCH_ON))) == CS::BLOCKED);
	sink.answer = CS::OUT_OF_RANGE;
	CHECK(Wait(Issue(*from.port, Setpoint(odc::EventType::AnalogOutputDouble64, 0, 1))) == CS::OUT_OF_RANGE);
	sink.answer = CS::HARDWARE_ERROR;
	CHECK(Wait(Issue(*from.port, Crob(1, CC::LATCH_ON))) == CS::HARDWARE_ERROR);
	sink.answer = CS::SUCCESS;
	CHECK(sink.Count() == n + 3);
	//and the device is fine afterwards
	CHECK(Wait(Issue(*from.port, Crob(0, CC::LATCH_ON))) == CS::SUCCESS);
}

//what the counters have to say after OperateAll (plus `extra` select + operate pairs sent before it, and the ones after a reconnect): 8 operates succeeded, of which
//6 had a select, and 2 more selects went to commands downstream then refused; 3 failed
void CheckControlStats(ControlPair::Side& from, ControlPair::Side& to, size_t extra)
{
	auto matches = [&]
			   {
				   const auto s = from.port->GetStatistics();
				   const auto r = to.port->GetStatistics();
				   return s["ControlOperatesSent"].asUInt64() == 8 + extra && s["ControlSelectsSent"].asUInt64() == 8 + extra && s["ControlFailures"].asUInt64() == 3 &&
				          r["ControlOperatesReceived"].asUInt64() == 8 + extra && r["ControlSelectsReceived"].asUInt64() == 8 + extra && r["ControlRefused"].asUInt64() == 3;
			   };
	INFO("stats of " << from.me << " " << from.port->GetStatistics().toStyledString() << " and " << to.me << " " << to.port->GetStatistics().toStyledString());
	CHECK(WaitFor(matches));
}

//Both nodes' ports, the TCP direction and the roles as given: every operating end drives its peer's devices; a Peer pair does it in both directions at once.
void RunControlPair(int port, const std::string& roleA, const std::string& roleB, bool aListens, bool reconnect)
{
	ControlPair h(port, roleA, roleB, aListens);
	REQUIRE(h.a.port->ConfigValid());
	REQUIRE(h.b.port->ConfigValid());
	auto& listener = aListens ? h.a : h.b;
	auto& initiator = aListens ? h.b : h.a;
	listener.port->Enable();
	initiator.port->Enable();
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));

	const size_t extra = (h.a.operates && h.b.operates) ? 2 : 0; //the commands of the concurrent round
	if(h.a.operates && h.b.operates)
	{
		//both directions at once: the first commands go out before either is answered
		auto p1 = Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON));
		auto p2 = Issue(*h.b.port, Crob(0, odc::ControlCode::LATCH_OFF));
		auto p3 = Issue(*h.a.port, Setpoint(odc::EventType::AnalogOutputDouble64, 0, 25));
		auto p4 = Issue(*h.b.port, Setpoint(odc::EventType::AnalogOutputDouble64, 0, 45));
		for(auto* p : {&p1, &p2, &p3, &p4})
			CHECK(Wait(*p) == odc::CommandStatus::SUCCESS);
		REQUIRE(WaitFor([&] { return h.b.sink.Count() == 2 && h.a.sink.Count() == 2; }));
		const auto atB = h.b.sink.All();
		const auto atA = h.a.sink.All();
		auto find = [](const std::vector<ControlSink::Rec>& v, odc::EventType t)
				{
					for(const auto& r : v)
						if(r.type == t)
							return r;
					return ControlSink::Rec{odc::EventType::Binary, 0, odc::ControlCode::UNDEFINED, 0}; //none found
				};
		CHECK(find(atB, odc::EventType::ControlRelayOutputBlock).code == odc::ControlCode::LATCH_ON);  //A operated B's device
		CHECK(find(atA, odc::EventType::ControlRelayOutputBlock).code == odc::ControlCode::LATCH_OFF); //B operated A's
		CHECK(find(atB, odc::EventType::AnalogOutputDouble64).value == Approx(25));
		CHECK(find(atA, odc::EventType::AnalogOutputDouble64).value == Approx(45));
	}
	//each operating end, one after the other
	for(auto* from : {&h.a, &h.b})
		if(from->operates)
		{
			auto* to = &h.Peer(*from);
			OperateAll(*from, *to);
			CheckControlStats(*from, *to, extra);
		}

	if(reconnect)
	{
		listener.port->Disable();
		REQUIRE(WaitFor([&] { return initiator.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
		REQUIRE(WaitFor([&] { return listener.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
		//a command while there is no association is answered, not lost
		for(auto* from : {&h.a, &h.b})
			if(from->operates && &h.Peer(*from) == &listener)
				CHECK(Wait(Issue(*from->port, Crob(1, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::UNDEFINED);
		listener.port->Enable();
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
		for(auto* from : {&h.a, &h.b})
			if(from->operates)
			{
				auto& to = h.Peer(*from);
				const size_t n = to.sink.Count();
				CHECK(Wait(Issue(*from->port, Crob(0, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::SUCCESS);
				REQUIRE(to.sink.Count() == n + 1);
				CheckControlStats(*from, to, extra + 1); //the counters add up over the associations
			}
	}
}
}

TEST_CASE(SUITE("block 5: Client (TCP initiator) operates the devices of a Server (TCP listener)"))
{
	RunControlPair(20331, "Client", "Server", false, true);
}

TEST_CASE(SUITE("block 5: Client (TCP listener) operates the devices of a Server (TCP initiator)"))
{
	RunControlPair(20332, "Client", "Server", true, true);
}

TEST_CASE(SUITE("block 5: Server (TCP initiator) with Client (TCP listener), the roles the other way round"))
{
	RunControlPair(20333, "Server", "Client", false, false);
}

TEST_CASE(SUITE("block 5: Peer with Peer, A initiating: both directions at once"))
{
	RunControlPair(20334, "Peer", "Peer", false, true);
}

TEST_CASE(SUITE("block 5: Peer with Peer, A listening"))
{
	RunControlPair(20335, "Peer", "Peer", true, false);
}

TEST_CASE(SUITE("block 5: Peer (TCP initiator) with Server (TCP listener)"))
{
	RunControlPair(20336, "Peer", "Server", false, false);
}

TEST_CASE(SUITE("block 5: Client (TCP listener) with Peer (TCP initiator)"))
{
	RunControlPair(20337, "Client", "Peer", true, false);
}

TEST_CASE(SUITE("block 5: an operate without a select is refused where a select is required, and the statistics count"))
{
	//the operating end is configured for a direct operate of a device that wants a select
	ControlPair h(20338, "Client", "Server", false, true);
	h.b.port->Enable();
	h.a.port->Enable();
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::ALREADY_ACTIVE); //temporarily-unavailable: not selected
	CHECK(h.b.sink.Count() == 0);                                                                             //nothing reached downstream
	//the device that is meant for a direct operate works
	CHECK(Wait(Issue(*h.a.port, Crob(1, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::SUCCESS);
	CHECK(h.b.sink.Count() == 1);
	REQUIRE(WaitFor([&]
		{
			const auto s = h.a.port->GetStatistics();
			const auto r = h.b.port->GetStatistics();
			return s["ControlFailures"].asUInt64() == 1 && s["ControlOperatesSent"].asUInt64() == 1 && r["ControlRefused"].asUInt64() == 1 && r["ControlOperatesReceived"].asUInt64() == 1;
		}));
	const auto s = h.a.port->GetStatistics();
	CHECK(s["ControlSelectsSent"].asUInt64() == 0);
	CHECK(h.b.port->GetStatistics()["ControlSelectsReceived"].asUInt64() == 0);
	//a port with no controls reports none of these
	CHECK_FALSE(h.a.port->GetStatus().isMember("ControlSelectsSent"));
}

TEST_CASE(SUITE("block 5: the CheckBackID the server returns and the operating end expects: match, mismatch, none expected"))
{
	using CS = odc::CommandStatus;
	{
		//the server returns 40000 for its select, the client expects the same
		ControlPair h(20340, "Client", "Server", false, false, 40000, 40000);
		h.b.port->Enable();
		h.a.port->Enable();
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == CS::SUCCESS);
		CHECK(h.b.sink.Count() == 1);
		CHECK(WaitFor([&]
			{
				return h.a.port->GetStatistics()["ControlSelectsSent"].asUInt64() == 1 && h.b.port->GetStatistics()["ControlSelectsReceived"].asUInt64() == 1;
			}));
	}
	{
		//a different one is expected: the operate is not sent
		ControlPair h(20341, "Client", "Server", false, false, 40000, 7);
		h.b.port->Enable();
		h.a.port->Enable();
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == CS::FORMAT_ERROR);
		CHECK(h.b.sink.Count() == 0); //nothing reached downstream
		CHECK(WaitFor([&]
			{
				const auto s = h.a.port->GetStatistics();
				return s["ControlOperatesSent"].asUInt64() == 0 && s["ControlFailures"].asUInt64() == 1 && h.b.port->GetStatistics()["ControlSelectsReceived"].asUInt64() == 1;
			}));
	}
	{
		//none expected: whatever the server returns is taken (here a value above 32767, and the default)
		ControlPair h(20342, "Client", "Server", false, false, 65535, std::nullopt);
		h.b.port->Enable();
		h.a.port->Enable();
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == CS::SUCCESS);
		CHECK(Wait(Issue(*h.a.port, Setpoint(odc::EventType::AnalogOutputDouble64, 0, 25))) == CS::SUCCESS); //the default CheckBackID, no expectation
		CHECK(h.b.sink.Count() == 2);
	}
}

TEST_CASE(SUITE("block 5: a command that arrives while the port is down is answered"))
{
	ControlPair h(20339, "Client", "Server", false);
	//not enabled: the port does not take events at all
	CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::UNDEFINED);
	h.a.port->Enable(); //enabled but the server is not there: no association
	CHECK(Wait(Issue(*h.a.port, Crob(0, odc::ControlCode::LATCH_ON))) == odc::CommandStatus::UNDEFINED);
}
