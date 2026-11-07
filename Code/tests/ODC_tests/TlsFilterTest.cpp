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
 * TlsFilterTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The TLS filter (TLSFilter.h) with two filters wired back to back in memory, over certificates made at run time.
 *  Only with ODC_ASIO_SSL.
 */

#ifdef ODC_ASIO_SSL

#include "TlsTestPki.h"
#include <algorithm>
#include <catch.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <opendatacon/TLSFilter.h>
#include <random>
#include <thread>

#define SUITE(name) "TLS filter - " name

using namespace odc::tls;

namespace
{
struct Files
{
	std::string cert;
	std::string key;
};

//the throw-away PKI of these tests (made once)
struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa, server, client, clientOtherCa, clientExpired, clientNotYet, clientRevoked, clientSoon, serverOtherCa, clientEncrypted, serverNoSan;
	Files serverF, clientF, clientOtherCaF, clientExpiredF, clientNotYetF, clientRevokedF, clientSoonF, serverOtherCaF, clientEncryptedF, serverNoSanF;
	std::string caFile, otherCaFile, crlRevokedFile, crlCleanFile, crlOtherCaFile, crlExpiredFile, garbageFile;

	Files Save(const std::string& name, const tlstest::Cert& c)
	{
		return {dir.Write(name + ".crt", c.certPem), dir.Write(name + ".key", c.keyPem)};
	}

	Pki()
	{
		ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
		otherCa = tlstest::MakeCert({"Other Test CA", {}, -3600, 86400LL * 3650, true, 1});
		server = tlstest::MakeCert({"tls-server.test", {"DNS:tls-server.test", "IP:127.0.0.1"}, -3600, 86400LL * 365, false, 100}, &ca);
		serverNoSan = tlstest::MakeCert({"tls-nosan.test", {}, -3600, 86400LL * 365, false, 101}, &ca);
		client = tlstest::MakeCert({"tls-client.test", {"DNS:tls-client.test"}, -3600, 86400LL * 365, false, 200}, &ca);
		clientOtherCa = tlstest::MakeCert({"tls-client.test", {}, -3600, 86400LL * 365, false, 201}, &otherCa);
		clientExpired = tlstest::MakeCert({"tls-expired.test", {}, -86400LL * 30, -86400, false, 202}, &ca);
		clientNotYet = tlstest::MakeCert({"tls-future.test", {}, 86400, 86400LL * 30, false, 203}, &ca);
		clientRevoked = tlstest::MakeCert({"tls-revoked.test", {}, -3600, 86400LL * 365, false, 204}, &ca);
		clientSoon = tlstest::MakeCert({"tls-soon.test", {}, -3600, 86400LL * 10, false, 205}, &ca);
		serverOtherCa = tlstest::MakeCert({"tls-server.test", {"DNS:tls-server.test"}, -3600, 86400LL * 365, false, 102}, &otherCa);
		clientEncrypted = tlstest::MakeCert({"tls-enc.test", {}, -3600, 86400LL * 365, false, 206, "synthetic-pass-1"}, &ca);
		serverF = Save("server", server);
		serverNoSanF = Save("servernosan", serverNoSan);
		clientF = Save("client", client);
		clientOtherCaF = Save("clientother", clientOtherCa);
		clientExpiredF = Save("clientexpired", clientExpired);
		clientNotYetF = Save("clientnotyet", clientNotYet);
		clientRevokedF = Save("clientrevoked", clientRevoked);
		clientSoonF = Save("clientsoon", clientSoon);
		serverOtherCaF = Save("serverother", serverOtherCa);
		clientEncryptedF = Save("clientenc", clientEncrypted);
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
		crlRevokedFile = dir.Write("revoked.crl", tlstest::MakeCrl(ca, {204}));
		crlCleanFile = dir.Write("clean.crl", tlstest::MakeCrl(ca, {}));
		crlOtherCaFile = dir.Write("otherca.crl", tlstest::MakeCrl(otherCa, {}));
		crlExpiredFile = dir.Write("expired.crl", tlstest::MakeCrl(ca, {}, -3600));
		garbageFile = dir.Write("garbage.pem", "this is not a PEM file\n");
	}
};

Pki& GetPki()
{
	static Pki pki;
	return pki;
}

//the configuration of a TLS server and a TLS client that trust the test CA and present the given certificates
TlsConf ServerConf(const Files& id)
{
	TlsConf c;
	c.enabled = true;
	c.certFile = id.cert;
	c.keyFile = id.key;
	c.caFile = GetPki().caFile;
	c.sessionResumption = false;
	return c;
}
TlsConf ClientConf(const Files* id)
{
	TlsConf c;
	c.enabled = true;
	if(id)
	{
		c.certFile = id->cert;
		c.keyFile = id->key;
	}
	c.caFile = GetPki().caFile;
	c.sessionResumption = false;
	return c;
}

std::shared_ptr<Context> MakeContext(const TlsConf& conf, Role role)
{
	std::string error;
	auto ctx = Context::Create(conf, role, error);
	INFO("Context::Create: " << error);
	REQUIRE(ctx);
	return ctx;
}

struct Endpoint
{
	std::unique_ptr<Filter> filter;
	Bytes plain;           //application bytes received
	std::deque<Bytes> out; //records produced, not yet delivered
	std::optional<PeerInfo> info;
	std::vector<std::string> failures;
	std::vector<std::string> logs;
	int peerClosed = 0;
	int established = 0;
	size_t bytesOut = 0;
	std::function<void(const uint8_t*, size_t)> onPlainExtra;
};

//two filters connected back to back, with a clock of their own
struct Link
{
	Clock::time_point now{};
	Endpoint c, s;
	std::function<void(bool toServer, Bytes&)> mutate; //applied to each record on its way
	size_t delivered = 0;

