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
 * TransferSetAllChangesTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The server side of a transfer set with AllChangesReported (IEC 60870-6-503 clause 5.3.10.2.2): the ResponderSession against a
 *  scripted client in memory, with a fake clock. Every change of a point within the buffer time is reported, in order, in reports
 *  that each carry a point at most once. Synthetic names only.
 */

#include "StackTestHelpers.h"
#include "Tase2Responder.h"
#include <catch.hpp>
#include <sstream>

#define SUITE(name) "ICCP all changes reported - " name

using namespace iccp;
using iccp::stack::LogLevel;
namespace DC = iccp::tase2::DsCondition;

namespace
{
constexpr odc::msSinceEpoch_t Wall = 1700000000000;

const char* const ServerJson = R"({
	"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Type": "RealQTimeTag"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"TransferSetPool": ["TS_POOL_001", "TS_POOL_002"]
	%EXTRA%
})";

ICCPPortConf LoadConf(const std::string& extra)
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

//what a client sees of one report
struct Change
{
	std::string name;
	double value = 0;
	int64_t seconds = 0;
	bool operator==(const Change&) const = default;
};
struct Seen
{
	uint8_t conditions = 0;
	int64_t stamp = 0;
	bool named = false; //names the data set (a complete report) instead of listing the changed members
	std::vector<Change> changes;
};

struct Params
{
	int64_t bufferTime = 2;
	int64_t interval = 0;
	int64_t integrityCheck = 0;
	uint8_t conditions = DC::ObjectChange;
	bool rbe = true;
	bool allChanges = true;
	bool allChangesMember = true;
};

struct TsFixture
{
	stacktest::Link link;
	ICCPPortConf conf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::vector<std::pair<LogLevel, std::string>> logs;
	std::vector<mms::ObjectName> members; //the data set the client defines: three special objects, then the points

	explicit TsFixture(const std::string& extra = ""):
		conf(LoadConf(extra))
	{
		model = std::make_unique<tase2::ResponderModel>(conf, true);
		MakeServer();
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		members = {tase2::IccName("DOMAIN_A", tase2::Name::TransferSetName), tase2::IccName("DOMAIN_A", tase2::Name::DsConditionsDetected),
			     tase2::IccName("DOMAIN_A", tase2::Name::TransferSetTimeStamp), Point("POINT_0001_ST"), Point("POINT_0001_MW"), Point("POINT_0002_MW")};
	}
	void MakeServer()
	{
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = []() { return Wall; };
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, tb, [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); });
	}
	static mms::ObjectName Point(const std::string& name)
	{
		return tase2::IccName("DOMAIN_A", name);
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
		const auto o = Request(tase2::BuildCreateDataSet(def));
		REQUIRE(o.Get<mms::DefineNamedVariableListResponse>() != nullptr);
	}
	void WriteTransferSet(const Params& p, bool status, const char* name = "TS_POOL_001")
	{
		tase2::DsTransferSet t;
		t.dataSetName = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "DS_TEST_001"};
		t.bufferTime = p.bufferTime;
		t.interval = p.interval;
		t.integrityCheck = p.integrityCheck;
		t.conditionsRequested = tase2::DsConditions{p.conditions, tase2::DsConditionBitCount};
		t.rbe = p.rbe;
		t.allChangesMember = p.allChangesMember;
		t.allChangesReported = p.allChanges;
		t.status = status;
		const auto o = Request(tase2::BuildStartTransfer(tase2::IccName("DOMAIN_A", name), t));
		const auto* wr = o.Get<mms::WriteResponse>();
		REQUIRE(wr != nullptr);
		REQUIRE(wr->results.size() == 1);
		REQUIRE(wr->results[0].success);
	}
	//define the data set and start the transfer set; the reports of the start (an integrity report) are dropped
	void Start(const Params& p = Params())
	{
		DefineDataSet();
		WriteTransferSet(p, true);
		Advance(50);
		link.a.reports.clear();
	}
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		link.Pump();
	}

	//a value change of a point, as the port does it: the model first, then the session
	void Analog(size_t index, double value, odc::msSinceEpoch_t timestamp, odc::QualityFlags quality = odc::QualityFlags::ONLINE)
	{
		if(const auto changed = model->Update(PointKind::Analog, index, false, value, 0, quality, timestamp))
			server->OnPointChanged(*changed);
		link.Pump();
	}
	void State(bool on, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Binary, 0, on, 0, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		link.Pump();
	}
	static odc::msSinceEpoch_t Ts(int seconds)
	{
		return Wall + 1000 * seconds;
	}

	Seen Decode(const mms::InformationReport& ir) const
	{
		const auto tr = tase2::ParseTransferReport(ir, &members);
		Seen s;
		REQUIRE(tr.conditionsDetected.has_value());
		s.conditions = tr.conditionsDetected->mask;
		REQUIRE(tr.timeStamp.has_value());
		s.stamp = *tr.timeStamp;
		REQUIRE(tr.layout.size() == 3);
		REQUIRE(tr.transferSet.name == "TS_POOL_001");
		std::vector<mms::ObjectName> names;
		if(const auto* list = std::get_if<mms::ListOfVariable>(&ir.specification))
			for(size_t i = tr.layout.size(); i < list->size(); ++i)
				names.push_back((*list)[i].name);
		else
		{
			s.named = true;
			names.assign(members.begin() + 3, members.end());
		}
		REQUIRE(names.size() == tr.values.size());
		for(size_t i = 0; i < names.size(); ++i)
		{
			REQUIRE(tr.values[i].success);
			const bool state = names[i].itemId == "POINT_0001_ST";
			const auto v = tase2::ParsePoint(tr.values[i].data, state ? tase2::PointType::StateQTimeTag : tase2::PointType::RealQTimeTag);
			s.changes.push_back(Change{names[i].itemId, state ? (v.state == tase2::StateValue::On ? 1.0 : 0.0) : v.real, v.timeStamp.value_or(0)});
		}
		return s;
	}
	std::vector<Seen> Reports() const
	{
		std::vector<Seen> out;
		for(const auto& r : link.a.reports)
			out.push_back(Decode(r));
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
};

