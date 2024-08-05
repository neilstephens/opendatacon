/*	opendatacon
 *
 *	Copyright (c) 2015:
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
 * EventConversion.h
 *
 *  Created on: 2024-08-05
 *  Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef EVENTCONVERSION_H
#define EVENTCONVERSION_H

#include "IOTypes.h"
#include "opendatacon/util.h"

namespace odc
{
template <EventType FromET, EventType ToET>
auto CapAnalogOutputValue(const std::shared_ptr<const EventInfo>& fromEvent)
{
	auto from_val = fromEvent->GetPayload<FromET>();
	typename EventTypePayload<ToET>::type to_val;
	auto HighLimit = std::numeric_limits<decltype(to_val.first)>::max();
	auto LowLimit = std::numeric_limits<decltype(to_val.first)>::lowest();
	if(from_val.first > HighLimit)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->warn("EventConversion: Conversion from '"+ToString(FromET)+"' to '"+ToString(ToET)+"' resulted in value being capped at the high limit");
		from_val.first = HighLimit;
	}
	if(from_val.first < LowLimit)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->warn("EventConversion: Conversion from '"+ToString(FromET)+"' to '"+ToString(ToET)+"' resulted in value being capped at the low limit");
		from_val.first = LowLimit;
	}
	to_val = from_val;
	auto toEvent = std::make_shared<EventInfo>(ToET,fromEvent->GetIndex(),fromEvent->GetSourcePort(),fromEvent->GetQuality(),fromEvent->GetTimestamp());
	toEvent->SetPayload<ToET>(std::move(to_val));
	return toEvent;
}

inline std::shared_ptr<const EventInfo> ConvertEvent(const std::shared_ptr<const EventInfo>& fromEvent, const EventType ToET)
{
	auto FromET = fromEvent->GetEventType();
	switch(FromET)
	{
		case EventType::AnalogOutputInt16:
			switch(ToET)
			{
				case EventType::AnalogOutputInt16:
					return CapAnalogOutputValue<EventType::AnalogOutputInt16,EventType::AnalogOutputInt16>(fromEvent);
				case EventType::AnalogOutputInt32:
					return CapAnalogOutputValue<EventType::AnalogOutputInt16,EventType::AnalogOutputInt32>(fromEvent);
				case EventType::AnalogOutputFloat32:
					return CapAnalogOutputValue<EventType::AnalogOutputInt16,EventType::AnalogOutputFloat32>(fromEvent);
				case EventType::AnalogOutputDouble64:
					return CapAnalogOutputValue<EventType::AnalogOutputInt16,EventType::AnalogOutputDouble64>(fromEvent);
				default:
					break;
			}
			break;
		case EventType::AnalogOutputInt32:
			switch(ToET)
			{
				case EventType::AnalogOutputInt16:
					return CapAnalogOutputValue<EventType::AnalogOutputInt32,EventType::AnalogOutputInt16>(fromEvent);
				case EventType::AnalogOutputInt32:
					return CapAnalogOutputValue<EventType::AnalogOutputInt32,EventType::AnalogOutputInt32>(fromEvent);
				case EventType::AnalogOutputFloat32:
					return CapAnalogOutputValue<EventType::AnalogOutputInt32,EventType::AnalogOutputFloat32>(fromEvent);
				case EventType::AnalogOutputDouble64:
					return CapAnalogOutputValue<EventType::AnalogOutputInt32,EventType::AnalogOutputDouble64>(fromEvent);
				default:
					break;
			}
			break;
		case EventType::AnalogOutputFloat32:
			switch(ToET)
			{
				case EventType::AnalogOutputInt16:
					return CapAnalogOutputValue<EventType::AnalogOutputFloat32,EventType::AnalogOutputInt16>(fromEvent);
				case EventType::AnalogOutputInt32:
					return CapAnalogOutputValue<EventType::AnalogOutputFloat32,EventType::AnalogOutputInt32>(fromEvent);
				case EventType::AnalogOutputFloat32:
					return CapAnalogOutputValue<EventType::AnalogOutputFloat32,EventType::AnalogOutputFloat32>(fromEvent);
				case EventType::AnalogOutputDouble64:
					return CapAnalogOutputValue<EventType::AnalogOutputFloat32,EventType::AnalogOutputDouble64>(fromEvent);
				default:
					break;
			}
			break;
		case EventType::AnalogOutputDouble64:
			switch(ToET)
			{
				case EventType::AnalogOutputInt16:
					return CapAnalogOutputValue<EventType::AnalogOutputDouble64,EventType::AnalogOutputInt16>(fromEvent);
				case EventType::AnalogOutputInt32:
					return CapAnalogOutputValue<EventType::AnalogOutputDouble64,EventType::AnalogOutputInt32>(fromEvent);
				case EventType::AnalogOutputFloat32:
					return CapAnalogOutputValue<EventType::AnalogOutputDouble64,EventType::AnalogOutputFloat32>(fromEvent);
				case EventType::AnalogOutputDouble64:
					return CapAnalogOutputValue<EventType::AnalogOutputDouble64,EventType::AnalogOutputDouble64>(fromEvent);
				default:
					break;
			}
			break;
		default:
			break;
	}
	throw std::runtime_error("EventConversion: Cannot convert between event types '"+ToString(FromET)+"' => '"+ToString(ToET)+"'");
}
}

#endif // EVENTCONVERSION_H