	static FilterCallbacks Callbacks(Endpoint& e)
	{
		FilterCallbacks cb;
		cb.send = [&e](const Bytes& b)
			    {
				    e.bytesOut += b.size();
				    e.out.push_back(b);
			    };
		cb.onPlain = [&e](const uint8_t* d, size_t n)
				 {
					 e.plain.insert(e.plain.end(), d, d + n);
					 if(e.onPlainExtra)
						 e.onPlainExtra(d, n);
				 };
		cb.onEstablished = [&e](const PeerInfo& p)
					 {
						 e.info = p;
						 ++e.established;
					 };
		cb.onFailed = [&e](const std::string& why) { e.failures.push_back(why); };
		cb.onPeerClosed = [&e]() { ++e.peerClosed; };
		cb.log = [&e](LogLevel, const std::string& t) { e.logs.push_back(t); };
		return cb;
	}

	Link(std::shared_ptr<Context> clientCtx, std::shared_ptr<Context> serverCtx, const std::string& peerName = "")
	{
		auto clock = [this] { return now; };
		c.filter = std::make_unique<Filter>(clientCtx, Role::Client, Callbacks(c), peerName, clock);
		s.filter = std::make_unique<Filter>(serverCtx, Role::Server, Callbacks(s), "", clock);
	}

	void Deliver(Endpoint& to, bool toServer, Bytes b, size_t chunk)
	{
		++delivered;
		if(mutate)
			mutate(toServer, b);
		if(b.empty())
			return;
		if(chunk == 0)
			to.filter->OnTransportData(b.data(), b.size());
		else
			for(size_t i = 0; i < b.size(); i += chunk)
				to.filter->OnTransportData(b.data() + i, std::min(chunk, b.size() - i));
	}

	//deliver the records both ways until nothing is left
	void Run(size_t chunk = 0, int maxRounds = 500)
	{
		for(int round = 0; round < maxRounds; ++round)
		{
			bool moved = false;
			while(!c.out.empty())
			{
				Bytes b = std::move(c.out.front());
				c.out.pop_front();
				Deliver(s, true, std::move(b), chunk);
				moved = true;
			}
			while(!s.out.empty())
			{
				Bytes b = std::move(s.out.front());
				s.out.pop_front();
				Deliver(c, false, std::move(b), chunk);
				moved = true;
			}
			if(!moved)
				return;
		}
		FAIL("the exchange did not settle");
	}

	void Handshake(size_t chunk = 0)
	{
		c.filter->Start();
		s.filter->Start();
		Run(chunk);
	}
	void Advance(std::chrono::milliseconds d)
	{
		now += d;
		c.filter->Tick();
		s.filter->Tick();
		Run();
	}
	bool Up() const
	{
		return c.filter->Established() && s.filter->Established();
	}
	//what either side reported as the failure
	std::string Failures() const
	{
		std::string t;
		for(const auto& f : c.failures)
			t += "client: " + f + "\n";
		for(const auto& f : s.failures)
			t += "server: " + f + "\n";
		return t;
	}
};

bool Contains(const std::string& text, const std::string& part)
{
	return text.find(part) != std::string::npos;
}

Bytes RandomBytes(std::mt19937& rng, size_t n)
{
	Bytes b(n);
	for(auto& x : b)
		x = static_cast<uint8_t>(rng());
	return b;
}

Bytes Text(const std::string& s)
{
	return Bytes(s.begin(), s.end());
}

void Send(Endpoint& e, const Bytes& b)
{
	e.filter->Write(b.data(), b.size());
}

//both sides' configurations for one protocol version
struct VersionParam
{
	int version;
	const char* name;
};
const VersionParam Versions[] = {
	{12, "TLS 1.2"}, {13, "TLS 1.3"}
};
}

TEST_CASE(SUITE("mutual authentication and data both ways, TLS 1.2 and 1.3"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		INFO(link.Failures());
		REQUIRE(link.Up());
		CHECK(link.c.established == 1);
		CHECK(link.s.established == 1);

		//what each side learned about the other
		REQUIRE(link.s.info);
		REQUIRE(link.c.info);
		CHECK(link.c.info->protocol == (v.version == 13 ? "TLSv1.3" : "TLSv1.2"));
		CHECK(link.s.info->protocol == link.c.info->protocol);
		CHECK_FALSE(link.c.info->cipher.empty());
		CHECK(link.c.info->certificate);
		CHECK(link.c.info->subject == "CN=tls-server.test");
		CHECK(link.c.info->issuer == "CN=Test CA");
		CHECK(link.c.info->serial == "64"); //100
		CHECK(link.c.info->sha256.size() == 64);
		CHECK(link.c.info->notAfter.size() == 20);
		CHECK(link.c.info->secondsToExpiry > 86400LL * 300);
		CHECK(link.s.info->subject == "CN=tls-client.test");
		CHECK(link.s.info->serial == "c8"); //200
		CHECK_FALSE(link.s.info->resumed);

		Send(link.c, Text("hello from the client"));
		Send(link.s, Text("and from the server"));
		link.Run();
		CHECK(link.s.plain == Text("hello from the client"));
		CHECK(link.c.plain == Text("and from the server"));

		//nothing of the plaintext is on the wire
		Link wire(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		std::string seen;
		wire.mutate = [&seen](bool, Bytes& b) { seen.append(b.begin(), b.end()); };
		wire.Handshake();
		Send(wire.c, Text("SECRET-PLAINTEXT-MARKER"));
		wire.Run();
		REQUIRE(wire.s.plain == Text("SECRET-PLAINTEXT-MARKER"));
		CHECK_FALSE(Contains(seen, "SECRET-PLAINTEXT-MARKER"));
	}
}

TEST_CASE(SUITE("large messages and one byte at a time"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		auto sctx = MakeContext(sc, Role::Server);
		auto cctx = MakeContext(cc, Role::Client);
		std::mt19937 rng(42);
		{
			Link link(cctx, sctx);
			link.Handshake();
			REQUIRE(link.Up());
			const Bytes big = RandomBytes(rng, 2 * 1024 * 1024 + 123);
			const Bytes big2 = RandomBytes(rng, 300000);
			Send(link.c, big);
			Send(link.s, big2);
			link.Run();
			CHECK(link.s.plain == big);
			CHECK(link.c.plain == big2);
			//many small writes arrive in order
			Bytes expected;
			for(int i = 0; i < 200; ++i)
			{
				const Bytes piece = RandomBytes(rng, 1 + rng() % 50);
				expected.insert(expected.end(), piece.begin(), piece.end());
				Send(link.c, piece);
			}
			link.Run();
			Bytes want = big;
			want.insert(want.end(), expected.begin(), expected.end());
			CHECK(link.s.plain == want);
		}
		{
			//the whole conversation through one octet reads
			Link link(cctx, sctx);
			link.Handshake(1);
			INFO(link.Failures());
			REQUIRE(link.Up());
			const Bytes msg = RandomBytes(rng, 40000);
			Send(link.c, msg);
			Send(link.s, msg);
			link.Run(1);
			CHECK(link.s.plain == msg);
			CHECK(link.c.plain == msg);
		}
	}
}

TEST_CASE(SUITE("data written before the handshake completes is queued and sent after it"))
{
	auto& pki = GetPki();
	Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(ServerConf(pki.serverF), Role::Server));
	Send(link.c, Text("early"));
	CHECK(link.c.filter->PendingPlainBytes() == 5);
	link.Handshake();
	REQUIRE(link.Up());
	link.Run();
	CHECK(link.s.plain == Text("early"));
	CHECK(link.c.filter->PendingPlainBytes() == 0);
}

