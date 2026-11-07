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
 * TCPSessionManagerTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TCPSessionManager (TCPSessionManager.h) over real TCP on the loopback interface: two managers talking to each other, and a scripted
 *  peer (a plain TCP listener that runs the TLS filter, or not, and can be told to hold back, drop the connection or send garbage).
 *  The TLS tests need ODC_ASIO_SSL. Synthetic names only; TCP ports 20700 to 20799.
 */

#include "../ThreadPool.h"
#ifdef ODC_ASIO_SSL
#include "TlsTestPki.h"
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <opendatacon/TCPSessionManager.h>
#include <string>
#include <thread>
#include <vector>

#define SUITE(name) "TCPSessionManager - " name

using namespace odc;

namespace
{
using Clock = std::chrono::steady_clock;

struct Files
{
	std::string cert;
	std::string key;
};

#ifdef ODC_ASIO_SSL
struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa, server, client;
	Files serverF, clientF;
	std::string caFile, otherCaFile;

	Pki()
	{
		ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Test CA", {}, -3600, 86400LL * 3650, true, 1});
		server = tlstest::MakeCert({"tls-server.test", {"DNS:tls-server.test", "IP:127.0.0.1"}, -3600, 86400LL * 365, false, 100}, &ca);
		client = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 200}, &ca);
		serverF = {dir.Write("server.crt", server.certPem), dir.Write("server.key", server.keyPem)};
		clientF = {dir.Write("client.crt", client.certPem), dir.Write("client.key", client.keyPem)};
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
	}
};

Pki& GetPki()
{
	static Pki pki;
	return pki;
}

tls::TlsConf ServerConf(const Files& id, bool requireClientCert = true)
{
	tls::TlsConf c;
	c.enabled = true;
	c.certFile = id.cert;
	c.keyFile = id.key;
	c.caFile = GetPki().caFile;
	c.requireClientCert = requireClientCert;
	return c;
}

tls::TlsConf ClientConf(const Files* id, const std::string& caFile)
{
	tls::TlsConf c;
	c.enabled = true;
	if(id)
	{
		c.certFile = id->cert;
		c.keyFile = id->key;
	}
	c.caFile = caFile;
	return c;
}

std::shared_ptr<tls::Context> MakeContext(const tls::TlsConf& conf, tls::Role role)
{
	std::string error;
	auto ctx = tls::Context::Create(conf, role, error);
	INFO("Context::Create: " << error);
	REQUIRE(ctx);
	return ctx;
}
#endif

//One manager, with a record of what its callbacks were given
struct Peer
{
	std::mutex mtx;
	std::string received;
	size_t readCalls = 0;
	size_t maxChunk = 0;
	std::vector<bool> states;
	std::vector<std::string> failures; //the reasons of the sessions that failed (in the handshake as well)
	std::vector<tls::PeerInfo> infos;
	std::vector<std::string> logs;
	//what SetSessionEvents() reported
	std::vector<std::string> events; //"up 1", "est 1", "end Failed 1 est=0", "down 1", in the order they came
	std::vector<TCPSessionManager::SessionEnd> ends;
	std::unique_ptr<TCPSessionManager> mgr; //last: destroyed first, before what its callbacks use

	//Init() calls it (unless told not to); call it again before Open() for other settings
	void InitEvents(bool reconnectAfterEnd = true, int tickMs = 0)
	{
		TCPSessionManager::SessionEvents ev;
		ev.onConnection = [this](bool up, uint64_t serial)
					{
						std::lock_guard<std::mutex> lock(mtx);
						events.push_back(std::string(up ? "up " : "down ") + std::to_string(serial));
					};
		ev.onEstablished = [this](const tls::PeerInfo& info, uint64_t serial)
					 {
						 std::lock_guard<std::mutex> lock(mtx);
						 events.push_back("est " + std::to_string(serial));
						 infos.push_back(info);
					 };
		ev.onEnded = [this](const TCPSessionManager::SessionEnd& end)
				 {
					 std::lock_guard<std::mutex> lock(mtx);
					 static const char* names[] = {"Failed", "PeerClosed", "ConnectionLost", "ClosedLocally"};
					 events.push_back(std::string("end ") + names[static_cast<int>(end.kind)] + " " + std::to_string(end.serial) + " est=" + (end.established ? "1" : "0"));
					 ends.push_back(end);
					 if(end.kind == TCPSessionManager::SessionEnd::Kind::Failed || !end.handshakeSummary.empty())
						 failures.push_back(end.reason);
				 };
		mgr->SetSessionEvents(ev);
		mgr->SetReconnectAfterSessionEnd(reconnectAfterEnd);
		if(tickMs > 0)
			mgr->SetTickPeriod(std::chrono::milliseconds(tickMs));
	}
	std::string Events()
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::string s;
		for(const auto& e : events)
			s += (s.empty() ? "" : ", ") + e;
		return s;
	}
	size_t Ends()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return ends.size();
	}
	TCPSessionManager::SessionEnd End(size_t i)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return i < ends.size() ? ends[i] : TCPSessionManager::SessionEnd();
	}
	auto ReadCallback()
	{
		return [this](buf_t& buf)
			 {
				 std::string s(buf.size(), '\0');
				 asio::buffer_copy(asio::buffer(s), buf.data());
				 buf.consume(buf.size());
				 std::lock_guard<std::mutex> lock(mtx);
				 received += s;
				 ++readCalls;
				 maxChunk = std::max(maxChunk, s.size());
			 };
	}
	auto StateCallback()
	{
		return [this](bool state)
			 {
				 std::lock_guard<std::mutex> lock(mtx);
				 states.push_back(state);
			 };
	}
	auto LogCallback()
	{
		return [this](const std::string& level, const std::string& text)
			 {
				 std::lock_guard<std::mutex> lock(mtx);
				 logs.push_back(level + ": " + text);
			 };
	}
	//TLS
	void Init(bool server, uint16_t port, std::shared_ptr<tls::Context> ctx, const std::string& peerName = "", bool autoReopen = true, uint16_t retryMs = 50, bool withEvents = true)
	{
		mgr = std::make_unique<TCPSessionManager>(odc::asio_service::Get(), server, "127.0.0.1", std::to_string(port), ctx, peerName,
			ReadCallback(), StateCallback(), std::numeric_limits<size_t>::max(), autoReopen, retryMs, 0, 0, 0, LogCallback());
		if(withEvents)
			InitEvents();
	}
	//plain TCP
	void InitPlain(bool server, uint16_t port, bool autoReopen = true, uint16_t retryMs = 50)
	{
		mgr = std::make_unique<TCPSessionManager>(odc::asio_service::Get(), server, "127.0.0.1", std::to_string(port),
			ReadCallback(), StateCallback(), std::numeric_limits<size_t>::max(), autoReopen, retryMs, 0, 0, 0, LogCallback());
		InitEvents();
	}
	size_t Received()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return received.size();
	}
	std::string Data()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return received;
	}
	size_t Count(bool state)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return static_cast<size_t>(std::count(states.begin(), states.end(), state));
	}
	size_t Failures()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return failures.size();
	}
	std::string Failure(size_t i)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return i < failures.size() ? failures[i] : std::string();
	}
	size_t Infos()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return infos.size();
	}
	tls::PeerInfo Info(size_t i)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return i < infos.size() ? infos[i] : tls::PeerInfo();
	}
	bool Logged(const std::string& text)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return std::any_of(logs.begin(), logs.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
	}
	std::string LogText()
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::string t;
		for(size_t i = logs.size() > 25 ? logs.size() - 25 : 0; i < logs.size(); ++i)
			t += "    " + logs[i] + "\n";
		return t;
	}
};

