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
 * MaceLoopbackTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE authentication: ICCPClientPort and ICCPServerPort authenticating to each other in the ACSE association over real TCP on
 *  127.0.0.1, with certificates made at run time, with and without TLS underneath, and the refusals an operator would see.
 *  Only with ODC_ASIO_SSL. Synthetic names only; TCP ports 20600 to 20699.
 */

#ifdef ODC_ASIO_SSL

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include "MaceTestPki.h"
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <opendatacon/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP MACE loopback - " name

namespace
{
//keeps what the ICCP port logs, so that the tests can check the messages an operator would see
class CaptureSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
	size_t Count(const std::string& part, spdlog::level::level_enum level = spdlog::level::trace) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(const auto& l : lines)
			if((level == spdlog::level::trace || l.level == level) && l.text.find(part) != std::string::npos)
				++n;
		return n;
	}
	std::string First(const std::string& part) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& l : lines)
			if(l.text.find(part) != std::string::npos)
				return l.text;
		return "";
	}
	void Clear()
	{
		std::lock_guard<std::mutex> lock(mtx);
		lines.clear();
	}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		lines.push_back({msg.level, std::string(msg.payload.data(), msg.payload.size())});
	}
	void flush_() override
	{}

private:
	struct Line
	{
		spdlog::level::level_enum level;
		std::string text;
	};
	mutable std::mutex mtx;
	std::vector<Line> lines;
};

std::shared_ptr<CaptureSink> CapturePtr()
{
	static std::shared_ptr<CaptureSink> sink = std::make_shared<CaptureSink>();
	return sink;
}

CaptureSink& Captured()
{
	return *CapturePtr();
}

void SetupLoggers()
{
	static bool done = false;
	if(done)
		return;
	done = true;
	auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console->set_level(spdlog::level::warn);
	for(const char* name : {"ICCPPort", "opendatacon"})
	{
		auto logger = odc::spdlog_get(name);
		if(!logger)
		{
			logger = std::make_shared<spdlog::logger>(name, console);
			odc::spdlog_register_logger(logger);
		}
		if(std::string(name) == "ICCPPort")
			logger->sinks().push_back(CapturePtr());
		logger->set_level(spdlog::level::info);
	}
}

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

