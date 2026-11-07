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
 * ReportAckTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Critical transfer reports and the time limit TLE (IEC 60870-6-503 clauses 5.3.10 and 7.2.5.5.2):
 *   - the encoding and decoding of Transfer_Report_ACK and Transfer_Report_NACK,
 *   - the server side (ResponderSession) against a scripted client in memory, with a fake clock: outstanding critical reports, acknowledgement,
 *     negative acknowledgement, timeout, cap, clearing, and TLE,
 *   - the client side (ClientSession) against a ResponderSession in memory: acknowledgements of critical reports, and the NACK of a report that
 *     is not from a transfer set the client started.
 *  The wire layout of the acknowledgement is not verified against a real peer. Synthetic names only.
 */

#include "StackTestHelpers.h"
#include "Tase2Client.h"
#include "Tase2Responder.h"
#include <catch.hpp>
#include <sstream>

#define SUITE(name) "ICCP critical reports - " name

using namespace iccp;
using iccp::stack::LogLevel;
namespace DC = iccp::tase2::DsCondition;

namespace
{
constexpr odc::msSinceEpoch_t Wall = 1700000000000;

//---------------------------------------------------------------------------------------------------------------------
// the encoding
//---------------------------------------------------------------------------------------------------------------------

const tase2::ObjectRef TransferSetA{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"};

//a tiny deterministic generator for the mutation test
struct Lcg
{
	uint32_t state;
	uint32_t Next()
	{
		state = state * 1664525u + 1013904223u;
		return state >> 8;
	}
};

//---------------------------------------------------------------------------------------------------------------------
// the server side against a scripted client
//---------------------------------------------------------------------------------------------------------------------

const char* const ServerJson = R"({
	"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Type": "RealQTimeTag"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"TransferSetPool": ["TS_POOL_001", "TS_POOL_002"]
	%EXTRA%
})";

ICCPPortConf LoadServerConf(const std::string& extra)
{
	std::string text = ServerJson;
	text.replace(text.find("%EXTRA%"), 7, extra);
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	ICCPPortConf c;
	const auto problems = c.Load(root, true);
	for(const auto& p : problems)
		UNSCOPED_INFO(p);
	REQUIRE(problems.empty());
	return c;
}

struct Params
{
	int64_t bufferTime = 0;
	int64_t interval = 0;
	int64_t integrityCheck = 0;
	int64_t tle = 0;
	uint8_t conditions = DC::ObjectChange;
	bool critical = true;
	bool rbe = false;
	bool allChanges = false;
};

struct CritFixture
{
	stacktest::Link link;
	ICCPPortConf conf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::vector<std::pair<LogLevel, std::string>> logs;
	std::vector<mms::ObjectName> members; //the data set the client defines
	stack::Clock::time_point start;
	size_t specials = 3;
	bool defined = false;

