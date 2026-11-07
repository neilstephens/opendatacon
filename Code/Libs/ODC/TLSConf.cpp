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
 * TLSConf.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Parsing of the "TLS" section of a port configuration, see TLSConf.h.
 */

#include <algorithm>
#include <cctype>
#include <opendatacon/TLSConf.h>
#include <set>

namespace odc
{
namespace tls
{

bool TlsSupported()
{
	#ifdef ODC_ASIO_SSL
	return true;
	#else
	return false;
	#endif
}

namespace
{
const char* const Where = "\"TLS\": ";

class Problems
{
public:
	explicit Problems(std::vector<std::string>& l): list(l)
	{}
	void Add(const std::string& s)
	{
		list.push_back(Where + s);
	}
	std::vector<std::string>& list;
};

bool GetString(const Json::Value& o, const char* key, std::string& out, Problems& p)
{
	if(!o.isMember(key))
		return false;
	if(!o[key].isString())
	{
		p.Add(std::string("\"") + key + "\" must be a string");
		return false;
	}
	out = o[key].asString();
	return true;
}

bool GetBool(const Json::Value& o, const char* key, bool& out, Problems& p)
{
	if(!o.isMember(key))
		return false;
	if(!o[key].isBool())
	{
		p.Add(std::string("\"") + key + "\" must be true or false");
		return false;
	}
	out = o[key].asBool();
	return true;
}

bool GetUInt(const Json::Value& o, const char* key, uint32_t& out, Problems& p, int64_t min, int64_t max)
{
	if(!o.isMember(key))
		return false;
	if(!(o[key].isInt64() || o[key].isUInt64()))
	{
		p.Add(std::string("\"") + key + "\" must be an integer");
		return false;
	}
	const int64_t v = o[key].isUInt64() && o[key].asUInt64() > static_cast<uint64_t>(INT64_MAX) ? INT64_MAX : o[key].asInt64();
	if(v < min || v > max)
	{
		p.Add(std::string("\"") + key + "\" must be between " + std::to_string(min) + " and " + std::to_string(max));
		return false;
	}
	out = static_cast<uint32_t>(v);
	return true;
}

bool GetVersion(const Json::Value& o, const char* key, int& out, Problems& p)
{
	std::string s;
	if(!GetString(o, key, s, p))
		return false;
	if(s == "1.2")
		out = 12;
	else if(s == "1.3")
		out = 13;
	else
	{
		p.Add(std::string("\"") + key + "\" must be \"1.2\" or \"1.3\" (older protocol versions are not supported)");
		return false;
	}
	return true;
}

bool HasControlChars(const std::string& s)
{
	return std::any_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20; });
}

bool ParseAllowEntry(const std::string& text, AllowEntry& e, std::string& problem)
{
	const auto colon = text.find(':');
	if(colon == std::string::npos)
	{
		problem = "\"PeerCertAllowList\" entry \"" + text + "\" must start with CN:, SAN: or SHA256: (for example \"CN=...\" is not accepted, use \"CN:name\")";
		return false;
	}
	const std::string prefix = text.substr(0, colon);
	std::string value = text.substr(colon + 1);
	e.text = text;
	if(value.empty() || HasControlChars(value))
	{
		problem = "\"PeerCertAllowList\" entry \"" + text + "\" has an empty or invalid value";
		return false;
	}
	if(prefix == "CN")
	{
		e.kind = AllowEntry::Kind::CommonName;
		e.value = value;
	}
	else if(prefix == "SAN")
	{
		e.kind = AllowEntry::Kind::San;
		e.value = value;
	}
	else if(prefix == "SHA256")
	{
		e.kind = AllowEntry::Kind::Sha256;
		value.erase(std::remove(value.begin(), value.end(), ':'), value.end());
		if(value.size() != 64 || !std::all_of(value.begin(), value.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }))
		{
			problem = "\"PeerCertAllowList\" entry \"" + text + "\": a SHA256 fingerprint is 64 hexadecimal digits (colons are optional)";
			return false;
		}
		std::transform(value.begin(), value.end(), value.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
		e.value = value;
	}
	else
	{
		problem = "\"PeerCertAllowList\" entry \"" + text + "\" must start with CN:, SAN: or SHA256:";
		return false;
	}
	return true;
}
}

