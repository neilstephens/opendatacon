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
 * TlsConfTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The "TLS" section of a port configuration (TLSConf.h), independent of any port: the keys and their messages, the defaults and the named
 *  presets that a port passes in (tested with dummy ones). These run in every build; the checks that need files and a TLS context only with
 *  ODC_ASIO_SSL. What a particular port passes in is tested with that port.
 */

#include "TlsTestPki.h"
#include <catch.hpp>
#include <opendatacon/TLSConf.h>
#include <opendatacon/TLSFilter.h>
#include <sstream>

#define SUITE(name) "TLS configuration - " name
#define NEWSUITE(name) "TLS configuration - " name

using namespace odc::tls;

namespace
{
Json::Value Parse(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b, in, &v, &errs));
	return v;
}

bool AnyContains(const std::vector<std::string>& list, const std::string& part)
{
	for(const auto& s : list)
		if(s.find(part) != std::string::npos)
			return true;
	return false;
}

std::vector<std::string> ParseTls(const std::string& tlsJson, bool listener, TlsConf* out = nullptr, const TlsConf& defaults = TlsConf(), const PresetTable& presets = PresetTable())
{
	TlsConf c;
	std::vector<std::string> problems;
	ParseTlsConf(Parse("{\"TLS\": " + tlsJson + "}"), listener, c, problems, defaults, presets);
	if(out)
		*out = c;
	return problems;
}

//dummy presets: one that sets values (and leaves alone what is given), and one that records what it is shown
void StrictPreset(TlsConf& c, const KeyGiven& isGiven)
{
	if(!isGiven("MinVersion"))
		c.minVersion = 13;
	if(!isGiven("Ciphers"))
		c.ciphers = "PRESET-CIPHER";
	if(!isGiven("Groups"))
		c.groups = "PRESET-GROUP";
	if(!isGiven("SendSNI"))
		c.sendSni = false;
}

void NoPreset(TlsConf&, const KeyGiven&)
{}

TlsConf g_seen;
std::vector<std::string> g_seenGiven;
void ProbePreset(TlsConf& c, const KeyGiven& isGiven)
{
	g_seen = c;
	for(const char* key : {"Ciphers", "Groups", "MinVersion", "Bogus"})
		if(isGiven(key))
			g_seenGiven.push_back(key);
}
}

TEST_CASE(SUITE("absent or disabled"))
{
	TlsConf c;
	std::vector<std::string> problems;
	ParseTlsConf(Parse("{}"), true, c, problems);
	CHECK(problems.empty());
	CHECK_FALSE(c.enabled);

	CHECK(ParseTls(R"({"Enabled": false})", true, &c).empty());
	CHECK_FALSE(c.enabled);
	//a disabled section is still checked for shape, but needs nothing else
	CHECK(ParseTls(R"({"Enabled": false, "CertFile": "x.pem", "MinVersion": "1.3"})", true, &c).empty());
	CHECK(c.minVersion == 13);
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "MinVersion": "1.1"})", true), "MinVersion"));
	CHECK(AnyContains(ParseTls(R"(true)", true), "must be a JSON object"));
}

TEST_CASE(SUITE("the shape is checked strictly, every message names the key"))
{
	CHECK(AnyContains(ParseTls(R"({"Enabled": "yes"})", true), "\"Enabled\" must be true or false"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "Bogus": 1})", true), "unknown key \"Bogus\""));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "KeyPassword": "secret"})", true), "KeyPasswordEnv"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "CertFile": 5})", true), "\"CertFile\" must be a string"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "MinVersion": 1.2})", true), "\"MinVersion\" must be a string"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "MaxVersion": "SSLv3"})", true), "\"MaxVersion\" must be \"1.2\" or \"1.3\""));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "HandshakeTimeoutms": 5})", true), "\"HandshakeTimeoutms\" must be between"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "HandshakeTimeoutms": "10"})", true), "\"HandshakeTimeoutms\" must be an integer"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "RenegotiateIntervalSeconds": -1})", true), "\"RenegotiateIntervalSeconds\" must be between"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": "CN:x"})", true), "\"PeerCertAllowList\" must be an array"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": [5]})", true), "entries must be strings"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": ["CN=x"]})", true), "must start with CN:, SAN: or SHA256:"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": ["CN:"]})", true), "empty or invalid"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": ["SHA256:abc"]})", true), "64 hexadecimal digits"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "PeerCertAllowList": ["MD5:abc"]})", true), "must start with"));
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "CertFile": "a\u0001b"})", true), "control characters"));
}

