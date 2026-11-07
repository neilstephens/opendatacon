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
 * Tase2Test.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the TASE.2 layer (Blocks 1 and 2). Fixtures are synthetic: invented names and values, with the
 *  byte structure of real ICCP traffic, assembled with an independent encoder.
 */

#include "Tase2.h"
#include "Tase2Quality.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP TASE.2 - " name

namespace
{
using namespace iccp::tase2;
using iccp::ber::DecodeError;
namespace mms = iccp::mms;

Bytes Hex(const std::string& s)
{
	Bytes out;
	int hi = -1;
	for(char c : s)
	{
		int v;
		if(c >= '0' && c <= '9') v = c - '0';
		else if(c >= 'a' && c <= 'f') v = c - 'a' + 10;
		else if(c >= 'A' && c <= 'F') v = c - 'A' + 10;
		else continue;
		if(hi < 0) hi = v;
		else
		{
			out.push_back(static_cast<uint8_t>(hi * 16 + v));
			hi = -1;
		}
	}
	return out;
}

std::string PointName(size_t i, const char* suffix = "MW")
{
	char buf[32];
	snprintf(buf, sizeof(buf), "POINT_%04zu_%s", i, suffix);
	return buf;
}

// ---- fixtures (synthetic) ----
const char* AssocReq = "a05902021234a453a151a04f3022a020a11e1a08444f4d41494e5f411a1242696c61746572616c5f5461626c655f49443011a00f800d54415345325f56657273696f6e3016a0148012537570706f727465645f4665617475726573";
const char* AssocReqSwapped = "a05902021234a453a151a04f3022a020a11e1a08444f4d41494e5f411a1242696c61746572616c5f5461626c655f49443016a0148012537570706f727465645f46656174757265733011a00f800d54415345325f56657273696f6e";
const char* AssocResp = "a12202021234a41ca11a8a0a42545f544553545f3031a207850207cc850108840304c800";
const char* NextReq = "a02f02021234a429a127a0253023a021a11f1a08444f4d41494e5f411a134e6578745f44535472616e736665725f536574";
const char* NextResp = "a12302021234a41da11ba2198601018a08444f4d41494e5f418a0a54535f544553545f3031";
const char* Write12 = "a06702021234a561a01c301aa018a1161a08444f4d41494e5f411a0a54535f544553545f3031a041a23fa21a8601018a08444f4d41494e5f418a0b44535f544553545f30303185010085010585010085010085020258840203c8830100830100830101830101850100";
const char* Write13Stop = "a06a02021234a564a01c301aa018a1161a08444f4d41494e5f411a0a54535f544553545f3031a044a242a21a8601018a08444f4d41494e5f418a0b44535f544553545f30303185010085010585010085010085020258840203c8830100830100830101830101830100850100";
const char* Report = "a35fa05da119a1171a08444f4d41494e5f411a0b44535f544553545f303031a040a2198601018a08444f4d41494e5f418a0a54535f544553545f303185046553f10084020348850100a20b87050842f6e979840200308402008080010a84020042";
const char* ReportMin = "a347a045a119a1171a08444f4d41494e5f411a0b44535f544553545f303031a028a2198601018a08444f4d41494e5f418a0a54535f544553545f3031a20b870508c020000084020000";

struct PointFixture
{
	PointType type;
	const char* hex;
};
const std::vector<PointFixture> PointFixtures = {
	{PointType::Real, "87050842f6e979"},
	{PointType::RealQ, "a20b87050842f6e9798402000c"},
	{PointType::RealQTimeTag, "a21185046553f10087050842f6e97984020000"},
	{PointType::RealExtended, "a21aa20a85046553f100850200fa87050842f6e97984020000850107"},
	{PointType::State, "84020680"},
	{PointType::StateQ, "84020080"},
	{PointType::StateQTimeTag, "a20a85046553f10084020040"},
	{PointType::StateExtended, "a213a20a85046553f100850200fa84020040850103"},
	{PointType::Discrete, "8501d6"},
	{PointType::DiscreteQ, "a2078501d684020030"},
	{PointType::DiscreteQTimeTag, "a20d85046553f1008501d684020030"},
	{PointType::DiscreteExtended, "a216a20a85046553f100850200fa8501d684020030850109"},
};

mms::ConfirmedRequest ReqOf(const char* hex)
{
	return std::get<mms::ConfirmedRequest>(mms::Decode(Hex(hex)));
}
mms::ConfirmedResponse RespOf(const char* hex)
{
	return std::get<mms::ConfirmedResponse>(mms::Decode(Hex(hex)));
}

Bytes Wire(const mms::ConfirmedRequest& r)
{
	return mms::Encode(r);
}

PointValue SamplePoint(PointType t)
{
	PointValue p;
	p.type = t;
	p.real = 123.456f;
	p.discrete = -42;
	switch(t)
	{
		case PointType::RealQ: p.flags = 0x0c; break;
		case PointType::State: p.state = StateValue::On; p.flags = 0x80; break;
		case PointType::StateQ: p.state = StateValue::On; p.flags = 0x80; break;
		case PointType::StateQTimeTag:
		case PointType::StateExtended: p.state = StateValue::Off; p.flags = 0x40; break;
		case PointType::DiscreteQ:
		case PointType::DiscreteQTimeTag:
		case PointType::DiscreteExtended: p.flags = 0x30; break;
		default: break;
	}
	if(t == PointType::RealQTimeTag || t == PointType::StateQTimeTag || t == PointType::DiscreteQTimeTag)
		p.timeStamp = 1700000000;
	if(t == PointType::RealExtended || t == PointType::StateExtended || t == PointType::DiscreteExtended)
	{
		p.timeStamp = 1700000000;
		p.milliseconds = 250;
		p.cov = t == PointType::RealExtended ? 7 : (t == PointType::StateExtended ? 3 : 9);
	}
	return p;
}
}

TEST_CASE(SUITE("Scope and object references"))
{
	REQUIRE(ScopeOf(VccName("X")) == Scope::Vcc);
	REQUIRE(ScopeOf(IccName("DOMAIN_A", "X")) == Scope::Icc);
	REQUIRE_THROWS_AS(ScopeOf(ObjectName::Aa("X")), DecodeError);

	const ObjectRef icc{Scope::Icc, "DOMAIN_A", "TS_TEST_01"};
	REQUIRE(iccp::mms::EncodeData(ToData(icc)) == Hex("a2 19 86 01 01 8a 08 444f4d41494e5f41 8a 0a 54535f544553545f3031"));
	REQUIRE(ParseObjectRef(ToData(icc)) == icc);
	REQUIRE(icc.ToName() == IccName("DOMAIN_A", "TS_TEST_01"));
	REQUIRE(ObjectRef::FromName(IccName("DOMAIN_A", "TS_TEST_01")) == icc);

	const ObjectRef vcc{Scope::Vcc, "", "ITEM_X"};
	REQUIRE(ParseObjectRef(ToData(vcc)) == vcc);
	REQUIRE(ObjectRef::FromName(VccName("ITEM_X")) == vcc);
	REQUIRE(vcc.ToName() == VccName("ITEM_X"));
	//the two member form for a VCC name is accepted
	REQUIRE(ParseObjectRef(Data::Structure({Data::Unsigned(0), Data::VisibleString("ITEM_X")})) == vcc);

	//malformed
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Integer(1)), DecodeError);
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Structure({})), DecodeError);
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Structure({Data::Unsigned(2), Data::VisibleString("A"), Data::VisibleString("B")})), DecodeError);
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Structure({Data::Unsigned(1), Data::VisibleString("B")})), DecodeError);
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Structure({Data::Unsigned(1), Data::Integer(1), Data::VisibleString("B")})), DecodeError);
	REQUIRE_THROWS_AS(ParseObjectRef(Data::Structure({Data::VisibleString("x"), Data::VisibleString("A"), Data::VisibleString("B")})), DecodeError);
}

