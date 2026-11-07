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
 * DiagnosticsTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The MMS error names, and the client's provisioning diagnostics: a ClientSession against a scripted peer, in memory.
 *  Synthetic names only.
 */

#include "MmsNames.h"
#include "StackTestHelpers.h"
#include "Tase2Client.h"
#include <algorithm>
#include <catch.hpp>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP diagnostics - " name

using namespace iccp;
using iccp::stack::LogLevel;

namespace
{
mms::ServiceError Err(int64_t cls, int64_t code)
{
	mms::ServiceError e;
	e.errorClass = cls;
	e.errorCode = code;
	return e;
}
}

TEST_CASE(SUITE("service error names"))
{
	CHECK(mms::Describe(Err(mms::ErrorClass::Definition, 1)) == "definition [2] / object-undefined [1]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Definition, 5)) == "definition [2] / object-exists [5]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Access, 2)) == "access [7] / object-non-existent [2]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Service, 3)) == "service [4] / pdu-size [3]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Resource, 1)) == "resource [3] / memory-unavailable [1]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Conclude, 1)) == "conclude [9] / further-communication-required [1]");
	CHECK(mms::Describe(Err(mms::ErrorClass::Others, 0)) == "other [12] / other [0]");

	//every class has a name, and "other" is code 0 in each
	const char* classes[] = {"vmd-state", "application-reference", "definition", "resource", "service", "service-preempt", "time-resolution",
		                   "access", "initiate", "conclude", "cancel", "file", "other"};
	for(int64_t c = 0; c <= 12; ++c)
	{
		REQUIRE(mms::ErrorClassName(c) != nullptr);
		CHECK(std::string(mms::ErrorClassName(c)) == classes[c]);
		REQUIRE(mms::ErrorCodeName(c, 0) != nullptr);
		CHECK(std::string(mms::ErrorCodeName(c, 0)) == "other");
	}
	//the definition class, as ISO 9506 lists it
	const char* definition[] = {"other", "object-undefined", "invalid-address", "type-unsupported", "type-inconsistent", "object-exists",
		                      "object-attribute-inconsistent"};
	for(int64_t c = 0; c <= 6; ++c)
		CHECK(std::string(mms::ErrorCodeName(mms::ErrorClass::Definition, c)) == definition[c]);

	//unknown values keep their number
	CHECK(mms::ErrorClassName(13) == nullptr);
	CHECK(mms::ErrorCodeName(mms::ErrorClass::Definition, 99) == nullptr);
	CHECK(mms::Describe(Err(mms::ErrorClass::Definition, 99)) == "definition [2] / unknown [99]");
	CHECK(mms::Describe(Err(77, 3)) == "unknown [77] / unknown [3]");

	//additional information, and text from the peer is made safe
	mms::ServiceError e = Err(mms::ErrorClass::Definition, 1);
	e.additionalCode = 42;
	CHECK(mms::Describe(e) == "definition [2] / object-undefined [1], additional code 42");
	e.additionalDescription = std::string("no thing\r\n\x01 here");
	CHECK(mms::Describe(e) == "definition [2] / object-undefined [1], additional code 42, additional description \"no thing??? here\"");
	e.additionalDescription = std::string(1000, 'x');
	CHECK(mms::Describe(e).size() < 300);
}