TEST_CASE(SUITE("the allow list and the options are parsed"))
{
	TlsConf c;
	const std::string fp = "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89";
	const auto problems = ParseTls(R"({"Enabled": false, "PeerCertAllowList": ["CN:tls-peer.test", "SAN:10.0.0.1", "SHA256:)" + fp + R"("],
		"MinVersion": "1.3", "MaxVersion": "1.3", "VerifyHostname": true, "ServerName": "tls-peer.test", "HandshakeTimeoutms": 2500,
		"RenegotiateIntervalSeconds": 3600, "SessionResumption": false, "CertExpiryWarningDays": 7, "VerifyPeer": false, "RequireClientCert": false,
		"CRLCheckChain": true, "Ciphers": "ECDHE-RSA-AES128-GCM-SHA256", "CipherSuites13": "TLS_AES_128_GCM_SHA256", "KeyPasswordEnv": "SOME_VARIABLE"})", true, &c);
	REQUIRE(problems.empty());
	REQUIRE(c.peerCertAllowList.size() == 3);
	CHECK(c.peerCertAllowList[0].kind == AllowEntry::Kind::CommonName);
	CHECK(c.peerCertAllowList[0].value == "tls-peer.test");
	CHECK(c.peerCertAllowList[1].kind == AllowEntry::Kind::San);
	CHECK(c.peerCertAllowList[2].kind == AllowEntry::Kind::Sha256);
	CHECK(c.peerCertAllowList[2].value == "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
	CHECK(c.minVersion == 13);
	CHECK(c.maxVersion == 13);
	CHECK(c.verifyHostname);
	CHECK(c.serverName == "tls-peer.test");
	CHECK(c.handshakeTimeoutMs == 2500);
	CHECK(c.renegotiateIntervalSeconds == 3600);
	CHECK_FALSE(c.sessionResumption);
	CHECK(c.certExpiryWarningDays == 7);
	CHECK_FALSE(c.verifyPeer);
	CHECK_FALSE(c.requireClientCert);
	CHECK(c.crlCheckChain);
	CHECK(c.ciphers == "ECDHE-RSA-AES128-GCM-SHA256");
	CHECK(c.cipherSuites13 == "TLS_AES_128_GCM_SHA256");
	CHECK(c.keyPasswordEnv == "SOME_VARIABLE");

	//the library's own defaults: secure ones, and no opinion on the cipher lists (OpenSSL's)
	TlsConf d;
	CHECK_FALSE(d.enabled);
	CHECK(d.minVersion == 12);
	CHECK(d.maxVersion == 13);
	CHECK(d.verifyPeer);
	CHECK(d.requireClientCert);
	CHECK(d.renegotiateIntervalSeconds == 0);
	CHECK(d.handshakeTimeoutMs == 10000);
	CHECK_FALSE(d.sessionResumption); //off by default: see TLSConf.h
	CHECK(d.certExpiryWarningDays == 30);
	CHECK(d.ciphers.empty());
	CHECK(d.cipherSuites13.empty());
	CHECK(d.groups.empty());
	CHECK(d.sessionIdContext == "opendatacon");
	CHECK(d.compatibility == "Default");
}

