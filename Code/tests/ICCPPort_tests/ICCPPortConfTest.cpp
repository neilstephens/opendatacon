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
 * ICCPPortConfTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Configuration parsing and validation tests (synthetic configurations only).
 */

#include "ICCPPortConf.h"
#include <catch.hpp>
#include <sstream>

#define SUITE(name) "ICCPPortConf - " name

namespace
{
Json::Value Parse(const std::string& text)
{
	Json::Value root;
	Json::CharReaderBuilder b;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b, in, &root, &errs));
	return root;
}

bool AnyContains(const std::vector<std::string>& problems, const std::string& text)
{
	for(const auto& p : problems)
		if(p.find(text) != std::string::npos)
			return true;
	return false;
}

const char* ServerText = R"({
	"IP": "0.0.0.0", "Port": 10102, "TASE2Edition": "1996-08",
	"LocalTSelector": "0001", "RemoteTSelector": "0002", "LocalSSelector": "0003", "RemoteSSelector": "0004",
	"LocalPSelector": "00000005", "RemotePSelector": "00000006",
	"LocalAPTitle": "1.3.9999.1.1", "RemoteAPTitle": "1.3.9999.1.2", "LocalAEQualifier": 11, "RemoteAEQualifier": 12,
	"TPDUSize": 512, "BilateralTableID": "BT_TEST_A", "RemoteBilateralTableID": "BT_TEST_B",
	"LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}, {"Index": 1, "Name": "POINT_0002_ST", "Type": "State", "Scope": "VCC"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "ScaleFactor": 2.0, "Offset": 1.0}],
	"Counters": [{"Index": 0, "Name": "POINT_0001_CNT", "Domain": "DOMAIN_C", "Type": "Discrete"}],
	"TransferSets": [{"Name": "TS_POOL_001"}, {"Name": "TS_POOL_002"}]
})";

const char* ClientText = R"({
	"IP": "127.0.0.1", "Port": "10102", "RemoteDomain": "DOMAIN_A", "RemoteBilateralTableID": "BT_TEST_A",
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}],
	"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_0001_MW"]},
		{"Name": "DS_TEST_002", "Members": ["POINT_0002_MW"], "PollIntervalms": 1000}],
	"TransferSets": [{"DataSet": "DS_TEST_001", "Interval": 10, "IntegrityCheck": 60, "RBE": true, "BufferTime": 2}]
})";
}

TEST_CASE(SUITE("server configuration"))
{
	ICCPPortConf c;
	const auto problems = c.Load(Parse(ServerText), true);
	REQUIRE(problems.empty());
	CHECK(c.ip == "0.0.0.0");
	CHECK(c.port == "10102");
	CHECK(c.edition == iccp::tase2::Edition1996);
	CHECK(c.local.tsap == iccp::ber::Bytes{0x00, 0x01});
	CHECK(c.remote.tsap == iccp::ber::Bytes{0x00, 0x02});
	CHECK(c.local.sSelector == iccp::ber::Bytes{0x00, 0x03});
	CHECK(c.remote.pSelector == iccp::ber::Bytes{0x00, 0x00, 0x00, 0x06});
	CHECK(c.local.apTitle == iccp::acse::Oid{1, 3, 9999, 1, 1});
	CHECK(c.remote.aeQualifier == 12);
	CHECK(c.tpduSizeCode == iccp::cotp::TpduSize512);
	CHECK(c.bilateralTableId == "BT_TEST_A");
	CHECK(c.remoteBilateralTableId == "BT_TEST_B");
	REQUIRE(c.points.size() == 4);
	//the server's points live in its local domain unless given
	CHECK(c.FindPoint("POINT_0001_ST", PointDirection::Serve)->domain == "DOMAIN_A");
	CHECK(c.FindPoint("POINT_0001_ST", PointDirection::Serve)->type == iccp::tase2::PointType::StateQ);
	CHECK(c.FindPoint("POINT_0002_ST", PointDirection::Serve)->scope == iccp::tase2::Scope::Vcc);
	CHECK(c.FindPoint("POINT_0001_MW", PointDirection::Serve)->type == iccp::tase2::PointType::RealQ);
	CHECK(c.FindPoint("POINT_0001_MW", PointDirection::Serve)->scale == 2.0);
	CHECK(c.FindPoint("POINT_0001_MW", PointDirection::Serve)->offset == 1.0);
	CHECK(c.FindPoint("POINT_0001_CNT", PointDirection::Serve)->domain == "DOMAIN_C");
	CHECK(c.FindPoint("POINT_0001_CNT", PointDirection::Serve)->type == iccp::tase2::PointType::Discrete);
	CHECK(c.FindPoint("POINT_0001_ST", PointDirection::Read) == nullptr);
	//the legacy server form: "TransferSets" with names only is the pool offered to the peer
	CHECK(c.role == ServiceRole::Server);
	CHECK(c.tcpListener);
	CHECK(c.transferSets.empty());
	REQUIRE(c.transferSetPool.size() == 2);
	CHECK(c.transferSetPool[0] == "TS_POOL_001");
	CHECK(c.supportedBlocks == std::set<int>{1, 2});
	CHECK(c.requestTimeoutMs == 5000);
}

