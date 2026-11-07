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
 * NetTlsTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Tests of odc.TLSSocket: real Lua scripts in a LuaPort, against each other and against TCPSessionManager peers on loopback, with certificates
 *  made at run time by the test PKI of the ODC tests. TCP ports 20700 to 20799. Without TLS support only the 'no_tls' test is compiled in.
 */
#include "NetTestHelpers.h"
#include <opendatacon/TCPSessionManager.h>
#include <cstdlib>
#include <future>
#include <map>
#include <sstream>

#ifdef ODC_ASIO_SSL
#include "TlsTestPki.h"
#endif

#define SUITE(name) "LuaNetTlsTestSuite - " name

using namespace nettest;

TEST_CASE(SUITE("Without TLS support the constructor returns no_tls"))
{
	if(odc::tls::TlsSupported())
	{
		INFO("This build has TLS support: the 'no_tls' answer is tested in a build without it.");
		return;
	}
	Json::Value conf;
	conf["port"] = 20799;
	LuaHarness h(R"lua(
function Enable()
	local s, err = odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = { CAFile = "none.pem" } }
	report("result", tostring(s)..","..tostring(err))
	local t, terr = odc.TCPSocket{ host = "127.0.0.1", port = odc.Config.port }
	report("tcp", tostring(t ~= nil)..","..tostring(terr))
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("result") == "nil,no_tls");
	CHECK(h.Expect("tcp") == "true,nil"); //plain TCP is not affected
}

#ifdef ODC_ASIO_SSL

namespace
{
struct Files
{
	std::string cert;
	std::string key;
};

//The certificates, made once: every name is synthetic
struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa, server, serverRenewed, serverOtherName, serverEncrypted, client, clientExpired;
	Files serverF, serverOtherNameF, serverEncryptedF, clientF, clientExpiredF;
	std::string caFile, otherCaFile;

	Pki()
	{
		const std::vector<std::string> sans = {"DNS:tls-server.test", "IP:127.0.0.1"};
		ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Test CA", {}, -3600, 86400LL * 3650, true, 1});
		server = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 100}, &ca);
		serverRenewed = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 101}, &ca);
		serverOtherName = tlstest::MakeCert({"other-name.test", {"DNS:other-name.test"}, -3600, 86400LL * 365, false, 102}, &ca);
		serverEncrypted = tlstest::MakeCert({"tls-server.test", sans, -3600, 86400LL * 365, false, 103, "correct-horse"}, &ca);
		client = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 200}, &ca);
		clientExpired = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -7200, -3600, false, 201}, &ca);
		serverF = {dir.Write("server.crt", server.certPem), dir.Write("server.key", server.keyPem)};
		serverOtherNameF = {dir.Write("othername.crt", serverOtherName.certPem), dir.Write("othername.key", serverOtherName.keyPem)};
		serverEncryptedF = {dir.Write("encrypted.crt", serverEncrypted.certPem), dir.Write("encrypted.key", serverEncrypted.keyPem)};
		clientF = {dir.Write("client.crt", client.certPem), dir.Write("client.key", client.keyPem)};
		clientExpiredF = {dir.Write("expired.crt", clientExpired.certPem), dir.Write("expired.key", clientExpired.keyPem)};
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
	}
};

Pki& GetPki()
{
	static Pki pki;
	return pki;
}

//the "tls" option of an object (as a JSON object, which the harness turns into a Lua table)
Json::Value TlsOpt(const Files* id, const std::string& ca)
{
	Json::Value t(Json::objectValue);
	if(id)
	{
		t["CertFile"] = id->cert;
		t["KeyFile"] = id->key;
	}
	t["CAFile"] = ca;
	return t;
}

void SetEnv(const char* name, const char* value)
{
	#ifdef _WIN32
	_putenv_s(name,value);
	#else
	setenv(name,value,1);
	#endif
}

std::shared_ptr<odc::tls::Context> MakeContext(const Files* id, const std::string& ca, odc::tls::Role role)
{
	odc::tls::TlsConf c;
	c.enabled = true;
	if(id)
	{
		c.certFile = id->cert;
		c.keyFile = id->key;
	}
	c.caFile = ca;
	std::string error;
	auto ctx = odc::tls::Context::Create(c,role,error);
	INFO("Context::Create: "+error);
	REQUIRE(ctx);
	return ctx;
}

//A TCPSessionManager with TLS on loopback, with a record of what it saw
class TlsPeer
{
public:
	TlsPeer(bool server, uint16_t port, std::shared_ptr<odc::tls::Context> ctx, bool auto_reopen)
	{
		mgr = std::make_unique<odc::TCPSessionManager>(odc::asio_service::Get(),server,"127.0.0.1",std::to_string(port),ctx,"",
			[this](odc::buf_t& buf)
			{
				std::lock_guard<std::mutex> lock(mtx);
				received.append(asio::buffers_begin(buf.data()),asio::buffers_end(buf.data()));
				buf.consume(buf.size());
			},
			[](bool){},std::numeric_limits<size_t>::max(),auto_reopen,20);
		odc::TCPSessionManager::SessionEvents events;
		events.onEstablished = [this](const odc::tls::PeerInfo&, uint64_t)
					     {
						     std::lock_guard<std::mutex> lock(mtx);
						     ++established;
					     };
		events.onEnded = [this](const odc::TCPSessionManager::SessionEnd& end)
				     {
					     static const char* const kinds[] = {"Failed","PeerClosed","ConnectionLost","ClosedLocally"};
					     std::lock_guard<std::mutex> lock(mtx);
					     ends.push_back(kinds[static_cast<int>(end.kind)]);
				     };
		mgr->SetSessionEvents(events);
	}
	void Open()
	{
		mgr->Open();
	}
	~TlsPeer()
	{
		mgr->Close();
	}
	int Established()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return established;
	}
	std::string Received()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return received;
	}
	std::vector<std::string> Ends()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return ends;
	}
	std::unique_ptr<odc::TCPSessionManager> mgr;
private:
	std::mutex mtx;
	std::string received;
	std::vector<std::string> ends;
	int established = 0;
};