TEST_CASE(SUITE("a client without a certificate is refused by a server that requires one"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(nullptr);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		REQUIRE(link.s.failures.size() == 1);
		CHECK(Contains(link.s.failures[0], "peer did not return a certificate"));
		CHECK(link.s.established == 0);
		//the client learns of it as well (TLS 1.3: after its own handshake completed)
		CHECK(link.c.filter->GetState() == Filter::State::Failed);
		CHECK(link.c.failures.size() == 1);
	}
	//when the server does not require it, the connection is made without
	TlsConf sc = ServerConf(pki.serverF);
	sc.requireClientCert = false;
	Link link(MakeContext(ClientConf(nullptr), Role::Client), MakeContext(sc, Role::Server));
	link.Handshake();
	INFO(link.Failures());
	REQUIRE(link.Up());
	CHECK_FALSE(link.s.info->certificate);
}

TEST_CASE(SUITE("a client certificate from an untrusted CA, an expired, a not yet valid and a revoked one are refused"))
{
	auto& pki = GetPki();
	struct Case
	{
		const char* what;
		const Files* id;
		std::string crl;
		const char* expect;
	};
	const Case cases[] = {
		{"untrusted CA", &pki.clientOtherCaF, "", "unable to get local issuer certificate"},
		{"expired", &pki.clientExpiredF, "", "certificate has expired"},
		{"not yet valid", &pki.clientNotYetF, "", "certificate is not yet valid"},
		{"revoked", &pki.clientRevokedF, pki.crlRevokedFile, "certificate revoked"},
	};
	for(const auto& v : Versions)
		for(const auto& t : cases)
		{
			INFO(v.name << " " << t.what);
			TlsConf sc = ServerConf(pki.serverF);
			sc.crlFile = t.crl;
			TlsConf cc = ClientConf(t.id);
			sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
			Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
			link.Handshake();
			REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
			REQUIRE(link.s.failures.size() == 1);
			INFO(link.s.failures[0]);
			CHECK(Contains(link.s.failures[0], t.expect));
			CHECK(Contains(link.s.failures[0], "depth 0"));
			CHECK(link.s.established == 0);
			CHECK(link.c.filter->GetState() == Filter::State::Failed);
			//the client is told by an alert; it names the problem
			CHECK(Contains(link.c.failures[0], "alert received"));
		}
}

TEST_CASE(SUITE("revocation checking fails closed"))
{
	auto& pki = GetPki();
	//a CRL that does not revoke this client lets it in
	{
		TlsConf sc = ServerConf(pki.serverF);
		sc.crlFile = pki.crlCleanFile;
		Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		INFO(link.Failures());
		CHECK(link.Up());
	}
	//a CRL of another CA: there is no CRL for this client's issuer
	{
		TlsConf sc = ServerConf(pki.serverF);
		sc.crlFile = pki.crlOtherCaFile;
		Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link.s.failures[0], "unable to get certificate CRL"));
	}
	//an expired CRL
	{
		TlsConf sc = ServerConf(pki.serverF);
		sc.crlFile = pki.crlExpiredFile;
		Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link.s.failures[0], "CRL has expired"));
	}
	//the client checks the server's certificate against a CRL as well
	{
		TlsConf cc = ClientConf(&pki.clientF);
		cc.crlFile = pki.crlOtherCaFile;
		Link link(MakeContext(cc, Role::Client), MakeContext(ServerConf(pki.serverF), Role::Server));
		link.Handshake();
		REQUIRE(link.c.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link.c.failures[0], "unable to get certificate CRL"));
	}
}

