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
 * StaleTransportTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Nothing written for one TCP connection may reach the next. The ICCP ports talk through a small TCP proxy that records every byte of every
 *  connection in each direction, so the tests can say what the first bytes on each connection were: a TLS handshake record on a TLS port
 *  (never an application data record of an old session, which a peer answers with a fatal unexpected_message alert), a COTP connection
 *  request on a plain port (never a stale disconnect). The proxy also plays the strict peer: it answers a bad first record with that alert.
 *
 *  The stale writes are provoked the way they happen in the field: the TCP connection goes away and a new one comes up while the port's strand
 *  has not yet heard of it (here: the strand is held up by a test task), then the old association writes (the abort PDUs).
 *  Synthetic names only; TCP ports 20500 to 20599.
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include "TlsTestPki.h"
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>
#include <opendatacon/spdlog.h>
#include <random>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP stale transport - " name

namespace
{
using Bytes = std::vector<uint8_t>;
using Clock = std::chrono::steady_clock;

class CaptureSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
	size_t Count(const std::string& part) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(const auto& l : lines)
			if(l.find(part) != std::string::npos)
				++n;
		return n;
	}
	std::string First(const std::string& part) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& l : lines)
			if(l.find(part) != std::string::npos)
				return l;
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
		lines.push_back(std::string(msg.payload.data(), msg.payload.size()));
	}
	void flush_() override
	{}

private:
	mutable std::mutex mtx;
	std::vector<std::string> lines;
};