//Accepts a connection, connects to the target and, after a delay, passes the bytes both ways: so the client's TCP connection is up but its handshake waits
class DelayRelay
{
public:
	DelayRelay(uint16_t listen_port, uint16_t target_port, std::chrono::milliseconds delay):
		acceptor(io,asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),listen_port)),
		target(target_port),
		delay(delay),
		a(io),
		b(io)
	{
		thread = std::thread([this](){ Run(); });
	}
	~DelayRelay()
	{
		stopping = true;
		WakeAcceptor(io,acceptor);
		std::error_code ec;
		acceptor.close(ec);
		a.close(ec);
		b.close(ec);
		if(thread.joinable())
			thread.join();
		for(auto& t : pumps)
			t.join();
	}
private:
	void Run()
	{
		std::error_code ec;
		acceptor.accept(a,ec);
		if(ec || stopping)
			return;
		if(!ConnectTo(io,b,target))
			return;
		const auto end = std::chrono::steady_clock::now()+delay;
		while(!stopping && std::chrono::steady_clock::now() < end)
			std::this_thread::sleep_for(5ms);
		if(stopping)
			return;
		pumps.emplace_back([this](){ Pump(a,b); });
		pumps.emplace_back([this](){ Pump(b,a); });
	}
	static void Pump(asio::ip::tcp::socket& from, asio::ip::tcp::socket& to)
	{
		char buf[16384];
		std::error_code ec;
		while(true)
		{
			auto n = from.read_some(asio::buffer(buf),ec);
			if(ec)
				break;
			asio::write(to,asio::buffer(buf,n),ec);
			if(ec)
				break;
		}
		to.shutdown(asio::ip::tcp::socket::shutdown_send,ec);
	}
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	uint16_t target;
	std::chrono::milliseconds delay;
	asio::ip::tcp::socket a;
	asio::ip::tcp::socket b;
	std::atomic<bool> stopping{false};
	std::thread thread;
	std::vector<std::thread> pumps;
};

//a server and a client object in one script, each reporting what it sees: <name>_open, <name>_failed, <name>_failed_detail, <name>_close
const char* PairScript = R"lua(
local C = odc.Config
local function watch(name, opts)
	opts.on_open = function(sock, peer)
		report(name.."_open", peer.subject.."|"..peer.protocol.."|"..tostring(peer.certificate))
		if name == "cli" and C.close_after_open ~= false then sock:close() end
	end
	opts.on_tls_failed = function(sock, reason, detail)
		report(name.."_failed", reason)
		report(name.."_failed_detail", detail)
	end
	opts.on_close = function(sock, reason, detail) report(name.."_close", reason) end
	return opts
end
function Enable()
	srv = assert(odc.TLSSocket(watch("srv", { host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, retry_ms = 50, tls = C.srv_tls })))
	assert(srv:open())
	cli = assert(odc.TLSSocket(watch("cli", { host = C.cli_host or "127.0.0.1", port = C.port, tls = C.cli_tls, peer_name = C.peer_name })))
	--(the client makes one attempt: it waits for the server to be listening)
	odc.msTimerCallback(200, function() assert(cli:open()) end)
end
)lua";

//what the two objects reported (reading a name that was not reported gives an empty string and does not add it)
struct Outcome
{
	std::map<std::string,std::string> m;
	std::string operator[](const std::string& k) const
	{
		auto it = m.find(k);
		return it == m.end() ? std::string() : it->second;
	}
	size_t count(const std::string& k) const
	{
		return m.count(k);
	}
};

//Waits for the client's connection to end (which it always does, once it came up) and the server's, then takes what both reported
Outcome WaitPair(LuaHarness& h)
{
	Outcome o;
	o.m["cli_close"] = h.Expect("cli_close",30s);
	std::string v;
	if(h.sink.Take("srv_close",v,5s))
		o.m["srv_close"] = v;
	for(const char* tag : {"cli_open","cli_failed","cli_failed_detail","srv_open","srv_failed","srv_failed_detail"})
		if(h.sink.Take(tag,v,0ms))
			o.m[tag] = v;
	return o;
}

Json::Value PairConf(uint16_t port, const Json::Value& srv_tls, const Json::Value& cli_tls)
{
	Json::Value conf;
	conf["port"] = port;
	conf["srv_tls"] = srv_tls;
	conf["cli_tls"] = cli_tls;
	return conf;
}
}

