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
 * PointConversionTest.cpp
 *
 *  Conversion between ODC point values and TASE.2 point values (Tase2Points.h): the four states of a State point and the 32 bit Discrete value.
 */

#include "Tase2Points.h"
#include <catch.hpp>
#include <cmath>
#include <limits>

#define SUITE(name) "ICCP points - " name

using namespace iccp::tase2;
using odc::QualityFlags;

namespace
{
IccpPointConf Point(PointKind kind, PointType type, double scale = 1.0, double offset = 0.0)
{
	IccpPointConf pt;
	pt.kind = kind;
	pt.type = type;
	pt.scale = scale;
	pt.offset = offset;
	return pt;
}

PointState Known(QualityFlags q = QualityFlags::ONLINE)
{
	PointState s;
	s.known = true;
	s.quality = q;
	s.timestamp = 1700000000000;
	return s;
}

constexpr StateValue AllStates[] = {StateValue::Between, StateValue::Off, StateValue::On, StateValue::Invalid};
}

TEST_CASE(SUITE("an ODC double bit is the TASE.2 state: 00 Between, 01 Off, 10 On, 11 Invalid"))
{
	CHECK(DoubleBitToState(false, false) == StateValue::Between);
	CHECK(DoubleBitToState(false, true) == StateValue::Off);
	CHECK(DoubleBitToState(true, false) == StateValue::On);
	CHECK(DoubleBitToState(true, true) == StateValue::Invalid);
	for(const StateValue s : AllStates)
	{
		const auto bits = StateToDoubleBit(s);
		CHECK(DoubleBitToState(bits.first, bits.second) == s);
		//the same two bits as the state has on the wire (the top two bits of the flags octet)
		CHECK(static_cast<uint8_t>(s) == ((bits.first ? 2 : 0) + (bits.second ? 1 : 0)));
	}
}

TEST_CASE(SUITE("a DoubleBinary point keeps all four states in every State type"))
{
	for(const PointType type : {PointType::State, PointType::StateQ, PointType::StateQTimeTag, PointType::StateExtended})
		for(const StateValue state : AllStates)
		{
			CAPTURE(static_cast<int>(type));
			CAPTURE(static_cast<int>(state));
			const IccpPointConf pt = Point(PointKind::DoubleBinary, type);
			PointState s = Known();
			s.state = state;
			const Data wire = ToData(pt, s, s.timestamp);
			const PointState back = FromData(pt, wire, 0);
			CHECK(back.state == state);
			CHECK(back.binary == (state == StateValue::On));
			CHECK(NumericValue(pt, back) == static_cast<double>(static_cast<uint8_t>(state)));
		}
}

TEST_CASE(SUITE("a Binary point still collapses the state to a bool"))
{
	const IccpPointConf pt = Point(PointKind::Binary, PointType::StateQ);
	PointState s = Known();
	s.binary = true;
	CHECK(FromData(pt, ToData(pt, s, 0), 0).state == StateValue::On);
	s.binary = false;
	CHECK(FromData(pt, ToData(pt, s, 0), 0).state == StateValue::Off);
}

TEST_CASE(SUITE("a DoubleBinary point that is not online, or has no value yet, is Invalid on the wire"))
{
	const IccpPointConf pt = Point(PointKind::DoubleBinary, PointType::StateQ);
	PointState s = Known(QualityFlags::COMM_LOST);
	s.state = StateValue::On;
	CHECK(ToPointValue(pt, s, 0).state == StateValue::Invalid);
	CHECK(ToPointValue(pt, PointState(), 0).state == StateValue::Invalid);
	s.quality = QualityFlags::ONLINE;
	CHECK(ToPointValue(pt, s, 0).state == StateValue::On);
}

TEST_CASE(SUITE("a received Between state is flagged with the quality DISCONTINUITY, an Invalid one is not online"))
{
	const IccpPointConf pt = Point(PointKind::DoubleBinary, PointType::StateQ);
	PointState s = Known();
	s.state = StateValue::Between;
	CHECK(FromData(pt, ToData(pt, s, 0), 0).quality == (QualityFlags::ONLINE | QualityFlags::DISCONTINUITY));
	s.state = StateValue::Invalid;
	CHECK((FromData(pt, ToData(pt, s, 0), 0).quality & QualityFlags::ONLINE) != QualityFlags::ONLINE);
}

TEST_CASE(SUITE("ScaledDiscrete: an integer of width 32, signed, never clamped"))
{
	const IccpPointConf analog = Point(PointKind::Analog, PointType::DiscreteQ);
	CHECK(ScaledDiscrete(analog, 0, 0) == 0);
	CHECK(ScaledDiscrete(analog, 2147483647.0, 0) == 2147483647);
	CHECK(ScaledDiscrete(analog, -2147483648.0, 0) == -2147483648LL);
	CHECK_FALSE(ScaledDiscrete(analog, 2147483648.0, 0));
	CHECK_FALSE(ScaledDiscrete(analog, -2147483649.0, 0));
	CHECK_FALSE(ScaledDiscrete(analog, std::numeric_limits<double>::infinity(), 0));
	CHECK_FALSE(ScaledDiscrete(analog, std::nan(""), 0));
	//rounded to the nearest integer; a value that rounds back inside the range is fine
	CHECK(ScaledDiscrete(analog, 2.4, 0) == 2);
	CHECK(ScaledDiscrete(analog, 2.5, 0) == 3);
	CHECK(ScaledDiscrete(analog, -2.6, 0) == -3);
	CHECK(ScaledDiscrete(analog, 2147483647.4, 0) == 2147483647);
	CHECK_FALSE(ScaledDiscrete(analog, 2147483647.6, 0));
	//the scaling is undone first: ODC value = TASE.2 value * scale + offset
	const IccpPointConf scaled = Point(PointKind::Analog, PointType::DiscreteQ, 0.5, 10);
	CHECK(ScaledDiscrete(scaled, 110, 0) == 200);
	CHECK(ScaledDiscrete(scaled, 2147483647.0 * 0.5 + 10, 0) == 2147483647);
	CHECK_FALSE(ScaledDiscrete(scaled, 2147483648.0 * 0.5 + 10, 0));
	CHECK(ScaledDiscrete(scaled, -2147483648.0 * 0.5 + 10, 0) == -2147483648LL);
}