TEST_CASE(SUITE("defaults"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse("{}"), true).empty());
	CHECK(c.edition == iccp::tase2::Edition2000);
	CHECK(c.port == "102");
	CHECK(c.tpduSizeCode == iccp::cotp::TpduSize1024);
	CHECK(c.vendorName == "opendatacon");
	CHECK(c.dsTransferSetStatusMember == true); //the 2000-08 default
	CHECK(c.maxClientDataSets == 16);
	CHECK(c.traceHex == false);
	CHECK(c.traceHexMaxBytes == 512);
	REQUIRE(c.Load(Parse(R"({"TraceHex": true, "TraceHexMaxBytes": 64})"), true).empty());
	CHECK(c.traceHex);
	CHECK(c.traceHexMaxBytes == 64);
	REQUIRE(c.Load(Parse(R"({"TASE2Edition":"2000-08"})"), false).empty());
	CHECK(c.edition == iccp::tase2::Edition2000);
}

TEST_CASE(SUITE("DSTransferSet Status member follows the edition unless set"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse(R"({"TASE2Edition":"1996-08"})"), false).empty());
	CHECK(c.dsTransferSetStatusMember == false);
	CHECK_FALSE(c.dsTransferSetStatusExplicit);
	CHECK(c.requireMatchingEdition);
	REQUIRE(c.Load(Parse(R"({"TASE2Edition":"2000-08"})"), false).empty());
	CHECK(c.dsTransferSetStatusMember == true);
	REQUIRE(c.Load(Parse(R"({"TASE2Edition":"2000-08","DSTransferSetStatusMember":false,"RequireMatchingEdition":false})"), false).empty());
	CHECK(c.dsTransferSetStatusMember == false);
	CHECK(c.dsTransferSetStatusExplicit);
	CHECK_FALSE(c.requireMatchingEdition);
	REQUIRE(c.Load(Parse(R"({"TASE2Edition":"1996-08","DSTransferSetStatusMember":true})"), false).empty());
	CHECK(c.dsTransferSetStatusMember == true);
}

TEST_CASE(SUITE("client configuration"))
{
	ICCPPortConf c;
	const auto problems = c.Load(Parse(ClientText), false);
	REQUIRE(problems.empty());
	//the client's points live in the remote domain
	CHECK(c.role == ServiceRole::Client);
	CHECK_FALSE(c.tcpListener);
	CHECK(c.FindPoint("POINT_0002_MW", PointDirection::Read)->domain == "DOMAIN_A");
	CHECK(c.FindPoint("POINT_0002_MW", PointDirection::Read)->type == iccp::tase2::PointType::RealQTimeTag);
	REQUIRE(c.dataSets.size() == 2);
	CHECK(c.dataSets[0].domain == "DOMAIN_A");
	CHECK(c.dataSets[0].direction == PointDirection::Read);
	CHECK(c.dataSets[1].pollIntervalMs == 1000);
	REQUIRE(c.transferSets.size() == 1);
	const auto& t = c.transferSets[0];
	CHECK(t.name.empty());
	CHECK(t.dataSet == "DS_TEST_001");
	CHECK(t.rbe);
	CHECK(t.bufferTime == 2);
	//no "Conditions": ObjectChange plus the periodic conditions that have a period
	CHECK(t.conditions.Has(iccp::tase2::DsCondition::ObjectChange));
	CHECK(t.conditions.Has(iccp::tase2::DsCondition::IntervalTimeOut));
	CHECK(t.conditions.Has(iccp::tase2::DsCondition::IntegrityTimeOut));
	CHECK_FALSE(t.conditions.Has(iccp::tase2::DsCondition::OperatorRequest));
	//Block 2 is needed once there are transfer sets
	CHECK(c.requiredRemoteBlocks.count(1) == 1);
	CHECK(c.requiredRemoteBlocks.count(2) == 1);
	CHECK(c.StackConfig().tpduSizeCode == iccp::cotp::TpduSize1024);
	const auto la = c.LocalAssociation();
	CHECK(la.version == iccp::tase2::Edition2000);
	CHECK(la.features.Get(0));
	CHECK(la.features.Get(1));
	CHECK_FALSE(la.features.Get(2));
}