TEST_CASE(SUITE("A handshake with the peer information then an echo and a close"))
{
	auto& pki = GetPki();
	Json::Value conf = PairConf(20700,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientF,pki.caFile));
	LuaHarness h(R"lua(
local C = odc.Config
local order = {}
function Enable()
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, tls = C.srv_tls,
		on_open = function(sock, peer) report("srv_open", peer.subject.."|"..tostring(peer.certificate)..'|'..peer.serial) end,
		on_data = function(sock, d) sock:write(d) end,
		on_close = function(sock, reason) report("srv_close", reason) end })
	assert(srv:open())
	cli = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = C.cli_tls,
		on_open = function(sock, peer)
			order[#order+1] = "open"
			local p = sock:peer()
			report("cli_peer", peer.subject.."|"..peer.issuer.."|"..peer.serial.."|"..#peer.sha256.."|"..tostring(peer.certificate)..
				"|"..tostring(peer.resumed).."|"..tostring(peer.seconds_to_expiry > 0).."|"..tostring(p ~= nil and p.sha256 == peer.sha256))
			report("cli_proto", peer.protocol.."|"..#peer.cipher.."|"..#peer.not_before.."|"..#peer.not_after)
			report("cli_state", tostring(sock == cli)..","..tostring(sock:is_open())..","..sock:serial())
			assert(sock:write("hello"))
		end,
		on_data = function(sock, d)
			order[#order+1] = "data"
			report("cli_echo", d)
			sock:close()
			order[#order+1] = "after_close"
		end,
		on_tls_failed = function(sock, reason, detail) report("cli_failed", reason) end,
		on_close = function(sock, reason)
			order[#order+1] = "close"
			report("cli_close", reason..","..tostring(sock:is_open())..","..sock:serial())
			report("cli_order", table.concat(order, ","))
			report("cli_peer_after", tostring(sock:peer() ~= nil))
		end })
	odc.msTimerCallback(200, function() assert(cli:open()) end) --(one attempt: once the server listens)
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("cli_peer") == "CN=tls-server.test|CN=Test CA|64|64|true|false|true|true");
	const auto proto = h.Expect("cli_proto");
	CHECK((proto.rfind("TLSv1.3|",0) == 0 || proto.rfind("TLSv1.2|",0) == 0));
	CHECK(h.Expect("cli_state") == "true,true,1");
	CHECK(h.Expect("cli_echo") == "hello");
	CHECK(h.Expect("cli_close") == "closed,false,0");
	CHECK(h.Expect("cli_order") == "open,data,after_close,close"); //on_close after close() has returned
	CHECK(h.Expect("cli_peer_after") == "true");
	CHECK(h.Expect("srv_open") == "CN=tls-client.test|true|c8"); //mutual TLS: the server saw the client's certificate
	CHECK(h.Expect("srv_close") == "closed");
	CHECK(h.NoReport("cli_failed",100ms));
}

TEST_CASE(SUITE("A client without a certificate is rejected by a server that requires one"))
{
	auto& pki = GetPki();
	LuaHarness h(PairScript,PairConf(20701,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(nullptr,pki.caFile)));
	h.Enable();
	auto o = WaitPair(h);
	INFO("client: "+o["cli_close"]+" "+o["cli_failed"]+" "+o["cli_failed_detail"]+" / server: "+o["srv_close"]+" "+o["srv_failed"]+" "+o["srv_failed_detail"]);
	//the server refuses and says why; a TLS 1.3 client has completed its own handshake by then, so it sees on_open and then the connection end
	REQUIRE(o.count("srv_failed"));
	CHECK(o["srv_failed"] == "tls_failed");
	CHECK(o["srv_close"] == "tls_failed");
	CHECK(o["srv_failed_detail"].find("peer did not return a certificate") != std::string::npos);
	CHECK(o.count("cli_close") == 1);
}

TEST_CASE(SUITE("An expired client certificate is rejected"))
{
	auto& pki = GetPki();
	LuaHarness h(PairScript,PairConf(20702,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientExpiredF,pki.caFile)));
	h.Enable();
	auto o = WaitPair(h);
	INFO("client: "+o["cli_close"]+" "+o["cli_failed"]+" / server: "+o["srv_close"]+" "+o["srv_failed"]+" "+o["srv_failed_detail"]);
	REQUIRE(o.count("srv_failed"));
	CHECK(o["srv_failed"] == "tls_failed");
	CHECK(o["srv_close"] == "tls_failed");
	CHECK(o["srv_failed_detail"].find("expired") != std::string::npos);
	CHECK(o.count("cli_close") == 1);
}

TEST_CASE(SUITE("A server certificate from a CA the client does not trust"))
{
	auto& pki = GetPki();
	LuaHarness h(PairScript,PairConf(20703,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientF,pki.otherCaFile)));
	h.Enable();
	auto o = WaitPair(h);
	INFO("client: "+o["cli_close"]+" "+o["cli_failed"]+" "+o["cli_failed_detail"]+" / server: "+o["srv_close"]+" "+o["srv_failed"]);
	CHECK(o["cli_failed"] == "tls_failed");
	CHECK(o["cli_close"] == "tls_failed"); //on_tls_failed first, then on_close with the same reason
	CHECK(o["cli_failed_detail"].find("certificate verification failed") != std::string::npos);
	CHECK(o.count("cli_open") == 0); //the session was never established
	CHECK(o.count("srv_failed") == 1);
}

TEST_CASE(SUITE("A server certificate for another name: the host name is checked by default"))
{
	auto& pki = GetPki();
	LuaHarness h(PairScript,PairConf(20704,TlsOpt(&pki.serverOtherNameF,pki.caFile),TlsOpt(&pki.clientF,pki.caFile)));
	h.Enable();
	auto o = WaitPair(h);
	INFO("client: "+o["cli_close"]+" "+o["cli_failed"]+" "+o["cli_failed_detail"]);
	CHECK(o["cli_failed"] == "tls_failed");
	CHECK(o["cli_close"] == "tls_failed");
	CHECK(o["cli_failed_detail"].find("mismatch") != std::string::npos); //(the name to check is the IP address connected to)
	CHECK(o.count("cli_open") == 0);
}

TEST_CASE(SUITE("peer_name is the name checked against the certificate"))
{
	auto& pki = GetPki();
	//connecting by an address the certificate does not carry, with the name it does: allowed with peer_name; the host name check uses the other name
	Json::Value conf = PairConf(20705,TlsOpt(&pki.serverOtherNameF,pki.caFile),TlsOpt(&pki.clientF,pki.caFile));
	conf["peer_name"] = "other-name.test";
	LuaHarness h(PairScript,conf);
	h.Enable();
	auto o = WaitPair(h);
	INFO("client: "+o["cli_close"]+" "+o["cli_failed"]+" "+o["cli_failed_detail"]);
	CHECK(o.count("cli_failed") == 0);
	REQUIRE(o.count("cli_open"));
	CHECK(o["cli_open"].rfind("CN=other-name.test|",0) == 0);
	CHECK(o["cli_close"] == "closed");
}

TEST_CASE(SUITE("VerifyHostname = false connects to a certificate for another name and warns on every connect"))
{
	auto& pki = GetPki();
	auto cli_tls = TlsOpt(&pki.clientF,pki.caFile);
	cli_tls["VerifyHostname"] = false;
	Json::Value conf = PairConf(20706,TlsOpt(&pki.serverOtherNameF,pki.caFile),cli_tls);
	conf["close_after_open"] = true;
	LuaHarness h(PairScript,conf);
	h.Enable();
	auto o = WaitPair(h);
	CHECK(o.count("cli_failed") == 0);
	CHECK(o.count("cli_open") == 1);
	CHECK(o["cli_close"] == "closed");
	CHECK(h.capture->Has("TLSSocket 127.0.0.1:20706: the server's name is not verified against its certificate (VerifyHostname is false)."));
}