TEST_CASE(SUITE("Association objects: version and supported features"))
{
	REQUIRE(iccp::mms::EncodeData(ToData(Edition1996)) == Hex("a2 07 85 02 07 cc 85 01 08"));
	REQUIRE(iccp::mms::EncodeData(ToData(Edition2000)) == Hex("a2 07 85 02 07 d0 85 01 08"));
	REQUIRE(ParseVersion(ToData(Edition1996)) == Edition1996);
	REQUIRE_THROWS_AS(ParseVersion(Data::Integer(1)), DecodeError);
	REQUIRE_THROWS_AS(ParseVersion(Data::Structure({Data::Integer(1)})), DecodeError);
	REQUIRE_THROWS_AS(ParseVersion(Data::Structure({Data::Integer(1), Data::VisibleString("x")})), DecodeError);

	const auto features = MakeSupportedFeatures({Block::Block1, Block::Block2, Block::Block5});
	REQUIRE(features.BitCount() == SupportedFeaturesBitCount);
	REQUIRE(iccp::mms::EncodeData(ToData(features)) == Hex("84 03 04 c8 00"));
	REQUIRE(HasBlock(features, Block::Block1));
	REQUIRE(HasBlock(features, Block::Block2));
	REQUIRE_FALSE(HasBlock(features, Block::Block3));
	REQUIRE(HasBlock(features, Block::Block5));
	REQUIRE_FALSE(HasBlock(features, Block::Block9));
	REQUIRE(ParseSupportedFeatures(ToData(features)) == features);
	REQUIRE(MakeSupportedFeatures({Block::Block9}, 9).BitCount() == 9);
	REQUIRE_THROWS_AS(MakeSupportedFeatures({Block::Block1}, 8), std::invalid_argument);
	REQUIRE_THROWS_AS(ParseSupportedFeatures(Data::Integer(1)), DecodeError);
	//a peer's string of any length is carried as is
	REQUIRE(ParseSupportedFeatures(Data::Bits(iccp::mms::MakeBits({0, 9}, 10))).Get(9));
}

TEST_CASE(SUITE("Association objects: read request and response"))
{
	const mms::ReadRequest req = BuildAssociationInfoRequest("DOMAIN_A");
	REQUIRE(Wire(mms::ConfirmedRequest{0x1234, req}) == Hex(AssocReq));
	REQUIRE(std::get<mms::ReadRequest>(ReqOf(AssocReq).service) == req);
	REQUIRE(IsAssociationInfoRequest(req));
	REQUIRE(BuildAssociationInfoRequest("DOMAIN_A", false, false).specification ==
		mms::VariableAccessSpecification(mms::ListOfVariable{mms::VariableSpec{IccName("DOMAIN_A", "Bilateral_Table_ID"), std::nullopt}
			}));

	const mms::ReadResponse resp = std::get<mms::ReadResponse>(RespOf(AssocResp).service);
	const AssociationInfo info = ParseAssociationInfoResponse(req, resp);
	REQUIRE(info.bilateralTableId == "BT_TEST_01");
	REQUIRE(info.version == Edition1996);
	REQUIRE(info.features == MakeSupportedFeatures({Block::Block1, Block::Block2, Block::Block5}));
	REQUIRE(info.failures.empty());

	//either order of the request is understood; results follow the request
	const mms::ReadRequest swapped = std::get<mms::ReadRequest>(ReqOf(AssocReqSwapped).service);
	mms::ReadResponse swappedResp;
	swappedResp.results = {resp.results[0], resp.results[2], resp.results[1]};
	REQUIRE(ParseAssociationInfoResponse(swapped, swappedResp) == info);

	//failures are recorded by item name
	mms::ReadResponse partial;
	partial.results = {resp.results[0], mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent), resp.results[2]};
	const AssociationInfo p = ParseAssociationInfoResponse(req, partial);
	REQUIRE(p.bilateralTableId == "BT_TEST_01");
	REQUIRE_FALSE(p.version.has_value());
	REQUIRE(p.failures.at("TASE2_Version") == mms::DataAccessError::ObjectNonExistent);

	//mismatches
	mms::ReadResponse shortResp;
	shortResp.results = {resp.results[0]};
	REQUIRE_THROWS_AS(ParseAssociationInfoResponse(req, shortResp), DecodeError);
	mms::ReadResponse wrongType = resp;
	wrongType.results[1] = mms::AccessResult::Success(Data::Integer(1));
	REQUIRE_THROWS_AS(ParseAssociationInfoResponse(req, wrongType), DecodeError);
	mms::ReadRequest byList;
	byList.specification = IccName("DOMAIN_A", "LIST_X");
	REQUIRE_THROWS_AS(ParseAssociationInfoResponse(byList, resp), DecodeError);
	REQUIRE_FALSE(IsAssociationInfoRequest(byList));
}

TEST_CASE(SUITE("Association objects: server side response"))
{
	LocalAssociation local;
	local.bilateralTableIds = {
		{"DOMAIN_A", "BT_TEST_01"}, {"DOMAIN_B", "BT_TEST_02"}
	};
	local.version = Edition1996;
	local.features = MakeSupportedFeatures({Block::Block1, Block::Block2, Block::Block5});

	const mms::ReadRequest req = std::get<mms::ReadRequest>(ReqOf(AssocReq).service);
	const mms::ReadResponse resp = BuildAssociationInfoResponse(req, local);
	REQUIRE(mms::Encode(mms::ConfirmedResponse{0x1234, resp}) == Hex(AssocResp));

	//another domain, another id; an unknown domain fails; the swapped order is answered in request order
	const mms::ReadResponse b = BuildAssociationInfoResponse(BuildAssociationInfoRequest("DOMAIN_B", false, false), local);
	REQUIRE(b.results[0].data.AsString() == "BT_TEST_02");
	const mms::ReadResponse unknown = BuildAssociationInfoResponse(BuildAssociationInfoRequest("DOMAIN_X"), local);
	REQUIRE_FALSE(unknown.results[0].success);
	REQUIRE(unknown.results[0].error == mms::DataAccessError::ObjectNonExistent);
	REQUIRE(unknown.results[1].success);
	const mms::ReadRequest swapped = std::get<mms::ReadRequest>(ReqOf(AssocReqSwapped).service);
	const mms::ReadResponse sw = BuildAssociationInfoResponse(swapped, local);
	REQUIRE(sw.results[1].data.type == iccp::mms::DataType::BitString);

	//objects in the wrong scope do not exist
	mms::ReadRequest wrongScope;
	wrongScope.specification = mms::ListOfVariable{mms::VariableSpec{VccName("Bilateral_Table_ID"), std::nullopt},
		                                         mms::VariableSpec{IccName("DOMAIN_A", "TASE2_Version"), std::nullopt}, mms::VariableSpec{VccName("POINT_X"), std::nullopt}
	};
	const mms::ReadResponse ws = BuildAssociationInfoResponse(wrongScope, local);
	for(const auto& r : ws.results)
		REQUIRE_FALSE(r.success);
	REQUIRE_THROWS_AS(BuildAssociationInfoResponse(mms::ReadRequest{false, IccName("DOMAIN_A", "LIST_X")}, local), DecodeError);
}