TEST_CASE(SUITE("explicit conditions and fixed transfer set"))
{
	ICCPPortConf c;
	const auto problems = c.Load(Parse(R"({"RemoteDomain":"DOMAIN_A",
		"Binaries":[{"Index":0,"Name":"POINT_0001_ST"}],
		"DataSets":[{"Name":"DS_TEST_001","Members":["POINT_0001_ST"]}],
		"TransferSets":[{"Name":"TS_FIXED","DataSet":"DS_TEST_001","Conditions":["OperatorRequest","ObjectChange"],
			"SpecialObjects":["Transfer_Set_Name","Event_Code_Detected"],"EventCode":7}]})"), false);
	REQUIRE(problems.empty());
	const auto& t = c.transferSets[0];
	CHECK(t.name == "TS_FIXED");
	CHECK(t.conditions.mask == (iccp::tase2::DsCondition::OperatorRequest | iccp::tase2::DsCondition::ObjectChange));
	REQUIRE(t.specials.size() == 2);
	CHECK(t.specials[1] == iccp::tase2::Special::EventCode);
	CHECK(t.eventCode == 7);
}

TEST_CASE(SUITE("validation failures name the problem"))
{
	struct Case
	{
		const char* json;
		bool server;
		const char* expect;
	};
	const Case cases[] = {
		{R"([])", true, "JSON object"},
		{R"({"Port": 0})", true, "\"Port\""},
		{R"({"Port": "http"})", true, "\"Port\""},
		{R"({"IP": 5})", true, "\"IP\""},
		{R"({"LocalTSelector": "xyz"})", true, "LocalTSelector"},
		{R"({"LocalTSelector": "123"})", true, "LocalTSelector"},
		{R"({"LocalAPTitle": "1"})", true, "LocalAPTitle"},
		{R"({"RemoteAPTitle": "1.x.3"})", true, "RemoteAPTitle"},
		{R"({"LocalAEQualifier": -1})", true, "LocalAEQualifier"},
		{R"({"TPDUSize": 1000})", true, "TPDUSize"},
		{R"({"TASE2Edition": "1999-01"})", true, "TASE2Edition"},
		{R"({"SupportedBlocks": [0]})", true, "SupportedBlocks"},
		{R"({"SupportedBlocks": "1"})", true, "SupportedBlocks"},
		{R"({"RequestTimeoutms": 0})", true, "RequestTimeoutms"},
		{R"({"TraceHex": "yes"})", true, "TraceHex"},
		{R"({"TraceHexMaxBytes": 1})", true, "TraceHexMaxBytes"},
		{R"({"MaxOutstandingCalling": 0})", true, "MaxOutstandingCalling"},
		{R"({"Binaries": {}})", true, "\"Binaries\" must be an array"},
		{R"({"LocalDomain": "D", "Binaries": [{"Name": "P"}]})", true, "\"Index\" is required"},
		{R"({"LocalDomain": "D", "Binaries": [{"Index": 0}]})", true, "\"Name\" is required"},
		{R"({"LocalDomain": "D", "Binaries": [{"Index": 0, "Name": "P"}, {"Index": 0, "Name": "Q"}]})", true, "duplicate Index"},
		{R"({"LocalDomain": "D", "Binaries": [{"Index": 0, "Name": "P"}, {"Index": 1, "Name": "P"}]})", true, "duplicate point Name"},
		{R"({"LocalDomain": "D", "Binaries": [{"Index": 0, "Name": "P", "Type": "RealQ"}]})", true, "does not suit"},
		{R"({"LocalDomain": "D", "Analogs": [{"Index": 0, "Name": "P", "Type": "StateQ"}]})", true, "does not suit"},
		{R"({"LocalDomain": "D", "Analogs": [{"Index": 0, "Name": "P", "Type": "Bogus"}]})", true, "unknown point"},
		{R"({"LocalDomain": "D", "Analogs": [{"Index": 0, "Name": "P", "Scope": "XYZ"}]})", true, "\"Scope\""},
		{R"({"LocalDomain": "D", "Analogs": [{"Index": 0, "Name": "P", "ScaleFactor": 0}]})", true, "ScaleFactor"},
		{R"({"Analogs": [{"Index": 0, "Name": "P"}]})", true, "needs a domain"},
		{R"({"Analogs": [{"Index": 0, "Name": "P"}]})", false, "RemoteDomain"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["Q"]}]})", false, "not a configured point"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": []}]})", false, "\"Members\""},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}, {"Name": "S", "Members": ["P"]}]})", false, "duplicate data set"},
		{R"({"LocalDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"], "PollIntervalms": 5}]})", true, "applies to a data set this port reads"},
		{R"({"TransferSets": [{"Name": "A"}, {"Name": "A"}]})", true, "duplicate transfer set name"},
		{R"({"TransferSets": [{}]})", true, "a transfer set name is required"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "T"}]})", false, "must name a configured data set"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S", "Conditions": ["Nope"]}]})", false, "unknown condition"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S", "Conditions": ["IntervalTimeOut"]}]})", false, "\"Interval\" is 0"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S", "Conditions": ["IntegrityTimeOut"]}]})", false, "\"IntegrityCheck\" is 0"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S", "SpecialObjects": ["DSConditions_Detected"]}]})", false, "must start with Transfer_Set_Name"},
		{R"({"RemoteDomain": "D", "Analogs": [{"Index": 0, "Name": "P"}], "DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S", "RBE": 1}]})", false, "\"RBE\""},
	};
	for(const auto& c : cases)
	{
		INFO(c.json);
		ICCPPortConf conf;
		const auto problems = conf.Load(Parse(c.json), c.server);
		REQUIRE_FALSE(problems.empty());
		CHECK(AnyContains(problems, c.expect));
	}
}