std::shared_ptr<CaptureSink> CapturePtr()
{
	static std::shared_ptr<CaptureSink> sink = std::make_shared<CaptureSink>();
	return sink;
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
bool WaitFor(Pred&& pred, int timeoutMs = 15000)
{
	const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
	while(!pred())
	{
		if(Clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// the recording proxy and strict peer
// ---------------------------------------------------------------------------------------------------------------

class Proxy
{
public:
	struct Conn
	{
		std::unique_ptr<odc::strand_t, odc::deleter> strand;
		std::shared_ptr<odc::tcp::socket> client;
		std::shared_ptr<odc::tcp::socket> server;
		std::mutex mtx;
		Bytes c2s, s2c;
		std::vector<std::pair<Clock::time_point, size_t>> chunksC2S, chunksS2C; //when each read arrived, and its size
		Clock::time_point firstC2S{}, firstS2C{}, closedAt{};
		bool closed = false;
		bool stray = false; //the client's first bytes were not what its protocol starts with
		char bufC[65536];
		char bufS[65536];
	};

	//tls: the client side speaks TLS (its first record must be a handshake record), else a plain COTP connection request
	Proxy(std::shared_ptr<odc::asio_service> service, int listenPort, int serverPort, bool tlsMode):
		ios(std::move(service)),
		upstreamPort(serverPort),
		tls(tlsMode)
	{
		acceptor = ios->make_tcp_acceptor(odc::tcp::endpoint(asio::ip::address_v4::loopback(), static_cast<unsigned short>(listenPort)));
		Accept();
	}
	~Proxy()
	{
		stopping = true;
		std::error_code ec;
		acceptor->close(ec);
		std::lock_guard<std::mutex> lock(mtx);
		for(auto& c : conns)
		{
			c->client->close(ec);
			c->server->close(ec);
		}
	}

	size_t Connections() const
	{
		std::lock_guard<std::mutex> lock(mtx);
		return conns.size();
	}
	std::shared_ptr<Conn> Get(size_t i) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		return i < conns.size() ? conns[i] : nullptr;
	}
	Bytes C2S(size_t i) const
	{
		auto c = Get(i);
		if(!c)
			return {}
		;
		std::lock_guard<std::mutex> lock(c->mtx);
		return c->c2s;
	}
	Bytes S2C(size_t i) const
	{
		auto c = Get(i);
		if(!c)
			return {}
		;
		std::lock_guard<std::mutex> lock(c->mtx);
		return c->s2c;
	}
	bool Closed(size_t i) const
	{
		auto c = Get(i);
		if(!c)
			return false;
		std::lock_guard<std::mutex> lock(c->mtx);
		return c->closed;
	}
	size_t StrayConnections() const
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(auto& c : conns)
		{
			std::lock_guard<std::mutex> l2(c->mtx);
			n += c->stray ? 1 : 0;
		}
		return n;
	}
	//cut a connection abruptly, both ways
	void Kill(size_t i)
	{
		if(auto c = Get(i))
			Close(c);
	}

private:
	std::shared_ptr<odc::asio_service> ios;
	const int upstreamPort;
	const bool tls;
	std::atomic<bool> stopping{false};
	std::unique_ptr<odc::tcp::acceptor, odc::deleter> acceptor;
	mutable std::mutex mtx;
	std::vector<std::shared_ptr<Conn>> conns;

	void Accept()
	{
		std::shared_ptr<odc::tcp::socket> client = ios->make_tcp_socket();
		acceptor->async_accept(*client, [this, client](std::error_code ec)
			{
				if(ec || stopping)
					return;
				Start(client);
				Accept();
			});
	}
	void Start(std::shared_ptr<odc::tcp::socket> client)
	{
		auto c = std::make_shared<Conn>();
		c->strand = ios->make_strand();
		c->client = client;
		c->server = ios->make_tcp_socket();
		{
			std::lock_guard<std::mutex> lock(mtx);
			conns.push_back(c);
		}
		c->server->async_connect(odc::tcp::endpoint(asio::ip::address_v4::loopback(), static_cast<unsigned short>(upstreamPort)),
			c->strand->wrap([this, c](std::error_code ec)
				{
					if(ec)
					{
						Close(c);
						return;
					}
					Pump(c, true);
					Pump(c, false);
				}));
	}
	static void Close(const std::shared_ptr<Conn>& c)
	{
		c->strand->post([c]()
			{
				{
					std::lock_guard<std::mutex> lock(c->mtx);
					if(c->closed)
						return;
					c->closed = true;
					c->closedAt = Clock::now();
				}
				std::error_code ec;
				c->client->close(ec);
				c->server->close(ec);
			});
	}
	void Pump(const std::shared_ptr<Conn>& c, bool toServer)
	{
		auto& from = toServer ? c->client : c->server;
		auto& to = toServer ? c->server : c->client;
		char* buf = toServer ? c->bufC : c->bufS;
		from->async_read_some(asio::buffer(buf, 65536), c->strand->wrap([this, c, toServer, buf, &to](std::error_code ec, size_t n)
			{
				if(ec)
				{
					Close(c);
					return;
				}
				bool reject = false;
				{
					std::lock_guard<std::mutex> lock(c->mtx);
					Bytes& rec = toServer ? c->c2s : c->s2c;
					if(rec.empty())
					{
						(toServer ? c->firstC2S : c->firstS2C) = Clock::now();
						if(toServer)
						{
							const auto* d = reinterpret_cast<const uint8_t*>(buf);
							const bool ok = tls ? (n >= 3 && d[0] == 0x16 && d[1] == 0x03) : (n >= 6 && d[0] == 0x03 && d[1] == 0x00 && d[5] == 0xE0);
							if(!ok)
							{
								c->stray = true;
								reject = true;
							}
						}
					}
					rec.insert(rec.end(), buf, buf + n);
					(toServer ? c->chunksC2S : c->chunksS2C).emplace_back(Clock::now(), n);
				}
				if(reject && tls)
				{
					//the strict peer: a record that is not a hello before the hello is answered with a fatal unexpected_message alert
					static const uint8_t alert[7] = {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x0A};
					auto data = std::make_shared<Bytes>(alert, alert + 7);
					asio::async_write(*c->client, asio::buffer(*data), c->strand->wrap([this, c, data](std::error_code, size_t) { Close(c); }));
					return;
				}
				if(reject)
				{
					Close(c);
					return;
				}
				auto data = std::make_shared<Bytes>(buf, buf + n);
				asio::async_write(*to, asio::buffer(*data), c->strand->wrap([this, c, toServer, data](std::error_code ec2, size_t)
					{
						if(ec2)
						{
							Close(c);
							return;
						}
						Pump(c, toServer);
					}));
			}));
	}
};

// ---------------------------------------------------------------------------------------------------------------
// the ports
// ---------------------------------------------------------------------------------------------------------------

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
	void Event(std::shared_ptr<const odc::EventInfo>, const std::string&, odc::SharedStatusCallback_t cb) override
	{
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

private:
	std::mutex mtx;
	std::vector<odc::ConnectState> states;
};

Json::Value ParseJson(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b, in, &v, &errs));
	return v;
}

