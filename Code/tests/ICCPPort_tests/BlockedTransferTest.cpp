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
 * BlockedTransferTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 3, blocked transfers (the BlockData attribute, IEC 60870-6-503 clause 7.2.5.5.3):
 *   - the octet layout of every point type and the three RBE blocking rules, byte for byte, with the malformed input table, truncation and mutation,
 *   - the recognition of a blocked report,
 *   - the server side (ResponderSession) against a scripted client in memory with a fake clock,
 *   - the client side (ClientSession) against a ResponderSession and against scripted reports in memory,
 *   - two ICCP ports over real TCP (ports 20450 to 20469).
 *  Part of the layout is verified against real peers' captures ([V] in the README): the vectors there are built by hand with synthetic names and values. The rest pins this port's reading of the
 *  standard's text (a regression pin), it is not a capture.
 *  Synthetic names only.
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"
#include "StackTestHelpers.h"
#include "Tase2Blocked.h"
#include "Tase2Client.h"
#include "Tase2Responder.h"
#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <opendatacon/spdlog.h>
#include <optional>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <thread>

#define SUITE(name) "ICCP blocked transfers - " name

using namespace iccp;
using iccp::stack::LogLevel;
namespace DC = iccp::tase2::DsCondition;
namespace B = iccp::tase2::blocked;
using tase2::Bytes;
using tase2::PointType;
using tase2::PointValue;

namespace
{
constexpr odc::msSinceEpoch_t Wall = 1700000000000;
constexpr int64_t WallSeconds = 1700000000;
odc::msSinceEpoch_t Ms(int64_t seconds)
{
	return static_cast<odc::msSinceEpoch_t>(seconds) * 1000;
}

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

Bytes Cat(std::initializer_list<Bytes> parts)
{
	Bytes out;
	for(const auto& p : parts)
		out.insert(out.end(), p.begin(), p.end());
	return out;
}

//---------------------------------------------------------------------------------------------------------------------
// the value layouts
//---------------------------------------------------------------------------------------------------------------------

PointValue Pv(PointType type, double real, int64_t discrete, uint8_t flags, std::optional<int64_t> ts = std::nullopt, std::optional<int64_t> ms = std::nullopt,
	std::optional<int64_t> cov = std::nullopt)
{
	PointValue p;
	p.type = type;
	p.real = real;
	p.discrete = discrete;
	p.flags = flags;
	p.timeStamp = ts;
	p.milliseconds = ms;
	p.cov = cov;
	if(tase2::IsStateType(type))
		p.state = tase2::UnpackFlags(flags).state;
	return p;
}

struct VectorCase
{
	const char* name;
	PointType type;
	PointValue value;
	Bytes bytes;
};

//1700000000 = 0x6553F100; 1.5f = 3FC00000; -2.0f = C0000000; 100.25f = 42C88000
const std::vector<VectorCase>& Cases()
{
	static const std::vector<VectorCase> cases = {
		{"Real", PointType::Real, Pv(PointType::Real, 1.5, 0, 0), {0x3F, 0xC0, 0x00, 0x00}
		},
		{"RealQ", PointType::RealQ, Pv(PointType::RealQ, 1.5, 0, 0x10), {0x3F, 0xC0, 0x00, 0x00, 0x10}
		},
		{"RealQTimeTag", PointType::RealQTimeTag, Pv(PointType::RealQTimeTag, -2.0, 0, 0x30, 1700000000), {0x65, 0x53, 0xF1, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x30}
		},
		{"RealExtended", PointType::RealExtended, Pv(PointType::RealExtended, 100.25, 0, 0x01, 1700000000, 500, 258),
		 {0x65, 0x53, 0xF1, 0x00, 0x01, 0xF4, 0x42, 0xC8, 0x80, 0x00, 0x01, 0x01, 0x02}
		},
		{"State", PointType::State, Pv(PointType::State, 0, 0, 0x80), {0x80}
		},
		{"StateQ", PointType::StateQ, Pv(PointType::StateQ, 0, 0, 0x90), {0x90}
		},
		{"StateQTimeTag", PointType::StateQTimeTag, Pv(PointType::StateQTimeTag, 0, 0, 0x48, 1700000000), {0x65, 0x53, 0xF1, 0x00, 0x48}
		},
		{"StateExtended", PointType::StateExtended, Pv(PointType::StateExtended, 0, 0, 0xC1, 1700000000, 999, 65535),
		 {0x65, 0x53, 0xF1, 0x00, 0x03, 0xE7, 0xC1, 0xFF, 0xFF}
		},
		{"Discrete", PointType::Discrete, Pv(PointType::Discrete, 0, -2, 0), {0xFF, 0xFF, 0xFF, 0xFE}
		},
		{"DiscreteQ", PointType::DiscreteQ, Pv(PointType::DiscreteQ, 0, 305419896, 0x20), {0x12, 0x34, 0x56, 0x78, 0x20}
		},
		{"DiscreteQTimeTag", PointType::DiscreteQTimeTag, Pv(PointType::DiscreteQTimeTag, 0, -1, 0x00, 1700000000), {0x65, 0x53, 0xF1, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}
		},
		{"DiscreteExtended", PointType::DiscreteExtended, Pv(PointType::DiscreteExtended, 0, 2147483647, 0x02, 1700000000, 0, 0),
		 {0x65, 0x53, 0xF1, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x02, 0x00, 0x00}
		}
	};
	return cases;
}

//the state or the number of a decoded value, for short comparisons
double Num(const PointValue& v)
{
	if(tase2::IsStateType(v.type))
		return v.state == tase2::StateValue::On ? 1.0 : 0.0;
	return tase2::IsRealType(v.type) ? v.real : static_cast<double>(v.discrete);
}
}

TEST_CASE(SUITE("the octets of every point type (a regression pin of the layout, which is this port's reading [S])"))
{
	REQUIRE(Cases().size() == 12);
	for(const auto& c : Cases())
	{
		CAPTURE(c.name);
		CHECK(B::ValueWidth(c.type) == c.bytes.size());
		Bytes out;
		B::AppendValue(out, c.value);
		CHECK(out == c.bytes);
		const PointValue back = B::ReadValue(c.bytes.data(), c.bytes.size(), c.type);
		CHECK(back == c.value);
		//the same value as the ordinary form reads and writes it
		CHECK(tase2::ParsePoint(tase2::ToData(c.value), c.type) == c.value);
		//a value is exactly its width
		CHECK_THROWS_AS(B::ReadValue(c.bytes.data(), c.bytes.size() - 1, c.type), tase2::DecodeError);
		Bytes longer = c.bytes;
		longer.push_back(0);
		CHECK_THROWS_AS(B::ReadValue(longer.data(), longer.size(), c.type), tase2::DecodeError);
		CHECK_THROWS_AS(B::ReadValue(c.bytes.data(), 0, c.type), tase2::DecodeError);
	}
}

