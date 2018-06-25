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
 * DNP3Port/TypeConversion.cpp
 *
 *  Created on: 24/06/2018
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "TypeConversion.h"
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>
#include <opendnp3/app/AnalogOutput.h>
#include <opendatacon/IOTypes.h>

namespace odc
{

EventInfo ToODC(const opendnp3::Binary& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::Binary, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER))
		qual &= QualityFlags::CHATTER_FILTER;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST))
		qual &= QualityFlags::COMM_LOST;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED))
		qual &= QualityFlags::LOCAL_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
		qual &= QualityFlags::ONLINE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED))
		qual &= QualityFlags::REMOTE_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART))
		qual &= QualityFlags::RESTART;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::STATE))
		qual &= QualityFlags::STATE;

	event.SetPayload<EventType::Binary>(std::move(val));
	event.SetQuality(qual);
	event.SetTimestamp(dnp3.time.Get());

	return event;
}

EventInfo ToODC(const opendnp3::DoubleBitBinary& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::DoubleBitBinary, ind, source);

	EventTypePayload<EventType::DoubleBitBinary>::type val;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER))
		qual &= QualityFlags::CHATTER_FILTER;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST))
		qual &= QualityFlags::COMM_LOST;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED))
		qual &= QualityFlags::LOCAL_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE))
		qual &= QualityFlags::ONLINE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED))
		qual &= QualityFlags::REMOTE_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART))
		qual &= QualityFlags::RESTART;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE1))
	{
		qual &= QualityFlags::STATE1;
		val.first = true;
	}
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE2))
	{
		qual &= QualityFlags::STATE2;
		val.second = true;
	}

	event.SetPayload<EventType::DoubleBitBinary>(std::move(val));
	event.SetQuality(qual);
	event.SetTimestamp(dnp3.time.Get());

	return event;
}

EventInfo ToODC(const opendnp3::Analog& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::Analog, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::OVERRANGE))
		qual &= QualityFlags::OVERRANGE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST))
		qual &= QualityFlags::COMM_LOST;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::LOCAL_FORCED))
		qual &= QualityFlags::LOCAL_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE))
		qual &= QualityFlags::ONLINE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::REMOTE_FORCED))
		qual &= QualityFlags::REMOTE_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::RESTART))
		qual &= QualityFlags::RESTART;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::AnalogQuality::REFERENCE_ERR))
		qual &= QualityFlags::REFERENCE_ERR;

	event.SetPayload<EventType::Analog>(std::move(val));
	event.SetQuality(qual);
	event.SetTimestamp(dnp3.time.Get());

	return event;
}

EventInfo ToODC(const opendnp3::Counter& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::Counter, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::FrozenCounter& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::FrozenCounter, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryOutputStatus& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::BinaryOutputStatus, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputStatus& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogOutputStatus, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryQuality& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::BinaryQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::DoubleBitBinaryQuality& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::DoubleBitBinaryQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogQuality& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::CounterQuality& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::CounterQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryOutputStatusQuality& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::BinaryOutputStatusQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::ControlRelayOutputBlock& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::ControlRelayOutputBlock, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputInt16& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogOutputInt16, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputInt32& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogOutputInt32, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputFloat32& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogOutputFloat32, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputDouble64& dnp3, size_t ind, const std::string& source)
{ //TODO: unfinished
	EventInfo event(EventType::AnalogOutputDouble64, ind, source);

	return event;
}