//Wait (polling) for a condition. A timeout fails the test, with the logs of the peers.
template <typename P>
void Wait(P pred, const std::string& what, std::vector<Peer*> peers = {}, int timeoutMs = 20000)
{
	const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
	while(!pred())
	{
		if(Clock::now() > end)
		{
			std::string text = "timed out waiting for: " + what + "\n";
			for(size_t i = 0; i < peers.size(); ++i)
				text += "  peer " + std::to_string(i) + " log:\n" + peers[i]->LogText();
			FAIL(text);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

//For a condition that must not come true: it is held false for this long
template <typename P>
bool StaysFalse(P pred, int ms = 300)
{
	const auto end = Clock::now() + std::chrono::milliseconds(ms);
	while(Clock::now() < end)
	{
		if(pred())
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

#ifdef ODC_ASIO_SSL
std::string Pattern(size_t n, unsigned seed)
{
	std::string s(n, '\0');
	uint32_t x = seed * 2654435761u + 1;
	for(size_t i = 0; i < n; ++i)
	{
		x = x * 1664525u + 1013904223u;
		s[i] = static_cast<char>(x >> 24);
	}
	return s;
}
#endif

//A scripted TCP peer on the loopback interface, on its own io service and thread. With a context it runs the TLS server filter on the connection
//(it takes one connection at a time: a new one replaces the last), without one it is plain TCP and what it receives is what the other side sent.
class RawServer
{
public:
	RawServer(uint16_t aPort, std::shared_ptr<tls::Context> aCtx):
		port(aPort),
		ctx(std::move(aCtx)),
		acceptor(io),
		guard(asio::make_work_guard(io))
	{
		DoListen();
		thread = std::thread([this] { io.run(); });
	}
	~RawServer()
	{
		asio::post(io, [this]
			{
				std::error_code ec;
				acceptor.close(ec);
				if(cur)
					cur->sock.close(ec);
			});
		guard.reset();
		thread.join();
	}

	//-- control (asynchronous, in order)
	void Hold(bool h) //the bytes of the peer are kept, not passed to the TLS filter, until this is called with false
	{
		asio::post(io, [this, h]
			{
				hold = h;
				if(!h && cur)
				{
					tls::Bytes data = std::move(cur->held);
					cur->held.clear();
					if(!data.empty())
						Deliver(cur, data.data(), data.size());
				}
			});
	}
	void SendRaw(const tls::Bytes& b) //on the wire as it is, not through the filter
	{
		asio::post(io, [this, b] { DoSendRaw(b); });
	}
	void SendPlain(const std::string& s)
	{
		asio::post(io, [this, s]
			{
				if(cur && cur->filter)
					cur->filter->Write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
			});
	}
	void ShutdownTls() //close_notify
	{
		asio::post(io, [this] { DoShutdownTls(); });
	}
	void Drop() //close the TCP connection
	{
		asio::post(io, [this]
			{
				std::error_code ec;
				if(cur)
					cur->sock.close(ec);
			});
	}
	void StopListening()
	{
		asio::post(io, [this]
			{
				std::error_code ec;
				acceptor.close(ec);
			});
	}
	void Listen()
	{
		asio::post(io, [this] { DoListenAgain(); });
	}

	//-- what happened
	void BePolite(bool p) { polite = p; }
	size_t Accepted() const { return accepted.load(); }
	size_t Established() const { return established.load(); }
	size_t PeerClosed() const { return peerClosed.load(); }
	size_t Failed() const { return failed.load(); }
	size_t Eofs() const { return eofs.load(); }
	std::string Plain() //what the current connection delivered (decrypted if TLS)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return plain;
	}

private:
	struct Conn
	{
		explicit Conn(asio::io_context& io): sock(io) {}
		asio::ip::tcp::socket sock;
		std::unique_ptr<tls::Filter> filter;
		std::deque<std::shared_ptr<tls::Bytes>> queue;
		bool writing = false;
		std::array<uint8_t, 16384> buf;
		tls::Bytes held;
	};

	void DoSendRaw(const tls::Bytes& b)
	{
		if(cur)
			QueueWrite(cur, b);
	}
	void DoShutdownTls()
	{
		if(cur && cur->filter)
			cur->filter->Shutdown();
	}
	void DoListenAgain()
	{
		if(!acceptor.is_open())
			DoListen();
	}
	void OnPlain(const uint8_t* d, size_t n)
	{
		std::lock_guard<std::mutex> lock(mtx);
		plain.append(reinterpret_cast<const char*>(d), n);
	}
	void DoListen()
	{
		std::error_code ec;
		acceptor.open(asio::ip::tcp::v4(), ec);
		acceptor.set_option(asio::socket_base::reuse_address(true), ec);
		acceptor.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port), ec);
		acceptor.listen(asio::socket_base::max_listen_connections, ec);
		if(!ec)
			AsyncAccept();
	}
	void AsyncAccept()
	{
		auto conn = std::make_shared<Conn>(io);
		acceptor.async_accept(conn->sock, [this, conn](const std::error_code& ec)
			{
				if(ec)
					return; //the acceptor was closed
				OnAccepted(conn);
				if(acceptor.is_open())
					AsyncAccept();
			});
	}
	void OnAccepted(const std::shared_ptr<Conn>& conn)
	{
		if(cur)
		{
			std::error_code ec;
			cur->sock.close(ec);
		}
		cur = conn;
		{
			std::lock_guard<std::mutex> lock(mtx);
			plain.clear();
		}
		if(ctx)
		{
			tls::FilterCallbacks cb;
			cb.send = [this, conn](const tls::Bytes& b) { QueueWrite(conn, b); };
			cb.onPlain = [this](const uint8_t* d, size_t n) { OnPlain(d, n); };
			cb.onEstablished = [this](const tls::PeerInfo&) { ++established; };
			cb.onFailed = [this](const std::string&) { ++failed; };
			cb.onPeerClosed = [this]() { ++peerClosed; };
			cb.log = [](tls::LogLevel, const std::string&) {};
			conn->filter = std::make_unique<tls::Filter>(ctx, tls::Role::Server, std::move(cb));
			conn->filter->Start();
		}
		++accepted;
		Read(conn);
	}
	void Read(const std::shared_ptr<Conn>& conn)
	{
		conn->sock.async_read_some(asio::buffer(conn->buf), [this, conn](const std::error_code& ec, size_t n)
			{
				if(ec)
				{
					if(ec == asio::error::eof)
					{
						++eofs;
						if(polite && conn == cur) //as a TCP stack does: the peer is done sending, so is this end
						{
							std::error_code ignored;
							conn->sock.close(ignored);
						}
					}
					return;
				}
				if(hold && conn == cur)
					conn->held.insert(conn->held.end(), conn->buf.begin(), conn->buf.begin() + n);
				else
					Deliver(conn, conn->buf.data(), n);
				if(conn->sock.is_open())
					Read(conn);
			});
	}
	void Deliver(const std::shared_ptr<Conn>& conn, const uint8_t* data, size_t n)
	{
		if(conn->filter)
			conn->filter->OnTransportData(data, n);
		else
		{
			std::lock_guard<std::mutex> lock(mtx);
			plain.append(reinterpret_cast<const char*>(data), n);
		}
	}
	void QueueWrite(const std::shared_ptr<Conn>& conn, const tls::Bytes& b)
	{
		conn->queue.push_back(std::make_shared<tls::Bytes>(b));
		if(!conn->writing)
			WriteNext(conn);
	}
	void WriteNext(const std::shared_ptr<Conn>& conn)
	{
		if(conn->queue.empty())
		{
			conn->writing = false;
			return;
		}
		conn->writing = true;
		auto b = conn->queue.front();
		asio::async_write(conn->sock, asio::buffer(*b), [this, conn, b](const std::error_code& ec, size_t)
			{
				conn->queue.pop_front();
				if(ec)
				{
					conn->writing = false;
					return;
				}
				WriteNext(conn);
			});
	}

	const uint16_t port;
	std::shared_ptr<tls::Context> ctx;
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	asio::executor_work_guard<asio::io_context::executor_type> guard;
	std::thread thread;
	std::shared_ptr<Conn> cur; //io thread only
	bool hold = false;         //io thread only
	std::mutex mtx;
	std::string plain;
	std::atomic<size_t> accepted{0}, established{0}, peerClosed{0}, failed{0}, eofs{0};
	std::atomic<bool> polite{true}; //closes its end when the other end has closed theirs; false = never closes, for the timeout of the manager
};

//A plain TCP client: connects (waiting for the listener), sends what it is given and tells whether the peer ended the connection
class RawClient
{
public:
	explicit RawClient(uint16_t port): sock(io)
	{
		const auto end = Clock::now() + std::chrono::seconds(10);
		std::error_code ec;
		do
		{
			sock.close(ec);
			sock.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port), ec);
			if(ec)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} while(ec && Clock::now() < end);
		connected = !ec;
	}
	bool Connected() const { return connected; }
	void Send(const tls::Bytes& b)
	{
		std::error_code ec;
		asio::write(sock, asio::buffer(b), ec);
	}
	//true if the peer ended the connection (end of stream or reset) within the time; whatever it sent is thrown away
	bool PeerEnded(int timeoutMs)
	{
		std::error_code ec;
		sock.non_blocking(true, ec);
		const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
		std::array<uint8_t, 4096> buf;
		while(Clock::now() < end)
		{
			sock.read_some(asio::buffer(buf), ec);
			if(ec && ec != asio::error::would_block)
				return true;
			if(ec == asio::error::would_block)
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return false;
	}

private:
	asio::io_context io;
	asio::ip::tcp::socket sock;
	bool connected = false;
};

#ifdef ODC_ASIO_SSL
//a rewrite of a file that the Context sees as changed, however coarse the clock of the file system
void Rewrite(const std::string& path, const std::string& content, int generation)
{
	{
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		f << content;
	}
	std::error_code ec;
	std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10 * generation), ec);
}
#endif