TEST_CASE(SUITE("a value is cut to its field, a missing part is refused, a State value with other bits set is not read"))
{
	Bytes out;
	//time stamp and discrete to 32 bits, milliseconds and COV to 16
	B::AppendValue(out, Pv(PointType::DiscreteExtended, 0, 0x1FFFFFFFFLL, 0, 0x100000001LL, 0x10001, 0x20002));
	CHECK(out == Bytes{0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02});
	PointValue noStamp = Pv(PointType::RealQTimeTag, 1.0, 0, 0);
	CHECK_THROWS_AS(B::AppendValue(out, noStamp), std::invalid_argument);
	PointValue noCov = Pv(PointType::StateExtended, 0, 0, 0, 1, 2);
	CHECK_THROWS_AS(B::AppendValue(out, noCov), std::invalid_argument);
	const uint8_t bad[] = {0x81};
	CHECK_THROWS_AS(B::ReadValue(bad, 1, PointType::State), tase2::DecodeError);
	//a time stamp is unsigned, a discrete value signed
	const uint8_t high[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
	const PointValue t = B::ReadValue(high, 9, PointType::DiscreteQTimeTag);
	CHECK(*t.timeStamp == 4294967295LL);
	CHECK(t.discrete == -1);
}

TEST_CASE(SUITE("every point type in all three rules: encode, decode and the exact octets"))
{
	for(const auto& c : Cases())
	{
		CAPTURE(c.name);
		const std::vector<B::Member> members(3, B::Member::Point(c.type));
		const PointValue other = c.type == PointType::State || c.type == PointType::StateQ ? Pv(c.type, 0, 0, 0x40) : c.value;
		//rule 0: the whole data set in order, no positions
		const std::vector<B::Item> all = {
			{0, c.value}, {1, other}, {2, c.value}
		};
		Bytes o2, o0;
		B::AppendValue(o2, c.value);
		B::AppendValue(o0, other);
		const Bytes expected0 = Cat({
			{0x00}, o2, o0, o2});
		const Bytes block0 = B::Encode(B::Rule::NoRbe, all);
		CHECK(block0 == expected0);
		auto d0 = B::Decode(block0, members);
		CHECK(d0.rule == B::Rule::NoRbe);
		REQUIRE(d0.items.size() == 3);
		for(size_t i = 0; i < 3; ++i)
		{
			CHECK(d0.items[i].position == i);
			CHECK(d0.items[i].value == all[i].value);
		}
		CHECK(B::Encode(false, all) == block0);

		//rule 1: position (1 octet) and value, for the items given, in the order given
		const std::vector<B::Item> some = {
			{2, c.value}, {0, other}
		};
		const Bytes block1 = B::Encode(B::Rule::Offset1, some);
		CHECK(block1 == Cat({
			{0x01, 0x02}, o2, {0x00}, o0}));
		auto d1 = B::Decode(block1, members);
		CHECK(d1.rule == B::Rule::Offset1);
		REQUIRE(d1.items.size() == 2);
		CHECK(d1.items[0].position == 2);
		CHECK(d1.items[0].value == c.value);
		CHECK(d1.items[1].position == 0);
		CHECK(d1.items[1].value == other);
		CHECK(B::Encode(true, some) == block1); //the most compact for RBE

		//rule 2: position (2 octets, big endian)
		const Bytes block2 = B::Encode(B::Rule::Offset2, some);
		CHECK(block2 == Cat({
			{0x02, 0x00, 0x02}, o2, {0x00, 0x00}, o0}));
		auto d2 = B::Decode(block2, members);
		CHECK(d2.rule == B::Rule::Offset2);
		REQUIRE(d2.items.size() == 2);
		CHECK(d2.items[0].position == 2);
		CHECK(d2.items[0].value == c.value);
		CHECK(d2.items[1].position == 0);
		CHECK(d2.items[1].value == other);
	}
}

TEST_CASE(SUITE("a data set of all twelve types, whole and in part"))
{
	std::vector<B::Member> members;
	std::vector<B::Item> all;
	Bytes concat;
	for(const auto& c : Cases())
	{
		all.push_back(B::Item{members.size(), c.value});
		members.push_back(B::Member::Point(c.type));
		concat.insert(concat.end(), c.bytes.begin(), c.bytes.end());
	}
	const Bytes whole = B::Encode(B::Rule::NoRbe, all);
	CHECK(whole == Cat({
		{0x00}, concat}));
	const auto d = B::Decode(whole, members);
	REQUIRE(d.items.size() == 12);
	for(size_t i = 0; i < 12; ++i)
		CHECK(d.items[i].value == Cases()[i].value);

	//positions 11, 0, 5 and 7 (an RBE report), in that order
	std::vector<B::Item> some;
	Bytes expected = {0x01};
	for(size_t pos : {11u, 0u, 5u, 7u})
	{
		some.push_back(all[pos]);
		expected.push_back(static_cast<uint8_t>(pos));
		expected.insert(expected.end(), Cases()[pos].bytes.begin(), Cases()[pos].bytes.end());
	}
	CHECK(B::Encode(true, some) == expected);
	const auto ds = B::Decode(expected, members);
	CHECK(ds.rule == B::Rule::Offset1);
	REQUIRE(ds.items.size() == 4);
	const size_t order[] = {11, 0, 5, 7};
	for(size_t i = 0; i < 4; ++i)
	{
		CHECK(ds.items[i].position == order[i]);
		CHECK(ds.items[i].value == Cases()[order[i]].value);
	}
	//with RBE true a block that lists nothing is the rule octet only, and reads as no values
	CHECK(B::Encode(true, {}) == Bytes{0x01});
	CHECK(B::Decode(Bytes{0x01}, members).items.empty());
	CHECK(B::Decode(Bytes{0x02}, members).items.empty());
}

TEST_CASE(SUITE("rule 2 [S, from the standard's text]: rule octet 02, then (2 octet unsigned position, value) pairs, ALL pairs of the report with 2 octet positions, the special objects counted"))
{
	//three special objects and 300 RealQ points: positions 3 to 302
	std::vector<B::Member> members = {B::Member::Object(tase2::Special::TransferSetName), B::Member::Object(tase2::Special::TimeStamp), B::Member::Object(tase2::Special::ConditionsDetected)};
	for(int i = 0; i < 300; ++i)
		members.push_back(B::Member::Point(PointType::RealQ));
	const PointValue one = Pv(PointType::RealQ, 1.5, 0, 0x00);
	const PointValue two = Pv(PointType::RealQ, -2.0, 0, 0x10);
	//a report whose positions are 5 and 301: one is above 255, so BOTH are 2 octets; rule 02, 0005 3FC00000 00, 012D C0000000 10
	const std::vector<B::Item> mixed = {B::Item{5, one}, B::Item{301, two}
	};
	const Bytes expected = {0x02, 0x00, 0x05, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x01, 0x2D, 0xC0, 0x00, 0x00, 0x00, 0x10};
	CHECK(B::ChooseRule(true, 301) == B::Rule::Offset2);
	CHECK(B::Encode(true, mixed) == expected);
	CHECK(B::Encode(B::Rule::Offset2, mixed) == expected);
	const auto d = B::Decode(expected, members);
	CHECK(d.rule == B::Rule::Offset2);
	REQUIRE(d.items.size() == 2);
	CHECK(d.items[0].position == 5);
	CHECK(d.items[0].value == one);
	CHECK(d.items[1].position == 301);
	CHECK(d.items[1].value == two);
	//the same two points with positions that all fit one octet are rule 1, 2 octets shorter
	const std::vector<B::Item> small = {B::Item{5, one}, B::Item{255, two}
	};
	const Bytes narrow = B::Encode(true, small);
	CHECK(narrow == Bytes({0x01, 0x05, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x10}));
	CHECK(narrow.size() + 2 == B::Encode(B::Rule::Offset2, small).size());
	//the position is big endian: 0x0100 = 256 and 0x00FF = 255 are not the same; the maximum is 65535
	CHECK(B::Encode(B::Rule::Offset2, std::vector<B::Item>{B::Item{256, one}
		}) == Bytes({0x02, 0x01, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00}));
	CHECK(B::Encode(B::Rule::Offset2, std::vector<B::Item>{B::Item{65535, one}
		})[1] == 0xFF);
	CHECK_THROWS_AS(B::Encode(true, std::vector<B::Item>{B::Item{65536, one}
		}), std::invalid_argument);
}

TEST_CASE(SUITE("rule 2 is chosen when a position does not fit one octet, rule 1 otherwise (the most compact)"))
{
	CHECK(B::ChooseRule(false, 0) == B::Rule::NoRbe);
	CHECK(B::ChooseRule(false, 100000) == B::Rule::NoRbe);
	CHECK(B::ChooseRule(true, 0) == B::Rule::Offset1);
	CHECK(B::ChooseRule(true, 255) == B::Rule::Offset1);
	CHECK(B::ChooseRule(true, 256) == B::Rule::Offset2);
	CHECK(B::ChooseRule(true, 65535) == B::Rule::Offset2);

	//a large data set: 300 members
	const std::vector<B::Member> members(300, B::Member::Point(PointType::StateQ));
	const PointValue v = Pv(PointType::StateQ, 0, 0, 0x90);
	std::vector<B::Item> low = {
		{3, v}, {255, v}
	};
	std::vector<B::Item> high = {
		{3, v}, {256, v}, {299, v}
	};
	CHECK(B::Encode(true, low) == Bytes({0x01, 0x03, 0x90, 0xFF, 0x90}));
	CHECK(B::Encode(true, high) == Bytes({0x02, 0x00, 0x03, 0x90, 0x01, 0x00, 0x90, 0x01, 0x2B, 0x90}));
	const auto d = B::Decode(B::Encode(true, high), members);
	CHECK(d.rule == B::Rule::Offset2);
	REQUIRE(d.items.size() == 3);
	CHECK(d.items[2].position == 299);
	//the whole data set with RBE true lists every position
	std::vector<B::Item> every;
	for(size_t i = 0; i < 300; ++i)
		every.push_back(B::Item{i, v});
	const Bytes block = B::Encode(true, every);
	CHECK(block[0] == 2);
	CHECK(block.size() == 1 + 300 * 3);
	CHECK(B::Decode(block, members).items.size() == 300);
	CHECK(B::Encode(false, every).size() == 1 + 300);
	//what does not fit its rule is refused, not cut
	CHECK_THROWS_AS(B::Encode(B::Rule::Offset1, std::vector<B::Item>{
		{256, v}
	}), std::invalid_argument);
	CHECK_THROWS_AS(B::Encode(B::Rule::Offset2, std::vector<B::Item>{
		{65536, v}
	}), std::invalid_argument);
	//a block with an unreadable position and one in the second octet only
	CHECK_THROWS_AS(B::Decode(Bytes{0x01, 0xFF, 0x90}, std::vector<B::Member>(255, B::Member::Point(PointType::StateQ))), tase2::DecodeError);
	CHECK(B::Decode(Bytes{0x01, 0xFE, 0x90}, std::vector<B::Member>(255, B::Member::Point(PointType::StateQ))).items.size() == 1);
}

namespace
{
struct Malformed
{
	const char* name;
	std::vector<B::Member> members;
	Bytes data;
	const char* why;
};
}

TEST_CASE(SUITE("malformed blocked data is a DecodeError that says what is wrong, and returns nothing"))
{
	const std::vector<B::Member> three = {B::Member::Point(PointType::StateQ), B::Member::Point(PointType::RealQ), B::Member::Point(PointType::Discrete)};
	const Bytes good = {0x00, 0x90, 0x3F, 0xC0, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x05};
	REQUIRE(B::Decode(good, three).items.size() == 3); //the reference
	Bytes goodShort(good.begin(), good.end() - 1);
	Bytes goodLong = good;
	goodLong.push_back(0);
	const std::vector<Malformed> table = {
		{"no octets at all", three, {}, "empty"},
		{"unknown rule 3", three, {0x03, 0x90}, "unknown RBE blocking rule 3"},
		{"unknown rule 255", three, {0xFF}, "unknown RBE blocking rule 255"},
		{"rule 0 with no values", three, {0x00}, "truncated"},
		{"rule 0 one octet short", three, goodShort, "truncated"},
		{"rule 0 with leftover octets", three, goodLong, "leftover"},
		{"rule 1 position outside the data set", three, {0x01, 0x03, 0x90}, "outside the data set"},
		{"rule 1 a position and no value", three, {0x01, 0x00}, "truncated"},
		{"rule 1 a value cut short", three, {0x01, 0x01, 0x3F, 0xC0}, "truncated"},
		{"rule 2 half a position", three, {0x02, 0x00}, "truncated"},
		{"rule 2 position 256 in a data set of three", three, {0x02, 0x01, 0x00, 0x90}, "outside the data set"},
		{"rule 1 more values than members", three, {0x01, 0x00, 0x90, 0x00, 0x90, 0x00, 0x90, 0x00, 0x90}, "count mismatch"},
		{"rule 2 more values than members", three, {0x02, 0x00, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x00, 0x90}, "count mismatch"},
		{"a State value with bits below the state", {B::Member::Point(PointType::State)}, {0x00, 0x81}, "below the 2 bit state"},
		{"rule 0 of an empty data set with a value", {}, {0x00, 0x90}, "leftover"}
	};
	for(const auto& m : table)
	{
		CAPTURE(m.name);
		try
		{
			B::Decode(m.data, m.members);
			FAIL("decoded");
		}
		catch(const tase2::DecodeError& e)
		{
			CHECK(std::string(e.what()).find(m.why) != std::string::npos);
		}
	}
}

namespace
{
Bytes Pad32(const std::string& s)
{
	Bytes b(s.begin(), s.end());
	b.resize(32, 0);
	return b;
}
std::vector<B::Member> RealHead()
{
	return {B::Member::Object(tase2::Special::TransferSetName), B::Member::Object(tase2::Special::TimeStamp), B::Member::Object(tase2::Special::ConditionsDetected)};
}
//the three special objects of a report, as the items of the first three positions
std::vector<B::Item> HeadItems(uint8_t conditions = DC::IntervalTimeOut, int64_t stamp = 1700000000)
{
	B::SpecialValue name;
	name.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"};
	B::SpecialValue time;
	time.number = stamp;
	B::SpecialValue cond;
	cond.conditions = conditions;
	return {B::ObjectItem(0, tase2::Special::TransferSetName, name), B::ObjectItem(1, tase2::Special::TimeStamp, time), B::ObjectItem(2, tase2::Special::ConditionsDetected, cond)};
}
}

TEST_CASE(SUITE("the layout a real peer sends, rule 0 [V]: the special objects first, in fixed width storage forms, then the points; synthetic bytes built by hand from the capture's facts"))
{
	//Transfer_Set_Name: scope 1, the domain "DOMAIN_A" and the item "TS_POOL_001", each in 32 octets, zero padded = 65 octets
	const Bytes name = Cat({
		{0x01, 0x44, 0x4F, 0x4D, 0x41, 0x49, 0x4E, 0x5F, 0x41}, Bytes(24, 0x00), {0x54, 0x53, 0x5F, 0x50, 0x4F, 0x4F, 0x4C, 0x5F, 0x30, 0x30, 0x31}, Bytes(21, 0x00)});
	REQUIRE(name.size() == 65);
	const Bytes block = Cat({
		{0x00}, name,
		{0x65, 0x53, 0xF1, 0x00},       //Transfer_Set_Time_Stamp 1700000000, unsigned seconds, big endian
		{0x80},                         //DSConditions_Detected: IntervalTimeOut, bit 0 the most significant
		{0x3F, 0xC0, 0x00, 0x00, 0x10}, //RealQ: the float 1.5 and the flags
		{0xC0, 0x00, 0x00, 0x00, 0x00}
	}); //RealQ: -2.0
	CHECK(block.size() == 1 + 65 + 4 + 1 + 5 * 2);
	std::vector<B::Member> members = RealHead();
	members.push_back(B::Member::Point(PointType::RealQ));
	members.push_back(B::Member::Point(PointType::RealQ));
	const auto d = B::Decode(block, members);
	CHECK(d.rule == B::Rule::NoRbe);
	REQUIRE(d.items.size() == 5);
	CHECK(d.items[0].special == tase2::Special::TransferSetName);
	CHECK(d.items[0].object.transferSet == tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"});
	CHECK(d.items[1].special == tase2::Special::TimeStamp);
	CHECK(d.items[1].object.number == 1700000000);
	CHECK(d.items[2].special == tase2::Special::ConditionsDetected);
	CHECK(d.items[2].object.conditions == DC::IntervalTimeOut);
	CHECK_FALSE(d.items[3].special.has_value());
	CHECK(d.items[3].value == Pv(PointType::RealQ, 1.5, 0, 0x10));
	CHECK(d.items[4].value == Pv(PointType::RealQ, -2.0, 0, 0x00));
	//and the encoder writes the same octets
	std::vector<B::Item> items = HeadItems();
	items.push_back(B::Item{3, Pv(PointType::RealQ, 1.5, 0, 0x10)});
	items.push_back(B::Item{4, Pv(PointType::RealQ, -2.0, 0, 0x00)});
	CHECK(B::Encode(false, items) == block);
	//the total of the capture: 1 + 65 + 4 + 1 + 5 N octets (821 for 150 points), nothing left over
	std::vector<B::Member> big = RealHead();
	std::vector<B::Item> bigItems = HeadItems();
	for(size_t i = 0; i < 150; ++i)
	{
		big.push_back(B::Member::Point(PointType::RealQ));
		bigItems.push_back(B::Item{3 + i, Pv(PointType::RealQ, static_cast<double>(i), 0, 0x00)});
	}
	const Bytes bigBlock = B::Encode(false, bigItems);
	CHECK(bigBlock.size() == 821);
	CHECK(B::Decode(bigBlock, big).items.size() == 153);
}

TEST_CASE(SUITE("the partial report of an RBE set a real peer sends, rule 1 [V]: the changed point only, a 1 octet position counting the special objects; the three real octet strings, synthetic names"))
{
	//a data set of three special objects and 126 RealQ points: the first point is position 3, the third point position 5
	std::vector<B::Member> members = RealHead();
	for(int i = 0; i < 126; ++i)
		members.push_back(B::Member::Point(PointType::RealQ));
	float hundred = 0;
	const uint32_t bits = 0x42C80001; //100.00007
	std::memcpy(&hundred, &bits, sizeof(hundred));
	struct Real
	{
		Bytes octets;
		double value;
		uint8_t flags;
	};
	const std::vector<Real> reals = {
		{
			{0x01, 0x05, 0xC2, 0xA0, 0x00, 0x00, 0x08}, -80.0, 0x08},
		{
			{0x01, 0x05, 0x42, 0xC8, 0x00, 0x01, 0x08}, static_cast<double>(hundred), 0x08},
		{
			{0x01, 0x05, 0xC2, 0xBA, 0x14, 0x7B, 0x00}, static_cast<double>(-93.04f), 0x00}
	};
	for(const auto& r : reals)
	{
		const auto d = B::Decode(r.octets, members);
		CHECK(d.rule == B::Rule::Offset1);
		REQUIRE(d.items.size() == 1);
		CHECK(d.items[0].position == 5); //the third point: the position counts the three special objects
		CHECK_FALSE(d.items[0].special.has_value());
		CHECK(d.items[0].value == Pv(PointType::RealQ, r.value, 0, r.flags));
		//the encoder writes the same octets
		CHECK(B::Encode(B::Rule::Offset1, std::vector<B::Item>{B::Item{5, Pv(PointType::RealQ, r.value, 0, r.flags)}
			}) == r.octets);
		CHECK(B::Encode(true, std::vector<B::Item>{B::Item{5, Pv(PointType::RealQ, r.value, 0, r.flags)}
			}) == r.octets);
	}
	CHECK(hundred > 100.0f);
	//several points in one report [V] (a real peer's 13 octet string had two RealQ points): repeated pairs, in the order given
	const Bytes two = Cat({
		{0x01, 0x05, 0xC2, 0xA0, 0x00, 0x00, 0x08}, {0x08, 0x42, 0xC8, 0x00, 0x00, 0x00}
	});
	const auto d2 = B::Decode(two, members);
	REQUIRE(d2.items.size() == 2);
	CHECK(d2.items[0].position == 5);
	CHECK(d2.items[1].position == 8);
	CHECK(d2.items[1].value == Pv(PointType::RealQ, 100.0, 0, 0x00));
	CHECK(B::Encode(B::Rule::Offset1, std::vector<B::Item>{d2.items[0], d2.items[1]}) == two);
	//rule 2 [S]: the same with 2 octet big endian positions
	const Bytes wide = {0x02, 0x00, 0x05, 0xC2, 0xA0, 0x00, 0x00, 0x08};
	const auto d3 = B::Decode(wide, members);
	REQUIRE(d3.items.size() == 1);
	CHECK(d3.items[0].position == 5);
	CHECK(B::Encode(B::Rule::Offset2, std::vector<B::Item>{d3.items[0]}) == wide);
	//the complete report of the same RBE set is rule 0: 1 + 65 + 4 + 1 + 126 * 5 = 701 octets, every member in order, the special objects included
	std::vector<B::Item> all = HeadItems();
	for(size_t i = 0; i < 126; ++i)
		all.push_back(B::Item{3 + i, Pv(PointType::RealQ, static_cast<double>(i), 0, 0x00)});
	const Bytes complete = B::Encode(false, all);
	CHECK(complete.size() == 701);
	CHECK(complete[0] == 0);
	CHECK(B::Decode(complete, members).items.size() == 129);
	//a position that is a special object (a lenient reading [S]): its storage form is read and its value ignored by the client
	Bytes odd = Cat({
		{0x01, 0x01}, {0x65, 0x53, 0xF1, 0x00}, {0x05, 0xC2, 0xA0, 0x00, 0x00, 0x08}
	});
	const auto d4 = B::Decode(odd, members);
	REQUIRE(d4.items.size() == 2);
	CHECK(d4.items[0].special == tase2::Special::TimeStamp);
	CHECK(d4.items[0].object.number == 1700000000);
	CHECK(d4.items[1].position == 5);
}

TEST_CASE(SUITE("several changed points in one rule 1 report [V]: rule, then (position, value) pairs in ascending order, the special objects counted in the positions; synthetic values, the layout of a real peer's 13 octet string"))
{
	//three special objects and then StateQ, RealQ, RealQ: the first point is position 3, the real ones 4 and 5
	std::vector<B::Member> members = RealHead();
	members.push_back(B::Member::Point(PointType::StateQ));
	members.push_back(B::Member::Point(PointType::RealQ));
	members.push_back(B::Member::Point(PointType::RealQ));
	//01 | 04 3FC00000 00 | 05 C0000000 10: rule 1, point at position 4 (1.5, flags 00), point at position 5 (-2.0, flags 10)
	const Bytes two = {0x01, 0x04, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x05, 0xC0, 0x00, 0x00, 0x00, 0x10};
	REQUIRE(two.size() == 13);
	const auto d = B::Decode(two, members);
	CHECK(d.rule == B::Rule::Offset1);
	REQUIRE(d.items.size() == 2);
	CHECK(d.items[0].position == 4);
	CHECK(d.items[0].value == Pv(PointType::RealQ, 1.5, 0, 0x00));
	CHECK(d.items[1].position == 5);
	CHECK(d.items[1].value == Pv(PointType::RealQ, -2.0, 0, 0x10));
	const std::vector<B::Item> items = {B::Item{4, Pv(PointType::RealQ, 1.5, 0, 0x00)}, B::Item{5, Pv(PointType::RealQ, -2.0, 0, 0x10)}
	};
	CHECK(B::Encode(B::Rule::Offset1, items) == two);
	CHECK(B::Encode(true, items) == two); //the most compact rule is rule 1

	//a data set with Event_Code_Detected as well (four special objects, the 2 octet code at position 3): the first point is position 4
	std::vector<B::Member> withCode = RealHead();
	withCode.push_back(B::Member::Object(tase2::Special::EventCode));
	withCode.push_back(B::Member::Point(PointType::StateQ));
	withCode.push_back(B::Member::Point(PointType::RealQ));
	withCode.push_back(B::Member::Point(PointType::RealQ));
	const Bytes shifted = {0x01, 0x05, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x06, 0xC0, 0x00, 0x00, 0x00, 0x10};
	const auto ds = B::Decode(shifted, withCode);
	REQUIRE(ds.items.size() == 2);
	CHECK(ds.items[0].position == 5);
	CHECK(ds.items[1].position == 6);
	CHECK(ds.items[1].value == Pv(PointType::RealQ, -2.0, 0, 0x10));
	//a pair whose position is the code is read as the 2 octet special object (lenient), not as a point
	const Bytes odd = {0x01, 0x03, 0x00, 0x00, 0x04, 0x90};
	const auto dc = B::Decode(odd, withCode);
	REQUIRE(dc.items.size() == 2);
	CHECK(dc.items[0].special == tase2::Special::EventCode);
	CHECK(dc.items[1].position == 4);
	CHECK(dc.items[1].value == Pv(PointType::StateQ, 0, 0, 0x90));

	//the complete report of that data set, rule 0: 1 + 65 + 4 + 1 + 2 + 1 + 5 + 5 = 84 octets; the event code is 2 octets, nothing is left over
	std::vector<B::Item> all = HeadItems(DC::IntegrityTimeOut);
	B::SpecialValue code;
	code.number = 0;
	all.push_back(B::ObjectItem(3, tase2::Special::EventCode, code));
	all.push_back(B::Item{4, Pv(PointType::StateQ, 0, 0, 0x90)});
	all.push_back(B::Item{5, Pv(PointType::RealQ, 1.5, 0, 0x00)});
	all.push_back(B::Item{6, Pv(PointType::RealQ, -2.0, 0, 0x10)});
	const Bytes whole = B::Encode(false, all);
	CHECK(whole.size() == 1 + 65 + 4 + 1 + 2 + 1 + 5 + 5);
	CHECK(whole[0] == 0);
	CHECK(whole[71] == 0x00); //the code, 2 octets
	CHECK(whole[72] == 0x00);
	CHECK(whole[73] == 0x90); //then the first point, a StateQ
	CHECK(B::Decode(whole, withCode).items.size() == 7);
}

TEST_CASE(SUITE("the storage forms of the special objects: names, time stamp, conditions, event code"))
{
	auto one = [](tase2::Special s, const B::SpecialValue& v)
		     {
			     Bytes out;
			     B::AppendSpecial(out, s, v);
			     CHECK(out.size() == B::SpecialWidth(s));
			     CHECK(B::ReadSpecial(out.data(), out.size(), s) == v);
			     return out;
		     };
	B::SpecialValue v;
	//names: scope octet and two 32 octet fields; a VMD-specific name has scope 0
	v.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"};
	CHECK(one(tase2::Special::TransferSetName, v) == Cat({
		{0x01}, Pad32("DOMAIN_A"), Pad32("TS_POOL_001")}));
	v.transferSet = tase2::ObjectRef{tase2::Scope::Vcc, "", "TS_X"};
	CHECK(one(tase2::Special::TransferSetName, v) == Cat({
		{0x00}, Pad32(""), Pad32("TS_X")}));
	//ids of exactly 32 octets fill their field; one more is refused, not cut
	v.transferSet = tase2::ObjectRef{tase2::Scope::Icc, std::string(32, 'D'), std::string(32, 'T')};
	CHECK(one(tase2::Special::TransferSetName, v).size() == 65);
	Bytes out;
	v.transferSet = tase2::ObjectRef{tase2::Scope::Icc, std::string(33, 'D'), "T"};
	CHECK_THROWS_AS(B::AppendSpecial(out, tase2::Special::TransferSetName, v), std::invalid_argument);
	v.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "D", std::string(33, 'T')};
	CHECK_THROWS_AS(B::AppendSpecial(out, tase2::Special::TransferSetName, v), std::invalid_argument);
	//a field ends at its first zero octet
	Bytes raw = Cat({
		{0x01}, Pad32("AB"), Pad32("CD")});
	raw[4] = 'X'; //after the zero padding began
	CHECK(B::ReadSpecial(raw.data(), raw.size(), tase2::Special::TransferSetName).transferSet == tase2::ObjectRef{tase2::Scope::Icc, "AB", "CD"});
	//time stamp: 4 octets, unsigned, big endian
	B::SpecialValue t;
	t.number = 4294967295LL;
	CHECK(one(tase2::Special::TimeStamp, t) == Bytes{0xFF, 0xFF, 0xFF, 0xFF});
	t.number = 1700000000;
	CHECK(one(tase2::Special::TimeStamp, t) == Bytes{0x65, 0x53, 0xF1, 0x00});
	//conditions: the content octet of the bit string, bit 0 (IntervalTimeOut) the most significant
	B::SpecialValue c;
	c.conditions = DC::IntervalTimeOut;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0x80});
	c.conditions = DC::IntegrityTimeOut;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0x40});
	c.conditions = DC::ObjectChange;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0x20});
	c.conditions = DC::OperatorRequest;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0x10});
	c.conditions = DC::OtherExternalEvent;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0x08});
	c.conditions = 0x1F;
	CHECK(one(tase2::Special::ConditionsDetected, c) == Bytes{0xF8});
	//the octet is the same as the content of the bit string a report carries
	CHECK(tase2::ToData(tase2::DsConditions{DC::IntervalTimeOut, tase2::DsConditionBitCount}).content == Bytes({0x03, 0x80}));
	//event code: 2 octets [V] (the standard types it Integer16; a real peer sent the value 0, 00 00); byte order and sign of other values [S]: big endian, two's complement
	B::SpecialValue e;
	CHECK(B::SpecialWidth(tase2::Special::EventCode) == 2);
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0x00, 0x00});
	e.number = 7;
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0x00, 0x07});
	e.number = 258;
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0x01, 0x02});
	e.number = -2;
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0xFF, 0xFE});
	e.number = -32768;
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0x80, 0x00});
	e.number = 32767;
	CHECK(one(tase2::Special::EventCode, e) == Bytes{0x7F, 0xFF});
	//four octets are no longer an event code
	const uint8_t four[] = {0x00, 0x00, 0x00, 0x07};
	CHECK_THROWS_AS(B::ReadSpecial(four, 4, tase2::Special::EventCode), tase2::DecodeError);
	//a length that is not the width, and a scope octet that is neither 0 nor 1
	const uint8_t two[] = {0x00, 0x01};
	CHECK_THROWS_AS(B::ReadSpecial(two, 2, tase2::Special::TimeStamp), tase2::DecodeError);
	CHECK_THROWS_AS(B::ReadSpecial(two, 0, tase2::Special::ConditionsDetected), tase2::DecodeError);
	Bytes badScope = Cat({
		{0x02}, Pad32("A"), Pad32("B")});
	CHECK_THROWS_AS(B::ReadSpecial(badScope.data(), badScope.size(), tase2::Special::TransferSetName), tase2::DecodeError);
}

