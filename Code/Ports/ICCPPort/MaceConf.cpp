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
 * MaceConf.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Parsing of the "MACE" section of the ICCP port configuration, see MaceConf.h.
 */

#include "MaceConf.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace iccp
{
namespace mace
{

bool MaceSupported()
{
	#ifdef ODC_ASIO_SSL
	return true;
	#else
	return false;
	#endif
}

std::string OidToString(const acse::Oid& oid)
{
	std::string text;
	for(const uint32_t arc : oid)
	{
		if(!text.empty())
			text += '.';
		text += std::to_string(arc);
	}
	return text;
}

namespace
{
const char* const Where = "\"MACE\": ";

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

bool HasControlChars(const std::string& s)
{
	return std::any_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20; });
}

bool ParseOid(const std::string& s, acse::Oid& out)
{
	out.clear();
	size_t pos = 0;
	while(pos <= s.size())
	{
		const size_t dot = s.find('.', pos);
		const std::string part = s.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
		if(part.empty() || part.size() > 9 || !std::all_of(part.begin(), part.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
			return false;
		out.push_back(static_cast<uint32_t>(std::stoul(part)));
		if(dot == std::string::npos)
			break;
		pos = dot + 1;
	}
	return out.size() >= 2 && out[0] <= 2 && (out[0] == 2 || out[1] < 40);
}

//the same syntax as the TLS "PeerCertAllowList": CN:name, SAN:name, SHA256:hex
bool ParseAllowEntry(const std::string& text, tls::AllowEntry& e, std::string& problem)
{
	const auto colon = text.find(':');
	if(colon == std::string::npos)
	{
		problem = "\"PeerCertAllowList\" entry \"" + text + "\" must start with CN:, SAN: or SHA256:";
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
		e.kind = tls::AllowEntry::Kind::CommonName;
		e.value = value;
	}
	else if(prefix == "SAN")
	{
		e.kind = tls::AllowEntry::Kind::San;
		e.value = value;
	}
	else if(prefix == "SHA256")
	{
		e.kind = tls::AllowEntry::Kind::Sha256;
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

void ParseMaceConf(const Json::Value& root, MaceConf& out, std::vector<std::string>& problems)
{
	out = MaceConf();
	if(!root.isObject() || !root.isMember("MACE"))
		return;
	Problems p(problems);
	const Json::Value& o = root["MACE"];
	if(!o.isObject())
	{
		p.Add("the \"MACE\" section must be a JSON object");
		return;
	}

	static const std::set<std::string> known = {"Enabled", "MechanismName", "CertFile", "KeyFile", "KeyPasswordEnv", "CAFile", "CAPath", "CRLFile",
		                                      "CRLCheckChain", "PeerCertAllowList", "TimeWindowSeconds", "MinKeyBits", "MaxKeyBits", "RequireAuthentication",
		                                      "RequireResponderAuthentication", "VerifyPeer"};
	for(const auto& key : o.getMemberNames())
	{
		if(key == "KeyPassword")
			p.Add("\"KeyPassword\" is not supported: do not keep a secret in the configuration; put the password in an environment variable and name it with \"KeyPasswordEnv\"");
		else if(known.count(key) == 0)
			p.Add("unknown key \"" + key + "\"");
	}

	GetBool(o, "Enabled", out.enabled, p);
	GetString(o, "MechanismName", out.mechanismText, p);
	GetString(o, "CertFile", out.certFile, p);
	GetString(o, "KeyFile", out.keyFile, p);
	GetString(o, "KeyPasswordEnv", out.keyPasswordEnv, p);
	GetString(o, "CAFile", out.caFile, p);
	GetString(o, "CAPath", out.caPath, p);
	GetString(o, "CRLFile", out.crlFile, p);
	GetBool(o, "CRLCheckChain", out.crlCheckChain, p);
	GetUInt(o, "TimeWindowSeconds", out.timeWindowSeconds, p, 1, 86400);
	GetUInt(o, "MinKeyBits", out.minKeyBits, p, 1024, AbsoluteMaxKeyBits);
	GetUInt(o, "MaxKeyBits", out.maxKeyBits, p, 1024, AbsoluteMaxKeyBits);
	GetBool(o, "RequireAuthentication", out.requireAuthentication, p);
	GetBool(o, "RequireResponderAuthentication", out.requireResponderAuthentication, p);
	GetBool(o, "VerifyPeer", out.verifyPeer, p);

	for(const char* key : {"MechanismName", "CertFile", "KeyFile", "KeyPasswordEnv", "CAFile", "CAPath", "CRLFile"})
		if(o.isMember(key) && o[key].isString() && HasControlChars(o[key].asString()))
			p.Add(std::string("\"") + key + "\" contains control characters");

	if(o.isMember("PeerCertAllowList"))
	{
		if(!o["PeerCertAllowList"].isArray())
			p.Add("\"PeerCertAllowList\" must be an array of strings (CN:name, SAN:name or SHA256:hex)");
		else
			for(const auto& item : o["PeerCertAllowList"])
			{
				tls::AllowEntry e;
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

	// ---- the rest only matters when MACE is enabled
	if(!MaceSupported())
	{
		p.Add("this build has no MACE support: rebuild with ODC_ASIO_SSL=ON (and OpenSSL) to use \"Enabled\": true");
		return;
	}
	if(o.isMember("MechanismName") && !ParseOid(out.mechanismText, out.mechanismName))
		p.Add(std::string("\"MechanismName\" must be a dotted object identifier such as ") + OfficialMechanismText + " (the default; some peers use " + LegacyMechanismText + ")");
	if(out.certFile.empty())
		p.Add("\"CertFile\" is required: our certificate is sent to the peer (as initiator in the association request, as responder in the response)");
	if(out.keyFile.empty())
		p.Add("\"KeyFile\" is required: the signing time is signed with our private key");
	if(out.minKeyBits > out.maxKeyBits)
		p.Add("\"MinKeyBits\" must not exceed \"MaxKeyBits\"");
	if(out.verifyPeer && out.caFile.empty() && out.caPath.empty())
		p.Add("\"CAFile\" (or \"CAPath\") is required to verify the peer's certificate; set \"VerifyPeer\": false only for testing");
	if(!out.verifyPeer && !out.peerCertAllowList.empty())
		p.Add("\"PeerCertAllowList\" needs \"VerifyPeer\": true");
	if(!out.verifyPeer && !out.crlFile.empty())
		p.Add("\"CRLFile\" needs \"VerifyPeer\": true");
	if(out.crlCheckChain && out.crlFile.empty())
		p.Add("\"CRLCheckChain\" needs a \"CRLFile\"");
}

} //namespace mace
} //namespace iccp