void ConnectedBoth(Peer& a, Peer& b)
{
	Wait([&] { return a.mgr->Established() && b.mgr->Established(); }, "both ends established", {&a, &b});
}

#ifdef ODC_ASIO_SSL
bool Equal(const std::string& a, const std::string& b)
{
	return a == b;
}
#endif
}

TEST_CASE(SUITE("plain: data both ways, writes tied to a connection, and the events"))
{
	ThreadPool pool(3);
	const uint16_t port = 20760;
	Peer server, client;
	server.InitPlain(true, port);
	client.InitPlain(false, port);
	CHECK(!client.mgr->IsTls());
	client.mgr->Write(std::string("before open")); //no session: dropped
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);
	CHECK(client.mgr->Connected());
	CHECK(client.mgr->ConnectionSerial() == 1);
	CHECK(client.Events() == "up 1, est 1");
	REQUIRE(client.Infos() == 1);
	CHECK(!client.Info(0).certificate);

	client.mgr->Write(std::string("hello server"));
	server.mgr->Write(std::vector<uint8_t>{'h', 'i'});
	client.mgr->Write(std::string("tied"), client.mgr->ConnectionSerial());
	client.mgr->Write(std::string("stale"), client.mgr->ConnectionSerial() + 7);
	Wait([&] { return server.Received() == 16 && client.Received() == 2; }, "the data in both directions", {&server, &client});
	CHECK(server.Data() == "hello servertied");
	CHECK(client.Data() == "hi");

	//the client ends the connection, not itself: it connects again, and nothing of the first one reaches the second
	client.mgr->CloseConnection(client.mgr->ConnectionSerial() + 1); //not the live one: ignored
	client.mgr->Write(std::string("last"));
	client.mgr->CloseConnection(1);
	client.mgr->Write(std::string("dropped"), 1);
	Wait([&] { return client.Count(true) == 2 && server.Count(true) == 2; }, "the next connection", {&server, &client});
	CHECK(client.mgr->ConnectionSerial() == 2);
	CHECK(client.End(0).kind == TCPSessionManager::SessionEnd::Kind::ClosedLocally);
	CHECK(server.End(0).kind == TCPSessionManager::SessionEnd::Kind::ConnectionLost);
	client.mgr->Write(std::string("second"), 2);
	Wait([&] { return server.Received() == 26; }, "the write on the second connection", {&server, &client});
	CHECK(server.Data() == "hello servertiedlastsecond");

	client.mgr->Close();
	Wait([&] { return client.Count(false) == 2 && server.Count(false) == 2; }, "both ends down", {&server, &client});
	CHECK(client.Events() == "up 1, est 1, end ClosedLocally 1 est=1, down 1, up 2, est 2, end ClosedLocally 2 est=1, down 2");
	CHECK(StaysFalse([&] { return client.mgr->Connected(); }));
	server.mgr->Close();
}