TEST_CASE(SUITE("ScaledDiscrete of a Counter: an unsigned ODC value above 2^31 - 1 has no signed 32 bit form"))
{
	const IccpPointConf counter = Point(PointKind::Counter, PointType::DiscreteQ);
	CHECK(ScaledDiscrete(counter, 0, 0) == 0);
	CHECK(ScaledDiscrete(counter, 0, 2147483647u) == 2147483647);
	CHECK_FALSE(ScaledDiscrete(counter, 0, 2147483648u));
	CHECK_FALSE(ScaledDiscrete(counter, 0, 4000000000u));
	//a scale that brings it back into the range
	const IccpPointConf halved = Point(PointKind::Counter, PointType::DiscreteQ, 2.0);
	CHECK(ScaledDiscrete(halved, 0, 4000000000u) == 2000000000);
	//the analog value is ignored for a Counter
	CHECK(ScaledDiscrete(counter, 1e30, 5u) == 5);
}

TEST_CASE(SUITE("a Discrete value that fits is sent as it is, one that does not fit is not valid and has no value"))
{
	for(const PointType type : {PointType::DiscreteQ, PointType::DiscreteQTimeTag, PointType::DiscreteExtended})
	{
		CAPTURE(static_cast<int>(type));
		const IccpPointConf pt = Point(PointKind::Analog, type);
		PointState s = Known();
		s.analog = 2147483647.0;
		PointValue p = ToPointValue(pt, s, 0);
		CHECK(p.discrete == 2147483647);
		CHECK(p.Flags().validity == Validity::Valid);
		s.analog = -2147483648.0;
		p = ToPointValue(pt, s, 0);
		CHECK(p.discrete == -2147483648LL);
		CHECK(p.Flags().validity == Validity::Valid);
		for(const double bad : {2147483648.0, -2147483649.0, 1e12, std::numeric_limits<double>::infinity(), std::nan("")})
		{
			CAPTURE(bad);
			s.analog = bad;
			p = ToPointValue(pt, s, 0);
			CHECK(p.discrete == 0); //not the nearest value that fits
			CHECK(p.Flags().validity == Validity::NotValid);
			CHECK((FromPointValue(pt, p, 0).quality & QualityFlags::ONLINE) != QualityFlags::ONLINE);
		}
	}
}

TEST_CASE(SUITE("a Counter above 2^31 - 1 is served as not valid, not as 2^31 - 1"))
{
	const IccpPointConf pt = Point(PointKind::Counter, PointType::DiscreteQ);
	PointState s = Known();
	s.counter = 4000000000u;
	const PointValue p = ToPointValue(pt, s, 0);
	CHECK(p.discrete != 2147483647);
	CHECK(p.Flags().validity == Validity::NotValid);
	s.counter = 2147483647u;
	CHECK(ToPointValue(pt, s, 0).discrete == 2147483647);
	CHECK(ToPointValue(pt, s, 0).Flags().validity == Validity::Valid);
}

TEST_CASE(SUITE("a Discrete value read as an Analog is exact over the whole range of an integer of width 32"))
{
	const IccpPointConf pt = Point(PointKind::Analog, PointType::DiscreteQ);
	for(const int64_t v : {int64_t(0), int64_t(1), int64_t(-1), int64_t(2147483647), int64_t(-2147483648LL), int64_t(16777217)})
	{
		CAPTURE(v);
		PointValue p;
		p.type = PointType::DiscreteQ;
		p.discrete = v;
		CHECK(FromPointValue(pt, p, 0).analog == static_cast<double>(v)); //16777217 does not survive a float: it is a double here
	}
}

TEST_CASE(SUITE("a negative Discrete value read as an unsigned Counter is flagged OVERRANGE and is 0"))
{
	const IccpPointConf pt = Point(PointKind::Counter, PointType::DiscreteQ);
	PointValue p;
	p.type = PointType::DiscreteQ;
	p.discrete = -5;
	PointState s = FromPointValue(pt, p, 0);
	CHECK(s.counter == 0);
	CHECK((s.quality & QualityFlags::OVERRANGE) == QualityFlags::OVERRANGE);
	p.discrete = 2147483647;
	s = FromPointValue(pt, p, 0);
	CHECK(s.counter == 2147483647u);
	CHECK((s.quality & QualityFlags::OVERRANGE) != QualityFlags::OVERRANGE);
	//a scale that takes a good value past what a uint32 holds
	const IccpPointConf big = Point(PointKind::Counter, PointType::DiscreteQ, 4.0);
	s = FromPointValue(big, p, 0);
	CHECK(s.counter == std::numeric_limits<uint32_t>::max());
	CHECK((s.quality & QualityFlags::OVERRANGE) == QualityFlags::OVERRANGE);
}