TEST_CASE(SUITE("data access error and reject names"))
{
	const char* names[] = {"object-invalidated", "hardware-fault", "temporarily-unavailable", "object-access-denied", "object-undefined", "invalid-address",
		                 "type-unsupported", "type-inconsistent", "object-attribute-inconsistent", "object-access-unsupported", "object-non-existent", "object-value-invalid"};
	for(int64_t i = 0; i <= 11; ++i)
		CHECK(mms::DescribeDataAccessError(i) == std::string(names[i]) + " [" + std::to_string(i) + "]");
	CHECK(mms::DescribeDataAccessError(10) == "object-non-existent [10]");
	CHECK(mms::DescribeDataAccessError(12) == "unknown [12]");
	//the constants agree with the names
	CHECK(std::string(mms::DataAccessErrorName(mms::DataAccessError::ObjectNonExistent)) == "object-non-existent");
	CHECK(std::string(mms::DataAccessErrorName(mms::DataAccessError::ObjectUndefined)) == "object-undefined");
	CHECK(std::string(mms::DataAccessErrorName(mms::DataAccessError::ObjectAccessDenied)) == "object-access-denied");

	mms::Reject r;
	r.type = mms::RejectType::ConfirmedRequestPdu;
	r.reason = mms::RejectReason::UnrecognizedService;
	CHECK(mms::Describe(r) == "confirmed-requestPDU [1] / unrecognized-service [1]");
	r.originalInvokeId = 7;
	CHECK(mms::Describe(r) == "confirmed-requestPDU [1] / unrecognized-service [1], invoke id 7");
	//the constants we send are named as the standard names them
	r.reason = mms::RejectReason::InvalidInvokeId;
	CHECK(mms::Describe(r).find("invalid-invokeID") != std::string::npos);
	r.reason = mms::RejectReason::InvalidArgument;
	CHECK(mms::Describe(r).find("invalid-argument") != std::string::npos);
	r.reason = mms::RejectReason::MaxServOutstandingExceeded;
	CHECK(mms::Describe(r).find("max-serv-outstanding-exceeded") != std::string::npos);
	r.type = mms::RejectType::UnconfirmedPdu;
	r.reason = 2;
	CHECK(mms::Describe(r).find("unconfirmedPDU [4] / invalid-argument [2]") == 0);
	r.type = 99;
	r.reason = 1;
	CHECK(mms::Describe(r).find("unknown [99] / unknown [1]") == 0);
	for(int64_t t = 1; t <= 11; ++t)
		CHECK(mms::RejectTypeName(t) != nullptr);

	stack::Outcome o;
	o.kind = stack::Outcome::Kind::Error;
	o.error = Err(mms::ErrorClass::Definition, 1);
	CHECK(stack::Describe(o) == "service error: definition [2] / object-undefined [1]");
	o.kind = stack::Outcome::Kind::Rejected;
	o.reject = r;
	CHECK(stack::Describe(o).find("rejected: unknown [99]") == 0);
	o.kind = stack::Outcome::Kind::Timeout;
	CHECK(stack::Describe(o) == "timed out");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the client's provisioning diagnostics against a scripted peer
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
std::string Num(const char* prefix, size_t n, size_t width)
{
	std::string s = std::to_string(n);
	return std::string(prefix) + std::string(width > s.size() ? width - s.size() : 0, '0') + s;
}
std::string BinaryName(size_t i) //i is 0-based
{
	return Num("POINT_B", i + 1, 4);
}
std::string AnalogName(size_t i)
{
	return Num("POINT_", i + 1, 4) + "_MW";
}
std::string CounterName(size_t i)
{
	return Num("POINT_C", i + 1, 3);
}

//A client configuration: nBinary, nAnalog and nCounter Read points (ODC indexes from 0), a data set DS_BAD with all of them and a data set
//DS_GOOD with the first two binaries, each with a transfer set of a fixed name. extra, if given, starts with ", ". The policy is Fail (the diagnostics
//tests) unless extra sets MissingMemberPolicy. pollMembers, if not empty, is the JSON list of member names of a third data set DS_POLL that is polled.
std::string ClientJson(size_t nBinary, size_t nAnalog, size_t nCounter, std::string extra = "", const std::string& pollMembers = "")
{
	if(extra.find("MissingMemberPolicy") == std::string::npos)
		extra += ", \"MissingMemberPolicy\": \"Fail\"";
	std::string bin, ana, cnt, all, good;
	auto add = [](std::string& list, const std::string& item)
		     {
			     list += (list.empty() ? "" : ", ") + item;
		     };
	for(size_t i = 0; i < nBinary; ++i)
	{
		add(bin, "{\"Index\": " + std::to_string(i) + ", \"Name\": \"" + BinaryName(i) + "\"}");
		add(all, "\"" + BinaryName(i) + "\"");
		if(i < 2)
			add(good, "\"" + BinaryName(i) + "\"");
	}
	for(size_t i = 0; i < nAnalog; ++i)
	{
		add(ana, "{\"Index\": " + std::to_string(i) + ", \"Name\": \"" + AnalogName(i) + "\"}");
		add(all, "\"" + AnalogName(i) + "\"");
	}
	for(size_t i = 0; i < nCounter; ++i)
	{
		add(cnt, "{\"Index\": " + std::to_string(i) + ", \"Name\": \"" + CounterName(i) + "\"}");
		add(all, "\"" + CounterName(i) + "\"");
	}
	return "{\"ServiceRole\": \"Client\", \"RemoteDomain\": \"DOMAIN_A\", \"LocalDomain\": \"DOMAIN_B\", \"RequiredRemoteBlocks\": [1],"
	       " \"Binaries\": [" + bin + "], \"Analogs\": [" + ana + "], \"Counters\": [" + cnt + "],"
	       " \"DataSets\": [{\"Name\": \"DS_BAD\", \"Members\": [" + all + "]}, {\"Name\": \"DS_GOOD\", \"Members\": [" + good + "]}" +
	       (pollMembers.empty() ? "" : ", {\"Name\": \"DS_POLL\", \"PollIntervalms\": 1, \"Members\": [" + pollMembers + "]}") + "],"
	       " \"TransferSets\": [{\"Name\": \"TS_BAD\", \"DataSet\": \"DS_BAD\"}, {\"Name\": \"TS_GOOD\", \"DataSet\": \"DS_GOOD\"}]" + extra + "}";
}

//positions (0-based) in DS_BAD, where two special objects come first
size_t PosBinary(size_t i)
{
	return 2 + i;
}
size_t PosAnalog(size_t nBinary, size_t i)
{
	return 2 + nBinary + i;
}
size_t PosCounter(size_t nBinary, size_t nAnalog, size_t i)
{
	return 2 + nBinary + nAnalog + i;
}

struct Fixture
{
	stacktest::Link link;
	ICCPPortConf conf;
	std::vector<std::pair<LogLevel, std::string>> logs;
	std::vector<tase2::ProbeResult> probes;
	std::vector<tase2::ProvisionStatus> status;
	std::unique_ptr<tase2::ClientSession> session;
	tase2::LocalAssociation peerAssociation;

	//what the scripted peer does
	std::set<std::string> rejectDefine; //data sets whose definition is answered with an error
	mms::ServiceError defineError = Err(mms::ErrorClass::Definition, 1);
	std::set<std::string> missing;    //item names a Read answers with object-non-existent
	std::set<std::string> unreadable; //... with object-access-denied
	enum class ProbeMode
	{
		Normal,
		TooFewResults,
		TooManyResults,
		WrongResponseKind,
		Error,
		Reject,
		Silent,
		FailSecondChunk
	};
	ProbeMode probeMode = ProbeMode::Normal;
	size_t probeReads = 0;
	size_t largestProbe = 0;
	size_t writes = 0;
	bool deleteFails = false;
	bool strictDefine = false; //a definition naming any member of `missing` is answered with defineError
	bool existsOnce = false;   //the first definition of DS_BAD without a missing member is answered with object-exists
	struct Defined
	{
		std::string name;
		std::vector<std::string> members;
	};
	std::vector<Defined> defines;                                 //every definition received, in order
	std::map<std::string, std::vector<mms::ObjectName>> dataSets; //the definitions that were accepted
	//what the client published
	struct Event
	{
		std::string name;
		PointKind kind;
		size_t index;
		double value;
		odc::QualityFlags quality;
		odc::msSinceEpoch_t timestamp;
		bool unavailable;
	};
	std::vector<Event> events;

	explicit Fixture(const std::string& json)
	{
		Json::Value root;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream in(json);
		REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
		const auto problems = conf.Load(root, false);
		REQUIRE(problems.empty());
		peerAssociation = conf.LocalAssociation();
		peerAssociation.bilateralTableIds["DOMAIN_A"] = "BT_PEER";
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req) { return Peer(id, req); };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
	}

	void Run()
	{
		tase2::ClientSession::Callbacks cb;
		cb.onProbe = [this](const tase2::ProbeResult& r) { probes.push_back(r); };
		cb.onProvisioning = [this](const std::vector<tase2::ProvisionStatus>& s) { status = s; };
		auto record = [this](bool unavailable)
				  {
					  return [this, unavailable](const IccpPointConf& pt, const tase2::PointState& s)
						   {
							   events.push_back(Event{pt.name, pt.kind, pt.index, tase2::NumericValue(pt, s), s.quality, s.timestamp, unavailable});
						   };
				  };
		cb.onPoint = record(false);
		cb.onUnavailable = record(true);
		session = std::make_unique<tase2::ClientSession>(conf, link.a.conn, tase2::Timebase(), [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); },
			std::move(cb));
		session->OnAssociated();
		link.Pump();
	}

	//the events published for a point (by configured name)
	std::vector<Event> EventsFor(const std::string& name) const
	{
		std::vector<Event> v;
		for(const auto& e : events)
			if(e.name == name)
				v.push_back(e);
		return v;
	}
	size_t Flagged() const
	{
		size_t n = 0;
		for(const auto& e : events)
			n += e.unavailable ? 1 : 0;
		return n;
	}

	//The peer's value for a point: binaries 1, analogs 100 + index (+ bump), counters 7 + index
	double PeerValue(const IccpPointConf& pt, double bump = 0) const
	{
		switch(pt.kind)
		{
			case PointKind::Binary: return 1;
			case PointKind::Analog: return 100.0 + static_cast<double>(pt.index) + bump;
			case PointKind::Counter: return 7.0 + static_cast<double>(pt.index) + bump;
		}
		return 0;
	}
	mms::Data PeerData(const IccpPointConf& pt, double bump = 0) const
	{
		tase2::PointState s;
		s.known = true;
		s.quality = odc::QualityFlags::ONLINE;
		s.binary = true;
		s.analog = PeerValue(pt, bump);
		s.counter = static_cast<uint32_t>(PeerValue(pt, bump));
		s.timestamp = 1700000000000;
		return tase2::ToData(pt, s, s.timestamp);
	}

	//A transfer set report from the peer for an accepted data set: the values of `names` (point names; the others are not in the report), by-name or inline form.
	//For the by-name form `names` must be all the points of the data set definition, in order.
	void SendReport(const std::string& dataSet, const std::vector<std::string>& names, bool inlineForm, double bump = 0)
	{
		tase2::TransferReport tr;
		tr.layout = {tase2::Special::TransferSetName, tase2::Special::ConditionsDetected};
		tr.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_BAD"};
		tr.conditionsDetected = tase2::DsConditions{tase2::DsCondition::ObjectChange, tase2::DsConditionBitCount};
		mms::ListOfVariable list;
		list.push_back(mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Transfer_Set_Name"), std::nullopt});
		list.push_back(mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "DSConditions_Detected"), std::nullopt});
		for(const auto& n : names)
		{
			const IccpPointConf* pt = conf.FindPoint(n, PointDirection::Read);
			REQUIRE(pt != nullptr);
			tr.values.push_back(mms::AccessResult::Success(PeerData(*pt, bump)));
			list.push_back(mms::VariableSpec{pt->Object(), std::nullopt});
		}
		if(inlineForm)
			tr.specification = list;
		else
			tr.specification = mms::ObjectName::Domain("DOMAIN_A", dataSet);
		REQUIRE(link.b.conn.SendReport(tase2::BuildTransferReport(tr)));
		link.Pump();
		//the test endpoint collects the reports: hand them to the session as the port does
		const auto reports = link.a.reports;
		link.a.reports.clear();
		for(const auto& r : reports)
			session->OnReport(r);
	}

	size_t Count(const std::string& part, std::optional<LogLevel> level = std::nullopt) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if((!level || *level == l) && t.find(part) != std::string::npos)
				++n;
		return n;
	}
	std::string First(const std::string& part) const
	{
		for(const auto& [l, t] : logs)
			if(t.find(part) != std::string::npos)
				return t;
		return "";
	}
	size_t IndexOf(const std::string& part) const
	{
		for(size_t i = 0; i < logs.size(); ++i)
			if(logs[i].second.find(part) != std::string::npos)
				return i;
		return std::string::npos;
	}
	//the log lines of a level that are not the expected ones
	size_t Unexpected(LogLevel level) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			n += (l == level) ? 1 : 0;
		return n;
	}

	bool Peer(uint32_t id, const mms::ConfirmedServiceRequest& req)
	{
		auto& conn = link.b.conn;
		if(const auto* r = std::get_if<mms::ReadRequest>(&req))
		{
			if(tase2::IsAssociationInfoRequest(*r))
			{
				conn.Respond(id, tase2::BuildAssociationInfoResponse(*r, peerAssociation));
				return true;
			}
			const auto* list = std::get_if<mms::ListOfVariable>(&r->specification);
			if(list == nullptr)
			{
				//a read of a data set by name (polling): the values of its members
				const auto* name = std::get_if<mms::ObjectName>(&r->specification);
				const auto it = name ? dataSets.find(name->itemId) : dataSets.end();
				if(it == dataSets.end())
				{
					conn.RespondError(id, Err(mms::ErrorClass::Access, 2));
					return true;
				}
				mms::ReadResponse rr;
				for(const auto& m : it->second)
				{
					const IccpPointConf* pt = conf.FindPoint(m.itemId, PointDirection::Read);
					rr.results.push_back(pt ? mms::AccessResult::Success(PeerData(*pt)) : mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent));
				}
				conn.Respond(id, rr);
				return true;
			}
			++probeReads;
			largestProbe = std::max(largestProbe, list->size());
			mms::ReadResponse rr;
			for(const auto& v : *list)
			{
				if(missing.count(v.name.itemId))
					rr.results.push_back(mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent));
				else if(unreadable.count(v.name.itemId))
					rr.results.push_back(mms::AccessResult::Failure(mms::DataAccessError::ObjectAccessDenied));
				else
					rr.results.push_back(mms::AccessResult::Success(mms::Data::Integer(1)));
			}
			switch(probeMode)
			{
				case ProbeMode::Normal: break;
				case ProbeMode::TooFewResults: rr.results.pop_back(); break;
				case ProbeMode::TooManyResults: rr.results.push_back(mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent)); break;
				case ProbeMode::WrongResponseKind: conn.Respond(id, mms::IdentifyResponse{"VENDOR_X", "MODEL_Y", "REV_Z", std::nullopt}); return true;
				case ProbeMode::Error: conn.RespondError(id, Err(mms::ErrorClass::Service, 3)); return true;
				case ProbeMode::Reject: conn.RejectRequest(id); return true;
				case ProbeMode::Silent: return true;
				case ProbeMode::FailSecondChunk:
					if(probeReads == 2)
					{
						conn.RespondError(id, Err(mms::ErrorClass::Access, 3));
						return true;
					}
					break;
			}
			conn.Respond(id, rr);
			return true;
		}
		if(const auto* d = std::get_if<mms::DefineNamedVariableListRequest>(&req))
		{
			Defined def;
			def.name = d->name.itemId;
			bool lacksMember = false;
			for(const auto& v : d->variables)
			{
				def.members.push_back(v.name.itemId);
				lacksMember = lacksMember || missing.count(v.name.itemId) != 0;
			}
			defines.push_back(def);
			if(existsOnce && !lacksMember && d->name.itemId == "DS_BAD")
			{
				existsOnce = false;
				conn.RespondError(id, Err(mms::ErrorClass::Definition, 5));
				return true;
			}
			if(rejectDefine.count(d->name.itemId) || (strictDefine && lacksMember))
				conn.RespondError(id, defineError);
			else
			{
				std::vector<mms::ObjectName> members;
				for(const auto& v : d->variables)
					members.push_back(v.name);
				dataSets[d->name.itemId] = members;
				conn.Respond(id, mms::DefineNamedVariableListResponse{});
			}
			return true;
		}
		if(std::holds_alternative<mms::DeleteNamedVariableListRequest>(req))
		{
			if(deleteFails)
				conn.RespondError(id, Err(mms::ErrorClass::Definition, 1));
			else
				conn.Respond(id, mms::DeleteNamedVariableListResponse{});
			return true;
		}
		if(std::holds_alternative<mms::WriteRequest>(req))
		{
			++writes;
			mms::WriteResponse wr;
			wr.results.push_back(mms::WriteResult{true, 0});
			conn.Respond(id, wr);
			return true;
		}
		return false;
	}
};
}