class Recorder: public odc::IOHandler
{
public:
	Recorder(): odc::IOHandler("Recorder")
	{}
	void Event(odc::ConnectState state, const std::string&) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		states.push_back(state);
	}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t cb) override
	{
		if(event->GetEventType() != odc::EventType::ConnectState)
		{
			std::lock_guard<std::mutex> lock(mtx);
			Rec r{event->GetEventType(), event->GetIndex(), 0};
			switch(r.type)
			{
				case odc::EventType::Binary: r.value = event->GetPayload<odc::EventType::Binary>() ? 1 : 0; break;
				case odc::EventType::Analog: r.value = event->GetPayload<odc::EventType::Analog>(); break;
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
	bool LastIs(odc::EventType type, size_t index, double value)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(auto it = events.rbegin(); it != events.rend(); ++it)
			if(it->type == type && it->index == index)
				return it->value == value;
		return false;
	}

private:
	struct Rec
	{
		odc::EventType type;
		size_t index;
		double value;
	};
	std::mutex mtx;
	std::vector<odc::ConnectState> states;
	std::vector<Rec> events;
};

void Feed(ICCPPortBase& port, odc::EventType type, size_t index, double value, odc::msSinceEpoch_t ts)
{
	auto event = std::make_shared<odc::EventInfo>(type, index, "TestHarness", odc::QualityFlags::ONLINE, ts);
	switch(type)
	{
		case odc::EventType::Binary: event->SetPayload<odc::EventType::Binary>(value != 0); break;
		case odc::EventType::Analog: event->SetPayload<odc::EventType::Analog>(double(value)); break;
		default: break;
	}
	std::atomic_bool done{false};
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([&done](odc::CommandStatus) { done = true; });
	port.Event(event, "TestHarness", cb);
	REQUIRE(WaitFor([&] { return done.load(); }));
}

//node "A" or "B": it serves points ST (binary) and MW (analog) and, if it reads, reads the peer's
std::string NodeText(const std::string& me, const std::string& role, int port, bool serves, bool reads, const Json::Value& tls, const Json::Value& mace)
{
	const std::string peer = me == "A" ? "B" : "A";
	const bool a = me == "A";
	Json::Value j;
	j["ServiceRole"] = role;
	j["IP"] = "127.0.0.1";
	j["Port"] = std::to_string(port);
	j["TASE2Edition"] = "1996-08";
	j["TCPConnectRetryPeriodms"] = 200;
	j["LocalTSelector"] = a ? "0001" : "0002";
	j["RemoteTSelector"] = a ? "0002" : "0001";
	j["LocalSSelector"] = a ? "0001" : "0002";
	j["RemoteSSelector"] = a ? "0002" : "0001";
	j["LocalPSelector"] = a ? "00000001" : "00000002";
	j["RemotePSelector"] = a ? "00000002" : "00000001";
	j["LocalAPTitle"] = a ? "1.3.9999.1.1" : "1.3.9999.1.2";
	j["RemoteAPTitle"] = a ? "1.3.9999.1.2" : "1.3.9999.1.1";
	j["LocalAEQualifier"] = a ? 1 : 2;
	j["RemoteAEQualifier"] = a ? 2 : 1;
	j["BilateralTableID"] = "BT_" + me;
	j["RemoteBilateralTableID"] = "BT_" + peer;
	j["LocalDomain"] = "DOMAIN_" + me;
	j["RemoteDomain"] = "DOMAIN_" + peer;
	j["Binaries"] = Json::Value(Json::arrayValue);
	j["Analogs"] = Json::Value(Json::arrayValue);
	auto point = [](const std::string& name, const std::string& direction, Json::ArrayIndex index)
			 {
				 Json::Value p;
				 p["Index"] = index;
				 p["Name"] = name;
				 p["Direction"] = direction;
				 return p;
			 };
	if(serves)
	{
		j["Binaries"].append(point(me + "_ST", "Serve", 0));
		j["Analogs"].append(point(me + "_MW", "Serve", 0));
		j["TransferSetPool"].append("TS_" + me + "_1");
		j["TransferSetPool"].append("TS_" + me + "_2");
	}
	if(reads)
	{
		j["Binaries"].append(point(peer + "_ST", "Read", 0));
		j["Analogs"].append(point(peer + "_MW", "Read", 0));
		Json::Value ds;
		ds["Name"] = "DS_" + peer + "_001";
		ds["Members"].append(peer + "_ST");
		ds["Members"].append(peer + "_MW");
		j["DataSets"].append(ds);
		Json::Value ts;
		ts["DataSet"] = "DS_" + peer + "_001";
		ts["Conditions"].append("ObjectChange");
		ts["Conditions"].append("IntegrityTimeOut");
		ts["IntegrityCheck"] = 30;
		j["TransferSets"].append(ts);
	}
	if(!tls.isNull())
		j["TLS"] = tls;
	if(!mace.isNull())
		j["MACE"] = mace;
	Json::StreamWriterBuilder wb;
	return Json::writeString(wb, j);
}

//the MACE section of a node: its certificate, and the CA it trusts
Json::Value MaceOf(const macetest::Files& id, const std::string& ca)
{
	Json::Value m;
	m["Enabled"] = true;
	m["MechanismName"] = macetest::Mechanism;
	m["CertFile"] = id.cert;
	m["KeyFile"] = id.key;
	m["CAFile"] = ca;
	return m;
}

Json::Value TlsOf(const macetest::Files& id, const std::string& ca)
{
	Json::Value t;
	t["Enabled"] = true;
	t["CertFile"] = id.cert;
	t["KeyFile"] = id.key;
	t["CAFile"] = ca;
	return t;
}

//Two nodes' ports with the TCP direction and roles as given
struct Pair
{
	struct Side
	{
		std::string file;
		std::shared_ptr<ICCPPortBase> port;
		Recorder recorder;
		std::string me;
		bool serves = false;
		bool reads = false;
	};
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	Side a, b;
	Side& listener;
	Side& initiator;

	Pair(int port, const std::string& roleA, const std::string& roleB, bool aListens, const Json::Value& tlsA, const Json::Value& tlsB, const Json::Value& maceA,
		const Json::Value& maceB):
		listener(aListens ? a : b),
		initiator(aListens ? b : a)
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		a.me = "A";
		b.me = "B";
		a.serves = roleA != "Client";
		b.serves = roleB != "Client";
		a.reads = roleA != "Server" && b.serves;
		b.reads = roleB != "Server" && a.serves;
		for(Side* s : {&a, &b})
		{
			const bool isA = s == &a;
			s->file = "iccp_mace_loopback_" + s->me + "_" + std::to_string(port) + ".conf";
			{
				std::ofstream ofs(s->file);
				REQUIRE(ofs.good());
				ofs << NodeText(s->me, isA ? roleA : roleB, port, s->serves, s->reads, isA ? tlsA : tlsB, isA ? maceA : maceB);
			}
			const bool listens = isA ? aListens : !aListens;
			if(listens)
				s->port = std::make_shared<ICCPServerPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			else
				s->port = std::make_shared<ICCPClientPort>("Node" + s->me, s->file, Json::Value(Json::objectValue));
			s->port->Build();
			s->port->Subscribe(&s->recorder, "Recorder");
		}
	}
	~Pair()
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

struct Values
{
	bool st;
	double mw;
};
Values ValuesOf(const std::string& me)
{
	return me == "A" ? Values{true, 100.5} : Values{false, 300.25};
}
constexpr odc::msSinceEpoch_t Ts1 = 1700000000000;

bool Sees(Pair::Side& reader, const Values& v)
{
	return reader.recorder.LastIs(odc::EventType::Binary, 0, v.st ? 1 : 0) && reader.recorder.LastIs(odc::EventType::Analog, 0, v.mw);
}

void CheckMace(Pair& h, const std::string& expectedState = "Verified")
{
	for(auto* s : {&h.a, &h.b})
	{
		const Json::Value st = s->port->GetStatus()["MACE"];
		INFO("node " << s->me);
		CHECK(st["Enabled"].asBool());
		CHECK(st["State"].asString() == expectedState);
		CHECK(st["PeerSubject"].asString() == std::string("CN=mace-") + (s == &h.a ? "b" : "a") + ".test");
		CHECK_FALSE(st["PeerNotAfter"].asString().empty());
		CHECK_FALSE(st["SigningTime"].asString().empty());
		CHECK_FALSE(st["PeerSigningTime"].asString().empty());
		CHECK(st["LastError"].asString().empty());
	}
}

//Both nodes up, authenticated in the association, the data flowing in each direction the roles allow, a change reported, optionally a reconnect
void RunPair(int port, const std::string& roleA, const std::string& roleB, bool aListens, bool reconnect, const Json::Value& tlsA, const Json::Value& tlsB)
{
	auto& pki = macetest::GetPki();
	Captured().Clear();
	Pair h(port, roleA, roleB, aListens, tlsA, tlsB, MaceOf(pki.a, pki.caFile), MaceOf(pki.b, pki.caFile));
	REQUIRE(h.a.port->ConfigValid());
	REQUIRE(h.b.port->ConfigValid());
	h.listener.port->Enable();
	h.initiator.port->Enable();
	for(auto* s : {&h.a, &h.b})
		if(s->serves)
		{
			Feed(*s->port, odc::EventType::Binary, 0, ValuesOf(s->me).st ? 1 : 0, Ts1);
			Feed(*s->port, odc::EventType::Analog, 0, ValuesOf(s->me).mw, Ts1);
		}
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	CheckMace(h);
	CHECK(Captured().Count("MACE: authenticating with certificate", spdlog::level::info) == 2);
	CHECK(Captured().Count("MACE: peer authenticated:", spdlog::level::info) == 2);

	auto checkFlow = [&](const char* when)
			     {
				     INFO(when);
				     for(auto* reader : {&h.a, &h.b})
				     {
					     auto* peer = reader == &h.a ? &h.b : &h.a;
					     if(reader->reads)
						     REQUIRE(WaitFor([&] { return Sees(*reader, ValuesOf(peer->me)); }));
				     }
			     };
	checkFlow("after the association");
	for(auto* s : {&h.a, &h.b})
		if(s->serves)
			Feed(*s->port, odc::EventType::Analog, 0, ValuesOf(s->me).mw + 1000, Ts1 + 5000);
	for(auto* reader : {&h.a, &h.b})
	{
		auto* peer = reader == &h.a ? &h.b : &h.a;
		if(reader->reads)
			REQUIRE(WaitFor([&] { return reader->recorder.LastIs(odc::EventType::Analog, 0, ValuesOf(peer->me).mw + 1000); }));
	}

	if(reconnect)
	{
		const std::string firstTime = h.initiator.port->GetStatus()["MACE"]["SigningTime"].asString();
		h.listener.port->Disable();
		REQUIRE(WaitFor([&] { return h.initiator.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
		REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
		std::this_thread::sleep_for(std::chrono::milliseconds(1100)); //a later signing time
		h.listener.port->Enable();
		for(auto* s : {&h.a, &h.b})
			if(s->serves)
			{
				Feed(*s->port, odc::EventType::Binary, 0, ValuesOf(s->me).st ? 1 : 0, Ts1);
				Feed(*s->port, odc::EventType::Analog, 0, ValuesOf(s->me).mw, Ts1);
			}
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
		checkFlow("after the reconnect");
		CheckMace(h);
		//a fresh signature for the new association
		CHECK(h.initiator.port->GetStatus()["MACE"]["SigningTime"].asString() > firstTime);
	}

	h.initiator.port->Disable();
	REQUIRE(WaitFor([&] { return h.initiator.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
	REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::DISCONNECTED) == (reconnect ? 2u : 1u); }));
	h.listener.port->Disable();
	REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::PORT_DOWN) == (reconnect ? 2u : 1u); }));
}
}

