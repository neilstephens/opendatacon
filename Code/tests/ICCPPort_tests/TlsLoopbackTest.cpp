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
 * TlsLoopbackTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Secure ICCP: ICCPClientPort and ICCPServerPort talking TLS to each other over real TCP on 127.0.0.1, with certificates made at run
 *  time, and the failures a real network brings: the wrong CA, a plain peer, a silent peer. Only with ODC_ASIO_SSL.
 *  Synthetic names only; TCP ports 20400 to 20499.
 */

#ifdef ODC_ASIO_SSL

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include "TlsTestPki.h"
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

#define SUITE(name) "ICCP TLS loopback - " name

namespace
{
//keeps what the ICCP port logs, so that the tests can check the messages an operator would see
class CaptureSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
	struct Line
	{
		spdlog::level::level_enum level;
		std::string text;
	};
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
	std::string All() const
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::string s;
		for(const auto& l : lines)
			s += l.text + "\n";
		return s;
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
	mutable std::mutex mtx; //(base_sink has its own, serialising sink_it_; this one protects the readers)
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

//the throw-away PKI (made once)
struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa, serverA, serverB, clientA, clientB, serverRenewed;
	std::string caFile, otherCaFile;
	struct Files
	{
		std::string cert, key;
	} a, b, other, renewed; //node A's certificate (CN iccp-node-a.test), node B's, a node certificate of the other CA, A's replacement

	Files Save(const std::string& name, const tlstest::Cert& c)
	{
		return {dir.Write(name + ".crt", c.certPem), dir.Write(name + ".key", c.keyPem)};
	}
	Pki()
	{
		ca = tlstest::MakeCert({"Loopback Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Loopback Test CA", {}, -3600, 86400LL * 3650, true, 1});
		serverA = tlstest::MakeCert({"iccp-node-a.test", {"DNS:iccp-node-a.test", "IP:127.0.0.1"}, -3600, 86400LL * 365, false, 10}, &ca);
		serverB = tlstest::MakeCert({"iccp-node-b.test", {"DNS:iccp-node-b.test", "IP:127.0.0.1"}, -3600, 86400LL * 365, false, 11}, &ca);
		clientA = tlstest::MakeCert({"iccp-node-x.test", {}, -3600, 86400LL * 365, false, 12}, &otherCa);
		serverRenewed = tlstest::MakeCert({"iccp-node-a-renewed.test", {"IP:127.0.0.1"}, -3600, 86400LL * 365, false, 13}, &ca);
		a = Save("a", serverA);
		b = Save("b", serverB);
		other = Save("other", clientA);
		renewed = Save("renewed", serverRenewed);
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
	}
};
Pki& GetPki()
{
	static Pki pki;
	return pki;
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

Json::Value ParseJson(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b, in, &v, &errs));
	return v;
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

//node "A" or "B": it serves points ST (binary) and MW (analog) and, if it reads, reads the peer's. The text is the same under either port type.
std::string NodeText(const std::string& me, const std::string& role, int port, bool serves, bool reads, const Json::Value& tls)
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
	Json::StreamWriterBuilder wb;
	return Json::writeString(wb, j);
}

//the TLS section of a node: its certificate, and the CA it trusts
Json::Value TlsOf(const Pki::Files& id, const std::string& ca, const std::string& extra = "")
{
	Json::Value t = extra.empty() ? Json::Value(Json::objectValue) : ParseJson(extra);
	t["Enabled"] = true;
	t["CertFile"] = id.cert;
	t["KeyFile"] = id.key;
	t["CAFile"] = ca;
	return t;
}

