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
 * TagTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The optional parts of TASE.2 Block 5: device tags (Get Tag, Set Tag, the variable <Device>_TAG, enforcement on select and operate) and the Local Reset action.
 *  The Tag_Value encoding, the server side (a ResponderSession against a scripted peer in memory with a fake clock), the client side against it, the
 *  configuration, and two ICCP ports over real TCP (ports 20430 to 20439) driven through their UI commands. Synthetic names only.
 *
 *  No real peer has been seen using tags: the byte vectors here are built from the standard text (IEC 60870-6-503 / -802) and the bit order is an assumption,
 *  so they check that the implementation does what the README says, not what a peer does.
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include "StackTestHelpers.h"
#include "Tase2Client.h"
#include "Tase2Control.h"
#include <algorithm>
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <opendatacon/spdlog.h>
#include <random>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP tags - " name

using namespace iccp;
using iccp::stack::LogLevel;
using tase2::ControlError;
using tase2::ControlType;
using tase2::DeviceTag;
using tase2::TagValue;
namespace DA = iccp::mms::DataAccessError;

namespace
{
using Bytes = ber::Bytes;

Bytes Hex(std::initializer_list<int> bytes)
{
	Bytes b;
	for(int v : bytes)
		b.push_back(static_cast<uint8_t>(v));
	return b;
}

//A2 (the structure) around the flags member `84 len unused bits...` and the reason `8A len text`
Bytes TagWire(const Bytes& flagsContent, const std::string& reason)
{
	Bytes content;
	content.push_back(0x84);
	content.push_back(static_cast<uint8_t>(flagsContent.size()));
	content.insert(content.end(), flagsContent.begin(), flagsContent.end());
	content.push_back(0x8A);
	if(reason.size() < 128)
		content.push_back(static_cast<uint8_t>(reason.size()));
	else
	{
		content.push_back(0x81);
		content.push_back(static_cast<uint8_t>(reason.size()));
	}
	content.insert(content.end(), reason.begin(), reason.end());
	Bytes out;
	out.push_back(0xA2);
	if(content.size() < 128)
		out.push_back(static_cast<uint8_t>(content.size()));
	else
	{
		out.push_back(0x82);
		out.push_back(static_cast<uint8_t>(content.size() >> 8));
		out.push_back(static_cast<uint8_t>(content.size() & 0xFF));
	}
	out.insert(out.end(), content.begin(), content.end());
	return out;
}

TagValue Parse(const Bytes& wire)
{
	return tase2::ParseTagData(mms::DecodeData(wire));
}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the Tag_Value encoding and the pure functions
// ---------------------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("Tag_Value: the bytes of every tag value, the state bit and the reasons"))
{
	struct Case
	{
		DeviceTag tag;
		bool armed;
		std::string reason;
		int flagsByte;
	};
	//tag_hi is bit 0 (the most significant bit of the first octet), tag_lo bit 1, tag_state bit 2: NO_TAG 00, OPEN_AND_CLOSE_INHIBIT 01, CLOSE_ONLY_INHIBIT 10
	const Case cases[] = {
		{DeviceTag::NoTag, false, "", 0x00},
		{DeviceTag::OpenAndCloseInhibit, false, "", 0x40},
		{DeviceTag::CloseOnlyInhibit, false, "", 0x80},
		{DeviceTag::NoTag, true, "", 0x20},
		{DeviceTag::OpenAndCloseInhibit, true, "AB", 0x60},
		{DeviceTag::CloseOnlyInhibit, true, "AB", 0xA0},
		{DeviceTag::CloseOnlyInhibit, false, "WORK ON FEEDER 12", 0x80},
	};
	for(const auto& c : cases)
	{
		INFO(tase2::ToString(c.tag) << (c.armed ? " armed " : " idle ") << c.reason);
		TagValue v;
		v.tag = c.tag;
		v.armed = c.armed;
		v.reason = c.reason;
		const Bytes expected = TagWire(Hex({5, c.flagsByte}), c.reason); //a bit string of 3 bits: 5 unused bits
		const mms::Data data = tase2::TagToData(v);
		CHECK(mms::EncodeData(data) == expected);
		CHECK(Parse(expected) == v);
		CHECK(Parse(mms::EncodeData(data)) == v);
	}
	//the first of them, spelled out
	CHECK(mms::EncodeData(tase2::TagToData(TagValue{})) == Hex({0xA2, 0x06, 0x84, 0x02, 0x05, 0x00, 0x8A, 0x00}));
	TagValue oac;
	oac.tag = DeviceTag::OpenAndCloseInhibit;
	oac.reason = "AB";
	CHECK(mms::EncodeData(tase2::TagToData(oac)) == Hex({0xA2, 0x08, 0x84, 0x02, 0x05, 0x40, 0x8A, 0x02, 0x41, 0x42}));

	//the longest reason: 255 characters (a length of two octets)
	TagValue longest;
	longest.tag = DeviceTag::OpenAndCloseInhibit;
	longest.reason = std::string(255, 'x');
	const Bytes wire = mms::EncodeData(tase2::TagToData(longest));
	CHECK(wire == TagWire(Hex({5, 0x40}), longest.reason));
	CHECK(wire[0] == 0xA2);
	CHECK(wire[1] == 0x82);
	CHECK(Parse(wire) == longest);
	//and one more is not a Tag_Value
	TagValue tooLong = longest;
	tooLong.reason += 'y';
	CHECK_THROWS_AS(tase2::ParseTagData(tase2::TagToData(tooLong)), mms::DecodeError);
}

TEST_CASE(SUITE("Tag_Value: bit strings shorter or longer than 3 bits are accepted"))
{
	struct Case
	{
		Bytes flags; //the content of the bit string: the number of unused bits, then the octets
		DeviceTag tag;
		bool armed;
	};
	const Case cases[] = {
		{Hex({0}), DeviceTag::NoTag, false},                           //no bits at all (no octets, none unused)
		{Hex({7, 0x80}), DeviceTag::CloseOnlyInhibit, false},          //1 bit: tag_hi
		{Hex({6, 0x40}), DeviceTag::OpenAndCloseInhibit, false},       //2 bits: 01
		{Hex({6, 0x80}), DeviceTag::CloseOnlyInhibit, false},          //2 bits: 10
		{Hex({5, 0x20}), DeviceTag::NoTag, true},                      //3 bits: the state only
		{Hex({0, 0x40}), DeviceTag::OpenAndCloseInhibit, false},       //8 bits
		{Hex({0, 0xA0}), DeviceTag::CloseOnlyInhibit, true},           //8 bits
		{Hex({0, 0x40, 0xFF}), DeviceTag::OpenAndCloseInhibit, false}, //16 bits: the bits after the third are ignored
		{Hex({3, 0x9F, 0xFF}), DeviceTag::CloseOnlyInhibit, false},    //13 bits
		{Hex({0, 0x1F}), DeviceTag::NoTag, false},                     //8 bits, only bits that are not defined
	};
	for(const auto& c : cases)
	{
		INFO("flags " << c.flags.size() << " octets, first " << static_cast<int>(c.flags.empty() ? 0 : c.flags[0]));
		const TagValue v = Parse(TagWire(c.flags, "R"));
		CHECK(v.tag == c.tag);
		CHECK(v.armed == c.armed);
		CHECK(v.reason == "R");
	}
	//the undefined value 3 (both tag bits set) is refused, whatever the length
	for(const Bytes& flags : {Hex({5, 0xC0}), Hex({5, 0xE0}), Hex({0, 0xFF}), Hex({6, 0xC0}), Hex({0, 0xC0, 0x00})})
		CHECK_THROWS_AS(Parse(TagWire(flags, "R")), mms::DecodeError);
	//an MMS string for the reason is read like a visible string
	const mms::Data ms = mms::Data::Structure({mms::Data::Bits(mms::BitString(3)), mms::Data::MmsString("TEXT")});
	CHECK(tase2::ParseTagData(ms).reason == "TEXT");
}

TEST_CASE(SUITE("Tag_Value: what is not a Tag_Value is refused"))
{
	const mms::Data flags = mms::Data::Bits(mms::BitString(3));
	const mms::Data reason = mms::Data::VisibleString("R");
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Integer(1)), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({flags})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({flags, reason, reason})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({reason, flags})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({mms::Data::Integer(1), reason})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({flags, mms::Data::Integer(0)})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Array({flags, reason})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({mms::Data::BitsArray(mms::BitString(3)), reason})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseTagData(mms::Data::Structure({flags, mms::Data::VisibleString(std::string(256, 'x'))})), mms::DecodeError);
	CHECK_NOTHROW(tase2::ParseTagData(mms::Data::Structure({flags, mms::Data::VisibleString(std::string(255, 'x'))})));
}

TEST_CASE(SUITE("Tag_Value: truncated and mutated bytes only ever give a DecodeError"))
{
	TagValue v;
	v.tag = DeviceTag::CloseOnlyInhibit;
	v.armed = true;
	v.reason = "WORK ON FEEDER 12";
	const Bytes good = mms::EncodeData(tase2::TagToData(v));
	for(size_t n = 0; n < good.size(); ++n)
	{
		const Bytes cut(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(n));
		try
		{
			Parse(cut);
		}
		catch(const mms::DecodeError&)
		{}
	}
	std::mt19937 rng(12345);
	size_t decoded = 0;
	for(int i = 0; i < 4000; ++i)
	{
		Bytes bad = good;
		const int changes = 1 + static_cast<int>(rng() % 3);
		for(int k = 0; k < changes; ++k)
			bad[rng() % bad.size()] = static_cast<uint8_t>(rng());
		try
		{
			const TagValue t = Parse(bad);
			++decoded;
			CHECK(t.reason.size() <= tase2::MaxTagReasonLength);
			CHECK(static_cast<int>(t.tag) <= 2);
		}
		catch(const mms::DecodeError&)
		{}
	}
	CHECK(decoded > 0); //some mutations (of the reason) are still a Tag_Value
}