TEST_CASE(SUITE("plain: a manager destroyed while connected, or never opened, does not hang"))
{
	ThreadPool pool(3);
	const uint16_t port = 20761;
	Peer server, client, idle;
	server.InitPlain(true, port);
	client.InitPlain(false, port);
	idle.InitPlain(false, port + 1);
	idle.mgr->Write(std::string("x"));
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);
	client.mgr->Write(std::string("data in flight"));
	client.mgr.reset();
	Wait([&] { return server.Count(false) == 1; }, "the server noticing", {&server});
	idle.mgr.reset();
	server.mgr.reset();
}

#ifdef ODC_ASIO_SSL

TEST_CASE(SUITE("data both ways, the state callbacks and the peer certificate"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20700;
	Peer server, client;
	server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	CHECK(!client.mgr->Connected());
	CHECK(!client.mgr->Established());
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);
	CHECK(client.mgr->Connected());
	CHECK(client.mgr->ConnectionSerial() == 1);
	CHECK(server.mgr->ConnectionSerial() == 1);

	client.mgr->Write(std::string("hello server"));
	server.mgr->Write(std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o', ' ', 'c', 'l', 'i', 'e', 'n', 't'});
	Wait([&] { return server.Received() == 12 && client.Received() == 12; }, "the data in both directions", {&server, &client});
	CHECK(server.Data() == "hello server");
	CHECK(client.Data() == "hello client");

	//the state callbacks: true once each so far, and the certificates were reported before them
	CHECK(server.Count(true) == 1);
	CHECK(client.Count(true) == 1);
	CHECK(server.Count(false) == 0);
	REQUIRE(client.Infos() == 1);
	REQUIRE(server.Infos() == 1);
	const auto seen = client.Info(0); //what the client saw of the server
	CHECK(seen.certificate);
	CHECK(seen.subject.find("tls-server.test") != std::string::npos);
	CHECK(!seen.protocol.empty());
	CHECK(!seen.cipher.empty());
	CHECK(server.Info(0).subject.find("tls-client.test") != std::string::npos);
	CHECK(client.Failures() == 0);
	CHECK(server.Failures() == 0);

	//a Write tied to the connection serial: the right one is sent, a stale one is dropped
	client.mgr->Write(std::string("tied"), client.mgr->ConnectionSerial());
	client.mgr->Write(std::string("stale"), client.mgr->ConnectionSerial() + 7);
	client.mgr->Write(std::string("!"));
	Wait([&] { return server.Received() == 12 + 5; }, "the tied write", {&server, &client});
	CHECK(server.Data() == "hello servertied!");
	CHECK(server.Data().find("stale") == std::string::npos);

	client.mgr->Close();
	Wait([&] { return client.Count(false) == 1 && server.Count(false) == 1; }, "the state callbacks false", {&server, &client});
	CHECK(!client.mgr->Established());
	CHECK(!server.mgr->Established());
	server.mgr->Close();
}

TEST_CASE(SUITE("large buffers both ways arrive complete and in chunks"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20701;
	Peer server, client;
	server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);

	const size_t big = 4u * 1024 * 1024;
	const std::string toServer = Pattern(big, 1);
	const std::string toClient = Pattern(3 * 1024 * 1024 + 17, 2);
	//one write of 4 MB
	client.mgr->Write(toServer);
	//and 100 kB writes the other way, at the same time
	for(size_t off = 0; off < toClient.size(); off += 100000)
		server.mgr->Write(toClient.substr(off, 100000));
	Wait([&] { return server.Received() == toServer.size() && client.Received() == toClient.size(); }, "the large transfers", {&server, &client}, 60000);
	CHECK(Equal(server.Data(), toServer));
	CHECK(Equal(client.Data(), toClient));
	{
		std::lock_guard<std::mutex> lock(server.mtx);
		CHECK(server.readCalls > 1); //chunked: no one read holds it all
		CHECK(server.maxChunk <= 65536);
	}
	client.mgr->Close();
	server.mgr->Close();
}

TEST_CASE(SUITE("bytes written before the handshake completes are delivered after it"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20702;
	RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
	server.Hold(true); //the server does not answer the handshake yet
	Peer client;
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	client.mgr->Open();
	Wait([&] { return client.mgr->Connected(); }, "the TCP connection with a session", {&client});
	CHECK(!client.mgr->Established());
	client.mgr->Write(std::string("early one,"));
	client.mgr->Write(std::string("early two"));
	CHECK(StaysFalse([&] { return client.mgr->Established(); }, 100));
	CHECK(server.Plain().empty());
	server.Hold(false);
	Wait([&] { return client.mgr->Established(); }, "the handshake after the hold", {&client});
	Wait([&] { return server.Plain().size() == 19; }, "the early writes", {&client});
	CHECK(server.Plain() == "early one,early two");
	CHECK(client.Failures() == 0);
	client.mgr->Close();
}

TEST_CASE(SUITE("a write while there is no connection is dropped, never carried over"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20703;
	Peer client;
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	client.mgr->Write(std::string("before open")); //not even opened
	client.mgr->Open();
	client.mgr->Write(std::string("while refused")); //nobody listens: the manager keeps retrying
	Wait([&] { return client.Logged("Connection error"); }, "a connection attempt that failed", {&client});
	client.mgr->Write(std::string("still refused"));
	{
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		Wait([&] { return client.mgr->Established(); }, "the connection once the peer listens", {&client});
		client.mgr->Write(std::string("first"));
		Wait([&] { return server.Plain().size() == 5; }, "the first write of the session", {&client});
		CHECK(server.Plain() == "first");
		CHECK(server.Established() == 1);
		CHECK(server.Failed() == 0);
		client.mgr->Close();
		Wait([&] { return client.Count(false) == 1; }, "the state callback false", {&client});
		client.mgr->Write(std::string("after close")); //no session any more
	}
}

TEST_CASE(SUITE("a handshake that fails reports onTlsFailed and the state never goes true"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("the client trusts another CA than the server's")
	{
		const uint16_t port = 20704;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.otherCaFile), tls::Role::Client));
		server.mgr->Open();
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the failure of the client", {&server, &client});
		CHECK(!client.Failure(0).empty());
		CHECK(client.Count(true) == 0);
		CHECK(server.Count(true) == 0);
		CHECK(!client.mgr->Established());
		CHECK(client.Infos() == 0);
		client.mgr->Close();
		server.mgr->Close();
	}
	SECTION("the server requires a client certificate and the client has none")
	{
		const uint16_t port = 20705;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF, true), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(nullptr, pki.caFile), tls::Role::Client));
		server.mgr->Open();
		client.mgr->Open();
		Wait([&] { return server.Failures() >= 1; }, "the failure of the server", {&server, &client});
		CHECK(!server.Failure(0).empty());
		CHECK(server.Count(true) == 0);
		CHECK(server.Infos() == 0);
		CHECK(!server.mgr->Established());
		client.mgr->Close();
		server.mgr->Close();
	}
	SECTION("a server that does not require one accepts a client without")
	{
		const uint16_t port = 20706;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF, false), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(nullptr, pki.caFile), tls::Role::Client));
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		REQUIRE(server.Infos() == 1);
		CHECK(!server.Info(0).certificate);
		CHECK(server.Failures() == 0);
		client.mgr->Close();
		server.mgr->Close();
	}
}