TEST_CASE(SUITE("PeerCertAllowList: common name, subject alternative name, fingerprint"))
{
	auto& pki = GetPki();
	auto run = [&](const std::vector<std::string>& entries, bool serverSide)
		     {
			     TlsConf sc = ServerConf(pki.serverF);
			     TlsConf cc = ClientConf(&pki.clientF);
			     std::vector<std::string> problems;
			     Json::Value list(Json::arrayValue);
			     for(const auto& e : entries)
				     list.append(e);
			     Json::Value root;
			     root["TLS"]["Enabled"] = true;
			     root["TLS"]["CAFile"] = pki.caFile;
			     root["TLS"]["PeerCertAllowList"] = list;
			     TlsConf parsed;
			     ParseTlsConf(root, false, parsed, problems);
			     REQUIRE(problems.empty());
			     (serverSide ? sc : cc).peerCertAllowList = parsed.peerCertAllowList;
			     auto link = std::make_unique<Link>(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
			     link->Handshake();
			     return link;
		     };
	//the client's certificate, checked by the server
	CHECK(run({"CN:tls-client.test"}, true)->Up());
	CHECK(run({"CN:other.test", "SAN:tls-client.test"}, true)->Up());
	CHECK(run({"SHA256:" + std::string(64, '0'), "CN:tls-client.test"}, true)->Up());
	{
		auto link = run({"CN:someone-else.test"}, true);
		REQUIRE(link->s.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link->s.failures[0], "not in PeerCertAllowList"));
		CHECK(Contains(link->s.failures[0], "tls-client.test"));
	}
	CHECK(run({"SAN:tls-client.test.evil"}, true)->s.filter->GetState() == Filter::State::Failed);
	CHECK(run({"CN:tls-client.tes"}, true)->s.filter->GetState() == Filter::State::Failed);
	CHECK(run({"SHA256:" + std::string(64, 'a')}, true)->s.filter->GetState() == Filter::State::Failed);
	//the fingerprint of the certificate itself, as reported by a handshake, with colons and capitals
	{
		auto first = run({"CN:tls-client.test"}, true);
		REQUIRE(first->s.info);
		std::string fp = first->s.info->sha256, spaced;
		for(size_t i = 0; i < fp.size(); i += 2)
			spaced += (i ? ":" : "") + std::string(1, static_cast<char>(std::toupper(fp[i]))) + std::string(1, static_cast<char>(std::toupper(fp[i + 1])));
		CHECK(run({"SHA256:" + spaced}, true)->Up());
	}
	//the server's certificate, checked by the client; the SAN may be an IP address
	CHECK(run({"CN:tls-server.test"}, false)->Up());
	CHECK(run({"SAN:127.0.0.1"}, false)->Up());
	CHECK(run({"SAN:127.0.0.2"}, false)->c.filter->GetState() == Filter::State::Failed);
}

TEST_CASE(SUITE("the server's certificate is verified by the client"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		//signed by another CA
		TlsConf sc = ServerConf(pki.serverOtherCaF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.c.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link.c.failures[0], "unable to get local issuer certificate"));
		CHECK(Contains(link.c.failures[0], "tls-server.test"));
		CHECK(link.c.established == 0);
		CHECK(link.s.filter->GetState() == Filter::State::Failed); //the client's alert
	}
	//with VerifyPeer off the same server is accepted (and a warning is in the context)
	TlsConf cc = ClientConf(&pki.clientF);
	cc.verifyPeer = false;
	cc.caFile.clear();
	auto cctx = MakeContext(cc, Role::Client);
	REQUIRE(cctx->Warnings().size() == 1);
	CHECK(Contains(cctx->Warnings()[0], "VerifyPeer"));
	Link link(cctx, MakeContext(ServerConf(pki.serverOtherCaF), Role::Server));
	link.Handshake();
	CHECK(link.Up());
}

TEST_CASE(SUITE("host name and IP address verification by the client"))
{
	auto& pki = GetPki();
	auto run = [&](const Files& serverId, const std::string& name, bool verify)
		     {
			     TlsConf cc = ClientConf(&pki.clientF);
			     cc.verifyHostname = verify;
			     auto link = std::make_unique<Link>(MakeContext(cc, Role::Client), MakeContext(ServerConf(serverId), Role::Server), name);
			     link->Handshake();
			     return link;
		     };
	CHECK(run(pki.serverF, "tls-server.test", true)->Up());
	CHECK(run(pki.serverF, "127.0.0.1", true)->Up());
	{
		auto link = run(pki.serverF, "other-name.test", true);
		REQUIRE(link->c.filter->GetState() == Filter::State::Failed);
		INFO(link->c.failures[0]);
		CHECK(Contains(link->c.failures[0], "ostname mismatch"));
	}
	CHECK(run(pki.serverF, "127.0.0.2", true)->c.filter->GetState() == Filter::State::Failed);
	//a certificate without a SAN does not match (the subject CN is not used)
	CHECK(run(pki.serverNoSanF, "tls-nosan.test", true)->c.filter->GetState() == Filter::State::Failed);
	//without VerifyHostname the name is only the SNI
	CHECK(run(pki.serverF, "other-name.test", false)->Up());
	CHECK(run(pki.serverNoSanF, "tls-nosan.test", false)->Up());
}

TEST_CASE(SUITE("protocol version and cipher suite mismatches"))
{
	auto& pki = GetPki();
	{
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = 13;
		cc.maxVersion = 12;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		REQUIRE(link.c.filter->GetState() == Filter::State::Failed);
		INFO(link.Failures());
		CHECK(Contains(link.s.failures[0], "protocol version"));
	}
	{
		//TLS 1.2 only, with no suite in common
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = 12;
		sc.ciphers = "ECDHE-ECDSA-AES256-GCM-SHA384";
		cc.ciphers = "ECDHE-ECDSA-AES128-GCM-SHA256";
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		INFO(link.Failures());
		CHECK(Contains(link.s.failures[0], "no shared cipher"));
	}
	{
		//the cipher list restricts what is negotiated
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = 12;
		cc.ciphers = "ECDHE-ECDSA-AES128-GCM-SHA256";
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		CHECK(link.c.info->cipher == "ECDHE-ECDSA-AES128-GCM-SHA256");
	}
	{
		//TLS 1.3 suites
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.cipherSuites13 = "TLS_AES_256_GCM_SHA384";
		cc.cipherSuites13 = "TLS_AES_256_GCM_SHA384";
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		CHECK(link.c.info->protocol == "TLSv1.3");
		CHECK(link.c.info->cipher == "TLS_AES_256_GCM_SHA384");
	}
}

