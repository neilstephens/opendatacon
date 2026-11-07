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
 *  The "TLS" section of the ICCP port configuration: what the port passes to the generic TLS parser (the default cipher lists, the Legacy12
 *  preset, the session id context), the default port, and the errors when the build has no TLS support or the files are wrong. The generic
 *  parser itself is tested with the ODC library (Code/tests/ODC_tests). These run in every build: without OpenSSL (ODC_ASIO_SSL off)
 *  enabling TLS must be a clear configuration error, with it the files are checked too.
 */

#include "ICCPPortConf.h"
#include "IccpTlsDefaults.h"
#include "TlsTestPki.h"
#include <catch.hpp>
#include <sstream>

#define SUITE(name) "ICCP TLS configuration - " name

using namespace iccp::tls;

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

[[maybe_unused]] bool AnyContains(const std::vector<std::string>& list, const std::string& part)
{
	for(const auto& s : list)
		if(s.find(part) != std::string::npos)
			return true;
	return false;
}

#ifndef ODC_ASIO_SSL
std::vector<std::string> ParseTls(const std::string& tlsJson, bool listener, TlsConf* out = nullptr)
{
	TlsConf c;
	std::vector<std::string> problems;
	ParseTlsConf(Parse("{\"TLS\": " + tlsJson + "}"), listener, c, problems);
	if(out)
		*out = c;
	return problems;
}
#endif
}

//What an ICCP port configuration yields, written out literally so that it pins the behaviour whatever the TLS parser looks like inside: the default
//cipher lists, the Legacy12 preset and the messages. A disabled section is parsed in full, in every build. (The generic parser is tested in ODC_tests.)
TEST_CASE(SUITE("an ICCP port configuration gets the ICCP defaults and the Legacy12 preset"))
{
	const std::string defaultCiphers = "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:"
	                                   "ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256";
	const std::string legacyCiphers = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:"
	                                  "AES256-GCM-SHA384:AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256:"
	                                  "DHE-RSA-AES128-SHA256:AES256-SHA256:AES128-SHA256:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:"
	                                  "DHE-RSA-AES128-SHA:AES256-SHA:AES128-SHA";
	const std::string defaultSuites13 = "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256";

	//the constants the port keeps are those values
	CHECK(std::string(iccp::DefaultCiphers12) == defaultCiphers);
	CHECK(std::string(iccp::DefaultCipherSuites13) == defaultSuites13);
	CHECK(std::string(iccp::LegacyCiphers12) == legacyCiphers);
	CHECK(std::string(iccp::LegacyGroups) == "P-256:P-384:P-521");
	CHECK(std::string(iccp::DefaultSecurePort) == "3782");
	CHECK(iccp::IccpTlsPresets().size() == 1);
	CHECK(iccp::IccpTlsPresets()[0].name == "Legacy12");
	auto load = [&](const std::string& tlsJson, std::vector<std::string>* errors = nullptr)
			{
				ICCPPortConf c;
				const auto e = c.Load(Parse(tlsJson.empty() ? "{}" : "{\"TLS\": " + tlsJson + "}"), false);
				if(errors)
					*errors = e;
				return c.tlsConf;
			};

	//no TLS section, a disabled one, and the explicit "Default" preset: the ICCP default lists, nothing else changed
	for(const std::string json : {std::string(), std::string(R"({"Enabled": false})"), std::string(R"({"Enabled": false, "Compatibility": "Default"})")})
	{
		INFO(json);
		const TlsConf c = load(json);
		CHECK_FALSE(c.enabled);
		CHECK(c.ciphers == defaultCiphers);
		CHECK(c.cipherSuites13 == defaultSuites13);
		CHECK(c.groups.empty());
		CHECK(c.signatureAlgorithms.empty());
		CHECK(c.compatibility == "Default");
		CHECK(c.minVersion == 12);
		CHECK(c.maxVersion == 13);
		CHECK(c.sendSni);
		CHECK(c.sessionTickets);
		CHECK(c.extendedMasterSecret);
		CHECK_FALSE(c.sessionResumption);
		CHECK(c.securityLevel == -1);
		CHECK(c.sessionIdContext == "opendatacon-iccp"); //as before the TLS filter moved to the library: sessions are resumed only between ICCP ports
	}

	//Legacy12: TLS 1.2 only, no tickets, no SNI, the NIST curves, RSA based suites; the TLS 1.3 suites are not touched
	{
		const TlsConf c = load(R"({"Enabled": false, "Compatibility": "Legacy12"})");
		CHECK(c.compatibility == "Legacy12");
		CHECK(c.minVersion == 12);
		CHECK(c.maxVersion == 12);
		CHECK_FALSE(c.sessionTickets);
		CHECK_FALSE(c.sendSni);
		CHECK(c.groups == "P-256:P-384:P-521");
		CHECK(c.ciphers == legacyCiphers);
		CHECK(c.cipherSuites13 == defaultSuites13);
		CHECK(c.extendedMasterSecret);
		CHECK_FALSE(c.sessionResumption);
	}
	//explicit keys override the preset, one by one
	{
		const TlsConf c = load(R"({"Enabled": false, "Compatibility": "Legacy12", "MaxVersion": "1.3", "SendSNI": true, "Groups": "X25519", "SessionTickets": true,
			"Ciphers": "ECDHE-RSA-AES128-GCM-SHA256"})");
		CHECK(c.minVersion == 12);
		CHECK(c.maxVersion == 13);
		CHECK(c.sendSni);
		CHECK(c.sessionTickets);
		CHECK(c.groups == "X25519");
		CHECK(c.ciphers == "ECDHE-RSA-AES128-GCM-SHA256");
	}
	{
		const TlsConf c = load(R"({"Enabled": false, "Compatibility": "Legacy12", "MinVersion": "1.3", "MaxVersion": "1.3"})");
		CHECK(c.minVersion == 13);
		CHECK(c.maxVersion == 13);
		CHECK_FALSE(c.sendSni); //not given: the preset
	}
	//an unknown name is refused with this text, and a name that is not a string
	{
		std::vector<std::string> errors;
		load(R"({"Enabled": false, "Compatibility": "Modern"})", &errors);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("\"TLS\": \"Compatibility\" must be \"Default\" or \"Legacy12\"") != std::string::npos);
		load(R"({"Enabled": false, "Compatibility": 12})", &errors);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("\"TLS\": \"Compatibility\" must be a string") != std::string::npos);
	}
	//the lists are still checked when they are given empty
	#ifdef ODC_ASIO_SSL
	{
		std::vector<std::string> errors;
		load(R"({"Enabled": true, "CAFile": "x", "Ciphers": ""})", &errors);
		CHECK(AnyContains(errors, "\"Ciphers\" must not be empty"));
		load(R"({"Enabled": true, "CAFile": "x", "CipherSuites13": ""})", &errors);
		CHECK(AnyContains(errors, "\"CipherSuites13\" must not be empty"));
	}
	#endif
}