//Two nodes' ports with the TCP direction, roles and TLS sections as given (a null TLS = plain TCP on that side)
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

	Pair(int port, const std::string& roleA, const std::string& roleB, bool aListens, const Json::Value& tlsA, const Json::Value& tlsB):
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
			s->file = "iccp_tls_loopback_" + s->me + "_" + std::to_string(port) + ".conf";
			{
				std::ofstream ofs(s->file);
				REQUIRE(ofs.good());
				ofs << NodeText(s->me, isA ? roleA : roleB, port, s->serves, s->reads, isA ? tlsA : tlsB);
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

//what node "me" serves
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

//Both nodes up over TLS, the data flowing in each direction the roles allow, a change reported, the TLS status, optionally a reconnect, release.
void RunTlsPair(int port, const std::string& roleA, const std::string& roleB, bool aListens, bool reconnect, const Json::Value& tlsA, const Json::Value& tlsB,
	const char* protocol)
{
	Pair h(port, roleA, roleB, aListens, tlsA, tlsB);
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
	REQUIRE(h.a.port->AssociationUp());
	REQUIRE(h.b.port->AssociationUp());

	//the TLS status of each end
	for(auto* s : {&h.a, &h.b})
	{
		const Json::Value st = s->port->GetStatus()["TLS"];
		CHECK(st["Enabled"].asBool());
		CHECK(st["State"].asString() == "Established");
		CHECK(st["Protocol"].asString() == protocol);
		CHECK_FALSE(st["Cipher"].asString().empty());
		CHECK(st["PeerSubject"].asString() == std::string("CN=iccp-node-") + (s == &h.a ? "b" : "a") + ".test");
		CHECK_FALSE(st["PeerNotAfter"].asString().empty());
	}
	CHECK(h.listener.port->GetStatus()["TLS"]["Role"].asString() == "Server");
	CHECK(h.initiator.port->GetStatus()["TLS"]["Role"].asString() == "Client");

	auto checkFlow = [&](const char* when)
			     {
				     INFO(when);
				     for(auto* reader : {&h.a, &h.b})
				     {
					     auto* peer = reader == &h.a ? &h.b : &h.a;
					     if(reader->reads)
						     REQUIRE(WaitFor([&] { return Sees(*reader, ValuesOf(peer->me)); }));
					     else
						     CHECK(reader->recorder.EventCount() == 0);
				     }
			     };
	checkFlow("after the association");

	//a change at each serving side arrives as a report
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
		h.listener.port->Disable();
		REQUIRE(WaitFor([&] { return h.initiator.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
		REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
		CHECK(h.listener.port->GetStatus()["TLS"]["State"].asString() != "Established");
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
		CHECK(h.initiator.port->GetStatus()["TLS"]["State"].asString() == "Established");
	}

	//release: the initiator first, then the listener
	h.initiator.port->Disable();
	REQUIRE(WaitFor([&] { return h.initiator.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
	REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::DISCONNECTED) == (reconnect ? 2u : 1u); }));
	h.listener.port->Disable();
	REQUIRE(WaitFor([&] { return h.listener.recorder.Count(odc::ConnectState::PORT_DOWN) == (reconnect ? 2u : 1u); }));
}
}

TEST_CASE(SUITE("Client (TCP initiator) with Server (TCP listener), TLS 1.3, mutual authentication"))
{
	auto& pki = GetPki();
	RunTlsPair(20401, "Client", "Server", false, false, TlsOf(pki.a, pki.caFile), TlsOf(pki.b, pki.caFile), "TLSv1.3");
}

TEST_CASE(SUITE("Server (TCP initiator) with Client (TCP listener), TLS 1.2 only"))
{
	auto& pki = GetPki();
	const std::string v12 = R"({"MinVersion": "1.2", "MaxVersion": "1.2"})";
	//A is a Server on the initiating side: the TLS role follows the TCP direction, not the service role
	RunTlsPair(20402, "Server", "Client", false, false, TlsOf(pki.a, pki.caFile, v12), TlsOf(pki.b, pki.caFile, v12), "TLSv1.2");
}

TEST_CASE(SUITE("Peer with Peer, both directions at once, with a reconnect (TLS 1.2)"))
{
	auto& pki = GetPki();
	const std::string v12 = R"({"MinVersion": "1.2", "MaxVersion": "1.2"})";
	RunTlsPair(20403, "Peer", "Peer", false, true, TlsOf(pki.a, pki.caFile, v12), TlsOf(pki.b, pki.caFile, v12), "TLSv1.2");
}

TEST_CASE(SUITE("Peer with Peer, A listening, with a reconnect (TLS 1.3)"))
{
	auto& pki = GetPki();
	RunTlsPair(20404, "Peer", "Peer", true, true, TlsOf(pki.a, pki.caFile), TlsOf(pki.b, pki.caFile), "TLSv1.3");
}

TEST_CASE(SUITE("the association stays up through renegotiations and key updates"))
{
	auto& pki = GetPki();
	for(const char* version : {"1.2", "1.3"})
	{
		INFO("TLS " << version);
		const std::string extra = std::string(R"({"RenegotiateIntervalSeconds": 1, "MinVersion": ")") + version + R"(", "MaxVersion": ")" + version + R"("})";
		Pair h(std::string(version) == "1.2" ? 20405 : 20406, "Peer", "Peer", true, TlsOf(pki.a, pki.caFile, extra), TlsOf(pki.b, pki.caFile, extra));
		REQUIRE(h.a.port->ConfigValid());
		REQUIRE(h.b.port->ConfigValid());
		h.a.port->Enable();
		h.b.port->Enable();
		for(auto* s : {&h.a, &h.b})
		{
			Feed(*s->port, odc::EventType::Binary, 0, 1, Ts1);
			Feed(*s->port, odc::EventType::Analog, 0, 1, Ts1);
		}
		for(auto* s : {&h.a, &h.b})
			REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		//four seconds of reports across at least three renegotiations
		for(int i = 2; i <= 9; ++i)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			for(auto* s : {&h.a, &h.b})
				Feed(*s->port, odc::EventType::Analog, 0, i, Ts1 + i * 1000);
			for(auto* s : {&h.a, &h.b})
				REQUIRE(WaitFor([&] { return s->recorder.LastIs(odc::EventType::Analog, 0, i); }));
		}
		for(auto* s : {&h.a, &h.b})
		{
			CHECK(s->port->AssociationUp());
			CHECK(s->recorder.Count(odc::ConnectState::DISCONNECTED) == 0);
			CHECK(s->port->GetStatistics()["Associations"].asUInt64() == 1);
		}
	}
}

TEST_CASE(SUITE("the initiator does not trust the listener's CA: no association, a clear error, a sane retry"))
{
	auto& pki = GetPki();
	Captured().Clear();
	Pair h(20407, "Client", "Server", false, TlsOf(pki.a, pki.otherCaFile), TlsOf(pki.b, pki.caFile));
	REQUIRE(h.a.port->ConfigValid());
	h.b.port->Enable(); //B listens
	h.a.port->Enable(); //A initiates
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: TLS:", spdlog::level::err) >= 1; }, 5000));
	const std::string text = Captured().First("NodeA: TLS:");
	INFO(text);
	CHECK(text.find("TLS handshake failed") != std::string::npos);
	//the listener sends its CA certificate with its own (it is in its store), so the chain is "self-signed certificate in certificate chain"
	CHECK(text.find("certificate verification failed") != std::string::npos);
	CHECK(text.find("Loopback Test CA") != std::string::npos);
	//the listener sees the client's alert, and says so
	REQUIRE(WaitFor([&] { return Captured().Count("NodeB: TLS:", spdlog::level::err) >= 1; }, 5000));
	//retries are spaced (about 1 s, then 2 s, ...): a few in 3.5 s, not a flood
	std::this_thread::sleep_for(std::chrono::milliseconds(3500));
	const size_t attempts = Captured().Count("NodeA: TLS:", spdlog::level::err);
	CHECK(attempts >= 2);
	CHECK(attempts <= 5);
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK_FALSE(h.b.port->AssociationUp());
	CHECK(h.a.recorder.Count(odc::ConnectState::CONNECTED) == 0);
	CHECK(h.b.port->GetStatistics()["Associations"].asUInt64() == 0);
	CHECK(h.a.port->GetStatus()["TLS"]["State"].asString() != "Established");
}

TEST_CASE(SUITE("the listener does not trust the initiator's certificate (client certificate refused)"))
{
	auto& pki = GetPki();
	Captured().Clear();
	//A initiates with a certificate of the other CA; B (listener) trusts only the test CA
	Pair h(20408, "Client", "Server", false, TlsOf(pki.other, pki.caFile), TlsOf(pki.b, pki.caFile));
	h.b.port->Enable();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeB: TLS:", spdlog::level::err) >= 1; }, 5000));
	const std::string text = Captured().First("NodeB: TLS:");
	INFO(text);
	CHECK(text.find("unable to get local issuer certificate") != std::string::npos);
	CHECK(text.find("iccp-node-x.test") != std::string::npos);
	//the initiator is told by an alert (TLS 1.3: after its own handshake completed)
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: TLS:", spdlog::level::err) >= 1; }, 5000));
	CHECK(h.b.port->GetStatistics()["Associations"].asUInt64() == 0);
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK(h.a.recorder.Count(odc::ConnectState::CONNECTED) == 0);
}

TEST_CASE(SUITE("a peer that is not on the allow list is refused"))
{
	auto& pki = GetPki();
	Captured().Clear();
	Pair h(20409, "Client", "Server", false, TlsOf(pki.a, pki.caFile), TlsOf(pki.b, pki.caFile, R"({"PeerCertAllowList": ["CN:someone-else.test"]})"));
	h.b.port->Enable();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeB: TLS:", spdlog::level::err) >= 1; }, 5000));
	CHECK(Captured().First("NodeB: TLS:").find("not in PeerCertAllowList") != std::string::npos);
	CHECK_FALSE(h.b.port->AssociationUp());
}

TEST_CASE(SUITE("a plain client to a TLS server fails cleanly and quickly"))
{
	auto& pki = GetPki();
	Captured().Clear();
	Pair h(20410, "Client", "Server", false, Json::Value(), TlsOf(pki.b, pki.caFile)); //A (initiator) is plain, B (listener) is TLS
	h.b.port->Enable();
	const auto start = std::chrono::steady_clock::now();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeB: TLS:", spdlog::level::err) >= 1; }, 5000));
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
	const std::string text = Captured().First("NodeB: TLS:");
	INFO(text);
	CHECK(text.find("not a TLS handshake record") != std::string::npos);
	CHECK(text.find("plain RFC 1006") != std::string::npos);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK_FALSE(h.b.port->AssociationUp());
	CHECK(h.b.port->GetStatistics()["Associations"].asUInt64() == 0);
}

TEST_CASE(SUITE("a TLS client to a plain server fails cleanly and quickly"))
{
	auto& pki = GetPki();
	Captured().Clear();
	Pair h(20411, "Client", "Server", false, TlsOf(pki.a, pki.caFile), Json::Value()); //A (initiator) is TLS, B (listener) is plain
	h.b.port->Enable();
	const auto start = std::chrono::steady_clock::now();
	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: TLS:", spdlog::level::err) >= 1; }, 5000));
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));
	const std::string text = Captured().First("NodeA: TLS:");
	INFO(text);
	CHECK(text.find("is the peer using TLS on this port?") != std::string::npos);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK_FALSE(h.b.port->AssociationUp());
	CHECK(h.b.port->GetStatistics()["Associations"].asUInt64() == 0);
	CHECK(h.a.port->GetStatus()["TLS"]["State"].asString() != "Established");
}