TEST_CASE(SUITE("the tag names, the reason rule, the objects, the types and the requests"))
{
	CHECK(std::string(tase2::ToString(DeviceTag::NoTag)) == "NO_TAG");
	CHECK(std::string(tase2::ToString(DeviceTag::OpenAndCloseInhibit)) == "OPEN_AND_CLOSE_INHIBIT");
	CHECK(std::string(tase2::ToString(DeviceTag::CloseOnlyInhibit)) == "CLOSE_ONLY_INHIBIT");
	for(const char* text : {"NO_TAG", "no_tag", "No-Tag", "NO-TAG"})
		CHECK(tase2::DeviceTagFromString(text) == DeviceTag::NoTag);
	for(const char* text : {"OPEN_AND_CLOSE_INHIBIT", "open-and-close-inhibit"})
		CHECK(tase2::DeviceTagFromString(text) == DeviceTag::OpenAndCloseInhibit);
	CHECK(tase2::DeviceTagFromString("Close_Only_Inhibit") == DeviceTag::CloseOnlyInhibit);
	for(const char* text : {"", "NONE", "OPEN", "NOTAG", "CLOSE_ONLY", "0", "1"})
		CHECK_FALSE(tase2::DeviceTagFromString(text).has_value());

	CHECK(tase2::ValidTagReason(""));
	CHECK(tase2::ValidTagReason(std::string(255, ' ')));
	CHECK_FALSE(tase2::ValidTagReason(std::string(256, 'x')));
	CHECK_FALSE(tase2::ValidTagReason("tab\there"));
	CHECK_FALSE(tase2::ValidTagReason("new\nline"));
	CHECK_FALSE(tase2::ValidTagReason("caf\xC3\xA9"));

	CHECK(tase2::TagName("DEV_CMD_01") == "DEV_CMD_01_TAG");
	CHECK(tase2::TagObject(mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01")) == mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01_TAG"));
	CHECK(tase2::TagObject(mms::ObjectName::Vmd("DEV_CMD_01")) == mms::ObjectName::Vmd("DEV_CMD_01_TAG"));

	//the type: a structure of the flags (a bit string of 3 bits) and the reason (a visible string of at most 255 characters)
	using Kind = mms::TypeDescription::Kind;
	const auto t = tase2::TagTypeDescription();
	REQUIRE(t.kind == Kind::Structure);
	REQUIRE(t.subtypes.size() == 2);
	CHECK(t.subtypes[0].kind == Kind::BitString);
	CHECK(t.subtypes[0].size == 3);
	CHECK(t.subtypes[1].kind == Kind::VisibleString);
	CHECK(t.subtypes[1].size == -255);
	REQUIRE(t.componentNames.size() == 2);
	CHECK(t.componentNames[0] == std::optional<std::string>("Flags"));
	CHECK(t.componentNames[1] == std::optional<std::string>("Reason"));

	//the requests: Get Tag is a Read of the tag object, Set Tag a Write of it with the Tag_Value (the state bit written as IDLE)
	const auto icc = mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01");
	const auto get = tase2::BuildGetTag(icc);
	REQUIRE(std::holds_alternative<mms::ListOfVariable>(get.specification));
	REQUIRE(std::get<mms::ListOfVariable>(get.specification).size() == 1);
	CHECK(std::get<mms::ListOfVariable>(get.specification)[0].name == mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01_TAG"));
	const auto set = tase2::BuildSetTag(mms::ObjectName::Vmd("DEV_CMD_01"), DeviceTag::CloseOnlyInhibit, "WHY");
	REQUIRE(std::get<mms::ListOfVariable>(set.specification).size() == 1);
	CHECK(std::get<mms::ListOfVariable>(set.specification)[0].name == mms::ObjectName::Vmd("DEV_CMD_01_TAG"));
	REQUIRE(set.data.size() == 1);
	CHECK(tase2::ParseTagData(set.data[0]) == TagValue{DeviceTag::CloseOnlyInhibit, false, "WHY"});
}

TEST_CASE(SUITE("what a tag prevents: NO_TAG nothing, OPEN_AND_CLOSE_INHIBIT everything, CLOSE_ONLY_INHIBIT a Close or Raise (ON) of a command"))
{
	for(ControlType type : {ControlType::Command, ControlType::DiscreteCommand})
	{
		for(bool on : {false, true})
		{
			CHECK_FALSE(tase2::TagPrevents(DeviceTag::NoTag, type, on));
			CHECK(tase2::TagPrevents(DeviceTag::OpenAndCloseInhibit, type, on));
		}
		CHECK(tase2::TagPrevents(DeviceTag::CloseOnlyInhibit, type, true));
		CHECK_FALSE(tase2::TagPrevents(DeviceTag::CloseOnlyInhibit, type, false));
	}
	for(ControlType type : {ControlType::RealSetpoint, ControlType::DiscreteSetpoint})
		for(bool on : {false, true}) //a setpoint has no direction: `on` is not looked at
		{
			CHECK_FALSE(tase2::TagPrevents(DeviceTag::NoTag, type, on));
			CHECK(tase2::TagPrevents(DeviceTag::OpenAndCloseInhibit, type, on));
			CHECK_FALSE(tase2::TagPrevents(DeviceTag::CloseOnlyInhibit, type, on));
		}
	//the data access error of a refusal by a tag is temporarily-unavailable, like the other refusals of the device state
	CHECK(tase2::DataAccessErrorOf(ControlError::Tagged) == DA::TemporarilyUnavailable);
	CHECK(std::string(tase2::ToString(ControlError::Tagged)) == "tagged");
	CHECK(tase2::CommandStatusOf(DA::TemporarilyUnavailable) == odc::CommandStatus::ALREADY_ACTIVE);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the server side and the client side in memory
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
//the devices: DEV_CMD_01 (a command, SBO, taggable), DEV_CMD_02 (direct, ON 2 / OFF 3, taggable), DEV_CMD_03 (not taggable), DEV_CMD_04 (taggable, starts with a tag),
//VCC_CMD_01 (a VCC device, direct, taggable), DEV_SP_01 (a real setpoint, direct, taggable), DEV_SP_02 (a discrete setpoint, SBO, taggable)
const char* ServerJson = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST"}],
	"BinaryControls": [
		{"Index": 0, "Name": "DEV_CMD_01", "SelectTimeoutms": 2000, "Timeoutms": 3000, "CheckBackID": 7, "Taggable": true},
		{"Index": 1, "Name": "DEV_CMD_02", "SelectRequired": false, "CommandValueOn": 2, "CommandValueOff": 3, "Taggable": true},
		{"Index": 2, "Name": "DEV_CMD_03"},
		{"Index": 3, "Name": "DEV_CMD_04", "Taggable": true, "InitialTag": "CLOSE_ONLY_INHIBIT", "InitialTagReason": "SAFETY HOLD"},
		{"Index": 4, "Name": "VCC_CMD_01", "Scope": "VCC", "SelectRequired": false, "Taggable": true}],
	"AnalogControls": [
		{"Index": 0, "Name": "DEV_SP_01", "SelectRequired": false, "Taggable": true},
		{"Index": 1, "Name": "DEV_SP_02", "Type": "DiscreteSetpoint", "Taggable": true}]})";

//the same devices seen from the operating end
const char* ClientJson = R"({"ServiceRole": "Client", "RemoteDomain": "DOMAIN_A",
	"BinaryControls": [
		{"Index": 0, "Name": "DEV_CMD_01", "CheckBackID": 7},
		{"Index": 1, "Name": "DEV_CMD_02", "SelectRequired": false, "CommandValueOn": 2, "CommandValueOff": 3},
		{"Index": 2, "Name": "DEV_CMD_03"},
		{"Index": 3, "Name": "DEV_CMD_04"},
		{"Index": 4, "Name": "VCC_CMD_01", "Scope": "VCC", "SelectRequired": false}],
	"AnalogControls": [
		{"Index": 0, "Name": "DEV_SP_01", "SelectRequired": false},
		{"Index": 1, "Name": "DEV_SP_02", "Type": "DiscreteSetpoint"}]})";

ICCPPortConf LoadConf(const std::string& json, bool listener)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream in(json);
	REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	ICCPPortConf c;
	const auto problems = c.Load(root, listener);
	for(const auto& p : problems)
		UNSCOPED_INFO(p);
	REQUIRE(problems.empty());
	return c;
}

std::vector<std::string> LoadProblems(const std::string& json, bool listener, ICCPPortConf* out = nullptr)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream in(json);
	REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	ICCPPortConf c;
	auto problems = c.Load(root, listener);
	if(out)
		*out = c;
	return problems;
}

bool Mentions(const std::vector<std::string>& problems, const std::string& part)
{
	return std::any_of(problems.begin(), problems.end(), [&](const std::string& p) { return p.find(part) != std::string::npos; });
}

using CS = odc::CommandStatus;

struct TagFixture
{
	stacktest::Link link;
	ICCPPortConf serverConf;
	ICCPPortConf clientConf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::unique_ptr<tase2::ClientSession> client;
	std::vector<std::pair<LogLevel, std::string>> logs;

	std::vector<tase2::ControlOperation> ops;                 //what reached downstream
	std::deque<std::function<void(odc::CommandStatus)>> held; //operates downstream has not answered
	bool hold = false;
	bool silent = false;                                                          //the server swallows requests
	std::function<bool(uint32_t, const mms::ConfirmedServiceRequest&)> intercept; //answers a request instead of the server

	explicit TagFixture(const std::string& serverJson = ServerJson):
		serverConf(LoadConf(serverJson, true)),
		clientConf(LoadConf(ClientJson, false))
	{
		model = std::make_unique<tase2::ResponderModel>(serverConf, true);
		StartServer();
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   if(intercept && intercept(id, req))
							   return true;
						   if(silent)
							   return true;
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
	}

	tase2::Timebase Time()
	{
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = []() { return odc::msSinceEpoch_t(1700000000000); };
		return tb;
	}
	void StartServer()
	{
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, Time(), [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); });
		server->SetClientIdentity("CLIENT_ONE");
		server->SetControlHandler([this](const tase2::ControlOperation& op, std::function<void(odc::CommandStatus)> done)
			{
				ops.push_back(op);
				if(hold)
					held.push_back(std::move(done));
				else
					done(CS::SUCCESS);
			});
	}
	void StartClient()
	{
		tase2::ClientSession::Callbacks cb;
		cb.onFatal = [](const std::string&) {};
		client = std::make_unique<tase2::ClientSession>(clientConf, link.a.conn, Time(), [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); }, std::move(cb));
		client->OnAssociated();
		link.Pump();
		REQUIRE(client->Ready());
	}
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		if(client)
			client->Tick();
		link.Pump();
	}
	size_t Count(const std::string& part, std::optional<LogLevel> level = std::nullopt) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if((!level || *level == l) && t.find(part) != std::string::npos)
				++n;
		return n;
	}
	size_t Index(const std::string& name) const
	{
		const IccpControlConf* c = serverConf.FindControl(name, ControlDirection::Serve);
		REQUIRE(c != nullptr);
		return static_cast<size_t>(c - serverConf.controls.data());
	}

	// ---- raw requests of a peer
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
	static mms::ObjectName Dev(const std::string& name)
	{
		return mms::ObjectName::Domain("DOMAIN_A", name);
	}
	static mms::ObjectName Vcc(const std::string& name)
	{
		return mms::ObjectName::Vmd(name);
	}
	//Get Tag: the value, or the data access error
	std::pair<std::optional<TagValue>, std::optional<int64_t>> GetTag(const mms::ObjectName& device)
	{
		const auto o = Request(tase2::BuildGetTag(device));
		const auto* rr = o.Get<mms::ReadResponse>();
		REQUIRE(rr != nullptr);
		REQUIRE(rr->results.size() == 1);
		if(!rr->results[0].success)
			return {std::nullopt, rr->results[0].error}
		;
		return {tase2::ParseTagData(rr->results[0].data), std::nullopt};
	}
	std::optional<int64_t> WriteResult(const stack::Outcome& o)
	{
		const auto* wr = o.Get<mms::WriteResponse>();
		REQUIRE(wr != nullptr);
		REQUIRE(wr->results.size() == 1);
		if(wr->results[0].success)
			return std::nullopt;
		return wr->results[0].error;
	}
	//Set Tag: the data access error, nullopt for success
	std::optional<int64_t> SetTag(const mms::ObjectName& device, DeviceTag tag, const std::string& reason = "")
	{
		return WriteResult(Request(tase2::BuildSetTag(device, tag, reason)));
	}
	//a Write of the tag object with any data
	std::optional<int64_t> WriteTagData(const mms::ObjectName& device, const mms::Data& data)
	{
		mms::WriteRequest w;
		w.specification = mms::ListOfVariable{mms::VariableSpec{tase2::TagObject(device), std::nullopt}
		};
		w.data.push_back(data);
		return WriteResult(Request(w));
	}
	//the select (a Read of the select object): the data access error, nullopt for success
	std::optional<int64_t> Select(const mms::ObjectName& device)
	{
		const auto o = Request(tase2::BuildReadCheckBack(device));
		const auto* rr = o.Get<mms::ReadResponse>();
		REQUIRE(rr != nullptr);
		REQUIRE(rr->results.size() == 1);
		if(!rr->results[0].success)
			return rr->results[0].error;
		return std::nullopt;
	}
	//an operate with the bare value
	std::optional<int64_t> Operate(const mms::ObjectName& device, ControlType type, int64_t integer, double real = 0)
	{
		tase2::ControlValue v;
		v.integer = integer;
		v.real = real;
		return WriteResult(Request(tase2::BuildOperate(device, type, v)));
	}
};
}

TEST_CASE(SUITE("Get Tag: a taggable device starts with NO_TAG (or its initial tag), IDLE, and the reason; the tag object is discoverable"))
{
	TagFixture f;
	for(const char* name : {"DEV_CMD_01", "DEV_CMD_02", "DEV_SP_01", "DEV_SP_02"})
	{
		const auto r = f.GetTag(TagFixture::Dev(name));
		REQUIRE(r.first.has_value());
		CHECK(*r.first == TagValue{});
	}
	const auto vcc = f.GetTag(TagFixture::Vcc("VCC_CMD_01"));
	REQUIRE(vcc.first.has_value());
	CHECK(vcc.first->tag == DeviceTag::NoTag);
	//the initial tag and reason of the configuration
	const auto initial = f.GetTag(TagFixture::Dev("DEV_CMD_04"));
	REQUIRE(initial.first.has_value());
	CHECK(*initial.first == TagValue{DeviceTag::CloseOnlyInhibit, false, "SAFETY HOLD"});

	//GetNameList of the domain lists the tag objects of the taggable devices, and only of those; the VCC list the VCC device's
	mms::GetNameListRequest nl;
	nl.objectClass = mms::ObjectClass::NamedVariable;
	nl.scope = mms::ObjectScope::DomainSpecific;
	nl.domainId = "DOMAIN_A";
	const auto names = f.Request(nl).Get<mms::GetNameListResponse>()->identifiers;
	auto has = [](const std::vector<std::string>& v, const char* n) { return std::find(v.begin(), v.end(), n) != v.end(); };
	for(const char* n : {"DEV_CMD_01_TAG", "DEV_CMD_02_TAG", "DEV_CMD_04_TAG", "DEV_SP_01_TAG", "DEV_SP_02_TAG", "DEV_CMD_01", "DEV_CMD_01_SBO", "DEV_CMD_03", "DEV_CMD_03_SBO"})
		CHECK(has(names, n));
	CHECK_FALSE(has(names, "DEV_CMD_03_TAG"));
	CHECK_FALSE(has(names, "VCC_CMD_01_TAG")); //VCC scope
	nl.scope = mms::ObjectScope::VmdSpecific;
	nl.domainId.clear();
	const auto vnames = f.Request(nl).Get<mms::GetNameListResponse>()->identifiers;
	CHECK(has(vnames, "VCC_CMD_01"));
	CHECK(has(vnames, "VCC_CMD_01_SBO"));
	CHECK(has(vnames, "VCC_CMD_01_TAG"));
	CHECK_FALSE(has(vnames, "DEV_CMD_01_TAG"));

	//the type of the tag object
	mms::GetVariableAccessAttributesRequest ga;
	ga.name = TagFixture::Dev("DEV_CMD_01_TAG");
	const auto go = f.Request(ga);
	REQUIRE(go.Get<mms::GetVariableAccessAttributesResponse>() != nullptr);
	CHECK(go.Get<mms::GetVariableAccessAttributesResponse>()->typeDescription == tase2::TagTypeDescription());
	ga.name = TagFixture::Vcc("VCC_CMD_01_TAG");
	CHECK(f.Request(ga).Get<mms::GetVariableAccessAttributesResponse>()->typeDescription == tase2::TagTypeDescription());
	//a device that is not taggable has no tag object: unknown, as any unknown object
	ga.name = TagFixture::Dev("DEV_CMD_03_TAG");
	CHECK(f.Request(ga).kind == stack::Outcome::Kind::Error);
}