	explicit CritFixture(const std::string& extra = "", bool withStamp = true):
		conf(LoadServerConf(extra))
	{
		start = link.clock.now;
		model = std::make_unique<tase2::ResponderModel>(conf, true);
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = [this]() { return Wall + std::chrono::duration_cast<std::chrono::milliseconds>(link.clock.now - start).count(); };
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, tb, [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); });
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		members = {tase2::IccName("DOMAIN_A", tase2::Name::TransferSetName)};
		if(withStamp)
			members.push_back(tase2::IccName("DOMAIN_A", tase2::Name::TransferSetTimeStamp));
		members.push_back(tase2::IccName("DOMAIN_A", tase2::Name::DsConditionsDetected));
		specials = members.size();
		for(const char* p : {"POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"})
			members.push_back(tase2::IccName("DOMAIN_A", p));
	}

	stack::Outcome Request(mms::ConfirmedServiceRequest request)
	{
		stack::Outcome out;
		bool got = false;
		link.a.conn.SendRequest(std::move(request), [&](const stack::Outcome& o)
			{
				out = o;
				got = true;
			});
		link.Pump();
		REQUIRE(got);
		return out;
	}
	void DefineDataSet()
	{
		tase2::DataSetDefinition def;
		def.name = tase2::IccName("DOMAIN_A", "DS_TEST_001");
		def.members = members;
		REQUIRE(Request(tase2::BuildCreateDataSet(def)).Get<mms::DefineNamedVariableListResponse>() != nullptr);
		defined = true;
	}
	void WriteTransferSet(const Params& p, bool status, const char* name = "TS_POOL_001")
	{
		tase2::DsTransferSet t;
		t.dataSetName = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "DS_TEST_001"};
		t.bufferTime = p.bufferTime;
		t.interval = p.interval;
		t.integrityCheck = p.integrityCheck;
		t.tle = p.tle;
		t.critical = p.critical;
		t.conditionsRequested = tase2::DsConditions{p.conditions, tase2::DsConditionBitCount};
		t.rbe = p.rbe;
		t.allChangesMember = true;
		t.allChangesReported = p.allChanges;
		t.status = status;
		const auto o = Request(tase2::BuildStartTransfer(tase2::IccName("DOMAIN_A", name), t));
		const auto* wr = o.Get<mms::WriteResponse>();
		REQUIRE(wr != nullptr);
		REQUIRE(wr->results.size() == 1);
		REQUIRE(wr->results[0].success);
	}
	//define the data set (once) and start the transfer set; whatever the start sent is dropped
	void Start(const Params& p = Params(), const char* name = "TS_POOL_001")
	{
		if(!defined)
			DefineDataSet();
		WriteTransferSet(p, true, name);
		Advance(50);
		link.a.reports.clear();
	}
	//what the client sent is handed to the server session, as the port does
	void Route()
	{
		auto reports = std::move(link.b.reports);
		link.b.reports.clear();
		for(const auto& r : reports)
			routed.push_back(server->OnInformationReport(r));
	}
	std::vector<bool> routed; //the answer of OnInformationReport for each report the client sent
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		link.Pump();
		Route();
	}

	void Analog(size_t index, double value)
	{
		if(const auto changed = model->Update(PointKind::Analog, index, false, value, 0, odc::QualityFlags::ONLINE, Wall))
			server->OnPointChanged(*changed);
		link.Pump();
	}
	//the scripted client's reports
	void Send(const mms::InformationReport& r)
	{
		REQUIRE(link.a.conn.SendReport(r));
		link.Pump();
		Route();
	}
	void Ack(const std::string& transferSet, std::optional<int64_t> stamp)
	{
		Send(tase2::BuildTransferReportAck(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", transferSet}, stamp));
	}
	void Nack(const std::string& transferSet)
	{
		Send(tase2::BuildTransferReportNack(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", transferSet}));
	}

	//the transfer reports the server sent
	std::vector<tase2::TransferReport> Reports() const
	{
		std::vector<tase2::TransferReport> out;
		for(const auto& r : link.a.reports)
			out.push_back(tase2::ParseTransferReport(r, &members));
		return out;
	}
	//the time stamps the server put in its reports (the layout must have one)
	std::vector<int64_t> Stamps() const
	{
		std::vector<int64_t> out;
		for(const auto& r : Reports())
		{
			REQUIRE(r.timeStamp.has_value());
			out.push_back(*r.timeStamp);
		}
		return out;
	}
	//the points an inline report lists
	std::vector<std::string> Names(const mms::InformationReport& r) const
	{
		std::vector<std::string> out;
		if(const auto* list = std::get_if<mms::ListOfVariable>(&r.specification))
			for(size_t i = specials; i < list->size(); ++i)
				out.push_back((*list)[i].name.itemId);
		return out;
	}
	size_t Count(const std::string& part, LogLevel level) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if(l == level && t.find(part) != std::string::npos)
				++n;
		return n;
	}
	const tase2::ResponderSession::ReportStats& Stats() const
	{
		return server->ReportCounters();
	}
};
}

TEST_CASE(SUITE("the acknowledgement names the transfer set and echoes its time stamp"))
{
	const auto ack = tase2::BuildTransferReportAck(TransferSetA, 1700000123);
	const auto* list = std::get_if<mms::ListOfVariable>(&ack.specification);
	REQUIRE(list != nullptr);
	REQUIRE(list->size() == 2);
	//both variables are named in the domain of the transfer set
	CHECK((*list)[0].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_ACK"));
	CHECK((*list)[1].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Set_Time_Stamp"));
	REQUIRE(ack.results.size() == 2);
	REQUIRE(ack.results[0].success);
	CHECK(tase2::ParseObjectRef(ack.results[0].data) == TransferSetA); //the object name value is the transfer set name of the report
	REQUIRE(ack.results[1].success);
	CHECK(tase2::ParseTransferSetTimeStamp(ack.results[1].data) == 1700000123);

	//through the MMS codec: an unconfirmed PDU that reads back as the same report
	const auto bytes = mms::Encode(mms::Pdu(ack));
	const auto back = mms::Decode(bytes);
	const auto* report = std::get_if<mms::InformationReport>(&back);
	REQUIRE(report != nullptr);
	const auto parsed = tase2::ParseReportAck(*report);
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->negative);
	CHECK(parsed->transferSet == TransferSetA);
	CHECK(parsed->timeStamp == std::optional<int64_t>(1700000123));
}

TEST_CASE(SUITE("the acknowledgement of a report without a time stamp names the transfer set only"))
{
	const auto ack = tase2::BuildTransferReportAck(TransferSetA, std::nullopt);
	const auto* list = std::get_if<mms::ListOfVariable>(&ack.specification);
	REQUIRE(list != nullptr);
	REQUIRE(list->size() == 1);
	CHECK((*list)[0].name.itemId == "Transfer_Report_ACK");
	CHECK(ack.results.size() == 1);
	const auto parsed = tase2::ParseReportAck(ack);
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->timeStamp.has_value());
}

TEST_CASE(SUITE("the negative acknowledgement names the transfer set, in its domain"))
{
	const auto nack = tase2::BuildTransferReportNack(TransferSetA);
	const auto* list = std::get_if<mms::ListOfVariable>(&nack.specification);
	REQUIRE(list != nullptr);
	REQUIRE(list->size() == 1);
	CHECK((*list)[0].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_NACK"));
	REQUIRE(nack.results.size() == 1);
	CHECK(tase2::ParseObjectRef(nack.results[0].data) == TransferSetA);
	const auto bytes = mms::Encode(mms::Pdu(nack));
	const auto back = mms::Decode(bytes);
	const auto parsed = tase2::ParseReportAck(std::get<mms::InformationReport>(back));
	REQUIRE(parsed.has_value());
	CHECK(parsed->negative);
	CHECK(parsed->transferSet == TransferSetA);
	CHECK_FALSE(parsed->timeStamp.has_value());
}

TEST_CASE(SUITE("wire bytes (a regression pin: the layout is not verified against a real peer)"))
{
	auto hex = [](const ber::Bytes& b)
		     {
			     std::string s;
			     char buf[4];
			     for(auto c : b)
			     {
				     snprintf(buf, sizeof(buf), "%02X", c);
				     s += buf;
			     }
			     return s;
		     };
	CHECK(hex(mms::Encode(mms::Pdu(tase2::BuildTransferReportAck(TransferSetA, 1700000123)))) ==
		"A376A074A04E3023A021A11F1A08444F4D41494E5F411A135472616E736665725F5265706F72745F41434B3027A025A1231A08444F4D41494E5F411A175472616E736665725F5365745F54696D655F5374616D70"
		"A022A21A8601018A08444F4D41494E5F418A0B54535F504F4F4C5F30303185046553F17B");
	CHECK(hex(mms::Encode(mms::Pdu(tase2::BuildTransferReportNack(TransferSetA)))) ==
		"A348A046A0263024A022A1201A08444F4D41494E5F411A145472616E736665725F5265706F72745F4E41434B"
		"A01CA21A8601018A08444F4D41494E5F418A0B54535F504F4F4C5F303031");
}

TEST_CASE(SUITE("a vmd-specific transfer set is acknowledged with vmd-specific variables"))
{
	const tase2::ObjectRef vcc{tase2::Scope::Vcc, "", "TS_VCC_001"};
	const auto ack = tase2::BuildTransferReportAck(vcc, 5);
	const auto& list = std::get<mms::ListOfVariable>(ack.specification);
	CHECK(list[0].name == mms::ObjectName::Vmd("Transfer_Report_ACK"));
	CHECK(list[1].name == mms::ObjectName::Vmd("Transfer_Set_Time_Stamp"));
	const auto parsed = tase2::ParseReportAck(ack);
	REQUIRE(parsed.has_value());
	CHECK(parsed->transferSet == vcc);
}

TEST_CASE(SUITE("an acknowledgement is read leniently: any order, other variables, an unreadable time stamp"))
{
	//the time stamp first, an unrelated variable, then the acknowledgement
	mms::InformationReport r;
	r.specification = mms::ListOfVariable{
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Transfer_Set_Time_Stamp"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Something_Else"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_ACK"), std::nullopt}
	};
	r.results = {mms::AccessResult::Success(tase2::TransferSetTimeStampData(77)), mms::AccessResult::Success(mms::Data::Integer(1)),
		       mms::AccessResult::Success(tase2::ToData(TransferSetA))};
	auto parsed = tase2::ParseReportAck(r);
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->negative);
	CHECK(parsed->transferSet == TransferSetA);
	CHECK(parsed->timeStamp == std::optional<int64_t>(77));

	//a time stamp that is not an integer, or that failed, is as good as none
	r.results[0] = mms::AccessResult::Success(mms::Data::VisibleString("not a time"));
	parsed = tase2::ParseReportAck(r);
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->timeStamp.has_value());
	r.results[0] = mms::AccessResult::Failure(mms::DataAccessError::ObjectAccessDenied);
	parsed = tase2::ParseReportAck(r);
	REQUIRE(parsed.has_value());
	CHECK_FALSE(parsed->timeStamp.has_value());

	//the scope and domain of the variable names are not checked
	r.specification = mms::ListOfVariable{
		mms::VariableSpec{mms::ObjectName::Vmd("Transfer_Set_Time_Stamp"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Vmd("Something_Else"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Vmd("Transfer_Report_NACK"), std::nullopt}
	};
	parsed = tase2::ParseReportAck(r);
	REQUIRE(parsed.has_value());
	CHECK(parsed->negative);
}

TEST_CASE(SUITE("what is not an acknowledgement is recognised as such, and a broken one is a DecodeError"))
{
	//an ordinary transfer report (inline and by data set name)
	tase2::TransferReport tr;
	tr.transferSet = TransferSetA;
	tr.conditionsDetected = tase2::DsConditions{DC::ObjectChange, tase2::DsConditionBitCount};
	tr.layout = {tase2::Special::TransferSetName, tase2::Special::ConditionsDetected};
	tr.specification = mms::ListOfVariable{mms::VariableSpec{tase2::IccName("DOMAIN_A", tase2::Name::TransferSetName), std::nullopt},
		                                 mms::VariableSpec{tase2::IccName("DOMAIN_A", tase2::Name::DsConditionsDetected), std::nullopt}
	};
	CHECK_FALSE(tase2::ParseReportAck(tase2::BuildTransferReport(tr)).has_value());
	tr.specification = tase2::IccName("DOMAIN_A", "DS_TEST_001");
	CHECK_FALSE(tase2::ParseReportAck(tase2::BuildTransferReport(tr)).has_value());
	//no variables at all
	CHECK_FALSE(tase2::ParseReportAck(mms::InformationReport{}).has_value());

	//the object name is not readable
	auto ack = tase2::BuildTransferReportAck(TransferSetA, 1);
	ack.results[0] = mms::AccessResult::Failure(mms::DataAccessError::ObjectAccessDenied);
	CHECK_THROWS_AS(tase2::ParseReportAck(ack), mms::DecodeError);
	ack.results[0] = mms::AccessResult::Success(mms::Data::Integer(5));
	CHECK_THROWS_AS(tase2::ParseReportAck(ack), mms::DecodeError);
	//more variables than results: not one of ours
	ack = tase2::BuildTransferReportAck(TransferSetA, 1);
	ack.results.pop_back();
	CHECK_FALSE(tase2::ParseReportAck(ack).has_value());
}

TEST_CASE(SUITE("truncated and mutated acknowledgements only ever decode or throw DecodeError"))
{
	const auto bytes = mms::Encode(mms::Pdu(tase2::BuildTransferReportAck(TransferSetA, 1700000123)));
	auto attempt = [](const ber::Bytes& b)
			   {
				   try
				   {
					   const auto pdu = mms::Decode(b);
					   if(const auto* r = std::get_if<mms::InformationReport>(&pdu))
						   (void)tase2::ParseReportAck(*r);
				   }
				   catch(const ber::DecodeError&)
				   {}
			   };
	for(size_t n = 0; n < bytes.size(); ++n)
		attempt(ber::Bytes(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n)));
	Lcg rng{12345};
	for(int i = 0; i < 4000; ++i)
	{
		ber::Bytes b = bytes;
		const int flips = 1 + static_cast<int>(rng.Next() % 3);
		for(int f = 0; f < flips; ++f)
			b[rng.Next() % b.size()] = static_cast<uint8_t>(rng.Next());
		attempt(b);
	}
	SUCCEED();
}

//---------------------------------------------------------------------------------------------------------------------
// the server: outstanding critical reports
//---------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("a critical report is outstanding until its acknowledgement arrives"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	CHECK(f.server->OutstandingCriticalReports("TS_POOL_001") == 1);
	CHECK(f.Stats().criticalSent == 1);
	const auto stamps = f.Stamps();
	f.Ack("TS_POOL_001", stamps[0]);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 1);
	CHECK(f.Stats().dataFailures == 0);
	REQUIRE(f.routed.size() == 1);
	CHECK(f.routed[0]); //the report was consumed by the server side
	//nothing happens afterwards, however long it takes
	f.Advance(120000);
	CHECK(f.Stats().dataFailures == 0);
	CHECK(f.Stats().ackTimeouts == 0);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 0);
}