TEST_CASE(SUITE("AllChangesReported: true only with 2000-08, RBE, the ObjectChange condition and a BufferTime above 0"))
{
	struct Result
	{
		std::vector<std::string> problems;
		std::vector<std::string> warnings;
		bool allChangesReported = false;
	};
	auto load = [](const std::string& fields, const std::string& edition = "2000-08")
			{
				ICCPPortConf conf;
				const std::string text = R"({"RemoteDomain": "D", "TASE2Edition": ")" + edition + R"(", "Analogs": [{"Index": 0, "Name": "P"}], )"
				                         R"("DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S")" + fields + "}]}";
				Result r;
				r.problems = conf.Load(Parse(text), false);
				r.warnings = conf.warnings;
				if(!conf.transferSets.empty())
					r.allChangesReported = conf.transferSets[0].allChangesReported;
				return r;
			};

	//the default is false, and always valid
	auto r = load("");
	CHECK(r.problems.empty());
	CHECK_FALSE(r.allChangesReported);
	r = load(R"(, "RBE": true)");
	CHECK(r.problems.empty());

	//the whole rule satisfied: RBE, ObjectChange (the default condition) and a buffer time
	r = load(R"(, "AllChangesReported": true, "RBE": true, "BufferTime": 2)");
	CHECK(r.problems.empty());
	CHECK(r.allChangesReported);

	//each part of the rule on its own
	r = load(R"(, "AllChangesReported": true, "BufferTime": 2)");
	REQUIRE_FALSE(r.problems.empty());
	CHECK(AnyContains(r.problems, "needs \"RBE\" true"));
	CHECK_FALSE(AnyContains(r.problems, "BufferTime"));
	r = load(R"(, "AllChangesReported": true, "RBE": true, "Conditions": ["IntegrityTimeOut"], "IntegrityCheck": 30, "BufferTime": 2)");
	REQUIRE_FALSE(r.problems.empty());
	CHECK(AnyContains(r.problems, "the ObjectChange condition"));
	r = load(R"(, "AllChangesReported": true, "RBE": true)");
	REQUIRE_FALSE(r.problems.empty());
	CHECK(AnyContains(r.problems, "a \"BufferTime\" above 0"));
	r = load(R"(, "AllChangesReported": true, "Conditions": ["OperatorRequest"])");
	REQUIRE_FALSE(r.problems.empty());
	CHECK(AnyContains(r.problems, "\"RBE\" true, the ObjectChange condition, a \"BufferTime\" above 0"));

	//1996-08 has no such member: ignored, with a warning, not an error
	r = load(R"(, "AllChangesReported": true)", "1996-08");
	CHECK(r.problems.empty());
	CHECK_FALSE(r.allChangesReported);
	REQUIRE(r.warnings.size() == 1);
	CHECK(r.warnings[0].find("1996-08") != std::string::npos);
	r = load("", "1996-08");
	CHECK(r.warnings.empty());
}

TEST_CASE(SUITE("load can be repeated"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse(ServerText), true).empty());
	REQUIRE(c.Load(Parse(ServerText), true).empty());
	CHECK(c.points.size() == 4);
	CHECK(c.transferSetPool.size() == 2);
}

namespace
{
//one configuration for every port type and role: the points carry their direction
const char* PeerText = R"({
	"ServiceRole": "Peer", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
	"BilateralTableID": "BT_TEST_A", "RemoteBilateralTableID": "BT_TEST_B",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Direction": "Serve"}, {"Index": 0, "Name": "POINT_0001_ST", "Direction": "Read"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Direction": "Serve"}, {"Index": 0, "Name": "POINT_0002_MW", "Direction": "Read"},
		{"Index": 1, "Name": "POINT_0003_MW", "Direction": "Read", "Domain": "DOMAIN_C"}],
	"DataSets": [{"Name": "DS_LOCAL_001", "Members": ["POINT_0001_MW"]},
		{"Name": "DS_REMOTE_001", "Members": ["POINT_0002_MW", "POINT_0003_MW"], "PollIntervalms": 500},
		{"Name": "DS_REMOTE_002", "Members": ["POINT_0001_ST"], "Direction": "Read"}],
	"TransferSetPool": ["TS_POOL_001", {"Name": "TS_POOL_002"}],
	"TransferSets": [{"DataSet": "DS_REMOTE_001", "Interval": 5}]
})";
}