TEST_CASE(SUITE("malformed special objects: name storage too short, time stamp truncated, a bad scope octet"))
{
	std::vector<B::Member> members = RealHead();
	members.push_back(B::Member::Point(PointType::RealQ));
	std::vector<B::Item> items = HeadItems();
	items.push_back(B::Item{3, Pv(PointType::RealQ, 1.5, 0, 0x10)});
	const Bytes good = B::Encode(false, items);
	REQUIRE(good.size() == 1 + 65 + 4 + 1 + 5);
	REQUIRE(B::Decode(good, members).items.size() == 4);
	auto why = [&](const Bytes& data)
		     {
			     try
			     {
				     B::Decode(data, members);
			     }
			     catch(const tase2::DecodeError& e)
			     {
				     return std::string(e.what());
			     }
			     return std::string("decoded");
		     };
	CHECK(why(Bytes(good.begin(), good.begin() + 1 + 40)).find("truncated, the value of member 0 needs 65 octets and 40 are left") != std::string::npos);
	CHECK(why(Bytes(good.begin(), good.begin() + 1 + 64)).find("truncated, the value of member 0 needs 65 octets and 64 are left") != std::string::npos);
	CHECK(why(Bytes(good.begin(), good.begin() + 1 + 65 + 2)).find("truncated, the value of member 1 needs 4 octets and 2 are left") != std::string::npos);
	CHECK(why(Bytes(good.begin(), good.begin() + 1 + 65 + 4)).find("truncated, the value of member 2 needs 1 octets and 0 are left") != std::string::npos);
	Bytes scope = good;
	scope[1] = 7;
	CHECK(why(scope).find("scope octet") != std::string::npos);
	//rules 1 and 2 with a special object: a position, then its storage form
	Bytes cut = {0x01, 0x00};
	cut.insert(cut.end(), good.begin() + 2, good.begin() + 2 + 30);
	CHECK(why(cut).find("truncated, the value at position 0 needs 65 octets and 30 are left") != std::string::npos);
	CHECK(why(Bytes{0x01, 0x01, 0x65, 0x53}).find("truncated, the value at position 1 needs 4 octets and 2 are left") != std::string::npos);
	//every strict prefix of a whole block is refused
	for(size_t n = 0; n < good.size(); ++n)
		CHECK_THROWS_AS(B::Decode(Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(n)), members), tase2::DecodeError);
}

TEST_CASE(SUITE("truncated and mutated blocked data only ever decodes or throws DecodeError, never reads past the end"))
{
	std::vector<B::Member> members;
	std::vector<B::Item> all;
	for(const auto& c : Cases())
	{
		all.push_back(B::Item{members.size(), c.value});
		members.push_back(B::Member::Point(c.type));
	}
	const Bytes rule0 = B::Encode(B::Rule::NoRbe, all);
	const Bytes rule1 = B::Encode(B::Rule::Offset1, all);
	const Bytes rule2 = B::Encode(B::Rule::Offset2, all);
	//rule 0: every strict prefix is short; a whole data set is the only length that decodes
	for(size_t n = 0; n < rule0.size(); ++n)
		CHECK_THROWS_AS(B::Decode(Bytes(rule0.begin(), rule0.begin() + static_cast<std::ptrdiff_t>(n)), members), tase2::DecodeError);
	//rules 1 and 2: a prefix is either cut inside an item (an error) or at the end of one (fewer values), and then carries only values that were in the block
	for(const Bytes* block : {&rule1, &rule2})
		for(size_t n = 0; n <= block->size(); ++n)
		{
			try
			{
				const auto d = B::Decode(Bytes(block->begin(), block->begin() + static_cast<std::ptrdiff_t>(n)), members);
				CHECK(d.items.size() <= 12);
				for(const auto& item : d.items)
					CHECK(item.value == all[item.position].value);
			}
			catch(const tase2::DecodeError&)
			{}
		}
	Lcg rng{12345};
	for(int i = 0; i < 4000; ++i)
	{
		Bytes data = (i % 3 == 0) ? rule0 : (i % 3 == 1 ? rule1 : rule2);
		const int changes = 1 + static_cast<int>(rng.Next() % 4);
		for(int c = 0; c < changes; ++c)
			data[rng.Next() % data.size()] = static_cast<uint8_t>(rng.Next());
		if(rng.Next() % 5 == 0)
			data.resize(rng.Next() % (data.size() + 1));
		try
		{
			const auto d = B::Decode(data, members);
			for(const auto& item : d.items)
				CHECK(item.position < members.size());
		}
		catch(const tase2::DecodeError&)
		{}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// recognising a blocked report
//---------------------------------------------------------------------------------------------------------------------

namespace
{
mms::ObjectName Icc(const char* item)
{
	return tase2::IccName("DOMAIN_A", item);
}

//a blocked report as a peer would send it: the special objects, then the octet string (the names and the order are the caller's)
mms::InformationReport MakeBlocked(const Bytes& data, const mms::ObjectName& dataName = mms::ObjectName::Vmd(B::DefaultVariableName), bool dataFirst = false,
	const tase2::ObjectRef& transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"}, bool withName = true)
{
	mms::InformationReport r;
	mms::ListOfVariable list;
	std::vector<mms::AccessResult> results;
	if(withName)
	{
		list.push_back(mms::VariableSpec{Icc(tase2::Name::TransferSetName), std::nullopt});
		results.push_back(mms::AccessResult::Success(tase2::ToData(transferSet)));
	}
	list.push_back(mms::VariableSpec{Icc(tase2::Name::TransferSetTimeStamp), std::nullopt});
	results.push_back(mms::AccessResult::Success(tase2::TransferSetTimeStampData(1700000000)));
	list.push_back(mms::VariableSpec{Icc(tase2::Name::DsConditionsDetected), std::nullopt});
	results.push_back(mms::AccessResult::Success(tase2::ToData(tase2::DsConditions{DC::ObjectChange, tase2::DsConditionBitCount})));
	const auto octets = mms::AccessResult::Success(mms::Data::OctetString(data));
	if(dataFirst)
	{
		list.insert(list.begin(), mms::VariableSpec{dataName, std::nullopt});
		results.insert(results.begin(), octets);
	}
	else
	{
		list.push_back(mms::VariableSpec{dataName, std::nullopt});
		results.push_back(octets);
	}
	r.specification = std::move(list);
	r.results = std::move(results);
	return r;
}
}

TEST_CASE(SUITE("a blocked report is recognised by its shape, whatever the variable is called and wherever it is"))
{
	const Bytes data = {0x01, 0x00, 0x90};
	{
		const auto b = B::RecognizeBlockedReport(MakeBlocked(data));
		REQUIRE(b.has_value());
		CHECK(b->problem.empty());
		CHECK(b->data == data);
		CHECK(b->transferSet == tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"});
		CHECK(b->timeStamp == 1700000000);
		REQUIRE(b->conditionsDetected.has_value());
		CHECK(b->conditionsDetected->mask == DC::ObjectChange);
		CHECK(b->layout == std::vector<tase2::Special>{tase2::Special::TransferSetName, tase2::Special::TimeStamp, tase2::Special::ConditionsDetected});
		CHECK(b->variables == std::vector<std::string>{"DOMAIN_A/Transfer_Set_Name", "DOMAIN_A/Transfer_Set_Time_Stamp", "DOMAIN_A/DSConditions_Detected", "Block_Data"});
		CHECK(b->dataName == mms::ObjectName::Vmd("Block_Data")); //a real peer's variable is VMD-specific [V]
	}
	//another name, another scope, another domain, in front of the special objects
	for(const auto& name : {mms::ObjectName::Vmd("WHATEVER"), tase2::IccName("DOMAIN_Z", "SOME_BLOCK"), Icc("Block_Data_2")})
		for(bool first : {false, true})
		{
			const auto b = B::RecognizeBlockedReport(MakeBlocked(data, name, first));
			REQUIRE(b.has_value());
			CHECK(b->problem.empty());
			CHECK(b->data == data);
			CHECK(b->dataName == name);
		}
	//special objects named in the vmd scope are special objects all the same
	auto vmd = MakeBlocked(data);
	for(auto& v : std::get<mms::ListOfVariable>(vmd.specification))
		if(v.name.itemId != B::DefaultVariableName)
			v.name = mms::ObjectName::Vmd(v.name.itemId);
	CHECK(B::RecognizeBlockedReport(vmd).has_value());
}

TEST_CASE(SUITE("what is not a blocked report is not recognised, and a blocked report that cannot be attributed says why"))
{
	const Bytes data = {0x01, 0x00, 0x90};
	//an ordinary report: the points' values, no octet string
	{
		tase2::TransferReport tr;
		tr.layout = {tase2::Special::TransferSetName};
		tr.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"};
		tr.specification = mms::ListOfVariable{mms::VariableSpec{Icc(tase2::Name::TransferSetName), std::nullopt}, mms::VariableSpec{Icc("POINT_0001_ST"), std::nullopt}
		};
		tr.values.push_back(mms::AccessResult::Success(tase2::ToData(Pv(PointType::StateQ, 0, 0, 0x90))));
		CHECK_FALSE(B::RecognizeBlockedReport(tase2::BuildTransferReport(tr)).has_value());
	}
	//a report that names its data set
	{
		auto r = MakeBlocked(data);
		r.specification = Icc("DS_TEST_001");
		CHECK_FALSE(B::RecognizeBlockedReport(r).has_value());
	}
	//two variables that are not special objects, or none
	{
		auto r = MakeBlocked(data);
		std::get<mms::ListOfVariable>(r.specification).push_back(mms::VariableSpec{Icc("MORE"), std::nullopt});
		r.results.push_back(mms::AccessResult::Success(mms::Data::OctetString(data)));
		CHECK_FALSE(B::RecognizeBlockedReport(r).has_value());
		auto none = MakeBlocked(data);
		std::get<mms::ListOfVariable>(none.specification).pop_back();
		none.results.pop_back();
		CHECK_FALSE(B::RecognizeBlockedReport(none).has_value());
	}
	//the one variable is not an octet string, or failed; the counts differ
	{
		auto r = MakeBlocked(data);
		r.results.back() = mms::AccessResult::Success(tase2::ToData(Pv(PointType::StateQ, 0, 0, 0x90)));
		CHECK_FALSE(B::RecognizeBlockedReport(r).has_value());
		r.results.back() = mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent);
		CHECK_FALSE(B::RecognizeBlockedReport(r).has_value());
		auto fewer = MakeBlocked(data);
		fewer.results.pop_back();
		CHECK_FALSE(B::RecognizeBlockedReport(fewer).has_value());
		mms::InformationReport empty;
		empty.specification = mms::ListOfVariable{};
		CHECK_FALSE(B::RecognizeBlockedReport(empty).has_value());
	}
	//blocked in shape but not usable: no Transfer_Set_Name, a special object that failed or has the wrong type
	{
		const auto noName = B::RecognizeBlockedReport(MakeBlocked(data, Icc("Block_Data"), false, tase2::ObjectRef{}, false));
		REQUIRE(noName.has_value());
		CHECK(noName->problem.find("Transfer_Set_Name") != std::string::npos);
		CHECK(noName->data == data);
		auto failed = MakeBlocked(data);
		failed.results[1] = mms::AccessResult::Failure(mms::DataAccessError::ObjectAccessDenied);
		const auto f = B::RecognizeBlockedReport(failed);
		REQUIRE(f.has_value());
		CHECK_FALSE(f->problem.empty());
		auto wrong = MakeBlocked(data);
		wrong.results[0] = mms::AccessResult::Success(mms::Data::Boolean(true));
		const auto w = B::RecognizeBlockedReport(wrong);
		REQUIRE(w.has_value());
		CHECK_FALSE(w->problem.empty());
		CHECK(w->variables.size() == 4); //the names are there for the diagnostics even so
	}
}

TEST_CASE(SUITE("the hex dump is capped and says how much it left out"))
{
	CHECK(B::HexDump({}) == "");
	CHECK(B::HexDump(Bytes{0x01, 0xAB}) == "01 AB");
	Bytes big(100);
	for(size_t i = 0; i < big.size(); ++i)
		big[i] = static_cast<uint8_t>(i);
	const std::string dump = B::HexDump(big, 64);
	CHECK(dump.substr(0, 11) == "00 01 02 03");
	CHECK(dump.find("3F") != std::string::npos);
	CHECK(dump.find("40 ") == std::string::npos);
	CHECK(dump.find("... (36 more)") != std::string::npos);
	CHECK(B::HexDump(Bytes(64, 0xEE), 64).find("more") == std::string::npos);
	//the diagnostics show the first 96 octets
	CHECK(B::DiagnosticOctets == 96);
	CHECK(B::HexDump(Bytes(200, 0x01)).find("... (104 more)") != std::string::npos);
	CHECK(B::HexDump(Bytes(96, 0x01)).find("more") == std::string::npos);
	CHECK(B::DisplayName(Icc("X")) == "DOMAIN_A/X");
	CHECK(B::DisplayName(mms::ObjectName::Vmd("Y")) == "Y");
}

//---------------------------------------------------------------------------------------------------------------------
// the server side against a scripted client
//---------------------------------------------------------------------------------------------------------------------

namespace
{
const char* const ServerJson = R"({
	"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Type": "RealQTimeTag"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"TransferSetPool": ["TS_POOL_001", "TS_POOL_002"]
	%EXTRA%
})";
const std::string Block3Extra = R"(, "SupportedBlocks": [1, 2, 3])";
//Block 3 with the option off that sends a report needing a position above 255 (rule 2) in the ordinary form
const std::string Block3NoRule2 = Block3Extra + R"(, "BlockedRule2": false)";

ICCPPortConf LoadText(const std::string& text)
{
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

ICCPPortConf LoadServer(const std::string& extra)
{
	std::string text = ServerJson;
	text.replace(text.find("%EXTRA%"), 7, extra);
	return LoadText(text);
}

bool IsSpecialItem(const std::string& item)
{
	return item == tase2::Name::TransferSetName || item == tase2::Name::TransferSetTimeStamp || item == tase2::Name::DsConditionsDetected || item == tase2::Name::EventCodeDetected;
}

//the data set the client defines: the special objects, then the three points (state, then two analogs: the data positions 0, 1 and 2)
std::vector<mms::ObjectName> DefaultMembers(const std::vector<const char*>& specials = {tase2::Name::TransferSetName, tase2::Name::DsConditionsDetected, tase2::Name::TransferSetTimeStamp})
{
	std::vector<mms::ObjectName> m;
	for(const char* s : specials)
		m.push_back(Icc(s));
	for(const char* p : {"POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"})
		m.push_back(Icc(p));
	return m;
}

struct Params
{
	int64_t bufferTime = 0;
	int64_t interval = 0;
	int64_t integrityCheck = 0;
	int64_t tle = 0;
	uint8_t conditions = DC::ObjectChange;
	bool rbe = false;
	bool allChanges = false;
	bool blockData = true;
	bool critical = false;
};

//what a client sees of a blocked report: the members the octet string holds, the special objects included
struct View
{
	B::BlockedReport report;
	B::Decoded decoded;
	//the positions of every member in the block, special objects and points
	std::vector<size_t> Positions() const
	{
		std::vector<size_t> out;
		for(const auto& i : decoded.items)
			out.push_back(i.position);
		return out;
	}
	//the positions of the points only
	std::vector<size_t> PointPositions() const
	{
		std::vector<size_t> out;
		for(const auto& i : decoded.items)
			if(!i.special)
				out.push_back(i.position);
		return out;
	}
	//the values of the points only
	std::vector<double> Numbers() const
	{
		std::vector<double> out;
		for(const auto& i : decoded.items)
			if(!i.special)
				out.push_back(Num(i.value));
		return out;
	}
	//the special object in the block, if it is there
	const B::Item* Object(tase2::Special s) const
	{
		for(const auto& i : decoded.items)
			if(i.special == s)
				return &i;
		return nullptr;
	}
};

struct Fx
{
	stacktest::Link link;
	ICCPPortConf conf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::vector<std::pair<LogLevel, std::string>> logs;
	std::vector<mms::ObjectName> members;
	std::vector<bool> routed;
	std::string transferSetName = "TS_POOL_001";

	explicit Fx(ICCPPortConf c, std::vector<mms::ObjectName> m = DefaultMembers(), size_t maxTsdu = 0):
		link(maxTsdu == 0 ? std::function<void(stacktest::Tweak&, stacktest::Tweak&)>() : [maxTsdu](stacktest::Tweak& a, stacktest::Tweak& b)
		{
			a.config.maxTsduSize = maxTsdu;
			b.config.maxTsduSize = maxTsdu;
		}),
		conf(std::move(c)),
		members(std::move(m))
	{
		model = std::make_unique<tase2::ResponderModel>(conf, true);
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = []() { return Wall; };
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, tb, [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); });
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
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
		def.name = Icc("DS_TEST_001");
		def.members = members;
		REQUIRE(Request(tase2::BuildCreateDataSet(def)).Get<mms::DefineNamedVariableListResponse>() != nullptr);
	}
	void WriteTransferSet(const Params& p, bool status)
	{
		tase2::DsTransferSet t;
		t.dataSetName = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "DS_TEST_001"};
		t.bufferTime = p.bufferTime;
		t.interval = p.interval;
		t.integrityCheck = p.integrityCheck;
		t.tle = p.tle;
		t.conditionsRequested = tase2::DsConditions{p.conditions, tase2::DsConditionBitCount};
		t.rbe = p.rbe;
		t.blockData = p.blockData;
		t.critical = p.critical;
		t.allChangesMember = true;
		t.allChangesReported = p.allChanges;
		t.status = status;
		const auto o = Request(tase2::BuildStartTransfer(Icc(transferSetName.c_str()), t));
		const auto* wr = o.Get<mms::WriteResponse>();
		REQUIRE(wr != nullptr);
		REQUIRE(wr->results.size() == 1);
		REQUIRE(wr->results[0].success);
	}
	//define the data set and start the transfer set; the report the start causes (an integrity report), if any, stays in link.a.reports
	void StartKeep(const Params& p = Params())
	{
		DefineDataSet();
		WriteTransferSet(p, true);
		Advance(50);
	}
	//the same, and whatever the start sent is dropped
	void Start(const Params& p = Params())
	{
		StartKeep(p);
		link.a.reports.clear();
	}
	void Route()
	{
		auto reports = std::move(link.b.reports);
		link.b.reports.clear();
		for(const auto& r : reports)
			routed.push_back(server->OnInformationReport(r));
	}
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		link.Pump();
		Route();
	}
	static odc::msSinceEpoch_t Ts(int seconds)
	{
		return Wall + 1000 * seconds;
	}
	//a value change of a point, as the port does it: the model first, then the session
	void Analog(size_t index, double value, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Analog, index, false, value, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		link.Pump();
	}
	void State(bool on, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Binary, 0, on, 0, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		link.Pump();
	}
	void Binary(size_t index, bool on, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Binary, index, on, 0, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		link.Pump();
	}

	//the types of the data members of the data set (the special objects at its head are not data)
	//the members of the data set as the layout sees them: the special objects at its head (a data set that does not start with Transfer_Set_Name has none), then the points
	std::vector<B::Member> Members() const
	{
		std::vector<B::Member> out;
		bool head = !members.empty() && members[0].itemId == tase2::Name::TransferSetName;
		for(const auto& m : members)
		{
			if(head && IsSpecialItem(m.itemId))
			{
				out.push_back(B::Member::Object(m.itemId == tase2::Name::TransferSetName ? tase2::Special::TransferSetName :
					m.itemId == tase2::Name::TransferSetTimeStamp ? tase2::Special::TimeStamp :
					m.itemId == tase2::Name::DsConditionsDetected ? tase2::Special::ConditionsDetected : tase2::Special::EventCode));
				continue;
			}
			head = false;
			const auto idx = model->FindPoint(m);
			REQUIRE(idx.has_value());
			out.push_back(B::Member::Point(model->Point(*idx).type));
		}
		return out;
	}
	View Read(const mms::InformationReport& ir) const
	{
		const auto b = B::RecognizeBlockedReport(ir);
		REQUIRE(b.has_value());
		REQUIRE(b->problem.empty());
		View v;
		v.report = *b;
		v.decoded = B::Decode(b->data, Members());
		return v;
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

std::vector<std::string> VariableNames(const mms::InformationReport& ir)
{
	std::vector<std::string> out;
	if(const auto* list = std::get_if<mms::ListOfVariable>(&ir.specification))
		for(const auto& v : *list)
			out.push_back(B::DisplayName(v.name));
	else
		out.push_back("data set " + B::DisplayName(std::get<mms::ObjectName>(ir.specification)));
	return out;
}
}