//the server's configuration (a Server that serves one point) and the client's (a Client that reads it with a transfer set)
std::string NodeText(bool server, int port, const Json::Value& tls)
{
	Json::Value j;
	j["ServiceRole"] = server ? "Server" : "Client";
	j["IP"] = "127.0.0.1";
	j["Port"] = std::to_string(port);
	j["TASE2Edition"] = "2000-08";
	j["TCPConnectRetryPeriodms"] = 50;
	j["LocalTSelector"] = server ? "0001" : "0002";
	j["RemoteTSelector"] = server ? "0002" : "0001";
	j["LocalSSelector"] = server ? "0001" : "0002";
	j["RemoteSSelector"] = server ? "0002" : "0001";
	j["LocalPSelector"] = server ? "00000001" : "00000002";
	j["RemotePSelector"] = server ? "00000002" : "00000001";
	j["LocalAPTitle"] = server ? "1.3.9999.1.1" : "1.3.9999.1.2";
	j["RemoteAPTitle"] = server ? "1.3.9999.1.2" : "1.3.9999.1.1";
	j["LocalAEQualifier"] = server ? 1 : 2;
	j["RemoteAEQualifier"] = server ? 2 : 1;
	j["BilateralTableID"] = server ? "BT_A" : "BT_B";
	j["RemoteBilateralTableID"] = server ? "BT_B" : "BT_A";
	j["LocalDomain"] = server ? "DOMAIN_A" : "DOMAIN_B";
	j["RemoteDomain"] = server ? "DOMAIN_B" : "DOMAIN_A";
	Json::Value point;
	point["Index"] = 0;
	point["Name"] = "POINT_0001_ST";
	j["Binaries"].append(point);
	if(server)
	{
		j["TransferSetPool"].append("TS_POOL_001");
		j["TransferSetPool"].append("TS_POOL_002");
	}
	else
	{
		Json::Value ds;
		ds["Name"] = "DS_TEST_001";
		ds["Members"].append("POINT_0001_ST");
		j["DataSets"].append(ds);
		Json::Value ts;
		ts["DataSet"] = "DS_TEST_001";
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

//client port -> proxy -> server port
struct Rig
{
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::unique_ptr<Proxy> proxy;
	std::shared_ptr<ICCPServerPort> server;
	std::shared_ptr<ICCPClientPort> client;
	Recorder clientRec, serverRec;
	std::string serverFile, clientFile;
	const bool tls;

	Rig(int basePort, const Json::Value& clientTls, const Json::Value& serverTls, bool disableGuards = false):
		serverFile("iccp_stale_server_" + std::to_string(basePort) + ".conf"),
		clientFile("iccp_stale_client_" + std::to_string(basePort) + ".conf"),
		tls(!clientTls.isNull())
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 4; i++)
			threads.emplace_back([this] { ios->run(); });
		{
			std::ofstream s(serverFile);
			s << NodeText(true, basePort + 1, serverTls);
			std::ofstream c(clientFile);
			c << NodeText(false, basePort, clientTls);
		}
		proxy = std::make_unique<Proxy>(ios, basePort, basePort + 1, tls);
		server = std::make_shared<ICCPServerPort>("StaleServer", serverFile, Json::Value(Json::objectValue));
		client = std::make_shared<ICCPClientPort>("StaleClient", clientFile, Json::Value(Json::objectValue));
		server->Build();
		client->Build();
		server->Subscribe(&serverRec, "Recorder");
		client->Subscribe(&clientRec, "Recorder");
		if(disableGuards)
		{
			server->TestDisableStaleGuards();
			client->TestDisableStaleGuards();
		}
	}
	~Rig()
	{
		client->Disable();
		server->Disable();
		WaitFor([&] { return !client->AssociationUp() && !server->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		client.reset();
		server.reset();
		proxy.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(serverFile.c_str());
		std::remove(clientFile.c_str());
	}
	void Start()
	{
		REQUIRE(server->ConfigValid());
		REQUIRE(client->ConfigValid());
		server->Enable();
		client->Enable();
	}
	bool UpCount(size_t n)
	{
		return clientRec.Count(odc::ConnectState::CONNECTED) == n && serverRec.Count(odc::ConnectState::CONNECTED) == n;
	}
};

bool IsTlsHandshake(const Bytes& b)
{
	return b.size() >= 3 && b[0] == 0x16 && b[1] == 0x03;
}
//the TLS records of a stream: "type:length" each
std::vector<std::pair<int, size_t>> Records(const Bytes& b)
{
	std::vector<std::pair<int, size_t>> r;
	for(size_t i = 0; i + 5 <= b.size();)
	{
		const size_t len = (static_cast<size_t>(b[i + 3]) << 8) | b[i + 4];
		r.emplace_back(b[i], len);
		i += 5 + len;
	}
	return r;
}
std::string RecordList(const Bytes& b)
{
	std::string s;
	for(const auto& [t, l] : Records(b))
		s += std::to_string(t) + ":" + std::to_string(l) + " ";
	return s;
}
bool IsCotpCr(const Bytes& b)
{
	return b.size() >= 6 && b[0] == 0x03 && b[1] == 0x00 && b[5] == 0xE0;
}
bool IsCotpCc(const Bytes& b)
{
	return b.size() >= 6 && b[0] == 0x03 && b[1] == 0x00 && b[5] == 0xD0;
}
std::string HexHead(const Bytes& b, size_t n = 8)
{
	static const char d[] = "0123456789abcdef";
	std::string s;
	for(size_t i = 0; i < b.size() && i < n; ++i)
	{
		s.push_back(d[b[i] >> 4]);
		s.push_back(d[b[i] & 15]);
		s.push_back(' ');
	}
	return s;
}

//the association has finished its set-up traffic (the transfer set started) and nothing is in flight
void Quiesce(Rig& rig)
{
	REQUIRE(WaitFor([&] { return rig.client->GetStatus()["Provisioning"].size() == 1 && rig.client->GetStatus()["Provisioning"][0]["State"].asString() == "transfer-started"; }));
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

//The first bytes on a connection must be what its protocol starts with
void CheckAllConnections(Rig& rig, bool tls)
{
	for(size_t i = 0; i < rig.proxy->Connections(); ++i)
	{
		const Bytes c = rig.proxy->C2S(i);
		const Bytes s = rig.proxy->S2C(i);
		INFO("connection " << i << " client first bytes " << HexHead(c) << " server first bytes " << HexHead(s));
		if(!c.empty())
			CHECK((tls ? IsTlsHandshake(c) : IsCotpCr(c)));
		if(!s.empty())
			CHECK((tls ? (s[0] == 0x16 || s[0] == 0x15) : IsCotpCc(s)));
	}
	CHECK(rig.proxy->StrayConnections() == 0);
}

#ifdef ODC_ASIO_SSL
//realistic certificates: RSA 2048, a root CA and an intermediate CA, the chain in the certificate file
struct RealisticPki
{
	tlstest::TempDir dir;
	std::string caFile, serverCert, serverKey, clientCert, clientKey;
	RealisticPki()
	{
		const auto root = tlstest::MakeCert({"Example Root CA", {}, -3600, 86400LL * 3650, true, 1, "", 2048});
		const auto inter = tlstest::MakeCert({"Example Issuing CA", {}, -3600, 86400LL * 3000, true, 2, "", 2048}, &root);
		const auto server = tlstest::MakeCert({"iccp-server.example", {"DNS:iccp-server.example", "IP:127.0.0.1"}, -3600, 86400LL * 800, false, 100, "", 2048}, &inter);
		const auto client = tlstest::MakeCert({"iccp-client.example", {"DNS:iccp-client.example"}, -3600, 86400LL * 800, false, 101, "", 2048}, &inter);
		caFile = dir.Write("root.crt", root.certPem);
		serverCert = dir.Write("server-chain.crt", server.certPem + inter.certPem); //the certificate and its issuing CA
		serverKey = dir.Write("server.key", server.keyPem);
		clientCert = dir.Write("client-chain.crt", client.certPem + inter.certPem);
		clientKey = dir.Write("client.key", client.keyPem);
	}
};
RealisticPki& GetPki()
{
	static RealisticPki pki;
	return pki;
}
Json::Value TlsJson(bool server, const std::string& extra = "")
{
	auto& pki = GetPki();
	Json::Value t = extra.empty() ? Json::Value(Json::objectValue) : ParseJson(extra);
	t["Enabled"] = true;
	t["CertFile"] = server ? pki.serverCert : pki.clientCert;
	t["KeyFile"] = server ? pki.serverKey : pki.clientKey;
	t["CAFile"] = pki.caFile;
	return t;
}
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// plain TCP
// ---------------------------------------------------------------------------------------------------------------

namespace
{
//The client's connection goes away and a new one comes up before the client's strand has heard of it; then the old association writes.
//what = "client": the client's port is held up, "server": the server's
bool StaleWrites(int basePort, const Json::Value& clientTls, const Json::Value& serverTls, bool serverSide, bool expectClean, bool lastAttempt = true, bool guardsOff = false)
{
	Rig rig(basePort, clientTls, serverTls, !expectClean || guardsOff);
	const bool tls = !clientTls.isNull();
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	auto& victim = serverSide ? static_cast<ICCPPortBase&>(*rig.server) : static_cast<ICCPPortBase&>(*rig.client);
	REQUIRE(rig.proxy->Connections() == 1);

	//hold the victim's strand up, then cut the connection: the socket manager sees the end of the stream and (the client) reconnects while
	//the strand still believes in the old connection and its association; then the association aborts: its abort PDUs are written
	//(the held-up task waits until the new connection really exists, so the scenario does not depend on how fast the machine is)
	auto aborted = std::make_shared<std::atomic<bool>>(false);
	victim.TestPost([&rig, aborted](ICCPPortBase& p)
		{
			for(int i = 0; i < 500 && rig.proxy->Connections() < 2; ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
			p.TestAbortAssociation("test: the old association writes");
			*aborted = true;
		});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	rig.proxy->Kill(0);
	REQUIRE(WaitFor([&] { return rig.proxy->Connections() >= 2; }));
	REQUIRE(WaitFor([&] { return aborted->load(); }, 10000));
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); //let what was written (or not) arrive

	if(expectClean)
	{
		//the association comes up again, on the new connection, and nothing stale was written to it
		REQUIRE(WaitFor([&] { return rig.UpCount(2); }));
		CheckAllConnections(rig, tls);
		CHECK(rig.proxy->StrayConnections() == 0);
	}
	else
	{
		//with the guards off the stale bytes do reach the new connection (this is the bug): the first bytes are not a hello / a CR / a CC
		const Bytes first = serverSide ? rig.proxy->S2C(1) : rig.proxy->C2S(1);
		INFO("first bytes of the new connection: " << HexHead(first));
		//(the stale abort also closes the new connection at once, and with the guards off that close can win the race against the write,
		//so that nothing arrives at all: that proves nothing either way, and the caller tries again)
		if(first.empty() && !lastAttempt)
			return false;
		REQUIRE_FALSE(first.empty());
		if(serverSide)
			CHECK_FALSE((tls ? (first[0] == 0x16 || first[0] == 0x15) : IsCotpCc(first)));
		else
			CHECK_FALSE((tls ? IsTlsHandshake(first) : IsCotpCr(first)));
		if(!serverSide)
			CHECK(rig.proxy->StrayConnections() >= 1);
	}
	return true;
}

//the reproduction (guards off), with up to three more tries on other ports if the race above hides the stale bytes
void StaleWritesReproduced(int basePort, int retryPort, const Json::Value& clientTls, const Json::Value& serverTls, bool serverSide)
{
	if(StaleWrites(basePort, clientTls, serverTls, serverSide, false, false))
		return;
	for(int attempt = 0; attempt < 3; ++attempt)
		if(StaleWrites(retryPort + 2 * attempt, clientTls, serverTls, serverSide, false, attempt == 2))
			return;
}
}

TEST_CASE(SUITE("plain TCP: a stale association's abort never reaches the next connection (initiator)"))
{
	StaleWrites(20510, Json::Value(), Json::Value(), false, true);
}

TEST_CASE(SUITE("plain TCP: a stale association's abort never reaches the next connection (listener)"))
{
	StaleWrites(20512, Json::Value(), Json::Value(), true, true);
}

TEST_CASE(SUITE("plain TCP: without the guards the stale abort does reach the new connection (the bug, reproduced)"))
{
	StaleWritesReproduced(20514, 20560, Json::Value(), Json::Value(), false);
}

TEST_CASE(SUITE("plain TCP: a new transport event while the association is still up writes nothing for the old one"))
{
	Rig rig(20516, Json::Value(), Json::Value());
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	Quiesce(rig);
	const size_t before = rig.proxy->C2S(0).size();
	rig.client->TestSimulateTransportState(true); //the connection event, with the old association alive
	REQUIRE(WaitFor([&] { return rig.proxy->C2S(0).size() > before; }));
	const Bytes all = rig.proxy->C2S(0);
	const Bytes added(all.begin() + before, all.end());
	INFO("bytes written after the event: " << HexHead(added));
	CHECK(IsCotpCr(added)); //the new association's connection request, not an abort of the old one
	//and the pair recovers
	REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::CONNECTED) >= 2; }, 20000));
}