TEST_CASE(SUITE("a device that is not taggable has no tag: Get Tag and Set Tag answer object-non-existent, and it is otherwise unchanged"))
{
	TagFixture f;
	CHECK(f.GetTag(TagFixture::Dev("DEV_CMD_03")).second == std::optional<int64_t>(DA::ObjectNonExistent));
	CHECK(f.SetTag(TagFixture::Dev("DEV_CMD_03"), DeviceTag::OpenAndCloseInhibit, "X") == std::optional<int64_t>(DA::ObjectNonExistent));
	CHECK(f.Count("control DEV_CMD_03: Get Tag refused: no such device: the device has no Tag attribute", LogLevel::Warn) == 1);
	CHECK(f.Count("control DEV_CMD_03: Set Tag refused: no such device: the device has no Tag attribute", LogLevel::Warn) == 1);
	//still selectable and operable, whatever was tried
	CHECK_FALSE(f.Select(TagFixture::Dev("DEV_CMD_03")).has_value());
	CHECK_FALSE(f.Operate(TagFixture::Dev("DEV_CMD_03"), ControlType::Command, 1).has_value());
	//an object that is not a device at all: a Read is object-non-existent as before, a Write object-access-denied as before
	CHECK(f.GetTag(TagFixture::Dev("NO_SUCH_DEV")).second == std::optional<int64_t>(DA::ObjectNonExistent));
	CHECK(f.SetTag(TagFixture::Dev("NO_SUCH_DEV"), DeviceTag::NoTag) == std::optional<int64_t>(DA::ObjectAccessDenied));
	//the counters: the two refusals
	CHECK(f.model->TagCounts().refused == 2);
	CHECK(f.model->TagCounts().set == 0);
	//and a device whose name looks like the tag object of another keeps its own: "DEV_X_TAG" is a device here, so what it is as a variable is what it always was (the device
	//itself is write only: a Read of it is refused as access denied, not answered as a tag)
	const std::string json = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A",
		"BinaryControls": [{"Index": 0, "Name": "DEV_X"}, {"Index": 1, "Name": "DEV_X_TAG"}]})";
	TagFixture g(json);
	CHECK_FALSE(g.Select(TagFixture::Dev("DEV_X_TAG")).has_value());
	CHECK(g.GetTag(TagFixture::Dev("DEV_X")).second == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(g.model->TagCounts().refused == 0);
}

TEST_CASE(SUITE("Set Tag: every tag value and reason is stored, read back, kept by the model and counted"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_CMD_01");
	CHECK_FALSE(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, "WORK ON FEEDER 12").has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "WORK ON FEEDER 12"});
	CHECK_FALSE(f.SetTag(dev, DeviceTag::CloseOnlyInhibit, "").has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::CloseOnlyInhibit, false, ""});
	const std::string longest(255, 'z');
	CHECK_FALSE(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, longest).has_value());
	CHECK(f.GetTag(dev).first->reason == longest);
	CHECK_FALSE(f.SetTag(dev, DeviceTag::NoTag, "back to normal").has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::NoTag, false, "back to normal"}); //the reason is kept with NO_TAG too
	//the model has it, owned by the client (nobody owns NO_TAG)
	CHECK(f.model->TagOf(f.Index("DEV_CMD_01")).tag == DeviceTag::NoTag);
	CHECK(f.model->TagCounts().set == 4);
	CHECK(f.model->TagCounts().refused == 0);
	CHECK(f.Count("control DEV_CMD_01: tag set by the peer: OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12)", LogLevel::Info) == 1);
	CHECK(f.Count("control DEV_CMD_01: tag set by the peer: CLOSE_ONLY_INHIBIT", LogLevel::Info) == 1);
	//the state bit written by a client is ignored: the device is IDLE
	TagValue armedWrite;
	armedWrite.tag = DeviceTag::CloseOnlyInhibit;
	armedWrite.armed = true;
	CHECK_FALSE(f.WriteTagData(dev, tase2::TagToData(armedWrite)).has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::CloseOnlyInhibit, false, ""});
	//a VCC device and a setpoint
	CHECK_FALSE(f.SetTag(TagFixture::Vcc("VCC_CMD_01"), DeviceTag::CloseOnlyInhibit, "V").has_value());
	CHECK(f.GetTag(TagFixture::Vcc("VCC_CMD_01")).first->tag == DeviceTag::CloseOnlyInhibit);
	CHECK_FALSE(f.SetTag(TagFixture::Dev("DEV_SP_01"), DeviceTag::OpenAndCloseInhibit, "S").has_value());
	CHECK(f.GetTag(TagFixture::Dev("DEV_SP_01")).first->tag == DeviceTag::OpenAndCloseInhibit);
	//the other devices are untouched
	CHECK(f.GetTag(TagFixture::Dev("DEV_CMD_02")).first->tag == DeviceTag::NoTag);
}

TEST_CASE(SUITE("Set Tag: data that is not a Tag_Value is type-inconsistent and changes nothing"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_CMD_01");
	REQUIRE_FALSE(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, "KEEP").has_value());
	const mms::Data flags = mms::Data::Bits(mms::BitString(3));
	const std::vector<mms::Data> bad = {
		mms::Data::Integer(1), mms::Data::VisibleString("OPEN_AND_CLOSE_INHIBIT"), mms::Data::Structure({}), mms::Data::Structure({flags}),
		mms::Data::Structure({flags, mms::Data::VisibleString("R"), mms::Data::VisibleString("R")}), mms::Data::Structure({mms::Data::Integer(1), mms::Data::VisibleString("R")}),
		mms::Data::Structure({flags, mms::Data::Integer(0)}), mms::Data::Float32(1.0f)
	};
	for(const auto& d : bad)
		CHECK(f.WriteTagData(dev, d) == std::optional<int64_t>(DA::TypeInconsistent));
	//the tag value 3, and a reason of 256 characters
	mms::BitString both(3);
	both.Set(0, true);
	both.Set(1, true);
	CHECK(f.WriteTagData(dev, mms::Data::Structure({mms::Data::Bits(both), mms::Data::VisibleString("R")})) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteTagData(dev, mms::Data::Structure({flags, mms::Data::VisibleString(std::string(256, 'x'))})) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "KEEP"});
	CHECK(f.Count("control DEV_CMD_01: Set Tag refused: wrong type of value: got an integer, expected a Tag_Value", LogLevel::Warn) == 1);
	CHECK(f.model->TagCounts().refused == bad.size() + 2);
	//lenient on receipt: a 1 bit and a 16 bit string are still Tag_Values
	mms::BitString oneBit(1);
	oneBit.Set(0, true);
	CHECK_FALSE(f.WriteTagData(dev, mms::Data::Structure({mms::Data::Bits(oneBit), mms::Data::VisibleString("ONE")})).has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::CloseOnlyInhibit, false, "ONE"});
	mms::BitString wide(16);
	wide.Set(1, true);
	wide.Set(9, true);
	CHECK_FALSE(f.WriteTagData(dev, mms::Data::Structure({mms::Data::Bits(wide), mms::Data::VisibleString("WIDE")})).has_value());
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "WIDE"});
}

TEST_CASE(SUITE("Set Tag is temporarily unavailable while the device is ARMED or busy, and works again when it is not"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_CMD_01");
	//ARMED: the select
	REQUIRE_FALSE(f.Select(dev).has_value());
	CHECK(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, "X") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("control DEV_CMD_01: Set Tag refused: busy: the device is ARMED (selected) (data access error temporarily-unavailable [2])", LogLevel::Warn) == 1);
	CHECK(f.GetTag(dev).first->tag == DeviceTag::NoTag); //nothing was changed
	//Get Tag is allowed whatever the state, and shows the state bit
	CHECK(f.GetTag(dev).first->armed);
	//the select lapses (2 s): the device is IDLE and the tag can be set, the state bit is back to 0
	f.Advance(2100);
	CHECK_FALSE(f.GetTag(dev).first->armed);
	CHECK_FALSE(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, "X").has_value());
	//the operate uses the select up, and the device is IDLE again
	CHECK_FALSE(f.SetTag(dev, DeviceTag::NoTag).has_value());
	REQUIRE_FALSE(f.Select(dev).has_value());
	CHECK(f.GetTag(dev).first->armed);
	CHECK_FALSE(f.Operate(dev, ControlType::Command, 1).has_value());
	CHECK_FALSE(f.GetTag(dev).first->armed);
	CHECK_FALSE(f.SetTag(dev, DeviceTag::CloseOnlyInhibit, "AFTER").has_value());

	//busy: an operate in flight downstream
	f.hold = true;
	f.StartClient();
	const auto op = std::make_shared<std::optional<CS>>();
	const auto* c = f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate);
	REQUIRE(c != nullptr);
	f.client->Operate(*c, {true, 0}, [op](CS s) { *op = s; });
	f.link.Pump();
	REQUIRE(f.held.size() == 1);
	CHECK_FALSE(op->has_value());
	CHECK(f.SetTag(TagFixture::Dev("DEV_CMD_02"), DeviceTag::OpenAndCloseInhibit, "X") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("control DEV_CMD_02: Set Tag refused: busy: an operation is in flight", LogLevel::Warn) == 1);
	CHECK_FALSE(f.GetTag(TagFixture::Dev("DEV_CMD_02")).first->armed); //busy is not ARMED: the operate has used the select up
	f.held.front()(CS::SUCCESS);
	f.link.Pump();
	REQUIRE(op->has_value());
	CHECK(**op == CS::SUCCESS);
	CHECK_FALSE(f.SetTag(TagFixture::Dev("DEV_CMD_02"), DeviceTag::OpenAndCloseInhibit, "X").has_value());
}

TEST_CASE(SUITE("the tag_state bit follows the device: ARMED from the select to the operate, the timeout or a local reset"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_SP_02");
	CHECK_FALSE(f.GetTag(dev).first->armed);
	REQUIRE_FALSE(f.Select(dev).has_value());
	CHECK(f.GetTag(dev).first->armed);
	CHECK(f.server->DeviceArmed(f.Index("DEV_SP_02")));
	CHECK_FALSE(f.Operate(dev, ControlType::DiscreteSetpoint, 5).has_value());
	CHECK_FALSE(f.GetTag(dev).first->armed);
	//the raw wire: the state is the third bit of the flags
	REQUIRE_FALSE(f.Select(dev).has_value());
	const auto o = f.Request(tase2::BuildGetTag(dev));
	const mms::Data d = o.Get<mms::ReadResponse>()->results[0].data;
	REQUIRE(d.elements.size() == 2);
	const auto bits = d.elements[0].AsBits();
	CHECK(bits.BitCount() == 3);
	CHECK(bits.Get(2));
	CHECK_FALSE(bits.Get(0));
	CHECK_FALSE(bits.Get(1));
	f.Advance(11000); //SelectTimeoutms of DEV_SP_02 is the default 10 s
	CHECK_FALSE(f.GetTag(dev).first->armed);
	//ARMED and tagged at once: OPEN_AND_CLOSE_INHIBIT set locally while armed shows both
	REQUIRE_FALSE(f.Select(dev).has_value());
	f.model->SetTag(f.Index("DEV_SP_02"), DeviceTag::OpenAndCloseInhibit, "LOCAL", std::nullopt);
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::OpenAndCloseInhibit, true, "LOCAL"});
}

namespace
{
//what the matrix expects: the rule of the standard, with ON as Close / Raise and a setpoint never prevented by CLOSE_ONLY_INHIBIT
bool Prevented(DeviceTag tag, bool isCommand, bool on)
{
	if(tag == DeviceTag::OpenAndCloseInhibit)
		return true;
	return tag == DeviceTag::CloseOnlyInhibit && isCommand && on;
}
}

