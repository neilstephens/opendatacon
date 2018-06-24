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

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER))
		qual = qual & QualityFlags::CHATTER_FILTER;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST))
		qual = qual & QualityFlags::COMM_LOST;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED))
		qual = qual & QualityFlags::LOCAL_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
		qual = qual & QualityFlags::ONLINE;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED))
		qual = qual & QualityFlags::REMOTE_FORCED;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART))
		qual = qual & QualityFlags::RESTART;
	if(dnp3.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::STATE))
		qual = qual & QualityFlags::STATE;

	bool state = dnp3.value;
	event.SetPayload<EventType::Binary>(std::move(state));
	event.SetQuality(qual);
	event.SetTimestamp(dnp3.time.Get());

	return event;
}

EventInfo ToODC(const opendnp3::DoubleBitBinary& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::DoubleBitBinary, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::Analog& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::Analog, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::Counter& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::Counter, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::FrozenCounter& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::FrozenCounter, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryOutputStatus& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::BinaryOutputStatus, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputStatus& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogOutputStatus, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryQuality& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::BinaryQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::DoubleBitBinaryQuality& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::DoubleBitBinaryQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogQuality& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::CounterQuality& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::CounterQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::BinaryOutputStatusQuality& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::BinaryOutputStatusQuality, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::ControlRelayOutputBlock& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::ControlRelayOutputBlock, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputInt16& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogOutputInt16, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputInt32& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogOutputInt32, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputFloat32& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogOutputFloat32, ind, source);

	return event;
}

EventInfo ToODC(const opendnp3::AnalogOutputDouble64& dnp3, size_t ind, const std::string& source)
{
	EventInfo event(EventType::AnalogOutputDouble64, ind, source);

	return event;
}


template<> opendnp3::Binary                    FromODC<opendnp3::Binary                   >(EventInfo event)
{
	opendnp3::Binary dnp3;
	return dnp3;
}
template<> opendnp3::DoubleBitBinary           FromODC<opendnp3::DoubleBitBinary          >(EventInfo event)
{
	opendnp3::DoubleBitBinary dnp3;
	return dnp3;
}
template<> opendnp3::Analog                    FromODC<opendnp3::Analog                   >(EventInfo event)
{
	opendnp3::Analog dnp3;
	return dnp3;
}
template<> opendnp3::Counter                   FromODC<opendnp3::Counter                  >(EventInfo event)
{
	opendnp3::Counter dnp3;
	return dnp3;
}
template<> opendnp3::FrozenCounter             FromODC<opendnp3::FrozenCounter            >(EventInfo event)
{
	opendnp3::FrozenCounter dnp3;
	return dnp3;
}
template<> opendnp3::BinaryOutputStatus        FromODC<opendnp3::BinaryOutputStatus       >(EventInfo event)
{
	opendnp3::BinaryOutputStatus dnp3;
	return dnp3;
}
template<> opendnp3::AnalogOutputStatus        FromODC<opendnp3::AnalogOutputStatus       >(EventInfo event)
{
	opendnp3::AnalogOutputStatus dnp3;
	return dnp3;
}
template<> opendnp3::BinaryQuality             FromODC<opendnp3::BinaryQuality            >(EventInfo event)
{
	opendnp3::BinaryQuality dnp3;
	return dnp3;
}
template<> opendnp3::DoubleBitBinaryQuality    FromODC<opendnp3::DoubleBitBinaryQuality   >(EventInfo event)
{
	opendnp3::DoubleBitBinaryQuality dnp3;
	return dnp3;
}
template<> opendnp3::AnalogQuality             FromODC<opendnp3::AnalogQuality            >(EventInfo event)
{
	opendnp3::AnalogQuality dnp3;
	return dnp3;
}
template<> opendnp3::CounterQuality            FromODC<opendnp3::CounterQuality           >(EventInfo event)
{
	opendnp3::CounterQuality dnp3;
	return dnp3;
}
template<> opendnp3::BinaryOutputStatusQuality FromODC<opendnp3::BinaryOutputStatusQuality>(EventInfo event)
{
	opendnp3::BinaryOutputStatusQuality dnp3;
	return dnp3;
}
template<> opendnp3::ControlRelayOutputBlock   FromODC<opendnp3::ControlRelayOutputBlock  >(EventInfo event)
{
	opendnp3::ControlRelayOutputBlock dnp3;
	return dnp3;
}
template<> opendnp3::AnalogOutputInt16         FromODC<opendnp3::AnalogOutputInt16        >(EventInfo event)
{
	opendnp3::AnalogOutputInt16 dnp3;
	return dnp3;
}
template<> opendnp3::AnalogOutputInt32         FromODC<opendnp3::AnalogOutputInt32        >(EventInfo event)
{
	opendnp3::AnalogOutputInt32 dnp3;
	return dnp3;
}
template<> opendnp3::AnalogOutputFloat32       FromODC<opendnp3::AnalogOutputFloat32      >(EventInfo event)
{
	opendnp3::AnalogOutputFloat32 dnp3;
	return dnp3;
}
template<> opendnp3::AnalogOutputDouble64      FromODC<opendnp3::AnalogOutputDouble64     >(EventInfo event)
{
	opendnp3::AnalogOutputDouble64 dnp3;
	return dnp3;
}

} //namespace odc