TEST_CASE(SUITE("one configuration, either port type: the schema does not depend on the TCP direction"))
{
	for(const std::string role : {"Client", "Server", "Peer"})
	{
		INFO(role);
		//a config with an explicit role loads identically as a TCP initiator and as a listener
		std::string text = std::string(R"({"ServiceRole": ")") + role + R"(", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B")";
		if(role == "Client")
			text += R"(, "Analogs": [{"Index": 0, "Name": "P_MW"}], "DataSets": [{"Name": "DS_001", "Members": ["P_MW"]}], "TransferSets": [{"DataSet": "DS_001"}])";
		else if(role == "Server")
			text += R"(, "Analogs": [{"Index": 0, "Name": "P_MW"}], "DataSets": [{"Name": "DS_001", "Members": ["P_MW"]}], "TransferSetPool": ["TS_001"])";
		else
			text += R"(, "Analogs": [{"Index": 0, "Name": "P_MW", "Direction": "Serve"}, {"Index": 0, "Name": "Q_MW", "Direction": "Read"}],
				"DataSets": [{"Name": "DS_001", "Members": ["Q_MW"]}], "TransferSetPool": ["TS_001"], "TransferSets": [{"DataSet": "DS_001"}])";
		text += "}";
		ICCPPortConf a, b;
		REQUIRE(a.Load(Parse(text), false).empty());
		REQUIRE(b.Load(Parse(text), true).empty());
		CHECK(a.role == b.role);
		CHECK_FALSE(a.tcpListener);
		CHECK(b.tcpListener);
		REQUIRE(a.points.size() == b.points.size());
		for(size_t i = 0; i < a.points.size(); ++i)
		{
			CHECK(a.points[i].direction == b.points[i].direction);
			CHECK(a.points[i].domain == b.points[i].domain);
		}
		CHECK(a.transferSets.size() == b.transferSets.size());
		CHECK(a.transferSetPool == b.transferSetPool);
		CHECK(a.HasServerSide() == (role != "Client"));
		CHECK(a.HasClientSide() == (role != "Server"));
	}
}

TEST_CASE(SUITE("the default role follows the port type, an explicit ServiceRole overrides it"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse(R"({})"), false).empty());
	CHECK(c.role == ServiceRole::Client);
	REQUIRE(c.Load(Parse(R"({})"), true).empty());
	CHECK(c.role == ServiceRole::Server);
	REQUIRE(c.Load(Parse(R"({"ServiceRole": "Peer"})"), true).empty());
	CHECK(c.role == ServiceRole::Peer);
	REQUIRE(c.Load(Parse(R"({"ServiceRole": "Server"})"), false).empty());
	CHECK(c.role == ServiceRole::Server);
	CHECK(AnyContains(c.Load(Parse(R"({"ServiceRole": "Boss"})"), false), "ServiceRole"));
	//the old defaults: an initiator's points are read (domain: the remote one), a listener's are served (the local one)
	REQUIRE(c.Load(Parse(R"({"LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P"}]})"), false).empty());
	CHECK(c.points[0].direction == PointDirection::Read);
	CHECK(c.points[0].domain == "B");
	REQUIRE(c.Load(Parse(R"({"LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P"}]})"), true).empty());
	CHECK(c.points[0].direction == PointDirection::Serve);
	CHECK(c.points[0].domain == "A");
}

TEST_CASE(SUITE("a peer serves and reads, with its own indexes per direction"))
{
	ICCPPortConf c;
	const auto problems = c.Load(Parse(PeerText), true);
	REQUIRE(problems.empty());
	REQUIRE(c.points.size() == 5);
	//the same index and kind in both directions is fine (Event() inputs versus PublishEvent outputs)
	CHECK(c.FindPoint("POINT_0001_ST", PointDirection::Serve)->domain == "DOMAIN_A");
	CHECK(c.FindPoint("POINT_0001_ST", PointDirection::Read)->domain == "DOMAIN_B");
	CHECK(c.FindPoint("POINT_0003_MW", PointDirection::Read)->domain == "DOMAIN_C");
	//data sets: the direction follows the members; the served one has the local domain, the others the peer's
	REQUIRE(c.dataSets.size() == 3);
	CHECK(c.dataSets[0].direction == PointDirection::Serve);
	CHECK(c.dataSets[0].domain == "DOMAIN_A");
	CHECK(c.dataSets[1].direction == PointDirection::Read);
	CHECK(c.dataSets[1].domain == "DOMAIN_B");
	CHECK(c.dataSets[1].pollIntervalMs == 500);
	CHECK(c.dataSets[2].direction == PointDirection::Read);
	CHECK(c.transferSetPool == std::vector<std::string>({"TS_POOL_001", "TS_POOL_002"}));
	REQUIRE(c.transferSets.size() == 1);
	CHECK(c.transferSets[0].dataSet == "DS_REMOTE_001");
	CHECK(c.requiredRemoteBlocks.count(2) == 1);
}