TEST_CASE(SUITE("an acknowledgement with another time stamp does not clear the report; it times out, once"))
{
	CritFixture f(R"(, "ReportAckTimeoutms": 1000)");
	f.Start();
	f.Analog(0, 1);
	const auto stamp = f.Stamps()[0];
	f.Ack("TS_POOL_001", stamp + 5);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	CHECK(f.Stats().acknowledged == 0);
	f.Advance(500);
	CHECK(f.Stats().dataFailures == 0);
	f.Advance(600);
	CHECK(f.Stats().ackTimeouts == 1);
	CHECK(f.Stats().dataFailures == 1);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	//one warning for the report, naming the transfer set and the condition
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 1);
	CHECK(f.Count("TS_POOL_001", LogLevel::Warn) == 1);
	CHECK(f.Count("ReportAckTimeoutms", LogLevel::Warn) == 1);
	f.Advance(5000);
	f.Advance(5000);
	CHECK(f.Stats().dataFailures == 1);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 1);
	//a late acknowledgement matches nothing: no change, no failure
	f.Ack("TS_POOL_001", stamp);
	CHECK(f.Stats().acknowledged == 0);
	CHECK(f.Stats().dataFailures == 1);
	CHECK(f.Count("matches no outstanding", LogLevel::Debug) >= 2);
	//the association is untouched, and the data goes on
	CHECK(f.link.a.conn.IsAssociated());
	f.link.a.reports.clear();
	f.Analog(0, 2);
	CHECK(f.link.a.reports.size() == 1);
}