TEST_CASE(SUITE("server, RBE false: the complete report is rule 0, the special objects separate variables, the octet string last"))
{
	Fx f(LoadServer(Block3Extra));
	f.State(true, Fx::Ts(1));
	f.Analog(0, 11.5, Fx::Ts(2));
	f.Analog(1, 12.5, Fx::Ts(3));
	Params p;
	p.conditions = DC::IntegrityTimeOut;
	p.integrityCheck = 10;
	f.StartKeep(p);
	REQUIRE(f.link.a.reports.size() == 1);
	const auto& ir = f.link.a.reports[0];
	CHECK(VariableNames(ir) == std::vector<std::string>{"DOMAIN_A/Transfer_Set_Name", "DOMAIN_A/DSConditions_Detected", "DOMAIN_A/Transfer_Set_Time_Stamp", "Block_Data"});
	//the blocked variable is VMD-specific, as a real peer's is [V]; the special objects are domain-specific, in the domain of the transfer set
	const auto& list = std::get<mms::ListOfVariable>(ir.specification);
	REQUIRE(list.size() == 4);
	CHECK(list[3].name == mms::ObjectName::Vmd("Block_Data"));
	CHECK(list[3].name.scope == mms::ObjectScope::VmdSpecific);
	CHECK(list[0].name == Icc(tase2::Name::TransferSetName));
	REQUIRE(ir.results.size() == 4);
	CHECK(ir.results[3].data.type == mms::DataType::OctetString);
	const View v = f.Read(ir);
	CHECK(v.decoded.rule == B::Rule::NoRbe);
	//every member of the data set, the special objects included, in data set order [V]
	CHECK(v.Positions() == std::vector<size_t>{0, 1, 2, 3, 4, 5});
	CHECK(v.PointPositions() == std::vector<size_t>{3, 4, 5});
	CHECK(v.Numbers() == std::vector<double>{1, 11.5, 12.5});
	REQUIRE(v.Object(tase2::Special::TransferSetName) != nullptr);
	CHECK(v.Object(tase2::Special::TransferSetName)->object.transferSet == tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_001"});
	CHECK(v.Object(tase2::Special::ConditionsDetected)->object.conditions == DC::IntegrityTimeOut);
	CHECK(v.Object(tase2::Special::TimeStamp)->object.number == WallSeconds);
	CHECK(*v.decoded.items[3].value.timeStamp == 1700000001);
	CHECK(*v.decoded.items[4].value.timeStamp == 1700000002);
	CHECK(*v.decoded.items[5].value.timeStamp == 1700000003);
	//the separate variables carry the same values
	CHECK(v.report.conditionsDetected->mask == DC::IntegrityTimeOut);
	CHECK(v.report.timeStamp == WallSeconds);
	CHECK(v.report.transferSet.name == "TS_POOL_001");
	//one octet string: the rule octet, 65 + 1 + 4 for the special objects, then 5 + 9 + 9 (the state point is StateQTimeTag, the analogs RealQTimeTag)
	CHECK(v.report.data.size() == 1 + 65 + 1 + 4 + 5 + 9 + 9);
	CHECK(v.report.data[0] == 0);
	CHECK(v.report.data[1] == 1); //the scope of the name: domain-specific
	CHECK(f.server->ReportsSent() == 1);
	CHECK(f.server->ReportCounters().blockedSent == 1);
	CHECK(f.Count("BlockData is honoured", LogLevel::Info) == 1);
	f.Advance(10000);
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.Read(f.link.a.reports[1]).decoded.rule == B::Rule::NoRbe);
	CHECK(f.server->ReportCounters().blockedSent == 2);
}

TEST_CASE(SUITE("server, RBE true: a change is rule 1 [V], only the changed point as (position, value), no special objects, the position counting them"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	f.Start(p);
	f.Analog(1, 12.5, Fx::Ts(5));
	REQUIRE(f.link.a.reports.size() == 1);
	const View v = f.Read(f.link.a.reports[0]);
	CHECK(v.decoded.rule == B::Rule::Offset1);
	CHECK(v.Positions() == std::vector<size_t>{5}); //POINT_0002_MW is the sixth member, the special objects counted; none of them is in the string
	CHECK(v.PointPositions() == std::vector<size_t>{5});
	CHECK(v.Numbers() == std::vector<double>{12.5});
	CHECK(v.Object(tase2::Special::ConditionsDetected) == nullptr);
	//the special objects are separate variables of the report all the same [V]
	CHECK(v.report.conditionsDetected->mask == DC::ObjectChange);
	CHECK(v.report.timeStamp == WallSeconds);
	CHECK(VariableNames(f.link.a.reports[0]) == std::vector<std::string>{"DOMAIN_A/Transfer_Set_Name", "DOMAIN_A/DSConditions_Detected", "DOMAIN_A/Transfer_Set_Time_Stamp", "Block_Data"});
	const Bytes octets = f.link.a.reports[0].results.back().data.AsOctetString();
	REQUIRE(octets.size() == 1 + 1 + 9); //the rule, a 1 octet position, a RealQTimeTag
	CHECK(octets[0] == 1);
	CHECK(octets[1] == 5);
	CHECK(f.server->ReportCounters().blockedSent == 1);
	CHECK(f.Count("blocked", LogLevel::Warn) == 0); //no warning: RBE is blocked now
	//the points that did not change are not in it
	f.State(true, Fx::Ts(6));
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.Read(f.link.a.reports[1]).PointPositions() == std::vector<size_t>{3});
}

TEST_CASE(SUITE("server, buffer time: the changes of the period are one rule 1 report, the points in order of the data set"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	p.bufferTime = 2;
	f.Start(p);
	f.Analog(1, 22, Fx::Ts(1));
	f.Analog(0, 21, Fx::Ts(2));
	f.State(true, Fx::Ts(3));
	f.Advance(1900);
	CHECK(f.link.a.reports.empty());
	f.Advance(200);
	REQUIRE(f.link.a.reports.size() == 1);
	const View v = f.Read(f.link.a.reports[0]);
	CHECK(v.decoded.rule == B::Rule::Offset1);
	CHECK(v.Positions() == std::vector<size_t>{3, 4, 5}); //several points: repeated pairs, ascending [V]
	CHECK(v.Numbers() == std::vector<double>{1, 21, 22});
	CHECK(v.report.conditionsDetected->mask == DC::ObjectChange);
	CHECK(v.report.data.size() == 1 + 3 + 5 + 9 + 9); //the rule, three 1 octet positions, a StateQTimeTag and two RealQTimeTags
}

TEST_CASE(SUITE("server, interval: rule 0 for the complete report, only what changed (rule 1) with RBE, nothing when nothing changed"))
{
	{
		Fx f(LoadServer(Block3Extra));
		f.Analog(0, 1, Fx::Ts(1));
		Params p;
		p.conditions = DC::IntervalTimeOut;
		p.interval = 5;
		f.Start(p);
		f.Advance(5000);
		REQUIRE(f.link.a.reports.size() == 1);
		const View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::NoRbe);
		CHECK(v.Positions() == std::vector<size_t>{0, 1, 2, 3, 4, 5});
		CHECK(v.report.conditionsDetected->mask == DC::IntervalTimeOut);
		CHECK(v.Object(tase2::Special::ConditionsDetected)->object.conditions == DC::IntervalTimeOut);
	}
	{
		Fx f(LoadServer(Block3Extra));
		Params p;
		p.conditions = DC::IntervalTimeOut;
		p.interval = 5;
		p.rbe = true;
		f.Start(p);
		f.Analog(1, 7, Fx::Ts(1));
		CHECK(f.link.a.reports.empty()); //the change waits for the interval
		f.Advance(5000);
		REQUIRE(f.link.a.reports.size() == 1);
		const View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::Offset1);
		CHECK(v.Positions() == std::vector<size_t>{5});
		CHECK(v.report.conditionsDetected->mask == DC::IntervalTimeOut);
		f.Advance(5000);
		CHECK(f.link.a.reports.size() == 1); //an interval with no change sends nothing
	}
}

TEST_CASE(SUITE("server, integrity: the complete report of a transfer set with RBE is rule 0 [V], every member including the special objects"))
{
	Fx f(LoadServer(Block3Extra));
	f.State(false, Fx::Ts(1));
	f.Analog(0, 3, Fx::Ts(2));
	f.Analog(1, 4, Fx::Ts(3));
	Params p;
	p.conditions = DC::IntegrityTimeOut;
	p.integrityCheck = 10;
	p.rbe = true;
	f.StartKeep(p);
	REQUIRE(f.link.a.reports.size() == 1);
	const View v = f.Read(f.link.a.reports[0]);
	CHECK(v.decoded.rule == B::Rule::NoRbe);
	CHECK(v.Positions() == std::vector<size_t>{0, 1, 2, 3, 4, 5});
	CHECK(v.Numbers() == std::vector<double>{0, 3, 4});
	CHECK(v.report.conditionsDetected->mask == DC::IntegrityTimeOut);
	CHECK(v.report.data[0] == 0);
	CHECK(v.report.data.size() == 1 + 65 + 1 + 4 + 5 + 9 + 9);
	f.Advance(10000);
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.Read(f.link.a.reports[1]).decoded.rule == B::Rule::NoRbe);
	//an operator request is a complete report too
	CHECK(f.server->RaiseCondition(DC::IntegrityTimeOut, 0) == 1);
	f.link.Pump();
	REQUIRE(f.link.a.reports.size() == 3);
	CHECK(f.Read(f.link.a.reports[2]).decoded.rule == B::Rule::NoRbe);
}

TEST_CASE(SUITE("server, a data set with Event_Code_Detected is reported in the blocked form [V]: rule 0, the 2 octet event code after the conditions, then the points; no warning"))
{
	const auto members = DefaultMembers({tase2::Name::TransferSetName, tase2::Name::TransferSetTimeStamp, tase2::Name::DsConditionsDetected, tase2::Name::EventCodeDetected});
	for(const std::string& extra : {Block3Extra, Block3NoRule2})
	{
		Fx f(LoadServer(extra), members);
		f.State(true, Fx::Ts(2));
		f.Analog(1, 4, Fx::Ts(3));
		Params p;
		p.conditions = static_cast<uint8_t>(DC::OperatorRequest | DC::OtherExternalEvent);
		f.Start(p);
		CHECK(f.server->RaiseCondition(DC::OtherExternalEvent, 7) == 1);
		CHECK(f.server->RaiseCondition(DC::OperatorRequest, 0) == 1);
		f.link.Pump();
		REQUIRE(f.link.a.reports.size() == 2);
		CHECK(f.Count("BlockData is true but", LogLevel::Warn) == 0);
		CHECK(f.server->ReportCounters().blockedSent == 2);
		const std::vector<std::string> names = {"DOMAIN_A/Transfer_Set_Name", "DOMAIN_A/Transfer_Set_Time_Stamp", "DOMAIN_A/DSConditions_Detected", "DOMAIN_A/Event_Code_Detected", "Block_Data"};
		CHECK(VariableNames(f.link.a.reports[0]) == names);
		const View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::NoRbe);
		//every member in data set order: the four special objects, then the points, whose positions count all four
		CHECK(v.Positions() == std::vector<size_t>{0, 1, 2, 3, 4, 5, 6});
		CHECK(v.PointPositions() == std::vector<size_t>{4, 5, 6});
		CHECK(v.Numbers() == std::vector<double>{1, 0, 4});
		REQUIRE(v.Object(tase2::Special::EventCode) != nullptr);
		CHECK(v.Object(tase2::Special::EventCode)->object.number == 7);
		CHECK(v.Object(tase2::Special::ConditionsDetected)->object.conditions == DC::OtherExternalEvent);
		CHECK(v.report.eventCode == 7); //the separate variable says the same
		//the octets: the rule, 65 + 4 + 1 + 2 for the special objects, then StateQTimeTag 5, RealQTimeTag 9, RealQTimeTag 9
		const Bytes& octets = v.report.data;
		REQUIRE(octets.size() == 1 + 65 + 4 + 1 + 2 + 5 + 9 + 9);
		CHECK(octets[0] == 0);
		CHECK(octets[70] == 0x08); //DSConditions_Detected: OtherExternalEvent is bit 3, the fourth most significant
		CHECK(octets[71] == 0x00); //the event code: 2 octets, big endian
		CHECK(octets[72] == 0x07);
		CHECK(f.Read(f.link.a.reports[1]).Object(tase2::Special::EventCode)->object.number == 0);
	}
}