TEST_CASE(SUITE("enforcement on a device that needs a select: a tag at the time of the select, and at the time of the operate"))
{
	for(DeviceTag tag : {DeviceTag::NoTag, DeviceTag::OpenAndCloseInhibit, DeviceTag::CloseOnlyInhibit})
	{
		//DEV_CMD_01 (a command), DEV_SP_02 (a discrete setpoint): ON, OFF and a value
		struct Dev
		{
			const char* name;
			ControlType type;
			std::vector<int64_t> values; //the command values or the setpoint
			bool command;
		};
		const Dev devs[] = {
			{"DEV_CMD_01", ControlType::Command, {1, 0}, true}, {"DEV_SP_02", ControlType::DiscreteSetpoint, {5}, false}
		};
		for(const auto& d : devs)
			for(size_t k = 0; k < d.values.size(); ++k)
			{
				const bool on = k == 0;
				INFO(tase2::ToString(tag) << " " << d.name << (d.command ? (on ? " ON" : " OFF") : " setpoint"));
				const auto dev = TagFixture::Dev(d.name);
				{
					//tagged before the select: only OPEN_AND_CLOSE_INHIBIT refuses it, the device stays IDLE, nothing reaches downstream
					TagFixture f;
					f.model->SetTag(f.Index(d.name), tag, "R", std::nullopt);
					const auto sel = f.Select(dev);
					if(tag == DeviceTag::OpenAndCloseInhibit)
					{
						CHECK(sel == std::optional<int64_t>(DA::TemporarilyUnavailable));
						CHECK_FALSE(f.server->DeviceArmed(f.Index(d.name)));
						CHECK(f.Operate(dev, d.type, d.values[k]) == std::optional<int64_t>(DA::TemporarilyUnavailable)); //not selected either
						CHECK(f.ops.empty());
						CHECK(f.Count(std::string("control ") + d.name + ": select refused: tagged: the device is tagged OPEN_AND_CLOSE_INHIBIT (R) (data access error temporarily-unavailable [2])", LogLevel::Warn) == 1);
						CHECK(f.model->TagCounts().selectsRefused == 1);
						//untagged, it can be selected at once
						f.model->SetTag(f.Index(d.name), DeviceTag::NoTag, "", std::nullopt);
						CHECK_FALSE(f.Select(dev).has_value());
					}
					else
						CHECK_FALSE(sel.has_value());
				}
				{
					//tagged between the select and the operate
					TagFixture f;
					REQUIRE_FALSE(f.Select(dev).has_value());
					f.model->SetTag(f.Index(d.name), tag, "R", std::nullopt);
					const auto result = f.Operate(dev, d.type, d.values[k]);
					if(Prevented(tag, d.command, on))
					{
						CHECK(result == std::optional<int64_t>(DA::TemporarilyUnavailable));
						CHECK(f.ops.empty());
						CHECK(f.server->DeviceArmed(f.Index(d.name))); //a refused operate leaves the device armed until its timeout
						CHECK(f.model->TagCounts().operatesRefused == 1);
						CHECK(f.Count(std::string("control ") + d.name + ": operate refused: tagged: the device is tagged " + tase2::ToString(tag) + " (R)", LogLevel::Warn) == 1);
						//the tag is removed: the same select is still good
						f.model->SetTag(f.Index(d.name), DeviceTag::NoTag, "", std::nullopt);
						CHECK_FALSE(f.Operate(dev, d.type, d.values[k]).has_value());
						CHECK(f.ops.size() == 1);
					}
					else
					{
						CHECK_FALSE(result.has_value());
						REQUIRE(f.ops.size() == 1);
						CHECK(f.model->TagCounts().operatesRefused == 0);
						if(d.command)
							CHECK(f.ops[0].on == on);
					}
				}
			}
	}
}

TEST_CASE(SUITE("enforcement on a device that needs no select: the operate alone, every tag, both command values and setpoints"))
{
	//DEV_CMD_02 (ON 2 / OFF 3), VCC_CMD_01 (ON 1 / OFF 0, VCC scope), DEV_SP_01 (a real setpoint)
	for(DeviceTag tag : {DeviceTag::NoTag, DeviceTag::OpenAndCloseInhibit, DeviceTag::CloseOnlyInhibit})
	{
		TagFixture f;
		struct Dev
		{
			const char* name;
			mms::ObjectName object;
			ControlType type;
			bool command;
			int64_t on, off;
		};
		const Dev devs[] = {
			{"DEV_CMD_02", TagFixture::Dev("DEV_CMD_02"), ControlType::Command, true, 2, 3},
			{"VCC_CMD_01", TagFixture::Vcc("VCC_CMD_01"), ControlType::Command, true, 1, 0},
			{"DEV_SP_01", TagFixture::Dev("DEV_SP_01"), ControlType::RealSetpoint, false, 0, 0}
		};
		for(const auto& d : devs)
		{
			INFO(tase2::ToString(tag) << " " << d.name);
			//the tag is set by the client, over the wire
			REQUIRE_FALSE(f.SetTag(d.object, tag, "WHY").has_value());
			size_t expectedOps = f.ops.size();
			size_t expectedRefused = f.model->TagCounts().operatesRefused;
			for(bool on : {true, false})
			{
				if(!d.command && !on)
					continue; //a setpoint: one value
				const auto result = d.command ? f.Operate(d.object, d.type, on ? d.on : d.off) : f.Operate(d.object, d.type, 0, 12.5);
				if(Prevented(tag, d.command, on))
				{
					CHECK(result == std::optional<int64_t>(DA::TemporarilyUnavailable));
					++expectedRefused;
				}
				else
				{
					CHECK_FALSE(result.has_value());
					++expectedOps;
				}
				CHECK(f.ops.size() == expectedOps);
				CHECK(f.model->TagCounts().operatesRefused == expectedRefused);
			}
		}
	}
}

TEST_CASE(SUITE("a device that is not taggable, and a configuration without tags, is never refused for a tag"))
{
	TagFixture f;
	for(int round = 0; round < 3; ++round)
	{
		CHECK_FALSE(f.Select(TagFixture::Dev("DEV_CMD_03")).has_value());
		CHECK_FALSE(f.Operate(TagFixture::Dev("DEV_CMD_03"), ControlType::Command, round % 2).has_value());
	}
	CHECK(f.ops.size() == 3);
	CHECK(f.model->TagCounts().selectsRefused == 0);
	CHECK(f.model->TagCounts().operatesRefused == 0);
	//a taggable device with the default tag behaves like the others
	CHECK_FALSE(f.Select(TagFixture::Dev("DEV_CMD_01")).has_value());
	CHECK_FALSE(f.Operate(TagFixture::Dev("DEV_CMD_01"), ControlType::Command, 1).has_value());
	CHECK(f.model->TagCounts().operatesRefused == 0);
}

TEST_CASE(SUITE("the VCC owner rule: only the client that set a VCC device's tag can change it; any client can change an ICC device's"))
{
	TagFixture f;
	const auto vcc = TagFixture::Vcc("VCC_CMD_01");
	const auto icc = TagFixture::Dev("DEV_CMD_02");
	//CLIENT_ONE tags both
	REQUIRE_FALSE(f.SetTag(vcc, DeviceTag::OpenAndCloseInhibit, "ONE").has_value());
	REQUIRE_FALSE(f.SetTag(icc, DeviceTag::OpenAndCloseInhibit, "ONE").has_value());
	CHECK(f.model->TagOf(f.Index("VCC_CMD_01")).owner == std::optional<std::string>("CLIENT_ONE"));
	//another client: the ICC device is changed, the VCC device is temporarily unavailable
	f.server->SetClientIdentity("CLIENT_TWO");
	CHECK_FALSE(f.SetTag(icc, DeviceTag::CloseOnlyInhibit, "TWO").has_value());
	CHECK(f.SetTag(vcc, DeviceTag::NoTag, "TWO") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "TWO") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(*f.GetTag(vcc).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "ONE"}); //it can read it
	CHECK(f.Count("control VCC_CMD_01: Set Tag refused: tagged: the device is tagged OPEN_AND_CLOSE_INHIBIT by another client", LogLevel::Warn) == 2);
	//and the tag is enforced against the other client's operate
	CHECK(f.Operate(vcc, ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	//the owner changes it, and clears it
	f.server->SetClientIdentity("CLIENT_ONE");
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "ONE AGAIN").has_value());
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::NoTag, "").has_value());
	CHECK_FALSE(f.model->TagOf(f.Index("VCC_CMD_01")).owner.has_value()); //nobody owns NO_TAG
	//with no tag set, any client can tag, and then owns it
	f.server->SetClientIdentity("CLIENT_TWO");
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "TWO").has_value());
	CHECK(f.model->TagOf(f.Index("VCC_CMD_01")).owner == std::optional<std::string>("CLIENT_TWO"));
	f.server->SetClientIdentity("CLIENT_ONE");
	CHECK(f.SetTag(vcc, DeviceTag::NoTag, "") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	//the empty identity (an association whose request did not say) is one client too
	f.server->SetClientIdentity("");
	CHECK(f.SetTag(vcc, DeviceTag::NoTag, "") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	f.server->SetClientIdentity("CLIENT_TWO");
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::NoTag, "").has_value());
	f.server->SetClientIdentity("");
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "ANON").has_value());
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::NoTag, "").has_value());
	//a tag set locally (the operator, or the configuration) has no client owner: no client can change it on a VCC device, any client can on an ICC device
	f.model->SetTag(f.Index("VCC_CMD_01"), DeviceTag::OpenAndCloseInhibit, "LOCAL", std::nullopt);
	f.model->SetTag(f.Index("DEV_CMD_02"), DeviceTag::OpenAndCloseInhibit, "LOCAL", std::nullopt);
	CHECK(f.SetTag(vcc, DeviceTag::NoTag, "") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("the device is tagged OPEN_AND_CLOSE_INHIBIT locally: only the local operator can change", LogLevel::Warn) == 1);
	CHECK_FALSE(f.SetTag(icc, DeviceTag::NoTag, "").has_value());
	f.model->SetTag(f.Index("VCC_CMD_01"), DeviceTag::NoTag, "", std::nullopt);
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "NOW").has_value());
}