TEST_CASE(SUITE("Data flags and quality mapping"))
{
	REQUIRE(PackFlags(DataFlags{}) == 0x00);
	REQUIRE(PackFlags(DataFlags{StateValue::On, Validity::Valid, CurrentSource::Telemetered, false, false}) == 0x80);
	REQUIRE(PackFlags(DataFlags{StateValue::Off, Validity::NotValid, CurrentSource::Estimated, true, true}) == 0x7f);
	REQUIRE(PackFlags(DataFlags{StateValue::Invalid, Validity::Held, CurrentSource::Calculated, false, false}) == 0xd4);
	for(unsigned b = 0; b < 256; ++b)
		REQUIRE(PackFlags(UnpackFlags(static_cast<uint8_t>(b))) == b);
	const DataFlags f = UnpackFlags(0x0c);
	REQUIRE(f.source == CurrentSource::Estimated);
	REQUIRE(f.validity == Validity::Valid);
	REQUIRE(UnpackFlags(0x30).validity == Validity::NotValid);
	REQUIRE(UnpackFlags(0x20).validity == Validity::Suspect);
	REQUIRE(UnpackFlags(0x02).abnormal);
	REQUIRE(UnpackFlags(0x01).timeStampInvalid);
	REQUIRE(UnpackFlags(0x40).state == StateValue::Off);

	using Q = odc::QualityFlags;
	REQUIRE(ToQuality(UnpackFlags(0x00)) == Q::ONLINE);
	REQUIRE(ToQuality(UnpackFlags(0x10)) == (Q::ONLINE | Q::COMM_LOST));
	REQUIRE(ToQuality(UnpackFlags(0x20)) == (Q::ONLINE | Q::REFERENCE_ERR));
	REQUIRE(ToQuality(UnpackFlags(0x30)) == Q::COMM_LOST);
	REQUIRE(ToQuality(UnpackFlags(0x08)) == (Q::ONLINE | Q::LOCAL_FORCED));
	REQUIRE(ToQuality(UnpackFlags(0x0c)) == (Q::ONLINE | Q::REMOTE_FORCED));
	REQUIRE(ToQuality(UnpackFlags(0x03)) == Q::ONLINE); //abnormal and time stamp quality are dropped
	//state points
	REQUIRE(ToQuality(UnpackFlags(0xc0), true) == Q::COMM_LOST);
	REQUIRE(ToQuality(UnpackFlags(0x00), true) == (Q::ONLINE | Q::DISCONTINUITY));
	REQUIRE(ToQuality(UnpackFlags(0x80), true) == Q::ONLINE);
	REQUIRE(ToQuality(UnpackFlags(0xc0), false) == Q::ONLINE);

	//and back
	REQUIRE(PackFlags(FromQuality(Q::ONLINE)) == 0x00);
	REQUIRE(PackFlags(FromQuality(Q::NONE)) == 0x30);
	REQUIRE(PackFlags(FromQuality(Q::COMM_LOST)) == 0x30);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::COMM_LOST)) == 0x10);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::REFERENCE_ERR)) == 0x20);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::LOCAL_FORCED)) == 0x08);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::REMOTE_FORCED)) == 0x0c);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::LOCAL_FORCED | Q::REMOTE_FORCED)) == 0x08);
	REQUIRE(PackFlags(FromQuality(Q::ONLINE | Q::OVERRANGE | Q::RESTART | Q::ROLLOVER | Q::CHATTER_FILTER)) == 0x00);
	//state bits come from the base
	REQUIRE(PackFlags(FromQuality(Q::ONLINE, DataFlags{StateValue::On, Validity::Valid, CurrentSource::Telemetered, false, false})) == 0x80);
	REQUIRE(StateToBinary(StateValue::On));
	REQUIRE_FALSE(StateToBinary(StateValue::Off));
	REQUIRE_FALSE(StateToBinary(StateValue::Between));
	REQUIRE(BinaryToState(true) == StateValue::On);
	REQUIRE(BinaryToState(false) == StateValue::Off);
}

TEST_CASE(SUITE("Point values: every type encodes, decodes and is recognised"))
{
	for(const auto& f : PointFixtures)
	{
		INFO("type " << static_cast<int>(f.type));
		const Bytes wire = Hex(f.hex);
		const Data d = iccp::mms::DecodeData(wire);
		const PointValue p = ParsePoint(d, f.type);
		REQUIRE(p.type == f.type);
		REQUIRE(iccp::mms::EncodeData(ToData(p)) == wire);

		//built from scratch gives the same bytes
		const PointValue sample = SamplePoint(f.type);
		REQUIRE(iccp::mms::EncodeData(ToData(sample)) == wire);

		//shape recognition: only {integer, flags} is ambiguous (StateQTimeTag shares it with DiscreteQ)
		const auto inferred = InferPointType(d);
		REQUIRE(inferred.has_value());
		if(f.type == PointType::StateQTimeTag)
			REQUIRE(*inferred == PointType::DiscreteQ);
		else
			REQUIRE(*inferred == f.type);

		//type descriptions: a server's answer is classified back
		const mms::TypeDescription td = TypeDescriptionOf(f.type);
		const auto classified = ClassifyTypeDescription(td);
		REQUIRE(classified.has_value());
		REQUIRE(*classified == *inferred);
	}

	const PointValue rq = ParsePoint(iccp::mms::DecodeData(Hex("a2 0b 87 05 08 42 f6 e9 79 84 02 00 0c")), PointType::RealQ);
	REQUIRE(rq.real == Approx(123.456).epsilon(1e-6));
	REQUIRE(rq.flags == 0x0c);
	REQUIRE(rq.Flags().source == CurrentSource::Estimated);
	const PointValue sq = ParsePoint(iccp::mms::DecodeData(Hex("84 02 00 40")), PointType::StateQ);
	REQUIRE(sq.state == StateValue::Off);
	REQUIRE(ParsePoint(iccp::mms::DecodeData(Hex("84 02 06 80")), PointType::State).state == StateValue::On);
	const PointValue ext = ParsePoint(iccp::mms::DecodeData(Hex("a2 1a a2 0a 85 04 65 53 f1 00 85 02 00 fa 87 05 08 42 f6 e9 79 84 02 00 00 85 01 07")), PointType::RealExtended);
	REQUIRE(ext.timeStamp == 1700000000);
	REQUIRE(ext.milliseconds == 250);
	REQUIRE(ext.cov == 7);

	//wrong layouts
	REQUIRE_THROWS_AS(ParsePoint(Data::Integer(1), PointType::Real), DecodeError);
	REQUIRE_THROWS_AS(ParsePoint(Data::Float32(1), PointType::RealQ), DecodeError);
	REQUIRE_THROWS_AS(ParsePoint(Data::Structure({Data::Float32(1)}), PointType::RealQ), DecodeError);
	REQUIRE_THROWS_AS(ParsePoint(Data::Structure({Data::Float32(1), Data::Boolean(true)}), PointType::RealQ), DecodeError);
	REQUIRE_THROWS_AS(ParsePoint(iccp::mms::DecodeData(Hex("84 03 04 80 00")), PointType::StateQ), DecodeError); //12 bits
	REQUIRE_THROWS_AS(ParsePoint(iccp::mms::DecodeData(Hex("84 02 03 80")), PointType::State), DecodeError);     //5 bits
	REQUIRE_THROWS_AS(ParsePoint(iccp::mms::DecodeData(Hex("84 02 06 81")), PointType::State), DecodeError);     //padding set
	REQUIRE_THROWS_AS(ParsePoint(Data::Float32(1), PointType::Discrete), DecodeError);
	REQUIRE_THROWS_AS(ParsePoint(Data::Structure({Data::Integer(1), Data::Integer(2)}), PointType::Real), DecodeError);
	//a point that lacks required members cannot be built
	PointValue bad;
	bad.type = PointType::RealQTimeTag;
	REQUIRE_THROWS_AS(ToData(bad), std::invalid_argument);
	bad.type = PointType::DiscreteExtended;
	bad.timeStamp = 1;
	REQUIRE_THROWS_AS(ToData(bad), std::invalid_argument);
	REQUIRE_FALSE(InferPointType(Data::VisibleString("x")).has_value());
	REQUIRE_FALSE(InferPointType(Data::Structure({Data::Boolean(true)})).has_value());
	REQUIRE(IsRealType(PointType::RealQ));
	REQUIRE(IsStateType(PointType::StateExtended));
	REQUIRE(IsDiscreteType(PointType::Discrete));
	REQUIRE_FALSE(IsRealType(PointType::State));
}