TEST_CASE(SUITE("Client (TCP initiator) with Server (TCP listener), plain TCP"))
{
	RunPair(20601, "Client", "Server", false, false, Json::Value(), Json::Value());
}

TEST_CASE(SUITE("Server on the initiating side with a Client on the listener"))
{
	RunPair(20602, "Server", "Client", false, false, Json::Value(), Json::Value());
}

TEST_CASE(SUITE("Peer with Peer, with a reconnect and a fresh signature"))
{
	RunPair(20603, "Peer", "Peer", true, true, Json::Value(), Json::Value());
}

TEST_CASE(SUITE("MACE inside TLS: both on at once"))
{
	auto& pki = macetest::GetPki();
	RunPair(20604, "Peer", "Peer", false, false, TlsOf(pki.a, pki.caFile), TlsOf(pki.b, pki.caFile));
	//TLS on one side only is not a MACE matter: it fails as it always did, so it is not tried here
}

TEST_CASE(SUITE("A certificate of the wrong CA: refused, with the diagnostic in the initiator's log"))
{
	auto& pki = macetest::GetPki();
	Captured().Clear();
	//A initiates with a certificate of the other CA; B (listener) trusts only the test CA
	Pair h(20605, "Client", "Server", false, Json::Value(), Json::Value(), MaceOf(pki.untrustedF, pki.caFile), MaceOf(pki.b, pki.caFile));
	REQUIRE(h.a.port->ConfigValid());
	h.b.port->Enable();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeB: refusing the association: MACE:", spdlog::level::warn) >= 1; }, 5000));
	const std::string listenerText = Captured().First("NodeB: refusing the association: MACE:");
	INFO(listenerText);
	CHECK(listenerText.find("mace-untrusted.test") != std::string::npos);
	CHECK(listenerText.find("is not acceptable") != std::string::npos);
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: association ended: refused", spdlog::level::warn) >= 1; }, 5000));
	const std::string initiatorText = Captured().First("NodeA: association ended: refused");
	INFO(initiatorText);
	CHECK(initiatorText.find("diagnostic from the ACSE service user: authentication-failure (13)") != std::string::npos);
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK_FALSE(h.b.port->AssociationUp());
	CHECK(h.a.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	CHECK(h.b.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	const Json::Value st = h.b.port->GetStatus()["MACE"];
	CHECK(st["State"].asString() == "Failed");
	CHECK(st["LastError"].asString().find("is not acceptable") != std::string::npos);
	CHECK(h.a.port->GetStatus()["MACE"]["State"].asString() == "Sent");
}

TEST_CASE(SUITE("An initiator without MACE is refused by a listener that requires it"))
{
	auto& pki = macetest::GetPki();
	Captured().Clear();
	Pair h(20606, "Client", "Server", false, Json::Value(), Json::Value(), Json::Value(), MaceOf(pki.b, pki.caFile));
	h.b.port->Enable();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: association ended: refused", spdlog::level::warn) >= 1; }, 5000));
	CHECK(Captured().First("NodeA: association ended: refused").find("authentication-required (14)") != std::string::npos);
	CHECK(Captured().Count("NodeB: refusing the association: MACE: the association request carries no authentication", spdlog::level::warn) >= 1);
	CHECK(h.a.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	CHECK_FALSE(h.a.port->GetStatus()["MACE"]["Enabled"].asBool());
}

TEST_CASE(SUITE("An initiator with MACE aborts when the listener does not authenticate"))
{
	auto& pki = macetest::GetPki();
	Captured().Clear();
	Pair h(20607, "Client", "Server", false, Json::Value(), Json::Value(), MaceOf(pki.a, pki.caFile), Json::Value());
	h.b.port->Enable();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: MACE: the association response carries no authentication", spdlog::level::err) >= 1; }, 5000));
	CHECK(Captured().Count("NodeB: the association request carries authentication fields, which are ignored because MACE is not enabled", spdlog::level::warn) >= 1);
	CHECK(h.a.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	CHECK(h.a.port->GetStatus()["MACE"]["State"].asString() == "Failed");
}

TEST_CASE(SUITE("The legacy mechanism-name on one side and the default on the other: associated, with the warnings"))
{
	auto& pki = macetest::GetPki();
	Captured().Clear();
	Json::Value legacy = MaceOf(pki.a, pki.caFile);
	legacy["MechanismName"] = "1.0.840.0.1.0.1.1";
	Json::Value standard = MaceOf(pki.b, pki.caFile);
	standard.removeMember("MechanismName"); //the default: 1.0.62351.4.3.1
	Pair h(20608, "Client", "Server", false, Json::Value(), Json::Value(), legacy, standard);
	REQUIRE(h.a.port->ConfigValid());
	REQUIRE(h.b.port->ConfigValid());
	h.b.port->Enable();
	h.a.port->Enable();
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	CheckMace(h);
	//the configuration warning is on the node that has the legacy value
	CHECK(Captured().Count("\"MACE\": the configured \"MechanismName\" 1.0.840.0.1.0.1.1 is not the mechanism-name IEC 62351-4 defines (1.0.62351.4.3.1)", spdlog::level::warn) == 1);
	//one warning per association on each node: the listener got the legacy value, the initiator gets it back (the response echoes it)
	CHECK(Captured().Count("MACE: the peer's mechanism-name 1.0.840.0.1.0.1.1 is not the one IEC 62351-4 defines (1.0.62351.4.3.1)", spdlog::level::warn) == 2);
	CHECK(Captured().Count("it also differs from ours 1.0.62351.4.3.1", spdlog::level::warn) == 1);
}

#endif // ODC_ASIO_SSL