#ifndef ODC_ASIO_SSL

TEST_CASE(SUITE("without TLS support in the build, enabling TLS is a clear error"))
{
	CHECK_FALSE(TlsSupported());
	const auto problems = ParseTls(R"({"Enabled": true, "CertFile": "a.pem", "KeyFile": "a.key", "CAFile": "ca.pem"})", true);
	REQUIRE(problems.size() == 1);
	CHECK(problems[0].find("this build has no TLS support: rebuild with ODC_ASIO_SSL=ON") != std::string::npos);

	//through the port configuration, for both port types, and the port is then invalid
	for(bool listener : {true, false})
	{
		ICCPPortConf conf;
		const auto errors = conf.Load(Parse(R"({"TLS": {"Enabled": true, "CertFile": "a.pem", "KeyFile": "a.key", "CAFile": "ca.pem"}})"), listener);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("rebuild with ODC_ASIO_SSL=ON") != std::string::npos);
		CHECK_FALSE(conf.tlsContext);
	}
	//TLS disabled is fine in any build
	ICCPPortConf conf;
	CHECK(conf.Load(Parse(R"({"TLS": {"Enabled": false}})"), true).empty());
}

#else

TEST_CASE(SUITE("with TLS support: required keys, files and the default port"))
{
	CHECK(TlsSupported());
	tlstest::TempDir dir;
	const auto ca = tlstest::MakeCert({"Test CA", {}, -3600, 86400LL * 3650, true, 1});
	const auto server = tlstest::MakeCert({"iccp-server.test", {"IP:127.0.0.1"}, -3600, 86400LL * 365, false, 2}, &ca);
	const auto other = tlstest::MakeCert({"iccp-other.test", {}, -3600, 86400LL * 365, false, 3}, &ca);
	const std::string caFile = dir.Write("ca.pem", ca.certPem);
	const std::string certFile = dir.Write("server.pem", server.certPem);
	const std::string keyFile = dir.Write("server.key", server.keyPem);
	const std::string otherKey = dir.Write("other.key", other.keyPem);

	auto conf = [&](const std::string& extra, bool port)
			{
				Json::Value root = Parse(port ? R"({"Port": 4000})" : "{}");
				Json::Value tls = Parse("{" + extra + "}");
				tls["Enabled"] = true;
				root["TLS"] = tls;
				return root;
			};
	auto withFiles = [&](Json::Value root)
			     {
				     root["TLS"]["CertFile"] = certFile;
				     root["TLS"]["KeyFile"] = keyFile;
				     root["TLS"]["CAFile"] = caFile;
				     return root;
			     };

	//valid: the TLS port is the default only when "Port" is not given
	{
		ICCPPortConf c;
		const auto errors = c.Load(withFiles(conf("", false)), true);
		INFO((errors.empty() ? std::string() : errors[0]));
		REQUIRE(errors.empty());
		CHECK(c.port == "3782");
		CHECK(c.tlsConf.enabled);
		CHECK(c.tlsContext);
		//the same object, loaded again without TLS: back to the plain default
		CHECK(c.Load(Parse("{}"), true).empty());
		CHECK(c.port == "102");
		CHECK_FALSE(c.tlsContext);
	}
	{
		ICCPPortConf c;
		REQUIRE(c.Load(withFiles(conf("", true)), true).empty());
		CHECK(c.port == "4000"); //the explicit port wins
	}
	{
		ICCPPortConf c;
		REQUIRE(c.Load(withFiles(conf("", false)), false).empty()); //the TCP initiator, too
		CHECK(c.port == "3782");
	}
	//a TCP initiator needs no certificate of its own (and then does not present one)
	{
		ICCPPortConf c;
		Json::Value root = conf("", false);
		root["TLS"]["CAFile"] = caFile;
		CHECK(c.Load(root, false).empty());
	}

	//errors
	{
		ICCPPortConf c;
		const auto errors = c.Load(conf("", false), true);
		CHECK(AnyContains(errors, "\"CertFile\" is required"));
		CHECK(AnyContains(errors, "\"KeyFile\" is required"));
		CHECK(AnyContains(errors, "\"CAFile\" (or \"CAPath\") is required"));
		CHECK_FALSE(c.tlsContext);
	}
	{
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["KeyFile"] = otherKey;
		const auto errors = c.Load(root, true);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("do not belong together") != std::string::npos);
	}
	{
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["CAFile"] = dir.Path("missing.pem");
		const auto errors = c.Load(root, true);
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].find("\"CAFile\"") != std::string::npos);
		CHECK(errors[0].find("missing.pem") != std::string::npos);
	}
	{
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["MinVersion"] = "1.3";
		root["TLS"]["MaxVersion"] = "1.2";
		CHECK(AnyContains(c.Load(root, true), "\"MinVersion\" must not be later than \"MaxVersion\""));
	}
	{
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["VerifyPeer"] = false;
		root["TLS"]["PeerCertAllowList"] = Parse(R"(["CN:x"])");
		CHECK(AnyContains(c.Load(root, true), "\"PeerCertAllowList\" needs \"VerifyPeer\": true"));
		root["TLS"].removeMember("PeerCertAllowList");
		root["TLS"]["CRLFile"] = certFile;
		CHECK(AnyContains(c.Load(root, true), "\"CRLFile\" needs \"VerifyPeer\": true"));
		root["TLS"].removeMember("CRLFile");
		root["TLS"]["VerifyHostname"] = true;
		CHECK(AnyContains(c.Load(root, true), "\"VerifyHostname\" needs \"VerifyPeer\": true"));
	}
	{
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["CRLCheckChain"] = true;
		CHECK(AnyContains(c.Load(root, true), "\"CRLCheckChain\" needs a \"CRLFile\""));
	}
	{
		//a malformed section does not stop the rest of the configuration being checked, and does not create a context
		ICCPPortConf c;
		Json::Value root = withFiles(conf("", false));
		root["TLS"]["Bogus"] = 1;
		root["Port"] = 70000;
		const auto errors = c.Load(root, true);
		CHECK(AnyContains(errors, "unknown key \"Bogus\""));
		CHECK(AnyContains(errors, "\"Port\""));
		CHECK_FALSE(c.tlsContext);
	}
}

#endif