//the changes of a report as "name=value", for short checks
std::string Show(const Seen& s)
{
	std::string out;
	for(const auto& c : s.changes)
		out += (out.empty() ? "" : " ") + c.name.substr(6, 4) + c.name.substr(11, 2) + "=" + std::to_string(static_cast<int>(c.value));
	return out;
}
std::vector<std::string> ShowAll(const std::vector<Seen>& reports)
{
	std::vector<std::string> out;
	for(const auto& r : reports)
		out.push_back(Show(r));
	return out;
}
}

TEST_CASE(SUITE("several changes of one point inside the buffer time are all reported, in order, with their own time stamps"))
{
	TsFixture f;
	f.Start();
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Advance(500);
	f.Analog(0, 2, TsFixture::Ts(2));
	f.Advance(500);
	f.Analog(0, 3, TsFixture::Ts(3));
	f.Advance(900);
	CHECK(f.link.a.reports.empty()); //1900 ms after the first change: the buffer time (2 s) has not passed
	f.Advance(200);
	const auto reports = f.Reports();
	REQUIRE(reports.size() == 3);
	CHECK(ShowAll(reports) == std::vector<std::string>({"0001MW=1", "0001MW=2", "0001MW=3"}));
	for(size_t i = 0; i < reports.size(); ++i)
	{
		CHECK(reports[i].conditions == DC::ObjectChange);
		CHECK_FALSE(reports[i].named); //an ordinary report by exception: the changed members, inline
		CHECK(reports[i].stamp == Wall / 1000);
		CHECK(reports[i].changes[0].seconds == Wall / 1000 + static_cast<int64_t>(i) + 1); //the time stamp of the change, not of the report
	}
	CHECK(f.server->ReportsSent() == 3);
}

