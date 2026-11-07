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
 * ICCPPortConf.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  JSON configuration of the ICCP ports.
 */

#include "ICCPPortConf.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

using namespace iccp;

namespace
{
class Problems
{
public:
	std::vector<std::string> list;
	void Add(const std::string& s)
	{
		list.push_back(s);
	}
};

bool IsInt(const Json::Value& v)
{
	return v.isInt64() || v.isUInt64();
}

//a member that, if present, must be of the right type
bool GetString(const Json::Value& root, const char* key, std::string& out, Problems& p)
{
	if(!root.isMember(key))
		return false;
	if(!root[key].isString())
	{
		p.Add(std::string("\"") + key + "\" must be a string");
		return false;
	}
	out = root[key].asString();
	return true;
}

bool GetInt(const Json::Value& root, const char* key, int64_t& out, Problems& p, int64_t min, int64_t max, const std::string& where = "")
{
	if(!root.isMember(key))
		return false;
	if(!IsInt(root[key]))
	{
		p.Add(where + "\"" + key + "\" must be an integer");
		return false;
	}
	const int64_t v = root[key].asInt64();
	if(v < min || v > max)
	{
		p.Add(where + "\"" + key + "\" must be between " + std::to_string(min) + " and " + std::to_string(max));
		return false;
	}
	out = v;
	return true;
}

bool GetBool(const Json::Value& root, const char* key, bool& out, Problems& p, const std::string& where = "")
{
	if(!root.isMember(key))
		return false;
	if(!root[key].isBool())
	{
		p.Add(where + "\"" + key + "\" must be true or false");
		return false;
	}
	out = root[key].asBool();
	return true;
}

bool GetDouble(const Json::Value& root, const char* key, double& out, Problems& p, const std::string& where)
{
	if(!root.isMember(key))
		return false;
	if(!root[key].isNumeric())
	{
		p.Add(where + "\"" + key + "\" must be a number");
		return false;
	}
	out = root[key].asDouble();
	return true;
}

bool ParseHex(const std::string& s, size_t maxOctets, ber::Bytes& out)
{
	if(s.size() % 2 != 0 || s.size() / 2 > maxOctets)
		return false;
	out.clear();
	for(size_t i = 0; i < s.size(); i += 2)
	{
		if(!std::isxdigit(static_cast<unsigned char>(s[i])) || !std::isxdigit(static_cast<unsigned char>(s[i + 1])))
			return false;
		out.push_back(static_cast<uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
	}
	return true;
}

void GetSelector(const Json::Value& root, const char* key, size_t maxOctets, ber::Bytes& out, Problems& p)
{
	std::string s;
	if(!GetString(root, key, s, p))
		return;
	if(!ParseHex(s, maxOctets, out))
		p.Add(std::string("\"") + key + "\" must be hexadecimal digits (an even number, at most " + std::to_string(maxOctets) + " octets)");
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

void GetOid(const Json::Value& root, const char* key, acse::Oid& out, Problems& p)
{
	std::string s;
	if(!GetString(root, key, s, p))
		return;
	if(!ParseOid(s, out))
		p.Add(std::string("\"") + key + "\" must be a dotted object identifier such as 1.3.9999.1.2");
}

bool ValidIdentifier(const std::string& s)
{
	return !s.empty() && s.size() <= iccp::mms::MaxIdentifierLength;
}

const std::map<std::string, tase2::PointType>& PointTypeNames()
{
	using tase2::PointType;
	static const std::map<std::string, PointType> names = {
		{"Real", PointType::Real}, {"RealQ", PointType::RealQ}, {"RealQTimeTag", PointType::RealQTimeTag}, {"RealExtended", PointType::RealExtended},
		{"State", PointType::State}, {"StateQ", PointType::StateQ}, {"StateQTimeTag", PointType::StateQTimeTag}, {"StateExtended", PointType::StateExtended},
		{"Discrete", PointType::Discrete}, {"DiscreteQ", PointType::DiscreteQ}, {"DiscreteQTimeTag", PointType::DiscreteQTimeTag},
		{"DiscreteExtended", PointType::DiscreteExtended}
	};
	return names;
}

const std::map<std::string, uint8_t>& ConditionNames()
{
	using namespace tase2::DsCondition;
	static const std::map<std::string, uint8_t> names = {
		{"IntervalTimeOut", IntervalTimeOut}, {"IntegrityTimeOut", IntegrityTimeOut},
		{"ObjectChange", ObjectChange}, {"OperatorRequest", OperatorRequest}, {"OtherExternalEvent", OtherExternalEvent}
	};
	return names;
}

const std::map<std::string, tase2::Special>& SpecialNames()
{
	using tase2::Special;
	static const std::map<std::string, Special> names = {
		{tase2::Name::TransferSetName, Special::TransferSetName},
		{tase2::Name::TransferSetTimeStamp, Special::TimeStamp}, {tase2::Name::DsConditionsDetected, Special::ConditionsDetected},
		{tase2::Name::EventCodeDetected, Special::EventCode}
	};
	return names;
}

bool TypeFits(PointKind kind, tase2::PointType t)
{
	switch(kind)
	{
		case PointKind::Binary: return tase2::IsStateType(t);
		case PointKind::Analog: return tase2::IsRealType(t) || tase2::IsDiscreteType(t);
		case PointKind::Counter: return tase2::IsDiscreteType(t);
	}
	return false;
}

const char* KindName(PointKind k)
{
	switch(k)
	{
		case PointKind::Binary: return "Binaries";
		case PointKind::Analog: return "Analogs";
		case PointKind::Counter: return "Counters";
	}
	return "?";
}
}

std::vector<std::string> ICCPPortConf::Load(const Json::Value& root, bool listener)
{
	Problems p;
	tcpListener = listener;
	points.clear();
	controls.clear();
	supportedBlocks = {1, 2};
	requiredRemoteBlocks = {1};
	dataSets.clear();
	transferSets.clear();
	transferSetPool.clear();
	warnings.clear();
	if(!root.isObject())
	{
		p.Add("the configuration must be a JSON object");
		return p.list;
	}

	// ---- connection
	GetString(root, "IP", ip, p);
	if(root.isMember("Port"))
	{
		//accept a number or a string
		int64_t n = 0;
		if(IsInt(root["Port"]))
			n = root["Port"].asInt64();
		else if(root["Port"].isString())
		{
			const std::string portStr = root["Port"].asString();
			if(!portStr.empty() && portStr.size() <= 5 &&
			   std::all_of(portStr.begin(), portStr.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
				n = std::stoll(portStr);
		}
		if(n < 1 || n > 65535)
			p.Add("\"Port\" must be a TCP port number between 1 and 65535");
		else
			port = std::to_string(n);
	}
	int64_t v = 0;
	if(GetInt(root, "TCPConnectRetryPeriodms", v, p, 0, 65535))
		tcpConnectRetryMs = static_cast<uint16_t>(v);
	if(GetInt(root, "TCPThrottleBitrate", v, p, 0, INT64_MAX))
		tcpThrottleBitrate = static_cast<uint64_t>(v);
	if(GetInt(root, "TCPThrottleChunksize", v, p, 0, INT64_MAX))
		tcpThrottleChunksize = static_cast<uint64_t>(v);
	if(GetInt(root, "TCPThrottleWriteDelayms", v, p, 0, INT64_MAX))
		tcpThrottleWriteDelayMs = static_cast<uint64_t>(v);

	// ---- secure ICCP (TLS, IEC 62351-3); the default port with TLS is 3782
	{
		const size_t problemsBefore = p.list.size();
		tls::ParseTlsConf(root, listener, tlsConf, p.list, IccpTlsDefaults(), IccpTlsPresets());
		tlsContext.reset();
		if(!root.isMember("Port"))
			port = tlsConf.enabled ? DefaultSecurePort : "102";
		if(tlsConf.enabled && p.list.size() == problemsBefore)
		{
			std::string tlsError;
			tlsContext = tls::Context::Create(tlsConf, listener ? tls::Role::Server : tls::Role::Client, tlsError);
			if(!tlsContext)
				p.Add(tlsError);
		}
	}

	// ---- authentication in the ACSE association (MACE); independent of TLS
	{
		const size_t problemsBefore = p.list.size();
		mace::ParseMaceConf(root, maceConf, p.list);
		maceContext.reset();
		if(maceConf.enabled && p.list.size() == problemsBefore)
		{
			std::string maceError;
			maceContext = mace::Context::Create(maceConf, maceError);
			if(!maceContext)
				p.Add(maceError);
		}
		if(maceConf.enabled && p.list.size() == problemsBefore && maceConf.mechanismName != mace::OfficialMechanismName)
			warnings.push_back("\"MACE\": the configured \"MechanismName\" " + mace::OidToString(maceConf.mechanismName) + " is not the mechanism-name IEC 62351-4 defines (" +
				mace::OfficialMechanismText + "); use it only for peers that use that value" +
				(maceConf.mechanismName == mace::LegacyMechanismName ? std::string(" (it is the legacy value of IEC TS 62351-4:2007)") : std::string()));
	}

	// ---- addressing
	GetSelector(root, "LocalTSelector", cotp::MaxTsapLength, local.tsap, p);
	GetSelector(root, "RemoteTSelector", cotp::MaxTsapLength, remote.tsap, p);
	GetSelector(root, "LocalSSelector", session::MaxSelectorLength, local.sSelector, p);
	GetSelector(root, "RemoteSSelector", session::MaxSelectorLength, remote.sSelector, p);
	GetSelector(root, "LocalPSelector", presentation::MaxSelectorLength, local.pSelector, p);
	GetSelector(root, "RemotePSelector", presentation::MaxSelectorLength, remote.pSelector, p);
	GetOid(root, "LocalAPTitle", local.apTitle, p);
	GetOid(root, "RemoteAPTitle", remote.apTitle, p);
	if(GetInt(root, "LocalAEQualifier", v, p, 0, INT32_MAX))
		local.aeQualifier = v;
	if(GetInt(root, "RemoteAEQualifier", v, p, 0, INT32_MAX))
		remote.aeQualifier = v;
	if(GetInt(root, "TPDUSize", v, p, 128, 8192))
	{
		if(v != 128 && v != 256 && v != 512 && v != 1024 && v != 2048 && v != 4096 && v != 8192)
			p.Add("\"TPDUSize\" must be 128, 256, 512, 1024, 2048, 4096 or 8192");
		else
		{
			uint8_t code = 0;
			for(int64_t s = v; s > 1; s >>= 1)
				++code;
			tpduSizeCode = code;
		}
	}

	// ---- TASE.2
	std::string s;
	if(GetString(root, "TASE2Edition", s, p))
	{
		if(s == "2000-08")
			edition = tase2::Edition2000;
		else if(s == "1996-08")
			edition = tase2::Edition1996;
		else
			p.Add("\"TASE2Edition\" must be \"2000-08\" or \"1996-08\"");
	}
	GetString(root, "BilateralTableID", bilateralTableId, p);
	GetString(root, "RemoteBilateralTableID", remoteBilateralTableId, p);
	GetString(root, "LocalDomain", localDomain, p);
	GetString(root, "RemoteDomain", remoteDomain, p);
	GetString(root, "VendorName", vendorName, p);
	GetString(root, "ModelName", modelName, p);
	GetString(root, "Revision", revision, p);
	for(const char* key : {"BilateralTableID", "RemoteBilateralTableID"})
	{
		if(!root.isMember(key) || !root[key].isString())
			continue;
		const std::string id = root[key].asString();
		//Bilateral_Table_ID is a Visible-String-32 (IEC 60870-6-503 clause 8.3.3)
		if(id.size() > tase2::BilateralTableIdWidth)
			p.Add(std::string("\"") + key + "\" is too long (" + std::to_string(id.size()) + " characters): Bilateral_Table_ID is a Visible-String-32 (IEC 60870-6-503 clause 8.3.3), at most " +
				std::to_string(tase2::BilateralTableIdWidth) + " characters; shorten it");
		else if(std::any_of(id.begin(), id.end(), [](char ch) { return static_cast<unsigned char>(ch) < 0x20 || static_cast<unsigned char>(ch) > 0x7E; }))
			p.Add(std::string("\"") + key + "\" has a character that is not visible ASCII (0x20 to 0x7E): Bilateral_Table_ID is a Visible-String-32 (IEC 60870-6-503 clause 8.3.3); use letters, digits and punctuation only");
	}
	for(const char* key : {"LocalDomain", "RemoteDomain", "VendorName", "ModelName", "Revision"})
		if(root.isMember(key) && root[key].isString() && root[key].asString().size() > iccp::mms::MaxIdentifierLength)
			p.Add(std::string("\"") + key + "\" is too long");

	bool requiredGiven = false;
	bool supportedGiven = false;
	for(const char* key : {"SupportedBlocks", "RequiredRemoteBlocks"})
	{
		if(!root.isMember(key))
			continue;
		std::set<int> blocks;
		if(!root[key].isArray())
			p.Add(std::string("\"") + key + "\" must be an array of block numbers 1 to 9");
		else
			for(const auto& b : root[key])
			{
				if(!IsInt(b) || b.asInt64() < 1 || b.asInt64() > 9)
					p.Add(std::string("\"") + key + "\" must contain block numbers 1 to 9");
				else
					blocks.insert(static_cast<int>(b.asInt64()));
			}
		if(std::string(key) == "SupportedBlocks")
		{
			supportedBlocks = blocks;
			supportedGiven = true;
		}
		else
		{
			requiredRemoteBlocks = blocks;
			requiredGiven = true;
		}
	}

	// ---- timers, MMS parameters, limits
	if(GetInt(root, "RequestTimeoutms", v, p, 1, 3600000))
		requestTimeoutMs = static_cast<uint32_t>(v);
	if(GetInt(root, "AssociateTimeoutms", v, p, 1, 3600000))
		associateTimeoutMs = static_cast<uint32_t>(v);
	if(GetInt(root, "ReleaseTimeoutms", v, p, 1, 3600000))
		releaseTimeoutMs = static_cast<uint32_t>(v);
	GetInt(root, "MaxOutstandingCalling", maxOutstandingCalling, p, 1, 1000);
	GetInt(root, "MaxOutstandingCalled", maxOutstandingCalled, p, 1, 1000);
	GetInt(root, "NestingLevel", nestingLevel, p, 1, 32);
	GetBool(root, "AbortOnBilateralTableMismatch", abortOnBilateralTableMismatch, p);
	GetBool(root, "TraceHex", traceHex, p);
	if(GetInt(root, "TraceHexMaxBytes", v, p, 16, 65536))
		traceHexMaxBytes = static_cast<size_t>(v);
	GetBool(root, "ProbeOnDefineFailure", probeOnDefineFailure, p);
	if(GetInt(root, "ProbeChunkSize", v, p, 1, 500))
		probeChunkSize = static_cast<size_t>(v);
	{
		std::string text;
		if(GetString(root, "MissingMemberPolicy", text, p))
		{
			if(text == "Prune")
				missingMemberPolicy = MissingMemberPolicy::Prune;
			else if(text == "Fail")
				missingMemberPolicy = MissingMemberPolicy::Fail;
			else
				p.Add("\"MissingMemberPolicy\" must be \"Prune\" (define the data set without the members the peer lacks) or \"Fail\" (skip the data set)");
		}
		if(GetString(root, "MissingMemberQuality", text, p))
		{
			//flag names separated by '|', ',' or '+': COMM_LOST, COMM_LOST|REFERENCE_ERR, ...
			static const std::pair<const char*, odc::QualityFlags> flagNames[] = {
				{"ONLINE", odc::QualityFlags::ONLINE}, {"RESTART", odc::QualityFlags::RESTART},
				{"COMM_LOST", odc::QualityFlags::COMM_LOST}, {"REMOTE_FORCED", odc::QualityFlags::REMOTE_FORCED}, {"LOCAL_FORCED", odc::QualityFlags::LOCAL_FORCED},
				{"OVERRANGE", odc::QualityFlags::OVERRANGE}, {"REFERENCE_ERR", odc::QualityFlags::REFERENCE_ERR}, {"ROLLOVER", odc::QualityFlags::ROLLOVER},
				{"DISCONTINUITY", odc::QualityFlags::DISCONTINUITY}, {"CHATTER_FILTER", odc::QualityFlags::CHATTER_FILTER}
			};
			odc::QualityFlags q = odc::QualityFlags::NONE;
			bool ok = !text.empty();
			size_t pos = 0;
			while(ok && pos <= text.size())
			{
				size_t end = text.find_first_of("|,+", pos);
				if(end == std::string::npos)
					end = text.size();
				std::string name = text.substr(pos, end - pos);
				name.erase(std::remove_if(name.begin(), name.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }), name.end());
				bool found = false;
				for(const auto& [n, f] : flagNames)
					if(name == n)
					{
						q = q | f;
						found = true;
					}
				ok = found;
				pos = end + 1;
			}
			if(!ok)
				p.Add("\"MissingMemberQuality\" must be ODC quality flag names separated by '|', for example \"COMM_LOST\" or \"COMM_LOST|REFERENCE_ERR\"");
			else if(((q & odc::QualityFlags::ONLINE) == odc::QualityFlags::ONLINE) && ((q & (odc::QualityFlags::COMM_LOST | odc::QualityFlags::REFERENCE_ERR)) == odc::QualityFlags::NONE))
				p.Add("\"MissingMemberQuality\" must not mark the points as good: ONLINE needs COMM_LOST or REFERENCE_ERR with it");
			else
			{
				missingMemberQuality = q;
				std::string norm;
				for(const auto& [n, f] : flagNames)
					if((q & f) == f)
						norm += (norm.empty() ? "" : "|") + std::string(n);
				missingMemberQualityText = norm.empty() ? "NONE" : norm;
			}
		}
	}
	//the peers seen on the 2000-08 edition write and expect the 13 member DSTransferSet (with Status), the 1996-08 ones the 12 member form
	dsTransferSetStatusExplicit = root.isMember("DSTransferSetStatusMember");
	if(dsTransferSetStatusExplicit)
		GetBool(root, "DSTransferSetStatusMember", dsTransferSetStatusMember, p);
	else
		dsTransferSetStatusMember = edition.major >= 2000;
	GetBool(root, "RequireMatchingEdition", requireMatchingEdition, p);
	if(GetInt(root, "MaxClientDataSets", v, p, 0, 100000))
		maxClientDataSets = static_cast<size_t>(v);
	if(GetInt(root, "MaxDataSetMembers", v, p, 1, mms::MaxListEntries))
		maxDataSetMembers = static_cast<size_t>(v);
	if(GetInt(root, "NamePageSize", v, p, 1, 100000))
		namePageSize = static_cast<size_t>(v);
	if(GetInt(root, "MaxBufferedChanges", v, p, 1, 100000))
		maxBufferedChanges = static_cast<size_t>(v);
	GetBool(root, "BlockedRule2", blockedRule2, p);
	if(GetString(root, "BlockedDataName", blockedDataName, p))
	{
		//the name of the variable that carries the blocked octet string (Block 3): it must not be mistaken for a special transfer object
		if(!ValidIdentifier(blockedDataName))
			p.Add("\"BlockedDataName\" must be an MMS identifier of 1 to " + std::to_string(iccp::mms::MaxIdentifierLength) + " characters");
		else if(blockedDataName == tase2::Name::TransferSetName || blockedDataName == tase2::Name::TransferSetTimeStamp ||
		        blockedDataName == tase2::Name::DsConditionsDetected || blockedDataName == tase2::Name::EventCodeDetected)
			p.Add("\"BlockedDataName\" must not be the name of a special transfer object (" + blockedDataName + ")");
	}
	if(GetInt(root, "ReportAckTimeoutms", v, p, 0, 3600000))
		reportAckTimeoutMs = static_cast<uint32_t>(v);
	if(GetInt(root, "MaxUnacknowledgedReports", v, p, 1, 100000))
		maxUnacknowledgedReports = static_cast<size_t>(v);
	GetBool(root, "AcknowledgeCriticalReports", acknowledgeCriticalReports, p);
	GetBool(root, "NackUnknownReports", nackUnknownReports, p);

	//the service role: independent of the TCP direction. The default follows the port type (a listener serves, an initiator is a client).
	role = listener ? ServiceRole::Server : ServiceRole::Client;
	{
		std::string text;
		if(GetString(root, "ServiceRole", text, p))
		{
			if(text == "Client")
				role = ServiceRole::Client;
			else if(text == "Server")
				role = ServiceRole::Server;
			else if(text == "Peer")
				role = ServiceRole::Peer;
			else
				p.Add("\"ServiceRole\" must be \"Client\", \"Server\" or \"Peer\"");
		}
	}
	const char* roleName = role == ServiceRole::Client ? "Client" : (role == ServiceRole::Server ? "Server" : "Peer");

	// ---- points
	std::set<std::string> pointNames[2];                           //by direction
	std::map<std::pair<PointKind, int>, std::set<size_t>> indexes; //an ODC index is used once per kind and direction
	const std::pair<PointKind, tase2::PointType> kinds[] = {
		{PointKind::Binary, tase2::PointType::StateQ}, {PointKind::Analog, tase2::PointType::RealQ},
		{PointKind::Counter, tase2::PointType::DiscreteQ}
	};
	for(const auto& [kind, defaultType] : kinds)
	{
		const char* key = KindName(kind);
		if(!root.isMember(key))
			continue;
		if(!root[key].isArray())
		{
			p.Add(std::string("\"") + key + "\" must be an array");
			continue;
		}
		size_t n = 0;
		for(const auto& e : root[key])
		{
			const std::string where = std::string(key) + "[" + std::to_string(n++) + "]: ";
			if(!e.isObject())
			{
				p.Add(where + "must be an object");
				continue;
			}
			IccpPointConf pt;
			pt.kind = kind;
			pt.type = defaultType;
			//the direction: Serve = the value comes from ODC Event() and is served to the peer; Read = it is read from the peer and published to ODC
			if(role == ServiceRole::Server)
				pt.direction = PointDirection::Serve;
			else if(role == ServiceRole::Client)
				pt.direction = PointDirection::Read;
			bool directionGiven = false;
			std::string text;
			if(GetString(e, "Direction", text, p))
			{
				if(text == "Serve")
					pt.direction = PointDirection::Serve;
				else if(text == "Read")
					pt.direction = PointDirection::Read;
				else
					p.Add(where + "\"Direction\" must be \"Serve\" or \"Read\"");
				directionGiven = text == "Serve" || text == "Read";
			}
			if(role == ServiceRole::Peer && !directionGiven)
				p.Add(where + "\"Direction\" is required with ServiceRole \"Peer\": \"Serve\" for a point whose value this port supplies to the peer (from ODC events), "
					"\"Read\" for a point read from the peer and published to ODC");
			if(directionGiven && role == ServiceRole::Client && pt.direction == PointDirection::Serve)
				p.Add(where + "\"Direction\" \"Serve\" needs ServiceRole \"Server\" or \"Peer\" (the ServiceRole is \"Client\": it only reads); set \"ServiceRole\"");
			if(directionGiven && role == ServiceRole::Server && pt.direction == PointDirection::Read)
				p.Add(where + "\"Direction\" \"Read\" needs ServiceRole \"Client\" or \"Peer\" (the ServiceRole is \"Server\": it only serves); set \"ServiceRole\"");
			const int dir = static_cast<int>(pt.direction);
			//the domain defaults to this port's own for a served point and to the peer's for one that is read
			pt.domain = pt.direction == PointDirection::Serve ? localDomain : remoteDomain;
			int64_t idx = 0;
			if(!e.isMember("Index"))
				p.Add(where + "\"Index\" is required");
			else if(GetInt(e, "Index", idx, p, 0, INT32_MAX, where))
			{
				pt.index = static_cast<size_t>(idx);
				if(!indexes[{kind, dir}].insert(pt.index).second)
					p.Add(where + "duplicate Index " + std::to_string(idx) + " among the \"" + (dir == 0 ? "Serve" : "Read") + "\" " + key +
						" (an index may be used once per kind and direction)");
			}
			if(!GetString(e, "Name", pt.name, p) || !ValidIdentifier(pt.name))
				p.Add(where + "\"Name\" is required (1 to 255 characters)");
			else if(!pointNames[dir].insert(pt.name).second)
				p.Add(where + "duplicate point Name \"" + pt.name + "\" among the \"" + (dir == 0 ? "Serve" : "Read") + "\" points");
			if(GetString(e, "Scope", text, p))
			{
				if(text == "ICC")
					pt.scope = tase2::Scope::Icc;
				else if(text == "VCC")
					pt.scope = tase2::Scope::Vcc;
				else
					p.Add(where + "\"Scope\" must be \"ICC\" or \"VCC\"");
			}
			GetString(e, "Domain", pt.domain, p);
			if(pt.scope == tase2::Scope::Icc && !ValidIdentifier(pt.domain))
				p.Add(where + "an ICC point needs a domain: set \"Domain\" or \"" + (pt.direction == PointDirection::Serve ? "LocalDomain" : "RemoteDomain") + "\"");
			if(GetString(e, "Type", text, p))
			{
				const auto it = PointTypeNames().find(text);
				if(it == PointTypeNames().end())
					p.Add(where + "unknown point \"Type\" \"" + text + "\"");
				else if(!TypeFits(kind, it->second))
					p.Add(where + "\"Type\" \"" + text + "\" does not suit " + key);
				else
					pt.type = it->second;
			}
			GetDouble(e, "ScaleFactor", pt.scale, p, where);
			GetDouble(e, "Offset", pt.offset, p, where);
			if(pt.scale == 0.0 || !std::isfinite(pt.scale) || !std::isfinite(pt.offset))
				p.Add(where + "\"ScaleFactor\" must be a non-zero finite number");
			points.push_back(pt);
		}
	}

	// ---- controls (TASE.2 Block 5)
	//"BinaryControls" (ODC ControlRelayOutputBlock) and "AnalogControls" (ODC AnalogOutput events): the devices this port serves (Direction "Serve":
	//the peer operates them and the ODC control event is published downstream) or operates on the peer (Direction "Operate": from ODC control events).
	std::set<std::string> controlNames[2];                                  //by direction
	std::map<std::pair<ControlKind, int>, std::set<size_t>> controlIndexes; //an ODC index is used once per kind and direction
	const std::pair<const char*, ControlKind> controlLists[] = {
		{"BinaryControls", ControlKind::Binary}, {"AnalogControls", ControlKind::Analog}
	};
	for(const auto& [key, kind] : controlLists)
	{
		if(!root.isMember(key))
			continue;
		if(!root[key].isArray())
		{
			p.Add(std::string("\"") + key + "\" must be an array");
			continue;
		}
		size_t n = 0;
		for(const auto& e : root[key])
		{
			const std::string where = std::string(key) + "[" + std::to_string(n++) + "]: ";
			if(!e.isObject())
			{
				p.Add(where + "must be an object");
				continue;
			}
			IccpControlConf c;
			c.kind = kind;
			c.type = kind == ControlKind::Binary ? tase2::ControlType::Command : tase2::ControlType::RealSetpoint;
			if(role == ServiceRole::Server)
				c.direction = ControlDirection::Serve;
			else if(role == ServiceRole::Client)
				c.direction = ControlDirection::Operate;
			bool directionGiven = false;
			std::string text;
			if(GetString(e, "Direction", text, p))
			{
				if(text == "Serve")
					c.direction = ControlDirection::Serve;
				else if(text == "Operate")
					c.direction = ControlDirection::Operate;
				else
					p.Add(where + "\"Direction\" must be \"Serve\" (the device is ours: the peer operates it, the ODC control event is published) or \"Operate\" (the device is the peer's: we operate it from ODC control events)");
				directionGiven = text == "Serve" || text == "Operate";
			}
			if(role == ServiceRole::Peer && !directionGiven)
				p.Add(where + "\"Direction\" is required with ServiceRole \"Peer\": \"Serve\" for a device this port owns (the peer operates it), \"Operate\" for a device of the peer that this port operates");
			if(directionGiven && role == ServiceRole::Client && c.direction == ControlDirection::Serve)
				p.Add(where + "\"Direction\" \"Serve\" needs ServiceRole \"Server\" or \"Peer\" (the ServiceRole is \"Client\": it only operates the peer's devices); set \"ServiceRole\"");
			if(directionGiven && role == ServiceRole::Server && c.direction == ControlDirection::Operate)
				p.Add(where + "\"Direction\" \"Operate\" needs ServiceRole \"Client\" or \"Peer\" (the ServiceRole is \"Server\": it only serves its own devices); set \"ServiceRole\"");
			const int dir = static_cast<int>(c.direction);
			const char* dirName = dir == 0 ? "Serve" : "Operate";
			c.domain = c.direction == ControlDirection::Serve ? localDomain : remoteDomain;
			int64_t idx = 0;
			if(!e.isMember("Index"))
				p.Add(where + "\"Index\" is required");
			else if(GetInt(e, "Index", idx, p, 0, INT32_MAX, where))
			{
				c.index = static_cast<size_t>(idx);
				if(!controlIndexes[{kind, dir}].insert(c.index).second)
					p.Add(where + "duplicate Index " + std::to_string(idx) + " among the \"" + dirName + "\" " + key + " (an index may be used once per kind and direction)");
			}
			if(!GetString(e, "Name", c.name, p) || !ValidIdentifier(c.name) || (c.name + tase2::SelectSuffix).size() > iccp::mms::MaxIdentifierLength)
				p.Add(where + "\"Name\" is required (1 to 251 characters: the select object is the name with \"_SBO\" added)");
			else if(!controlNames[dir].insert(c.name).second)
				p.Add(where + "duplicate control Name \"" + c.name + "\" among the \"" + dirName + "\" controls");
			if(GetString(e, "Scope", text, p))
			{
				if(text == "ICC")
					c.scope = tase2::Scope::Icc;
				else if(text == "VCC")
					c.scope = tase2::Scope::Vcc;
				else
					p.Add(where + "\"Scope\" must be \"ICC\" or \"VCC\"");
			}
			GetString(e, "Domain", c.domain, p);
			if(c.scope == tase2::Scope::Icc && !ValidIdentifier(c.domain))
				p.Add(where + "an ICC control needs a domain: set \"Domain\" or \"" + (c.direction == ControlDirection::Serve ? "LocalDomain" : "RemoteDomain") + "\"");
			if(GetString(e, "Type", text, p))
			{
				static const std::map<std::string, tase2::ControlType> types = {
					{"Command", tase2::ControlType::Command}, {"DiscreteCommand", tase2::ControlType::DiscreteCommand},
					{"RealSetpoint", tase2::ControlType::RealSetpoint}, {"DiscreteSetpoint", tase2::ControlType::DiscreteSetpoint}
				};
				const auto it = types.find(text);
				if(it == types.end())
					p.Add(where + "unknown control \"Type\" \"" + text + "\" (Command, DiscreteCommand, RealSetpoint or DiscreteSetpoint)");
				else if(tase2::IsSetpoint(it->second) != (kind == ControlKind::Analog))
					p.Add(where + "\"Type\" \"" + text + "\" does not suit " + key + (kind == ControlKind::Binary ? " (Command or DiscreteCommand)" : " (RealSetpoint or DiscreteSetpoint)"));
				else
					c.type = it->second;
			}
			GetBool(e, "SelectRequired", c.selectRequired, p, where);
			if(GetInt(e, "CheckBackID", v, p, tase2::MinCheckBackId, tase2::MaxCheckBackId, where))
				c.checkBackId = v;
			//the "...ms" spelling of the other timers, and "...Ms"
			auto timeoutKey = [&](const char* a, const char* b)
						{
							return e.isMember(a) ? a : b;
						};
			if(GetInt(e, timeoutKey("SelectTimeoutms", "SelectTimeoutMs"), v, p, 1, 3600000, where))
				c.selectTimeoutMs = static_cast<uint32_t>(v);
			if(GetInt(e, timeoutKey("Timeoutms", "TimeoutMs"), v, p, 1, 3600000, where))
				c.timeoutMs = static_cast<uint32_t>(v);
			for(const auto& [vkey, target] : {std::make_pair("CommandValueOn", &c.commandValueOn), std::make_pair("CommandValueOff", &c.commandValueOff)})
			{
				int64_t x = 0;
				if(!GetInt(e, vkey, x, p, INT64_MIN, INT64_MAX, where))
					continue;
				if(x < tase2::MinControlValue || x > tase2::MaxControlValue)
					p.Add(where + "\"" + vkey + "\" must be between " + std::to_string(tase2::MinControlValue) + " and " + std::to_string(tase2::MaxControlValue) +
						": a command is an integer of width 16 that this port writes as a signed value (IEC 60870-6-802 clause 6.2.2), so use a value in that range");
				else
					*target = x;
			}
			if(kind == ControlKind::Binary && c.commandValueOn == c.commandValueOff)
				p.Add(where + "\"CommandValueOn\" and \"CommandValueOff\" must differ");
			for(const auto& [ckey, target] : {std::make_pair("ControlCodeOn", &c.codeOn), std::make_pair("ControlCodeOff", &c.codeOff)})
				if(GetString(e, ckey, text, p))
				{
					const auto code = odc::ControlCodeFromString(text);
					if(code == odc::ControlCode::UNDEFINED || code == odc::ControlCode::NUL)
						p.Add(where + "\"" + ckey + "\" must be an ODC control code (LATCH_ON, LATCH_OFF, PULSE_ON, PULSE_OFF, CLOSE_PULSE_ON or TRIP_PULSE_ON)");
					else
						*target = code;
				}
			if(GetString(e, "AnalogOutputType", text, p))
			{
				if(text == "Int16")
					c.analogEvent = odc::EventType::AnalogOutputInt16;
				else if(text == "Int32")
					c.analogEvent = odc::EventType::AnalogOutputInt32;
				else if(text == "Float32")
					c.analogEvent = odc::EventType::AnalogOutputFloat32;
				else if(text == "Double64")
					c.analogEvent = odc::EventType::AnalogOutputDouble64;
				else
					p.Add(where + "\"AnalogOutputType\" must be \"Int16\", \"Int32\", \"Float32\" or \"Double64\"");
			}
			else if(c.type == tase2::ControlType::DiscreteSetpoint)
				c.analogEvent = odc::EventType::AnalogOutputInt32;
			GetDouble(e, "ScaleFactor", c.scale, p, where);
			GetDouble(e, "Offset", c.offset, p, where);
			if(c.scale == 0.0 || !std::isfinite(c.scale) || !std::isfinite(c.offset))
				p.Add(where + "\"ScaleFactor\" must be a non-zero finite number");
			//device tags (optional): only a device this port serves can be taggable
			GetBool(e, "Taggable", c.taggable, p, where);
			if(c.taggable && c.direction == ControlDirection::Operate)
				p.Add(where + "\"Taggable\" is for a device this port serves (Direction \"Serve\"): a device of the peer that this port operates needs no setting, the GetTag and SetTag commands try its tag object");
			if(GetString(e, "InitialTag", text, p))
			{
				const auto tag = tase2::DeviceTagFromString(text);
				if(!tag)
					p.Add(where + "\"InitialTag\" must be \"NO_TAG\", \"OPEN_AND_CLOSE_INHIBIT\" or \"CLOSE_ONLY_INHIBIT\"");
				else
					c.initialTag = *tag;
				if(!c.taggable)
					p.Add(where + "\"InitialTag\" needs \"Taggable\" true (only a taggable device has a tag)");
			}
			if(GetString(e, "InitialTagReason", c.initialTagReason, p))
			{
				if(!tase2::ValidTagReason(c.initialTagReason))
					p.Add(where + "\"InitialTagReason\" must be at most " + std::to_string(tase2::MaxTagReasonLength) + " characters of visible ASCII (0x20 to 0x7E): it is a TASE.2 VisibleString");
				if(!c.taggable)
					p.Add(where + "\"InitialTagReason\" needs \"Taggable\" true (only a taggable device has a tag)");
			}
			controls.push_back(c);
		}
	}
	//a control device D and its select object D_SBO are two objects of the same domain: they must not clash with another device or with a point
	for(const auto& c : controls)
		for(const auto& other : controls)
			if(&c != &other && c.direction == other.direction && c.scope == other.scope && c.domain == other.domain && other.name == tase2::SelectName(c.name))
				p.Add("control \"" + other.name + "\" clashes with the select object of control \"" + c.name + "\" (\"" + c.name + "_SBO\"): rename one");
	//a taggable device's tag object D_TAG is a third object of the domain (only taggable devices have one)
	for(const auto& c : controls)
		if(c.taggable)
		{
			for(const auto& other : controls)
				if(&c != &other && c.direction == other.direction && c.scope == other.scope && c.domain == other.domain && other.name == tase2::TagName(c.name))
					p.Add("control \"" + other.name + "\" clashes with the tag object of the taggable control \"" + c.name + "\" (\"" + c.name + "_TAG\"): rename one");
			for(const auto& pt : points)
				if(pt.direction == PointDirection::Serve && c.scope == pt.scope && c.domain == pt.domain && pt.name == tase2::TagName(c.name))
					p.Add("the taggable control \"" + c.name + "\" has the tag object \"" + pt.name + "\", which is also the name of a point: a point and a tag object cannot share a name in a domain");
		}
	for(const auto& c : controls)
		for(const auto& pt : points)
			if((c.direction == ControlDirection::Serve) == (pt.direction == PointDirection::Serve) && c.scope == pt.scope && c.domain == pt.domain &&
			   (pt.name == c.name || pt.name == tase2::SelectName(c.name)))
				p.Add("control \"" + c.name + "\" has the same object name as a point (\"" + pt.name + "\"): a control device and a point cannot share a name in a domain");

	// ---- data sets
	//a data set is either one this port serves (all members are "Serve" points: the peer may use it, or define its own up to the caps) or
	//one this port defines on the peer and reads (all members are "Read" points). The direction is inferred from the members unless given.
	std::set<std::string> dataSetNames[2];
	if(root.isMember("DataSets"))
	{
		if(!root["DataSets"].isArray())
			p.Add("\"DataSets\" must be an array");
		else
		{
			size_t n = 0;
			for(const auto& e : root["DataSets"])
			{
				const std::string where = "DataSets[" + std::to_string(n++) + "]: ";
				if(!e.isObject())
				{
					p.Add(where + "must be an object");
					continue;
				}
				DataSetConf d;
				std::string dsName;
				if(!GetString(e, "Name", dsName, p) || !ValidIdentifier(dsName))
					p.Add(where + "\"Name\" is required");
				d.name = dsName;
				if(!e.isMember("Members") || !e["Members"].isArray() || e["Members"].empty())
					p.Add(where + "\"Members\" must be a non-empty array of point names");
				else
					for(const auto& m : e["Members"])
					{
						if(!m.isString())
							p.Add(where + "\"Members\" must contain point names");
						else
							d.members.push_back(m.asString());
					}
				std::optional<int> explicitDir;
				std::string text;
				if(GetString(e, "Direction", text, p))
				{
					if(text == "Serve")
						explicitDir = 0;
					else if(text == "Read")
						explicitDir = 1;
					else
						p.Add(where + "\"Direction\" must be \"Serve\" or \"Read\"");
				}
				//which directions can every member be found in?
				bool canServe = true, canRead = true;
				for(const auto& m : d.members)
				{
					const bool s = pointNames[0].count(m) != 0, r = pointNames[1].count(m) != 0;
					if(!s && !r)
					{
						p.Add(where + "member \"" + m + "\" is not a configured point");
						continue;
					}
					canServe = canServe && s;
					canRead = canRead && r;
				}
				int dir = 0;
				if(explicitDir)
				{
					dir = *explicitDir;
					if((dir == 0 && !canServe) || (dir == 1 && !canRead))
						p.Add(where + "\"Direction\" \"" + text + "\" but not every member is a \"" + text + "\" point");
				}
				else if(canServe && canRead)
				{
					if(!d.members.empty())
						p.Add(where + "every member exists both as a \"Serve\" and as a \"Read\" point: set \"Direction\" on the data set (\"Serve\" = served by this port, \"Read\" = defined on the peer)");
					dir = role == ServiceRole::Server ? 0 : 1;
				}
				else if(canServe)
					dir = 0;
				else if(canRead)
					dir = 1;
				else if(!d.members.empty())
				{
					bool anyServe = false, anyRead = false;
					for(const auto& m : d.members)
					{
						anyServe = anyServe || pointNames[0].count(m) != 0;
						anyRead = anyRead || pointNames[1].count(m) != 0;
					}
					if(anyServe && anyRead)
						p.Add(where + "mixes \"Serve\" and \"Read\" points: a data set is either served by this port or defined on the peer; split it in two");
				}
				d.direction = dir == 0 ? PointDirection::Serve : PointDirection::Read;
				if(d.direction == PointDirection::Serve && role == ServiceRole::Client)
					p.Add(where + "this data set holds \"Serve\" points but the ServiceRole is \"Client\"");
				if(d.direction == PointDirection::Read && role == ServiceRole::Server)
					p.Add(where + "this data set holds \"Read\" points but the ServiceRole is \"Server\" (a server does not define data sets on the peer)");
				d.domain = d.direction == PointDirection::Serve ? localDomain : remoteDomain;
				GetString(e, "Domain", d.domain, p);
				if(!d.name.empty() && !dataSetNames[dir].insert(d.name).second)
					p.Add(where + "duplicate data set Name \"" + d.name + "\"");
				if(d.members.size() > maxDataSetMembers)
					p.Add(where + "more members than \"MaxDataSetMembers\"");
				if(GetInt(e, "PollIntervalms", v, p, 0, 86400000, where))
					d.pollIntervalMs = static_cast<uint64_t>(v);
				if(d.direction == PointDirection::Serve && d.pollIntervalMs != 0)
					p.Add(where + "\"PollIntervalms\" applies to a data set this port reads (Direction \"Read\"), not to one it serves");
				if(!ValidIdentifier(d.domain))
					p.Add(where + "a data set needs a domain: set \"Domain\" or \"" + (d.direction == PointDirection::Serve ? "LocalDomain" : "RemoteDomain") + "\"");
				dataSets.push_back(d);
			}
		}
	}

	// ---- transfer sets
	//"TransferSets" = the transfer sets this port STARTS on the peer (it needs a "DataSet" it defines there); "TransferSetPool" = the names this port
	//OFFERS to the peer (served by it). A legacy server configuration lists its pool in "TransferSets" as [{"Name": ...}]: when the ServiceRole is
	//"Server" (the port never starts any) such entries are taken as the pool.
	std::set<std::string> tsNames, poolNames;
	auto addPoolName = [&](const std::string& name, const std::string& where)
				 {
					 if(!ValidIdentifier(name))
						 p.Add(where + "a transfer set name is required (1 to 255 characters)");
					 else if(!poolNames.insert(name).second)
						 p.Add(where + "duplicate transfer set name \"" + name + "\" in the pool");
					 else
						 transferSetPool.push_back(name);
				 };
	if(root.isMember("TransferSetPool"))
	{
		if(role == ServiceRole::Client)
			p.Add(std::string("\"TransferSetPool\" is the list of transfer sets this port offers to the peer: it needs ServiceRole \"Server\" or \"Peer\" (the ServiceRole is \"")
				+ roleName + "\")");
		if(!root["TransferSetPool"].isArray())
			p.Add("\"TransferSetPool\" must be an array of transfer set names");
		else
		{
			size_t n = 0;
			for(const auto& e : root["TransferSetPool"])
			{
				const std::string where = "TransferSetPool[" + std::to_string(n++) + "]: ";
				if(e.isString())
					addPoolName(e.asString(), where);
				else if(e.isObject() && e.isMember("Name") && e["Name"].isString())
					addPoolName(e["Name"].asString(), where);
				else
					p.Add(where + "must be a transfer set name (a string, or an object with a \"Name\")");
			}
		}
	}
	if(root.isMember("TransferSets"))
	{
		if(!root["TransferSets"].isArray())
			p.Add("\"TransferSets\" must be an array");
		else
		{
			size_t n = 0;
			for(const auto& e : root["TransferSets"])
			{
				const std::string where = "TransferSets[" + std::to_string(n++) + "]: ";
				if(!e.isObject())
				{
					p.Add(where + "must be an object");
					continue;
				}
				TransferSetConf t;
				GetString(e, "Name", t.name, p);
				if(role == ServiceRole::Server)
				{
					if(e.isMember("DataSet"))
					{
						p.Add(where + "ServiceRole \"Server\" never starts transfer sets, so \"TransferSets\" cannot name a \"DataSet\": set ServiceRole \"Client\" or \"Peer\" to start "
							"transfer sets on the peer, and list the names this port offers in \"TransferSetPool\"");
						continue;
					}
					addPoolName(t.name, where);
					continue;
				}
				if(!e.isMember("DataSet"))
				{
					p.Add(where + "\"DataSet\" is required: \"TransferSets\" lists the transfer sets this port starts on the peer (ServiceRole \"" + roleName +
						"\"). To offer transfer sets to the peer list their names in \"TransferSetPool\" (needs ServiceRole \"Server\" or \"Peer\")");
					continue;
				}
				if(!t.name.empty() && !tsNames.insert(t.name).second)
					p.Add(where + "duplicate transfer set Name \"" + t.name + "\"");
				GetString(e, "DataSet", t.dataSet, p);
				if(t.dataSet.empty() || !dataSetNames[1].count(t.dataSet))
				{
					if(dataSetNames[0].count(t.dataSet))
						p.Add(where + "\"DataSet\" \"" + t.dataSet + "\" is a data set this port serves; \"TransferSets\" starts transfer sets for data sets this port defines on the peer "
							"(a data set whose members are \"Read\" points)");
					else
						p.Add(where + "\"DataSet\" must name a configured data set whose members are \"Read\" points");
				}
				if(e.isMember("Conditions"))
				{
					t.conditions.mask = 0;
					if(!e["Conditions"].isArray())
						p.Add(where + "\"Conditions\" must be an array");
					else
						for(const auto& c : e["Conditions"])
						{
							const auto it = c.isString() ? ConditionNames().find(c.asString()) : ConditionNames().end();
							if(it == ConditionNames().end())
								p.Add(where + "unknown condition (use IntervalTimeOut, IntegrityTimeOut, ObjectChange, OperatorRequest or OtherExternalEvent)");
							else
								t.conditions.mask = static_cast<uint8_t>(t.conditions.mask | it->second);
						}
				}
				GetInt(e, "StartTime", t.startTime, p, 0, INT64_MAX, where);
				GetInt(e, "Interval", t.interval, p, 0, 86400 * 366, where);
				GetInt(e, "TLE", t.tle, p, 0, 86400 * 366, where);
				GetInt(e, "BufferTime", t.bufferTime, p, 0, 86400, where);
				GetInt(e, "IntegrityCheck", t.integrityCheck, p, 0, 86400 * 366, where);
				GetBool(e, "BlockData", t.blockData, p, where);
				GetBool(e, "Critical", t.critical, p, where);
				GetBool(e, "RBE", t.rbe, p, where);
				GetBool(e, "AllChangesReported", t.allChangesReported, p, where);
				GetInt(e, "EventCode", t.eventCode, p, 0, INT32_MAX, where);
				if(!e.isMember("Conditions"))
				{
					//default: report changes, plus the periodic conditions whose period was given
					t.conditions.mask = tase2::DsCondition::ObjectChange;
					if(t.interval > 0)
						t.conditions.mask = static_cast<uint8_t>(t.conditions.mask | tase2::DsCondition::IntervalTimeOut);
					if(t.integrityCheck > 0)
						t.conditions.mask = static_cast<uint8_t>(t.conditions.mask | tase2::DsCondition::IntegrityTimeOut);
				}
				if(t.conditions.Has(tase2::DsCondition::IntervalTimeOut) && t.interval == 0)
					p.Add(where + "IntervalTimeOut requested but \"Interval\" is 0");
				if(t.conditions.Has(tase2::DsCondition::IntegrityTimeOut) && t.integrityCheck == 0)
					p.Add(where + "IntegrityTimeOut requested but \"IntegrityCheck\" is 0");
				if(t.allChangesReported)
				{
					if(edition.major < 2000)
					{
						//the 1996-08 DSTransferSet has no such member: it is never sent
						t.allChangesReported = false;
						warnings.push_back(where + "\"AllChangesReported\" is ignored (false) with TASE2Edition 1996-08: that edition has no such member");
					}
					else
					{
						std::string needs;
						if(!t.rbe)
							needs += ", \"RBE\" true";
						if(!t.conditions.Has(tase2::DsCondition::ObjectChange))
							needs += ", the ObjectChange condition";
						if(t.bufferTime <= 0)
							needs += ", a \"BufferTime\" above 0";
						if(!needs.empty())
							p.Add(where + "\"AllChangesReported\" true needs" + needs.substr(1) + " (otherwise it must be false)");
					}
				}
				if(e.isMember("SpecialObjects"))
				{
					t.specials.clear();
					if(!e["SpecialObjects"].isArray())
						p.Add(where + "\"SpecialObjects\" must be an array");
					else
						for(const auto& so : e["SpecialObjects"])
						{
							const auto it = so.isString() ? SpecialNames().find(so.asString()) : SpecialNames().end();
							if(it == SpecialNames().end())
								p.Add(where + "unknown special object");
							else if(std::find(t.specials.begin(), t.specials.end(), it->second) != t.specials.end())
								p.Add(where + "a special object is listed twice");
							else
								t.specials.push_back(it->second);
						}
					if(t.specials.empty() || t.specials.front() != tase2::Special::TransferSetName)
						p.Add(where + "\"SpecialObjects\" must start with Transfer_Set_Name");
				}
				if(t.critical)
				{
					if(std::find(t.specials.begin(), t.specials.end(), tase2::Special::TimeStamp) == t.specials.end())
						warnings.push_back(where + "\"Critical\" is true but \"SpecialObjects\" has no Transfer_Set_Time_Stamp: the acknowledgement of a report echoes the time stamp of "
							"the report, so with none it only names the transfer set (the peer then has to match it to its oldest unacknowledged report)");
					if(!acknowledgeCriticalReports)
						warnings.push_back(where + "\"Critical\" is true but \"AcknowledgeCriticalReports\" is false: no report will be acknowledged, so the peer will count each one as a failure");
				}
				transferSets.push_back(t);
			}
		}
	}

	// ---- cross checks
	//Block 3: the name storage of Transfer_Set_Name in a blocked report has 32 octets for the domain and 32 for the item id [V]: a longer name cannot be sent blocked (the report is sent in the ordinary form)
	if(supportedBlocks.count(3) && HasServerSide())
	{
		bool tooLong = localDomain.size() > tase2::blocked::NameIdWidth;
		for(const auto& n : transferSetPool)
			tooLong = tooLong || n.size() > tase2::blocked::NameIdWidth;
		if(tooLong)
			warnings.push_back("\"SupportedBlocks\" has 3 but \"LocalDomain\" or a transfer set name in \"TransferSetPool\" is longer than " + std::to_string(tase2::blocked::NameIdWidth) +
				" characters, which the storage form of Transfer_Set_Name in a blocked report cannot hold: such a transfer set is reported in the ordinary form even when the peer asks for BlockData");
	}
	bool anyRead = false, anyServe = false, anyServeIcc = false;
	for(const auto& pt : points)
	{
		anyRead = anyRead || pt.direction == PointDirection::Read;
		anyServe = anyServe || pt.direction == PointDirection::Serve;
		anyServeIcc = anyServeIcc || (pt.direction == PointDirection::Serve && pt.scope == tase2::Scope::Icc);
	}
	for(const auto& d : dataSets)
	{
		anyRead = anyRead || d.direction == PointDirection::Read;
		anyServe = anyServe || d.direction == PointDirection::Serve;
	}
	if(HasClientSide() && !transferSets.empty() && !requiredGiven)
		requiredRemoteBlocks.insert(2);
	//TASE.2 Block 5 [S]: serving devices means advertising block 5; operating the peer's devices needs the peer to advertise it
	bool serveControls = false, operateControls = false, serveControlIcc = false;
	for(const auto& c : controls)
	{
		serveControls = serveControls || c.direction == ControlDirection::Serve;
		operateControls = operateControls || c.direction == ControlDirection::Operate;
		serveControlIcc = serveControlIcc || (c.direction == ControlDirection::Serve && c.scope == tase2::Scope::Icc);
	}
	if(serveControls)
	{
		if(!supportedGiven)
			supportedBlocks.insert(5);
		else if(!supportedBlocks.count(5))
			p.Add("this port serves control devices (Direction \"Serve\" in the controls), so \"SupportedBlocks\" must contain 5 (device control)");
	}
	if(operateControls)
	{
		if(!requiredGiven)
			requiredRemoteBlocks.insert(5);
		else if(!requiredRemoteBlocks.count(5))
			p.Add("this port operates the peer's control devices (Direction \"Operate\" in the controls), so \"RequiredRemoteBlocks\" must contain 5 (device control)");
	}
	if(operateControls && !ValidIdentifier(remoteDomain))
		for(const auto& c : controls)
			if(c.direction == ControlDirection::Operate && c.scope == tase2::Scope::Icc)
			{
				p.Add("\"RemoteDomain\" is required when the peer's devices are operated (Direction \"Operate\")");
				break;
			}
	if(serveControlIcc && !ValidIdentifier(localDomain))
		p.Add("\"LocalDomain\" is required when ICC control devices are served (Direction \"Serve\")");
	if(anyRead && !ValidIdentifier(remoteDomain))
		p.Add("\"RemoteDomain\" is required when points or data sets are read from the peer (Direction \"Read\")");
	if(anyServeIcc && !ValidIdentifier(localDomain))
		p.Add("\"LocalDomain\" is required when ICC points are served (Direction \"Serve\")");
	return p.list;
}

stack::Config ICCPPortConf::StackConfig() const
{
	stack::Config c;
	c.local = local;
	c.remote = remote;
	c.tpduSizeCode = tpduSizeCode;
	c.maxOutstandingCalling = maxOutstandingCalling;
	c.maxOutstandingCalled = maxOutstandingCalled;
	c.nestingLevel = nestingLevel;
	c.associateTimeout = std::chrono::milliseconds(associateTimeoutMs);
	c.requestTimeout = std::chrono::milliseconds(requestTimeoutMs);
	c.releaseTimeout = std::chrono::milliseconds(releaseTimeoutMs);
	c.mace = maceContext;
	return c;
}

tase2::LocalAssociation ICCPPortConf::LocalAssociation() const
{
	tase2::LocalAssociation a;
	if(!localDomain.empty() && !bilateralTableId.empty())
		a.bilateralTableIds[localDomain] = bilateralTableId;
	a.version = edition;
	mms::BitString features(tase2::SupportedFeaturesBitCount);
	for(int b : supportedBlocks)
		features.Set(static_cast<size_t>(b - 1), true);
	a.features = features;
	return a;
}

const IccpControlConf* ICCPPortConf::FindControl(const std::string& name, ControlDirection direction) const
{
	for(const auto& c : controls)
		if(c.direction == direction && c.name == name)
			return &c;
	return nullptr;
}

const IccpControlConf* ICCPPortConf::FindControl(ControlKind kind, size_t index, ControlDirection direction) const
{
	for(const auto& c : controls)
		if(c.direction == direction && c.kind == kind && c.index == index)
			return &c;
	return nullptr;
}

bool ICCPPortConf::HasControls(ControlDirection direction) const
{
	for(const auto& c : controls)
		if(c.direction == direction)
			return true;
	return false;
}

bool ICCPPortConf::HasTaggableControls() const
{
	for(const auto& c : controls)
		if(c.taggable && c.direction == ControlDirection::Serve)
			return true;
	return false;
}

const IccpPointConf* ICCPPortConf::FindPoint(const std::string& name, PointDirection direction) const
{
	for(const auto& p : points)
		if(p.direction == direction && p.name == name)
			return &p;
	return nullptr;
}

const DataSetConf* ICCPPortConf::FindDataSet(const std::string& name, PointDirection direction) const
{
	for(const auto& d : dataSets)
		if(d.direction == direction && d.name == name)
			return &d;
	return nullptr;
}