TEST_CASE(SUITE("the tag belongs to the device: it survives the association, with its owner, and starts the next association as it was"))
{
	TagFixture f;
	const auto vcc = TagFixture::Vcc("VCC_CMD_01");
	REQUIRE_FALSE(f.SetTag(vcc, DeviceTag::OpenAndCloseInhibit, "OVER THE LINK").has_value());
	REQUIRE_FALSE(f.SetTag(TagFixture::Dev("DEV_CMD_01"), DeviceTag::CloseOnlyInhibit, "AND ANOTHER").has_value());
	//the association ends: a new session on the same model (what the port does for the next association)
	f.server->AssociationEnded();
	f.StartServer(); //the old session is gone; the stack connection is reused only for the transport
	CHECK(*f.GetTag(vcc).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "OVER THE LINK"});
	CHECK(*f.GetTag(TagFixture::Dev("DEV_CMD_01")).first == TagValue{DeviceTag::CloseOnlyInhibit, false, "AND ANOTHER"});
	//the same client (the same identity) is the owner on the new association; another one is not
	CHECK_FALSE(f.SetTag(vcc, DeviceTag::CloseOnlyInhibit, "SAME CLIENT").has_value());
	f.server->SetClientIdentity("SOMEONE_ELSE");
	CHECK(f.SetTag(vcc, DeviceTag::NoTag, "") == std::optional<int64_t>(DA::TemporarilyUnavailable));
	//a select of the old association is gone with it (the state is the association's), the tag is not: it is still enforced, here against ON (a Close)
	f.StartServer();
	CHECK(f.GetTag(TagFixture::Dev("DEV_CMD_01")).first->tag == DeviceTag::CloseOnlyInhibit);
	CHECK(f.Operate(vcc, ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK_FALSE(f.Operate(vcc, ControlType::Command, 0).has_value());
}

TEST_CASE(SUITE("Local Reset: ARMED goes to IDLE, a later operate is refused as not armed, the tag may be changed, and it is logged and counted"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_CMD_01");
	const size_t idx = f.Index("DEV_CMD_01");
	//not armed: nothing happens
	CHECK_FALSE(f.server->LocalReset(idx));
	CHECK(f.model->TagCounts().localResets == 0);
	//armed: the reset takes it to IDLE
	REQUIRE_FALSE(f.Select(dev).has_value());
	CHECK(f.GetTag(dev).first->armed);
	CHECK(f.server->LocalReset(idx));
	CHECK_FALSE(f.server->DeviceArmed(idx));
	CHECK_FALSE(f.GetTag(dev).first->armed);
	CHECK(f.model->TagCounts().localResets == 1);
	CHECK(f.Count("control DEV_CMD_01: local reset: the device went from ARMED to IDLE (the MMS event notification of the action is not sent)", LogLevel::Info) == 1);
	//the operate of the client that had selected it is refused: the device is not selected
	CHECK(f.Operate(dev, ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("control DEV_CMD_01: operate refused: not selected", LogLevel::Warn) == 1);
	CHECK(f.ops.empty());
	//a second reset finds it IDLE
	CHECK_FALSE(f.server->LocalReset(idx));
	CHECK(f.model->TagCounts().localResets == 1);
	//the device can be selected again (and Set Tag works), and a reset together with a tag change: the reset, then the tag
	REQUIRE_FALSE(f.Select(dev).has_value());
	CHECK(f.server->LocalReset(idx));
	f.model->SetTag(idx, DeviceTag::OpenAndCloseInhibit, "EMERGENCY", std::nullopt);
	CHECK(f.Select(dev) == std::optional<int64_t>(DA::TemporarilyUnavailable)); //tagged: not selectable
	CHECK(*f.GetTag(dev).first == TagValue{DeviceTag::OpenAndCloseInhibit, false, "EMERGENCY"});
	CHECK(f.model->TagCounts().localResets == 2);
	//a lapsed select is not reset: nothing was ARMED any more
	f.model->SetTag(idx, DeviceTag::NoTag, "", std::nullopt);
	REQUIRE_FALSE(f.Select(dev).has_value());
	f.link.clock.now += std::chrono::milliseconds(2100);
	CHECK_FALSE(f.server->LocalReset(idx));
	CHECK(f.model->TagCounts().localResets == 2);
	//and a device that is operating is left alone
	f.hold = true;
	f.StartClient();
	const auto op = std::make_shared<std::optional<CS>>();
	const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate);
	f.client->Operate(*c, {true, 0}, [op](CS s) { *op = s; });
	f.link.Pump();
	REQUIRE(f.held.size() == 1);
	CHECK_FALSE(f.server->LocalReset(f.Index("DEV_CMD_02")));
	f.held.front()(CS::SUCCESS);
	f.link.Pump();
	CHECK(**op == CS::SUCCESS);
}

TEST_CASE(SUITE("a tag object is a member of a data set: defined, read with the points, never reported as changed"))
{
	TagFixture f;
	const tase2::DataSetDefinition def{TagFixture::Dev("DS_TAGS"), {TagFixture::Dev("DEV_CMD_01_TAG"), TagFixture::Dev("POINT_0001_ST"), TagFixture::Dev("DEV_SP_02_TAG")}
	};
	CHECK(f.Request(tase2::BuildCreateDataSet(def)).Get<mms::DefineNamedVariableListResponse>() != nullptr);
	//a device that is not taggable, or an unknown tag object, is not a member
	const tase2::DataSetDefinition bad{TagFixture::Dev("DS_BAD"), {TagFixture::Dev("DEV_CMD_03_TAG")}
	};
	CHECK(f.Request(tase2::BuildCreateDataSet(bad)).kind == stack::Outcome::Kind::Error);
	const tase2::DataSetDefinition unknown{TagFixture::Dev("DS_BAD2"), {TagFixture::Dev("NO_SUCH_DEV_TAG")}
	};
	CHECK(f.Request(tase2::BuildCreateDataSet(unknown)).kind == stack::Outcome::Kind::Error);
	//the element names, and the values: the tags in the structure, the state bit as it is
	const auto names = f.Request(tase2::BuildGetDataSetElementNames(def.name));
	REQUIRE(names.Get<mms::GetNamedVariableListAttributesResponse>() != nullptr);
	CHECK(tase2::ParseDataSetElementNames(def.name, *names.Get<mms::GetNamedVariableListAttributesResponse>()).members == def.members);
	REQUIRE_FALSE(f.SetTag(TagFixture::Dev("DEV_CMD_01"), DeviceTag::CloseOnlyInhibit, "IN A DATA SET").has_value());
	REQUIRE_FALSE(f.Select(TagFixture::Dev("DEV_SP_02")).has_value());
	const auto values = f.Request(tase2::BuildGetDataSetElementValues(def.name));
	const auto* rr = values.Get<mms::ReadResponse>();
	REQUIRE(rr != nullptr);
	REQUIRE(rr->results.size() == 3);
	REQUIRE(rr->results[0].success);
	CHECK(tase2::ParseTagData(rr->results[0].data) == TagValue{DeviceTag::CloseOnlyInhibit, false, "IN A DATA SET"});
	CHECK(rr->results[1].success); //the point
	REQUIRE(rr->results[2].success);
	CHECK(tase2::ParseTagData(rr->results[2].data) == TagValue{DeviceTag::NoTag, true, ""}); //DEV_SP_02 is ARMED
	//the same members read as a list of variables
	mms::ReadRequest list;
	list.specification = mms::ListOfVariable{mms::VariableSpec{TagFixture::Dev("DEV_CMD_01_TAG"), std::nullopt}, mms::VariableSpec{TagFixture::Dev("DEV_CMD_03_TAG"), std::nullopt}
	};
	const auto lo = f.Request(list);
	REQUIRE(lo.Get<mms::ReadResponse>()->results.size() == 2);
	CHECK(lo.Get<mms::ReadResponse>()->results[0].success);
	CHECK_FALSE(lo.Get<mms::ReadResponse>()->results[1].success);
	CHECK(lo.Get<mms::ReadResponse>()->results[1].error == DA::ObjectNonExistent);
}

TEST_CASE(SUITE("a tag object in a transfer set's data set is in the integrity report, and a change of the tag is not reported"))
{
	const std::string json = std::string(ServerJson).replace(std::string(ServerJson).find("\"Binaries\""), 0, R"("TransferSets": [{"Name": "TS_POOL_001"}], )");
	TagFixture f(json);
	const tase2::DataSetDefinition def{TagFixture::Dev("DS_TAGS"), {TagFixture::Dev("Transfer_Set_Name"), TagFixture::Dev("POINT_0001_ST"), TagFixture::Dev("DEV_CMD_01_TAG")}
	};
	REQUIRE(f.Request(tase2::BuildCreateDataSet(def)).Get<mms::DefineNamedVariableListResponse>() != nullptr);
	REQUIRE_FALSE(f.SetTag(TagFixture::Dev("DEV_CMD_01"), DeviceTag::OpenAndCloseInhibit, "IN A REPORT").has_value());
	const auto next = f.Request(tase2::BuildGetNextDsTransferSet("DOMAIN_A"));
	const auto ts = tase2::ParseNextDsTransferSet(*next.Get<mms::ReadResponse>());
	tase2::DsTransferSet params;
	params.dataSetName = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "DS_TAGS"};
	params.conditionsRequested = tase2::DsConditions{static_cast<uint8_t>(tase2::DsCondition::ObjectChange | tase2::DsCondition::IntegrityTimeOut), tase2::DsConditionBitCount};
	params.integrityCheck = 30;
	params.rbe = true;
	const auto start = f.Request(tase2::BuildStartTransfer(mms::ObjectName::Domain("DOMAIN_A", ts.name), params));
	REQUIRE(start.Get<mms::WriteResponse>() != nullptr);
	//the first report is the integrity report, sent at once (on the server's next tick): the tag is its third member
	f.Advance(10);
	REQUIRE(f.link.a.reports.size() == 1);
	const auto report = tase2::ParseTransferReport(f.link.a.reports[0], &def.members);
	REQUIRE(report.values.size() == 2);
	REQUIRE(report.values[1].success);
	CHECK(tase2::ParseTagData(report.values[1].data) == TagValue{DeviceTag::OpenAndCloseInhibit, false, "IN A REPORT"});
	//a change of the tag (by a client, or locally) is not reported: there is no change reporting for tags
	REQUIRE_FALSE(f.SetTag(TagFixture::Dev("DEV_CMD_01"), DeviceTag::CloseOnlyInhibit, "CHANGED").has_value());
	f.model->SetTag(f.Index("DEV_CMD_01"), DeviceTag::NoTag, "", std::nullopt);
	f.Advance(1000);
	CHECK(f.link.a.reports.size() == 1);
	//a change of the point is reported, without the tag
	const auto changed = f.model->Update(PointKind::Binary, 0, true, 0, 0, odc::QualityFlags::ONLINE, 1700000000000);
	REQUIRE(changed.has_value());
	f.server->OnPointChanged(*changed);
	f.link.Pump();
	REQUIRE(f.link.a.reports.size() == 2);
	for(const auto& r : f.link.a.reports[1].results)
		CHECK(r.success);
	const auto* names = std::get_if<mms::ListOfVariable>(&f.link.a.reports[1].specification);
	REQUIRE(names != nullptr);
	for(const auto& n : *names)
		CHECK(n.name.itemId.find("_TAG") == std::string::npos);
	//and the next integrity report has the tag as it is now
	f.Advance(31000);
	REQUIRE(f.link.a.reports.size() == 3);
	const auto again = tase2::ParseTransferReport(f.link.a.reports[2], &def.members);
	CHECK(tase2::ParseTagData(again.values[1].data) == TagValue{DeviceTag::NoTag, false, ""});
}

TEST_CASE(SUITE("the counters of the tags"))
{
	TagFixture f;
	const auto dev = TagFixture::Dev("DEV_CMD_02");
	REQUIRE_FALSE(f.SetTag(dev, DeviceTag::CloseOnlyInhibit, "A").has_value());                           //set
	REQUIRE_FALSE(f.SetTag(dev, DeviceTag::OpenAndCloseInhibit, "B").has_value());                        //set
	CHECK(f.Operate(dev, ControlType::Command, 2) == std::optional<int64_t>(DA::TemporarilyUnavailable)); //operate refused by the tag
	CHECK(f.Operate(dev, ControlType::Command, 3) == std::optional<int64_t>(DA::TemporarilyUnavailable)); //operate refused by the tag
	REQUIRE_FALSE(f.Select(TagFixture::Dev("DEV_CMD_01")).has_value());
	f.model->SetTag(f.Index("DEV_CMD_01"), DeviceTag::OpenAndCloseInhibit, "", std::nullopt);
	CHECK(f.server->LocalReset(f.Index("DEV_CMD_01")));                                                                //local reset
	CHECK(f.Select(TagFixture::Dev("DEV_CMD_01")) == std::optional<int64_t>(DA::TemporarilyUnavailable));              //select refused by the tag
	CHECK(f.SetTag(TagFixture::Dev("DEV_CMD_03"), DeviceTag::NoTag) == std::optional<int64_t>(DA::ObjectNonExistent)); //refused
	f.model->CountTag(tase2::ResponderModel::TagEvent::LocalSet);
	const auto c = f.model->TagCounts();
	CHECK(c.set == 2);
	CHECK(c.operatesRefused == 2);
	CHECK(c.selectsRefused == 1);
	CHECK(c.localResets == 1);
	CHECK(c.refused == 1);
	CHECK(c.localSet == 1);
	//the select and operate refusals are in the control counters too
	CHECK(f.server->ControlCounters().refused == 3);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the client side against the server side in memory
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
using TagResult = tase2::ClientSession::TagResult;

//Get Tag or Set Tag of one of the peer's devices from the client side, and what it answered (done is called once, and the callback is checked for that)
TagResult RunTag(TagFixture& f, bool set, const std::string& name, DeviceTag tag = DeviceTag::NoTag, const std::string& reason = "")
{
	std::vector<TagResult> got;
	const IccpControlConf* c = f.clientConf.FindControl(name, ControlDirection::Operate);
	REQUIRE(c != nullptr);
	auto cb = [&got](const TagResult& r) { got.push_back(r); };
	if(set)
		f.client->SetTag(*c, tag, reason, cb);
	else
		f.client->GetTag(*c, cb);
	f.link.Pump();
	REQUIRE(got.size() == 1);
	return got[0];
}
}

TEST_CASE(SUITE("the client: Set Tag and Get Tag of the peer's devices, for an ICC and a VCC device, and the counters"))
{
	TagFixture f;
	f.StartClient();
	const auto g0 = RunTag(f, false, "DEV_CMD_01");
	REQUIRE(g0.ok);
	CHECK(g0.value == TagValue{});
	CHECK_FALSE(g0.dataAccessError.has_value());
	CHECK(f.Count("control DEV_CMD_01: Get Tag: the peer's tag is NO_TAG, the device is IDLE", LogLevel::Info) == 1);
	const auto s1 = RunTag(f, true, "DEV_CMD_01", DeviceTag::OpenAndCloseInhibit, "WORK ON FEEDER 12");
	REQUIRE(s1.ok);
	CHECK(s1.value == TagValue{DeviceTag::OpenAndCloseInhibit, false, "WORK ON FEEDER 12"});
	CHECK(f.Count("control DEV_CMD_01: Set Tag accepted by the peer: OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12)", LogLevel::Info) == 1);
	CHECK(f.model->TagOf(f.Index("DEV_CMD_01")).tag == DeviceTag::OpenAndCloseInhibit);
	const auto g1 = RunTag(f, false, "DEV_CMD_01");
	REQUIRE(g1.ok);
	CHECK(g1.value == TagValue{DeviceTag::OpenAndCloseInhibit, false, "WORK ON FEEDER 12"});
	CHECK(f.Count("control DEV_CMD_01: Get Tag: the peer's tag is OPEN_AND_CLOSE_INHIBIT (WORK ON FEEDER 12), the device is IDLE", LogLevel::Info) == 1);
	//the state of the device is reported: ARMED
	REQUIRE_FALSE(f.Select(TagFixture::Dev("DEV_SP_02")).has_value());
	const auto armed = RunTag(f, false, "DEV_SP_02");
	REQUIRE(armed.ok);
	CHECK(armed.value.armed);
	CHECK(f.Count("control DEV_SP_02: Get Tag: the peer's tag is NO_TAG, the device is ARMED", LogLevel::Info) == 1);
	//a VCC device is addressed by its VMD-specific tag object
	const auto v = RunTag(f, true, "VCC_CMD_01", DeviceTag::CloseOnlyInhibit, "VCC");
	REQUIRE(v.ok);
	CHECK(f.model->TagOf(f.Index("VCC_CMD_01")).tag == DeviceTag::CloseOnlyInhibit);
	CHECK(RunTag(f, false, "VCC_CMD_01").value == TagValue{DeviceTag::CloseOnlyInhibit, false, "VCC"});
	//a device of ours that is operated afterwards is refused for the tag, the client's operate says so (temporarily-unavailable)
	std::optional<CS> result;
	f.client->Operate(*f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate), {true, 0}, [&](CS s) { result = s; });
	f.link.Pump();
	REQUIRE(result.has_value());
	CHECK(*result == CS::SUCCESS); //DEV_CMD_02 has no tag
	RunTag(f, true, "DEV_CMD_02", DeviceTag::OpenAndCloseInhibit, "NO");
	result.reset();
	f.client->Operate(*f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate), {true, 0}, [&](CS s) { result = s; });
	f.link.Pump();
	REQUIRE(result.has_value());
	CHECK(*result == CS::ALREADY_ACTIVE);
	CHECK(f.Count("failed: operate: data access error temporarily-unavailable [2]", LogLevel::Error) == 1);
	const auto& cc = f.client->ControlCounters();
	CHECK(cc.tagReads == 4);
	CHECK(cc.tagWrites == 3);
	CHECK(cc.tagFailures == 0);
}