TEST_CASE(SUITE("MaxBufferedChanges: the cap on the changes buffered per transfer set"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse("{}"), true).empty());
	CHECK(c.maxBufferedChanges == 1000);
	REQUIRE(c.Load(Parse(R"({"MaxBufferedChanges": 1})"), true).empty());
	CHECK(c.maxBufferedChanges == 1);
	REQUIRE(c.Load(Parse(R"({"MaxBufferedChanges": 100000})"), true).empty());
	CHECK(c.maxBufferedChanges == 100000);
	for(const char* bad : {"0", "-1", "100001", "\"many\"", "1.5"})
	{
		INFO(bad);
		ICCPPortConf conf;
		const auto problems = conf.Load(Parse(std::string(R"({"MaxBufferedChanges": )") + bad + "}"), true);
		REQUIRE_FALSE(problems.empty());
		CHECK((AnyContains(problems, "\"MaxBufferedChanges\" must be between 1 and 100000") || AnyContains(problems, "\"MaxBufferedChanges\" must be an integer")));
	}
}

TEST_CASE(SUITE("critical reports: ReportAckTimeoutms, MaxUnacknowledgedReports, AcknowledgeCriticalReports and NackUnknownReports"))
{
	ICCPPortConf c;
	REQUIRE(c.Load(Parse("{}"), true).empty());
	CHECK(c.reportAckTimeoutMs == 10000);
	CHECK(c.maxUnacknowledgedReports == 100);
	CHECK(c.acknowledgeCriticalReports);
	CHECK(c.nackUnknownReports);
	REQUIRE(c.Load(Parse(R"({"ReportAckTimeoutms": 0, "MaxUnacknowledgedReports": 1, "AcknowledgeCriticalReports": false, "NackUnknownReports": false})"), true).empty());
	CHECK(c.reportAckTimeoutMs == 0); //0 = the timeout check is off
	CHECK(c.maxUnacknowledgedReports == 1);
	CHECK_FALSE(c.acknowledgeCriticalReports);
	CHECK_FALSE(c.nackUnknownReports);
	REQUIRE(c.Load(Parse(R"({"ReportAckTimeoutms": 3600000, "MaxUnacknowledgedReports": 100000})"), true).empty());
	CHECK(c.reportAckTimeoutMs == 3600000);
	CHECK(c.maxUnacknowledgedReports == 100000);
	struct Bad
	{
		const char* json;
		const char* message;
	};
	const Bad bads[] = {
		{R"({"ReportAckTimeoutms": -1})", "\"ReportAckTimeoutms\" must be between 0 and 3600000"},
		{R"({"ReportAckTimeoutms": 3600001})", "\"ReportAckTimeoutms\" must be between 0 and 3600000"},
		{R"({"ReportAckTimeoutms": "soon"})", "\"ReportAckTimeoutms\" must be an integer"},
		{R"({"MaxUnacknowledgedReports": 0})", "\"MaxUnacknowledgedReports\" must be between 1 and 100000"},
		{R"({"MaxUnacknowledgedReports": 100001})", "\"MaxUnacknowledgedReports\" must be between 1 and 100000"},
		{R"({"MaxUnacknowledgedReports": 2.5})", "\"MaxUnacknowledgedReports\" must be an integer"},
		{R"({"AcknowledgeCriticalReports": "yes"})", "\"AcknowledgeCriticalReports\" must be true or false"},
		{R"({"NackUnknownReports": 1})", "\"NackUnknownReports\" must be true or false"},
	};
	for(const auto& b : bads)
	{
		INFO(b.json);
		ICCPPortConf conf;
		const auto problems = conf.Load(Parse(b.json), true);
		REQUIRE_FALSE(problems.empty());
		CHECK(AnyContains(problems, b.message));
	}
}

TEST_CASE(SUITE("a Critical transfer set warns when its reports carry no time stamp to acknowledge, or when acknowledgements are off"))
{
	const std::string common = R"("RemoteDomain": "DOMAIN_A", "Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}],
		"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST"]}])";
	auto load = [&](const std::string& extraTransferSet, const std::string& extraTop, ICCPPortConf& c)
			{
				return c.Load(Parse("{" + common + extraTop + R"(, "TransferSets": [{"DataSet": "DS_TEST_001", "Critical": true)" + extraTransferSet + "}]}"), false);
			};
	{
		ICCPPortConf c;
		REQUIRE(load("", "", c).empty());
		CHECK(c.transferSets[0].critical);
		REQUIRE(c.warnings.size() == 1);
		CHECK(c.warnings[0].find("Transfer_Set_Time_Stamp") != std::string::npos);
	}
	{
		ICCPPortConf c;
		REQUIRE(load(R"(, "SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp"])", "", c).empty());
		CHECK(c.warnings.empty());
	}
	{
		ICCPPortConf c;
		REQUIRE(load(R"(, "SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp"])", R"(, "AcknowledgeCriticalReports": false)", c).empty());
		REQUIRE(c.warnings.size() == 1);
		CHECK(c.warnings[0].find("AcknowledgeCriticalReports") != std::string::npos);
	}
	{
		//a transfer set that is not critical has nothing to warn about
		ICCPPortConf c;
		REQUIRE(c.Load(Parse("{" + common + R"(, "AcknowledgeCriticalReports": false, "TransferSets": [{"DataSet": "DS_TEST_001"}]})"), false).empty());
		CHECK(c.warnings.empty());
	}
}