TEST_CASE(SUITE("plain TCP: repeated drops and reconnects, clean and abrupt"))
{
	Rig rig(20518, Json::Value(), Json::Value());
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	const int cycles = 50;
	for(int i = 1; i <= cycles; ++i)
	{
		INFO("cycle " << i);
		if(i % 2 == 0)
			rig.proxy->Kill(rig.proxy->Connections() - 1); //abrupt
		else
		{
			rig.client->Disable(); //clean: release, then the port is enabled again
			REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::PORT_DOWN) == static_cast<size_t>((i + 1) / 2); }));
			rig.client->Enable();
		}
		REQUIRE(WaitFor([&]
			{
				return rig.clientRec.Count(odc::ConnectState::CONNECTED) == static_cast<size_t>(i + 1) &&
				       rig.serverRec.Count(odc::ConnectState::CONNECTED) == static_cast<size_t>(i + 1);
			}, 20000));
	}
	CheckAllConnections(rig, false);
	CHECK(rig.proxy->Connections() >= static_cast<size_t>(cycles + 1));
}

TEST_CASE(SUITE("plain TCP: connections cut at every moment of the association set-up"))
{
	Rig rig(20519, Json::Value(), Json::Value());
	rig.Start();
	std::mt19937 rng(98);
	for(int i = 0; i < 60; ++i)
	{
		const size_t n = rig.proxy->Connections();
		REQUIRE(WaitFor([&] { return rig.proxy->Connections() > n; }));
		std::this_thread::sleep_for(std::chrono::microseconds(rng() % 8000));
		rig.proxy->Kill(rig.proxy->Connections() - 1);
	}
	REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::CONNECTED) >= 1 && rig.client->AssociationUp() && rig.server->AssociationUp(); }, 30000));
	CheckAllConnections(rig, false);
}