TEST_CASE(SUITE("with AllChangesReported false only the last value is reported, as before"))
{
	Params p;
	p.allChanges = false;
	TsFixture f;
	f.Start(p);
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Analog(0, 2, TsFixture::Ts(2));
	f.Analog(0, 3, TsFixture::Ts(3));
	f.Analog(1, 10, TsFixture::Ts(3));
	f.Advance(2100);
	const auto reports = f.Reports();
	REQUIRE(reports.size() == 1);
	CHECK(Show(reports[0]) == "0001MW=3 0002MW=10");
	CHECK(reports[0].changes[0].seconds == Wall / 1000 + 3);
	CHECK(f.Count("AllChangesReported is honoured", LogLevel::Info) == 0);
}

TEST_CASE(SUITE("the 1996-08 form has no AllChangesReported: the changes are consolidated"))
{
	Params p;
	p.allChangesMember = false;
	p.allChanges = false;
	TsFixture f;
	f.Start(p);
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Analog(0, 2, TsFixture::Ts(2));
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=2"}));
}

TEST_CASE(SUITE("two points changing in turn: the reports follow the order of the changes and none repeats a point"))
{
	TsFixture f;
	f.Start();
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Analog(1, 10, TsFixture::Ts(1));
	f.Analog(0, 2, TsFixture::Ts(2));
	f.Analog(1, 20, TsFixture::Ts(2));
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=1 0002MW=10", "0001MW=2 0002MW=20"}));

	//a report ends where a point would repeat in it, so the order of the changes is kept across reports
	f.link.a.reports.clear();
	f.Analog(0, 3, TsFixture::Ts(3));
	f.Analog(0, 4, TsFixture::Ts(4));
	f.Analog(1, 30, TsFixture::Ts(4));
	f.Analog(1, 40, TsFixture::Ts(5));
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=3", "0001MW=4 0002MW=30", "0002MW=40"}));
}

TEST_CASE(SUITE("a state that flips and flips back is seen as two events, and a quality change alone is a change"))
{
	TsFixture f;
	f.Start();
	f.State(true, TsFixture::Ts(1));
	f.State(false, TsFixture::Ts(2));
	f.State(true, TsFixture::Ts(3));
	f.Analog(0, 5, TsFixture::Ts(3));
	f.Analog(0, 5, TsFixture::Ts(4), odc::QualityFlags::ONLINE | odc::QualityFlags::LOCAL_FORCED); //the same value, another quality
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001ST=1", "0001ST=0", "0001ST=1 0001MW=5", "0001MW=5"}));
}

TEST_CASE(SUITE("BufferTime 0, RBE false and no ObjectChange condition: nothing changes"))
{
	{
		//BufferTime 0: a report for each change at once (AllChangesReported has nothing to collect), as it is with false
		Params p;
		p.bufferTime = 0;
		TsFixture f;
		f.Start(p);
		f.Analog(0, 1, TsFixture::Ts(1));
		f.Analog(0, 2, TsFixture::Ts(2));
		CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=1", "0001MW=2"}));
		CHECK(f.Count("AllChangesReported has no effect", LogLevel::Info) == 1);
	}
	{
		//RBE false: one complete report with the current state at the end of the buffer time
		Params p;
		p.rbe = false;
		TsFixture f;
		f.Start(p);
		f.Analog(0, 1, TsFixture::Ts(1));
		f.Analog(0, 2, TsFixture::Ts(2));
		f.Advance(2100);
		const auto reports = f.Reports();
		REQUIRE(reports.size() == 1);
		CHECK(reports[0].named);
		CHECK(reports[0].changes.size() == 3);
		CHECK(reports[0].changes[1].value == 2);
		CHECK(f.Count("AllChangesReported has no effect", LogLevel::Info) == 1);
	}
	{
		//no ObjectChange: the changes are not reported at all (only an interval, with RBE, would send them)
		Params p;
		p.conditions = DC::IntegrityTimeOut;
		p.integrityCheck = 3600;
		TsFixture f;
		f.Start(p);
		f.Analog(0, 1, TsFixture::Ts(1));
		f.Analog(0, 2, TsFixture::Ts(2));
		f.Advance(5000);
		CHECK(f.link.a.reports.empty());
	}
}