template<> opendnp3::Binary                    FromODC<opendnp3::Binary                   >(EventInfo event)
{
	opendnp3::Binary dnp3;
	auto val = event.GetPayload<EventType::Binary>();
	dnp3.value = val;

	auto qual = event.GetQuality();
	dnp3.quality = 0;
	if(val)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::STATE);
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER);

	dnp3.time = opendnp3::DNPTime(event.GetTimestamp());

	return dnp3;
}
template<> opendnp3::DoubleBitBinary           FromODC<opendnp3::DoubleBitBinary          >(EventInfo event)
{
	opendnp3::DoubleBitBinary dnp3;
	auto val = event.GetPayload<EventType::DoubleBitBinary>();
	if((val.first != val.second) && val.first)
		dnp3.value = opendnp3::DoubleBit::DETERMINED_ON;
	else if((val.first != val.second) && val.second)
		dnp3.value = opendnp3::DoubleBit::DETERMINED_OFF;
	else
		dnp3.value = opendnp3::DoubleBit::INDETERMINATE;

	auto qual = event.GetQuality();
	dnp3.quality = 0;
	if(val.first)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE1);
	if(val.second)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE2);
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER);

	dnp3.time = opendnp3::DNPTime(event.GetTimestamp());

	return dnp3;
}
template<> opendnp3::Analog                    FromODC<opendnp3::Analog                   >(EventInfo event)
{
	opendnp3::Analog dnp3(event.GetPayload<EventType::Analog>());

	auto qual = event.GetQuality();
	dnp3.quality = 0;
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::LOCAL_FORCED);
	if((qual & QualityFlags::OVERRANGE) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::OVERRANGE);
	if((qual & QualityFlags::REFERENCE_ERR) != QualityFlags::NONE)
		dnp3.quality &= static_cast<uint8_t>(opendnp3::AnalogQuality::REFERENCE_ERR);

	dnp3.time = opendnp3::DNPTime(event.GetTimestamp());

	return dnp3;
}
template<> opendnp3::Counter                   FromODC<opendnp3::Counter                  >(EventInfo event)
{ //TODO: unfinished
	opendnp3::Counter dnp3(event.GetPayload<EventType::Counter>());
	return dnp3;
}
template<> opendnp3::FrozenCounter             FromODC<opendnp3::FrozenCounter            >(EventInfo event)
{ //TODO: unfinished
	opendnp3::FrozenCounter dnp3(event.GetPayload<EventType::FrozenCounter>());
	return dnp3;
}
template<> opendnp3::BinaryOutputStatus        FromODC<opendnp3::BinaryOutputStatus       >(EventInfo event)
{ //TODO: unfinished
	opendnp3::BinaryOutputStatus dnp3(event.GetPayload<EventType::BinaryOutputStatus>());
	return dnp3;
}
template<> opendnp3::AnalogOutputStatus        FromODC<opendnp3::AnalogOutputStatus       >(EventInfo event)
{ //TODO: unfinished
	opendnp3::AnalogOutputStatus dnp3(event.GetPayload<EventType::AnalogOutputStatus>());
	return dnp3;
}
template<> opendnp3::BinaryQuality             FromODC<opendnp3::BinaryQuality            >(EventInfo event)
{
	uint8_t dnp3 = 0;

	auto qual = event.GetQuality();
	if((qual & QualityFlags::STATE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::STATE);
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER);

	return static_cast<opendnp3::BinaryQuality>(dnp3);
}
template<> opendnp3::DoubleBitBinaryQuality    FromODC<opendnp3::DoubleBitBinaryQuality   >(EventInfo event)
{
	uint8_t dnp3 = 0;

	auto qual = event.GetQuality();
	if((qual & QualityFlags::STATE1) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE1);
	if((qual & QualityFlags::STATE2) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE2);
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER);

	return static_cast<opendnp3::DoubleBitBinaryQuality>(dnp3);
}
template<> opendnp3::AnalogQuality             FromODC<opendnp3::AnalogQuality            >(EventInfo event)
{
	uint8_t dnp3 = 0;

	auto qual = event.GetQuality();
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::LOCAL_FORCED);
	if((qual & QualityFlags::OVERRANGE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::OVERRANGE);
	if((qual & QualityFlags::REFERENCE_ERR) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::AnalogQuality::REFERENCE_ERR);

	return static_cast<opendnp3::AnalogQuality>(dnp3);
}
template<> opendnp3::CounterQuality            FromODC<opendnp3::CounterQuality           >(EventInfo event)
{
	uint8_t dnp3 = 0;

	auto qual = event.GetQuality();
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::LOCAL_FORCED);
	if((qual & QualityFlags::ROLLOVER) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::ROLLOVER);
	if((qual & QualityFlags::DISCONTINUITY) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::CounterQuality::DISCONTINUITY);

	return static_cast<opendnp3::CounterQuality>(dnp3);
}
template<> opendnp3::BinaryOutputStatusQuality FromODC<opendnp3::BinaryOutputStatusQuality>(EventInfo event)
{
	uint8_t dnp3 = 0;

	auto qual = event.GetQuality();
	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::LOCAL_FORCED);
	if((qual & QualityFlags::STATE) != QualityFlags::NONE)
		dnp3 &= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::STATE);

	return static_cast<opendnp3::BinaryOutputStatusQuality>(dnp3);
}
template<> opendnp3::ControlRelayOutputBlock   FromODC<opendnp3::ControlRelayOutputBlock  >(EventInfo event)
{ //TODO: unfinished
	opendnp3::ControlRelayOutputBlock dnp3(event.GetPayload<EventType::ControlRelayOutputBlock>());
	return dnp3;
}
template<> opendnp3::AnalogOutputInt16         FromODC<opendnp3::AnalogOutputInt16        >(EventInfo event)
{ //TODO: unfinished
	opendnp3::AnalogOutputInt16 dnp3(event.GetPayload<EventType::AnalogOutputInt16>());
	return dnp3;
}
template<> opendnp3::AnalogOutputInt32         FromODC<opendnp3::AnalogOutputInt32        >(EventInfo event)
{ //TODO: unfinished
	opendnp3::AnalogOutputInt32 dnp3(event.GetPayload<EventType::AnalogOutputInt32>());
	return dnp3;
}
template<> opendnp3::AnalogOutputFloat32       FromODC<opendnp3::AnalogOutputFloat32      >(EventInfo event)
{ //TODO: unfinished
	opendnp3::AnalogOutputFloat32 dnp3(event.GetPayload<EventType::AnalogOutputFloat32>());
	return dnp3;
}
template<> opendnp3::AnalogOutputDouble64      FromODC<opendnp3::AnalogOutputDouble64     >(EventInfo event)
{ //TODO: unfinished
	opendnp3::AnalogOutputDouble64 dnp3(event.GetPayload<EventType::AnalogOutputDouble64>());
	return dnp3;
}

} //namespace odc
