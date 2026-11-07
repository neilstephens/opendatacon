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
 * Tase2Quality.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Mapping between TASE.2 data flags and opendatacon QualityFlags (header only, kept apart from Tase2.h so that
 *  the protocol code does not depend on the opendatacon headers).
 *
 *  TASE.2 -> odc (ToQuality):
 *      Validity    Valid      ONLINE
 *                  Held       ONLINE | COMM_LOST        (last good value, no longer updated)
 *                  Suspect    ONLINE | REFERENCE_ERR    (questionable value)
 *                  NotValid   COMM_LOST                 (not online)
 *      Source      Entered    + LOCAL_FORCED            (operator entered)
 *                  Estimated  + REMOTE_FORCED           (substituted by an estimator)
 *                  Telemetered, Calculated              no flag
 *      State (state point types only):
 *                  Invalid    forces the NotValid mapping
 *                  Between    + DISCONTINUITY           (in transit)
 *      NormalValue (abnormal) and TimeStampQuality (invalid) have no odc equivalent and are dropped.
 *
 *  odc -> TASE.2 (FromQuality): not ONLINE gives NotValid; else COMM_LOST gives Held; else REFERENCE_ERR gives Suspect;
 *  else Valid. LOCAL_FORCED gives Entered, else REMOTE_FORCED gives Estimated, else Telemetered. RESTART, OVERRANGE,
 *  ROLLOVER, DISCONTINUITY and CHATTER_FILTER have no TASE.2 equivalent and are dropped. The mapping is lossy in both
 *  directions; the raw flags octet stays available in PointValue.
 *
 *  Binary points: StateToBinary (On is true, everything else false), BinaryToState (true is On, false Off).
 */

#ifndef ICCP_TASE2_QUALITY_H_
#define ICCP_TASE2_QUALITY_H_

#include "Tase2.h"
#include <opendatacon/IOTypes.h>

namespace iccp
{
namespace tase2
{

inline odc::QualityFlags ToQuality(const DataFlags& f, bool stateType = false)
{
	using Q = odc::QualityFlags;
	Validity v = f.validity;
	if(stateType && f.state == StateValue::Invalid)
		v = Validity::NotValid;
	Q q = Q::NONE;
	switch(v)
	{
		case Validity::Valid: q = q | Q::ONLINE; break;
		case Validity::Held: q = q | Q::ONLINE | Q::COMM_LOST; break;
		case Validity::Suspect: q = q | Q::ONLINE | Q::REFERENCE_ERR; break;
		case Validity::NotValid: q = q | Q::COMM_LOST; break;
	}
	if(f.source == CurrentSource::Entered)
		q = q | Q::LOCAL_FORCED;
	else if(f.source == CurrentSource::Estimated)
		q = q | Q::REMOTE_FORCED;
	if(stateType && f.state == StateValue::Between)
		q = q | Q::DISCONTINUITY;
	return q;
}

//the state bits are not touched: set them from the value
inline DataFlags FromQuality(odc::QualityFlags q, DataFlags base = DataFlags())
{
	using Q = odc::QualityFlags;
	const auto has = [&q](Q flag) { return (q & flag) == flag; };
	if(!has(Q::ONLINE))
		base.validity = Validity::NotValid;
	else if(has(Q::COMM_LOST))
		base.validity = Validity::Held;
	else if(has(Q::REFERENCE_ERR))
		base.validity = Validity::Suspect;
	else
		base.validity = Validity::Valid;
	if(has(Q::LOCAL_FORCED))
		base.source = CurrentSource::Entered;
	else if(has(Q::REMOTE_FORCED))
		base.source = CurrentSource::Estimated;
	else
		base.source = CurrentSource::Telemetered;
	return base;
}

inline bool StateToBinary(StateValue s)
{
	return s == StateValue::On;
}

inline StateValue BinaryToState(bool value)
{
	return value ? StateValue::On : StateValue::Off;
}

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_QUALITY_H_ */