TEST_CASE(SUITE("the buffer timer: it starts with the first change, and the next change after a flush starts a new one"))
{
	TsFixture f;
	f.Start();
	f.Advance(3000); //nothing is pending: no report, no timer
	CHECK(f.link.a.reports.empty());
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Advance(1900);
	f.Analog(0, 2, TsFixture::Ts(2)); //does not restart the timer
	CHECK(f.link.a.reports.empty());
	f.Advance(200);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=1", "0001MW=2"}));

	f.link.a.reports.clear();
	f.Analog(0, 3, TsFixture::Ts(3)); //a new buffer period
	f.Advance(1900);
	CHECK(f.link.a.reports.empty());
	f.Advance(200);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=3"}));
	f.link.a.reports.clear();
	f.Advance(5000);
	CHECK(f.link.a.reports.empty());
}

TEST_CASE(SUITE("the cap on the buffered changes: beyond it the last value of a point that has a change in the buffer wins, logged once"))
{
	TsFixture f(R"(, "MaxBufferedChanges": 3)");
	CHECK(f.conf.maxBufferedChanges == 3);
	f.Start();
	for(int v = 1; v <= 5; ++v)
		f.Analog(0, v, TsFixture::Ts(v));
	CHECK(f.Count("MaxBufferedChanges", LogLevel::Warn) == 1);
	CHECK(f.Count("the buffer is full", LogLevel::Trace) == 1); //the 5th change replaced the 4th: only the first overflow is a warning
	f.Analog(1, 10, TsFixture::Ts(6));                          //a point with no change in the buffer is not lost
	CHECK(f.Count("the buffer is full", LogLevel::Trace) == 2);
	CHECK(f.Count("it had none in the buffer", LogLevel::Trace) == 1);
	f.Advance(2100);
	const auto reports = f.Reports();
	CHECK(ShowAll(reports) == std::vector<std::string>({"0001MW=1", "0001MW=2", "0001MW=5 0002MW=10"}));
	CHECK(reports[2].changes[0].seconds == Wall / 1000 + 5);
	//the buffer is empty again, and the overflow is not logged again for this transfer set
	f.link.a.reports.clear();
	for(int v = 6; v <= 10; ++v)
		f.Analog(0, v, TsFixture::Ts(v));
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=6", "0001MW=7", "0001MW=10"}));
	CHECK(f.Count("MaxBufferedChanges", LogLevel::Warn) == 1);
	CHECK(f.Count("the buffer is full", LogLevel::Trace) == 4); //the later overflows are traced, not warned
}

TEST_CASE(SUITE("the association ending, and stopping the transfer set, clear the buffer"))
{
	TsFixture f;
	f.Start();
	f.Analog(0, 1, TsFixture::Ts(1));
	f.Analog(0, 2, TsFixture::Ts(2));
	//a new association has a new session (the model is shared): nothing of the old buffer is left
	f.link.a.reports.clear();
	f.MakeServer();
	f.Start();
	f.Advance(5000);
	CHECK(f.link.a.reports.empty());

	//Status false stops the transfer set and drops what it had buffered; a new start begins empty
	f.Analog(0, 3, TsFixture::Ts(3));
	f.Analog(0, 4, TsFixture::Ts(4));
	f.WriteTransferSet(Params(), false);
	f.Advance(5000);
	CHECK(f.link.a.reports.empty());
	f.WriteTransferSet(Params(), true);
	f.Advance(5000);
	CHECK(f.link.a.reports.empty());
	f.Analog(0, 5, TsFixture::Ts(5));
	f.Advance(2100);
	CHECK(ShowAll(f.Reports()) == std::vector<std::string>({"0001MW=5"}));
}