#ifdef ODC_ASIO_SSL
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("TLS: the realistic setting (RSA 2048, certificate chains, TLS 1.3, client certificate required) stays up"))
{
	CapturePtr()->Clear();
	Rig rig(20520, TlsJson(false), TlsJson(true));
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	//nothing closes the connection after the handshake, from either side, for as long as we look
	std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	CHECK(rig.UpCount(1));
	CHECK(rig.clientRec.Count(odc::ConnectState::DISCONNECTED) == 0);
	CHECK(rig.serverRec.Count(odc::ConnectState::DISCONNECTED) == 0);
	CHECK(rig.proxy->Connections() == 1);
	CHECK_FALSE(rig.proxy->Closed(0));
	CHECK(CapturePtr()->Count("TLS established") == 2); //one per side
	CHECK(CapturePtr()->Count("error") == 0);
	const Bytes c = rig.proxy->C2S(0), s = rig.proxy->S2C(0);
	INFO("client wrote " << c.size() << " bytes, server wrote " << s.size() << " bytes");
	WARN("client records " << RecordList(c));
	WARN("server records " << RecordList(s));
	{
		//the timeline of the connection, in ms from the client's first byte
		auto conn = rig.proxy->Get(0);
		std::lock_guard<std::mutex> lock(conn->mtx);
		const auto t0 = conn->chunksC2S.front().first;
		std::string tl;
		for(const auto& [t, n] : conn->chunksC2S)
			tl += "c2s +" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t - t0).count()) + "us " + std::to_string(n) + "B; ";
		for(const auto& [t, n] : conn->chunksS2C)
			tl += "s2c +" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t - t0).count()) + "us " + std::to_string(n) + "B; ";
		WARN("timeline: " << tl);
		//the client's last record before the application data is its Finished: the connection request follows at once, in its own record
		const auto recs = Records(c);
		REQUIRE(recs.size() >= 5);
		CHECK(recs[0].first == 0x16); //ClientHello
		CHECK(recs.back().first == 0x17);
		CHECK(conn->chunksC2S.size() >= 2); //the hello, then the second flight (with the connection request)
	}
	CHECK(IsTlsHandshake(c));
	CHECK(IsTlsHandshake(s));
	//after the client's Finished the client does send application data: the COTP connection request and what follows
	CHECK(c.size() > 2000);
	CheckAllConnections(rig, true);
	//the association carries data: the server's status shows it, and the client's transfer set was started
	REQUIRE(WaitFor([&] { return rig.client->GetStatus()["Provisioning"].size() == 1 && rig.client->GetStatus()["Provisioning"][0]["State"].asString() == "transfer-started"; }));
	CHECK(rig.server->GetStatus()["TLS"]["State"].asString() == "Established");
	CHECK(rig.client->GetStatus()["TLS"]["Protocol"].asString() == "TLSv1.3");
}