TEST_CASE(SUITE("Block 1: data values, names and types"))
{
	//Get Data Value
	const mms::ReadRequest gdv = BuildGetDataValue({IccName("DOMAIN_A", "POINT_0001_MW"), VccName("POINT_0002_KV")});
	const auto& list = std::get<mms::ListOfVariable>(gdv.specification);
	REQUIRE(list.size() == 2);
	REQUIRE(list[0].name == IccName("DOMAIN_A", "POINT_0001_MW"));
	REQUIRE(list[1].name == VccName("POINT_0002_KV"));
	const Bytes wire = mms::Encode(mms::ConfirmedRequest{1, gdv});
	REQUIRE(std::get<mms::ConfirmedRequest>(mms::Decode(wire)) == mms::ConfirmedRequest{1, gdv});

	//Get Data Value Names: ICC and VCC scope, continuation
	const mms::GetNameListRequest icc = BuildGetDataValueNames(std::string("DOMAIN_A"));
	REQUIRE(icc.objectClass == mms::ObjectClass::NamedVariable);
	REQUIRE(icc.scope == mms::ObjectScope::DomainSpecific);
	REQUIRE(icc.domainId == "DOMAIN_A");
	REQUIRE_FALSE(icc.continueAfter.has_value());
	const mms::GetNameListRequest vcc = BuildGetDataValueNames(std::nullopt, std::string("POINT_0100_MW"));
	REQUIRE(vcc.scope == mms::ObjectScope::VmdSpecific);
	REQUIRE(vcc.continueAfter == "POINT_0100_MW");
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, vcc}) == Hex("a0 1e 02 02 12 34 a1 18 a0 03 80 01 00 a1 02 80 00 82 0d 504f494e545f303130305f4d57"));
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, icc}) == Hex("a0 17 02 02 12 34 a1 11 a0 03 80 01 00 a1 0a 81 08 444f4d41494e5f41"));

	//Get Data Value Type
	const mms::GetVariableAccessAttributesRequest gvt = BuildGetDataValueType(IccName("DOMAIN_A", "POINT_0001_MW"));
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, gvt}) == Hex("a0 23 02 02 12 34 a6 1d a0 1b a1 19 1a 08 444f4d41494e5f41 1a 0d 504f494e545f303030315f4d57"));
}

TEST_CASE(SUITE("Block 1: name paging, client and server"))
{
	std::vector<std::string> all;
	for(size_t i = 0; i < 2500; ++i)
		all.push_back(PointName(i));

	//the client follows continueAfter until the last page; the server pages with 939 names
	std::vector<std::string> got;
	mms::GetNameListRequest req = BuildGetDataValueNames(std::string("DOMAIN_A"));
	size_t pages = 0;
	for(;;)
	{
		const auto page = BuildNamePage(all, req, 939);
		REQUIRE(page.has_value());
		++pages;
		//through the wire
		const Bytes wire = mms::Encode(mms::ConfirmedResponse{7, *page});
		const mms::GetNameListResponse back = std::get<mms::GetNameListResponse>(std::get<mms::ConfirmedResponse>(mms::Decode(wire)).service);
		REQUIRE(back == *page);
		got.insert(got.end(), back.identifiers.begin(), back.identifiers.end());
		const auto next = NextNamePage(req, back);
		if(!next)
			break;
		req = *next;
	}
	REQUIRE(pages == 3);
	REQUIRE(got == all);

	//moreFollows is omitted when more follows and FALSE on the last page
	const auto first = BuildNamePage(all, BuildGetDataValueNames(std::nullopt), 100);
	REQUIRE_FALSE(first->moreFollows.has_value());
	REQUIRE(first->MoreFollows());
	const auto last = BuildNamePage(all, BuildGetDataValueNames(std::nullopt, all[2450]), 100);
	REQUIRE(last->identifiers.size() == 49);
	REQUIRE(last->moreFollows == false);
	//unlimited page size, the whole list
	REQUIRE(BuildNamePage(all, BuildGetDataValueNames(std::nullopt), 0)->identifiers.size() == all.size());
	//an unknown continuation, and the end of the list
	REQUIRE_FALSE(BuildNamePage(all, BuildGetDataValueNames(std::nullopt, std::string("NO_SUCH_NAME")), 10).has_value());
	const auto atEnd = BuildNamePage(all, BuildGetDataValueNames(std::nullopt, all.back()), 10);
	REQUIRE(atEnd->identifiers.empty());
	REQUIRE(atEnd->moreFollows == false);
	REQUIRE_FALSE(BuildNamePage({}, BuildGetDataValueNames(std::nullopt), 10)->MoreFollows());

	//the client stops on an empty page even if moreFollows defaults to TRUE
	mms::GetNameListResponse emptyMore;
	REQUIRE_FALSE(NextNamePage(req, emptyMore).has_value());
	mms::GetNameListResponse more;
	more.identifiers = {"A", "B"};
	REQUIRE(NextNamePage(req, more)->continueAfter == "B");
	more.moreFollows = true;
	REQUIRE(NextNamePage(req, more).has_value());
	more.moreFollows = false;
	REQUIRE_FALSE(NextNamePage(req, more).has_value());
}

TEST_CASE(SUITE("Block 1: data sets"))
{
	DataSetDefinition def;
	def.name = IccName("DOMAIN_A", "DS_TEST_001");
	def.members = {IccName("DOMAIN_A", "POINT_0001_MW"), VccName("POINT_0002_KV"), IccName("DOMAIN_B", "POINT_0003_CB")};

	const mms::DefineNamedVariableListRequest create = BuildCreateDataSet(def);
	REQUIRE(create.name == def.name);
	REQUIRE(create.variables.size() == 3);
	REQUIRE(create.variables[1].name == VccName("POINT_0002_KV"));
	const Bytes wire = mms::Encode(mms::ConfirmedRequest{5, create});
	REQUIRE(std::get<mms::ConfirmedRequest>(mms::Decode(wire)) == mms::ConfirmedRequest{5, create});

	//delete: the list of names only, no scope
	const mms::DeleteNamedVariableListRequest del = BuildDeleteDataSet(def.name);
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, del}) == Hex("a0 21 02 02 12 34 ad 1b a1 19 a1 17 1a 08 444f4d41494e5f41 1a 0b 44535f544553545f303031"));
	REQUIRE(BuildDeleteResponse(1, 1) == mms::DeleteNamedVariableListResponse{1, 1});

	//names of data sets
	const mms::GetNameListRequest names = BuildGetDataSetNames(std::string("DOMAIN_A"));
	REQUIRE(names.objectClass == mms::ObjectClass::NamedVariableList);
	REQUIRE(names.domainId == "DOMAIN_A");
	REQUIRE(BuildGetDataSetNames(std::nullopt, std::string("DS_X")).continueAfter == "DS_X");

	//element names
	const mms::GetNamedVariableListAttributesRequest en = BuildGetDataSetElementNames(def.name);
	REQUIRE(en.name == def.name);
	const mms::GetNamedVariableListAttributesResponse enr = BuildDataSetElementNamesResponse(def);
	REQUIRE(enr.mmsDeletable);
	REQUIRE(ParseDataSetElementNames(def.name, enr) == def);
	mms::GetNamedVariableListAttributesResponse withAlt = enr;
	withAlt.variables[0].alternateAccess = Hex("a5 00");
	REQUIRE_THROWS_AS(ParseDataSetElementNames(def.name, withAlt), DecodeError);

	//element values: a Read of the list name; the result count must match
	const mms::ReadRequest values = BuildGetDataSetElementValues(def.name);
	REQUIRE(std::get<ObjectName>(values.specification) == def.name);
	mms::ReadResponse resp;
	resp.results = {mms::AccessResult::Success(Data::Integer(1)), mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent),
		          mms::AccessResult::Success(Data::Integer(3))};
	REQUIRE(ParseDataSetValues(def, resp).size() == 3);
	resp.results.pop_back();
	REQUIRE_THROWS_AS(ParseDataSetValues(def, resp), DecodeError);
}