TEST_CASE(SUITE("VerifyPeer = false connects to a server of an unknown CA and warns"))
{
	auto& pki = GetPki();
	auto cli_tls = TlsOpt(&pki.clientF,pki.otherCaFile);
	cli_tls["VerifyPeer"] = false;
	cli_tls["VerifyHostname"] = false;
	LuaHarness h(PairScript,PairConf(20707,TlsOpt(&pki.serverF,pki.caFile),cli_tls));
	h.Enable();
	auto o = WaitPair(h);
	CHECK(o.count("cli_failed") == 0);
	CHECK(o.count("cli_open") == 1);
	CHECK(h.capture->Has("the peer's certificate is not verified (VerifyPeer is false)."));
	CHECK(h.capture->Has("the server's name is not verified against its certificate (VerifyHostname is false)."));
}

TEST_CASE(SUITE("Invalid options and TLS configuration problems are reported by the constructor"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["ca"] = pki.caFile;
	conf["cert"] = pki.serverF.cert;
	conf["key"] = pki.serverF.key;
	conf["enc_cert"] = pki.serverEncryptedF.cert;
	conf["enc_key"] = pki.serverEncryptedF.key;
	conf["missing"] = pki.dir.Path("does-not-exist.pem");
	conf["not_a_key"] = pki.dir.Write("not-a-key.pem","this is not a key\n");
	conf["garbage_ca"] = pki.dir.Write("garbage-ca.pem","this is not a certificate\n");
	SetEnv("ODC_TEST_KEY_PW_WRONG","wrong-battery");
	SetEnv("ODC_TEST_KEY_PW_RIGHT","correct-horse");
	LuaHarness h(R"lua(
local C = odc.Config
local H, P = "127.0.0.1", 7000
local function tls(t) t.CAFile = t.CAFile or C.ca; return t end
local function id(t) t.CertFile = C.cert; t.KeyFile = C.key; return tls(t) end
local cases =
{
	{ "none",         function() return odc.TLSSocket() end },
	{ "no_tls_table", function() return odc.TLSSocket{ host = H, port = P } end },
	{ "tls_not_table",function() return odc.TLSSocket{ host = H, port = P, tls = "x" } end },
	{ "tls_list",     function() return odc.TLSSocket{ host = H, port = P, tls = { "a" } } end },
	{ "peer_name_num",function() return odc.TLSSocket{ host = H, port = P, tls = tls{}, peer_name = 5 } end },
	{ "tcp_peer_name",function() return odc.TCPSocket{ host = H, port = P, peer_name = "x" } end },
	{ "tcp_tls",      function() return odc.TCPSocket{ host = H, port = P, tls = {} } end },
	{ "typo",         function() return odc.TLSSocket{ host = H, port = P, tls = tls{}, auto_reoppen = true } end },
	{ "ok_client",    function() return odc.TLSSocket{ host = H, port = P, tls = tls{} } end },
	{ "ok_mutual",    function() return odc.TLSSocket{ host = H, port = P, tls = id{} } end },
	{ "ok_server",    function() return odc.TLSSocket{ host = H, port = P, server = true, tls = id{} } end },
	{ "server_nocert",function() return odc.TLSSocket{ host = H, port = P, server = true, tls = tls{} } end },
	{ "keylogfile",   function() return odc.TLSSocket{ host = H, port = P, tls = tls{ KeyLogFile = "keys.log" } } end },
	{ "keylogfile_lc",function() return odc.TLSSocket{ host = H, port = P, tls = tls{ keylogfile = "keys.log" } } end },
	{ "keypassword",  function() return odc.TLSSocket{ host = H, port = P, server = true, tls = id{ KeyPassword = "hunter2" } } end },
	{ "unknown_key",  function() return odc.TLSSocket{ host = H, port = P, tls = tls{ Bogus = true } } end },
	{ "minversion_n", function() return odc.TLSSocket{ host = H, port = P, tls = tls{ MinVersion = 1.2 } } end },
	{ "minversion_s", function() return odc.TLSSocket{ host = H, port = P, tls = tls{ MinVersion = "1.2", MaxVersion = "1.3" } } end },
	{ "version_order",function() return odc.TLSSocket{ host = H, port = P, tls = tls{ MinVersion = "1.3", MaxVersion = "1.2" } } end },
	{ "timeout_text", function() return odc.TLSSocket{ host = H, port = P, tls = tls{ HandshakeTimeoutms = "soon" } } end },
	{ "allow_list",   function() return odc.TLSSocket{ host = H, port = P, tls = tls{ PeerCertAllowList = { "CN:tls-server.test" } } } end },
	{ "allow_bad",    function() return odc.TLSSocket{ host = H, port = P, tls = tls{ PeerCertAllowList = { "NOPE" } } } end },
	{ "function_val", function() return odc.TLSSocket{ host = H, port = P, tls = tls{ Ciphers = print } } end },
	{ "missing_ca",   function() return odc.TLSSocket{ host = H, port = P, tls = { CAFile = C.missing } } end },
	{ "garbage_ca",   function() return odc.TLSSocket{ host = H, port = P, tls = { CAFile = C.garbage_ca } } end },
	{ "missing_cert", function() return odc.TLSSocket{ host = H, port = P, tls = { CAFile = C.ca, CertFile = C.missing, KeyFile = C.key } } end },
	{ "bad_key",      function() return odc.TLSSocket{ host = H, port = P, tls = { CAFile = C.ca, CertFile = C.cert, KeyFile = C.not_a_key } } end },
	{ "key_pw_right", function() return odc.TLSSocket{ host = H, port = P, server = true, tls = { CAFile = C.ca, CertFile = C.enc_cert, KeyFile = C.enc_key, KeyPasswordEnv = "ODC_TEST_KEY_PW_RIGHT" } } end },
	{ "key_pw_wrong", function() return odc.TLSSocket{ host = H, port = P, server = true, tls = { CAFile = C.ca, CertFile = C.enc_cert, KeyFile = C.enc_key, KeyPasswordEnv = "ODC_TEST_KEY_PW_WRONG" } } end },
	{ "key_pw_unset", function() return odc.TLSSocket{ host = H, port = P, server = true, tls = { CAFile = C.ca, CertFile = C.enc_cert, KeyFile = C.enc_key, KeyPasswordEnv = "ODC_TEST_KEY_PW_UNSET" } } end },
}
for _, c in ipairs(cases) do
	local obj, err = c[2]()
	report("case_"..c[1], obj and "object" or tostring(err))
end
report("done", "")
)lua",conf);
	h.Enable();
	h.Expect("done");
	std::map<std::string,std::string> r;
	for(const auto& e : h.sink.All())
	{
		const auto tab = e.find('\t');
		r[e.substr(0,tab)] = e.substr(tab+1);
	}
	auto Is = [&](const std::string& name, const std::string& value)
		    {
			    INFO(name+" = "+r["case_"+name]);
			    CHECK(r["case_"+name] == value);
		    };
	auto Has = [&](const std::string& name, const std::string& text)
		     {
			     INFO(name+" = "+r["case_"+name]);
			     CHECK(r["case_"+name].find(text) != std::string::npos);
		     };
	Is("none","invalid_options: options");
	Is("no_tls_table","invalid_options: tls");
	Is("tls_not_table","invalid_options: tls");
	Is("tls_list","invalid_options: tls");
	Is("peer_name_num","invalid_options: peer_name");
	Is("tcp_peer_name","invalid_options: peer_name");
	Is("tcp_tls","invalid_options: tls");
	Is("typo","invalid_options: auto_reoppen");
	Is("ok_client","object");
	Is("ok_mutual","object");
	Is("ok_server","object");
	Has("server_nocert","tls config: ");
	Has("server_nocert","CertFile");
	Is("keylogfile","invalid_options: tls.KeyLogFile");
	Is("keylogfile_lc","invalid_options: tls.KeyLogFile");
	Has("keypassword","tls config: ");
	Has("keypassword","KeyPasswordEnv");
	CHECK(r["case_keypassword"].find("hunter2") == std::string::npos); //the secret is not echoed
	Has("unknown_key","tls config: ");
	Has("unknown_key","Bogus");
	Has("minversion_n","tls config: ");
	Has("minversion_n","MinVersion");
	Is("minversion_s","object");
	Has("version_order","MinVersion");
	Has("timeout_text","HandshakeTimeoutms");
	Is("allow_list","object");
	Has("allow_bad","tls config: ");
	Has("function_val","tls config: ");
	Has("missing_ca","CAFile");
	Has("garbage_ca","CAFile");
	Has("missing_cert","CertFile");
	Has("bad_key","KeyFile");
	Is("key_pw_right","object");
	CHECK(r["case_key_pw_wrong"] != "object");
	CHECK(r["case_key_pw_wrong"].find("wrong-battery") == std::string::npos);
	CHECK(r["case_key_pw_wrong"].find("correct-horse") == std::string::npos);
	CHECK(r["case_key_pw_unset"] != "object");
	Has("key_pw_unset","KeyPasswordEnv");
}

TEST_CASE(SUITE("A server with a password protected key (KeyPasswordEnv) serves a client"))
{
	auto& pki = GetPki();
	SetEnv("ODC_TEST_KEY_PW_RIGHT","correct-horse");
	auto srv_tls = TlsOpt(&pki.serverEncryptedF,pki.caFile);
	srv_tls["KeyPasswordEnv"] = "ODC_TEST_KEY_PW_RIGHT";
	LuaHarness h(PairScript,PairConf(20708,srv_tls,TlsOpt(&pki.clientF,pki.caFile)));
	h.Enable();
	auto o = WaitPair(h);
	CHECK(o.count("cli_failed") == 0);
	REQUIRE(o.count("cli_open"));
	CHECK(o["cli_close"] == "closed");
}

TEST_CASE(SUITE("A handshake that never completes ends with HandshakeTimeoutms"))
{
	auto& pki = GetPki();
	Peer silent(20709,[&](Peer::Socket& s, int)
		{
			std::string r;
			Peer::ReadAll(s,r); //takes the hello and says nothing
		});
	auto tls = TlsOpt(&pki.clientF,pki.caFile);
	tls["HandshakeTimeoutms"] = 300;
	Json::Value conf;
	conf["port"] = 20709;
	conf["tls"] = tls;
	LuaHarness h(R"lua(
function Enable()
	local t0 = odc.msSinceEpoch()
	s = assert(odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls,
		on_open = function() report("open", "") end,
		on_tls_failed = function(sock, reason, detail) report("failed", reason..","..(odc.msSinceEpoch()-t0)) end,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail ~= nil)) end })
	assert(s:open())
	odc.msTimerCallback(50, function()
		local ok, err = s:write("early")
		report("early", tostring(ok)..","..tostring(err)..","..tostring(s:is_open())..","..s:serial())
	end)