TEST_CASE(SUITE("acknowledgements match the report by its time stamp, in any order"))
{
	CritFixture f;
	f.Start();
	for(int i = 1; i <= 3; ++i)
	{
		f.Analog(0, i);
		f.Advance(1000); //the time stamp has a resolution of a second
	}
	const auto stamps = f.Stamps();
	REQUIRE(stamps.size() == 3);
	CHECK(stamps[0] != stamps[1]);
	CHECK(stamps[1] != stamps[2]);
	CHECK(f.server->OutstandingCriticalReports() == 3);
	f.Ack("TS_POOL_001", stamps[1]);
	CHECK(f.server->OutstandingCriticalReports() == 2);
	f.Ack("TS_POOL_001", stamps[1]); //again: nothing is left for it
	CHECK(f.server->OutstandingCriticalReports() == 2);
	f.Ack("TS_POOL_001", stamps[2]);
	f.Ack("TS_POOL_001", stamps[0]);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 3);
	CHECK(f.Stats().criticalSent == 3);
	CHECK(f.Stats().dataFailures == 0);
}

TEST_CASE(SUITE("two reports in the same second share a time stamp: each acknowledgement takes the oldest of them"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	f.Analog(0, 2);
	const auto stamps = f.Stamps();
	REQUIRE(stamps.size() == 2);
	CHECK(stamps[0] == stamps[1]);
	f.Ack("TS_POOL_001", stamps[0]);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	f.Ack("TS_POOL_001", stamps[0]);
	CHECK(f.server->OutstandingCriticalReports() == 0);
}

TEST_CASE(SUITE("an acknowledgement without a time stamp matches the oldest outstanding report"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	f.Advance(1000);
	f.Analog(0, 2);
	const auto stamps = f.Stamps();
	REQUIRE(stamps.size() == 2);
	f.Ack("TS_POOL_001", std::nullopt);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	CHECK(f.Count("without a time stamp", LogLevel::Debug) == 1);
	f.Ack("TS_POOL_001", stamps[0]); //the oldest has gone: this one is for nothing
	CHECK(f.server->OutstandingCriticalReports() == 1);
	f.Ack("TS_POOL_001", stamps[1]);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 2);
}

TEST_CASE(SUITE("reports without a Transfer_Set_Time_Stamp are acknowledged by the oldest-first rule"))
{
	CritFixture f("", false); //the data set has no time stamp object: the reports carry none to echo
	f.Start();
	f.Analog(0, 1);
	f.Analog(0, 2);
	REQUIRE(f.link.a.reports.size() == 2);
	for(const auto& r : f.Reports())
		CHECK_FALSE(r.timeStamp.has_value());
	f.Ack("TS_POOL_001", std::nullopt);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	f.Ack("TS_POOL_001", 12345); //a time stamp the report never had matches any
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 2);
}

TEST_CASE(SUITE("an acknowledgement with other variables, in another order, is accepted"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	const auto stamp = f.Stamps()[0];
	mms::InformationReport r;
	r.specification = mms::ListOfVariable{
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Transfer_Set_Time_Stamp"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Something_Else"), std::nullopt},
		mms::VariableSpec{mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_ACK"), std::nullopt}
	};
	r.results = {mms::AccessResult::Success(tase2::TransferSetTimeStampData(stamp)), mms::AccessResult::Success(mms::Data::Integer(1)),
		       mms::AccessResult::Success(tase2::ToData(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"}))};
	f.Send(r);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 1);
}

TEST_CASE(SUITE("a Transfer_Report_NACK is a Data_failure: counted, logged, and the report is no longer outstanding"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	f.Nack("TS_POOL_001");
	CHECK(f.Stats().nacksReceived == 1);
	CHECK(f.Stats().dataFailures == 1);
	CHECK(f.Stats().ackTimeouts == 0);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 1);
	CHECK(f.Count("TS_POOL_001", LogLevel::Warn) == 1);
	CHECK(f.Count("Transfer_Report_NACK", LogLevel::Warn) == 1);
	//no timeout follows for it
	f.Advance(60000);
	CHECK(f.Stats().dataFailures == 1);
	//nothing is resent or held back, and the association stays up
	CHECK(f.link.a.conn.IsAssociated());
	f.link.a.reports.clear();
	f.Analog(0, 2);
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.server->OutstandingCriticalReports() == 1);
}

TEST_CASE(SUITE("a NACK with nothing outstanding is still a failure, and says so"))
{
	CritFixture f;
	f.Start();
	f.Nack("TS_POOL_001");
	CHECK(f.Stats().nacksReceived == 1);
	CHECK(f.Stats().dataFailures == 1);
	CHECK(f.Count("none outstanding matched", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("a NACK for a transfer set that is not critical is logged and counted as a NACK, not as a Data_failure"))
{
	CritFixture f;
	Params p;
	p.critical = false;
	f.Start(p);
	f.Analog(0, 1);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	f.Nack("TS_POOL_001");
	CHECK(f.Stats().nacksReceived == 1);
	CHECK(f.Stats().dataFailures == 0);
	CHECK(f.Count("not critical", LogLevel::Warn) == 1);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 0);
}

TEST_CASE(SUITE("with MaxUnacknowledgedReports the oldest outstanding report becomes a failure, whatever the timeout setting"))
{
	CritFixture f(R"(, "MaxUnacknowledgedReports": 3, "ReportAckTimeoutms": 0)");
	f.Start();
	for(int i = 1; i <= 5; ++i)
	{
		f.Analog(0, i);
		f.Advance(1000);
	}
	const auto stamps = f.Stamps();
	REQUIRE(stamps.size() == 5);
	CHECK(f.Stats().criticalSent == 5);
	CHECK(f.server->OutstandingCriticalReports() == 3);
	CHECK(f.Stats().ackOverflows == 2);
	CHECK(f.Stats().dataFailures == 2);
	CHECK(f.Count("MaxUnacknowledgedReports", LogLevel::Warn) == 2);
	//the oldest two were given up: acknowledging them matches nothing, the other three match
	f.Ack("TS_POOL_001", stamps[0]);
	f.Ack("TS_POOL_001", stamps[1]);
	CHECK(f.server->OutstandingCriticalReports() == 3);
	f.Ack("TS_POOL_001", stamps[4]);
	f.Ack("TS_POOL_001", stamps[2]);
	f.Ack("TS_POOL_001", stamps[3]);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().acknowledged == 3);
}

TEST_CASE(SUITE("ReportAckTimeoutms 0 switches the timeout check off: nothing times out"))
{
	CritFixture f(R"(, "ReportAckTimeoutms": 0)");
	f.Start();
	f.Analog(0, 1);
	for(int i = 0; i < 10; ++i)
		f.Advance(360000); //an hour
	CHECK(f.Stats().ackTimeouts == 0);
	CHECK(f.Stats().dataFailures == 0);
	CHECK(f.server->OutstandingCriticalReports() == 1);
}

TEST_CASE(SUITE("each critical transfer set keeps its own outstanding reports"))
{
	CritFixture f;
	f.Start(Params(), "TS_POOL_001");
	f.Start(Params(), "TS_POOL_002");
	f.Analog(0, 1); //both report it
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.server->OutstandingCriticalReports("TS_POOL_001") == 1);
	CHECK(f.server->OutstandingCriticalReports("TS_POOL_002") == 1);
	f.Ack("TS_POOL_002", f.Reports()[1].timeStamp);
	CHECK(f.server->OutstandingCriticalReports("TS_POOL_001") == 1);
	CHECK(f.server->OutstandingCriticalReports("TS_POOL_002") == 0);
	f.Nack("TS_POOL_001");
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().dataFailures == 1);
}