TEST_CASE(SUITE("Block 2: DSConditions"))
{
	REQUIRE(iccp::mms::EncodeData(ToData(DsConditions{DsCondition::IntervalTimeOut | DsCondition::OperatorRequest | DsCondition::OtherExternalEvent, 5})) == Hex("84 02 03 98"));
	REQUIRE(iccp::mms::EncodeData(ToData(DsConditions{DsCondition::IntegrityTimeOut, 5})) == Hex("84 02 03 40"));
	REQUIRE(iccp::mms::EncodeData(ToData(DsConditions{0, 5})) == Hex("84 02 03 00"));
	REQUIRE(iccp::mms::EncodeData(ToData(DsConditions{0, 0})) == Hex("84 01 00"));

	const DsConditions c = ParseDsConditions(iccp::mms::DecodeData(Hex("84 02 03 48")));
	REQUIRE(c.mask == (DsCondition::IntegrityTimeOut | DsCondition::OtherExternalEvent));
	REQUIRE(c.bitCount == 5);
	REQUIRE(c.Has(DsCondition::IntegrityTimeOut));
	REQUIRE_FALSE(c.Has(DsCondition::IntervalTimeOut));
	REQUIRE(ParseDsConditions(iccp::mms::DecodeData(Hex("84 01 00"))).bitCount == 0);
	//the bit count survives so that other lengths rebuild identically
	REQUIRE(iccp::mms::EncodeData(ToData(ParseDsConditions(iccp::mms::DecodeData(Hex("84 02 00 f0"))))) == Hex("84 02 00 f0"));

	REQUIRE_THROWS_AS(ParseDsConditions(Data::Integer(1)), DecodeError);
	REQUIRE_THROWS_AS(ParseDsConditions(iccp::mms::DecodeData(Hex("84 03 00 ff ff"))), DecodeError); //16 bits
	REQUIRE_THROWS_AS(ParseDsConditions(iccp::mms::DecodeData(Hex("84 02 03 49"))), DecodeError);    //padding set
	REQUIRE_THROWS_AS(ToData(DsConditions{0x20, 5}), std::invalid_argument);
	REQUIRE_THROWS_AS(ToData(DsConditions{0, 9}), std::invalid_argument);
}

TEST_CASE(SUITE("Block 2: DSTransferSet, start and stop"))
{
	DsTransferSet t;
	t.dataSetName = ObjectRef{Scope::Icc, "DOMAIN_A", "DS_TEST_001"};
	t.interval = 5;
	t.integrityCheck = 600;
	t.conditionsRequested = DsConditions{DsCondition::IntervalTimeOut | DsCondition::IntegrityTimeOut | DsCondition::OtherExternalEvent, 5};
	t.rbe = true;

	const ObjectName tsName = IccName("DOMAIN_A", "TS_TEST_01");
	const mms::WriteRequest start = BuildStartTransfer(tsName, t);
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, start}) == Hex(Write12));

	//12 members: the shape seen from 1996-08 peers, whose fourth boolean is Status (TRUE: start)
	const TransferSetWrite parsed = ParseTransferSetWrite(std::get<mms::WriteRequest>(ReqOf(Write12).service));
	REQUIRE(parsed.transferSet == tsName);
	REQUIRE(parsed.parameters == t);
	REQUIRE(parsed.parameters.status);
	REQUIRE_FALSE(parsed.parameters.allChangesMember);
	REQUIRE_FALSE(parsed.parameters.allChangesReported);
	REQUIRE(parsed.parameters.dataSetName.name == "DS_TEST_001");
	REQUIRE(parsed.parameters.integrityCheck == 600);
	REQUIRE(parsed.parameters.conditionsRequested.Has(DsCondition::IntegrityTimeOut));
	REQUIRE(parsed.parameters.rbe);
	REQUIRE_FALSE(parsed.parameters.blockData);
	REQUIRE_FALSE(parsed.parameters.critical);

	//Stop in the 12 member form: Status FALSE
	DsTransferSet stop12 = t;
	stop12.status = false;
	REQUIRE(BuildStopTransfer(tsName, t).data.at(0) == ToData(stop12));
	REQUIRE(ToData(stop12).elements.size() == 12);
	REQUIRE(ParseDsTransferSet(ToData(stop12)) == stop12);

	//13 members, with AllChangesReported before Status (the 2000-08 form), used for Stop
	DsTransferSet t13 = t;
	t13.allChangesMember = true;
	t13.allChangesReported = true;
	const mms::WriteRequest stop = BuildStopTransfer(tsName, t13);
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, stop}) == Hex(Write13Stop));
	const TransferSetWrite stopParsed = ParseTransferSetWrite(std::get<mms::WriteRequest>(ReqOf(Write13Stop).service));
	REQUIRE(stopParsed.parameters.status == false);
	REQUIRE(stopParsed.parameters.allChangesMember);
	REQUIRE(stopParsed.parameters.allChangesReported);
	DsTransferSet withStatus = t13;
	withStatus.status = false;
	REQUIRE(stopParsed.parameters == withStatus);
	withStatus.status = true;
	REQUIRE(ParseDsTransferSet(ToData(withStatus)) == withStatus);

	REQUIRE(BuildWriteSuccess().results == std::vector<mms::WriteResult>{mms::WriteResult{true, 0}
		});
	REQUIRE(BuildWriteSuccess(3).results.size() == 3);

	//every field survives
	DsTransferSet all;
	all.dataSetName = ObjectRef{Scope::Vcc, "", "DS_X"};
	all.startTime = 1700000000;
	all.interval = 30;
	all.tle = 3600;
	all.bufferTime = 2;
	all.integrityCheck = 900;
	all.conditionsRequested = DsConditions{DsCondition::ObjectChange | DsCondition::OtherExternalEvent, 5};
	all.blockData = true;
	all.critical = true;
	all.rbe = true;
	all.allChangesMember = true;
	all.allChangesReported = true;
	all.status = true;
	all.eventCodeRequested = 12;
	REQUIRE(ParseDsTransferSet(iccp::mms::DecodeData(iccp::mms::EncodeData(ToData(all)))) == all);

	//malformed
	REQUIRE_THROWS_AS(ParseDsTransferSet(Data::Integer(1)), DecodeError);
	Data d = ToData(t);
	d.elements.pop_back();
	REQUIRE_THROWS_AS(ParseDsTransferSet(d), DecodeError); //11 members
	d = ToData(t);
	d.elements.push_back(Data::Integer(0));
	d.elements.push_back(Data::Integer(0));
	REQUIRE_THROWS_AS(ParseDsTransferSet(d), DecodeError); //14 members
	for(size_t i = 0; i < 12; ++i)
	{
		d = ToData(t);
		d.elements[i] = (i == 0) ? Data::Integer(1) : Data::VisibleString("x");
		REQUIRE_THROWS_AS(ParseDsTransferSet(d), DecodeError);
	}
	d = ToData(t);
	d.elements[7] = Data::Integer(1); //BlockData is not a boolean
	REQUIRE_THROWS_AS(ParseDsTransferSet(d), DecodeError);
	//a write of the wrong shape
	mms::WriteRequest w = start;
	w.data.push_back(Data::Integer(1));
	REQUIRE_THROWS_AS(ParseTransferSetWrite(w), DecodeError);
	w = start;
	w.specification = tsName;
	REQUIRE_THROWS_AS(ParseTransferSetWrite(w), DecodeError);
	w = start;
	std::get<mms::ListOfVariable>(w.specification).push_back(mms::VariableSpec{tsName, std::nullopt});
	REQUIRE_THROWS_AS(ParseTransferSetWrite(w), DecodeError);
}