end
)lua",conf);
	h.Enable();
	auto failed = h.Expect("failed",30s);
	CHECK(failed.rfind("tls_failed,",0) == 0);
	CHECK(std::stoi(failed.substr(11)) >= 250);
	CHECK(std::stoi(failed.substr(11)) < 5000);
	CHECK(h.Expect("close") == "tls_failed,true");
	CHECK(h.Expect("early") == "true,nil,false,1"); //the TCP connection is up and takes writes, which wait for the handshake
	CHECK(h.NoReport("open",50ms));
}

TEST_CASE(SUITE("close_notify: the peer sees the session closed and the script sees the peer's"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20710;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	s = assert(odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls, auto_reopen = true, retry_ms = 50,
		on_open = function(sock, peer) report("open", sock:serial()); sock:write("goodbye") end,
		on_data = function(sock, d) report("data", d) end,
		on_tls_failed = function(sock, reason, detail) report("failed", reason) end,
		on_close = function(sock, reason, detail) report("close", sock:serial()..","..reason) end })
	assert(s:open())
end
function Event(e, sender, cb)
	if e.Payload == "close" then s:close() end
end
)lua",conf);
	TlsPeer server(true,20710,MakeContext(&pki.serverF,pki.caFile,odc::tls::Role::Server),true);
	server.Open();

	h.Enable();
	CHECK(h.Expect("open") == "1");
	REQUIRE(WaitFor([&](){ return server.Received() == "goodbye"; }));
	//the script closes: the peer gets close_notify, with what was written before it
	h.Send("close");
	CHECK(h.Expect("close") == "0,closed");
	REQUIRE(WaitFor([&](){ return !server.Ends().empty(); }));
	CHECK(server.Ends().front() == "PeerClosed");
	CHECK(h.NoReport("failed",100ms));
}

TEST_CASE(SUITE("A peer that closes the session is a close and not a failure"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20711;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	s = assert(odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls,
		on_open = function(sock, peer) report("open", "") end,
		on_tls_failed = function(sock, reason, detail) report("failed", reason) end,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail)) end })
	assert(s:open())