TEST_CASE(SUITE("Block 3: BlockData is silent at load, SupportedBlocks 3 is advertised, BlockedDataName is validated"))
{
	const std::string common = R"("RemoteDomain": "DOMAIN_A", "Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}],
		"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST"]}])";
	auto load = [&](const std::string& extraTransferSet, const std::string& extraTop, ICCPPortConf& c)
			{
				return c.Load(Parse("{" + common + extraTop + R"(, "TransferSets": [{"DataSet": "DS_TEST_001")" + extraTransferSet + "}]}"), false);
			};
	{
		//BlockData true is a deliberate choice: no warning at load (the association warns if the peer lacks Block 3)
		ICCPPortConf c;
		REQUIRE(load(R"(, "BlockData": true)", "", c).empty());
		CHECK(c.transferSets[0].blockData);
		CHECK(c.warnings.empty());
		CHECK_FALSE(c.requiredRemoteBlocks.count(3)); //the peer is not required to advertise it
	}
	{
		ICCPPortConf c;
		REQUIRE(load(R"(, "BlockData": true)", R"(, "RequiredRemoteBlocks": [1, 2, 3])", c).empty());
		CHECK(c.warnings.empty());
	}
	{
		//false is the default and says nothing
		ICCPPortConf c;
		REQUIRE(load("", "", c).empty());
		CHECK_FALSE(c.transferSets[0].blockData);
		CHECK(c.warnings.empty());
	}
	{
		//what is advertised
		ICCPPortConf c;
		REQUIRE(load("", "", c).empty());
		CHECK_FALSE(iccp::tase2::HasBlock(c.LocalAssociation().features, iccp::tase2::Block::Block3));
		ICCPPortConf d;
		REQUIRE(load("", R"(, "SupportedBlocks": [1, 2, 3])", d).empty());
		CHECK(iccp::tase2::HasBlock(d.LocalAssociation().features, iccp::tase2::Block::Block3));
	}
	{
		//the name of the blocked variable: a guess by default, an option, an MMS identifier, not a special object
		ICCPPortConf c;
		REQUIRE(load("", "", c).empty());
		CHECK(c.blockedDataName == "Block_Data");
		ICCPPortConf d;
		REQUIRE(load("", R"(, "BlockedDataName": "SOME_BLOCK")", d).empty());
		CHECK(d.blockedDataName == "SOME_BLOCK");
		ICCPPortConf e;
		CHECK(AnyContains(load("", R"(, "BlockedDataName": "")", e), "\"BlockedDataName\" must be an MMS identifier"));
		ICCPPortConf f;
		CHECK(AnyContains(load("", ", \"BlockedDataName\": \"" + std::string(256, 'x') + "\"", f), "\"BlockedDataName\" must be an MMS identifier"));
		ICCPPortConf g;
		CHECK(AnyContains(load("", R"(, "BlockedDataName": "Transfer_Set_Name")", g), "must not be the name of a special transfer object"));
		ICCPPortConf h;
		CHECK(AnyContains(load("", R"(, "BlockedDataName": 5)", h), "BlockedDataName"));
	}
}

TEST_CASE(SUITE("Block 3: BlockedRule2 is on by default and can be turned off; Blocks 7 and 8 are advertised like any other and change nothing"))
{
	const std::string common = R"("ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}], "TransferSetPool": ["TS_1"])";
	ICCPPortConf c;
	REQUIRE(c.Load(Parse("{" + common + "}"), true).empty());
	CHECK(c.blockedRule2);
	CHECK(c.warnings.empty());
	ICCPPortConf d;
	REQUIRE(d.Load(Parse("{" + common + R"(, "SupportedBlocks": [1, 2, 3], "BlockedRule2": false})"), true).empty());
	CHECK_FALSE(d.blockedRule2);
	CHECK(d.warnings.empty());
	ICCPPortConf d2;
	REQUIRE(d2.Load(Parse("{" + common + R"(, "SupportedBlocks": [1, 2, 3], "BlockedRule2": true})"), true).empty());
	CHECK(d2.blockedRule2);
	ICCPPortConf e;
	CHECK(AnyContains(e.Load(Parse("{" + common + R"(, "BlockedRule2": "yes"})"), true), "BlockedRule2"));
	//Block 7 (events) and Block 8 (accounts): advertised in Supported_Features, nothing else
	ICCPPortConf f;
	REQUIRE(f.Load(Parse("{" + common + R"(, "SupportedBlocks": [1, 2, 7, 8]})"), true).empty());
	CHECK(f.warnings.empty());
	CHECK(iccp::tase2::HasBlock(f.LocalAssociation().features, iccp::tase2::Block::Block7));
	CHECK(iccp::tase2::HasBlock(f.LocalAssociation().features, iccp::tase2::Block::Block8));
	CHECK_FALSE(iccp::tase2::HasBlock(f.LocalAssociation().features, iccp::tase2::Block::Block3));
}