TEST_CASE(NEWSUITE("the starting values are the library's defaults, or what the port passes in"))
{
	//nothing passed: the library's own defaults, with or without a TLS section
	{
		TlsConf c;
		c.ciphers = "left over";
		std::vector<std::string> problems;
		ParseTlsConf(Parse("{}"), false, c, problems);
		CHECK(problems.empty());
		CHECK(c.ciphers.empty());
		CHECK(c.cipherSuites13.empty());
		CHECK(c.sessionIdContext == "opendatacon");
	}
	TlsConf mine;
	mine.ciphers = "DUMMY-CIPHER-A:DUMMY-CIPHER-B";
	mine.cipherSuites13 = "DUMMY_SUITE";
	mine.groups = "DUMMY-GROUP";
	mine.sessionIdContext = "dummy-port";
	mine.minVersion = 13;
	mine.handshakeTimeoutMs = 4321;

	//no TLS section, a disabled one, an empty one: the starting values come out unchanged
	for(const std::string& json : {std::string("{}"), std::string(R"({"TLS": {"Enabled": false}})"), std::string(R"({"TLS": {}})")})
	{
		INFO(json);
		TlsConf c;
		std::vector<std::string> problems;
		ParseTlsConf(Parse(json), true, c, problems, mine);
		CHECK(problems.empty());
		CHECK_FALSE(c.enabled);
		CHECK(c.ciphers == mine.ciphers);
		CHECK(c.cipherSuites13 == mine.cipherSuites13);
		CHECK(c.groups == mine.groups);
		CHECK(c.sessionIdContext == mine.sessionIdContext);
		CHECK(c.minVersion == 13);
		CHECK(c.maxVersion == 13);
		CHECK(c.handshakeTimeoutMs == 4321);
		CHECK(c.verifyPeer); //what nobody changed is the library's
	}
	//explicit keys override them, key by key
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Enabled": false, "Ciphers": "EXPLICIT", "MaxVersion": "1.3"})", false, &c, mine);
		CHECK(problems.empty());
		CHECK(c.ciphers == "EXPLICIT");
		CHECK(c.cipherSuites13 == "DUMMY_SUITE");
		CHECK(c.groups == "DUMMY-GROUP");
		CHECK(c.minVersion == 13);
	}
	//the starting values may be the object being filled
	{
		TlsConf c = mine;
		std::vector<std::string> problems;
		ParseTlsConf(Parse(R"({"TLS": {"Enabled": false, "Groups": "OTHER"}})"), false, c, problems, c);
		CHECK(c.ciphers == "DUMMY-CIPHER-A:DUMMY-CIPHER-B");
		CHECK(c.groups == "OTHER");
	}
	//a parse starts from the starting values again every time, nothing is left over from the one before
	{
		TlsConf c;
		std::vector<std::string> problems;
		ParseTlsConf(Parse(R"({"TLS": {"Enabled": false, "Groups": "OTHER", "CertFile": "x.pem"}})"), false, c, problems, mine);
		ParseTlsConf(Parse("{}"), false, c, problems, mine);
		CHECK(c.groups == "DUMMY-GROUP");
		CHECK(c.certFile.empty());
	}
}