end
)lua",conf);
	TlsPeer server(true,20711,MakeContext(&pki.serverF,pki.caFile,odc::tls::Role::Server),false);
	server.Open();

	h.Enable();
	h.Expect("open");
	server.mgr->Close(); //close_notify, then the TCP connection
	CHECK(h.Expect("close") == "closed,nil");
	CHECK(h.NoReport("failed",100ms));
}

TEST_CASE(SUITE("A write made before the handshake completes waits for it; one after a failure is refused"))
{
	auto& pki = GetPki();
	//a Lua server behind a relay that holds the client's bytes back, so that the TCP connection is up well before the handshake
	DelayRelay relay(20712,20713,500ms);
	auto cli_tls = TlsOpt(&pki.clientF,pki.caFile);
	Json::Value conf = PairConf(20713,TlsOpt(&pki.serverF,pki.caFile),cli_tls);
	conf["relay_port"] = 20712;
	LuaHarness h(R"lua(
local C = odc.Config
function Enable()
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, tls = C.srv_tls,
		on_data = function(sock, d) sock:write(d) end })
	assert(srv:open())
	cli = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.relay_port, tls = C.cli_tls,
		on_open = function(sock, peer) report("open", "") end,
		on_data = function(sock, d) report("echo", d); sock:close() end,
		on_close = function(sock, reason) report("close", reason) end })
	odc.msTimerCallback(200, function() assert(cli:open()) end) --(one attempt: once the server listens)
	local function early()
		if cli:serial() ~= 0 then
			local ok, err = cli:write("early bird")
			report("early", tostring(ok)..","..tostring(err)..","..tostring(cli:is_open()))
		else
			odc.msTimerCallback(5, early)
		end
	end
	early()
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("early") == "true,nil,false"); //accepted, with the handshake still to do
	CHECK(h.Expect("open") != "x");
	CHECK(h.Expect("echo") == "early bird"); //and sent when it completed
	CHECK(h.Expect("close") == "closed");
}

TEST_CASE(SUITE("A write after a failed handshake is refused and nothing is carried to the next connection"))
{
	auto& pki = GetPki();
	Json::Value conf = PairConf(20714,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientF,pki.otherCaFile));
	LuaHarness h(R"lua(
local C = odc.Config
function Enable()
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, retry_ms = 50, tls = C.srv_tls,
		on_open = function() report("srv_open", "") end,
		on_data = function(sock, d) report("srv_data", d) end })
	assert(srv:open())
	cli = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = C.cli_tls,
		on_tls_failed = function(sock, reason) 
			local ok, err = sock:write("too late")
			report("after_failure", reason..","..tostring(ok)..","..tostring(err))
		end,
		on_close = function(sock, reason)
			local ok, err = sock:write("after close")
			report("after_close", reason..","..tostring(ok)..","..tostring(err))
		end })
	odc.msTimerCallback(200, function() assert(cli:open()) end) --(one attempt: once the server listens)
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("after_failure") == "tls_failed,nil,closed");
	CHECK(h.Expect("after_close") == "tls_failed,nil,closed");
	CHECK(h.NoReport("srv_data",500ms));
	CHECK(h.NoReport("srv_open",10ms));
}

TEST_CASE(SUITE("A large write arrives whole after close()"))
{
	auto& pki = GetPki();
	Json::Value conf = PairConf(20715,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientF,pki.caFile));
	LuaHarness h(R"lua(
local C = odc.Config
local N = 4*1024*1024
function Enable()
	local pat = {}
	for j = 0, 250 do pat[#pat+1] = string.char(j) end
	local expected = table.concat(pat):rep(N // 251 + 1):sub(1, N)
	local parts, total = {}, 0
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, tls = C.srv_tls,
		on_data = function(sock, d) parts[#parts+1] = d; total = total + #d end,
		on_close = function(sock, reason) report("srv_result", tostring(table.concat(parts) == expected)..","..total..","..reason) end })
	assert(srv:open())
	cli = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = C.cli_tls,
		on_open = function(sock, peer)
			assert(sock:write(expected))
			sock:close()
		end,
		on_close = function(sock, reason) report("cli_close", reason) end })
	odc.msTimerCallback(200, function() assert(cli:open()) end) --(one attempt: once the server listens)
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("cli_close") == "closed");
	CHECK(h.Expect("srv_result") == "true,4194304,closed");
}

TEST_CASE(SUITE("idle_timeout_ms closes an idle session with close_notify"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20724;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	local t0 = odc.msSinceEpoch()
	s = assert(odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls, idle_timeout_ms = 300,
		on_open = function(sock, peer) report("open", "") end,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail)..","..(odc.msSinceEpoch()-t0)) end })
	assert(s:open())
end
)lua",conf);
	TlsPeer server(true,20724,MakeContext(&pki.serverF,pki.caFile,odc::tls::Role::Server),false);
	server.Open();
	h.Enable();
	h.Expect("open");
	auto parts = h.Expect("close");
	CHECK(parts.rfind("timeout,idle_timeout_ms,",0) == 0);
	CHECK(std::stoi(parts.substr(24)) >= 290);
	REQUIRE(WaitFor([&](){ return !server.Ends().empty(); }));
	CHECK(server.Ends().front() == "PeerClosed");
}

TEST_CASE(SUITE("connect_timeout_ms ends a TCP connect attempt that does not complete"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	s = assert(odc.TLSSocket{ host = "192.0.2.1", port = 9, tls = odc.Config.tls, connect_timeout_ms = 300,
		on_open = function() report("open", "") end,
		on_tls_failed = function() report("tls_failed", "") end,
		on_close = function(sock, reason, detail) report("close", reason..","..tostring(detail)) end })
	assert(s:open())
end
)lua",conf);
	h.Enable();
	auto close = h.Expect("close",30s);
	if(close.rfind("timeout,",0) == 0)
		CHECK(close == "timeout,connect_timeout_ms");
	else
		WARN("This host has no route to the documentation network, so the connect failed at once with '"+close+"': the timeout is not exercised.");
	CHECK(h.NoReport("tls_failed",100ms));
	CHECK(h.NoReport("open",10ms));
}