TEST_CASE(SUITE("garbage, truncated and silent peers never crash or hang"))
{
	auto& pki = GetPki();
	auto sctx = MakeContext(ServerConf(pki.serverF), Role::Server);
	auto cctx = MakeContext(ClientConf(&pki.clientF), Role::Client);
	std::mt19937 rng(7);

	//random bytes into a server
	for(int i = 0; i < 300; ++i)
	{
		Endpoint e;
		Clock::time_point now{};
		e.filter = std::make_unique<Filter>(sctx, Role::Server, Link::Callbacks(e), "", [&now] { return now; });
		e.filter->Start();
		const Bytes junk = RandomBytes(rng, 1 + rng() % 3000);
		e.filter->OnTransportData(junk.data(), junk.size());
		const auto st = e.filter->GetState();
		CHECK((st == Filter::State::Failed || st == Filter::State::Handshaking));
		if(st == Filter::State::Handshaking)
		{
			now += std::chrono::seconds(11);
			e.filter->Tick();
			CHECK(e.filter->GetState() == Filter::State::Failed);
			REQUIRE(e.failures.size() == 1);
			CHECK(Contains(e.failures[0], "did not complete"));
		}
		//after the end nothing more happens
		e.filter->OnTransportData(junk.data(), junk.size());
		Send(e, junk);
		e.filter->Tick();
		CHECK(e.failures.size() <= 1);
	}
	//text that is not TLS (a plain TCP client, an HTTP request)
	for(const std::string& text : {std::string("GET / HTTP/1.0\r\n\r\n"), std::string("\x03\x00\x00\x16\x11\xe0\x00\x00\x00\x01\x00\xc0\x01\x0a\xc2\x02\x00\x01\xc1\x02\x00\x01", 22)})
	{
		Endpoint e;
		e.filter = std::make_unique<Filter>(sctx, Role::Server, Link::Callbacks(e));
		e.filter->Start();
		e.filter->OnTransportData(reinterpret_cast<const uint8_t*>(text.data()), text.size());
		CHECK(e.filter->GetState() == Filter::State::Failed);
		REQUIRE(e.failures.size() == 1);
		INFO(e.failures[0]);
		CHECK(Contains(e.failures[0], "not a TLS handshake record"));
		CHECK(Contains(e.failures[0], "plain RFC 1006") == (text[0] == 0x03));
		CHECK(e.bytesOut == 0); //nothing is sent back to a peer that does not speak TLS
	}
	//a handshake truncated after the hello: times out
	{
		Link link(cctx, sctx);
		link.c.filter->Start();
		link.s.filter->Start();
		REQUIRE_FALSE(link.c.out.empty());
		Bytes hello = link.c.out.front();
		link.c.out.pop_front();
		hello.resize(hello.size() / 2);
		link.s.filter->OnTransportData(hello.data(), hello.size());
		link.Run();
		CHECK(link.s.filter->GetState() == Filter::State::Handshaking);
		link.Advance(std::chrono::milliseconds(9000));
		CHECK(link.s.filter->GetState() == Filter::State::Handshaking);
		link.Advance(std::chrono::milliseconds(2000));
		CHECK(link.s.filter->GetState() == Filter::State::Failed);
		CHECK(link.c.filter->GetState() == Filter::State::Failed);
		REQUIRE(link.s.failures.size() == 1);
		CHECK(Contains(link.s.failures[0], "10000 ms"));
	}
	//a client whose server never answers
	{
		Link link(cctx, sctx);
		link.c.filter->Start();
		link.now += std::chrono::seconds(30);
		link.c.filter->Tick();
		CHECK(link.c.filter->GetState() == Filter::State::Failed);
	}
	//the configured timeout
	{
		TlsConf sc = ServerConf(pki.serverF);
		sc.handshakeTimeoutMs = 500;
		Link link(cctx, MakeContext(sc, Role::Server));
		link.s.filter->Start();
		link.Advance(std::chrono::milliseconds(400));
		CHECK(link.s.filter->GetState() == Filter::State::Handshaking);
		link.Advance(std::chrono::milliseconds(200));
		CHECK(link.s.filter->GetState() == Filter::State::Failed);
	}
}

TEST_CASE(SUITE("mutated handshake and data records end cleanly"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		auto sctx = MakeContext(sc, Role::Server);
		auto cctx = MakeContext(cc, Role::Client);
		std::mt19937 rng(1234 + v.version);
		size_t failed = 0, established = 0;
		for(int i = 0; i < 250; ++i)
		{
			Link link(cctx, sctx);
			const size_t target = rng() % 12; //which record to damage: handshake records and then the data records
			const int kind = static_cast<int>(rng() % 4);
			const size_t pos = rng();
			const uint8_t mask = static_cast<uint8_t>(1 + rng() % 255);
			size_t n = 0;
			link.mutate = [&](bool, Bytes& b)
					  {
						  if(n++ != target || b.empty())
							  return;
						  switch(kind)
						  {
							  case 0: b[pos % b.size()] ^= mask; break;                      //flip bits
							  case 1: b.resize(pos % b.size()); break;                       //truncate
							  case 2: b.insert(b.end(), b.begin(), b.end()); break;          //duplicate
							  case 3: b.assign(std::min<size_t>(b.size(), 20), mask); break; //replace by junk
						  }
					  };
			link.c.filter->Start();
			link.s.filter->Start();
			link.Run();
			Send(link.c, Text("client data"));
			Send(link.s, Text("server data"));
			Send(link.c, Text("more client data"));
			link.Run();
			link.now += std::chrono::seconds(20);
			link.c.filter->Tick();
			link.s.filter->Tick();
			link.Run();
			for(auto* e : {&link.c, &link.s})
			{
				const auto st = e->filter->GetState();
				CHECK(e->failures.size() <= 1);
				if(st == Filter::State::Failed)
					CHECK(e->failures.size() == 1);
				else
					CHECK(e->failures.empty());
				CHECK(st != Filter::State::Handshaking); //the timeout ended it
				if(st == Filter::State::Failed)
					++failed;
				else if(st == Filter::State::Established)
					++established;
			}
		}
		INFO("failed " << failed << " established " << established);
		CHECK(failed > 100); //most damage is noticed
	}
}