TEST_CASE(SUITE("a rejected definition is logged with its context and every missing member is listed one by one"))
{
	constexpr size_t NB = 5, NA = 20, NC = 3; //30 members with the two special objects: positions 0 to 31
	Fixture f(ClientJson(NB, NA, NC));
	f.rejectDefine = {"DS_BAD"};
	//a binary, two analogs (one of them ODC index 11) and a counter are missing; one analog exists but is not readable
	f.missing = {BinaryName(2), AnalogName(11), AnalogName(19), CounterName(1)};
	f.unreadable = {AnalogName(4)};
	f.Run();

	//the definition failure: operation, data set and domain, member count, first and last member, symbolic error
	CHECK(f.First("DefineNamedVariableList failed") ==
		"DefineNamedVariableList failed for data set DS_BAD (domain DOMAIN_A), 30 members, first \"Transfer_Set_Name\", last \"POINT_C003\": "
		"service error: definition [2] / object-undefined [1]");
	CHECK(f.Count("DefineNamedVariableList failed", LogLevel::Error) == 1);

	//the probe: one result, every missing member, in data set order
	REQUIRE(f.probes.size() == 1);
	const tase2::ProbeResult& p = f.probes[0];
	CHECK(p.dataSet == "DS_BAD");
	CHECK(p.domain == "DOMAIN_A");
	CHECK(p.members == 30);
	CHECK(p.complete);
	CHECK(p.failure.empty());
	CHECK(p.probed == 30);
	CHECK(p.missing == 4);
	CHECK(p.presentWithError == 1);
	CHECK(p.present == 25);
	CHECK(p.chunks == 1); //30 members fit in one Read of 50
	REQUIRE(p.missingListed.size() == 4);
	CHECK(p.missingListed[0].name == "POINT_B0003");
	CHECK(p.missingListed[0].index == PosBinary(2));
	CHECK(p.missingListed[0].scope == "ICC");
	CHECK(p.missingListed[0].domain == "DOMAIN_A");
	CHECK(p.missingListed[0].kind == "Binary");
	CHECK(p.missingListed[0].odcIndex == std::optional<size_t>(2));
	CHECK(p.missingListed[0].error == mms::DataAccessError::ObjectNonExistent);
	CHECK(p.missingListed[1].name == "POINT_0012_MW");
	CHECK(p.missingListed[1].index == PosAnalog(NB, 11));
	CHECK(p.missingListed[1].kind == "Analog");
	CHECK(p.missingListed[1].odcIndex == std::optional<size_t>(11));
	CHECK(p.missingListed[2].name == "POINT_0020_MW");
	CHECK(p.missingListed[2].index == PosAnalog(NB, 19));
	CHECK(p.missingListed[3].name == "POINT_C002");
	CHECK(p.missingListed[3].index == PosCounter(NB, NA, 1));
	CHECK(p.missingListed[3].kind == "Counter");
	CHECK(p.missingListed[3].odcIndex == std::optional<size_t>(1));
	REQUIRE(p.errorListed.size() == 1);
	CHECK(p.errorListed[0].name == "POINT_0005_MW");
	CHECK(p.errorListed[0].error == mms::DataAccessError::ObjectAccessDenied);

	//the log: one error line per missing member with its full identification; "member N of M" is 1-based, "position" 0-based
	CHECK(f.Count("missing member ", LogLevel::Error) == 4);
	CHECK(f.First("missing member ") ==
		"missing member 5 of 30 (position 4, 0-based) in data set DS_BAD (domain DOMAIN_A): POINT_B0003 (ICC, domain DOMAIN_A; ODC Binary index 2) - object-non-existent [10]");
	CHECK(f.Count("missing member 19 of 30 (position 18, 0-based) in data set DS_BAD (domain DOMAIN_A): POINT_0012_MW (ICC, domain DOMAIN_A; ODC Analog index 11) - "
		"object-non-existent [10]") == 1);
	CHECK(f.Count("missing member 29 of 30 (position 28, 0-based) in data set DS_BAD (domain DOMAIN_A): POINT_C002 (ICC, domain DOMAIN_A; ODC Counter index 1)") == 1);
	CHECK(f.Count("present but not readable member 12 of 30 (position 11, 0-based) in data set DS_BAD (domain DOMAIN_A): POINT_0005_MW (ICC, domain DOMAIN_A; ODC Analog index 4)"
		" - object-access-denied [3]", LogLevel::Error) == 1);
	//then the totals line, after the member lines
	const std::string totals = "probe of data set DS_BAD (domain DOMAIN_A): 30 of 30 members probed, 4 missing, 25 present, 1 present but not readable";
	CHECK(f.Count(totals, LogLevel::Error) == 1);
	CHECK(f.IndexOf(totals) > f.IndexOf("missing member 29 of 30"));
	CHECK(f.Count("missing member ") == 4);

	//provisioning went on with the next data set, and the summary comes once, after the probe
	CHECK(f.writes == 1);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD", LogLevel::Error) == 1);
	CHECK(f.Count("provisioning finished") == 1);
	CHECK(f.IndexOf("provisioning finished") > f.IndexOf(totals));
	CHECK(f.Count("transfer set TS_GOOD started for data set DS_GOOD", LogLevel::Info) == 1);

	//the state
	REQUIRE(f.status.size() == 2);
	CHECK(f.status[0].dataSet == "DS_BAD");
	CHECK(f.status[0].domain == "DOMAIN_A");
	CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
	CHECK(std::string(tase2::ToString(f.status[0].state)) == "define-failed");
	CHECK(f.status[0].members == 30);
	CHECK(f.status[0].lastError == "DefineNamedVariableList: service error: definition [2] / object-undefined [1]");
	REQUIRE(f.status[0].probe.has_value());
	CHECK(f.status[0].probe->missing == 4);
	CHECK(f.status[0].probe->missingListed.size() == 4);
	CHECK(f.status[1].dataSet == "DS_GOOD");
	CHECK(f.status[1].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.status[1].transferSet == "TS_GOOD");
	CHECK(f.status[1].lastError.empty());
	CHECK_FALSE(f.status[1].probe.has_value());
	CHECK(f.session->Status().size() == 2);
}