TEST_CASE(SUITE("TLS: a stale session's abort never reaches the next connection (initiator)"))
{
	StaleWrites(20530, TlsJson(false), TlsJson(true), false, true);
}

TEST_CASE(SUITE("TLS: a stale session's abort never reaches the next connection (listener)"))
{
	StaleWrites(20532, TlsJson(false), TlsJson(true), true, true);
}

TEST_CASE(SUITE("TLS: switching the port's own guards off doesn't let a stale session's abort reach the next connection"))
{
	//The abort goes to the new connection's session, which starts with its own handshake, so it never reaches the wire as it was
	StaleWrites(20534, TlsJson(false), TlsJson(true), false, true, true, true);
	StaleWrites(20570, TlsJson(false), TlsJson(true), true, true, true, true);
}

TEST_CASE(SUITE("TLS: a new transport event while the session is still up ends the association and writes nothing for the old one"))
{
	Rig rig(20536, TlsJson(false), TlsJson(true));
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	Quiesce(rig);
	const size_t before = rig.proxy->C2S(0).size();
	rig.client->TestSimulateTransportState(true);                   //the connection event, with the old association and its TLS session alive
	REQUIRE(WaitFor([&] { return !rig.client->AssociationUp(); })); //the old association is ended: nothing more is written for it
	//a session starts only on a new TCP connection, so no second handshake starts and nothing is written on the old connection
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	CHECK(rig.proxy->C2S(0).size() == before);
	CHECK(rig.proxy->Connections() == 1);
	//and the pair recovers on a new connection, which starts with its own handshake
	rig.proxy->Kill(0);
	REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::CONNECTED) >= 2 && rig.client->AssociationUp() && rig.server->AssociationUp(); }, 20000));
	CheckAllConnections(rig, true);
}

