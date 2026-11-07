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
 * Tase2Points.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Conversion between the values of configured points (ODC) and TASE.2 point values (MMS Data).
 *
 *  Point types come from the configuration (they are not inferred). Scaling: ODC value = TASE.2 value * ScaleFactor + Offset;
 *  towards TASE.2 the inverse is used. Quality follows Tase2Quality.h. State points: on is true, off false; between or
 *  invalid states read as false with the quality of the flags (invalid also marks the value not valid).
 *  Time tags: seconds (and milliseconds for the extended types) since 1970; points without a time tag are stamped
 *  with the receive time. FromPointValue is FromData for a value that was already read (the blocked form of a report, Tase2Blocked.h).
 */

#ifndef ICCP_TASE2_POINTS_H_
#define ICCP_TASE2_POINTS_H_

#include "ICCPPortConf.h"
#include "Tase2Quality.h"
#include <opendatacon/IOTypes.h>

namespace iccp
{
namespace tase2
{

//The last known value of a point
struct PointState
{
	bool known = false;
	bool binary = false;
	double analog = 0;
	uint32_t counter = 0;
	odc::QualityFlags quality = odc::QualityFlags::NONE;
	odc::msSinceEpoch_t timestamp = 0;
	uint32_t cov = 0; //count of value changes, for the extended types
};

//The TASE.2 point value for a point's state. A point that is not known yet is sent as not valid.
PointValue ToPointValue(const IccpPointConf& point, const PointState& state, odc::msSinceEpoch_t now);
Data ToData(const IccpPointConf& point, const PointState& state, odc::msSinceEpoch_t now);

//Read a TASE.2 value of the point's configured type. Throws DecodeError if it does not have that layout.
//Points without a time tag get `now` as their timestamp.
PointState FromData(const IccpPointConf& point, const Data& data, odc::msSinceEpoch_t now);
//The same from a point value that was already read (of the point's configured type): the blocked form of a report (Tase2Blocked.h) reads the values without an MMS Data
PointState FromPointValue(const IccpPointConf& point, const PointValue& value, odc::msSinceEpoch_t now);

//The value the ODC event for this point carries (binary as 0/1, analog, counter), for comparisons and logs
double NumericValue(const IccpPointConf& point, const PointState& state);

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_POINTS_H_ */