TEST_CASE(SUITE("the client: every refusal of the peer is a readable line, and counted"))
{
	TagFixture f;
	f.StartClient();
	//no Tag attribute
	const auto a = RunTag(f, false, "DEV_CMD_03");
	CHECK_FALSE(a.ok);
	CHECK(a.dataAccessError == std::optional<int64_t>(DA::ObjectNonExistent));
	CHECK(a.error == "data access error object-non-existent [10]: the peer has no such device, or the device has no Tag attribute");
	CHECK(f.Count("control DEV_CMD_03: Get Tag failed: data access error object-non-existent [10]: the peer has no such device, or the device has no Tag attribute", LogLevel::Warn) == 1);
	const auto b = RunTag(f, true, "DEV_CMD_03", DeviceTag::NoTag);
	CHECK(b.dataAccessError == std::optional<int64_t>(DA::ObjectNonExistent));
	CHECK(f.Count("control DEV_CMD_03: Set Tag failed: data access error object-non-existent [10]", LogLevel::Warn) == 1);
	//ARMED
	REQUIRE_FALSE(f.Select(TagFixture::Dev("DEV_CMD_01")).has_value());
	const auto c = RunTag(f, true, "DEV_CMD_01", DeviceTag::OpenAndCloseInhibit, "X");
	CHECK_FALSE(c.ok);
	CHECK(c.dataAccessError == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(c.error == "data access error temporarily-unavailable [2]: the device is ARMED or busy, or its tag was set by another client");
	//the owner of a VCC device's tag
	REQUIRE(RunTag(f, true, "VCC_CMD_01", DeviceTag::OpenAndCloseInhibit, "MINE").ok);
	f.server->SetClientIdentity("CLIENT_TWO");
	const auto d = RunTag(f, true, "VCC_CMD_01", DeviceTag::NoTag);
	CHECK(d.dataAccessError == std::optional<int64_t>(DA::TemporarilyUnavailable));
	//a reason that is not a TASE.2 VisibleString of at most 255 characters is not sent
	const size_t before = f.model->TagCounts().set;
	const auto e = RunTag(f, true, "DEV_CMD_02", DeviceTag::NoTag, std::string(256, 'x'));
	CHECK_FALSE(e.ok);
	CHECK_FALSE(e.dataAccessError.has_value());
	CHECK(e.error == "the reason must be at most 255 characters of visible ASCII");
	CHECK(RunTag(f, true, "DEV_CMD_02", DeviceTag::NoTag, "caf\xC3\xA9").error == "the reason must be at most 255 characters of visible ASCII");
	CHECK(f.model->TagCounts().set == before);
	CHECK(f.Count("control DEV_CMD_02: Set Tag failed: the reason must be at most 255", LogLevel::Error) == 2);
	CHECK(f.client->ControlCounters().tagFailures == 6);
	CHECK(f.client->ControlCounters().tagReads == 0);
	CHECK(f.client->ControlCounters().tagWrites == 1);
}

TEST_CASE(SUITE("the client: a peer that does not answer, or answers wrongly, or not at all"))
{
	{
		TagFixture f;
		f.StartClient();
		f.silent = true;
		std::vector<TagResult> got;
		const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate);
		f.client->GetTag(*c, [&](const TagResult& r) { got.push_back(r); });
		f.link.Pump();
		CHECK(got.empty());
		f.Advance(5100); //the request timeout
		REQUIRE(got.size() == 1);
		CHECK_FALSE(got[0].ok);
		CHECK_FALSE(got[0].dataAccessError.has_value());
		CHECK(got[0].error.find("timed out") != std::string::npos);
		CHECK(f.Count("control DEV_CMD_01: Get Tag failed: timed out", LogLevel::Error) == 1);
		CHECK(f.client->ControlCounters().tagFailures == 1);
	}
	{
		//the client session goes away with a request in flight: done is called, once
		TagFixture f;
		f.StartClient();
		f.silent = true;
		std::vector<TagResult> got;
		const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate);
		f.client->SetTag(*c, DeviceTag::CloseOnlyInhibit, "R", [&](const TagResult& r) { got.push_back(r); });
		f.link.Pump();
		CHECK(got.empty());
		f.client.reset();
		REQUIRE(got.size() == 1);
		CHECK_FALSE(got[0].ok);
		CHECK(got[0].error == "the association ended");
	}
	{
		//not started: the association is not up. done is called at once, and there is no callback to lose when none is given
		TagFixture f;
		tase2::ClientSession::Callbacks cb;
		f.client = std::make_unique<tase2::ClientSession>(f.clientConf, f.link.a.conn, f.Time(), [&f](LogLevel l, const std::string& t) { f.logs.emplace_back(l, t); }, std::move(cb));
		const auto r = RunTag(f, false, "DEV_CMD_01");
		CHECK_FALSE(r.ok);
		CHECK(r.error == "the association is not ready");
		f.client->GetTag(*f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate), nullptr);
		f.client->SetTag(*f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate), DeviceTag::NoTag, "", nullptr);
		CHECK(f.client->ControlCounters().tagFailures == 3);
	}
	auto answers = [](TagFixture& f, mms::ConfirmedServiceResponse response)
			   {
				   f.intercept = [&f, response](uint32_t id, const mms::ConfirmedServiceRequest&)
						     {
							     f.link.b.conn.Respond(id, response);
							     return true;
						     };
			   };
	{
		//answers that are not right
		TagFixture f;
		f.StartClient();
		mms::ReadResponse notTag;
		notTag.results.push_back(mms::AccessResult::Success(mms::Data::Integer(5)));
		answers(f, notTag);
		auto r = RunTag(f, false, "DEV_CMD_01");
		CHECK_FALSE(r.ok);
		CHECK(r.error.find("the peer's answer is not a Tag_Value") == 0);
		mms::ReadResponse two;
		two.results.push_back(mms::AccessResult::Success(tase2::TagToData(TagValue{})));
		two.results.push_back(mms::AccessResult::Success(tase2::TagToData(TagValue{})));
		answers(f, two);
		CHECK(RunTag(f, false, "DEV_CMD_01").error == "the answer has 2 results for 1 object");
		mms::WriteResponse none;
		answers(f, none);
		CHECK(RunTag(f, true, "DEV_CMD_01", DeviceTag::NoTag).error == "the answer has 0 results for 1 object");
		mms::IdentifyResponse wrongKind;
		answers(f, wrongKind);
		CHECK(RunTag(f, false, "DEV_CMD_01").error == "the peer answered with an unexpected kind of response");
		//a data access error that is not one the standard names for a tag
		mms::ReadResponse odd;
		odd.results.push_back(mms::AccessResult::Failure(DA::HardwareFault));
		answers(f, odd);
		const auto h = RunTag(f, false, "DEV_CMD_01");
		CHECK(h.dataAccessError == std::optional<int64_t>(DA::HardwareFault));
		CHECK(h.error == "data access error hardware-fault [1]");
		//an access denied of the peer
		mms::WriteResponse denied;
		denied.results.push_back(mms::WriteResult{false, DA::ObjectAccessDenied});
		answers(f, denied);
		CHECK(RunTag(f, true, "DEV_CMD_01", DeviceTag::NoTag).error == "data access error object-access-denied [3]: the peer does not permit Set Tag to this client");
		CHECK(f.client->ControlCounters().tagFailures == 6);
	}
	{
		//a service error and a reject
		TagFixture f;
		f.StartClient();
		f.intercept = [&f](uint32_t id, const mms::ConfirmedServiceRequest&)
				  {
					  f.link.b.conn.RejectRequest(id);
					  return true;
				  };
		const auto r = RunTag(f, false, "DEV_CMD_01");
		CHECK_FALSE(r.ok);
		CHECK(r.error.find("rejected") != std::string::npos);
		f.intercept = [&f](uint32_t id, const mms::ConfirmedServiceRequest&)
				  {
					  mms::ServiceError e;
					  e.errorClass = mms::ErrorClass::Access;
					  e.errorCode = 3;
					  f.link.b.conn.RespondError(id, e);
					  return true;
				  };
		const auto s = RunTag(f, true, "DEV_CMD_01", DeviceTag::NoTag);
		CHECK_FALSE(s.ok);
		CHECK(s.error.find("service error") != std::string::npos);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("configuration: Taggable, InitialTag and InitialTagReason, and their defaults"))
{
	ICCPPortConf c;
	REQUIRE(LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A",
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"},
			{"Index": 1, "Name": "DEV_CMD_02", "Taggable": true},
			{"Index": 2, "Name": "DEV_CMD_03", "Taggable": true, "InitialTag": "open-and-close-inhibit", "InitialTagReason": "SOME REASON"},
			{"Index": 3, "Name": "DEV_CMD_04", "Taggable": false}],
		"AnalogControls": [{"Index": 0, "Name": "DEV_SP_01", "Taggable": true, "InitialTag": "CLOSE_ONLY_INHIBIT"}]})", true, &c).empty());
	REQUIRE(c.controls.size() == 5);
	CHECK_FALSE(c.controls[0].taggable); //the default: no Tag attribute
	CHECK(c.controls[0].initialTag == DeviceTag::NoTag);
	CHECK(c.controls[0].initialTagReason.empty());
	CHECK(c.controls[1].taggable);
	CHECK(c.controls[1].initialTag == DeviceTag::NoTag);
	CHECK(c.controls[2].taggable);
	CHECK(c.controls[2].initialTag == DeviceTag::OpenAndCloseInhibit);
	CHECK(c.controls[2].initialTagReason == "SOME REASON");
	CHECK_FALSE(c.controls[3].taggable);
	CHECK(c.controls[4].initialTag == DeviceTag::CloseOnlyInhibit);
	CHECK(c.HasTaggableControls());
	ICCPPortConf plain;
	REQUIRE(LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}]})", true, &plain).empty());
	CHECK_FALSE(plain.HasTaggableControls());
	//one file under either port type
	CHECK(LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}]})", false).empty());
	//a Peer: a taggable device is one of its Serve devices
	ICCPPortConf peer;
	REQUIRE(LoadProblems(R"({"ServiceRole": "Peer", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
		"BinaryControls": [{"Index": 0, "Name": "DEV_MINE", "Direction": "Serve", "Taggable": true}, {"Index": 0, "Name": "DEV_YOURS", "Direction": "Operate"}]})", true, &peer).empty());
	CHECK(peer.HasTaggableControls());
	CHECK(peer.controls[0].taggable);
	CHECK_FALSE(peer.controls[1].taggable);
}

TEST_CASE(SUITE("configuration: validation names the rule and the fix"))
{
	const std::string head = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B", )";
	//Taggable is for the devices this port serves
	auto p = LoadProblems(R"({"ServiceRole": "Client", "RemoteDomain": "DOMAIN_B", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}]})", false);
	CHECK(Mentions(p, "\"Taggable\" is for a device this port serves (Direction \"Serve\")"));
	p = LoadProblems(R"({"ServiceRole": "Peer", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate", "Taggable": true}]})", true);
	CHECK(Mentions(p, "\"Taggable\" is for a device this port serves"));
	//wrong types and values
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": "yes"}]})", true);
	CHECK(Mentions(p, "BinaryControls[0]: \"Taggable\" must be true or false"));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTag": "INHIBIT"}]})", true);
	CHECK(Mentions(p, "\"InitialTag\" must be \"NO_TAG\", \"OPEN_AND_CLOSE_INHIBIT\" or \"CLOSE_ONLY_INHIBIT\""));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTag": 1}]})", true);
	CHECK(Mentions(p, "\"InitialTag\" must be a string"));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTagReason": 5}]})", true);
	CHECK(Mentions(p, "\"InitialTagReason\" must be a string"));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTagReason": ")" + std::string(256, 'x') + R"("}]})", true);
	CHECK(Mentions(p, "\"InitialTagReason\" must be at most 255 characters of visible ASCII"));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTagReason": "caf\u00e9"}]})", true);
	CHECK(Mentions(p, "\"InitialTagReason\" must be at most 255 characters of visible ASCII"));
	CHECK(LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true, "InitialTagReason": ")" + std::string(255, 'x') + R"("}]})", true).empty());
	//a tag or a reason needs the attribute
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "InitialTag": "NO_TAG"}]})", true);
	CHECK(Mentions(p, "\"InitialTag\" needs \"Taggable\" true"));
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": false, "InitialTagReason": "R"}]})", true);
	CHECK(Mentions(p, "\"InitialTagReason\" needs \"Taggable\" true"));
	//the tag object D_TAG is a name of the domain: a taggable device must not clash with another device or a point; a device that is not taggable has no tag object, so it can
	p = LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}, {"Index": 1, "Name": "DEV_CMD_01_TAG"}]})", true);
	CHECK(Mentions(p, "control \"DEV_CMD_01_TAG\" clashes with the tag object of the taggable control \"DEV_CMD_01\" (\"DEV_CMD_01_TAG\"): rename one"));
	p = LoadProblems(head + R"("Binaries": [{"Index": 0, "Name": "DEV_CMD_01_TAG"}], "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}]})", true);
	CHECK(Mentions(p, "the taggable control \"DEV_CMD_01\" has the tag object \"DEV_CMD_01_TAG\", which is also the name of a point"));
	CHECK(LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}, {"Index": 1, "Name": "DEV_CMD_01_TAG"}]})", true).empty());
	CHECK(LoadProblems(head + R"("Binaries": [{"Index": 0, "Name": "DEV_CMD_01_TAG"}], "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}]})", true).empty());
	//the same name in another domain, or scope, is another object
	CHECK(LoadProblems(head + R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}, {"Index": 1, "Name": "DEV_CMD_01_TAG", "Scope": "VCC"}]})", true).empty());
}