TEST_CASE(SUITE("stopping the transfer set, or starting it again, clears its outstanding reports without a failure"))
{
	CritFixture f(R"(, "ReportAckTimeoutms": 1000)");
	f.Start();
	f.Analog(0, 1);
	f.Analog(0, 2);
	CHECK(f.server->OutstandingCriticalReports() == 2);
	f.WriteTransferSet(Params(), false); //Stop Transfer
	CHECK(f.server->OutstandingCriticalReports() == 0);
	f.Advance(10000);
	CHECK(f.Stats().dataFailures == 0);
	CHECK(f.Stats().ackTimeouts == 0);
	//a restart begins empty too
	f.WriteTransferSet(Params(), true);
	f.Analog(0, 3);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	f.WriteTransferSet(Params(), true);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	f.Advance(10000);
	CHECK(f.Stats().dataFailures == 0);
}

TEST_CASE(SUITE("deleting the data set stops the transfer set and clears its outstanding reports"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	const auto o = f.Request(tase2::BuildDeleteDataSet(tase2::IccName("DOMAIN_A", "DS_TEST_001")));
	REQUIRE(o.Get<mms::DeleteNamedVariableListResponse>() != nullptr);
	CHECK_FALSE(f.server->TransferSetEnabled("TS_POOL_001"));
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Stats().dataFailures == 0);
}

TEST_CASE(SUITE("the end of the association clears the outstanding reports without a failure"))
{
	CritFixture f(R"(, "ReportAckTimeoutms": 1000)");
	f.Start();
	f.Analog(0, 1);
	f.Analog(0, 2);
	CHECK(f.server->OutstandingCriticalReports() == 2);
	f.server->AssociationEnded();
	CHECK(f.server->OutstandingCriticalReports() == 0);
	f.Advance(10000);
	CHECK(f.Stats().dataFailures == 0);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 0);
}

TEST_CASE(SUITE("a transfer set that is not critical is not tracked at all, and its reports are the same as a critical set's"))
{
	CritFixture plain;
	Params notCritical;
	notCritical.critical = false;
	plain.Start(notCritical);
	CritFixture critical;
	critical.Start();
	for(auto* f : {&plain, &critical})
	{
		f->Analog(0, 1);
		f->Advance(1000);
		f->Analog(1, 2);
		f->Advance(1000);
	}
	REQUIRE(plain.link.a.reports.size() == 2);
	REQUIRE(critical.link.a.reports.size() == 2);
	//the same bytes: Critical only decides what the server remembers
	for(size_t i = 0; i < 2; ++i)
		CHECK(mms::Encode(mms::Pdu(plain.link.a.reports[i])) == mms::Encode(mms::Pdu(critical.link.a.reports[i])));
	CHECK(plain.server->OutstandingCriticalReports() == 0);
	CHECK(plain.Stats().criticalSent == 0);
	CHECK(critical.server->OutstandingCriticalReports() == 2);
	//however long it takes, and whatever the client says, nothing becomes a failure
	plain.Advance(600000);
	plain.Ack("TS_POOL_001", plain.Stamps()[0]);
	plain.Ack("TS_POOL_001", std::nullopt);
	CHECK(plain.Stats().dataFailures == 0);
	CHECK(plain.Stats().ackTimeouts == 0);
	CHECK(plain.Stats().acknowledged == 0);
	CHECK(plain.Stats().nacksReceived == 0);
	CHECK(plain.Count("Data_failure", LogLevel::Warn) == 0);
}