TEST_CASE(SUITE("BilateralTableID and RemoteBilateralTableID: a visible string of at most 32 characters"))
{
	const std::string id32(32, 'x');
	const std::string id33(33, 'x');
	for(const char* key : {"BilateralTableID", "RemoteBilateralTableID"})
	{
		INFO(key);
		auto load = [&](const std::string& value)
				{
					ICCPPortConf conf;
					return conf.Load(Parse(std::string("{\"") + key + "\": \"" + value + "\"}"), true);
				};
		CHECK(load("").empty());
		CHECK(load("BT_TEST_A").empty());
		CHECK(load(id32).empty());
		auto problems = load(id33);
		REQUIRE(problems.size() == 1);
		CHECK(AnyContains(problems, std::string("\"") + key + "\" is too long (33 characters)"));
		CHECK(AnyContains(problems, "Visible-String-32 (IEC 60870-6-503 clause 8.3.3), at most 32 characters; shorten it"));
		//the whole printable ASCII range is fine, space and tilde included
		CHECK(load(" ~!#$%&'()*+,-./0123456789:;<=").empty());
		CHECK(load(">?@AZaz[]^_`{|}").empty());
		for(const char* badChar : {"\\u0001", "\\u001f", "\\u007f", "\\u00e9", "\\t", "\\n", "\\u20ac"})
		{
			INFO(badChar);
			problems = load(std::string("BT_") + badChar + "_A");
			REQUIRE(problems.size() == 1);
			CHECK(AnyContains(problems, std::string("\"") + key + "\" has a character that is not visible ASCII (0x20 to 0x7E)"));
		}
	}
}

TEST_CASE(SUITE("direction validation names the rule and the fix"))
{
	struct Case
	{
		const char* json;
		bool listener;
		const char* expect;
	};
	const Case cases[] = {
		//a Peer must say which way each point goes
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "Analogs": [{"Index": 0, "Name": "P"}]})", false, "\"Direction\" is required with ServiceRole \"Peer\""},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Up"}]})", false, "\"Direction\" must be"},
		//a Client only reads, a Server only serves
		{R"({"ServiceRole": "Client", "LocalDomain": "A", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}]})", false, "needs ServiceRole \"Server\" or \"Peer\""},
		{R"({"ServiceRole": "Server", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Read"}]})", true, "needs ServiceRole \"Client\" or \"Peer\""},
		//an index once per kind and direction
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}, {"Index": 0, "Name": "Q", "Direction": "Serve"}]})", false, "once per kind and direction"},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}, {"Index": 1, "Name": "P", "Direction": "Serve"}]})", false, "duplicate point Name"},
		//domains per direction
		{R"({"ServiceRole": "Peer", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}]})", false, "LocalDomain"},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Read"}]})", false, "RemoteDomain"},
		//data sets: mixed, ambiguous, wrong role
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}, {"Index": 1, "Name": "Q", "Direction": "Read"}],
			"DataSets": [{"Name": "S", "Members": ["P", "Q"]}]})", false, "mixes \"Serve\" and \"Read\" points"},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}, {"Index": 1, "Name": "P", "Direction": "Read"}],
			"DataSets": [{"Name": "S", "Members": ["P"]}]})", false, "set \"Direction\" on the data set"},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}],
			"DataSets": [{"Name": "S", "Members": ["P"], "Direction": "Read"}]})", false, "not every member is a \"Read\" point"},
		//transfer sets: started (needs a data set that is read) versus offered (pool)
		{R"({"ServiceRole": "Client", "RemoteDomain": "B", "TransferSets": [{"Name": "T"}]})", false, "To offer transfer sets to the peer list their names in \"TransferSetPool\""},
		{R"({"ServiceRole": "Peer", "LocalDomain": "A", "RemoteDomain": "B", "Analogs": [{"Index": 0, "Name": "P", "Direction": "Serve"}],
			"DataSets": [{"Name": "S", "Members": ["P"]}], "TransferSets": [{"DataSet": "S"}]})", false, "a data set this port serves"},
		{R"({"ServiceRole": "Server", "TransferSets": [{"DataSet": "S"}]})", true, "never starts transfer sets"},
		{R"({"ServiceRole": "Client", "TransferSetPool": ["T"]})", false, "needs ServiceRole \"Server\" or \"Peer\""},
		{R"({"ServiceRole": "Server", "TransferSetPool": ["T", "T"]})", true, "duplicate transfer set name"},
	};
	for(const auto& c : cases)
	{
		INFO(c.json);
		ICCPPortConf conf;
		const auto problems = conf.Load(Parse(c.json), c.listener);
		REQUIRE_FALSE(problems.empty());
		CHECK(AnyContains(problems, c.expect));
	}
}