TEST_CASE(SUITE("the happy path logs no failures, and a delete of a data set that does not exist is only a debug line"))
{
	Fixture f(ClientJson(5, 3, 2));
	f.deleteFails = true;
	f.Run();
	CHECK(f.probeReads == 0);
	CHECK(f.probes.empty());
	CHECK(f.writes == 2);
	//nothing but the summary is above debug; the summary of a clean run is info
	CHECK(f.Count("provisioning finished: 2 data sets ok, 0 failed", LogLevel::Info) == 1);
	CHECK(f.Count("DeleteNamedVariableList failed", LogLevel::Debug) == 2);
	CHECK(f.Count("DeleteNamedVariableList failed", LogLevel::Warn) == 0);
	CHECK(f.Count("failed", LogLevel::Error) == 0);
	REQUIRE(f.status.size() == 2);
	CHECK(f.status[0].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.status[1].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.status[0].transferSet == "TS_BAD");
}

TEST_CASE(SUITE("an oversized data set is probed in chunks and every missing member is listed"))
{
	constexpr size_t NB = 5, NA = 1000;
	std::set<std::string> missing;
	for(size_t i = 0; i < NA; i += 3)
		missing.insert(AnalogName(i));
	REQUIRE(missing.size() == 334);

	for(size_t chunk : {50u, 100u, 1u + 0u})
	{
		DYNAMIC_SECTION("chunks of " << chunk)
		{
			if(chunk == 1)
				continue; //a chunk of one would be 1007 requests: not worth the time here
			Fixture f(ClientJson(NB, NA, 0, ", \"ProbeChunkSize\": " + std::to_string(chunk)));
			f.rejectDefine = {"DS_BAD"};
			f.missing = missing;
			f.Run();

			REQUIRE(f.probes.size() == 1);
			const auto& p = f.probes[0];
			CHECK(p.members == 2 + NB + NA);
			CHECK(p.complete);
			CHECK(p.probed == p.members);
			CHECK(p.missing == 334);
			CHECK(p.present == p.members - 334);
			CHECK(p.chunks == (p.members - 2 + chunk - 1) / chunk); //the special objects are not read
			CHECK(f.probeReads == p.chunks);
			CHECK(f.largestProbe <= chunk);
			//all of them: no truncation, in data set order, each with its position and ODC index
			REQUIRE(p.missingListed.size() == 334);
			size_t k = 0;
			size_t last = 0;
			for(size_t i = 0; i < NA; i += 3, ++k)
			{
				const auto& e = p.missingListed[k];
				CHECK(e.name == AnalogName(i));
				CHECK(e.index == PosAnalog(NB, i));
				CHECK(e.odcIndex == std::optional<size_t>(i));
				CHECK(e.kind == "Analog");
				if(k > 0)
					CHECK(e.index > last);
				last = e.index;
			}
			CHECK(f.Count("missing member ", LogLevel::Error) == 334);
			CHECK(f.Count("probe of data set DS_BAD (domain DOMAIN_A): 1007 of 1007 members probed, 334 missing, 673 present", LogLevel::Error) == 1);
			CHECK(f.Count("not listed") == 0);
			CHECK(f.status[0].probe->missingListed.size() == 334);
			CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
		}
	}
}

