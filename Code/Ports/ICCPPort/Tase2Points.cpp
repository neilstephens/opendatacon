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
 * Tase2Points.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Point value conversion, see Tase2Points.h.
 */

#include "Tase2Points.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace iccp
{
namespace tase2
{

namespace
{
using odc::QualityFlags;

bool Online(QualityFlags q)
{
	return (q & QualityFlags::ONLINE) == QualityFlags::ONLINE;
}

//the value in TASE.2 units
double RawValue(const IccpPointConf& pt, const PointState& s)
{
	const double v = pt.kind == PointKind::Counter ? static_cast<double>(s.counter) : s.analog;
	return (v - pt.offset) / pt.scale;
}
}

std::optional<int64_t> ScaledDiscrete(const IccpPointConf& pt, double analog, uint32_t counter)
{
	const double v = pt.kind == PointKind::Counter ? static_cast<double>(counter) : analog;
	const double raw = std::round((v - pt.offset) / pt.scale);
	if(!std::isfinite(raw) || raw < static_cast<double>(INT32_MIN) || raw > static_cast<double>(INT32_MAX))
		return std::nullopt;
	return static_cast<int64_t>(raw);
}

double NumericValue(const IccpPointConf& pt, const PointState& s)
{
	switch(pt.kind)
	{
		case PointKind::Binary: return s.binary ? 1.0 : 0.0;
		case PointKind::Analog: return s.analog;
		case PointKind::Counter: return static_cast<double>(s.counter);
		case PointKind::DoubleBinary: return static_cast<double>(static_cast<uint8_t>(s.state)); //0 Between, 1 Off, 2 On, 3 Invalid
	}
	return 0;
}

PointValue ToPointValue(const IccpPointConf& pt, const PointState& s, odc::msSinceEpoch_t now)
{
	PointValue p;
	p.type = pt.type;
	const bool known = s.known;
	const bool stateType = IsStateType(pt.type);

	DataFlags flags;
	if(!known)
	{
		flags.validity = Validity::NotValid;
		if(stateType)
			flags.state = StateValue::Invalid;
	}
	else
	{
		flags = FromQuality(s.quality);
		if(stateType)
			flags.state = !Online(s.quality) ? StateValue::Invalid : (pt.kind == PointKind::DoubleBinary ? s.state : BinaryToState(s.binary));
	}

	if(IsRealType(pt.type))
		p.real = static_cast<float>(RawValue(pt, s));
	else if(IsDiscreteType(pt.type))
	{
		//a value that does not fit an integer of width 32 is not sent as if it did: it is served as not valid (the Event() of the port refuses such a value up front)
		if(const auto d = ScaledDiscrete(pt, s.analog, s.counter))
			p.discrete = *d;
		else
			flags.validity = Validity::NotValid;
	}
	p.flags = PackFlags(flags);
	p.state = flags.state;

	const odc::msSinceEpoch_t ts = s.timestamp != 0 ? s.timestamp : now;
	switch(pt.type)
	{
		case PointType::RealQTimeTag:
		case PointType::StateQTimeTag:
		case PointType::DiscreteQTimeTag:
			p.timeStamp = static_cast<int64_t>(ts / 1000);
			break;
		case PointType::RealExtended:
		case PointType::StateExtended:
		case PointType::DiscreteExtended:
			p.timeStamp = static_cast<int64_t>(ts / 1000);
			p.milliseconds = static_cast<int64_t>(ts % 1000);
			p.cov = static_cast<int64_t>(s.cov & 0xFFFF);
			break;
		default:
			break;
	}
	return p;
}

Data ToData(const IccpPointConf& point, const PointState& state, odc::msSinceEpoch_t now)
{
	return ToData(ToPointValue(point, state, now));
}

PointState FromData(const IccpPointConf& pt, const Data& data, odc::msSinceEpoch_t now)
{
	return FromPointValue(pt, ParsePoint(data, pt.type), now);
}

PointState FromPointValue(const IccpPointConf& pt, const PointValue& p, odc::msSinceEpoch_t now)
{
	PointState s;
	s.known = true;
	const bool stateType = IsStateType(pt.type);
	bool overrange = false; //a Counter value that a uint32 cannot hold (negative, or too big after scaling)

	if(IsRealType(pt.type) || IsDiscreteType(pt.type))
	{
		const double raw = IsRealType(pt.type) ? p.real : static_cast<double>(p.discrete);
		const double v = raw * pt.scale + pt.offset;
		if(pt.kind == PointKind::Counter)
		{
			overrange = !std::isfinite(v) || v < -0.5 || v > static_cast<double>(std::numeric_limits<uint32_t>::max()) + 0.5;
			s.counter = std::isfinite(v) ? static_cast<uint32_t>(std::llround(std::max(0.0, std::min(static_cast<double>(std::numeric_limits<uint32_t>::max()), v)))) : 0u;
		}
		else
			s.analog = v;
	}
	else
	{
		s.state = p.state;
		s.binary = p.state == StateValue::On;
	}

	DataFlags f = p.Flags();
	if(pt.type == PointType::State)
	{
		//only the state: valid unless the state is invalid
		f = DataFlags();
		f.state = p.state;
	}
	s.quality = ToQuality(f, stateType);
	if(overrange)
		s.quality = s.quality | odc::QualityFlags::OVERRANGE;

	if(p.timeStamp)
		s.timestamp = static_cast<odc::msSinceEpoch_t>(*p.timeStamp) * 1000 + static_cast<odc::msSinceEpoch_t>(p.milliseconds.value_or(0));
	else
		s.timestamp = now;
	s.cov = static_cast<uint32_t>(p.cov.value_or(0));
	return s;
}

} // namespace tase2
} // namespace iccp