TEST_CASE(SUITE("hostname verification on and off"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("on, the name is in the certificate")
	{
		const uint16_t port = 20707;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		auto conf = ClientConf(&pki.clientF, pki.caFile);
		conf.verifyHostname = true;
		client.Init(false, port, MakeContext(conf, tls::Role::Client), "tls-server.test");
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		CHECK(client.Failures() == 0);
		client.mgr->Close();
		server.mgr->Close();
	}
	SECTION("on, with no name given the connect address is verified (the certificate has it as an IP address)")
	{
		const uint16_t port = 20708;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		auto conf = ClientConf(&pki.clientF, pki.caFile);
		conf.verifyHostname = true;
		client.Init(false, port, MakeContext(conf, tls::Role::Client), "");
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		client.mgr->Close();
		server.mgr->Close();
	}
	SECTION("on, the name is not in the certificate")
	{
		const uint16_t port = 20709;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		auto conf = ClientConf(&pki.clientF, pki.caFile);
		conf.verifyHostname = true;
		client.Init(false, port, MakeContext(conf, tls::Role::Client), "somewhere-else.test");
		server.mgr->Open();
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the failure of the client", {&server, &client});
		CHECK(client.Count(true) == 0);
		CHECK(!client.mgr->Established());
		client.mgr->Close();
		server.mgr->Close();
	}
	SECTION("off, the name is not in the certificate")
	{
		const uint16_t port = 20710;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client), "somewhere-else.test");
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		CHECK(client.Failures() == 0);
		client.mgr->Close();
		server.mgr->Close();
	}
}