TEST_CASE(NEWSUITE("the Compatibility presets come from the table the port passes in"))
{
	//dummy presets: one that sets values, and one that shows what it is given
	g_seen = TlsConf();
	g_seenGiven.clear();
	PresetTable table;
	table.push_back({"Strict", StrictPreset});
	table.push_back({"Probe", ProbePreset});
	TlsConf& seen = g_seen;
	std::vector<std::string>& seenGiven = g_seenGiven;
	TlsConf mine;
	mine.ciphers = "DUMMY-CIPHER";
	mine.groups = "";

	//"Default" is always valid and changes nothing, with or without a table
	for(const bool withTable : {true, false})
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Enabled": false, "Compatibility": "Default"})", false, &c, mine, withTable ? table : PresetTable());
		CHECK(problems.empty());
		CHECK(c.compatibility == "Default");
		CHECK(c.ciphers == "DUMMY-CIPHER");
		CHECK(c.minVersion == 12);
		CHECK(c.sendSni);
	}
	//a named preset fills in what is not given, on top of the starting values
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Enabled": false, "Compatibility": "Strict"})", false, &c, mine, table);
		CHECK(problems.empty());
		CHECK(c.compatibility == "Strict");
		CHECK(c.minVersion == 13);
		CHECK(c.ciphers == "PRESET-CIPHER");
		CHECK(c.groups == "PRESET-GROUP");
		CHECK_FALSE(c.sendSni);
		CHECK(c.maxVersion == 13); //untouched
		CHECK(c.cipherSuites13.empty());
	}
	//what is given explicitly wins, wherever the key stands in the section
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Ciphers": "EXPLICIT", "Enabled": false, "Compatibility": "Strict", "SendSNI": true, "MinVersion": "1.2"})", false, &c, mine, table);
		CHECK(problems.empty());
		CHECK(c.ciphers == "EXPLICIT");
		CHECK(c.sendSni);
		CHECK(c.minVersion == 12);
		CHECK(c.groups == "PRESET-GROUP");
	}
	//an explicit key given with a bad value is a problem and is still "given": the preset does not paper over it
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Enabled": false, "Compatibility": "Strict", "Ciphers": 5})", false, &c, mine, table);
		CHECK(AnyContains(problems, "\"Ciphers\" must be a string"));
		CHECK(c.ciphers == "DUMMY-CIPHER");
	}
	//the preset sees the starting values and the explicit keys, and which keys were given
	{
		TlsConf c;
		const auto problems = ParseTls(R"({"Enabled": false, "Ciphers": "EXPLICIT", "MinVersion": "1.3", "Compatibility": "Probe"})", false, &c, mine, table);
		CHECK(problems.empty());
		CHECK(seen.ciphers == "EXPLICIT");
		CHECK(seen.minVersion == 13);
		CHECK(seen.groups.empty());
		REQUIRE(seenGiven.size() == 2);
		CHECK(seenGiven[0] == "Ciphers");
		CHECK(seenGiven[1] == "MinVersion");
	}
	//a preset without a function is accepted and changes nothing
	{
		TlsConf c;
		PresetTable empty;
		empty.push_back({"Empty", nullptr});
		const auto problems = ParseTls(R"({"Enabled": false, "Compatibility": "Empty"})", false, &c, mine, empty);
		CHECK(problems.empty());
		CHECK(c.compatibility == "Empty");
		CHECK(c.ciphers == "DUMMY-CIPHER");
	}
	//the preset applies to a disabled section as well as an enabled one
	{
		TlsConf c;
		ParseTls(R"({"Enabled": true, "Compatibility": "Strict"})", true, &c, mine, table);
		CHECK(c.enabled);
		CHECK(c.ciphers == "PRESET-CIPHER");
	}
}

TEST_CASE(NEWSUITE("an unknown preset name is refused with the names of the table"))
{
	PresetTable one, two, three;
	one.push_back({"Alpha", NoPreset});
	two.push_back({"Alpha", NoPreset});
	two.push_back({"Beta", NoPreset});
	three.push_back({"Alpha", nullptr});
	three.push_back({"Beta", nullptr});
	three.push_back({"Gamma", nullptr});

	auto message = [](const PresetTable& t, const std::string& json)
			   {
				   const auto problems = ParseTls(json, false, nullptr, TlsConf(), t);
				   REQUIRE(problems.size() == 1);
				   return problems[0];
			   };
	CHECK(message(PresetTable(), R"({"Enabled": false, "Compatibility": "Alpha"})") == "\"TLS\": \"Compatibility\" must be \"Default\"");
	CHECK(message(one, R"({"Enabled": false, "Compatibility": "Modern"})") == "\"TLS\": \"Compatibility\" must be \"Default\" or \"Alpha\"");
	CHECK(message(two, R"({"Enabled": false, "Compatibility": "Modern"})") == "\"TLS\": \"Compatibility\" must be \"Default\", \"Alpha\" or \"Beta\"");
	CHECK(message(three, R"({"Enabled": false, "Compatibility": ""})") == "\"TLS\": \"Compatibility\" must be \"Default\", \"Alpha\", \"Beta\" or \"Gamma\"");
	//the names are case sensitive, and "Default" is not an entry of the table
	CHECK(AnyContains(ParseTls(R"({"Enabled": false, "Compatibility": "alpha"})", false, nullptr, TlsConf(), one), "must be \"Default\" or \"Alpha\""));
	CHECK(ParseTls(R"({"Enabled": false, "Compatibility": "Alpha"})", false, nullptr, TlsConf(), one).empty());
	//not a string
	CHECK(message(one, R"({"Enabled": false, "Compatibility": 12})") == "\"TLS\": \"Compatibility\" must be a string");
	//a refused name does not apply anything
	{
		TlsConf c;
		TlsConf mine;
		mine.ciphers = "DUMMY-CIPHER";
		ParseTls(R"({"Enabled": false, "Compatibility": "Modern"})", false, &c, mine, two);
		CHECK(c.ciphers == "DUMMY-CIPHER");
	}
}