TEST_CASE(SUITE("server, RBE true, a data set with Event_Code_Detected: partial reports are rule 1 with positions counting all four special objects, several points as several pairs [V]"))
{
	const auto members = DefaultMembers({tase2::Name::TransferSetName, tase2::Name::TransferSetTimeStamp, tase2::Name::DsConditionsDetected, tase2::Name::EventCodeDetected});
	Fx f(LoadServer(Block3Extra), members);
	Params p;
	p.rbe = true;
	p.bufferTime = 2;
	f.Start(p);
	//one change: rule 1, the first analog is the sixth member (position 5), no special object in the string
	f.Analog(0, 21.5, Fx::Ts(1));
	f.Advance(2100);
	REQUIRE(f.link.a.reports.size() == 1);
	const Bytes one = f.link.a.reports[0].results.back().data.AsOctetString();
	REQUIRE(one.size() == 1 + 1 + 9);
	CHECK(one[0] == 1);
	CHECK(one[1] == 5);
	CHECK(f.Read(f.link.a.reports[0]).PointPositions() == std::vector<size_t>{5});
	//three changes in one buffer time: one rule 1 report, three (position, value) pairs in ascending order of position
	f.link.a.reports.clear();
	f.Analog(1, 22.5, Fx::Ts(3));
	f.Analog(0, 23.5, Fx::Ts(4));
	f.State(true, Fx::Ts(5));
	f.Advance(2100);
	REQUIRE(f.link.a.reports.size() == 1);
	const Bytes three = f.link.a.reports[0].results.back().data.AsOctetString();
	REQUIRE(three.size() == 1 + (1 + 5) + (1 + 9) + (1 + 9));
	CHECK(three[0] == 1);
	CHECK(three[1] == 4);  //the StateQTimeTag
	CHECK(three[7] == 5);  //the first analog
	CHECK(three[17] == 6); //the second analog
	const View v = f.Read(f.link.a.reports[0]);
	CHECK(v.decoded.rule == B::Rule::Offset1);
	CHECK(v.Positions() == std::vector<size_t>{4, 5, 6});
	CHECK(v.Numbers() == std::vector<double>{1, 23.5, 22.5});
	CHECK(v.Object(tase2::Special::EventCode) == nullptr);
	CHECK(f.Count("BlockData is true but", LogLevel::Warn) == 0);
}

TEST_CASE(SUITE("server, a data set without special objects at its head: the default two are listed as variables only, the octet string holds the members"))
{
	Fx f(LoadServer(Block3Extra), DefaultMembers({}));
	f.Analog(0, 5, Fx::Ts(1));
	Params p;
	p.conditions = DC::OperatorRequest;
	f.Start(p);
	f.server->RaiseCondition(DC::OperatorRequest, 0);
	f.link.Pump();
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK(VariableNames(f.link.a.reports[0]) == std::vector<std::string>{"DOMAIN_A/Transfer_Set_Name", "DOMAIN_A/DSConditions_Detected", "Block_Data"});
	const View v = f.Read(f.link.a.reports[0]);
	CHECK(v.Positions() == std::vector<size_t>{0, 1, 2});
	CHECK(v.PointPositions() == std::vector<size_t>{0, 1, 2});
	CHECK(v.Numbers()[1] == 5);
}

TEST_CASE(SUITE("server, AllChangesReported: the consecutive reports are each blocked (rule 1), in the order of the changes"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	p.allChanges = true;
	p.bufferTime = 2;
	f.Start(p);
	//A = POINT_0001_MW (position 1), B = POINT_0002_MW (position 2): A1 B1 A2 A3 B2 are the reports [A1 B1] [A2] [A3 B2]
	f.Analog(0, 11, Fx::Ts(1));
	f.Analog(1, 21, Fx::Ts(2));
	f.Analog(0, 12, Fx::Ts(3));
	f.Analog(0, 13, Fx::Ts(4));
	f.Analog(1, 22, Fx::Ts(5));
	f.Advance(2100);
	REQUIRE(f.link.a.reports.size() == 3);
	const View r1 = f.Read(f.link.a.reports[0]);
	const View r2 = f.Read(f.link.a.reports[1]);
	const View r3 = f.Read(f.link.a.reports[2]);
	CHECK(r1.PointPositions() == std::vector<size_t>{4, 5});
	CHECK(r1.Numbers() == std::vector<double>{11, 21});
	CHECK(r2.PointPositions() == std::vector<size_t>{4});
	CHECK(r2.Numbers() == std::vector<double>{12});
	CHECK(r3.PointPositions() == std::vector<size_t>{4, 5});
	CHECK(r3.Numbers() == std::vector<double>{13, 22});
	//each report is a rule 1 string of its own, with no special objects in it: all three are blocked
	CHECK(r2.Positions() == std::vector<size_t>{4});
	//each value has the time stamp of its own change
	CHECK(*r1.decoded.items[0].value.timeStamp == 1700000001);
	CHECK(*r2.decoded.items[0].value.timeStamp == 1700000003);
	CHECK(*r3.decoded.items[1].value.timeStamp == 1700000005);
	CHECK(f.server->ReportCounters().blockedSent == 3);
	CHECK(f.server->ReportsSent() == 3);
	//B then A in one buffer period: the order of the changes (and not of the data set)
	f.link.a.reports.clear();
	f.Analog(1, 23, Fx::Ts(6));
	f.Analog(0, 14, Fx::Ts(7));
	f.Advance(2100);
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK(f.Read(f.link.a.reports[0]).PointPositions() == std::vector<size_t>{5, 4});
}

TEST_CASE(SUITE("server, Critical: the time stamp of the blocked report is what the acknowledgement echoes"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	p.critical = true;
	f.Start(p);
	f.Analog(1, 12.5, Fx::Ts(1));
	REQUIRE(f.link.a.reports.size() == 1);
	const View v = f.Read(f.link.a.reports[0]);
	REQUIRE(v.report.timeStamp.has_value());
	CHECK(f.server->OutstandingCriticalReports() == 1);
	CHECK(f.server->ReportCounters().criticalSent == 1);
	CHECK(f.server->ReportCounters().blockedSent == 1);
	//a wrong time stamp clears nothing
	REQUIRE(f.link.a.conn.SendReport(tase2::BuildTransferReportAck(v.report.transferSet, *v.report.timeStamp + 5)));
	f.link.Pump();
	f.Route();
	CHECK(f.server->OutstandingCriticalReports() == 1);
	REQUIRE(f.link.a.conn.SendReport(tase2::BuildTransferReportAck(v.report.transferSet, v.report.timeStamp)));
	f.link.Pump();
	f.Route();
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().acknowledged == 1);
	CHECK(f.server->ReportCounters().dataFailures == 0);
	//a blocked report that is not acknowledged times out like any other (ReportAckTimeoutms, 10 s)
	f.Analog(1, 13.5, Fx::Ts(2));
	CHECK(f.server->OutstandingCriticalReports() == 1);
	f.Advance(10100);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().ackTimeouts == 1);
	CHECK(f.server->ReportCounters().dataFailures == 1);
	CHECK(f.Count("Data_failure", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("server, TLE: a blocked report that is late is discarded and not counted as sent, the next one is sent"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	p.bufferTime = 1;
	p.tle = 1;
	f.Start(p);
	f.Analog(0, 5, Fx::Ts(1));
	f.Advance(3000); //the buffer time ended 2 s before the tick
	CHECK(f.link.a.reports.empty());
	CHECK(f.server->ReportCounters().tleDiscarded == 1);
	CHECK(f.server->ReportCounters().blockedSent == 0);
	f.Analog(0, 6, Fx::Ts(2));
	f.Advance(1500);
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK(f.Read(f.link.a.reports[0]).Numbers() == std::vector<double>{6});
	CHECK(f.server->ReportCounters().blockedSent == 1);
}

namespace
{
//the same session with BlockData asked for or not; what the client received
std::vector<mms::InformationReport> Scenario(const std::string& extra, bool blockData, bool rbe = true)
{
	Fx f(LoadServer(extra));
	f.State(true, Fx::Ts(1));
	f.Analog(0, 11.5, Fx::Ts(2));
	f.Analog(1, 12.5, Fx::Ts(3));
	Params p;
	p.rbe = rbe;
	p.conditions = static_cast<uint8_t>(DC::ObjectChange | DC::IntegrityTimeOut | DC::OperatorRequest);
	p.integrityCheck = 10;
	p.bufferTime = 2;
	p.blockData = blockData;
	f.DefineDataSet();
	f.WriteTransferSet(p, true);
	f.Advance(50);
	f.Analog(1, 13.5, Fx::Ts(4));
	f.Advance(2100);
	f.server->RaiseCondition(DC::OperatorRequest, 0);
	f.link.Pump();
	f.Advance(10000);
	//blocked: Block 3 advertised and BlockData asked for (the rule is 0 for a complete report, 1 for a partial one of an RBE set)
	const bool expectBlocked = blockData && !extra.empty();
	REQUIRE(f.server->ReportCounters().blockedSent == (expectBlocked ? f.server->ReportsSent() : 0));
	return f.link.a.reports;
}
}

TEST_CASE(SUITE("server, without 3 in SupportedBlocks BlockData is accepted and ignored: the reports are the ordinary ones, unchanged"))
{
	//the default SupportedBlocks is [1, 2]
	const auto plain = Scenario("", false);
	const auto ignored = Scenario("", true);
	REQUIRE(plain.size() >= 4);
	CHECK(ignored == plain);
	//advertising Block 3 changes nothing for a transfer set that did not ask for BlockData
	CHECK(Scenario(Block3Extra, false) == plain);
	//and the ordinary reports have no blocked variable
	for(const auto& ir : plain)
		for(const auto& n : VariableNames(ir))
			CHECK(n.find(B::DefaultVariableName) == std::string::npos);
	//with Block 3 and BlockData the same RBE session is blocked: rule 0 for the complete reports, rule 1 for the partial ones
	const auto blockedReports = Scenario(Block3Extra, true);
	REQUIRE(blockedReports.size() == plain.size());
	std::vector<int> rules;
	for(const auto& ir : blockedReports)
	{
		REQUIRE(B::RecognizeBlockedReport(ir).has_value());
		rules.push_back(ir.results.back().data.AsOctetString().at(0));
	}
	CHECK(blockedReports != plain);
	CHECK(rules == std::vector<int>{0, 1, 0, 0}); //start-up, the change, the operator request, the integrity report
	//BlockedRule2 false does not change that: it only decides what a report with a position above 255 is
	CHECK(Scenario(Block3NoRule2, true) == blockedReports);
	//without RBE (rule 0) Block 3 and BlockData send the blocked reports, which differ from the ordinary ones
	const auto plain0 = Scenario("", false, false);
	const auto rule0 = Scenario(Block3Extra, true, false);
	REQUIRE(rule0.size() == plain0.size());
	CHECK(Scenario("", true, false) == plain0);
	for(const auto& ir : rule0)
		CHECK(B::RecognizeBlockedReport(ir).has_value());
	//what is advertised
	const auto without = LoadServer("");
	const auto with = LoadServer(Block3Extra);
	CHECK_FALSE(tase2::HasBlock(without.LocalAssociation().features, tase2::Block::Block3));
	CHECK(tase2::HasBlock(with.LocalAssociation().features, tase2::Block::Block3));
	{
		Fx f(LoadServer(""));
		Params p;
		p.rbe = true;
		f.Start(p);
		CHECK(f.Count("BlockData is accepted and not acted on", LogLevel::Info) == 1);
		CHECK(f.Count("BlockData is honoured", LogLevel::Info) == 0);
		f.Analog(1, 1, Fx::Ts(1));
		CHECK(f.server->ReportCounters().blockedSent == 0);
	}
}

TEST_CASE(SUITE("server, RBE true with BlockData: complete reports are rule 0, partial reports rule 1, no warning"))
{
	Fx f(LoadServer(Block3Extra));
	Params p;
	p.rbe = true;
	f.DefineDataSet();
	f.WriteTransferSet(p, true);
	CHECK(f.Count("BlockData is honoured", LogLevel::Info) == 1);
	CHECK(f.Count("BlockData is true but", LogLevel::Warn) == 0);
	f.Advance(50);
	f.link.a.reports.clear();
	f.Analog(1, 12.5, Fx::Ts(5));
	f.State(true, Fx::Ts(6));
	REQUIRE(f.link.a.reports.size() == 2);
	for(const auto& ir : f.link.a.reports)
	{
		CHECK(B::RecognizeBlockedReport(ir).has_value());
		CHECK(VariableNames(ir).size() == 4); //the three special objects and the VMD-specific blocked variable
	}
	CHECK(f.server->ReportCounters().blockedSent == 2);
	//BlockedRule2 is about positions above 255 only: no warning in the configuration either way, the behaviour here is the same
	Fx g(LoadServer(Block3NoRule2));
	CHECK(g.conf.warnings.empty());
	CHECK_FALSE(g.conf.blockedRule2);
	CHECK(f.conf.blockedRule2);
}

TEST_CASE(SUITE("server, the item id of the blocked variable is an option; the variable is VMD-specific whatever the domain"))
{
	{
		Fx f(LoadServer(Block3Extra + R"(, "BlockedDataName": "SOME_BLOCK")"));
		CHECK(f.conf.blockedDataName == "SOME_BLOCK");
		Params p;
		p.rbe = true;
		f.Start(p);
		f.Analog(1, 12.5, Fx::Ts(1));
		REQUIRE(f.link.a.reports.size() == 1);
		const auto names = VariableNames(f.link.a.reports[0]);
		CHECK(names.back() == "SOME_BLOCK");
		CHECK(f.Read(f.link.a.reports[0]).report.dataName == mms::ObjectName::Vmd("SOME_BLOCK"));
	}
	{
		Fx f(LoadServer(Block3Extra));
		CHECK(f.conf.blockedDataName == "Block_Data");
		CHECK(std::string(B::DefaultVariableName) == "Block_Data");
	}
}

TEST_CASE(SUITE("server, a data set with a member that is not a point is reported in the ordinary form, with one warning"))
{
	Fx f(LoadServer(R"(, "SupportedBlocks": [1, 2, 3, 5], "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Taggable": true}])"));
	f.members.push_back(Icc("DEV_CMD_01_TAG"));
	Params p;
	p.conditions = DC::IntegrityTimeOut;
	p.integrityCheck = 10;
	f.StartKeep(p);
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK(f.link.a.reports[0].specification == mms::VariableAccessSpecification(Icc("DS_TEST_001"))); //the ordinary complete report names the data set
	CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[0]).has_value());
	f.Advance(10000);
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.Count("is not a point this port serves", LogLevel::Warn) == 1);
	CHECK(f.server->ReportCounters().blockedSent == 0);
}

namespace
{
//300 state points: the positions of a data set of 302 members (the special objects, then PT_0000 to PT_0299) need two octets
ICCPPortConf BigConf(bool rule2, size_t points = 300)
{
	std::string text = std::string(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BilateralTableID": "BT_TEST_A", "SupportedBlocks": [1, 2, 3], "MaxDataSetMembers": 70000, )") + (rule2 ? "" : R"("BlockedRule2": false, )") +
	                   R"("TransferSetPool": ["TS_POOL_001"], "Binaries": [)";
	for(size_t i = 0; i < points; ++i)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "PT_%05d", static_cast<int>(i));
		text += std::string(i == 0 ? "" : ", ") + "{\"Index\": " + std::to_string(i) + ", \"Name\": \"" + name + "\", \"Type\": \"StateQ\"}";
	}
	text += "]}";
	return LoadText(text);
}

std::vector<mms::ObjectName> BigMembers(size_t points = 300)
{
	std::vector<mms::ObjectName> m = {Icc(tase2::Name::TransferSetName), Icc(tase2::Name::DsConditionsDetected)};
	for(size_t i = 0; i < points; ++i)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "PT_%05d", static_cast<int>(i));
		m.push_back(Icc(name));
	}
	return m;
}
}