TEST_CASE(SUITE("Block 2: Next_DSTransfer_Set"))
{
	const mms::ReadRequest req = BuildGetNextDsTransferSet("DOMAIN_A");
	REQUIRE(mms::Encode(mms::ConfirmedRequest{0x1234, req}) == Hex(NextReq));
	REQUIRE(std::get<mms::ReadRequest>(ReqOf(NextReq).service) == req);

	const ObjectRef next = ParseNextDsTransferSet(std::get<mms::ReadResponse>(RespOf(NextResp).service));
	REQUIRE(next == ObjectRef{Scope::Icc, "DOMAIN_A", "TS_TEST_01"});
	REQUIRE(mms::Encode(mms::ConfirmedResponse{0x1234, BuildNextDsTransferSetResponse(next)}) == Hex(NextResp));

	//no transfer set free
	const mms::ReadResponse none = BuildNextDsTransferSetResponse(std::nullopt);
	REQUIRE_FALSE(none.results[0].success);
	REQUIRE_THROWS_AS(ParseNextDsTransferSet(none), DecodeError);
	mms::ReadResponse two = none;
	two.results.push_back(none.results[0]);
	REQUIRE_THROWS_AS(ParseNextDsTransferSet(two), DecodeError);
	mms::ReadResponse wrong;
	wrong.results = {mms::AccessResult::Success(Data::VisibleString("x"))};
	REQUIRE_THROWS_AS(ParseNextDsTransferSet(wrong), DecodeError);
}

TEST_CASE(SUITE("Block 2: transfer set reports"))
{
	const mms::InformationReport rep = std::get<mms::InformationReport>(mms::Decode(Hex(Report)));
	const std::vector<ObjectName> members = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", Name::TransferSetTimeStamp),
		                                   IccName("DOMAIN_A", Name::DsConditionsDetected), IccName("DOMAIN_A", Name::EventCodeDetected),
		                                   IccName("DOMAIN_A", "POINT_0001_MW"), IccName("DOMAIN_A", "POINT_0002_CB"), IccName("DOMAIN_A", "POINT_0003_CB"), VccName("POINT_0004_CB")};

	const TransferReport r = ParseTransferReport(rep, &members);
	REQUIRE(*r.DataSetName() == IccName("DOMAIN_A", "DS_TEST_001"));
	REQUIRE(r.layout == std::vector<Special>{Special::TransferSetName, Special::TimeStamp, Special::ConditionsDetected, Special::EventCode});
	REQUIRE(r.transferSet == ObjectRef{Scope::Icc, "DOMAIN_A", "TS_TEST_01"});
	REQUIRE(r.timeStamp == 1700000000);
	REQUIRE(r.conditionsDetected->Has(DsCondition::IntegrityTimeOut));
	REQUIRE(r.conditionsDetected->Has(DsCondition::OtherExternalEvent));
	REQUIRE(r.eventCode == 0);
	REQUIRE(r.values.size() == 4);
	REQUIRE(ParsePoint(r.values[0].data, PointType::RealQ).real == Approx(123.456).epsilon(1e-6));
	REQUIRE(ParsePoint(r.values[1].data, PointType::StateQ).state == StateValue::On);
	REQUIRE_FALSE(r.values[2].success); //a value that failed is carried
	REQUIRE(r.values[2].error == mms::DataAccessError::ObjectNonExistent);
	REQUIRE(ParsePoint(r.values[3].data, PointType::StateQ).flags == 0x42);
	REQUIRE(mms::Encode(BuildTransferReport(r)) == Hex(Report));

	//without the definition only the transfer set name is recognised; everything else is a value
	const TransferReport g = ParseTransferReport(rep);
	REQUIRE(g.layout == std::vector<Special>{Special::TransferSetName});
	REQUIRE(g.values.size() == 7);
	REQUIRE_FALSE(g.timeStamp.has_value());
	REQUIRE(mms::Encode(BuildTransferReport(g)) == Hex(Report));

	//the minimal report
	const mms::InformationReport minRep = std::get<mms::InformationReport>(mms::Decode(Hex(ReportMin)));
	const TransferReport m = ParseTransferReport(minRep);
	REQUIRE(m.values.size() == 1);
	REQUIRE(ParsePoint(m.values[0].data, PointType::RealQ).real == -2.5);
	REQUIRE(mms::Encode(BuildTransferReport(m)) == Hex(ReportMin));
	const std::vector<ObjectName> minMembers = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", "POINT_0001_MW")};
	REQUIRE(ParseTransferReport(minRep, &minMembers) == m);

	//built from scratch
	TransferReport b;
	b.specification = IccName("DOMAIN_A", "DS_TEST_001");
	b.layout = {Special::TransferSetName, Special::TimeStamp, Special::ConditionsDetected, Special::EventCode};
	b.transferSet = ObjectRef{Scope::Icc, "DOMAIN_A", "TS_TEST_01"};
	b.timeStamp = 1700000000;
	b.conditionsDetected = DsConditions{DsCondition::IntegrityTimeOut | DsCondition::OtherExternalEvent, 5};
	b.eventCode = 0;
	PointValue real;
	real.type = PointType::RealQ;
	real.real = 123.456f;
	real.flags = 0x30;
	PointValue state;
	state.type = PointType::StateQ;
	state.flags = 0x80;
	PointValue state2 = state;
	state2.flags = 0x42;
	b.values = {mms::AccessResult::Success(ToData(real)), mms::AccessResult::Success(ToData(state)),
		      mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent), mms::AccessResult::Success(ToData(state2))};
	REQUIRE(mms::Encode(BuildTransferReport(b)) == Hex(Report));
	REQUIRE(b == r);

	//the specials are only taken from the head of the definition, and a special may appear once
	const std::vector<ObjectName> late = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", "POINT_0001_MW"),
		                                IccName("DOMAIN_A", Name::EventCodeDetected), IccName("DOMAIN_A", "POINT_0002_CB"), IccName("DOMAIN_A", "POINT_0003_CB"),
		                                IccName("DOMAIN_A", "POINT_0004_CB"), IccName("DOMAIN_A", "POINT_0005_CB"), IccName("DOMAIN_A", "POINT_0006_CB")};
	const TransferReport l = ParseTransferReport(rep, &late);
	REQUIRE(l.layout.size() == 1);
	REQUIRE(l.values.size() == 7);
}

TEST_CASE(SUITE("Block 2: reports that list their variables inline"))
{
	//some peers send reports with a listOfVariable naming every member instead of the data set name
	const std::vector<ObjectName> members = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", Name::TransferSetTimeStamp),
		                                   IccName("DOMAIN_A", Name::DsConditionsDetected), IccName("DOMAIN_A", Name::EventCodeDetected),
		                                   IccName("DOMAIN_A", "POINT_0001_MW"), IccName("DOMAIN_A", "POINT_0002_CB"), IccName("DOMAIN_A", "POINT_0003_CB"), VccName("POINT_0004_CB")};
	mms::InformationReport rep = std::get<mms::InformationReport>(mms::Decode(Hex(Report)));
	mms::ListOfVariable list;
	for(const auto& m : members)
		list.push_back(mms::VariableSpec{m, std::nullopt});
	rep.specification = list;
	const Bytes wire = mms::Encode(rep);
	const mms::InformationReport back = std::get<mms::InformationReport>(mms::Decode(wire));

	const TransferReport r = ParseTransferReport(back);
	REQUIRE(r.DataSetName() == nullptr);
	REQUIRE(r.layout == std::vector<Special>{Special::TransferSetName, Special::TimeStamp, Special::ConditionsDetected, Special::EventCode});
	REQUIRE(r.timeStamp == 1700000000);
	REQUIRE(r.values.size() == 4);
	REQUIRE(mms::Encode(BuildTransferReport(r)) == wire);
	//a definition passed in is not needed (and not used)
	const std::vector<ObjectName> unrelated(8, VccName("X"));
	REQUIRE(ParseTransferReport(back, &unrelated) == r);

	//size mismatch, alternate access
	mms::InformationReport shortList = back;
	std::get<mms::ListOfVariable>(shortList.specification).pop_back();
	REQUIRE_THROWS_AS(ParseTransferReport(shortList), DecodeError);
	mms::InformationReport alt = back;
	std::get<mms::ListOfVariable>(alt.specification)[4].alternateAccess = Hex("a5 00");
	REQUIRE_THROWS_AS(ParseTransferReport(alt), DecodeError);
}