TEST_CASE(SUITE("TLS: with KeyLogFile the session secrets are written, for Wireshark to decrypt a capture, and it is warned about"))
{
	CapturePtr()->Clear();
	tlstest::TempDir dir;
	const std::string keys = dir.Path("keys.log");
	Json::Value clientTls = TlsJson(false);
	clientTls["KeyLogFile"] = keys;
	Rig rig(20544, clientTls, TlsJson(true));
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	std::ifstream f(keys);
	const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	CHECK(text.find("CLIENT_TRAFFIC_SECRET_0 ") != std::string::npos);
	CHECK(CapturePtr()->Count("StaleClient: TLS: KEY LOGGING IS ON") >= 2); //at start-up and for the connection
	CHECK(CapturePtr()->Count("StaleServer: TLS: KEY LOGGING IS ON") == 0);
}

TEST_CASE(SUITE("TLS: repeated drops and reconnects, clean and abrupt"))
{
	Rig rig(20538, TlsJson(false), TlsJson(true));
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.UpCount(1); }));
	const int cycles = 50;
	for(int i = 1; i <= cycles; ++i)
	{
		INFO("cycle " << i);
		if(i % 2 == 0)
			rig.proxy->Kill(rig.proxy->Connections() - 1);
		else
		{
			rig.client->Disable();
			REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::PORT_DOWN) == static_cast<size_t>((i + 1) / 2); }));
			rig.client->Enable();
		}
		REQUIRE(WaitFor([&]
			{
				return rig.clientRec.Count(odc::ConnectState::CONNECTED) == static_cast<size_t>(i + 1) &&
				       rig.serverRec.Count(odc::ConnectState::CONNECTED) == static_cast<size_t>(i + 1);
			}, 20000));
	}
	CheckAllConnections(rig, true);
	CHECK(rig.proxy->Connections() >= static_cast<size_t>(cycles + 1));
}