TEST_CASE(SUITE("server, a data set of more than 256 members: rule 2 for a report with a position above 255 (BlockedRule2, default true); ordinary with one warning per start when it is false"))
{
	{
		//the data set is the two special objects (positions 0 and 1) and PT_0000 to PT_0299 (positions 2 to 301): the special objects are counted
		Fx f(BigConf(false), BigMembers());
		Params p;
		p.rbe = true;
		f.Start(p);
		f.Binary(299, true, Fx::Ts(1));
		REQUIRE(f.link.a.reports.size() == 1);
		CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[0]).has_value()); //the ordinary form
		CHECK(f.Count("needs a position above 255", LogLevel::Warn) == 1);
		CHECK(f.server->ReportCounters().blockedSent == 0);
		f.Binary(3, true, Fx::Ts(2));
		REQUIRE(f.link.a.reports.size() == 2);
		View v = f.Read(f.link.a.reports[1]);
		CHECK(v.decoded.rule == B::Rule::Offset1); //the data set is large, but this report is not
		CHECK(v.PointPositions() == std::vector<size_t>{5});
		f.Binary(253, true, Fx::Ts(3)); //position 255 is the last that fits one octet
		REQUIRE(f.link.a.reports.size() == 3);
		CHECK(f.Read(f.link.a.reports[2]).decoded.rule == B::Rule::Offset1);
		f.Binary(254, true, Fx::Ts(4)); //position 256
		REQUIRE(f.link.a.reports.size() == 4);
		CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[3]).has_value());
		CHECK(f.Count("needs a position above 255", LogLevel::Warn) == 1); //one per start, not one per report
		CHECK(f.server->ReportCounters().blockedSent == 2);
		CHECK(f.server->ReportsSent() == 4);
	}
	{
		//BlockedRule2 on (the default): a report with a position above 255 is rule 2, the positions of the whole report 2 octets
		Fx f(BigConf(true), BigMembers());
		Params p;
		p.rbe = true;
		f.Start(p);
		f.Binary(299, true, Fx::Ts(1));
		REQUIRE(f.link.a.reports.size() == 1);
		View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::Offset2);
		CHECK(v.PointPositions() == std::vector<size_t>{301});
		CHECK(v.Positions() == std::vector<size_t>{301}); //no special objects in the string
		const Bytes octets = f.link.a.reports[0].results.back().data.AsOctetString();
		REQUIRE(octets.size() == 1 + 2 + 1);
		CHECK(octets == Bytes({0x02, 0x01, 0x2D, 0x80})); //rule 2, position 301, the state point On
		CHECK(f.Count("BlockData is true but", LogLevel::Warn) == 0);
		CHECK(f.server->ReportCounters().blockedSent == 1);
		f.Binary(3, true, Fx::Ts(2));
		REQUIRE(f.link.a.reports.size() == 2);
		CHECK(f.Read(f.link.a.reports[1]).decoded.rule == B::Rule::Offset1);
		//position 255 is the last that fits one octet, 256 the first that does not
		f.Binary(253, true, Fx::Ts(3));
		REQUIRE(f.link.a.reports.size() == 3);
		CHECK(f.Read(f.link.a.reports[2]).decoded.rule == B::Rule::Offset1);
		f.Binary(254, true, Fx::Ts(4));
		REQUIRE(f.link.a.reports.size() == 4);
		CHECK(f.Read(f.link.a.reports[3]).decoded.rule == B::Rule::Offset2);
		CHECK(f.server->ReportCounters().blockedSent == 4);
		CHECK(f.server->ReportsSent() == 4);
	}
	{
		//a mixed report: one position of the buffer time's changes is above 255, so ALL its pairs use 2 octet positions (the rule octet is one per string)
		Fx f(BigConf(true), BigMembers());
		Params p;
		p.rbe = true;
		p.bufferTime = 2;
		f.Start(p);
		f.Binary(299, true, Fx::Ts(1)); //position 301
		f.Binary(3, true, Fx::Ts(2));   //position 5
		f.Advance(2100);
		REQUIRE(f.link.a.reports.size() == 1);
		const View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::Offset2);
		CHECK(v.PointPositions() == std::vector<size_t>{5, 301});
		const Bytes octets = f.link.a.reports[0].results.back().data.AsOctetString();
		CHECK(octets == Bytes({0x02, 0x00, 0x05, 0x80, 0x01, 0x2D, 0x80})); //rule 2, (0005, On), (012D, On): ascending, every position 2 octets
	}
	for(const bool rbe : {true, false})
	{
		//a complete report is rule 0, whatever the size of the data set: no positions
		Fx f(BigConf(false), BigMembers());
		Params p;
		p.rbe = rbe;
		p.conditions = DC::IntegrityTimeOut;
		p.integrityCheck = 10;
		f.StartKeep(p);
		REQUIRE(f.link.a.reports.size() == 1);
		const View v = f.Read(f.link.a.reports[0]);
		CHECK(v.decoded.rule == B::Rule::NoRbe);
		CHECK(v.decoded.items.size() == 302);
		CHECK(v.report.data.size() == 1 + 65 + 1 + 300);
	}
}

TEST_CASE(SUITE("server, a position above 65535 fits no rule: that report is ordinary with one warning, the others of the transfer set stay blocked"))
{
	//the data set is the two special objects and 66000 StateQ points (positions 2 to 66001); the report of the last point needs a position above two octets
	const size_t points = 66000;
	Fx f(BigConf(true, points), BigMembers(points), 8 * 1024 * 1024); //the definition of the data set is about 1.5 MB
	f.link.chunk = 16384;                                             //delivered in pieces, as a TCP stream is: the receive buffer holds a bounded number of octets
	Params p;
	p.rbe = true;
	f.Start(p);
	f.Binary(points - 1, true, Fx::Ts(1)); //position 66001
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[0]).has_value()); //the ordinary form
	CHECK(f.Count("the blocked form cannot be built", LogLevel::Warn) == 1);
	CHECK(f.server->ReportCounters().blockedSent == 0);
	f.Binary(65533, true, Fx::Ts(2)); //position 65535: the last that fits two octets
	REQUIRE(f.link.a.reports.size() == 2);
	const View v = f.Read(f.link.a.reports[1]);
	CHECK(v.decoded.rule == B::Rule::Offset2);
	CHECK(v.PointPositions() == std::vector<size_t>{65535});
	CHECK(f.link.a.reports[1].results.back().data.AsOctetString() == Bytes({0x02, 0xFF, 0xFF, 0x80}));
	f.Binary(65534, true, Fx::Ts(3)); //position 65536
	REQUIRE(f.link.a.reports.size() == 3);
	CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[2]).has_value());
	f.Binary(10, true, Fx::Ts(4));
	REQUIRE(f.link.a.reports.size() == 4);
	CHECK(f.Read(f.link.a.reports[3]).decoded.rule == B::Rule::Offset1);
	CHECK(f.Count("the blocked form cannot be built", LogLevel::Warn) == 1); //one per start, not one per report
	CHECK(f.server->ReportCounters().blockedSent == 2);
	CHECK(f.server->ReportsSent() == 4);
}

TEST_CASE(SUITE("server, a name longer than its 32 octet storage form: reported in the ordinary form, one warning, and the configuration says so"))
{
	const std::string longName = "TS_POOL_" + std::string(30, 'X'); //38 characters
	std::string text = ServerJson;
	text.replace(text.find("%EXTRA%"), 7, Block3Extra);
	text.replace(text.find("\"TS_POOL_001\", \"TS_POOL_002\""), 28, "\"" + longName + "\"");
	const ICCPPortConf conf = LoadText(text);
	bool warned = false;
	for(const auto& w : conf.warnings)
		warned = warned || w.find("longer than 32 characters") != std::string::npos;
	CHECK(warned);
	Fx f(conf);
	f.transferSetName = longName;
	Params p;
	p.conditions = DC::IntegrityTimeOut;
	p.integrityCheck = 10;
	f.StartKeep(p);
	REQUIRE(f.link.a.reports.size() == 1);
	CHECK_FALSE(B::RecognizeBlockedReport(f.link.a.reports[0]).has_value()); //the ordinary form
	f.Advance(10000);
	REQUIRE(f.link.a.reports.size() == 2);
	CHECK(f.Count("the blocked form cannot be built", LogLevel::Warn) == 1);
	CHECK(f.server->ReportCounters().blockedSent == 0);
	CHECK(f.server->ReportsSent() == 2);
}

//---------------------------------------------------------------------------------------------------------------------
// the client side
//---------------------------------------------------------------------------------------------------------------------

namespace
{
const char* const ClientJson = R"({
	"ServiceRole": "Client", "LocalDomain": "DOMAIN_B", "RemoteDomain": "DOMAIN_A", "RemoteBilateralTableID": "BT_TEST_A",
	"Binaries": [{"Index": 0, "Name": "POINT_0001_ST", "Type": "StateQTimeTag"}],
	"Analogs": [{"Index": 0, "Name": "POINT_0001_MW", "Type": "RealQTimeTag"}, {"Index": 1, "Name": "POINT_0002_MW", "Type": "RealQTimeTag"}],
	"DataSets": [{"Name": "DS_TEST_001", "Members": ["POINT_0001_ST", "POINT_0001_MW", "POINT_0002_MW"]}],
	"TransferSets": [{"DataSet": "DS_TEST_001", "Conditions": ["ObjectChange", "IntegrityTimeOut"], "IntegrityCheck": 30,
		"SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"] %TRANSFER_SET%}]
	%EXTRA%
})";

std::string Keys(bool rbe, bool blockData, bool critical = false)
{
	auto yn = [](bool b) { return b ? "true" : "false"; };
	return std::string(", \"RBE\": ") + yn(rbe) + ", \"BlockData\": " + yn(blockData) + ", \"Critical\": " + yn(critical);
}

struct Pub
{
	std::string name;
	double value = 0;
	odc::QualityFlags quality = odc::QualityFlags::NONE;
	odc::msSinceEpoch_t timestamp = 0;
	bool operator==(const Pub&) const = default;
};

//the client's port configuration against a server's, in memory
struct Pair
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
	std::vector<Pub> published;

	//transferSet: the keys that follow the special objects in the transfer set (starting with ", "); extra: top level client keys (starting with ", ")
	Pair(const std::string& transferSet, const std::string& serverExtra, const std::string& clientExtra = "",
		const std::string& specials = ""):
		serverConf(LoadServer(serverExtra))
	{
		std::string text = ClientJson;
		if(!specials.empty())
		{
			const std::string standard = R"(["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"])";
			text.replace(text.find(standard), standard.size(), specials);
		}
		text.replace(text.find("%TRANSFER_SET%"), 14, transferSet);
		text.replace(text.find("%EXTRA%"), 7, clientExtra);
		Json::Value root;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream in(text);
		REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
		const auto problems = clientConf.Load(root, false);
		for(const auto& p : problems)
			UNSCOPED_INFO(p);
		REQUIRE(problems.empty());

		model = std::make_unique<tase2::ResponderModel>(serverConf, true);
		//the values the server has when the transfer set starts
		model->Update(PointKind::Binary, 0, true, 0, 0, odc::QualityFlags::ONLINE, Fx::Ts(1));
		model->Update(PointKind::Analog, 0, false, 11.5, 0, odc::QualityFlags::ONLINE, Fx::Ts(2));
		model->Update(PointKind::Analog, 1, false, 12.5, 0, odc::QualityFlags::ONLINE, Fx::Ts(3));
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = []() { return Wall; };
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, tb, [this](LogLevel l, const std::string& t) { serverLogs.emplace_back(l, t); });
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server->OnRequest(id, req);
						   return true;
					   };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		tase2::ClientSession::Callbacks cb;
		cb.onPoint = [this](const IccpPointConf& pt, const tase2::PointState& s) { published.push_back(Pub{pt.name, tase2::NumericValue(pt, s), s.quality, s.timestamp}); };
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
	void Analog(size_t index, double value, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Analog, index, false, value, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		Settle();
	}
	void State(bool on, odc::msSinceEpoch_t timestamp)
	{
		if(const auto changed = model->Update(PointKind::Binary, 0, on, 0, 0, odc::QualityFlags::ONLINE, timestamp))
			server->OnPointChanged(*changed);
		Settle();
	}
	//a report from a peer, handed to the client as the port does
	void Inject(const mms::InformationReport& r)
	{
		client->OnReport(r);
		Settle();
	}
	static size_t Count(const std::vector<std::pair<LogLevel, std::string>>& logs, const std::string& part, LogLevel level)
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if(l == level && t.find(part) != std::string::npos)
				++n;
		return n;
	}
	std::string FirstWarning(const std::string& part) const
	{
		for(const auto& [l, t] : clientLogs)
			if(l == LogLevel::Warn && t.find(part) != std::string::npos)
				return t;
		return "";
	}
};

//a block for the client's data set: the three special objects (Transfer_Set_Name, Transfer_Set_Time_Stamp, DSConditions_Detected, positions 0 to 2), then the points
//(StateQTimeTag, RealQTimeTag, RealQTimeTag, positions 3 to 5), whose items are given
std::vector<B::Item> Whole(const std::vector<B::Item>& points)
{
	std::vector<B::Item> items = HeadItems(DC::ObjectChange);
	items.insert(items.end(), points.begin(), points.end());
	return items;
}
PointValue StateAt(bool on, int64_t ts)
{
	return Pv(PointType::StateQTimeTag, 0, 0, static_cast<uint8_t>(on ? 0x80 : 0x40), ts);
}
PointValue RealAt(double v, int64_t ts)
{
	return Pv(PointType::RealQTimeTag, v, 0, 0x00, ts);
}

//the same session with BlockData asked for or not: the events the client published
std::vector<Pub> ClientScenario(bool rbe, bool blockData, bool block3, size_t* blockedSent = nullptr, size_t* blockedReceived = nullptr, size_t* undecodable = nullptr)
{
	Pair f(Keys(rbe, blockData) + R"(, "BufferTime": 2)", block3 ? Block3Extra : std::string());
	f.Analog(1, 13.5, Fx::Ts(4));
	f.Advance(2100);
	f.State(false, Fx::Ts(5));
	f.Analog(0, 14.5, Fx::Ts(6));
	f.Advance(2100);
	f.Advance(31000); //an integrity report
	if(blockedSent != nullptr)
		*blockedSent = f.server->ReportCounters().blockedSent;
	if(blockedReceived != nullptr)
		*blockedReceived = f.client->BlockedReportsReceived();
	if(undecodable != nullptr)
		*undecodable = f.client->BlockedReportsUndecodable();
	return f.published;
}
}

TEST_CASE(SUITE("client, BlockData: the blocked reports give the same events as the ordinary ones, with RBE and without"))
{
	for(bool rbe : {false, true})
	{
		CAPTURE(rbe);
		size_t sent = 0, received = 0, undecodable = 0;
		const auto ordinary = ClientScenario(rbe, false, true);
		const auto viaBlocked = ClientScenario(rbe, true, true, &sent, &received, &undecodable);
		const auto ignored = ClientScenario(rbe, true, false);
		REQUIRE(ordinary.size() >= 6);
		CHECK(viaBlocked == ordinary);
		CHECK(ignored == ordinary); //a server without Block 3 sends ordinary reports, which are handled as ever
		CHECK(sent >= 4);
		CHECK(received == sent);
		CHECK(undecodable == 0);
	}
	size_t sent = 0, received = 0;
	CHECK(ClientScenario(true, false, true, &sent, &received).size() >= 6);
	CHECK(sent == 0);
	CHECK(received == 0);
}

TEST_CASE(SUITE("client, a data set with Event_Code_Detected: the blocked reports decode with the 2 octet event code and positions counting all four special objects, also from the server"))
{
	const std::string specials = R"(["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected", "Event_Code_Detected"])";
	const int64_t t = 1700000100;
	const PointValue st = StateAt(true, t);
	const PointValue a = RealAt(42.25, t + 1);
	const PointValue b = RealAt(-43.5, t + 2);
	{
		Pair f(Keys(true, true), Block3Extra, "", specials);
		f.published.clear();
		//rule 0: the four special objects, the event code 2 octets (value 0 as in the capture), then the points at positions 4 to 6
		std::vector<B::Item> whole = HeadItems(DC::ObjectChange);
		B::SpecialValue code;
		whole.push_back(B::ObjectItem(3, tase2::Special::EventCode, code));
		whole.push_back(B::Item{4, st});
		whole.push_back(B::Item{5, a});
		whole.push_back(B::Item{6, b});
		const Bytes block = B::Encode(B::Rule::NoRbe, whole);
		CHECK(block.size() == 1 + 65 + 4 + 1 + 2 + 5 + 9 + 9);
		f.Inject(MakeBlocked(block));
		REQUIRE(f.published.size() == 3);
		CHECK(f.published[0].name == "POINT_0001_ST");
		CHECK(f.published[0].value == 1);
		CHECK(f.published[1].name == "POINT_0001_MW");
		CHECK(f.published[1].value == 42.25);
		CHECK(f.published[2].name == "POINT_0002_MW");
		CHECK(f.published[2].value == -43.5);
		//rule 1 with two points: positions 5 and 6 (the first analog and the second), the special objects counted
		f.published.clear();
		f.Inject(MakeBlocked(B::Encode(B::Rule::Offset1, {
			{5, a}, {6, b}
		})));
		REQUIRE(f.published.size() == 2);
		CHECK(f.published[0].name == "POINT_0001_MW");
		CHECK(f.published[1].name == "POINT_0002_MW");
		CHECK(f.client->BlockedReportsUndecodable() == 0);
	}
	{
		//the server and the client of the same data set: the changes of a buffer time are one rule 1 report, the integrity report rule 0
		Pair f(Keys(true, true) + R"(, "BufferTime": 2)", Block3Extra, "", specials);
		f.Advance(100); //the report that starts the transfer set
		f.published.clear();
		f.Analog(1, 13.5, Fx::Ts(4));
		f.Analog(0, 14.5, Fx::Ts(5));
		f.Advance(2100);
		REQUIRE(f.published.size() == 2);
		CHECK(f.published[0].value == 14.5);
		CHECK(f.published[1].name == "POINT_0002_MW");
		CHECK(f.published[1].value == 13.5);
		f.published.clear();
		f.Advance(31000); //an integrity report: the whole data set
		CHECK(f.published.size() >= 3);
		CHECK(f.server->ReportCounters().blockedSent >= 2);
		CHECK(f.client->BlockedReportsReceived() == f.server->ReportCounters().blockedSent);
		CHECK(f.client->BlockedReportsUndecodable() == 0);
		CHECK(Pair::Count(f.serverLogs, "BlockData is true but", LogLevel::Warn) == 0);
	}
}