TEST_CASE(SUITE("close_notify and shutdown"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		Send(link.c, Text("last words"));
		link.c.filter->Shutdown();
		CHECK(link.c.filter->GetState() == Filter::State::Closed);
		Send(link.c, Text("ignored after shutdown"));
		link.Run();
		CHECK(link.s.plain == Text("last words"));
		CHECK(link.s.peerClosed == 1);
		CHECK(link.s.filter->GetState() == Filter::State::Closed);
		CHECK(link.s.failures.empty());
		CHECK(link.c.failures.empty());
		//nothing more is delivered or sent
		const size_t out = link.s.bytesOut;
		Send(link.s, Text("too late"));
		link.Run();
		CHECK(link.s.bytesOut == out);
		CHECK(link.c.plain.empty());
		link.s.filter->Shutdown(); //twice is harmless
	}
	//Shutdown before the handshake finished
	Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(ServerConf(pki.serverF), Role::Server));
	link.c.filter->Start();
	link.c.filter->Shutdown();
	CHECK(link.c.filter->GetState() == Filter::State::Closed);
}

TEST_CASE(SUITE("a fatal alert from the peer ends the session"))
{
	auto& pki = GetPki();
	//damaged application data: the receiver sends bad_record_mac and fails
	Link link(MakeContext(ClientConf(&pki.clientF), Role::Client), MakeContext(ServerConf(pki.serverF), Role::Server));
	link.Handshake();
	REQUIRE(link.Up());
	link.mutate = [](bool toServer, Bytes& b)
			  {
				  if(toServer && b.size() > 10)
					  b[b.size() - 3] ^= 0x55;
			  };
	Send(link.c, Text("tampered"));
	link.Run();
	CHECK(link.s.filter->GetState() == Filter::State::Failed);
	REQUIRE(link.s.failures.size() == 1);
	CHECK(link.s.plain.empty());
	CHECK(link.c.filter->GetState() == Filter::State::Failed);
	REQUIRE(link.c.failures.size() == 1);
	CHECK(Contains(link.c.failures[0], "alert received"));
}

TEST_CASE(SUITE("session resumption"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		sc.sessionResumption = cc.sessionResumption = true;
		auto sctx = MakeContext(sc, Role::Server);
		auto cctx = MakeContext(cc, Role::Client);
		{
			Link link(cctx, sctx);
			link.Handshake();
			REQUIRE(link.Up());
			CHECK_FALSE(link.c.info->resumed);
			Send(link.s, Text("x")); //TLS 1.3: the session ticket arrives after the handshake
			link.Run();
			//a session is only kept when the connection was closed properly
			link.c.filter->Shutdown();
			link.Run();
		}
		{
			Link link(cctx, sctx);
			link.Handshake();
			INFO(link.Failures());
			REQUIRE(link.Up());
			CHECK(link.c.info->resumed);
			CHECK(link.s.info->resumed);
			CHECK(link.s.info->subject == "CN=tls-client.test"); //the peer is still known
			Send(link.c, Text("data"));
			link.Run();
			CHECK(link.s.plain == Text("data"));
		}
		//with resumption off a new context never resumes
		sc.sessionResumption = cc.sessionResumption = false;
		auto sctx2 = MakeContext(sc, Role::Server);
		auto cctx2 = MakeContext(cc, Role::Client);
		for(int i = 0; i < 2; ++i)
		{
			Link link(cctx2, sctx2);
			link.Handshake();
			REQUIRE(link.Up());
			CHECK_FALSE(link.c.info->resumed);
		}
	}
}

TEST_CASE(SUITE("a resumed session still has to pass PeerCertAllowList"))
{
	auto& pki = GetPki();
	TlsConf sc = ServerConf(pki.serverF);
	TlsConf cc = ClientConf(&pki.clientF);
	sc.sessionResumption = cc.sessionResumption = true;
	sc.peerCertAllowList.push_back({AllowEntry::Kind::CommonName, "tls-client.test", "CN:tls-client.test"});
	auto sctx = MakeContext(sc, Role::Server);
	auto cctx = MakeContext(cc, Role::Client);
	for(int i = 0; i < 2; ++i)
	{
		Link link(cctx, sctx);
		link.Handshake();
		INFO(link.Failures());
		CHECK(link.Up());
		Send(link.s, Text("x"));
		link.Run();
		link.c.filter->Shutdown();
		link.Run();
		if(i == 1)
			CHECK(link.s.info->resumed);
	}
}

TEST_CASE(SUITE("renegotiation (TLS 1.2) and key update (TLS 1.3) at the configured interval"))
{
	auto& pki = GetPki();
	for(const auto& v : Versions)
	{
		INFO(v.name);
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		cc.renegotiateIntervalSeconds = 2;
		sc.renegotiateIntervalSeconds = 2; //the server accepts them from the client
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		CHECK(link.c.filter->Renegotiations() == 0);
		link.Advance(std::chrono::milliseconds(1500));
		CHECK(link.c.filter->Renegotiations() == 0);
		for(int i = 1; i <= 4; ++i)
		{
			link.Advance(std::chrono::milliseconds(2000));
			INFO("renegotiation " << i << ": " << link.Failures());
			REQUIRE(link.Up());
			CHECK(link.c.filter->Renegotiations() == static_cast<uint64_t>(i));
			//the connection carries data through it, both ways
			const std::string text = "after " + std::to_string(i);
			Send(link.c, Text(text));
			Send(link.s, Text(text));
			link.Run();
			CHECK(Contains(std::string(link.s.plain.begin(), link.s.plain.end()), text));
			CHECK(Contains(std::string(link.c.plain.begin(), link.c.plain.end()), text));
		}
		if(v.version == 12)
			CHECK(link.s.filter->Renegotiations() == 4);
		//data written in the middle of a renegotiation is not lost
		Bytes burst;
		std::mt19937 rng(5);
		link.Advance(std::chrono::milliseconds(2000));
		for(int i = 0; i < 20; ++i)
		{
			const Bytes piece = RandomBytes(rng, 5000);
			burst.insert(burst.end(), piece.begin(), piece.end());
			Send(link.c, piece);
			Send(link.s, piece);
		}
		link.Run();
		CHECK(link.Up());
		CHECK(std::search(link.s.plain.begin(), link.s.plain.end(), burst.begin(), burst.end()) != link.s.plain.end());
		CHECK(std::search(link.c.plain.begin(), link.c.plain.end(), burst.begin(), burst.end()) != link.c.plain.end());
	}
}