TEST_CASE(SUITE("a silent peer: the handshake times out"))
{
	auto& pki = GetPki();
	Captured().Clear();
	Pair h(20412, "Client", "Server", false, TlsOf(pki.a, pki.caFile, R"({"HandshakeTimeoutms": 600})"), Json::Value());
	//a listener that accepts and says nothing, in place of B (which is not enabled)
	auto acceptor = h.ios->make_tcp_acceptor(odc::tcp::endpoint(asio::ip::address_v4::loopback(), 20412));
	auto held = h.ios->make_tcp_socket();
	acceptor->async_accept(*held, [](std::error_code) {});

	h.a.port->Enable();
	REQUIRE(WaitFor([&] { return Captured().Count("NodeA: TLS:", spdlog::level::err) >= 1; }, 5000));
	const std::string text = Captured().First("NodeA: TLS:");
	INFO(text);
	CHECK(text.find("did not complete within 600 ms") != std::string::npos);
	CHECK_FALSE(h.a.port->AssociationUp());
	CHECK(h.a.port->GetStatus()["TLS"]["State"].asString() == "Failed");
	h.a.port->Disable();
	REQUIRE(WaitFor([&] { return h.a.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
}

TEST_CASE(SUITE("renewed certificate files are used for the next connection"))
{
	auto& pki = GetPki();
	tlstest::TempDir dir;
	//B's certificate lives in files that are replaced while the ports are enabled
	const std::string cert = dir.Write("b.crt", pki.serverB.certPem);
	const std::string key = dir.Write("b.key", pki.serverB.keyPem);
	Pair h(20413, "Client", "Server", false, TlsOf(pki.a, pki.caFile), TlsOf({cert, key}, pki.caFile));
	h.b.port->Enable(); //B listens
	h.a.port->Enable();
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
	CHECK(h.a.port->GetStatus()["TLS"]["PeerSubject"].asString() == "CN=iccp-node-b.test");

	h.b.port->Disable();
	REQUIRE(WaitFor([&] { return h.a.recorder.Count(odc::ConnectState::DISCONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return h.b.recorder.Count(odc::ConnectState::PORT_DOWN) == 1; }));
	//the renewal: new files (a new certificate under the same CA), with a newer modification time
	dir.Write("b.crt", pki.serverRenewed.certPem);
	dir.Write("b.key", pki.serverRenewed.keyPem);
	for(const auto& f : {cert, key})
		std::filesystem::last_write_time(f, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10));
	h.b.port->Enable();
	for(auto* s : {&h.a, &h.b})
		REQUIRE(WaitFor([&] { return s->recorder.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
	CHECK(h.a.port->GetStatus()["TLS"]["PeerSubject"].asString() == "CN=iccp-node-a-renewed.test");
	CHECK(Captured().Count("the certificate files changed on disk: reloaded") >= 1);
}

TEST_CASE(SUITE("TLS port status and a port with TLS off"))
{
	auto& pki = GetPki();
	{
		Pair plain(20414, "Client", "Server", false, Json::Value(), Json::Value());
		CHECK_FALSE(plain.a.port->GetStatus()["TLS"]["Enabled"].asBool());
	}
	Pair secure(20415, "Client", "Server", false, TlsOf(pki.a, pki.caFile), TlsOf(pki.b, pki.caFile));
	const Json::Value st = secure.a.port->GetStatus()["TLS"];
	CHECK(st["Enabled"].asBool());
	CHECK(st["State"].asString() == "Idle");
}

TEST_CASE(SUITE("a configuration with TLS needs the files: the port does not enable"))
{
	auto& pki = GetPki();
	Json::Value tls = TlsOf(pki.a, pki.caFile);
	tls["CertFile"] = pki.a.cert + ".missing";
	Pair h(20416, "Client", "Server", false, tls, Json::Value());
	REQUIRE_FALSE(h.a.port->ConfigValid());
	REQUIRE(h.a.port->ConfigErrors().size() >= 1);
	CHECK(h.a.port->ConfigErrors()[0].find("\"CertFile\"") != std::string::npos);
	h.a.port->Enable();
	CHECK_FALSE(h.a.port->AssociationUp());
}

#endif // ODC_ASIO_SSL