TEST_CASE(SUITE("TLS: the server closes the session right after the handshake, and the next connection starts clean"))
{
	//what the capture showed: the connection the server closed, then the next attempt. Here the proxy plays the server's FIN.
	Rig rig(20540, TlsJson(false), TlsJson(true));
	rig.Start();
	REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::CONNECTED) == 1; }));
	rig.proxy->Kill(0);
	REQUIRE(WaitFor([&] { return rig.UpCount(2); }));
	CheckAllConnections(rig, true);
}

TEST_CASE(SUITE("TLS: connections cut at every moment of the handshake and just after it"))
{
	Rig rig(20542, TlsJson(false), TlsJson(true));
	rig.Start();
	std::mt19937 rng(99);
	for(int i = 0; i < 60; ++i)
	{
		const size_t n = rig.proxy->Connections();
		REQUIRE(WaitFor([&] { return rig.proxy->Connections() > n; }));
		std::this_thread::sleep_for(std::chrono::microseconds(rng() % 14000)); //somewhere in or just after the handshake
		rig.proxy->Kill(rig.proxy->Connections() - 1);
	}
	//whatever came of that, the association comes up once the cutting stops, and no connection began with a stale byte
	REQUIRE(WaitFor([&] { return rig.clientRec.Count(odc::ConnectState::CONNECTED) >= 1 && rig.client->AssociationUp() && rig.server->AssociationUp(); }, 30000));
	CheckAllConnections(rig, true);
}

#endif // ODC_ASIO_SSL