TEST_CASE(SUITE("Block 2: reports that are not transfer set reports"))
{
	const mms::InformationReport rep = std::get<mms::InformationReport>(mms::Decode(Hex(Report)));
	const std::vector<ObjectName> members(8, IccName("DOMAIN_A", "X"));
	REQUIRE_THROWS_AS(ParseTransferReport(rep, &members), DecodeError); //no Transfer_Set_Name in the definition
	const std::vector<ObjectName> wrongSize = {IccName("DOMAIN_A", Name::TransferSetName)};
	REQUIRE_THROWS_AS(ParseTransferReport(rep, &wrongSize), DecodeError); //size mismatch

	mms::InformationReport r = rep;
	r.specification = mms::ListOfVariable{};
	REQUIRE_THROWS_AS(ParseTransferReport(r), DecodeError); //not a named data set
	r = rep;
	r.results.clear();
	REQUIRE_THROWS_AS(ParseTransferReport(r), DecodeError);
	r = rep;
	r.results[0] = mms::AccessResult::Success(Data::Integer(1));
	REQUIRE_THROWS_AS(ParseTransferReport(r), DecodeError); //first is not the name
	r = rep;
	r.results[0] = mms::AccessResult::Failure(5);
	REQUIRE_THROWS_AS(ParseTransferReport(r), DecodeError); //the name failed

	//specials of the wrong type
	const std::vector<ObjectName> members2 = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", Name::TransferSetTimeStamp),
		                                    IccName("DOMAIN_A", Name::DsConditionsDetected), IccName("DOMAIN_A", Name::EventCodeDetected), VccName("A"), VccName("B"), VccName("C"), VccName("D")};
	r = rep;
	r.results[1] = mms::AccessResult::Success(Data::VisibleString("x"));
	REQUIRE_THROWS_AS(ParseTransferReport(r, &members2), DecodeError);
	r = rep;
	r.results[2] = mms::AccessResult::Success(Data::Integer(1));
	REQUIRE_THROWS_AS(ParseTransferReport(r, &members2), DecodeError);
	r = rep;
	r.results[3] = mms::AccessResult::Success(Data::VisibleString("x"));
	REQUIRE_THROWS_AS(ParseTransferReport(r, &members2), DecodeError);
	r = rep;
	r.results[3] = mms::AccessResult::Failure(1);
	REQUIRE_THROWS_AS(ParseTransferReport(r, &members2), DecodeError);

	//building
	TransferReport b;
	b.layout.clear();
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TimeStamp};
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TransferSetName, Special::TransferSetName};
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TransferSetName, Special::TimeStamp};
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TransferSetName, Special::ConditionsDetected};
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TransferSetName, Special::EventCode};
	REQUIRE_THROWS_AS(BuildTransferReport(b), std::invalid_argument);
	b.layout = {Special::TransferSetName};
	REQUIRE_NOTHROW(BuildTransferReport(b));
}

TEST_CASE(SUITE("Block 2: reports of hundreds of values"))
{
	std::vector<ObjectName> members = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", Name::EventCodeDetected)};
	TransferReport rep;
	rep.specification = IccName("DOMAIN_A", "DS_TEST_002");
	rep.layout = {Special::TransferSetName, Special::EventCode};
	rep.transferSet = ObjectRef{Scope::Icc, "DOMAIN_A", "TS_TEST_02"};
	rep.eventCode = 4;
	for(size_t i = 0; i < 885; ++i)
	{
		members.push_back(IccName("DOMAIN_A", PointName(i)));
		PointValue p;
		p.type = PointType::RealQ;
		p.real = static_cast<double>(i) * 0.5;
		p.flags = static_cast<uint8_t>((i % 4) << 4);
		rep.values.push_back(mms::AccessResult::Success(ToData(p)));
	}
	for(size_t i = 0; i < 777; ++i)
	{
		members.push_back(IccName("DOMAIN_A", PointName(i, "CB")));
		PointValue p;
		p.type = PointType::StateQ;
		p.flags = static_cast<uint8_t>(((i % 3) + 1) << 6);
		rep.values.push_back(mms::AccessResult::Success(ToData(p)));
	}
	const mms::InformationReport built = BuildTransferReport(rep);
	const Bytes wire = mms::Encode(built);
	REQUIRE(wire.size() > 14000);

	const mms::InformationReport back = std::get<mms::InformationReport>(mms::Decode(wire));
	const TransferReport parsed = ParseTransferReport(back, &members);
	REQUIRE(parsed == rep);
	REQUIRE(parsed.values.size() == 885 + 777);
	REQUIRE(mms::Encode(BuildTransferReport(parsed)) == wire);
	for(size_t i = 0; i < parsed.values.size(); ++i)
	{
		const auto type = InferPointType(parsed.values[i].data);
		REQUIRE(type.has_value());
		REQUIRE(*type == (i < 885 ? PointType::RealQ : PointType::StateQ));
		REQUIRE(ToData(ParsePoint(parsed.values[i].data, *type)) == parsed.values[i].data);
	}
	REQUIRE(ParsePoint(parsed.values[3].data, PointType::RealQ).real == 1.5);
	REQUIRE(ParsePoint(parsed.values[885].data, PointType::StateQ).state == StateValue::Off);
}

TEST_CASE(SUITE("Truncation and mutation of TASE.2 messages"))
{
	std::vector<Bytes> originals;
	for(const char* f : {AssocReq, AssocReqSwapped, AssocResp, NextReq, NextResp, Write12, Write13Stop, Report, ReportMin})
		originals.push_back(Hex(f));

	//every strict prefix is rejected by the MMS layer; nothing but DecodeError escapes
	for(const Bytes& wire : originals)
		for(size_t n = 0; n < wire.size(); ++n)
			REQUIRE_THROWS_AS(mms::Decode(Bytes(wire.begin(), wire.begin() + n)), DecodeError);

	uint32_t state = 77;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};
	const std::vector<ObjectName> members = {IccName("DOMAIN_A", Name::TransferSetName), IccName("DOMAIN_A", Name::TransferSetTimeStamp),
		                                   IccName("DOMAIN_A", Name::DsConditionsDetected), IccName("DOMAIN_A", Name::EventCodeDetected), VccName("A"), VccName("B"), VccName("C"), VccName("D")};
	size_t parsedOk = 0;
	for(int iter = 0; iter < 60000; ++iter)
	{
		Bytes m = originals[next() % originals.size()];
		const unsigned changes = 1 + next() % 3;
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		try
		{
			const mms::Pdu pdu = mms::Decode(m);
			if(const auto* req = std::get_if<mms::ConfirmedRequest>(&pdu))
			{
				if(const auto* w = std::get_if<mms::WriteRequest>(&req->service))
				{
					const TransferSetWrite p = ParseTransferSetWrite(*w);
					++parsedOk;
					const mms::WriteRequest again = BuildStartTransfer(p.transferSet, p.parameters);
					REQUIRE(ParseTransferSetWrite(again) == p);
				}
			}
			else if(const auto* rsp = std::get_if<mms::ConfirmedResponse>(&pdu))
			{
				if(const auto* r = std::get_if<mms::ReadResponse>(&rsp->service))
				{
					const mms::ReadRequest request = BuildAssociationInfoRequest("DOMAIN_A");
					const AssociationInfo info = ParseAssociationInfoResponse(request, *r);
					++parsedOk;
					(void)info;
				}
			}
			else if(const auto* ir = std::get_if<mms::InformationReport>(&pdu))
			{
				const TransferReport tr = ParseTransferReport(*ir, &members);
				++parsedOk;
				REQUIRE(ParseTransferReport(BuildTransferReport(tr), &members) == tr);
			}
		}
		catch(const DecodeError&)
		{}
	}
	REQUIRE(parsedOk > 0);
}