TEST_CASE(SUITE("a point that has the name of a device's tag object keeps it when the device is not taggable"))
{
	//the Read of the object is the point's, as before tags existed
	const std::string json = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A",
		"Binaries": [{"Index": 0, "Name": "DEV_CMD_01_TAG"}],
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}]})";
	TagFixture f(json);
	const auto r = f.Request(tase2::BuildGetDataValue({TagFixture::Dev("DEV_CMD_01_TAG")}));
	REQUIRE(r.Get<mms::ReadResponse>() != nullptr);
	REQUIRE(r.Get<mms::ReadResponse>()->results.size() == 1);
	CHECK(r.Get<mms::ReadResponse>()->results[0].success); //the point's value, as it always was: not a tag answer and not a refusal
	CHECK(r.Get<mms::ReadResponse>()->results[0].data.type != mms::DataType::Structure);
	CHECK(f.model->TagCounts().refused == 0);
	CHECK(f.Count("Get Tag") == 0);
	CHECK(f.Count("has no Tag attribute") == 0);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// two ports over TCP, driven through the UI commands
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
void SetupLoggers()
{
	static bool done = false;
	if(done)
		return;
	done = true;
	auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	sink->set_level(spdlog::level::warn);
	for(const char* name : {"ICCPPort", "opendatacon"})
	{
		if(odc::spdlog_get(name)) //another test file made it first
			continue;
		auto logger = std::make_shared<spdlog::logger>(name, sink);
		logger->set_level(spdlog::level::warn);
		odc::spdlog_register_logger(logger);
	}
}

template<typename Pred>
bool WaitFor(Pred&& pred, int timeoutMs = 10000)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while(!pred())
	{
		if(std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return true;
}

//the downstream of a node: records the control events the node publishes (the peer operated one of its devices) and answers them
class TagSink: public odc::IOHandler
{
public:
	TagSink():
		odc::IOHandler("TagSink")
	{}
	void Event(odc::ConnectState state, const std::string&) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		states.push_back(state);
	}
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string&, odc::SharedStatusCallback_t cb) override
	{
		if(event->GetEventType() != odc::EventType::ConnectState)
		{
			std::lock_guard<std::mutex> lock(mtx);
			recs.emplace_back(event->GetEventType(), event->GetIndex());
		}
		(*cb)(odc::CommandStatus::SUCCESS);
	}
	void Enable() override
	{}
	void Disable() override
	{}
	size_t Count(odc::ConnectState state)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return static_cast<size_t>(std::count(states.begin(), states.end(), state));
	}
	size_t Count()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return recs.size();
	}

private:
	std::mutex mtx;
	std::vector<std::pair<odc::EventType, size_t>> recs;
	std::vector<odc::ConnectState> states;
};

//The configuration of one node. "A" serves (Server role, the TCP listener), "B" operates (Client role, the TCP initiator). The devices: DEV_DIR (direct, ON 2 / OFF 3),
//DEV_SBO (select before operate, a long select, CheckBackID 7: B expects 8, so that its select arms the device and its operate is not sent), DEV_PLAIN (not taggable),
//VCC_DIR (a VCC device) and the setpoint DEV_SP.
std::string TagNodeConf(const std::string& me, int port, bool taggable)
{
	const bool a = me == "A";
	const std::string peer = a ? "B" : "A";
	const std::string tag = taggable ? ", \"Taggable\": true" : "";
	const std::string dir = a ? ", \"Direction\": \"Serve\"" : ", \"Direction\": \"Operate\"";
	const std::string sbo = a ? ", \"SelectTimeoutms\": 30000, \"CheckBackID\": 7" : ", \"CheckBackID\": 8";
	const std::string serveTag = a ? tag : "";
	return "{\"ServiceRole\": \"" + std::string(a ? "Server" : "Client") + "\", \"IP\": \"127.0.0.1\", \"Port\": \"" + std::to_string(port) + "\", \"TASE2Edition\": \"1996-08\","
	       " \"TCPConnectRetryPeriodms\": 200,"
	       " \"LocalTSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteTSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalSSelector\": \"000" + std::string(a ? "1" : "2") + "\", \"RemoteSSelector\": \"000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalPSelector\": \"0000000" + std::string(a ? "1" : "2") + "\", \"RemotePSelector\": \"0000000" + std::string(a ? "2" : "1") + "\","
	       " \"LocalAPTitle\": \"1.3.9999.1." + std::string(a ? "1" : "2") + "\", \"RemoteAPTitle\": \"1.3.9999.1." + std::string(a ? "2" : "1") + "\","
	       " \"LocalAEQualifier\": " + std::string(a ? "1" : "2") + ", \"RemoteAEQualifier\": " + std::string(a ? "2" : "1") + ","
	       " \"BilateralTableID\": \"BT_" + me + "\", \"RemoteBilateralTableID\": \"BT_" + peer + "\","
	       " \"LocalDomain\": \"DOMAIN_" + me + "\", \"RemoteDomain\": \"DOMAIN_" + peer + "\","
	       " \"BinaryControls\": ["
	       "{\"Index\": 0, \"Name\": \"DEV_DIR\"" + dir + ", \"SelectRequired\": false, \"CommandValueOn\": 2, \"CommandValueOff\": 3" + serveTag + "},"
	       "{\"Index\": 1, \"Name\": \"DEV_SBO\"" + dir + sbo + serveTag + "},"
	       "{\"Index\": 2, \"Name\": \"DEV_PLAIN\"" + dir + ", \"SelectRequired\": false},"
	       "{\"Index\": 3, \"Name\": \"VCC_DIR\", \"Scope\": \"VCC\"" + dir + ", \"SelectRequired\": false" + serveTag + "}],"
	       " \"AnalogControls\": [{\"Index\": 0, \"Name\": \"DEV_SP\"" + dir + ", \"SelectRequired\": false" + serveTag + "}]}";
}

void WriteFile(const std::string& file, const std::string& text)
{
	std::ofstream ofs(file);
	REQUIRE(ofs.good());
	ofs << text;
}

struct TagPair
{
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::string fileA, fileB;
	std::shared_ptr<ICCPPortBase> a; //the server: serves the devices
	std::shared_ptr<ICCPPortBase> b; //the client: operates them
	TagSink sinkA, sinkB;

	explicit TagPair(int port, bool taggable = true)
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		fileA = "iccp_tags_server_" + std::to_string(port) + ".conf";
		fileB = "iccp_tags_client_" + std::to_string(port) + ".conf";
		WriteFile(fileA, TagNodeConf("A", port, taggable));
		WriteFile(fileB, TagNodeConf("B", port, taggable));
		a = std::make_shared<ICCPServerPort>("NodeA", fileA, Json::Value(Json::objectValue));
		b = std::make_shared<ICCPClientPort>("NodeB", fileB, Json::Value(Json::objectValue));
		for(auto* p : {&a, &b})
			(*p)->Build();
		a->Subscribe(&sinkA, "TagSink");
		b->Subscribe(&sinkB, "TagSink");
	}
	~TagPair()
	{
		for(auto* p : {&a, &b})
			if(*p)
				(*p)->Disable();
		WaitFor([&] { return !a->AssociationUp() && !b->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		a.reset();
		b.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(fileA.c_str());
		std::remove(fileB.c_str());
	}
	void Connect()
	{
		REQUIRE(a->ConfigValid());
		REQUIRE(b->ConfigValid());
		a->Enable();
		b->Enable();
		REQUIRE(WaitFor([&] { return sinkA.Count(odc::ConnectState::CONNECTED) == 1 && sinkB.Count(odc::ConnectState::CONNECTED) == 1; }));
	}

	//a UI command of the node's port: the RESULT text
	static std::string Ui(ICCPPortBase& port, const std::string& command, const std::vector<std::string>& args)
	{
		const auto responder = port.GetUIResponder();
		REQUIRE(responder.second != nullptr);
		ParamCollection params;
		params["0"] = port.GetName();
		for(size_t i = 0; i < args.size(); ++i)
			params[std::to_string(i + 1)] = args[i];
		return responder.second->ExecuteCommand(command, params)["RESULT"].asString();
	}
	//the entry of the tag list of the server for a device
	Json::Value TagOfDevice(const std::string& device)
	{
		ParamCollection params;
		params["0"] = "NodeA";
		const auto r = a->GetUIResponder().second->ExecuteCommand("Tags", params);
		for(const auto& e : r["Tags"]["NodeA"])
			if(e["Control"].asString() == device)
				return e;
		return Json::Value();
	}
	bool TagIs(const std::string& device, const std::string& tag, const std::string& reason, const std::string& setBy)
	{
		const auto e = TagOfDevice(device);
		return e["Tag"].asString() == tag && e["Reason"].asString() == reason && e["SetBy"].asString() == setBy;
	}
	static uint64_t Stat(ICCPPortBase& p, const char* key)
	{
		return p.GetStatistics()[key].asUInt64();
	}
};

std::shared_ptr<odc::EventInfo> Crob(size_t index, odc::ControlCode code)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, "TestHarness", odc::QualityFlags::ONLINE);
	odc::ControlRelayOutputBlock crob;
	crob.functionCode = code;
	e->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(crob));
	return e;
}
std::shared_ptr<odc::EventInfo> Setpoint(size_t index, double value)
{
	auto e = std::make_shared<odc::EventInfo>(odc::EventType::AnalogOutputDouble64, index, "TestHarness", odc::QualityFlags::ONLINE);
	e->SetPayload<odc::EventType::AnalogOutputDouble64>(odc::AOD(value, odc::CommandStatus::SUCCESS));
	return e;
}
//an ODC command into a port, and what it answers
odc::CommandStatus Command(ICCPPortBase& port, std::shared_ptr<odc::EventInfo> event)
{
	std::atomic_bool done{false};
	std::atomic<odc::CommandStatus> status{odc::CommandStatus::UNDEFINED};
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([&](odc::CommandStatus s)
		{
			status = s;
			done = true;
		});
	port.Event(event, "TestHarness", cb);
	REQUIRE(WaitFor([&] { return done.load(); }));
	return status.load();
}
}