TEST_CASE(SUITE("client, a blocked report of each rule is decoded into events, in the order of the block"))
{
	Pair f(Keys(true, true), Block3Extra);
	f.published.clear();
	const int64_t t = 1700000100;
	const PointValue st = StateAt(true, t);
	const PointValue a = RealAt(42.25, t + 1);
	const PointValue b = RealAt(-43.5, t + 2);
	//rule 0: the whole data set, the special objects first [V]
	f.Inject(MakeBlocked(B::Encode(B::Rule::NoRbe, Whole({
		{3, st}, {4, a}, {5, b}
	}))));
	REQUIRE(f.published.size() == 3);
	CHECK(f.published[0].name == "POINT_0001_ST");
	CHECK(f.published[0].value == 1);
	CHECK(f.published[0].timestamp == Ms(t));
	CHECK(f.published[1].name == "POINT_0001_MW");
	CHECK(f.published[1].value == 42.25);
	CHECK(f.published[1].timestamp == Ms(t + 1));
	CHECK(f.published[2].name == "POINT_0002_MW");
	CHECK(f.published[2].value == -43.5);
	CHECK(f.published[2].quality == odc::QualityFlags::ONLINE);
	//rule 1 [V]: positions 5 and 3 only, in that order, no special objects (a real peer's partial reports; the order of positions is the sender's)
	f.published.clear();
	f.Inject(MakeBlocked(B::Encode(B::Rule::Offset1, {
		{5, b}, {3, StateAt(false, t + 3)}
	})));
	REQUIRE(f.published.size() == 2);
	CHECK(f.published[0].name == "POINT_0002_MW");
	CHECK(f.published[0].value == -43.5);
	CHECK(f.published[1].name == "POINT_0001_ST");
	CHECK(f.published[1].value == 0);
	//rule 1 with the special objects in it (not what a real peer does; accepted, the special objects ignored)
	f.published.clear();
	f.Inject(MakeBlocked(B::Encode(B::Rule::Offset1, Whole({
		{5, b}
	}))));
	REQUIRE(f.published.size() == 1);
	CHECK(f.published[0].name == "POINT_0002_MW");
	//rule 2 [S]: position 4 only
	f.published.clear();
	f.Inject(MakeBlocked(B::Encode(B::Rule::Offset2, {
		{4, RealAt(7, t + 4)}
	})));
	REQUIRE(f.published.size() == 1);
	CHECK(f.published[0].name == "POINT_0001_MW");
	CHECK(f.published[0].value == 7);
	//a block that is the rule octet only is a report with nothing to apply
	f.published.clear();
	f.Inject(MakeBlocked(Bytes{0x01}));
	CHECK(f.published.empty());
	CHECK(f.client->BlockedReportsUndecodable() == 0);
	CHECK(f.client->BlockedReportsReceived() >= 4);
}

TEST_CASE(SUITE("client, the copy of Transfer_Set_Name in the octet string is only compared with the variable: a difference is a debug note, not a refusal"))
{
	Pair f(Keys(true, true), Block3Extra);
	f.published.clear();
	std::vector<B::Item> items = Whole({
		{3, StateAt(true, 1700000100)}, {4, RealAt(5, 1700000100)}, {5, RealAt(6, 1700000100)}
	});
	items[0].object.transferSet = tase2::ObjectRef{tase2::Scope::Icc, "DOMAIN_Z", "TS_OTHER"}; //inside the octet string
	items[1].object.number = 1;                                                                //the time stamp too
	items[2].object.conditions = DC::OperatorRequest;
	f.Inject(MakeBlocked(B::Encode(B::Rule::NoRbe, items)));
	CHECK(f.published.size() == 3); //applied all the same
	CHECK(f.client->BlockedReportsUndecodable() == 0);
	CHECK(f.Count(f.clientLogs, "Transfer_Set_Name inside the octet string (DOMAIN_Z/TS_OTHER) differs from the variable's (DOMAIN_A/TS_POOL_001)", LogLevel::Debug) == 1);
	CHECK(f.Count(f.clientLogs, "Transfer_Set_Time_Stamp inside the octet string (1) differs", LogLevel::Debug) == 1);
	CHECK(f.Count(f.clientLogs, "DSConditions_Detected inside the octet string", LogLevel::Debug) == 1);
	CHECK(f.Count(f.clientLogs, "differs", LogLevel::Warn) == 0);
	//the same copies: nothing to say
	f.Inject(MakeBlocked(B::Encode(B::Rule::NoRbe, Whole({
		{3, StateAt(true, 1700000100)}, {4, RealAt(5, 1700000100)}, {5, RealAt(6, 1700000100)}
	}))));
	CHECK(f.Count(f.clientLogs, "differs", LogLevel::Debug) == 3);
	//the report's time stamp and conditions are those of the separate variables: a critical set's acknowledgement echoes the variable's
	Pair g(Keys(true, true, true), Block3Extra);
	std::vector<B::Item> other = Whole({
		{4, RealAt(5, 1700000100)}
	});
	other[1].object.number = 99;
	g.Inject(MakeBlocked(B::Encode(B::Rule::Offset1, other)));
	const auto acks = [&]
				{
					std::vector<tase2::ReportAck> out;
					for(const auto& r : g.toServer)
						if(const auto a = tase2::ParseReportAck(r))
							out.push_back(*a);
					return out;
				}();
	REQUIRE(acks.size() == 1);
	CHECK(acks[0].timeStamp == 1700000000); //the variable's (MakeBlocked), not the 99 inside
}

TEST_CASE(SUITE("client, the variable that carries the octet string may be called anything, in any scope, in any position"))
{
	Pair f(Keys(true, true), Block3Extra);
	f.published.clear();
	const Bytes block = B::Encode(B::Rule::Offset1, Whole({
		{4, RealAt(5, 1700000100)}
	}));
	f.Inject(MakeBlocked(block)); //a real peer's: VMD-specific Block_Data [V]
	f.Inject(MakeBlocked(block, mms::ObjectName::Vmd("WHATEVER")));
	f.Inject(MakeBlocked(block, tase2::IccName("DOMAIN_Z", "SOME_BLOCK"), true));
	f.Inject(MakeBlocked(block, Icc("Block_Data_2"), true));
	CHECK(f.published.size() == 4);
	CHECK(f.client->BlockedReportsUndecodable() == 0);
	CHECK(f.Count(f.clientLogs, "could not", LogLevel::Warn) == 0);
}

namespace
{
struct BadBlock
{
	const char* name;
	Bytes data;
	const char* why;
};
}

TEST_CASE(SUITE("client, a blocked report that does not decode: nothing applied, one warning with the diagnostics, counted, not answered"))
{
	Pair f(Keys(true, true, true), Block3Extra); //Critical: a report that is not understood must not be acknowledged either
	f.published.clear();
	const int64_t t = 1700000100;
	const Bytes whole = B::Encode(B::Rule::NoRbe, Whole({
		{3, StateAt(true, t)}, {4, RealAt(1, t)}, {5, RealAt(2, t)}
	}));
	REQUIRE(whole.size() == 1 + 65 + 4 + 1 + 5 + 9 + 9);
	Bytes unknownRule = whole;
	unknownRule[0] = 5;
	Bytes shortBlock(whole.begin(), whole.end() - 1);
	Bytes leftover = whole;
	leftover.push_back(0);
	Bytes nameTooShort(whole.begin(), whole.begin() + 1 + 40);
	Bytes stampCut(whole.begin(), whole.begin() + 1 + 65 + 2);
	Bytes badScope = whole;
	badScope[1] = 9;
	const std::vector<BadBlock> table = {
		{"no rule octet", {}, "empty"},
		{"unknown rule", unknownRule, "unknown RBE blocking rule 5"},
		{"truncated, rule 0", shortBlock, "truncated"},
		{"leftover octets, rule 0", leftover, "leftover"},
		{"name storage too short", nameTooShort, "the value of member 0 needs 65 octets and 40 are left"},
		{"time stamp truncated", stampCut, "the value of member 1 needs 4 octets and 2 are left"},
		{"a scope octet that is neither 0 nor 1", badScope, "scope octet"},
		{"position out of range, rule 1", {0x01, 0x06, 0x80, 0x00, 0x00, 0x00, 0x00}, "outside the data set"},
		{"position out of range, rule 2", {0x02, 0x00, 0x06, 0x80, 0x00, 0x00, 0x00, 0x00}, "outside the data set"},
		{"truncated value, rule 1", {0x01, 0x04, 0x00, 0x00}, "truncated"},
		{"half a position, rule 2", {0x02, 0x00}, "truncated"},
		{"special object value truncated, rule 1", {0x01, 0x01, 0x00, 0x00}, "the value at position 1 needs 4 octets and 2 are left"},
		{"position without a value, rule 1", {0x01, 0x04}, "needs"},
		{"more values than members, rule 1", B::Encode(B::Rule::Offset1, Whole({
				{3, StateAt(true, t)}, {4, RealAt(1, t)}, {5, RealAt(2, t)}, {3, StateAt(false, t)}
			})), "count mismatch"}
	};
	const size_t acksBefore = f.client->ReportAcksSent();
	const size_t toServerBefore = f.toServer.size();
	size_t expectedUndecodable = 0;
	for(const auto& bad : table)
	{
		CAPTURE(bad.name);
		const size_t warnings = f.Count(f.clientLogs, "a blocked report could not be decoded", LogLevel::Warn);
		const size_t received = f.client->BlockedReportsReceived();
		f.Inject(MakeBlocked(bad.data));
		++expectedUndecodable;
		CHECK(f.published.empty()); //not one value applied
		CHECK(f.client->BlockedReportsUndecodable() == expectedUndecodable);
		CHECK(f.client->BlockedReportsReceived() == received + 1);
		CHECK(f.Count(f.clientLogs, "a blocked report could not be decoded", LogLevel::Warn) == warnings + 1); //one warning
		const std::string line = f.clientLogs.back().second;
		CHECK(f.clientLogs.back().first == LogLevel::Warn);
		CHECK(line.find("transfer set TS_POOL_001") != std::string::npos);
		CHECK(line.find("none of its values was applied") != std::string::npos);
		CHECK(line.find(bad.why) != std::string::npos);
		CHECK(line.find("Rule byte " + (bad.data.empty() ? std::string("(none)") : std::to_string(bad.data[0]))) != std::string::npos);
		CHECK(line.find(std::to_string(bad.data.size()) + " octets of blocked data") != std::string::npos);
		CHECK(line.find("the data set defines 6 members (3 of them special objects)") != std::string::npos);
		CHECK(line.find("Variables seen: DOMAIN_A/Transfer_Set_Name, DOMAIN_A/Transfer_Set_Time_Stamp, DOMAIN_A/DSConditions_Detected, Block_Data") != std::string::npos);
		CHECK(line.find("Blocked data: " + B::HexDump(bad.data)) != std::string::npos);
	}
	//no acknowledgement (the transfer set is critical), no NACK, nothing sent to the peer, and the association is untouched
	CHECK(f.client->ReportAcksSent() == acksBefore);
	CHECK(f.client->ReportNacksSent() == 0);
	CHECK(f.toServer.size() == toServerBefore);
	CHECK(f.link.a.conn.IsAssociated());
	CHECK(f.link.b.conn.IsAssociated());
	CHECK(f.client->Ready());
	//the next good one is applied and acknowledged
	f.Inject(MakeBlocked(B::Encode(B::Rule::Offset1, Whole({
		{4, RealAt(9, t)}
	}))));
	REQUIRE(f.published.size() == 1);
	CHECK(f.published[0].value == 9);
	CHECK(f.client->ReportAcksSent() == acksBefore + 1);
}

TEST_CASE(SUITE("client, the hex dump of a block that does not decode stops at 96 octets and says how many more there are"))
{
	Pair f(Keys(true, true), Block3Extra);
	Bytes data(150);
	for(size_t i = 0; i < data.size(); ++i)
		data[i] = static_cast<uint8_t>(i + 7);
	f.Inject(MakeBlocked(data));
	const std::string line = f.FirstWarning("a blocked report could not be decoded");
	REQUIRE_FALSE(line.empty());
	CHECK(line.find("Rule byte 7, 150 octets of blocked data") != std::string::npos);
	CHECK(line.find("Blocked data: 07 08 09") != std::string::npos);
	CHECK(line.find(B::HexDump(data, 96)) != std::string::npos);
	CHECK(line.find("... (54 more)") != std::string::npos);
	CHECK(line.find(" 67 ") == std::string::npos); //octet 96 (0x67 = 7 + 96) is the first left out
}

TEST_CASE(SUITE("client, a blocked report of a transfer set started with BlockData false is decoded all the same, with one warning"))
{
	Pair f(Keys(true, false), "");
	f.published.clear();
	const Bytes block = B::Encode(B::Rule::Offset1, Whole({
		{4, RealAt(5, 1700000100)}
	}));
	f.Inject(MakeBlocked(block));
	f.Inject(MakeBlocked(block));
	CHECK(f.published.size() == 2);
	CHECK(f.client->BlockedReportsReceived() == 2);
	CHECK(f.client->BlockedReportsUndecodable() == 0);
	CHECK(f.Count(f.clientLogs, "started with BlockData false but a report arrived in the blocked form", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("client, a blocked report that names no transfer set of ours is NACKed like any other, or counted when NackUnknownReports is off"))
{
	const tase2::ObjectRef other{tase2::Scope::Icc, "DOMAIN_A", "TS_POOL_002"};
	const Bytes block = B::Encode(B::Rule::Offset1, Whole({
		{4, RealAt(5, 1700000100)}
	}));
	{
		Pair f(Keys(true, true), Block3Extra);
		f.published.clear();
		f.Inject(MakeBlocked(block, mms::ObjectName::Vmd("Block_Data"), false, other));
		CHECK(f.published.empty());
		CHECK(f.client->ReportNacksSent() == 1);
		CHECK(f.client->BlockedReportsUndecodable() == 0);
		REQUIRE(f.toServer.size() >= 1);
		const auto ack = tase2::ParseReportAck(f.toServer.back());
		REQUIRE(ack.has_value());
		CHECK(ack->negative);
		CHECK(ack->transferSet == other);
	}
	{
		Pair f(Keys(true, true), Block3Extra, R"(, "NackUnknownReports": false)");
		f.published.clear();
		f.Inject(MakeBlocked(block, mms::ObjectName::Vmd("Block_Data"), false, other));
		CHECK(f.published.empty());
		CHECK(f.client->ReportNacksSent() == 0);
		CHECK(f.client->BlockedReportsUndecodable() == 1);
		CHECK(f.Count(f.clientLogs, "this client started no transfer set TS_POOL_002", LogLevel::Warn) == 1);
	}
}

TEST_CASE(SUITE("client, a blocked report that has no Transfer_Set_Name, or a special object that cannot be read, is not applied and says why"))
{
	Pair f(Keys(true, true), Block3Extra);
	f.published.clear();
	const Bytes block = B::Encode(B::Rule::Offset1, Whole({
		{4, RealAt(5, 1700000100)}
	}));
	f.Inject(MakeBlocked(block, mms::ObjectName::Vmd("Block_Data"), false, tase2::ObjectRef{}, false));
	CHECK(f.published.empty());
	CHECK(f.client->BlockedReportsUndecodable() == 1);
	CHECK(f.Count(f.clientLogs, "has no Transfer_Set_Name", LogLevel::Warn) == 1);
	auto broken = MakeBlocked(block);
	broken.results[1] = mms::AccessResult::Success(mms::Data::Boolean(true)); //the time stamp is not an integer
	f.Inject(broken);
	CHECK(f.published.empty());
	CHECK(f.client->BlockedReportsUndecodable() == 2);
	CHECK(f.client->ReportNacksSent() == 0);
	CHECK(f.link.a.conn.IsAssociated());
}

TEST_CASE(SUITE("client, a transfer set with BlockData and a peer without Block 3: a warning at association"))
{
	{
		Pair f(Keys(true, true), "");
		CHECK(f.Count(f.clientLogs, "BlockData true, but the peer does not advertise Block 3", LogLevel::Warn) == 1);
	}
	{
		Pair f(Keys(true, true), Block3Extra);
		CHECK(f.Count(f.clientLogs, "Block 3", LogLevel::Warn) == 0);
	}
	{
		Pair f(Keys(true, false), "");
		CHECK(f.Count(f.clientLogs, "Block 3", LogLevel::Warn) == 0);
	}
	//a peer that required block 3 of the peer drops the association instead (RequiredRemoteBlocks)
	{
		stacktest::Link link;
		const auto serverConf = LoadServer("");
		tase2::ResponderModel model(serverConf, true);
		tase2::Timebase tb;
		tb.steady = [&link]() { return link.clock.now; };
		tb.wallMs = []() { return Wall; };
		tase2::ResponderSession server(model, link.b.conn, tb, nullptr);
		link.b.requestHook = [&server](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   server.OnRequest(id, req);
						   return true;
					   };
		link.Start();
		std::string text = ClientJson;
		text.replace(text.find("%TRANSFER_SET%"), 14, Keys(true, true));
		text.replace(text.find("%EXTRA%"), 7, R"(, "RequiredRemoteBlocks": [1, 2, 3])");
		Json::Value root;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream in(text);
		REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
		ICCPPortConf clientConf;
		REQUIRE(clientConf.Load(root, false).empty());
		//the configuration says so: no hint to add 3 when it is there
		for(const auto& w : clientConf.warnings)
			CHECK(w.find("add 3 to") == std::string::npos);
		std::string fatal;
		tase2::ClientSession::Callbacks cb;
		cb.onFatal = [&fatal](const std::string& why) { fatal = why; };
		tase2::ClientSession client(clientConf, link.a.conn, tb, nullptr, std::move(cb));
		client.OnAssociated();
		link.Pump();
		CHECK(fatal.find("does not support TASE.2 block 3") != std::string::npos);
		CHECK_FALSE(client.Ready());
	}
}

TEST_CASE(SUITE("client, Critical: a blocked report is acknowledged with the time stamp it carried, and the server's list ends empty"))
{
	Pair f(Keys(true, true, true), Block3Extra);
	f.Advance(50); //the integrity report at the start
	f.Analog(1, 14.5, Fx::Ts(4));
	f.Analog(0, 15.5, Fx::Ts(5));
	REQUIRE(f.server->ReportCounters().blockedSent >= 3); //the integrity report at the start and the two changes
	CHECK(f.client->ReportAcksSent() == f.server->ReportCounters().blockedSent);
	CHECK(f.server->OutstandingCriticalReports() == 0);
	CHECK(f.server->ReportCounters().criticalSent == f.server->ReportCounters().acknowledged);
	CHECK(f.server->ReportCounters().dataFailures == 0);
	size_t acks = 0;
	for(const auto& r : f.toServer)
		if(const auto a = tase2::ParseReportAck(r))
		{
			++acks;
			CHECK_FALSE(a->negative);
			CHECK(a->timeStamp == WallSeconds);
		}
	CHECK(acks == f.client->ReportAcksSent());
	f.Advance(20000);
	CHECK(f.server->ReportCounters().dataFailures == 0);
}

//---------------------------------------------------------------------------------------------------------------------
// two ICCP ports over real TCP
//---------------------------------------------------------------------------------------------------------------------

namespace
{
std::string ConfFileName(const char* role, int tag)
{
	return std::string("iccp_blocked_") + role + "_" + std::to_string(tag) + ".conf";
}

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

void WriteFile(const char* file, const std::string& text)
{
	std::ofstream ofs(file);
	REQUIRE(ofs.good());
	ofs << text;
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
	for(size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size()))
		text.replace(pos, from.size(), to);
	return text;
}

//every point type, on both ends: four state types (Binaries), four real types (Analogs), four discrete types (Counters)
const char* const PointsJson = R"(
	"Binaries": [{"Index": 0, "Name": "PT_B0", "Type": "State"}, {"Index": 1, "Name": "PT_B1", "Type": "StateQ"},
		{"Index": 2, "Name": "PT_B2", "Type": "StateQTimeTag"}, {"Index": 3, "Name": "PT_B3", "Type": "StateExtended"}],
	"Analogs": [{"Index": 0, "Name": "PT_A0", "Type": "Real"}, {"Index": 1, "Name": "PT_A1", "Type": "RealQ"},
		{"Index": 2, "Name": "PT_A2", "Type": "RealQTimeTag"}, {"Index": 3, "Name": "PT_A3", "Type": "RealExtended"}],
	"Counters": [{"Index": 0, "Name": "PT_C0", "Type": "Discrete"}, {"Index": 1, "Name": "PT_C1", "Type": "DiscreteQ"},
		{"Index": 2, "Name": "PT_C2", "Type": "DiscreteQTimeTag"}, {"Index": 3, "Name": "PT_C3", "Type": "DiscreteExtended"}])";

const char* const LoopServerConf = R"({
	"IP": "127.0.0.1", "Port": "%PORT%",
	"LocalTSelector": "0001", "RemoteTSelector": "0002", "LocalSSelector": "0001", "RemoteSSelector": "0002",
	"LocalPSelector": "00000001", "RemotePSelector": "00000002",
	"LocalAPTitle": "1.3.9999.1.1", "RemoteAPTitle": "1.3.9999.1.2", "LocalAEQualifier": 1, "RemoteAEQualifier": 2,
	"BilateralTableID": "BT_TEST_A", "RemoteBilateralTableID": "BT_TEST_B",
	"LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B", "ServiceRole": "Server",
	"SupportedBlocks": [1, 2%THREE%],
	%POINTS%,
	"TransferSetPool": ["TS_POOL_001", "TS_POOL_002"] %SRVEXTRA%
})";