TEST_CASE(SUITE("The server takes one connection at a time"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20716;
	conf["tls"] = TlsOpt(&pki.serverF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	s = assert(odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, server = true, auto_reopen = true, retry_ms = 50, tls = odc.Config.tls,
		on_open = function(sock, peer) report("open", sock:serial()..","..peer.subject) end,
		on_data = function(sock, d) sock:write(d) end,
		on_close = function(sock, reason) report("close", sock:serial()..","..reason) end })
	assert(s:open())
end
)lua",conf);
	h.Enable();
	std::this_thread::sleep_for(300ms); //(the client makes one attempt: once the server listens)
	TlsPeer first(false,20716,MakeContext(&pki.clientF,pki.caFile,odc::tls::Role::Client),false);
	first.Open();
	CHECK(h.Expect("open") == "1,CN=tls-client.test");
	REQUIRE(WaitFor([&](){ return first.Established() == 1; }));
	//while it is up, nobody is listening
	asio::io_context io;
	asio::ip::tcp::socket c2(io);
	std::error_code ec;
	c2.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),20716),ec);
	CHECK(ec);
	first.mgr->Close();
	CHECK(h.Expect("close") == "0,closed");
	TlsPeer second(false,20716,MakeContext(&pki.clientF,pki.caFile,odc::tls::Role::Client),true);
	second.Open();
	CHECK(h.Expect("open") == "2,CN=tls-client.test");
	REQUIRE(WaitFor([&](){ return second.Established() == 1; }));
}

TEST_CASE(SUITE("A renewed certificate is used by the next connection"))
{
	auto& pki = GetPki();
	tlstest::TempDir dir;
	Files files = {dir.Write("server.crt",pki.server.certPem),dir.Write("server.key",pki.server.keyPem)};
	Json::Value conf = PairConf(20717,TlsOpt(&files,pki.caFile),TlsOpt(&pki.clientF,pki.caFile));
	LuaHarness h(R"lua(
local C = odc.Config
local function connect(name)
	local o = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = C.cli_tls,
		on_open = function(sock, peer) report(name, peer.serial); sock:close() end,
		on_tls_failed = function(sock, reason, detail) report(name.."_failed", reason) end })
	assert(o:open())
	return o
end
function Enable()
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, retry_ms = 50, tls = C.srv_tls })
	assert(srv:open())
	odc.msTimerCallback(200, function() first = connect("first") end) --(one attempt: once the server listens)
end
function Event(e, sender, cb)
	if e.Payload == "again" then second = connect("second") end
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("first") == "64"); //serial 100
	//renewed certificate and key, as a deployment would write them
	const std::string cert_path = files.cert;
	const std::string key_path = files.key;
	for(const auto& f : {std::make_pair(cert_path,pki.serverRenewed.certPem),std::make_pair(key_path,pki.serverRenewed.keyPem)})
	{
		{
			std::ofstream out(f.first,std::ios::binary | std::ios::trunc);
			out << f.second;
		}
		std::error_code ec;
		std::filesystem::last_write_time(f.first,std::filesystem::file_time_type::clock::now()+std::chrono::seconds(10),ec);
	}
	std::this_thread::sleep_for(300ms); //(the client makes one attempt: once the server listens again)
	h.Send("again");
	CHECK(h.Expect("second") == "65"); //serial 101: no restart
	CHECK(h.NoReport("second_failed",50ms));
}

TEST_CASE(SUITE("An error raised in each kind of callback is logged and does not stop the others"))
{
	auto& pki = GetPki();
	Json::Value conf = PairConf(20718,TlsOpt(&pki.serverF,pki.caFile),TlsOpt(&pki.clientF,pki.caFile));
	conf["bad_ca"] = pki.otherCaFile;
	conf["refused_port"] = 20719;
	LuaHarness h(R"lua(
local C = odc.Config
function Enable()
	srv = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, server = true, auto_reopen = true, retry_ms = 50, tls = C.srv_tls,
		on_open = function(sock, peer) sock:write("data please") end,
		on_data = function(sock, d) sock:write(d) end })
	assert(srv:open())
	cli = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = C.cli_tls,
		on_open = function(sock, peer) report("open", ""); error("open boom") end,
		on_data = function(sock, d)
			report("data", d)
			sock:close()
			error("data boom")
		end,
		on_close = function(sock, reason)
			report("close", reason)
			--a failed handshake (the server takes one connection at a time, so after this one): on_tls_failed raising an error does not stop on_close
			bad = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.port, tls = { CAFile = C.bad_ca },
				on_tls_failed = function(sock, reason) report("failed", reason); error("failed boom") end,
				on_close = function(sock, reason) report("bad_close", reason) end })
			odc.msTimerCallback(200, function() assert(bad:open()) end) --(one attempt: once the server listens again)
			error("close boom")
		end })
	odc.msTimerCallback(200, function() assert(cli:open()) end) --(one attempt: once the server listens)

	--a connection that doesn't come up in time: on_error raising an error does not stop the retries
	local errors = 0
	retry = assert(odc.TLSSocket{ host = "127.0.0.1", port = C.refused_port, tls = { CAFile = C.bad_ca }, auto_reopen = true, retry_ms = 50, connect_timeout_ms = 100,
		on_error = function(sock, reason)
			errors = errors + 1
			if errors == 3 then report("errors_continue", reason); sock:close() end
			error("error boom")
		end })
	assert(retry:open())
end
)lua",conf);
	h.Enable();
	CHECK(h.Expect("open") == "");
	CHECK(h.Expect("data") == "data please");
	CHECK(h.Expect("close") == "closed");
	CHECK(h.Expect("failed") == "tls_failed");
	CHECK(h.Expect("bad_close") == "tls_failed");
	CHECK(h.Expect("errors_continue") == "timeout");
	REQUIRE(WaitFor([&](){ return h.capture->Has("data boom") && h.capture->Has("close boom") && h.capture->Has("failed boom") && h.capture->Has("error boom"); }));
	CHECK(h.capture->Has("open boom"));
	CHECK(h.capture->Has("PortUnderTest: Error calling TLSSocket 127.0.0.1:20718 on_open callback"));
	CHECK(h.capture->Has("on_tls_failed callback"));
}