TEST_CASE(SUITE("two ports over TCP: Set Tag and Get Tag from the client, enforcement, local tags and Local Reset from the UI, and the counters"))
{
	TagPair h(20430);
	h.Connect();
	using CC = odc::ControlCode;
	auto& A = *h.a;
	auto& B = *h.b;

	//1. the list of tags: the taggable devices only, all NO_TAG
	{
		ParamCollection params;
		params["0"] = "NodeA";
		const auto r = A.GetUIResponder().second->ExecuteCommand("Tags", params);
		CHECK(r["RESULT"].asString() == "Success");
		const auto& list = r["Tags"]["NodeA"];
		std::vector<std::string> names;
		for(const auto& e : list)
		{
			names.push_back(e["Control"].asString());
			CHECK(e["Tag"].asString() == "NO_TAG");
			CHECK(e["Reason"].asString() == "");
			CHECK(e["SetBy"].asString() == "");
		}
		std::sort(names.begin(), names.end());
		CHECK(names == std::vector<std::string>{"DEV_DIR", "DEV_SBO", "DEV_SP", "VCC_DIR"});
		CHECK(h.TagOfDevice("VCC_DIR")["Scope"].asString() == "VCC");
		CHECK(h.TagOfDevice("DEV_DIR")["Scope"].asString() == "ICC");
		CHECK(h.TagOfDevice("DEV_DIR")["Domain"].asString() == "DOMAIN_A");
	}

	//2. Set Tag from the client (the reason is the rest of the line)
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR", "OPEN_AND_CLOSE_INHIBIT", "maintenance", "on", "feeder", "7"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_DIR", "OPEN_AND_CLOSE_INHIBIT", "maintenance on feeder 7", "Client"); }));
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagsWritten") == 1 && TagPair::Stat(A, "TagsSet") == 1; }));
	//3. Get Tag
	CHECK(TagPair::Ui(B, "GetTag", {"DEV_DIR"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagsRead") == 1; }));

	//4. the tag is enforced: ON (value 2) and OFF (3) are both refused by OPEN_AND_CLOSE_INHIBIT; the answer is the busy / temporarily-unavailable one
	CHECK(Command(B, Crob(0, CC::LATCH_ON)) == odc::CommandStatus::ALREADY_ACTIVE);
	CHECK(Command(B, Crob(0, CC::LATCH_OFF)) == odc::CommandStatus::ALREADY_ACTIVE);
	CHECK(h.sinkA.Count() == 0);
	REQUIRE(WaitFor([&] { return TagPair::Stat(A, "ControlOperatesRefusedByTag") == 2 && TagPair::Stat(A, "ControlRefused") >= 2; }));
	//5. CLOSE_ONLY_INHIBIT: only the Close (ON) is refused
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR", "close-only-inhibit"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_DIR", "CLOSE_ONLY_INHIBIT", "", "Client"); }));
	CHECK(Command(B, Crob(0, CC::LATCH_ON)) == odc::CommandStatus::ALREADY_ACTIVE);
	CHECK(Command(B, Crob(0, CC::LATCH_OFF)) == odc::CommandStatus::SUCCESS);
	CHECK(h.sinkA.Count() == 1);
	//6. the local operator removes the tag: ON goes through
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR", "NO_TAG", "tag removed"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_DIR", "NO_TAG", "tag removed", ""); }));
	CHECK(Command(B, Crob(0, CC::LATCH_ON)) == odc::CommandStatus::SUCCESS);
	CHECK(h.sinkA.Count() == 2);
	REQUIRE(WaitFor([&] { return TagPair::Stat(A, "LocalTagsSet") == 1; }));
	//7. a setpoint is not prevented by CLOSE_ONLY_INHIBIT, and always by OPEN_AND_CLOSE_INHIBIT
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_SP", "OPEN_AND_CLOSE_INHIBIT"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_SP", "OPEN_AND_CLOSE_INHIBIT", "", "Client"); }));
	CHECK(Command(B, Setpoint(0, 5)) == odc::CommandStatus::ALREADY_ACTIVE);
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_SP", "CLOSE_ONLY_INHIBIT"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_SP", "CLOSE_ONLY_INHIBIT", "", "Client"); }));
	CHECK(Command(B, Setpoint(0, 5)) == odc::CommandStatus::SUCCESS);

	//8. Local Reset: B selects DEV_SBO and, expecting another CheckBackID, does not operate: the device stays ARMED on A
	CHECK(Command(B, Crob(1, CC::LATCH_ON)) == odc::CommandStatus::FORMAT_ERROR);
	const auto failuresBefore = TagPair::Stat(B, "TagRequestFailures");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_SBO", "CLOSE_ONLY_INHIBIT", "while", "armed"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagRequestFailures") == failuresBefore + 1; })); //refused: temporarily-unavailable
	CHECK(h.TagIs("DEV_SBO", "NO_TAG", "", ""));
	//the device state is shown by Get Tag (the log) and by the reset: the reset finds it ARMED
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_SBO", "OPEN_AND_CLOSE_INHIBIT", "emergency", "outage"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(A, "LocalResets") == 1; }));
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_SBO", "OPEN_AND_CLOSE_INHIBIT", "emergency outage", "Local"); }));
	//the device is IDLE: a client can set the tag of an ICC device that was tagged locally
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_SBO", "NO_TAG"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_SBO", "NO_TAG", "", ""); }));
	//a reset of a device that is not ARMED changes no state (and counts nothing); with no tag it leaves the tag alone
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_SBO"}) == "Success");
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_DIR"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_DIR", "NO_TAG", "tag removed", ""); }));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CHECK(TagPair::Stat(A, "LocalResets") == 1);

	//9. a device that is not taggable: refused by the peer, counted on both sides
	const auto refusedBefore = TagPair::Stat(A, "TagRequestsRefused");
	const auto failedBefore = TagPair::Stat(B, "TagRequestFailures");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_PLAIN", "NO_TAG"}) == "Success");
	CHECK(TagPair::Ui(B, "GetTag", {"DEV_PLAIN"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagRequestFailures") == failedBefore + 2 && TagPair::Stat(A, "TagRequestsRefused") == refusedBefore + 2; }));
	CHECK(Command(B, Crob(2, CC::LATCH_ON)) == odc::CommandStatus::SUCCESS); //still operable

	//10. a VCC device: the client that set the tag can change it, and a tag set locally can only be changed locally
	CHECK(TagPair::Ui(B, "SetTag", {"VCC_DIR", "CLOSE_ONLY_INHIBIT", "vcc"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("VCC_DIR", "CLOSE_ONLY_INHIBIT", "vcc", "Client"); }));
	CHECK(TagPair::Ui(A, "LocalSetTag", {"VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "local"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "local", "Local"); }));
	const auto vccFailed = TagPair::Stat(B, "TagRequestFailures");
	CHECK(TagPair::Ui(B, "SetTag", {"VCC_DIR", "NO_TAG"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagRequestFailures") == vccFailed + 1; }));
	CHECK(h.TagIs("VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "local", "Local"));
	CHECK(TagPair::Ui(A, "LocalSetTag", {"VCC_DIR", "NO_TAG"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("VCC_DIR", "NO_TAG", "", ""); }));
	CHECK(TagPair::Ui(B, "SetTag", {"VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "again"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "again", "Client"); }));

	//11. the tags belong to the devices: they survive the association, and the same client is still the owner of the VCC tag on the next one
	B.Disable();
	REQUIRE(WaitFor([&] { return h.sinkB.Count(odc::ConnectState::DISCONNECTED) == 1; }));
	REQUIRE(WaitFor([&] { return !A.AssociationUp(); }));
	CHECK(h.TagIs("VCC_DIR", "OPEN_AND_CLOSE_INHIBIT", "again", "Client"));
	CHECK(TagPair::Ui(B, "GetTag", {"DEV_DIR"}) == "NodeB: the association is not up");
	B.Enable();
	REQUIRE(WaitFor([&] { return h.sinkB.Count(odc::ConnectState::CONNECTED) == 2 && h.sinkA.Count(odc::ConnectState::CONNECTED) == 2; }, 20000));
	CHECK(TagPair::Ui(B, "SetTag", {"VCC_DIR", "NO_TAG"}) == "Success");
	REQUIRE(WaitFor([&] { return h.TagIs("VCC_DIR", "NO_TAG", "", ""); }));
	//the counters add up over the associations
	CHECK(TagPair::Stat(A, "LocalResets") == 1);
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagsWritten") == 8 && TagPair::Stat(A, "TagsSet") == 8; }));
	CHECK(TagPair::Stat(B, "TagsRead") == 1);
}

TEST_CASE(SUITE("the UI commands refuse what cannot be done, and say why"))
{
	TagPair h(20431);
	h.Connect();
	auto& A = *h.a;
	auto& B = *h.b;
	//the server side
	CHECK(TagPair::Ui(A, "LocalReset", {}) == "Bad parameter: the first two parameters are the ICCP port name (or a regex) and the control name");
	CHECK(TagPair::Ui(A, "LocalReset", {"NOPE"}) == "NodeA: this port does not serve a control device named 'NOPE'");
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_PLAIN", "NO_TAG"}) == "NodeA: control 'DEV_PLAIN' is not taggable (Taggable is not set): it has no tag to change");
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_DIR", "WRONG"}) == "Bad parameter: the tag must be NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT");
	CHECK(TagPair::Ui(A, "LocalReset", {"DEV_DIR", "NO_TAG", std::string(256, 'x')}) == "NodeA: the reason must be at most 255 characters of visible ASCII");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR"}) == "Bad parameter: the tag is required (NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT)");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR", "BAD"}) == "Bad parameter: the tag must be NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_PLAIN", "NO_TAG"}) == "NodeA: control 'DEV_PLAIN' is not taggable (Taggable is not set): it has no tag to change");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"NOPE", "NO_TAG"}) == "NodeA: this port does not serve a control device named 'NOPE'");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR", "NO_TAG", std::string(300, 'x')}) == "NodeA: the reason must be at most 255 characters of visible ASCII");
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR", "NO_TAG", "tab\there"}) == "NodeA: the reason must be at most 255 characters of visible ASCII");
	//nothing was changed by any of them
	CHECK(h.TagIs("DEV_DIR", "NO_TAG", "", ""));
	//the client side
	CHECK(TagPair::Ui(B, "GetTag", {}) == "Bad parameter: the first two parameters are the ICCP port name (or a regex) and the control name");
	CHECK(TagPair::Ui(B, "GetTag", {"NOPE"}) == "NodeB: this port does not operate a control device of the peer named 'NOPE'");
	CHECK(TagPair::Ui(B, "SetTag", {"NOPE", "NO_TAG"}) == "NodeB: this port does not operate a control device of the peer named 'NOPE'");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR"}) == "Bad parameter: the tag is required (NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT)");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR", "WRONG"}) == "Bad parameter: the tag must be NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR", "NO_TAG", std::string(256, 'x')}) == "NodeB: the reason must be at most 255 characters of visible ASCII");
	//a command goes to the ports that can do it: the client port has no devices of its own, the server port none of the peer's
	CHECK(TagPair::Ui(B, "LocalReset", {"DEV_DIR"}) == "No ICCP port that serves devices matched");
	CHECK(TagPair::Ui(B, "LocalSetTag", {"DEV_DIR", "NO_TAG"}) == "No ICCP port that serves devices matched");
	CHECK(TagPair::Ui(B, "Tags", {}) == "No ICCP port that serves devices matched");
	CHECK(TagPair::Ui(A, "GetTag", {"DEV_DIR"}) == "No ICCP port that operates the peer's devices matched");
	CHECK(TagPair::Ui(A, "SetTag", {"DEV_DIR", "NO_TAG"}) == "No ICCP port that operates the peer's devices matched");
	//the existing commands are for the ports that serve, as before
	CHECK(TagPair::Ui(B, "OperatorRequest", {}) == "No ICCP port matched");
	CHECK(TagPair::Ui(A, "OperatorRequest", {}) == "Success");
	//a regex names several ports; the ones that cannot do it are not asked
	{
		const auto responder = A.GetUIResponder();
		ParamCollection params;
		params["0"] = "Node.";
		params["1"] = "DEV_DIR";
		params["2"] = "CLOSE_ONLY_INHIBIT";
		CHECK(responder.second->ExecuteCommand("LocalSetTag", params)["RESULT"].asString() == "Success");
		params.erase("1");
		params.erase("2");
		const auto tags = responder.second->ExecuteCommand("Tags", params);
		CHECK(tags["Tags"].isMember("NodeA"));
		CHECK_FALSE(tags["Tags"].isMember("NodeB"));
		params["0"] = "(";
		CHECK(responder.second->ExecuteCommand("Tags", params)["RESULT"].asString() == "Bad regex");
		params["0"] = "NoSuchPort";
		CHECK(responder.second->ExecuteCommand("Tags", params)["RESULT"].asString() == "No ICCP port that serves devices matched");
	}
	REQUIRE(WaitFor([&] { return h.TagIs("DEV_DIR", "CLOSE_ONLY_INHIBIT", "", "Local"); }));
	//the help of the commands
	const auto commands = A.GetUIResponder().second->GetCommandList();
	std::vector<std::string> names;
	for(const auto& c : commands)
		names.push_back(c.asString());
	for(const char* n : {"OperatorRequest", "OtherExternalEvent", "LocalReset", "LocalSetTag", "Tags", "GetTag", "SetTag"})
		CHECK(std::find(names.begin(), names.end(), n) != names.end());
}

TEST_CASE(SUITE("a configuration without Taggable is unchanged: no tag statistics on the server, no tag objects, the devices work"))
{
	TagPair h(20432, false);
	h.Connect();
	auto& A = *h.a;
	auto& B = *h.b;
	using CC = odc::ControlCode;
	CHECK(Command(B, Crob(0, CC::LATCH_ON)) == odc::CommandStatus::SUCCESS);
	CHECK(Command(B, Crob(1, CC::LATCH_ON)) == odc::CommandStatus::FORMAT_ERROR); //the select happened (the CheckBackID differs)
	CHECK(Command(B, Setpoint(0, 3)) == odc::CommandStatus::SUCCESS);
	const auto serverStats = A.GetStatistics();
	for(const char* key : {"TagsSet", "LocalTagsSet", "TagRequestsRefused", "ControlSelectsRefusedByTag", "ControlOperatesRefusedByTag", "LocalResets", "TagsRead", "TagsWritten", "TagRequestFailures"})
		CHECK_FALSE(serverStats.isMember(key));
	CHECK(serverStats.isMember("ControlOperatesReceived")); //the existing statistics are there
	CHECK(serverStats.isMember("ControlRefused"));
	//the client side has the tag counters of a port that operates devices
	const auto clientStats = B.GetStatistics();
	for(const char* key : {"TagsRead", "TagsWritten", "TagRequestFailures"})
		CHECK(clientStats.isMember(key));
	CHECK_FALSE(clientStats.isMember("TagsSet"));
	//the list of tags is empty, and Set Tag of any device is answered object-non-existent by the peer (a failure here)
	ParamCollection params;
	params["0"] = "NodeA";
	const auto tags = A.GetUIResponder().second->ExecuteCommand("Tags", params);
	CHECK(tags["Tags"]["NodeA"].isArray());
	CHECK(tags["Tags"]["NodeA"].empty());
	CHECK(TagPair::Ui(A, "LocalSetTag", {"DEV_DIR", "NO_TAG"}) == "NodeA: control 'DEV_DIR' is not taggable (Taggable is not set): it has no tag to change");
	CHECK(TagPair::Ui(B, "SetTag", {"DEV_DIR", "OPEN_AND_CLOSE_INHIBIT"}) == "Success");
	REQUIRE(WaitFor([&] { return TagPair::Stat(B, "TagRequestFailures") == 1; }));
	CHECK(Command(B, Crob(0, CC::LATCH_ON)) == odc::CommandStatus::SUCCESS); //nothing was tagged
}