const char* const LoopClientConf = R"({
	"IP": "127.0.0.1", "Port": "%PORT%", "TCPConnectRetryPeriodms": 200,
	"LocalTSelector": "0002", "RemoteTSelector": "0001", "LocalSSelector": "0002", "RemoteSSelector": "0001",
	"LocalPSelector": "00000002", "RemotePSelector": "00000001",
	"LocalAPTitle": "1.3.9999.1.2", "RemoteAPTitle": "1.3.9999.1.1", "LocalAEQualifier": 2, "RemoteAEQualifier": 1,
	"BilateralTableID": "BT_TEST_B", "RemoteBilateralTableID": "BT_TEST_A",
	"LocalDomain": "DOMAIN_B", "RemoteDomain": "DOMAIN_A",
	%POINTS%,
	"DataSets": [{"Name": "DS_TEST_001", "Members": ["PT_B0", "PT_B1", "PT_B2", "PT_B3", "PT_A0", "PT_A1", "PT_A2", "PT_A3", "PT_C0", "PT_C1", "PT_C2", "PT_C3"]}],
	"TransferSets": [{"DataSet": "DS_TEST_001", "Conditions": [%CONDITIONS%], "IntegrityCheck": 30,
		"SpecialObjects": ["Transfer_Set_Name", "Transfer_Set_Time_Stamp", "DSConditions_Detected"] %TRANSFER_SET%}]
})";

class Recorder: public odc::IOHandler
{
public:
	struct Rec
	{
		odc::EventType type;
		size_t index;
		double value;
		odc::QualityFlags quality;
		odc::msSinceEpoch_t timestamp;
	};
	Recorder(): odc::IOHandler("Recorder")
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
			Rec r{event->GetEventType(), event->GetIndex(), 0, event->GetQuality(), event->GetTimestamp()};
			switch(r.type)
			{
				case odc::EventType::Binary: r.value = event->GetPayload<odc::EventType::Binary>() ? 1 : 0; break;
				case odc::EventType::Analog: r.value = event->GetPayload<odc::EventType::Analog>(); break;
				case odc::EventType::Counter: r.value = event->GetPayload<odc::EventType::Counter>(); break;
				default: break;
			}
			events.push_back(r);
		}
		(*cb)(odc::CommandStatus::SUCCESS);
	}
	void Enable() override
	{}
	void Disable() override
	{}
	size_t Count(odc::ConnectState s)
	{
		std::lock_guard<std::mutex> lock(mtx);
		size_t n = 0;
		for(auto x : states)
			n += (x == s) ? 1 : 0;
		return n;
	}
	std::optional<Rec> Last(odc::EventType type, size_t index)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(auto it = events.rbegin(); it != events.rend(); ++it)
			if(it->type == type && it->index == index)
				return *it;
		return std::nullopt;
	}
	//every record of a point, in the order it arrived
	std::vector<Rec> All(odc::EventType type, size_t index)
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::vector<Rec> out;
		for(const auto& e : events)
			if(e.type == type && e.index == index)
				out.push_back(e);
		return out;
	}

private:
	std::mutex mtx;
	std::vector<odc::ConnectState> states;
	std::vector<Rec> events;
};

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

void Feed(ICCPPortBase& server, odc::EventType type, size_t index, double value, odc::msSinceEpoch_t ts)
{
	auto event = std::make_shared<odc::EventInfo>(type, index, "TestHarness", odc::QualityFlags::ONLINE, ts);
	switch(type)
	{
		case odc::EventType::Binary: event->SetPayload<odc::EventType::Binary>(value != 0); break;
		case odc::EventType::Analog: event->SetPayload<odc::EventType::Analog>(double(value)); break;
		case odc::EventType::Counter: event->SetPayload<odc::EventType::Counter>(uint32_t(value)); break;
		default: break;
	}
	std::atomic_bool done{false};
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>([&done](odc::CommandStatus) { done = true; });
	server.Event(event, "TestHarness", cb);
	REQUIRE(WaitFor([&] { return done.load(); }));
}

struct Loop
{
	std::shared_ptr<odc::asio_service> ios = odc::asio_service::Get();
	std::unique_ptr<odc::work_guard, odc::deleter> work;
	std::vector<std::thread> threads;
	std::shared_ptr<ICCPServerPort> server;
	std::shared_ptr<ICCPClientPort> client;
	Recorder recorder;
	const std::string serverFile;
	const std::string clientFile;

	Loop(int port, bool rbe, bool blockData, bool block3, bool critical = false, const std::string& moreKeys = "", bool integrity = true, bool rule2Off = false):
		serverFile(ConfFileName("server", port)),
		clientFile(ConfFileName("client", port))
	{
		SetupLoggers();
		if(ios->stopped())
			throw std::runtime_error("the ODC io service was kept alive and stopped by an earlier test");
		work = ios->make_work();
		for(int i = 0; i < 3; i++)
			threads.emplace_back([this] { ios->run(); });
		const std::string p = std::to_string(port);
		WriteFile(serverFile.c_str(), Replace(Replace(Replace(Replace(LoopServerConf, "%PORT%", p), "%THREE%", block3 ? ", 3" : ""), "%POINTS%", PointsJson), "%SRVEXTRA%", rule2Off ? R"(, "BlockedRule2": false)" : ""));
		WriteFile(clientFile.c_str(), Replace(Replace(Replace(Replace(LoopClientConf, "%PORT%", p), "%POINTS%", PointsJson), "%TRANSFER_SET%", Keys(rbe, blockData, critical) + moreKeys),
			"%CONDITIONS%", integrity ? R"("ObjectChange", "IntegrityTimeOut")" : R"("ObjectChange")"));
		server = std::make_shared<ICCPServerPort>("TestServer", serverFile, Json::Value(Json::objectValue));
		client = std::make_shared<ICCPClientPort>("TestClient", clientFile, Json::Value(Json::objectValue));
		server->Build();
		client->Build();
		client->Subscribe(&recorder, "Recorder");
	}
	~Loop()
	{
		if(client)
			client->Disable();
		if(server)
			server->Disable();
		WaitFor([&] { return !client->AssociationUp() && !server->AssociationUp(); }, 5000);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		client.reset();
		server.reset();
		work.reset();
		ios->run();
		for(auto& t : threads)
			t.join();
		std::remove(serverFile.c_str());
		std::remove(clientFile.c_str());
	}
	void Run()
	{
		REQUIRE(server->ConfigValid());
		REQUIRE(client->ConfigValid());
		server->Enable();
		client->Enable();
		REQUIRE(WaitFor([&] { return recorder.Count(odc::ConnectState::CONNECTED) == 1; }));
		REQUIRE(WaitFor([&]
			{
				const Json::Value st = client->GetStatus();
				return st.isMember("Provisioning") && st["Provisioning"].size() == 1 && st["Provisioning"][0]["State"].asString() == "transfer-started";
			}));
	}
};

struct LoopPoint
{
	odc::EventType type;
	size_t index;
	PointType ptype;
};
const std::vector<LoopPoint>& LoopPoints()
{
	static const std::vector<LoopPoint> points = {
		{odc::EventType::Binary, 0, PointType::State}, {odc::EventType::Binary, 1, PointType::StateQ}, {odc::EventType::Binary, 2, PointType::StateQTimeTag},
		{odc::EventType::Binary, 3, PointType::StateExtended}, {odc::EventType::Analog, 0, PointType::Real}, {odc::EventType::Analog, 1, PointType::RealQ},
		{odc::EventType::Analog, 2, PointType::RealQTimeTag}, {odc::EventType::Analog, 3, PointType::RealExtended}, {odc::EventType::Counter, 0, PointType::Discrete},
		{odc::EventType::Counter, 1, PointType::DiscreteQ}, {odc::EventType::Counter, 2, PointType::DiscreteQTimeTag}, {odc::EventType::Counter, 3, PointType::DiscreteExtended}
	};
	return points;
}

//the value a point is given in a round, and its time stamp (milliseconds are only carried by the Extended types, seconds by the TimeTag types, none by the others)
double RoundValue(const LoopPoint& p, int round)
{
	switch(p.type)
	{
		case odc::EventType::Binary: return round % 2 == 1 ? 1 : 0;
		case odc::EventType::Analog: return 10.5 * round + static_cast<double>(p.index);
		default: return 100.0 * round + static_cast<double>(p.index);
	}
}
odc::msSinceEpoch_t RoundStamp(const LoopPoint& p, int round)
{
	return Wall + 1000 * static_cast<odc::msSinceEpoch_t>(round) + 250 + 10 * static_cast<odc::msSinceEpoch_t>(p.index) + (p.type == odc::EventType::Counter ? 3 : 0);
}

//change every point in a round and wait for the client port to publish each one
void FeedRound(Loop& h, int round)
{
	for(const auto& p : LoopPoints())
		Feed(*h.server, p.type, p.index, RoundValue(p, round), RoundStamp(p, round));
	for(const auto& p : LoopPoints())
	{
		CAPTURE(round, static_cast<int>(p.type), p.index);
		REQUIRE(WaitFor([&]
			{
				const auto r = h.recorder.Last(p.type, p.index);
				return r && r->value == RoundValue(p, round);
			}));
		const auto r = h.recorder.Last(p.type, p.index);
		const odc::msSinceEpoch_t stamp = RoundStamp(p, round);
		if(p.ptype == PointType::StateExtended || p.ptype == PointType::RealExtended || p.ptype == PointType::DiscreteExtended)
			CHECK(r->timestamp == stamp);
		else if(p.ptype == PointType::StateQTimeTag || p.ptype == PointType::RealQTimeTag || p.ptype == PointType::DiscreteQTimeTag)
			CHECK(r->timestamp == stamp / 1000 * 1000);
	}
}
}

TEST_CASE(SUITE("over TCP: every point type, RBE on and off, BlockData on and off, Block 3 advertised and not"))
{
	int port = 20450;
	for(bool rbe : {false, true})
		for(bool blockData : {false, true})
			for(bool block3 : {false, true})
			{
				CAPTURE(rbe, blockData, block3, port);
				Loop h(port++, rbe, blockData, block3);
				h.Run();
				FeedRound(h, 1);
				FeedRound(h, 2);
				//every report is blocked when BlockData is asked for and Block 3 advertised: rule 0 for a complete report, rule 1 for what changed (RBE)
				const bool blocked = blockData && block3;
				if(blocked)
				{
					REQUIRE(WaitFor([&] { return h.server->GetStatistics()["BlockedReportsSent"].asUInt64() > 0; }));
					REQUIRE(WaitFor([&] { return h.client->GetStatistics()["BlockedReportsReceived"].asUInt64() == h.server->GetStatistics()["BlockedReportsSent"].asUInt64(); }));
					//every report of the transfer set was blocked, and every one was decoded
					REQUIRE(WaitFor([&] { return h.server->GetStatistics()["ReportsSent"].asUInt64() == h.server->GetStatistics()["BlockedReportsSent"].asUInt64(); }));
				}
				else
					REQUIRE(WaitFor([&] { return h.server->GetStatistics()["ReportsSent"].asUInt64() >= 1; }));
				const auto sent = h.server->GetStatistics();
				const auto received = h.client->GetStatistics();
				CHECK(sent["BlockedReportsSent"].asUInt64() == (blocked ? sent["ReportsSent"].asUInt64() : 0));
				CHECK(received["BlockedReportsReceived"].asUInt64() == sent["BlockedReportsSent"].asUInt64());
				CHECK(received["BlockedReportsUndecodable"].asUInt64() == 0);
				CHECK(h.server->AssociationUp());
				CHECK(h.client->AssociationUp());
			}
}

TEST_CASE(SUITE("over TCP: Critical blocked reports are acknowledged, the server's outstanding list ends empty and nothing fails"))
{
	Loop h(20460, true, true, true, true);
	h.Run();
	FeedRound(h, 1);
	FeedRound(h, 2);
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["CriticalReportsSent"].asUInt64() > 0; }));
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["CriticalReportsOutstanding"].asUInt64() == 0; }));
	const auto s = h.server->GetStatistics();
	CHECK(s["CriticalReportsAcknowledged"].asUInt64() == s["CriticalReportsSent"].asUInt64());
	CHECK(s["BlockedReportsSent"].asUInt64() == s["CriticalReportsSent"].asUInt64());
	CHECK(s["DataFailures"].asUInt64() == 0);
	CHECK(s["ReportNacksReceived"].asUInt64() == 0);
	CHECK(h.client->GetStatistics()["ReportAcksSent"].asUInt64() == s["CriticalReportsSent"].asUInt64());
	CHECK(h.client->GetStatistics()["BlockedReportsUndecodable"].asUInt64() == 0);
}

TEST_CASE(SUITE("over TCP: AllChangesReported with BlockData: every change arrives as its own event, in order"))
{
	Loop h(20461, true, true, true, false, R"(, "BufferTime": 1, "AllChangesReported": true)", false);
	h.Run();
	//one buffer period: two analogs change in turn, one of them three times (no integrity report at the start to come in between)
	Feed(*h.server, odc::EventType::Analog, 2, 11, Wall + 1000);
	Feed(*h.server, odc::EventType::Analog, 3, 21, Wall + 2000);
	Feed(*h.server, odc::EventType::Analog, 2, 12, Wall + 3000);
	Feed(*h.server, odc::EventType::Analog, 2, 13, Wall + 4000);
	REQUIRE(WaitFor([&] { return h.server->GetStatistics()["BlockedReportsSent"].asUInt64() >= 3 && h.recorder.Last(odc::EventType::Analog, 2) && h.recorder.Last(odc::EventType::Analog, 2)->value == 13; }));
	CHECK(h.recorder.Last(odc::EventType::Analog, 3)->value == 21);
	//the changes of the same point arrive in the order they happened, each with the time stamp of its change
	const auto changes = h.recorder.All(odc::EventType::Analog, 2);
	REQUIRE(changes.size() >= 3);
	std::string seen;
	for(const auto& c : changes)
		seen += (seen.empty() ? "" : " ") + std::to_string(static_cast<int>(c.value)) + "@" + std::to_string(c.timestamp);
	CAPTURE(seen);
	const size_t n = changes.size();
	CHECK(changes[n - 3].value == 11);
	CHECK(changes[n - 2].value == 12);
	CHECK(changes[n - 1].value == 13);
	CHECK(changes[n - 3].timestamp == (Wall + 1000) / 1000 * 1000);
	CHECK(changes[n - 2].timestamp == (Wall + 3000) / 1000 * 1000);
	CHECK(changes[n - 1].timestamp == (Wall + 4000) / 1000 * 1000);
	REQUIRE(WaitFor([&] { return h.client->GetStatistics()["BlockedReportsReceived"].asUInt64() == h.server->GetStatistics()["BlockedReportsSent"].asUInt64(); }));
	CHECK(h.client->GetStatistics()["BlockedReportsUndecodable"].asUInt64() == 0);
}