TEST_CASE(SUITE("every member present: the totals line says the definition was refused for another reason"))
{
	Fixture f(ClientJson(5, 5, 1));
	f.rejectDefine = {"DS_BAD"};
	f.Run();
	REQUIRE(f.probes.size() == 1);
	CHECK(f.probes[0].complete);
	CHECK(f.probes[0].missing == 0);
	CHECK(f.probes[0].present == f.probes[0].members);
	CHECK(f.Count("missing member ") == 0);
	CHECK(f.Count("every member exists on the peer", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("a probe that cannot be completed is reported, and provisioning goes on"))
{
	struct Case
	{
		Fixture::ProbeMode mode;
		const char* failure;
		const char* failure2;
	};
	const Case cases[] = {
		{Fixture::ProbeMode::TooFewResults, "was answered with 27 results for 28 variables", nullptr},
		{Fixture::ProbeMode::TooManyResults, "was answered with 29 results for 28 variables", nullptr},
		{Fixture::ProbeMode::WrongResponseKind, "unexpected kind of response", "malformed"},
		{Fixture::ProbeMode::Error, "service error: service [4] / pdu-size [3]", nullptr},
		{Fixture::ProbeMode::Reject, "rejected: confirmed-requestPDU [1] / unrecognized-service [1]", nullptr}
	};
	for(const auto& c : cases)
	{
		Fixture f(ClientJson(5, 20, 3));
		f.rejectDefine = {"DS_BAD"};
		f.missing = {AnalogName(3)};
		f.probeMode = c.mode;
		f.Run();
		INFO("probe mode " << static_cast<int>(c.mode));
		REQUIRE(f.probes.size() == 1);
		const auto& p = f.probes[0];
		CHECK_FALSE(p.complete);
		CHECK(p.probed == 2); //only the two special objects, which are not read and count as present
		CHECK(p.missing == 0);
		CHECK(p.chunks == 0);
		CHECK((p.failure.find(c.failure) != std::string::npos || (c.failure2 && p.failure.find(c.failure2) != std::string::npos)));
		CHECK(f.Count("the probe is incomplete", LogLevel::Warn) == 1);
		CHECK(f.Count("members probed") == 1);
		//no crash, no stall: the other data set was defined and started, the summary came
		CHECK(f.writes == 1);
		CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
		REQUIRE(f.status.size() == 2);
		CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
		CHECK(f.status[0].probe.has_value());
		CHECK(f.status[1].state == tase2::ProvisionState::TransferStarted);
		CHECK(f.link.a.conn.OutstandingRequests() == 0);
	}
}

TEST_CASE(SUITE("a probe the peer never answers times out; provisioning had already gone on, the summary waits for the probe"))
{
	Fixture f(ClientJson(5, 20, 3));
	f.rejectDefine = {"DS_BAD"};
	f.probeMode = Fixture::ProbeMode::Silent;
	f.Run();
	CHECK(f.writes == 1); //the other data set did not wait for the probe
	CHECK(f.status[1].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.probes.empty());
	CHECK(f.Count("provisioning finished") == 0);
	f.link.clock.now += std::chrono::milliseconds(5001);
	f.link.a.conn.Tick();
	f.link.Pump();
	REQUIRE(f.probes.size() == 1);
	CHECK_FALSE(f.probes[0].complete);
	CHECK(f.probes[0].failure.find("timed out") != std::string::npos);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
	CHECK(f.link.a.conn.IsAssociated());
}

TEST_CASE(SUITE("a probe whose later chunk fails keeps what the earlier chunks found"))
{
	constexpr size_t NB = 5, NA = 200;
	Fixture f(ClientJson(NB, NA, 0));
	f.rejectDefine = {"DS_BAD"};
	f.missing = {AnalogName(0), AnalogName(10), AnalogName(100)}; //positions 7, 17 (first chunk, positions 0 to 49) and 107 (third)
	f.probeMode = Fixture::ProbeMode::FailSecondChunk;
	f.Run();
	REQUIRE(f.probes.size() == 1);
	const auto& p = f.probes[0];
	CHECK_FALSE(p.complete);
	CHECK(p.chunks == 1);
	CHECK(p.probed == 52); //the two special objects are not read: they count as present
	CHECK(p.missing == 2);
	REQUIRE(p.missingListed.size() == 2);
	CHECK(p.missingListed[0].index == PosAnalog(NB, 0));
	CHECK(p.missingListed[1].index == PosAnalog(NB, 10));
	CHECK(p.failure.find("members 52 to 101") != std::string::npos);
	CHECK(p.failure.find("service error: access [7] / object-access-denied [3]") != std::string::npos);
	CHECK(f.Count("missing member ", LogLevel::Error) == 2);
	CHECK(f.Count("52 of 207 members probed, 2 missing, 50 present; the probe is incomplete", LogLevel::Error) == 1);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
}

TEST_CASE(SUITE("no probe unless the definition failed with a definition class error (or when switched off)"))
{
	{
		//a resource error says nothing about the members
		Fixture f(ClientJson(5, 5, 1));
		f.rejectDefine = {"DS_BAD"};
		f.defineError = Err(mms::ErrorClass::Resource, 1);
		f.Run();
		CHECK(f.probeReads == 0);
		CHECK(f.probes.empty());
		CHECK(f.First("DefineNamedVariableList failed").find("service error: resource [3] / memory-unavailable [1]") != std::string::npos);
		CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
		CHECK_FALSE(f.status[0].probe.has_value());
		CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
	}
	{
		Fixture f(ClientJson(5, 5, 1, ", \"ProbeOnDefineFailure\": false"));
		REQUIRE_FALSE(f.conf.probeOnDefineFailure);
		f.rejectDefine = {"DS_BAD"};
		f.Run();
		CHECK(f.probeReads == 0);
		CHECK(f.probes.empty());
		CHECK(f.Count("DefineNamedVariableList failed", LogLevel::Error) == 1);
		CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD") == 1);
	}
}

TEST_CASE(SUITE("a failed start of a transfer set is one error line with the transfer set, data set and symbolic error"))
{
	Fixture f(ClientJson(5, 5, 1));
	f.Run();
	//the scripted peer accepts every write: check the success status first
	CHECK(f.status[0].state == tase2::ProvisionState::TransferStarted);

	//now a peer that answers the write with a data access error
	Fixture g(ClientJson(5, 5, 1));
	g.link.b.requestHook = [&g](uint32_t id, const mms::ConfirmedServiceRequest& req)
				     {
					     if(std::holds_alternative<mms::WriteRequest>(req))
					     {
						     mms::WriteResponse wr;
						     wr.results.push_back(mms::WriteResult{false, mms::DataAccessError::ObjectAccessDenied});
						     g.link.b.conn.Respond(id, wr);
						     return true;
					     }
					     return g.Peer(id, req);
				     };
	g.Run();
	CHECK(g.First("start transfer (Write of the DSTransferSet) failed") ==
		"start transfer (Write of the DSTransferSet) failed for transfer set TS_BAD (domain DOMAIN_A) for data set DS_BAD (domain DOMAIN_A), 13 members, first \"Transfer_Set_Name\", "
		"last \"POINT_C001\": data access error object-access-denied [3]");
	CHECK(g.Count("start transfer (Write of the DSTransferSet) failed", LogLevel::Error) == 2);
	REQUIRE(g.status.size() == 2);
	CHECK(g.status[0].state == tase2::ProvisionState::TransferFailed);
	CHECK(g.status[0].lastError.find("object-access-denied [3]") != std::string::npos);
	CHECK(g.Count("provisioning finished: 0 data sets ok, 2 failed: DS_BAD, DS_GOOD", LogLevel::Error) == 1);
	CHECK(g.probes.empty());
}

TEST_CASE(SUITE("a refused DSTransferSet value (object-attribute-inconsistent): the failure line lists what was sent and the peer's rule"))
{
	Fixture g(ClientJson(5, 5, 1));
	g.link.b.requestHook = [&g](uint32_t id, const mms::ConfirmedServiceRequest& req)
				     {
					     if(std::holds_alternative<mms::WriteRequest>(req))
					     {
						     mms::WriteResponse wr;
						     wr.results.push_back(mms::WriteResult{false, mms::DataAccessError::ObjectAttributeInconsistent});
						     g.link.b.conn.Respond(id, wr);
						     return true;
					     }
					     return g.Peer(id, req);
				     };
	g.Run();
	const std::string failure = g.First("start transfer (Write of the DSTransferSet) failed");
	CHECK(failure.find("object-attribute-inconsistent [8]") != std::string::npos);
	CHECK(failure.find("BufferTime 0") != std::string::npos);
	CHECK(failure.find("AllChangesReported false") != std::string::npos);
	CHECK(failure.find("conditions ObjectChange") != std::string::npos);
	CHECK(failure.find("requires false otherwise") != std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// MissingMemberPolicy Prune: define the data set again without the members the peer lacks, flag their points
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
const std::string Prune = ", \"MissingMemberPolicy\": \"Prune\"";
constexpr size_t NB = 5, NA = 20, NC = 3;

//the point names of DS_BAD (all binaries, analogs and counters), in data set order, without the ones in `without`
std::vector<std::string> PointNames(size_t nb, size_t na, size_t nc, const std::set<std::string>& without = {})
{
	std::vector<std::string> v;
	for(size_t i = 0; i < nb; ++i)
		v.push_back(BinaryName(i));
	for(size_t i = 0; i < na; ++i)
		v.push_back(AnalogName(i));
	for(size_t i = 0; i < nc; ++i)
		v.push_back(CounterName(i));
	v.erase(std::remove_if(v.begin(), v.end(), [&](const std::string& n) { return without.count(n) != 0; }), v.end());
	return v;
}

std::vector<Fixture::Defined> DefinesOf(const Fixture& f, const std::string& dataSet)
{
	std::vector<Fixture::Defined> v;
	for(const auto& d : f.defines)
		if(d.name == dataSet)
			v.push_back(d);
	return v;
}

//the latest event of a point that carries a value (not a flag)
const Fixture::Event* LastValue(const Fixture& f, const std::string& name)
{
	const Fixture::Event* e = nullptr;
	for(const auto& ev : f.events)
		if(ev.name == name && !ev.unavailable)
			e = &ev;
	return e;
}
}

TEST_CASE(SUITE("prune: the data set is defined again without the missing members, their points are flagged, the others are reported right"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune));
	f.strictDefine = true;
	const std::set<std::string> missing = {BinaryName(2), AnalogName(11), AnalogName(19), CounterName(1)};
	f.missing = missing;
	f.Run();

	//the data set was defined twice: as configured (refused), then without the four missing members, in the same order
	const auto bad = DefinesOf(f, "DS_BAD");
	REQUIRE(bad.size() == 2);
	CHECK(bad[0].members.size() == 2 + NB + NA + NC);
	std::vector<std::string> expected = {"Transfer_Set_Name", "DSConditions_Detected"};
	for(const auto& n : PointNames(NB, NA, NC, missing))
		expected.push_back(n);
	CHECK(bad[1].members == expected);
	CHECK(bad[1].members.size() == 26);
	CHECK(DefinesOf(f, "DS_GOOD").size() == 1);
	CHECK(f.writes == 2); //both transfer sets were started
	CHECK(f.dataSets.count("DS_BAD") == 1);

	//one warning per missing member, no error anywhere
	CHECK(f.Count("does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 4);
	CHECK(f.Count("data set DS_BAD: member POINT_0012_MW (ICC, domain DOMAIN_A; ODC Analog index 11) does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 1);
	CHECK(f.Count("data set DS_BAD: member POINT_B0003 (ICC, domain DOMAIN_A; ODC Binary index 2) does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 1);
	CHECK(f.Count("data set DS_BAD: member POINT_C002 (ICC, domain DOMAIN_A; ODC Counter index 1) does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 1);
	CHECK(f.Unexpected(LogLevel::Error) == 0);
	CHECK(f.Count("data set DS_BAD defined with 26 of 30 members (4 missing on the peer left out)", LogLevel::Info) == 1);
	CHECK(f.Count("transfer set TS_BAD started for data set DS_BAD", LogLevel::Info) == 1);
	CHECK(f.Count("provisioning finished: 2 data sets ok (1 partial), 0 failed; 4 points flagged COMM_LOST (not available on the peer)", LogLevel::Warn) == 1);

	//the state
	REQUIRE(f.status.size() == 2);
	CHECK(f.status[0].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.status[0].partial);
	CHECK(f.status[0].members == 30);
	CHECK(f.status[0].membersDefined == 26);
	CHECK(f.status[0].lastError.empty());
	REQUIRE(f.status[0].probe.has_value());
	CHECK(f.status[0].probe->missingListed.size() == 4);
	CHECK_FALSE(f.status[1].partial);
	CHECK(f.session->UnavailablePoints() == 4);

	//one flag event per missing point, with the configured quality, a neutral value and a current timestamp; nothing for the others
	CHECK(f.Flagged() == 4);
	const odc::msSinceEpoch_t now = odc::msSinceEpoch();
	for(const auto& name : missing)
	{
		const auto ev = f.EventsFor(name);
		REQUIRE(ev.size() == 1);
		CHECK(ev[0].unavailable);
		CHECK(ev[0].quality == odc::QualityFlags::COMM_LOST);
		CHECK(ev[0].value == 0);
		CHECK(ev[0].timestamp + 60000 > now);
		CHECK(ev[0].timestamp <= now + 1000);
	}
	CHECK(f.EventsFor("POINT_0012_MW")[0].kind == PointKind::Analog);
	CHECK(f.EventsFor("POINT_0012_MW")[0].index == 11);
	CHECK(f.EventsFor("POINT_B0003")[0].kind == PointKind::Binary);
	CHECK(f.EventsFor("POINT_B0003")[0].index == 2);
	CHECK(f.EventsFor("POINT_C002")[0].kind == PointKind::Counter);
	CHECK(f.EventsFor("POINT_C002")[0].index == 1);
	CHECK(f.EventsFor("POINT_0001_MW").empty());

	//an integrity report by data set name, with the values in the pruned order: every value reaches its own point although the positions shifted
	const auto present = PointNames(NB, NA, NC, missing);
	f.SendReport("DS_BAD", present, false);
	INFO("report problem: " << f.First("report") << " / " << f.First("wrong layout"));
	CHECK(f.Count("a report could not be interpreted") == 0);
	CHECK(f.Count("wrong layout") == 0);
	for(const auto& n : present)
	{
		const auto* ev = LastValue(f, n);
		REQUIRE(ev != nullptr);
		const IccpPointConf* pt = f.conf.FindPoint(n, PointDirection::Read);
		CHECK(ev->value == f.PeerValue(*pt));
		CHECK(ev->quality == odc::QualityFlags::ONLINE);
	}
	CHECK(LastValue(f, "POINT_0013_MW")->value == 112); //analog 13 sits two places earlier than configured
	CHECK(LastValue(f, "POINT_0020_MW") == nullptr);

	//report by exception, listing its members inline: only what changed
	const size_t before = f.events.size();
	f.SendReport("DS_BAD", {"POINT_0013_MW", "POINT_C003"}, true, 1000);
	REQUIRE(f.events.size() == before + 2);
	CHECK(f.events[before].name == "POINT_0013_MW");
	CHECK(f.events[before].value == 1112);
	CHECK(f.events[before + 1].name == "POINT_C003");
	CHECK(f.events[before + 1].value == 1009);

	//a peer that reports a member it said it does not have is not believed: the flagged point never gets a value
	f.SendReport("DS_BAD", {"POINT_0012_MW", "POINT_0014_MW"}, true, 5);
	CHECK(LastValue(f, "POINT_0012_MW") == nullptr);
	CHECK(f.EventsFor("POINT_0012_MW").size() == 1);
	CHECK(LastValue(f, "POINT_0014_MW")->value == 118);

	//a new association probes and flags again: the same four events
	Fixture g(ClientJson(NB, NA, NC, Prune));
	g.strictDefine = true;
	g.missing = missing;
	g.Run();
	CHECK(g.Flagged() == 4);
	CHECK(g.probes.size() == 1);
	CHECK(f.probes.size() == 1); //and the first association did not probe again
}

TEST_CASE(SUITE("prune: the quality of the flagged points is configurable"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune + ", \"MissingMemberQuality\": \"COMM_LOST | REFERENCE_ERR\""));
	REQUIRE(f.conf.missingMemberQualityText == "COMM_LOST|REFERENCE_ERR");
	f.strictDefine = true;
	f.missing = {AnalogName(11)};
	f.Run();
	REQUIRE(f.Flagged() == 1);
	CHECK(f.EventsFor("POINT_0012_MW")[0].quality == (odc::QualityFlags::COMM_LOST | odc::QualityFlags::REFERENCE_ERR));
	CHECK(f.Count("does not exist on the peer; flagged COMM_LOST|REFERENCE_ERR", LogLevel::Warn) == 1);
	CHECK(f.Count("1 points flagged COMM_LOST|REFERENCE_ERR") == 1);
}

TEST_CASE(SUITE("prune: configuration"))
{
	auto load = [](const std::string& extra)
			{
				Json::Value root;
				Json::CharReaderBuilder builder;
				std::string errs;
				std::istringstream in("{\"ServiceRole\": \"Client\", \"RemoteDomain\": \"D\"" + extra + "}");
				REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
				ICCPPortConf c;
				const auto problems = c.Load(root, false);
				return std::make_pair(c, problems);
			};
	{
		const auto [c, problems] = load("");
		CHECK(problems.empty());
		CHECK(c.missingMemberPolicy == MissingMemberPolicy::Prune);
		CHECK(c.missingMemberQuality == odc::QualityFlags::COMM_LOST);
		CHECK(c.missingMemberQualityText == "COMM_LOST");
	}
	{
		const auto [c, problems] = load(", \"MissingMemberPolicy\": \"Fail\", \"MissingMemberQuality\": \"REFERENCE_ERR\"");
		CHECK(problems.empty());
		CHECK(c.missingMemberPolicy == MissingMemberPolicy::Fail);
		CHECK(c.missingMemberQuality == odc::QualityFlags::REFERENCE_ERR);
	}
	//the same for a listener: the schema does not depend on the port type
	{
		Json::Value root;
		root["ServiceRole"] = "Peer";
		root["MissingMemberPolicy"] = "Fail";
		ICCPPortConf c;
		CHECK(c.Load(root, true).empty());
		CHECK(c.missingMemberPolicy == MissingMemberPolicy::Fail);
	}
	for(const char* bad : {", \"MissingMemberPolicy\": \"Skip\"", ", \"MissingMemberPolicy\": 1", ", \"MissingMemberQuality\": \"BOGUS\"", ", \"MissingMemberQuality\": \"\"",
	                       ", \"MissingMemberQuality\": \"COMM_LOST|\"", ", \"MissingMemberQuality\": \"ONLINE\""})
	{
		const auto [c, problems] = load(bad);
		INFO(bad);
		REQUIRE(problems.size() == 1);
		CHECK(problems[0].find("MissingMember") != std::string::npos);
	}
	//ONLINE with a validity problem is allowed (it is the "held" state)
	CHECK(load(", \"MissingMemberQuality\": \"ONLINE|COMM_LOST\"").second.empty());
}

TEST_CASE(SUITE("prune: a polled data set is defined without the missing members and its values reach the right points"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune, "\"POINT_0004_MW\", \"POINT_0012_MW\", \"POINT_0013_MW\""));
	f.strictDefine = true;
	f.missing = {AnalogName(11)};
	f.Run();
	const auto poll = DefinesOf(f, "DS_POLL");
	REQUIRE(poll.size() == 2);
	CHECK(poll[0].members == std::vector<std::string>({"POINT_0004_MW", "POINT_0012_MW", "POINT_0013_MW"})); //no special objects: no transfer set
	CHECK(poll[1].members == std::vector<std::string>({"POINT_0004_MW", "POINT_0013_MW"}));
	REQUIRE(f.status.size() == 3);
	CHECK(f.status[2].dataSet == "DS_POLL");
	CHECK(f.status[2].state == tase2::ProvisionState::DefinedPartial);
	CHECK(f.status[2].partial);
	CHECK(f.status[2].membersDefined == 2);
	//the poll: two values for the two members, mapped by the pruned list
	for(int i = 0; i < 100 && LastValue(f, "POINT_0013_MW") == nullptr; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
		f.session->Tick();
		f.link.Pump();
	}
	REQUIRE(LastValue(f, "POINT_0004_MW") != nullptr);
	REQUIRE(LastValue(f, "POINT_0013_MW") != nullptr);
	CHECK(LastValue(f, "POINT_0004_MW")->value == 103);
	CHECK(LastValue(f, "POINT_0013_MW")->value == 112);
	CHECK(LastValue(f, "POINT_0012_MW") == nullptr);
	CHECK(f.EventsFor("POINT_0012_MW").size() == 1); //the flag only
	CHECK(f.Count("polling data set DS_POLL failed") == 0);
	CHECK(f.Count("unexpected number of values") == 0);
}

TEST_CASE(SUITE("prune: the peer says object-exists to the pruned definition: delete, define again"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune));
	f.strictDefine = true;
	f.existsOnce = true;
	f.missing = {AnalogName(11)};
	f.Run();
	CHECK(DefinesOf(f, "DS_BAD").size() == 3); //as configured, pruned (object-exists), pruned again after the delete
	CHECK(f.status[0].state == tase2::ProvisionState::TransferStarted);
	CHECK(f.status[0].partial);
	CHECK(f.status[0].membersDefined == 29);
	CHECK(f.Unexpected(LogLevel::Error) == 0);
}

TEST_CASE(SUITE("prune: when the probe fails the data set is skipped and all its points are flagged"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune));
	f.strictDefine = true;
	f.missing = {AnalogName(11)};
	f.probeMode = Fixture::ProbeMode::Error;
	f.Run();
	REQUIRE(f.probes.size() == 1);
	CHECK_FALSE(f.probes[0].complete);
	CHECK(DefinesOf(f, "DS_BAD").size() == 1); //no second attempt
	CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
	CHECK_FALSE(f.status[0].partial);
	CHECK(f.Count("data set DS_BAD (domain DOMAIN_A) is skipped: the probe could not be completed", LogLevel::Error) == 1);
	CHECK(f.status[1].state == tase2::ProvisionState::TransferStarted);
	//DS_BAD's 28 points, less the two that DS_GOOD (which is fine) provides
	CHECK(f.Flagged() == 26);
	CHECK(f.Count("is not available because the data set could not be provisioned; flagged COMM_LOST", LogLevel::Warn) == 26);
	CHECK(f.Count("data set DS_BAD: member POINT_0012_MW (ICC, domain DOMAIN_A; ODC Analog index 11) is not available because the data set could not be provisioned; "
		"flagged COMM_LOST", LogLevel::Warn) == 1);
	CHECK(f.EventsFor("POINT_B0001").empty());
	CHECK(f.EventsFor("POINT_B0003").size() == 1);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD; 26 points flagged COMM_LOST (not available on the peer)", LogLevel::Error) == 1);
}

TEST_CASE(SUITE("prune: a probe reply that is garbage or a peer that rejects the probe falls back the same way"))
{
	for(auto mode : {Fixture::ProbeMode::TooFewResults, Fixture::ProbeMode::TooManyResults, Fixture::ProbeMode::WrongResponseKind, Fixture::ProbeMode::Reject})
	{
		Fixture f(ClientJson(NB, NA, NC, Prune));
		f.strictDefine = true;
		f.missing = {AnalogName(11)};
		f.probeMode = mode;
		f.Run();
		INFO("probe mode " << static_cast<int>(mode));
		REQUIRE(f.probes.size() == 1);
		CHECK_FALSE(f.probes[0].complete);
		CHECK(DefinesOf(f, "DS_BAD").size() == 1);
		CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
		CHECK(f.Flagged() == 26);
		CHECK(f.writes == 1);
		CHECK(f.link.a.conn.OutstandingRequests() == 0);
	}
}

TEST_CASE(SUITE("prune: a pruned definition the peer still refuses falls back, flagging the points"))
{
	Fixture f(ClientJson(NB, NA, NC, Prune));
	f.rejectDefine = {"DS_BAD"};
	f.missing = {AnalogName(11), CounterName(1)};
	f.Run();
	REQUIRE(DefinesOf(f, "DS_BAD").size() == 2);
	CHECK(DefinesOf(f, "DS_BAD")[1].members.size() == 28);
	CHECK(f.Count("DefineNamedVariableList (pruned to 28 of 30 members) failed for data set DS_BAD", LogLevel::Error) == 1);
	CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
	CHECK(f.Count("does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 2);
	CHECK(f.Count("is not available because the data set could not be provisioned", LogLevel::Warn) == 24);
	CHECK(f.Flagged() == 26);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD; 26 points flagged", LogLevel::Error) == 1);
}

TEST_CASE(SUITE("prune: every member missing, or a missing special object, is the old behaviour plus flags"))
{
	{
		Fixture f(ClientJson(NB, NA, NC, Prune));
		f.strictDefine = true;
		const auto all = PointNames(NB, NA, NC);
		f.missing = std::set<std::string>(all.begin(), all.end());
		f.Run();
		CHECK(f.defines.size() == 2); //each data set once: nothing was defined again
		CHECK(f.Count("is skipped: every point of the data set is missing on the peer", LogLevel::Error) == 2);
		CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
		CHECK(f.status[1].state == tase2::ProvisionState::DefineFailed);
		CHECK(f.Flagged() == 28);
		CHECK(f.Count("provisioning finished: 0 data sets ok, 2 failed: DS_BAD, DS_GOOD; 28 points flagged", LogLevel::Error) == 1);
		CHECK(f.writes == 0);
	}
	{
		//the peer lacks a special object: that is invisible to the probe (they are not read), so every point exists and there is nothing to prune
		Fixture f(ClientJson(NB, NA, NC, Prune));
		f.strictDefine = true;
		f.missing = {"Transfer_Set_Name"};
		f.Run();
		CHECK(f.defines.size() == 2);
		CHECK(f.Count("is skipped: every member exists on the peer, so there is nothing to prune", LogLevel::Error) == 2);
		CHECK(f.Flagged() == 28);
		CHECK(f.writes == 0);
	}
}

TEST_CASE(SUITE("prune: the Fail policy keeps the old behaviour: error, data set skipped, no flags, probe only for the diagnostics"))
{
	Fixture f(ClientJson(NB, NA, NC, ", \"MissingMemberPolicy\": \"Fail\""));
	f.strictDefine = true;
	f.missing = {AnalogName(11)};
	f.Run();
	CHECK(DefinesOf(f, "DS_BAD").size() == 1);
	REQUIRE(f.probes.size() == 1);
	CHECK(f.probes[0].missing == 1);
	CHECK(f.Count("DefineNamedVariableList failed", LogLevel::Error) == 1);
	CHECK(f.Count("missing member ", LogLevel::Error) == 1);
	CHECK(f.Count("flagged") == 0);
	CHECK(f.events.empty());
	CHECK(f.status[0].state == tase2::ProvisionState::DefineFailed);
	CHECK(f.Count("provisioning finished: 1 data sets ok, 1 failed: DS_BAD", LogLevel::Error) == 1);
}

TEST_CASE(SUITE("prune: 300 missing members are all listed, flagged once each, and the rest is mapped right"))
{
	constexpr size_t N_ANALOG = 400;
	Fixture f(ClientJson(NB, N_ANALOG, 0, Prune));
	f.strictDefine = true;
	std::set<std::string> missing;
	for(size_t i = 0; i < 300; ++i)
		missing.insert(AnalogName(i));
	f.missing = missing;
	f.Run();

	REQUIRE(f.probes.size() == 1);
	CHECK(f.probes[0].complete);
	CHECK(f.probes[0].members == 2 + NB + N_ANALOG);
	CHECK(f.probes[0].missing == 300);
	CHECK(f.probes[0].missingListed.size() == 300);
	CHECK(f.Count("does not exist on the peer; flagged COMM_LOST", LogLevel::Warn) == 300);
	CHECK(f.Unexpected(LogLevel::Error) == 0);
	CHECK(f.Flagged() == 300);
	for(size_t i = 0; i < 300; ++i)
	{
		const auto ev = f.EventsFor(AnalogName(i));
		REQUIRE(ev.size() == 1);
		CHECK(ev[0].unavailable);
		CHECK(ev[0].index == i);
	}
	const auto bad = DefinesOf(f, "DS_BAD");
	REQUIRE(bad.size() == 2);
	CHECK(bad[1].members.size() == 2 + NB + 100);
	CHECK(bad[1].members[2 + NB] == AnalogName(300));
	CHECK(f.Count("data set DS_BAD defined with 107 of 407 members (300 missing on the peer left out)", LogLevel::Info) == 1);
	CHECK(f.Count("provisioning finished: 2 data sets ok (1 partial), 0 failed; 300 points flagged", LogLevel::Warn) == 1);

	//positions shifted by 300: the values still reach their own points
	std::vector<std::string> present = PointNames(NB, 0, 0);
	for(size_t i = 300; i < N_ANALOG; ++i)
		present.push_back(AnalogName(i));
	f.SendReport("DS_BAD", present, false);
	CHECK(LastValue(f, AnalogName(300))->value == 400);
	CHECK(LastValue(f, AnalogName(399))->value == 499);
	CHECK(LastValue(f, AnalogName(0)) == nullptr);
	CHECK(f.EventsFor(AnalogName(0)).size() == 1);
}