//Optional regression against externally supplied data: ICCP_TASE2_CORPUS names a text file with one MMS PDU per line
//as hex. Every TASE.2 object and report in it must parse into the semantic structs and rebuild to byte-identical
//MMS. Only counts are reported; nothing from the file is printed.
TEST_CASE(SUITE("TASE.2 objects and reports from a corpus file (optional)"))
{
	std::string path;
	#ifdef _MSC_VER
	char* env = nullptr;
	size_t envLen = 0;
	if(_dupenv_s(&env, &envLen, "ICCP_TASE2_CORPUS") == 0 && env != nullptr)
	{
		path = env;
		free(env);
	}
	#else
	if(const char* env = std::getenv("ICCP_TASE2_CORPUS"))
		path = env;
	#endif
	if(path.empty())
		return;

	std::ifstream in(path);
	REQUIRE(in.good());
	std::vector<mms::Pdu> pdus;
	std::vector<Bytes> wires;
	std::string line;
	while(std::getline(in, line))
	{
		const Bytes wire = Hex(line);
		if(wire.empty())
			continue;
		pdus.push_back(mms::Decode(wire));
		wires.push_back(wire);
	}

	auto key = [](const ObjectName& n)
		     {
			     return std::string(1, static_cast<char>('0' + static_cast<int>(n.scope))) + "/" + n.domainId + "/" + n.itemId;
		     };

	//pass 1: read requests by invoke id, data set definitions by name
	std::map<uint32_t, std::vector<mms::ReadRequest>> reads;
	std::map<std::string, std::vector<std::vector<ObjectName>>> definitions;
	for(const auto& pdu : pdus)
	{
		const auto* req = std::get_if<mms::ConfirmedRequest>(&pdu);
		if(req == nullptr)
			continue;
		if(const auto* rd = std::get_if<mms::ReadRequest>(&req->service))
			reads[req->invokeId].push_back(*rd);
		else if(const auto* def = std::get_if<mms::DefineNamedVariableListRequest>(&req->service))
		{
			std::vector<ObjectName> m;
			for(const auto& v : def->variables)
				m.push_back(v.name);
			definitions[key(def->name)].push_back(std::move(m));
		}
	}

	size_t associationReads = 0, nextReads = 0, startWrites = 0, dataSets = 0, reports = 0, points = 0, unclassified = 0, failures = 0;
	size_t parseFailures = 0, reportFailures = 0, pointFailures = 0;
	for(size_t i = 0; i < pdus.size(); ++i)
	{
		const Bytes& wire = wires[i];
		try
		{
			if(const auto* req = std::get_if<mms::ConfirmedRequest>(&pdus[i]))
			{
				if(const auto* w = std::get_if<mms::WriteRequest>(&req->service))
				{
					const TransferSetWrite p = ParseTransferSetWrite(*w);
					++startWrites;
					if(mms::Encode(mms::ConfirmedRequest{req->invokeId, BuildStartTransfer(p.transferSet, p.parameters)}) != wire)
						++failures;
				}
				else if(const auto* def = std::get_if<mms::DefineNamedVariableListRequest>(&req->service))
				{
					DataSetDefinition d;
					d.name = def->name;
					for(const auto& v : def->variables)
						d.members.push_back(v.name);
					++dataSets;
					if(mms::Encode(mms::ConfirmedRequest{req->invokeId, BuildCreateDataSet(d)}) != wire)
						++failures;
				}
				else if(const auto* rd = std::get_if<mms::ReadRequest>(&req->service))
				{
					if(IsAssociationInfoRequest(*rd))
						++associationReads;
					const auto* list = std::get_if<mms::ListOfVariable>(&rd->specification);
					if(list != nullptr && list->size() == 1 && (*list)[0].name.itemId == Name::NextDsTransferSet)
					{
						++nextReads;
						if(mms::Encode(mms::ConfirmedRequest{req->invokeId, BuildGetNextDsTransferSet((*list)[0].name.domainId)}) != wire)
							++failures;
					}
				}
			}
			else if(const auto* rsp = std::get_if<mms::ConfirmedResponse>(&pdus[i]))
			{
				const auto* rr = std::get_if<mms::ReadResponse>(&rsp->service);
				if(rr == nullptr || reads.count(rsp->invokeId) == 0)
					continue;
				for(const auto& request : reads[rsp->invokeId])
				{
					const auto* list = std::get_if<mms::ListOfVariable>(&request.specification);
					if(list == nullptr || list->size() != rr->results.size())
						continue;
					if(IsAssociationInfoRequest(request))
					{
						const AssociationInfo info = ParseAssociationInfoResponse(request, *rr);
						mms::ReadResponse rebuilt;
						for(const auto& v : *list)
						{
							const std::string& item = v.name.itemId;
							if(info.failures.count(item))
								rebuilt.results.push_back(mms::AccessResult::Failure(info.failures.at(item)));
							else if(item == Name::BilateralTableId)
								rebuilt.results.push_back(mms::AccessResult::Success(Data::VisibleString(*info.bilateralTableId)));
							else if(item == Name::Tase2Version)
								rebuilt.results.push_back(mms::AccessResult::Success(ToData(*info.version)));
							else
								rebuilt.results.push_back(mms::AccessResult::Success(ToData(*info.features)));
						}
						if(mms::Encode(mms::ConfirmedResponse{rsp->invokeId, rebuilt}) != wire)
							++failures;
						break;
					}
					if(list->size() == 1 && (*list)[0].name.itemId == Name::NextDsTransferSet)
					{
						const ObjectRef next = ParseNextDsTransferSet(*rr);
						if(mms::Encode(mms::ConfirmedResponse{rsp->invokeId, BuildNextDsTransferSetResponse(next)}) != wire)
							++failures;
						break;
					}
				}
			}
			else if(const auto* ir = std::get_if<mms::InformationReport>(&pdus[i]))
			{
				const auto* name = std::get_if<ObjectName>(&ir->specification);
				std::optional<TransferReport> parsed;
				if(name != nullptr && definitions.count(key(*name)))
					for(const auto& candidate : definitions[key(*name)])
					{
						if(candidate.size() != ir->results.size())
							continue;
						try
						{
							parsed = ParseTransferReport(*ir, &candidate);
							break;
						}
						catch(const DecodeError&)
						{}
					}
				if(!parsed)
					parsed = ParseTransferReport(*ir);
				++reports;
				if(mms::Encode(BuildTransferReport(*parsed)) != wire)
				{
					++failures;
					++reportFailures;
				}
				for(const auto& v : parsed->values)
				{
					if(!v.success)
						continue;
					const auto type = InferPointType(v.data);
					if(!type)
					{
						++unclassified;
						continue;
					}
					++points;
					if(!(ToData(ParsePoint(v.data, *type)) == v.data))
					{
						++failures;
						++pointFailures;
					}
				}
			}
		}
		catch(const DecodeError&)
		{
			++failures;
			++parseFailures;
		}
	}
	INFO("pdus " << pdus.size() << ", association reads " << associationReads << ", next-transfer-set reads " << nextReads <<
		      ", transfer set writes " << startWrites << ", data sets " << dataSets << ", reports " << reports << ", points " << points <<
		      ", unclassified values " << unclassified << ", failures " << failures << " (parse " << parseFailures << ", report rebuild " <<
		      reportFailures << ", point rebuild " << pointFailures << ")");
	REQUIRE(reports > 0);
	REQUIRE(failures == 0);
}