TEST_CASE(SUITE("an acknowledgement for a transfer set that is not here, or in another domain, is consumed and ignored"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	f.Ack("TS_NOT_HERE", 1);
	f.Send(tase2::BuildTransferReportAck(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_OTHER", "TS_POOL_001"}, f.Stamps()[0]));
	f.Send(tase2::BuildTransferReportNack(tase2::ObjectRef{tase2::Scope::Vcc, "", "TS_POOL_001"}));
	CHECK(f.server->OutstandingCriticalReports() == 1);
	CHECK(f.Stats().acknowledged == 0);
	CHECK(f.Stats().nacksReceived == 0);
	CHECK(f.Stats().dataFailures == 0);
	REQUIRE(f.routed.size() == 3);
	for(bool consumed : f.routed)
		CHECK(consumed);
}

TEST_CASE(SUITE("a broken acknowledgement is consumed with a warning; a report that is not an acknowledgement is left to the client side"))
{
	CritFixture f;
	f.Start();
	f.Analog(0, 1);
	auto broken = tase2::BuildTransferReportAck(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"}, 1);
	broken.results[0] = mms::AccessResult::Success(mms::Data::Integer(9));
	f.Send(broken);
	REQUIRE(f.routed.size() == 1);
	CHECK(f.routed[0]);
	CHECK(f.Count("could not be read", LogLevel::Warn) == 1);
	CHECK(f.server->OutstandingCriticalReports() == 1);
	//a transfer report from the peer (it is also a client) is not ours
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK_FALSE(f.server->OnInformationReport(f.link.a.reports[0]));
}

//---------------------------------------------------------------------------------------------------------------------
// the server: TLE
//---------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("TLE 0: a late report is sent whatever its age"))
{
	CritFixture f;
	Params p;
	p.bufferTime = 2;
	f.Start(p);
	f.Analog(0, 1);
	f.Advance(2000 + 30000);
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.Stats().tleDiscarded == 0);
}

TEST_CASE(SUITE("TLE: a report handed over more than TLE after it was due is discarded, once logged and counted, and the next is sent regardless"))
{
	CritFixture f;
	Params p;
	p.bufferTime = 2;
	p.tle = 5;
	p.critical = false;
	f.Start(p);
	//the buffer time ends 2 s after the change; the tick comes 8 s late
	f.Analog(0, 1);
	f.Advance(2000 + 8000);
	CHECK(f.link.a.reports.empty());
	CHECK(f.server->ReportsSent() == 0);
	CHECK(f.Stats().tleDiscarded == 1);
	CHECK(f.Count("TLE", LogLevel::Warn) == 1);
	CHECK(f.Count("TS_POOL_001", LogLevel::Warn) == 1);
	CHECK(f.server->TransferSetEnabled("TS_POOL_001")); //the transfer set goes on
	CHECK(f.link.a.conn.IsAssociated());
	//the next one is as late, and is sent: the previous one exceeded TLE
	f.Analog(0, 2);
	f.Advance(2000 + 8000);
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.Stats().tleDiscarded == 1);
	CHECK(f.Count("sent regardless", LogLevel::Debug) == 1);
	//and after that the rule applies again
	f.link.a.reports.clear();
	f.Analog(0, 3);
	f.Advance(2000 + 8000);
	CHECK(f.link.a.reports.empty());
	CHECK(f.Stats().tleDiscarded == 2);
	//a report that is on time is sent, and clears the exception too
	f.Analog(0, 4);
	f.Advance(2000);
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.Stats().tleDiscarded == 2);
}

TEST_CASE(SUITE("TLE: exactly TLE late is still in time, a millisecond more is not"))
{
	Params p;
	p.bufferTime = 2;
	p.tle = 5;
	{
		CritFixture f;
		f.Start(p);
		f.Analog(0, 1);
		f.Advance(2000 + 5000);
		CHECK(f.link.a.reports.size() == 1);
		CHECK(f.Stats().tleDiscarded == 0);
	}
	{
		CritFixture f;
		f.Start(p);
		f.Analog(0, 1);
		f.Advance(2000 + 5001);
		CHECK(f.link.a.reports.empty());
		CHECK(f.Stats().tleDiscarded == 1);
	}
}

TEST_CASE(SUITE("TLE: an interval or integrity report is late from the time it was due"))
{
	Params p;
	p.interval = 10;
	p.integrityCheck = 100;
	p.conditions = DC::IntervalTimeOut | DC::IntegrityTimeOut;
	p.tle = 3;
	p.critical = false;
	CritFixture f;
	f.Start(p); //the integrity report that follows the start has been dropped by Start()
	f.Advance(9000);
	CHECK(f.link.a.reports.empty());
	f.Advance(2000); //due at 10 s, now 11.05 s: in time
	CHECK(f.link.a.reports.size() == 1);
	f.link.a.reports.clear();
	f.Advance(15000); //due at 20 s, now 26.05 s: 6 s late
	CHECK(f.link.a.reports.empty());
	CHECK(f.Stats().tleDiscarded == 1);
	f.Advance(4000); //the next is due at 30 s, now 30.05 s: in time anyway
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.Stats().tleDiscarded == 1);
}

TEST_CASE(SUITE("TLE: reports that are sent as soon as they arise (no buffer time, an operator request) are never late"))
{
	Params p;
	p.tle = 1;
	p.conditions = DC::ObjectChange | DC::OperatorRequest;
	CritFixture f;
	f.Start(p);
	f.Advance(60000);
	f.Analog(0, 1);
	CHECK(f.link.a.reports.size() == 1);
	CHECK(f.server->RaiseCondition(DC::OperatorRequest, 0) == 1);
	CHECK(f.Stats().tleDiscarded == 0);
}

TEST_CASE(SUITE("TLE with AllChangesReported: a discarded report drops only its own changes"))
{
	Params p;
	p.bufferTime = 2;
	p.tle = 5;
	p.rbe = true;
	p.allChanges = true;
	p.critical = false;
	CritFixture f;
	f.Start(p);
	//A1 B1 A2: the reports are [A1 B1] and [A2]
	f.Analog(0, 1);
	f.Analog(1, 10);
	f.Analog(0, 2);
	f.Advance(2000 + 8000);
	REQUIRE(f.link.a.reports.size() == 1); //the first report is discarded, the next one (its exception) is sent
	CHECK(f.Names(f.link.a.reports[0]) == std::vector<std::string>({"POINT_0001_MW"}));
	CHECK(f.Stats().tleDiscarded == 1);
}

TEST_CASE(SUITE("a report discarded for TLE is not a critical report: nothing is waiting for its acknowledgement"))
{
	Params p;
	p.bufferTime = 2;
	p.tle = 5;
	CritFixture f;
	f.Start(p);
	f.Analog(0, 1);
	f.Advance(2000 + 8000);
	CHECK(f.Stats().tleDiscarded == 1);
	CHECK(f.Stats().criticalSent == 0);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	f.Advance(60000);
	CHECK(f.Stats().dataFailures == 0);
}

//---------------------------------------------------------------------------------------------------------------------
// the client side against a ResponderSession
//---------------------------------------------------------------------------------------------------------------------

namespace
{
const char* const ClientJson = R"({
	"ServiceRole": "Client", "LocalDomain": "DOMAIN_B", "RemoteDomain": "DOMAIN_A", "RemoteBilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Type": "RealQTimeTag"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"]}],
	"TransferSets": [{"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange"] %TRANSFER_SET%}]
	%EXTRA%
})";

//the client's port configuration against a server's
struct PairFixture
{
	stacktest::Link link;
	ICCPPortConf serverConf;
	ICCPPortConf clientConf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::unique_ptr<tase2::ClientSession> client;
	std::vector<std::pair<LogLevel, std::string>> serverLogs;
	std::vector<std::pair<LogLevel, std::string>> clientLogs;
	std::vector<mms::InformationReport> toClient; //the reports the server side sent, in order
	std::vector<mms::InformationReport> toServer; //the reports the client side sent, in order
	struct Published
	{
		std::string name;
		double value;
	};
	std::vector<Published> published;
	stack::Clock::time_point start;