TEST_CASE(SUITE("a peer that refuses renegotiation, and one that renegotiates too often"))
{
	auto& pki = GetPki();
	//the server has renegotiation off (interval 0): it refuses the client's request, and the client ends the session (RFC 5746 alert
	//no_renegotiation during a renegotiation is fatal in OpenSSL). Nothing crashes or hangs, and no second handshake completes.
	{
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = 12;
		cc.renegotiateIntervalSeconds = 1;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		link.Advance(std::chrono::milliseconds(1100));
		INFO(link.Failures());
		CHECK(link.s.established == 1);
		CHECK(link.c.established == 1);
		REQUIRE(link.c.filter->GetState() == Filter::State::Failed);
		REQUIRE(link.c.failures.size() == 1);
		CHECK(Contains(link.c.failures[0], "no renegotiation"));
		CHECK(link.s.filter->GetState() == Filter::State::Failed);
	}
	//with the interval 0 on both sides nothing is ever renegotiated
	{
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = 12;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		link.Advance(std::chrono::hours(5));
		CHECK(link.Up());
		CHECK(link.c.filter->Renegotiations() == 0);
	}
	//a client that renegotiates much more often than the server's interval allows is cut off
	{
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = 12;
		cc.renegotiateIntervalSeconds = 1;
		sc.renegotiateIntervalSeconds = 3600;
		Link link(MakeContext(cc, Role::Client), MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		for(int i = 0; i < 4 && link.s.filter->GetState() == Filter::State::Established; ++i)
			link.Advance(std::chrono::milliseconds(1100));
		REQUIRE(link.s.filter->GetState() == Filter::State::Failed);
		CHECK(Contains(link.s.failures[0], "renegotiates too often"));
	}
}

TEST_CASE(SUITE("a peer certificate that expires soon is warned about, an expired one is refused"))
{
	auto& pki = GetPki();
	TlsConf sc = ServerConf(pki.serverF);
	Link link(MakeContext(ClientConf(&pki.clientSoonF), Role::Client), MakeContext(sc, Role::Server));
	link.Handshake();
	REQUIRE(link.Up());
	REQUIRE(link.s.logs.size() >= 1);
	CHECK(Contains(link.s.logs[0], "expires in"));
	CHECK(Contains(link.s.logs[0], "tls-soon.test"));
	CHECK(link.s.info->secondsToExpiry < 86400LL * 11);
	CHECK(link.s.info->secondsToExpiry > 86400LL * 9);
	//no warning when it is further away than the configured days
	sc.certExpiryWarningDays = 5;
	Link quiet(MakeContext(ClientConf(&pki.clientSoonF), Role::Client), MakeContext(sc, Role::Server));
	quiet.Handshake();
	REQUIRE(quiet.Up());
	CHECK(quiet.s.logs.empty());
}

TEST_CASE(SUITE("with VerifyPeer off an expired peer certificate is accepted but reported"))
{
	auto& pki = GetPki();
	TlsConf sc = ServerConf(pki.serverF);
	sc.verifyPeer = false;
	sc.caFile.clear();
	Link link(MakeContext(ClientConf(&pki.clientExpiredF), Role::Client), MakeContext(sc, Role::Server));
	link.Handshake();
	REQUIRE(link.s.filter->GetState() == Filter::State::Established);
	CHECK(link.s.info->secondsToExpiry < 0);
	REQUIRE_FALSE(link.s.logs.empty());
	CHECK(Contains(link.s.logs[0], "has expired"));
}

TEST_CASE(SUITE("the key password comes from an environment variable"))
{
	auto& pki = GetPki();
	auto setenv_ = [](const char* name, const char* value)
			   {
				   #ifdef _MSC_VER
				   _putenv_s(name, value);
				   #else
				   setenv(name, value, 1);
				   #endif
			   };
	TlsConf cc = ClientConf(&pki.clientEncryptedF);
	cc.keyPasswordEnv = "tls_TEST_KEY_PASSWORD_VARIABLE";
	std::string error;

	setenv_("tls_TEST_KEY_PASSWORD_VARIABLE", "wrong-password");
	CHECK_FALSE(Context::Create(cc, Role::Client, error));
	CHECK(Contains(error, "\"KeyFile\""));
	CHECK(Contains(error, "password is wrong"));

	setenv_("tls_TEST_KEY_PASSWORD_VARIABLE", "synthetic-pass-1");
	auto ctx = Context::Create(cc, Role::Client, error);
	INFO(error);
	REQUIRE(ctx);
	Link link(ctx, MakeContext(ServerConf(pki.serverF), Role::Server));
	link.Handshake();
	REQUIRE(link.Up());
	CHECK(link.s.info->subject == "CN=tls-enc.test");

	setenv_("tls_TEST_KEY_PASSWORD_VARIABLE", "");
	CHECK_FALSE(Context::Create(cc, Role::Client, error));

	//no variable named: an encrypted key is refused (never prompts), the message says what to do
	cc.keyPasswordEnv.clear();
	CHECK_FALSE(Context::Create(cc, Role::Client, error));
	CHECK(Contains(error, "KeyPasswordEnv"));

	cc.keyPasswordEnv = "tls_TEST_NO_SUCH_VARIABLE_SET";
	CHECK_FALSE(Context::Create(cc, Role::Client, error));
	CHECK(Contains(error, "tls_TEST_NO_SUCH_VARIABLE_SET"));
}

TEST_CASE(SUITE("configuration errors name the key and the file"))
{
	auto& pki = GetPki();
	std::string error;
	auto fails = [&](TlsConf c, Role role, const std::string& expectKey)
			 {
				 error.clear();
				 const bool created = Context::Create(c, role, error) != nullptr;
				 INFO(error);
				 CHECK_FALSE(created);
				 CHECK(Contains(error, expectKey));
			 };
	{
		TlsConf c = ServerConf(pki.serverF);
		c.certFile = pki.dir.Path("no-such.crt");
		fails(c, Role::Server, "\"CertFile\"");
		CHECK(Contains(error, "no-such.crt"));
		CHECK(Contains(error, "cannot be read"));
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.keyFile = pki.dir.Path("no-such.key");
		fails(c, Role::Server, "\"KeyFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.certFile = pki.garbageFile;
		fails(c, Role::Server, "\"CertFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.keyFile = pki.garbageFile;
		fails(c, Role::Server, "\"KeyFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.keyFile = pki.clientF.key; //another certificate's key
		fails(c, Role::Server, "do not belong together");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.caFile = pki.dir.Path("no-such-ca.crt");
		fails(c, Role::Server, "\"CAFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.caFile = pki.garbageFile;
		fails(c, Role::Server, "\"CAFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.caFile.clear();
		c.caPath = pki.dir.Path("no-such-dir");
		fails(c, Role::Server, "\"CAPath\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.crlFile = pki.dir.Path("no-such.crl");
		fails(c, Role::Server, "\"CRLFile\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.crlFile = pki.garbageFile;
		fails(c, Role::Server, "\"CRLFile\"");
		CHECK(Contains(error, "no valid PEM CRL"));
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.ciphers = "NO-SUCH-CIPHER";
		fails(c, Role::Server, "\"Ciphers\"");
	}
	{
		TlsConf c = ServerConf(pki.serverF);
		c.cipherSuites13 = "NO_SUCH_SUITE";
		fails(c, Role::Server, "\"CipherSuites13\"");
	}
	//a client without a certificate is fine; an expiring own certificate is a warning
	{
		TlsConf c = ClientConf(nullptr);
		std::string e;
		CHECK(Context::Create(c, Role::Client, e));
		TlsConf soon = ClientConf(&pki.clientSoonF);
		auto ctx = Context::Create(soon, Role::Client, e);
		REQUIRE(ctx);
		REQUIRE(ctx->Warnings().size() == 1);
		CHECK(Contains(ctx->Warnings()[0], "expires in"));
	}
}

TEST_CASE(SUITE("a context notices that its files changed"))
{
	auto& pki = GetPki();
	tlstest::TempDir dir;
	TlsConf c = ClientConf(nullptr);
	c.caFile = dir.Write("ca.pem", pki.ca.certPem);
	std::string error;
	auto ctx = Context::Create(c, Role::Client, error);
	REQUIRE(ctx);
	CHECK_FALSE(ctx->Stale());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	dir.Write("ca.pem", pki.ca.certPem + pki.otherCa.certPem);
	std::filesystem::last_write_time(c.caFile, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(5));
	CHECK(ctx->Stale());
	CHECK_FALSE(Context::Create(c, Role::Client, error)->Stale());
}

TEST_CASE(SUITE("a failure reports what the handshake exchanged"))
{
	auto& pki = GetPki();
	TlsConf cc = ClientConf(&pki.clientF);
	cc.caFile = pki.otherCaFile; //the server's certificate won't verify
	Link link(MakeContext(cc, Role::Client), MakeContext(ServerConf(pki.serverF), Role::Server));
	link.Handshake();
	REQUIRE(link.c.failures.size() == 1);
	const std::string summary = link.c.filter->HandshakeSummary();
	CHECK(Contains(summary, "we are the TLS client"));
	CHECK(Contains(summary, "sent: ClientHello ("));
	CHECK(Contains(summary, "received: ServerHello ("));
	CHECK(Contains(link.c.failures[0], "[" + summary + "]"));
	REQUIRE(link.s.failures.size() == 1);
	CHECK(Contains(link.s.filter->HandshakeSummary(), "alert fatal"));
}

TEST_CASE(SUITE("the key log file gets the session secrets, only when it is configured"))
{
	auto& pki = GetPki();
	tlstest::TempDir dir;
	for(const auto& v : Versions)
	{
		INFO(v.name);
		const std::string path = dir.Path(std::string("keys") + std::to_string(v.version) + ".log");
		TlsConf sc = ServerConf(pki.serverF);
		TlsConf cc = ClientConf(&pki.clientF);
		sc.minVersion = sc.maxVersion = cc.minVersion = cc.maxVersion = v.version;
		cc.keyLogFile = path;
		auto cctx = MakeContext(cc, Role::Client);
		REQUIRE(cctx->Warnings().size() == 1);
		CHECK(Contains(cctx->Warnings()[0], "KEY LOGGING IS ON"));
		Link link(cctx, MakeContext(sc, Role::Server));
		link.Handshake();
		REQUIRE(link.Up());
		CHECK(std::any_of(link.c.logs.begin(), link.c.logs.end(), [](const std::string& l) { return Contains(l, "KEY LOGGING IS ON"); }));
		CHECK(link.s.logs.empty());
		std::ifstream f(path);
		const std::string keys((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		CHECK(Contains(keys, v.version == 13 ? "CLIENT_TRAFFIC_SECRET_0 " : "CLIENT_RANDOM "));
	}
}

#endif // ODC_ASIO_SSL