#ifndef ODC_ASIO_SSL

TEST_CASE(NEWSUITE("without TLS support in the build, enabling TLS is a clear error"))
{
	CHECK_FALSE(TlsSupported());
	const auto problems = ParseTls(R"({"Enabled": true, "CertFile": "a.pem", "KeyFile": "a.key", "CAFile": "ca.pem"})", true);
	REQUIRE(problems.size() == 1);
	CHECK(problems[0].find("this build has no TLS support: rebuild with ODC_ASIO_SSL=ON") != std::string::npos);
	//TLS disabled is fine in any build
	CHECK(ParseTls(R"({"Enabled": false})", true).empty());

	std::string error;
	TlsConf c;
	c.enabled = true;
	CHECK_FALSE(Context::Create(c, Role::Client, error));
	CHECK(error.find("this build has no TLS support") != std::string::npos);
}

#else

TEST_CASE(NEWSUITE("with TLS support: required keys, files and the context"))
{
	CHECK(TlsSupported());
	tlstest::TempDir dir;
	const auto ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
	const auto server = tlstest::MakeCert({"tls-server.test", {"IP:127.0.0.1"}, -3600, 86400LL * 365, false, 2}, &ca);
	const auto other = tlstest::MakeCert({"tls-other.test", {}, -3600, 86400LL * 365, false, 3}, &ca);
	const std::string caFile = dir.Write("ca.pem", ca.certPem);
	const std::string certFile = dir.Write("server.pem", server.certPem);
	const std::string keyFile = dir.Write("server.key", server.keyPem);
	const std::string otherKey = dir.Write("other.key", other.keyPem);

	//the section as a user would write it: enabled, with our identity and the CA
	auto section = [&](const std::string& extra)
			   {
				   Json::Value tls = Parse("{" + extra + "}");
				   tls["Enabled"] = true;
				   return tls;
			   };
	auto withFiles = [&](Json::Value tls)
			     {
				     tls["CertFile"] = certFile;
				     tls["KeyFile"] = keyFile;
				     tls["CAFile"] = caFile;
				     return tls;
			     };
	//parse, and make the context when the parse was clean, the way a port does
	auto load = [&](const Json::Value& tls, bool listener, std::shared_ptr<Context>* context = nullptr, TlsConf* conf = nullptr)
			{
				Json::Value root(Json::objectValue);
				root["TLS"] = tls;
				TlsConf c;
				std::vector<std::string> problems;
				ParseTlsConf(root, listener, c, problems);
				if(problems.empty() && c.enabled)
				{
					std::string error;
					auto ctx = Context::Create(c, listener ? Role::Server : Role::Client, error);
					if(!ctx)
						problems.push_back(error);
					if(context)
						*context = ctx;
				}
				if(conf)
					*conf = c;
				return problems;
			};

	//valid, for both directions; a TCP initiator needs no certificate of its own
	{
		std::shared_ptr<Context> ctx;
		const auto problems = load(withFiles(section("")), true, &ctx);
		INFO((problems.empty() ? std::string() : problems[0]));
		REQUIRE(problems.empty());
		REQUIRE(ctx);
		CHECK(ctx->GetRole() == Role::Server);
		CHECK(ctx->Conf().enabled);
	}
	{
		std::shared_ptr<Context> ctx;
		Json::Value tls = section("");
		tls["CAFile"] = caFile;
		REQUIRE(load(tls, false, &ctx).empty());
		REQUIRE(ctx);
		CHECK(ctx->GetRole() == Role::Client);
	}
	//errors
	{
		std::shared_ptr<Context> ctx;
		const auto errors = load(section(""), true, &ctx);
		CHECK(AnyContains(errors, "\"CertFile\" is required"));
		CHECK(AnyContains(errors, "\"KeyFile\" is required"));
		CHECK(AnyContains(errors, "\"CAFile\" (or \"CAPath\") is required"));
		CHECK_FALSE(ctx);
	}
	{
		Json::Value tls = withFiles(section(""));
		tls["KeyFile"] = otherKey;
		const auto errors = load(tls, true);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("do not belong together") != std::string::npos);
	}
	{
		Json::Value tls = withFiles(section(""));
		tls["CAFile"] = dir.Path("missing.pem");
		const auto errors = load(tls, true);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("\"CAFile\"") != std::string::npos);
		CHECK(errors[0].find("missing.pem") != std::string::npos);
	}
	{
		Json::Value tls = withFiles(section(""));
		tls["MinVersion"] = "1.3";
		tls["MaxVersion"] = "1.2";
		CHECK(AnyContains(load(tls, true), "\"MinVersion\" must not be later than \"MaxVersion\""));
	}
	{
		Json::Value tls = withFiles(section(""));
		tls["VerifyPeer"] = false;
		tls["PeerCertAllowList"] = Parse(R"(["CN:x"])");
		CHECK(AnyContains(load(tls, true), "\"PeerCertAllowList\" needs \"VerifyPeer\": true"));
		tls.removeMember("PeerCertAllowList");
		tls["CRLFile"] = certFile;
		CHECK(AnyContains(load(tls, true), "\"CRLFile\" needs \"VerifyPeer\": true"));
		tls.removeMember("CRLFile");
		tls["VerifyHostname"] = true;
		CHECK(AnyContains(load(tls, true), "\"VerifyHostname\" needs \"VerifyPeer\": true"));
	}
	{
		Json::Value tls = withFiles(section(""));
		tls["CRLCheckChain"] = true;
		CHECK(AnyContains(load(tls, true), "\"CRLCheckChain\" needs a \"CRLFile\""));
	}
	{
		//a malformed section does not create a context
		std::shared_ptr<Context> ctx;
		Json::Value tls = withFiles(section(""));
		tls["Bogus"] = 1;
		CHECK(AnyContains(load(tls, true, &ctx), "unknown key \"Bogus\""));
		CHECK_FALSE(ctx);
	}

	//the cipher lists: given empty is an error, not given leaves OpenSSL's defaults (a context is made)
	{
		Json::Value tls = withFiles(section(""));
		tls["Ciphers"] = "";
		CHECK(AnyContains(load(tls, true), "\"Ciphers\" must not be empty"));
		tls = withFiles(section(""));
		tls["CipherSuites13"] = "";
		CHECK(AnyContains(load(tls, true), "\"CipherSuites13\" must not be empty"));
		std::shared_ptr<Context> ctx;
		TlsConf c;
		REQUIRE(load(withFiles(section("")), true, &ctx, &c).empty());
		CHECK(c.ciphers.empty());
		CHECK(c.cipherSuites13.empty());
		REQUIRE(ctx);
	}

	//the session id context: a name of the port's own (at most 32 bytes), empty means the library's
	{
		TlsConf c;
		std::string error;
		c.enabled = true;
		c.certFile = certFile;
		c.keyFile = keyFile;
		c.caFile = caFile;
		CHECK(Context::Create(c, Role::Server, error));
		c.sessionIdContext = "a-port-of-its-own";
		CHECK(Context::Create(c, Role::Server, error));
		c.sessionIdContext = std::string(32, 'x');
		CHECK(Context::Create(c, Role::Server, error));
		c.sessionIdContext.clear();
		CHECK(Context::Create(c, Role::Server, error));
		c.sessionIdContext = std::string(33, 'x');
		error.clear();
		CHECK_FALSE(Context::Create(c, Role::Server, error));
		CHECK(error.find("sessionIdContext") != std::string::npos);
	}
}

#endif