TEST_CASE(SUITE("after the peer drops the TCP connection the manager reconnects, with a new session and nothing of the old one"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20711;
	RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
	Peer client;
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	client.mgr->Open();
	Wait([&] { return client.mgr->Established(); }, "the first session", {&client});
	CHECK(client.mgr->ConnectionSerial() == 1);
	client.mgr->Write(std::string("old"));
	server.SendPlain("old reply");
	Wait([&] { return server.Plain() == "old" && client.Received() == 9; }, "the data of the first session", {&client});

	server.StopListening(); //so that the manager can't reconnect yet
	server.Drop();
	Wait([&] { return client.Count(false) == 1; }, "the state callback false", {&client});
	CHECK(!client.mgr->Established());
	CHECK(!client.mgr->Connected());
	client.mgr->Write(std::string("while down")); //no session: dropped
	//a connection that ended after the handshake is not a failure
	CHECK(client.Failures() == 0);

	server.Listen();
	Wait([&] { return client.mgr->Established() && client.Count(true) == 2; }, "the second session", {&client});
	CHECK(client.mgr->ConnectionSerial() == 2);
	client.mgr->Write(std::string("new"));
	server.SendPlain("new reply");
	Wait([&] { return server.Plain() == "new" && client.Received() == 18; }, "the data of the second session", {&client});
	CHECK(server.Plain() == "new"); //nothing of "old" or "while down"
	CHECK(client.Data() == "old replynew reply");
	CHECK(server.Established() == 2);
	CHECK(server.Failed() == 0);
	CHECK(client.Failures() == 0);
	client.mgr->Close();
}

TEST_CASE(SUITE("Close() sends close_notify, the peer sees the session closed and the manager does not reconnect"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("to a scripted peer")
	{
		const uint16_t port = 20712;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.mgr->Open();
		Wait([&] { return client.mgr->Established() && server.Established() == 1; }, "the session", {&client});
		client.mgr->Write(std::string("last words"));
		client.mgr->Close();
		Wait([&] { return server.PeerClosed() == 1; }, "close_notify at the peer", {&client});
		CHECK(server.Plain() == "last words"); //written before the close_notify
		Wait([&] { return client.Count(false) == 1; }, "the state callback false", {&client});
		Wait([&] { return server.Eofs() == 1; }, "the TCP connection closed", {&client});
		CHECK(client.Failures() == 0);
		CHECK(StaysFalse([&] { return server.Accepted() > 1 || client.mgr->Connected(); }));
	}
	SECTION("between two managers, from either side")
	{
		const uint16_t port = 20713;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		client.mgr->Close();
		Wait([&] { return server.Count(false) == 1 && client.Count(false) == 1; }, "both ends down", {&server, &client});
		CHECK(server.Failures() == 0);
		CHECK(client.Failures() == 0);
		CHECK(server.End(0).kind == TCPSessionManager::SessionEnd::Kind::PeerClosed);

		//the manager that was closed can be opened again
		client.mgr->Open();
		ConnectedBoth(server, client);
		CHECK(client.Count(true) == 2);
		server.mgr->Close(); //and the server side closes the session just the same
		Wait([&] { return server.Count(false) == 2 && client.Count(false) == 2; }, "both ends down again", {&server, &client});
		Wait([&] { return client.Ends() == 2; }, "the client's second session end", {&server, &client});
		CHECK(client.End(1).kind == TCPSessionManager::SessionEnd::Kind::PeerClosed);
		CHECK(client.Failures() == 0);
		client.mgr->Close();
	}
}

TEST_CASE(SUITE("a certificate file changed on disk is picked up by the next connection"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20714;
	tlstest::TempDir dir;
	Files files = {dir.Write("server.crt", pki.server.certPem), dir.Write("server.key", pki.server.keyPem)};
	auto second = tlstest::MakeCert({"tls-server.test", {"DNS:tls-server.test", "IP:127.0.0.1"}, -3600, 86400LL * 365, false, 101}, &pki.ca);

	Peer server, client;
	server.Init(true, port, MakeContext(ServerConf(files), tls::Role::Server));
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);
	REQUIRE(client.Infos() == 1);
	CHECK(client.Info(0).serial == "64"); //100

	//renewed certificate and key
	Rewrite(files.cert, second.certPem, 1);
	Rewrite(files.key, second.keyPem, 1);
	client.mgr->Close();
	Wait([&] { return server.Count(false) == 1 && client.Count(false) == 1; }, "the first session over", {&server, &client});
	client.mgr->Open();
	Wait([&] { return client.Count(true) == 2 && server.Count(true) == 2 && client.mgr->Established(); }, "the second session", {&server, &client});
	REQUIRE(client.Infos() == 2);
	CHECK(client.Info(1).serial == "65"); //101
	CHECK(server.Logged("reloaded"));

	//a file that can't be loaded: the connection fails closed, and the next one works again when the file does
	client.mgr->Close();
	Wait([&] { return server.Count(false) == 2 && client.Count(false) == 2; }, "the second session over", {&server, &client});
	const auto goodKey = second.keyPem;
	Rewrite(files.key, "this is not a key\n", 2);
	client.mgr->Open();
	Wait([&] { return server.Failures() >= 1; }, "the failure to load", {&server, &client});
	CHECK(server.Failure(0).find("cannot load the changed certificate files") != std::string::npos);
	CHECK(StaysFalse([&] { return server.Count(true) > 2 || client.Count(true) > 2; }, 200));
	Rewrite(files.key, goodKey, 3);
	Wait([&] { return client.mgr->Established() && client.Count(true) == 3; }, "the connection once the file is good", {&server, &client});
	client.mgr->Close();
	server.mgr->Close();
}

TEST_CASE(SUITE("a manager destroyed while connected, handshaking or waiting does not hang and does not take the process down"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const auto Elapsed = [](Clock::time_point since) { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count(); };

	SECTION("connected: the peer sees the session close")
	{
		const uint16_t port = 20715;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		server.mgr->Open();
		client.mgr->Open();
		ConnectedBoth(server, client);
		client.mgr->Write(std::string("data in flight"));
		const auto start = Clock::now();
		client.mgr.reset();
		CHECK(Elapsed(start) < 10000);
		Wait([&] { return server.Count(false) == 1; }, "the server noticing", {&server});
		CHECK(server.Failures() == 0);
		const auto start2 = Clock::now();
		server.mgr.reset();
		CHECK(Elapsed(start2) < 10000);
	}
	SECTION("handshaking: the peer never answers")
	{
		const uint16_t port = 20716;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		server.Hold(true);
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.mgr->Open();
		Wait([&] { return client.mgr->Connected(); }, "the TCP connection", {&client});
		CHECK(!client.mgr->Established());
		const auto start = Clock::now();
		client.mgr.reset();
		CHECK(Elapsed(start) < 10000);
		Wait([&] { return server.Eofs() >= 1; }, "the peer seeing the connection closed");
	}
	SECTION("a server with a plain TCP client that has said nothing")
	{
		const uint16_t port = 20717;
		Peer server;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		server.mgr->Open();
		RawClient raw(port);
		REQUIRE(raw.Connected());
		Wait([&] { return server.mgr->Connected(); }, "the connection", {&server});
		const auto start = Clock::now();
		server.mgr.reset();
		CHECK(Elapsed(start) < 10000);
		CHECK(raw.PeerEnded(10000));
	}
	SECTION("never opened, and opened but never connected")
	{
		Peer idle, lonely;
		idle.Init(false, 20718, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		lonely.Init(false, 20719, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		idle.mgr->Write(std::string("x"));
		lonely.mgr->Open();
		lonely.mgr->Write(std::string("y"));
		idle.mgr.reset();
		lonely.mgr.reset();
	}
}

TEST_CASE(SUITE("a peer that sends garbage ends that session and does not take the process down"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("garbage instead of the server hello")
	{
		const uint16_t port = 20720;
		RawServer server(port, nullptr); //plain TCP: it never answers the handshake properly
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.mgr->Open();
		tls::Bytes plausible = {0x16, 0x03, 0x03, 0x00, 0x30};
		for(int i = 0; i < 48; ++i)
			plausible.push_back(static_cast<uint8_t>(i * 7 + 3));
		const tls::Bytes http = {'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ', '4', '0', '0', '\r', '\n', '\r', '\n'};
		const std::vector<tls::Bytes> garbage = {plausible, tls::Bytes(300, 0xFF), http};
		for(size_t k = 0; k < garbage.size(); ++k)
		{
			Wait([&] { return client.mgr->ConnectionSerial() == k + 1 && client.mgr->Connected(); }, "connection " + std::to_string(k + 1), {&client});
			server.SendRaw(garbage[k]);
			Wait([&] { return client.Failures() >= k + 1; }, "the failure for garbage " + std::to_string(k), {&client});
		}
		CHECK(client.Count(true) == 0);
		CHECK(client.Infos() == 0);
		client.mgr->Close();
	}
	SECTION("garbage after the session is established")
	{
		const uint16_t port = 20721;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.mgr->Open();
		Wait([&] { return client.mgr->Established(); }, "the session", {&client});
		server.SendRaw(tls::Bytes(64, 0x41));
		Wait([&] { return client.Failures() == 1 && client.Count(false) == 1; }, "the failure and the state callback false", {&client});
		//and it connects again with a new session
		Wait([&] { return client.Count(true) == 2 && client.mgr->Established(); }, "the next session", {&client});
		client.mgr->Close();
	}
	SECTION("garbage to a server, which then serves the next client")
	{
		const uint16_t port = 20722;
		Peer server, client;
		server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		server.mgr->Open();
		{
			RawClient raw(port);
			REQUIRE(raw.Connected());
			tls::Bytes junk = {0x16, 0x03, 0x01, 0x00, 0x20};
			junk.resize(5 + 0x20, 0xA5);
			raw.Send(junk);
			Wait([&] { return server.Failures() >= 1; }, "the failure of the server", {&server});
			CHECK(raw.PeerEnded(10000));
		}
		CHECK(server.Count(true) == 0);
		client.mgr->Open();
		ConnectedBoth(server, client);
		client.mgr->Write(std::string("fine"));
		Wait([&] { return server.Received() == 4; }, "data after the garbage", {&server, &client});
		client.mgr->Close();
		server.mgr->Close();
	}
}

TEST_CASE(SUITE("a peer that never answers the handshake times out"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20723;
	RawServer server(port, nullptr);
	Peer client;
	auto conf = ClientConf(&pki.clientF, pki.caFile);
	conf.handshakeTimeoutMs = 1000;
	client.Init(false, port, MakeContext(conf, tls::Role::Client));
	client.mgr->Open();
	Wait([&] { return client.Failures() >= 1; }, "the handshake timeout", {&client}, 15000);
	CHECK(client.Failure(0).find("did not complete") != std::string::npos);
	CHECK(client.Count(true) == 0);
	client.mgr->Close();
}

TEST_CASE(SUITE("a peer that never closes its end of a failed session is closed on, and the manager connects again"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20726;
	RawServer server(port, nullptr);
	server.BePolite(false);
	Peer client;
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	client.mgr->Open();
	Wait([&] { return client.mgr->ConnectionSerial() == 1 && client.mgr->Connected(); }, "the first connection", {&client});
	server.SendRaw(tls::Bytes(100, 0xFF));
	Wait([&] { return client.Failures() >= 1; }, "the failure", {&client});
	Wait([&] { return client.mgr->ConnectionSerial() >= 2 && server.Accepted() >= 2; }, "the next connection", {&client}, 15000);
	client.mgr->Close();
}

TEST_CASE(SUITE("without a usable context every connection fails closed"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("no context")
	{
		const uint16_t port = 20724;
		RawServer server(port, nullptr);
		Peer client;
		client.Init(false, port, nullptr);
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the failure", {&client});
		CHECK(client.Failure(0).find("no TLS context") != std::string::npos);
		client.mgr->Write(std::string("never sent"));
		CHECK(client.Count(true) == 0);
		CHECK(StaysFalse([&] { return !server.Plain().empty(); }, 100)); //nothing, not even a hello, was sent
		client.mgr->Close();
	}
	SECTION("a context of the other role")
	{
		const uint16_t port = 20725;
		RawServer server(port, nullptr);
		Peer client;
		client.Init(false, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the failure", {&client});
		CHECK(client.Failure(0).find("other role") != std::string::npos);
		CHECK(client.Count(true) == 0);
		client.mgr->Close();
	}
}

TEST_CASE(SUITE("session events: connection, established and end come in order with their serial, and the manager does not log what they report"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20740;
	Peer server, client;
	server.Init(true, port, MakeContext(ServerConf(pki.serverF), tls::Role::Server), "", true, 50, false); //(no events: it logs)
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
	client.InitEvents();
	server.mgr->Open();
	client.mgr->Open();
	ConnectedBoth(server, client);
	CHECK(client.Events() == "up 1, est 1");
	CHECK(server.Logged("TLS: established"));
	CHECK(!client.Logged("TLS: established")); //the owner has onEstablished: it reports it

	//we close: ClosedLocally, then the connection is gone
	client.mgr->Close();
	Wait([&] { return client.Ends() == 1 && client.Count(false) == 1 && server.Count(false) == 1; }, "the session closed", {&server, &client});
	Wait([&] { return client.Events() == "up 1, est 1, end ClosedLocally 1 est=1, down 1"; }, "the closing events in order", {&client});
	CHECK(client.End(0).reason.empty());
	CHECK(client.Failures() == 0);

	//the peer closes: PeerClosed, with no log line about it
	client.mgr->Open();
	Wait([&] { return client.mgr->Established() && server.mgr->Established(); }, "the second session", {&server, &client});
	CHECK(client.Events().find("up 2, est 2") != std::string::npos);
	server.mgr->Close();
	Wait([&] { return client.Count(false) == 2; }, "the client seeing the close", {&server, &client});
	Wait([&] { return client.Ends() == 2 && client.Events().find("end PeerClosed 2 est=1, down 2") != std::string::npos; }, "PeerClosed then down", {&client});
	CHECK(!client.Logged("close_notify"));
	CHECK(client.Failures() == 0);
	client.mgr->Close();
}

TEST_CASE(SUITE("session events: a TCP connection lost under a session, before and after the handshake"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("during the handshake")
	{
		const uint16_t port = 20741;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		server.Hold(true);
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.InitEvents();
		client.mgr->Open();
		Wait([&] { return client.mgr->Connected(); }, "the TCP connection", {&client});
		server.StopListening();
		server.Drop();
		Wait([&] { return client.Ends() >= 1; }, "the end", {&client});
		const auto end = client.End(0);
		CHECK(end.kind == TCPSessionManager::SessionEnd::Kind::ConnectionLost);
		CHECK(!end.established);
		CHECK(end.serial == 1);
		CHECK(end.reason.find("during the TLS handshake") != std::string::npos);
		CHECK(end.reason.find("is the peer using TLS on this port?") != std::string::npos); //(for a client)
		CHECK(end.reason.find("[" + end.handshakeSummary + "]") != std::string::npos);
		CHECK(!client.Logged("during the TLS handshake")); //reported, not logged
		Wait([&] { return client.Events().rfind("up 1, end ConnectionLost 1 est=0, down 1", 0) == 0; }, "the events in order", {&client});
		client.mgr->Close();
	}
	SECTION("after the handshake")
	{
		const uint16_t port = 20742;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.InitEvents();
		client.mgr->Open();
		Wait([&] { return client.mgr->Established(); }, "the session", {&client});
		server.StopListening();
		server.Drop();
		Wait([&] { return client.Ends() >= 1; }, "the end", {&client});
		const auto end = client.End(0);
		CHECK(end.kind == TCPSessionManager::SessionEnd::Kind::ConnectionLost);
		CHECK(end.established);
		CHECK(end.handshakeSummary.empty());
		CHECK(!client.Logged("the TCP connection ended"));
		CHECK(client.Failures() == 0);
		Wait([&] { return client.Events().rfind("up 1, est 1, end ConnectionLost 1 est=1, down 1", 0) == 0; }, "the events in order", {&client});
		client.mgr->Close();
	}
}

TEST_CASE(SUITE("session events: a session that fails, and a connection refused for want of a context"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();

	SECTION("garbage after the handshake")
	{
		const uint16_t port = 20743;
		RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
		Peer client;
		client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client));
		client.InitEvents();
		client.mgr->Open();
		Wait([&] { return client.mgr->Established(); }, "the session", {&client});
		server.SendRaw(tls::Bytes(100, 0xFF));
		Wait([&] { return client.Ends() >= 1; }, "the end", {&client});
		const auto end = client.End(0);
		CHECK(end.kind == TCPSessionManager::SessionEnd::Kind::Failed);
		CHECK(end.established); //it had been up
		CHECK(end.serial == 1);
		CHECK(!end.reason.empty());
		CHECK(!client.Logged(end.reason)); //reported, not logged
		CHECK(client.Failure(0) == end.reason);
		Wait([&] { return client.Events().rfind("up 1, est 1, end Failed 1 est=1, down 1", 0) == 0; }, "the events in order", {&client});
		client.mgr->Close();
	}
	SECTION("no context: the connection is reported up, then failed, though there is no session")
	{
		const uint16_t port = 20744;
		RawServer server(port, nullptr);
		Peer client;
		client.Init(false, port, nullptr);
		client.InitEvents();
		client.mgr->Open();
		Wait([&] { return client.Ends() >= 1; }, "the end", {&client});
		const auto end = client.End(0);
		CHECK(end.kind == TCPSessionManager::SessionEnd::Kind::Failed);
		CHECK(!end.established);
		CHECK(end.serial == 1);
		CHECK(end.reason.find("no TLS context") != std::string::npos);
		CHECK(!client.Logged("no TLS context"));
		Wait([&] { return client.Events().rfind("up 1, end Failed 1 est=0, down 1", 0) == 0; }, "the events in order", {&client});
		client.mgr->Close();
	}
}

TEST_CASE(SUITE("SetReconnectAfterSessionEnd(false): after a failed session or one the peer closed the owner decides when to connect again"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	const uint16_t port = 20745;
	RawServer server(port, MakeContext(ServerConf(pki.serverF), tls::Role::Server));
	Peer client;
	client.Init(false, port, MakeContext(ClientConf(&pki.clientF, pki.caFile), tls::Role::Client), "", true, 50);
	client.InitEvents(false);
	client.mgr->Open();
	Wait([&] { return client.mgr->Established(); }, "the first session", {&client});
	CHECK(server.Accepted() == 1);

	//failed: no new connection until Open()
	server.SendRaw(tls::Bytes(100, 0xFF));
	Wait([&] { return client.Ends() == 1 && client.Count(false) == 1; }, "the failure", {&client});
	CHECK(client.End(0).kind == TCPSessionManager::SessionEnd::Kind::Failed);
	CHECK(StaysFalse([&] { return server.Accepted() > 1 || client.mgr->Connected(); }, 600));
	client.mgr->Open();
	Wait([&] { return client.mgr->Established() && client.Count(true) == 2; }, "the second session after Open()", {&client});
	CHECK(server.Accepted() == 2);

	//closed by the peer: the same
	server.ShutdownTls();
	Wait([&] { return client.Ends() == 2 && client.Count(false) == 2; }, "the close by the peer", {&client});
	CHECK(client.End(1).kind == TCPSessionManager::SessionEnd::Kind::PeerClosed);
	CHECK(StaysFalse([&] { return server.Accepted() > 2 || client.mgr->Connected(); }, 600));
	client.mgr->Open();
	Wait([&] { return client.mgr->Established() && client.Count(true) == 3; }, "the third session after Open()", {&client});

	//a TCP connection that is lost is still reconnected by the manager itself
	server.Drop();
	Wait([&] { return client.Count(true) == 4 && client.mgr->Established(); }, "the fourth session without Open()", {&client});
	CHECK(client.End(2).kind == TCPSessionManager::SessionEnd::Kind::ConnectionLost);
	client.mgr->Close();
}

TEST_CASE(SUITE("SetTickPeriod(): the handshake timeout is checked at that period"))
{
	ThreadPool pool(3);
	auto& pki = GetPki();
	auto conf = ClientConf(&pki.clientF, pki.caFile);
	conf.handshakeTimeoutMs = 300;
	const auto Elapsed = [](Clock::time_point since) { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count(); };

	SECTION("the default is 1 s: the 300 ms timeout is noticed at the first tick")
	{
		const uint16_t port = 20746;
		RawServer server(port, nullptr); //never answers
		Peer client;
		client.Init(false, port, MakeContext(conf, tls::Role::Client));
		const auto start = Clock::now();
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the handshake timeout", {&client}, 15000);
		CHECK(client.Failure(0).find("did not complete within 300 ms") != std::string::npos);
		CHECK(Elapsed(start) >= 900);
		client.mgr->Close();
	}
	SECTION("20 ms: noticed at once")
	{
		const uint16_t port = 20747;
		RawServer server(port, nullptr);
		Peer client;
		client.Init(false, port, MakeContext(conf, tls::Role::Client));
		client.InitEvents(true, 20);
		const auto start = Clock::now();
		client.mgr->Open();
		Wait([&] { return client.Failures() >= 1; }, "the handshake timeout", {&client}, 15000);
		CHECK(client.Failure(0).find("did not complete within 300 ms") != std::string::npos);
		CHECK(Elapsed(start) >= 300);
		CHECK(Elapsed(start) < 900);
		client.mgr->Close();
	}
}

#endif