TEST_CASE(SUITE("A refused connection and a failed lookup end with the connect timeout, as for TCPSocket"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20720;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	local function failing(name, opts)
		opts.tls = odc.Config.tls
		opts.on_open = function() report(name.."_open", "") end
		opts.on_tls_failed = function() report(name.."_tls_failed", "") end
		opts.on_close = function(sock, reason, detail) report(name, reason..","..tostring(detail)) end
		assert(assert(odc.TLSSocket(opts)):open())
	end
	failing("refused", { host = "127.0.0.1", port = odc.Config.port, connect_timeout_ms = 300 })
	failing("resolve", { host = "no-such-host.invalid", port = odc.Config.port, connect_timeout_ms = 300 })
end
)lua",conf);
	h.Enable();
	//(a connection that never came up has no TLS session: no on_tls_failed; the attempt ends at the connect timeout, as for TCPSocket without auto_reopen)
	CHECK(h.Expect("refused") == "timeout,connect_timeout_ms");
	CHECK(h.Expect("resolve",60s) == "timeout,connect_timeout_ms");
	CHECK(h.NoReport("refused_tls_failed",100ms));
	CHECK(h.NoReport("refused_open",10ms));
}

TEST_CASE(SUITE("A script that closes its TLS sockets in Disable(): close_notify to the peer and no callbacks afterwards"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20721;
	conf["server_port"] = 20722;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	conf["srv_tls"] = TlsOpt(&pki.serverF,pki.caFile);
	LuaHarness h(R"lua(
local disabled = false
local function watch(name, opts)
	opts.on_open = function(sock, peer) if disabled then report("late", name.." open") else report(name.."_open", "") end end
	opts.on_data = function(sock, d) if disabled then report("late", name.." data") end end
	--(the on_close that answers the script's own close() is not late)
	opts.on_error = function(sock, r) if disabled then report("late", name.." error") end end
	opts.on_tls_failed = function(sock, r) if disabled then report("late", name.." failed") end end
	return assert(odc.TLSSocket(opts))
end
function Enable()
	disabled = false
	c = watch("client", { host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls, auto_reopen = true, retry_ms = 50 })
	srv = watch("server", { host = "127.0.0.1", port = odc.Config.server_port, server = true, tls = odc.Config.srv_tls, auto_reopen = true })
	c:open()
	srv:open()
end
function Disable()
	disabled = true
	c:close()
	srv:close()
	c, srv = nil, nil --the script lets go of them: they are collected once closed
	report("disabled", "")
end
)lua",conf);
	TlsPeer peer(true,20721,MakeContext(&pki.serverF,pki.caFile,odc::tls::Role::Server),true);
	peer.Open();

	h.Enable();
	std::this_thread::sleep_for(300ms); //(the client makes one attempt: once the server listens)
	TlsPeer client_of_script(false,20722,MakeContext(&pki.clientF,pki.caFile,odc::tls::Role::Client),false);
	client_of_script.Open();
	h.Expect("client_open");
	h.Expect("server_open");
	h.Disable();
	h.Expect("disabled");
	//both peers see the sessions closed properly
	REQUIRE(WaitFor([&](){ return !peer.Ends().empty() && !client_of_script.Ends().empty(); }));
	CHECK(peer.Ends().front() == "PeerClosed");
	CHECK(client_of_script.Ends().front() == "PeerClosed");
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
	CHECK(h.NoReport("late",800ms));
}

TEST_CASE(SUITE("Collecting the last reference to an open TLS object does not close it"))
{
	auto& pki = GetPki();
	Json::Value conf;
	conf["port"] = 20723;
	conf["tls"] = TlsOpt(&pki.clientF,pki.caFile);
	LuaHarness h(R"lua(
function Enable()
	do
		local tmp = odc.TLSSocket{ host = "127.0.0.1", port = odc.Config.port, tls = odc.Config.tls,
			on_open = function(sock, peer) report("open", "") end,
			on_data = function(sock, d) report("data", d..","..tostring(sock:is_open())); sock:close() end,
			on_close = function(sock, reason) report("close", reason) end }
		tmp:open()
	end
end
function Event(e, sender, cb)
	collectgarbage()
	collectgarbage()
	report("collected", "")
end
)lua",conf);
	TlsPeer peer(true,20723,MakeContext(&pki.serverF,pki.caFile,odc::tls::Role::Server),false);
	peer.Open();

	h.Enable();
	h.Expect("open");
	h.Send("gc");
	h.Expect("collected");
	CHECK(NetObjectCount() == 1);
	std::string sent = "still here";
	peer.mgr->Write(sent);
	CHECK(h.Expect("data") == "still here,true");
	CHECK(h.Expect("close") == "closed");
	h.Send("gc");
	h.Expect("collected");
	CHECK(WaitFor([](){ return NetObjectCount() == 0; }));
}

TEST_CASE(SUITE("The example in the network objects documentation runs"))
{
	auto& pki = GetPki();
	std::ifstream readme(ODC_NETWORK_README);
	REQUIRE_FALSE(readme.fail());
	std::stringstream ss;
	ss << readme.rdbuf();
	const std::string text = ss.str();
	const auto marker = text.find("<!-- example:tls -->");
	REQUIRE(marker != std::string::npos);
	const auto begin = text.find("```lua",marker);
	REQUIRE(begin != std::string::npos);
	const auto code_begin = text.find('\n',begin)+1;
	const auto code_end = text.find("```",code_begin);
	REQUIRE(code_end != std::string::npos);

	Json::Value conf;
	conf["Port"] = 20790;
	conf["CAFile"] = pki.caFile;
	conf["ServerCert"] = pki.serverF.cert;
	conf["ServerKey"] = pki.serverF.key;
	conf["ClientCert"] = pki.clientF.cert;
	conf["ClientKey"] = pki.clientF.key;
	LuaHarness h(text.substr(code_begin,code_end-code_begin),conf);
	h.Enable();
	REQUIRE(WaitFor([&](){ return h.capture->Has("client closed: closed"); }));
	CHECK(h.capture->Has("PortUnderTest: client connected to CN=tls-server.test"));
	CHECK(h.capture->Has("PortUnderTest: client received: hello"));
	h.Disable();
}

#endif //ODC_ASIO_SSL