void ParseTlsConf(const Json::Value& root, bool listener, TlsConf& out, std::vector<std::string>& problems, const TlsConf& defaults, const PresetTable& presets)
{
	{
		const TlsConf start = defaults; //a copy: out may be the object defaults was taken from
		out = start;
	}
	if(!root.isObject() || !root.isMember("TLS"))
		return;
	Problems p(problems);
	const Json::Value& o = root["TLS"];
	if(!o.isObject())
	{
		p.Add("the \"TLS\" section must be a JSON object");
		return;
	}

	static const std::set<std::string> known = {"Enabled", "CertFile", "KeyFile", "KeyPasswordEnv", "CAFile", "CAPath", "CRLFile", "CRLCheckChain", "VerifyPeer",
		                                      "RequireClientCert", "MinVersion", "MaxVersion", "Ciphers", "CipherSuites13", "PeerCertAllowList", "VerifyHostname", "ServerName",
		                                      "HandshakeTimeoutms", "RenegotiateIntervalSeconds", "SessionResumption", "CertExpiryWarningDays", "Groups",
		                                      "SignatureAlgorithms", "SendSNI", "ExtendedMasterSecret", "SessionTickets", "SecurityLevel", "Compatibility", "KeyLogFile"};
	for(const auto& key : o.getMemberNames())
	{
		if(key == "KeyPassword")
			p.Add("\"KeyPassword\" is not supported: do not keep a secret in the configuration; put the password in an environment variable and name it with \"KeyPasswordEnv\"");
		else if(known.count(key) == 0)
			p.Add("unknown key \"" + key + "\"");
	}

	GetBool(o, "Enabled", out.enabled, p);
	GetString(o, "CertFile", out.certFile, p);
	GetString(o, "KeyFile", out.keyFile, p);
	GetString(o, "KeyPasswordEnv", out.keyPasswordEnv, p);
	GetString(o, "CAFile", out.caFile, p);
	GetString(o, "CAPath", out.caPath, p);
	GetString(o, "CRLFile", out.crlFile, p);
	GetBool(o, "CRLCheckChain", out.crlCheckChain, p);
	GetBool(o, "VerifyPeer", out.verifyPeer, p);
	GetBool(o, "RequireClientCert", out.requireClientCert, p);
	GetVersion(o, "MinVersion", out.minVersion, p);
	GetVersion(o, "MaxVersion", out.maxVersion, p);
	GetString(o, "Ciphers", out.ciphers, p);
	GetString(o, "CipherSuites13", out.cipherSuites13, p);
	GetBool(o, "VerifyHostname", out.verifyHostname, p);
	GetString(o, "ServerName", out.serverName, p);
	GetUInt(o, "HandshakeTimeoutms", out.handshakeTimeoutMs, p, 100, 600000);
	GetUInt(o, "RenegotiateIntervalSeconds", out.renegotiateIntervalSeconds, p, 0, 31536000);
	GetBool(o, "SessionResumption", out.sessionResumption, p);
	GetUInt(o, "CertExpiryWarningDays", out.certExpiryWarningDays, p, 0, 36500);
	GetString(o, "KeyLogFile", out.keyLogFile, p);
	GetString(o, "Groups", out.groups, p);
	GetString(o, "SignatureAlgorithms", out.signatureAlgorithms, p);
	GetBool(o, "SendSNI", out.sendSni, p);
	GetBool(o, "ExtendedMasterSecret", out.extendedMasterSecret, p);
	GetBool(o, "SessionTickets", out.sessionTickets, p);
	{
		uint32_t level = 0;
		if(GetUInt(o, "SecurityLevel", level, p, 0, 5))
			out.securityLevel = static_cast<int>(level);
	}
	const Preset* preset = nullptr;
	if(GetString(o, "Compatibility", out.compatibility, p))
	{
		if(out.compatibility != "Default")
		{
			const auto it = std::find_if(presets.begin(), presets.end(), [&](const Preset& s) { return s.name == out.compatibility; });
			if(it != presets.end())
				preset = &*it;
			else
			{
				//"Default" or "A" or "B": the names come from the table
				std::string names = "\"Default\"";
				for(size_t i = 0; i < presets.size(); ++i)
					names += (i + 1 == presets.size() ? " or \"" : ", \"") + presets[i].name + "\"";
				p.Add("\"Compatibility\" must be " + names);
			}
		}
	}
	if(preset && preset->apply)
	{
		//the preset fills in what the configuration does not say
		preset->apply(out, [&o](const char* key) { return o.isMember(key); });
	}

	for(const char* key : {"CertFile", "KeyFile", "KeyPasswordEnv", "CAFile", "CAPath", "CRLFile", "Ciphers", "CipherSuites13", "ServerName", "Groups", "SignatureAlgorithms", "KeyLogFile"})
		if(o.isMember(key) && o[key].isString() && HasControlChars(o[key].asString()))
			p.Add(std::string("\"") + key + "\" contains control characters");

	if(o.isMember("PeerCertAllowList"))
	{
		if(!o["PeerCertAllowList"].isArray())
			p.Add("\"PeerCertAllowList\" must be an array of strings (CN:name, SAN:name or SHA256:hex)");
		else
			for(const auto& item : o["PeerCertAllowList"])
			{
				AllowEntry e;
				std::string problem;
				if(!item.isString())
					p.Add("\"PeerCertAllowList\" entries must be strings (CN:name, SAN:name or SHA256:hex)");
				else if(!ParseAllowEntry(item.asString(), e, problem))
					p.Add(problem);
				else
					out.peerCertAllowList.push_back(e);
			}
	}

	if(!out.enabled)
		return;

	// ---- the rest only matters when TLS is enabled
	if(!TlsSupported())
	{
		p.Add("this build has no TLS support: rebuild with ODC_ASIO_SSL=ON (and OpenSSL) to use \"Enabled\": true");
		return;
	}
	if(out.minVersion > out.maxVersion)
		p.Add("\"MinVersion\" must not be later than \"MaxVersion\"");
	if(listener)
	{
		if(out.certFile.empty())
			p.Add("\"CertFile\" is required: this port is the TLS server (the TCP listener) and must present a certificate");
		if(out.keyFile.empty())
			p.Add("\"KeyFile\" is required: this port is the TLS server (the TCP listener) and must have a private key");
	}
	else if(out.certFile.empty() != out.keyFile.empty())
		p.Add("\"CertFile\" and \"KeyFile\" must be given together");
	if(out.verifyPeer && out.caFile.empty() && out.caPath.empty())
		p.Add("\"CAFile\" (or \"CAPath\") is required to verify the peer's certificate; set \"VerifyPeer\": false only for testing");
	if(!out.verifyPeer && !out.peerCertAllowList.empty())
		p.Add("\"PeerCertAllowList\" needs \"VerifyPeer\": true");
	if(!out.verifyPeer && !out.crlFile.empty())
		p.Add("\"CRLFile\" needs \"VerifyPeer\": true");
	if(out.crlCheckChain && out.crlFile.empty())
		p.Add("\"CRLCheckChain\" needs a \"CRLFile\"");
	if(out.verifyHostname && !out.verifyPeer)
		p.Add("\"VerifyHostname\" needs \"VerifyPeer\": true");
	if(o.isMember("Ciphers") && out.ciphers.empty())
		p.Add("\"Ciphers\" must not be empty");
	if(o.isMember("CipherSuites13") && out.cipherSuites13.empty())
		p.Add("\"CipherSuites13\" must not be empty");
	if(o.isMember("Groups") && out.groups.empty())
		p.Add("\"Groups\" must not be empty (OpenSSL syntax, for example \"P-256:P-384\"); leave it out for the library's default");
	if(o.isMember("SignatureAlgorithms") && out.signatureAlgorithms.empty())
		p.Add("\"SignatureAlgorithms\" must not be empty (OpenSSL syntax, for example \"RSA+SHA256:ECDSA+SHA256\"); leave it out for the library's default");
}

} //namespace tls
} //namespace odc