TEST_CASE(SUITE("an integrity report in the middle: the buffered changes go first, then the complete current state"))
{
	Params p;
	p.conditions = DC::ObjectChange | DC::IntegrityTimeOut;
	p.integrityCheck = 10;
	p.bufferTime = 12;
	TsFixture f;
	f.DefineDataSet();
	f.WriteTransferSet(p, true);
	f.Advance(50);
	REQUIRE(f.link.a.reports.size() == 1); //the first integrity report, at once
	CHECK(f.Reports()[0].conditions == DC::IntegrityTimeOut);
	CHECK(f.Reports()[0].named);
	f.link.a.reports.clear();

	f.Analog(0, 1, TsFixture::Ts(1));
	f.Advance(2000);
	f.Analog(0, 2, TsFixture::Ts(2));
	f.Advance(8000); //the integrity time (10 s from the start) comes before the buffer time (12 s from the first change)
	const auto reports = f.Reports();
	REQUIRE(reports.size() == 3);
	CHECK(reports[0].conditions == DC::ObjectChange);
	CHECK(Show(reports[0]) == "0001MW=1");
	CHECK(reports[1].conditions == DC::ObjectChange);
	CHECK(Show(reports[1]) == "0001MW=2");
	CHECK(reports[2].conditions == DC::IntegrityTimeOut);
	CHECK(reports[2].named);
	REQUIRE(reports[2].changes.size() == 3);
	CHECK(reports[2].changes[1].value == 2); //the integrity report is the current state, all members

	//nothing is left to be reported at the old buffer deadline
	f.link.a.reports.clear();
	f.Advance(3000);
	CHECK(f.link.a.reports.empty());
}

TEST_CASE(SUITE("an interval report with RBE reports every buffered change; an operator request reports them first and then the current state"))
{
	{
		Params p;
		p.conditions = DC::ObjectChange | DC::IntervalTimeOut;
		p.interval = 5;
		p.bufferTime = 8;
		TsFixture f;
		f.Start(p);
		f.Analog(0, 1, TsFixture::Ts(1));
		f.Advance(1000);
		f.Analog(0, 2, TsFixture::Ts(2));
		f.Advance(4000);
		const auto reports = f.Reports();
		REQUIRE(reports.size() == 2);
		CHECK(ShowAll(reports) == std::vector<std::string>({"0001MW=1", "0001MW=2"}));
		CHECK(reports[0].conditions == DC::IntervalTimeOut);
		f.link.a.reports.clear();
		f.Advance(5000); //nothing changed: an interval with RBE sends nothing, and the old buffer deadline sends nothing
		CHECK(f.link.a.reports.empty());
	}
	{
		Params p;
		p.conditions = DC::ObjectChange | DC::OperatorRequest;
		TsFixture f;
		f.Start(p);
		f.Analog(0, 1, TsFixture::Ts(1));
		f.Analog(0, 2, TsFixture::Ts(2));
		CHECK(f.server->RaiseCondition(DC::OperatorRequest, 0) == 1);
		f.link.Pump();
		const auto reports = f.Reports();
		REQUIRE(reports.size() == 3);
		CHECK(reports[0].conditions == DC::ObjectChange);
		CHECK(reports[2].conditions == DC::OperatorRequest);
		CHECK(reports[2].named);
		f.link.a.reports.clear();
		f.Advance(5000);
		CHECK(f.link.a.reports.empty());
	}
}

TEST_CASE(SUITE("every report is an ordinary report by exception: no report lists a point twice, so it never outgrows the data set"))
{
	TsFixture f;
	f.Start();
	for(int i = 1; i <= 200; ++i)
	{
		f.Analog(0, i, TsFixture::Ts(1) + i);
		f.Analog(1, -i, TsFixture::Ts(1) + i);
	}
	f.Advance(2100);
	const auto reports = f.Reports();
	REQUIRE(reports.size() == 200);
	for(size_t i = 0; i < reports.size(); ++i)
	{
		REQUIRE(reports[i].changes.size() == 2);
		CHECK(reports[i].changes[0].value == static_cast<double>(i + 1));
		CHECK(reports[i].changes[1].value == -static_cast<double>(i + 1));
	}
}

TEST_CASE(SUITE("the start of a transfer set is logged: honoured, or without effect"))
{
	{
		TsFixture f;
		f.Start();
		CHECK(f.Count("transfer set TS_POOL_001: AllChangesReported is honoured", LogLevel::Info) == 1);
		CHECK(f.Count("MaxBufferedChanges", LogLevel::Warn) == 0);
	}
	{
		Params p;
		p.allChanges = false;
		TsFixture f;
		f.Start(p);
		CHECK(f.Count("AllChangesReported", LogLevel::Info) == 0);
	}
}