	//transferSet: the keys that follow "Conditions" in the transfer set object (starting with ", "); extra: top level keys (starting with ", ")
	PairFixture(const std::string& transferSet, const std::string& extra = "", const std::string& serverExtra = ""):
		serverConf(LoadServerConf(serverExtra))
	{
		std::string text = ClientJson;
		text.replace(text.find("%TRANSFER_SET%"), 14, transferSet);
		text.replace(text.find("%EXTRA%"), 7, extra);
		Json::Value root;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream in(text);
		REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
		const auto problems = clientConf.Load(root, false);
		for(const auto& p : problems)
			UNSCOPED_INFO(p);
		REQUIRE(problems.empty());

		start = link.clock.now;
		model = std::make_unique<tase2::ResponderModel>(serverConf, true);
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = [this]() { return Wall + std::chrono::duration_cast<std::chrono::milliseconds>(link.clock.now - start).count(); };
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, tb, [this](LogLevel l, const std::string& t) { serverLogs.emplace_back(l, t); });
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		tase2::ClientSession::Callbacks cb;
		cb.onPoint = [this](const IccpPointConf& pt, const tase2::PointState& s) { published.push_back(Published{pt.name, tase2::NumericValue(pt, s)}); };
		client = std::make_unique<tase2::ClientSession>(clientConf, link.a.conn, tb, [this](LogLevel l, const std::string& t) { clientLogs.emplace_back(l, t); }, std::move(cb));
		client->OnAssociated();
		Settle();
		REQUIRE(client->Ready());
		REQUIRE(server->TransferSetEnabled("TS_POOL_001"));
	}

	//deliver what each side sent to the other's report handler, until nothing more happens
	void Settle()
	{
		for(int i = 0; i < 50; ++i)
		{
			link.Pump();
			bool any = false;
			if(!link.a.reports.empty())
			{
				auto reports = std::move(link.a.reports);
				link.a.reports.clear();
				for(const auto& r : reports)
				{
					toClient.push_back(r);
					client->OnReport(r);
				}
				any = true;
			}
			if(!link.b.reports.empty())
			{
				auto reports = std::move(link.b.reports);
				link.b.reports.clear();
				for(const auto& r : reports)
				{
					toServer.push_back(r);
					server->OnInformationReport(r);
				}
				any = true;
			}
			if(!any)
				break;
		}
		link.Pump();
	}
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		client->Tick();
		Settle();
	}
	void Analog(size_t index, double value)
	{
		if(const auto changed = model->Update(PointKind::Analog, index, false, value, 0, odc::QualityFlags::ONLINE, Wall))
			server->OnPointChanged(*changed);
		Settle();
	}

	//a transfer report as a peer's server would send it, from a transfer set of the given name, with the point values the server has
	mms::InformationReport Report(const tase2::ObjectRef& transferSet, bool named, std::optional<int64_t> stamp = 1700000000)
	{
		tase2::TransferReport tr;
		tr.layout = {tase2::Special::TransferSetName, tase2::Special::TimeStamp, tase2::Special::ConditionsDetected};
		tr.transferSet = transferSet;
		tr.timeStamp = stamp;
		tr.conditionsDetected = tase2::DsConditions{DC::ObjectChange, tase2::DsConditionBitCount};
		mms::ListOfVariable list;
		for(const char* special : {tase2::Name::TransferSetName, tase2::Name::TransferSetTimeStamp, tase2::Name::DsConditionsDetected})
			list.push_back(mms::VariableSpec{tase2::IccName("DOMAIN_A", special), std::nullopt});
		for(const char* name : {"POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"})
		{
			const auto n = tase2::IccName("DOMAIN_A", name);
			const auto index = model->FindPoint(n);
			REQUIRE(index.has_value());
			list.push_back(mms::VariableSpec{n, std::nullopt});
			tr.values.push_back(mms::AccessResult::Success(tase2::ToData(model->Point(*index), model->State(*index), Wall)));
		}
		if(named)
			tr.specification = tase2::IccName("DOMAIN_A", "DS_TEST_001");
		else
			tr.specification = list;
		return tase2::BuildTransferReport(tr);
	}
	//the reports the server side saw come in, as acknowledgements
	std::vector<tase2::ReportAck> Acks() const
	{
		std::vector<tase2::ReportAck> out;
		for(const auto& r : toServer)
			if(const auto a = tase2::ParseReportAck(r))
				out.push_back(*a);
		return out;
	}
	size_t Count(const std::vector<std::pair<LogLevel, std::string>>& logs, const std::string& part, LogLevel level) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if(l == level && t.find(part) != std::string::npos)
				++n;
		return n;
	}
	size_t PublishedCount(const std::string& name) const
	{
		size_t n = 0;
		for(const auto& p : published)
			n += p.name == name ? 1 : 0;
		return n;
	}
};

const char* const CriticalSet = R"(, "Critical": true, "RBE": true, "SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"])";
}

TEST_CASE(SUITE("client: a report of a critical transfer set is acknowledged with the transfer set name and the time stamp of the report"))
{
	PairFixture f(CriticalSet);
	f.published.clear();
	f.Analog(0, 11);
	REQUIRE(f.toClient.size() >= 1);
	CHECK(f.published.size() == 1); //the value was processed
	const auto acks = f.Acks();
	REQUIRE(acks.size() == 1);
	CHECK_FALSE(acks[0].negative);
	CHECK(acks[0].transferSet == tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"});
	//the time stamp of the report received
	const auto report = tase2::ParseTransferReport(f.toClient.back(), nullptr);
	REQUIRE(report.timeStamp.has_value());
	CHECK(acks[0].timeStamp == report.timeStamp);
	//the server's list is empty, and nothing failed
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().criticalSent == 1);
	CHECK(f.server->ReportCounters().acknowledged == 1);
	CHECK(f.server->ReportCounters().dataFailures == 0);
	CHECK(f.client->ReportAcksSent() == 1);
	CHECK(f.client->ReportNacksSent() == 0);
	//the acknowledgement is the variables the standard names, in the domain of the transfer set
	const auto& list = std::get<mms::ListOfVariable>(f.toServer.back().specification);
	REQUIRE(list.size() == 2);
	CHECK(list[0].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_ACK"));
	CHECK(list[1].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Set_Time_Stamp"));
	//many reports, each acknowledged
	for(int i = 0; i < 5; ++i)
	{
		f.Advance(1000);
		f.Analog(1, 20 + i);
	}
	CHECK(f.client->ReportAcksSent() == 6);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().acknowledged == 6);
	f.Advance(60000);
	CHECK(f.server->ReportCounters().dataFailures == 0);
}

TEST_CASE(SUITE("client: reports without a time stamp object are acknowledged by name only, and the server still clears them"))
{
	PairFixture f(R"(, "Critical": true, "RBE": true)"); //the default special objects: the transfer set name and the conditions
	f.Analog(0, 11);
	f.Analog(0, 12);
	const auto acks = f.Acks();
	REQUIRE(acks.size() == 2);
	for(const auto& a : acks)
		CHECK_FALSE(a.timeStamp.has_value());
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().acknowledged == 2);
	CHECK(f.server->ReportCounters().dataFailures == 0);
}

TEST_CASE(SUITE("client: a transfer set that is not critical is not acknowledged"))
{
	PairFixture f(R"(, "RBE": true)");
	f.published.clear();
	f.Analog(0, 11);
	CHECK(f.published.size() == 1);
	CHECK(f.toServer.empty());
	CHECK(f.client->ReportAcksSent() == 0);
	CHECK(f.server->ReportCounters().criticalSent == 0);
	CHECK(f.server->OutstandingCriticalReports() == 0);
}

TEST_CASE(SUITE("client: AcknowledgeCriticalReports false sends none; the server counts each as a failure after the timeout, and the data still flows"))
{
	PairFixture f(CriticalSet, R"(, "AcknowledgeCriticalReports": false)", R"(, "ReportAckTimeoutms": 2000)");
	f.published.clear();
	f.Analog(0, 11);
	f.Advance(500);
	f.Analog(0, 12);
	CHECK(f.published.size() == 2);
	CHECK(f.client->ReportAcksSent() == 0);
	CHECK(f.server->OutstandingCriticalReports() == 2);
	f.Advance(1000);
	CHECK(f.server->ReportCounters().dataFailures == 0);
	f.Advance(600); //2.1 s after the first, 1.6 s after the second
	CHECK(f.server->ReportCounters().ackTimeouts == 1);
	f.Advance(1000);
	CHECK(f.server->ReportCounters().ackTimeouts == 2);
	CHECK(f.server->ReportCounters().dataFailures == 2);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.Count(f.serverLogs, "Data_failure", LogLevel::Warn) == 2);
	//the association is untouched, and the reports keep coming
	CHECK(f.link.a.conn.IsAssociated());
	f.Analog(0, 13);
	CHECK(f.published.size() == 3);
}

TEST_CASE(SUITE("client: a report from a transfer set it did not start is discarded and answered with Transfer_Report_NACK"))
{
	PairFixture f(CriticalSet);
	f.published.clear();
	const tase2::ObjectRef other{tase2::Scope::Icc, "DOMAIN_A", "TS_OTHER_001"};
	f.Analog(0, 11);
	f.published.clear();
	f.toServer.clear();

	//inline form
	f.link.b.conn.SendReport(f.Report(other, false));
	f.Settle();
	CHECK(f.published.empty()); //discarded
	REQUIRE(f.Acks().size() == 1);
	CHECK(f.Acks()[0].negative);
	CHECK(f.Acks()[0].transferSet == other); //the transfer set name and domain of the report received
	CHECK_FALSE(f.Acks()[0].timeStamp.has_value());
	const auto& list = std::get<mms::ListOfVariable>(f.toServer.back().specification);
	REQUIRE(list.size() == 1);
	CHECK(list[0].name == mms::ObjectName::Domain("DOMAIN_A", "Transfer_Report_NACK"));
	CHECK(f.client->ReportNacksSent() == 1);
	CHECK(f.client->ReportAcksSent() == 1); //only the one for the report before
	CHECK(f.Count(f.clientLogs, "Transfer_Report_NACK", LogLevel::Warn) == 1);
	//the server side has no such transfer set: consumed, ignored
	CHECK(f.server->ReportCounters().nacksReceived == 0);

	//by data set name, with a transfer set that is not the one started for it
	f.link.b.conn.SendReport(f.Report(other, true));
	f.Settle();
	CHECK(f.published.empty());
	CHECK(f.client->ReportNacksSent() == 2);
	CHECK(f.Count(f.clientLogs, "Transfer_Report_NACK", LogLevel::Warn) == 1); //the warning is for the first report of a transfer set only
	CHECK(f.Count(f.clientLogs, "TS_OTHER_001", LogLevel::Debug) >= 1);

	//the right transfer set name in another domain is another transfer set
	f.link.b.conn.SendReport(f.Report(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_OTHER", "TS_POOL_001"}, false));
	f.Settle();
	CHECK(f.published.empty());
	CHECK(f.client->ReportNacksSent() == 3);

	//a report for a data set it did not define
	auto unknown = f.Report(other, true);
	unknown.specification = tase2::IccName("DOMAIN_A", "DS_UNKNOWN");
	f.link.b.conn.SendReport(unknown);
	f.Settle();
	CHECK(f.published.empty());
	CHECK(f.client->ReportNacksSent() == 4);
	CHECK(f.Acks().size() == 4);

	//the report of the transfer set it started is still accepted, and acknowledged, in both forms
	f.link.b.conn.SendReport(f.Report(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"}, false));
	f.link.b.conn.SendReport(f.Report(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"}, true));
	f.Settle();
	CHECK(f.published.size() == 6);
	CHECK(f.client->ReportAcksSent() == 3);
	CHECK(f.client->ReportNacksSent() == 4);
}

TEST_CASE(SUITE("client: a report it cannot read at all gets no answer"))
{
	PairFixture f(CriticalSet);
	f.toServer.clear();
	mms::InformationReport empty;
	empty.specification = mms::ListOfVariable{};
	f.link.b.conn.SendReport(empty);
	f.Settle();
	CHECK(f.toServer.empty());
	CHECK(f.Count(f.clientLogs, "could not be interpreted", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("client: NackUnknownReports false keeps the previous behaviour: published, never NACKed"))
{
	PairFixture f(CriticalSet, R"(, "NackUnknownReports": false)");
	f.published.clear();
	f.toServer.clear();
	const tase2::ObjectRef other{tase2::Scope::Icc, "DOMAIN_A", "TS_OTHER_001"};
	f.link.b.conn.SendReport(f.Report(other, false));
	f.Settle();
	CHECK(f.published.size() == 3); //taken, as before
	CHECK(f.client->ReportNacksSent() == 0);
	CHECK(f.Acks().empty()); //it is not a report of a transfer set started here: nothing to acknowledge
	//a by name report of the data set it defined, whatever transfer set it names
	f.link.b.conn.SendReport(f.Report(other, true));
	f.Settle();
	CHECK(f.published.size() == 6);
	CHECK(f.client->ReportNacksSent() == 0);
}

TEST_CASE(SUITE("client: Transfer_Report_NACK does not stop the data, and a NACK is no more than a failure on the server side"))
{
	//the NACK reaches a server that has such a transfer set: a Peer's server side
	PairFixture f(CriticalSet);
	f.Analog(0, 11);
	f.toServer.clear();
	//the client's own transfer set name is also a transfer set of the server: reports from it with a bad data set are not ours to judge,
	//so make the client NACK a name the server knows: TS_POOL_002 is a pool name the client never started
	f.link.b.conn.SendReport(f.Report(tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_002"}, false));
	f.Settle();
	REQUIRE(f.Acks().size() == 1);
	CHECK(f.Acks()[0].negative);
	//TS_POOL_002 is not critical (not started): a warning, no failure
	CHECK(f.server->ReportCounters().nacksReceived == 1);
	CHECK(f.server->ReportCounters().dataFailures == 0);
	//and the transfer set it did start goes on
	f.published.clear();
	f.Analog(0, 12);
	CHECK(f.published.size() == 1);
}
