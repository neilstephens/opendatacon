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
 * DNP3Port/TypeConversion.h
 *
 *  Created on: 24/06/2018
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <memory>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>
#include <opendnp3/app/AnalogOutput.h>
#include <opendnp3/app/BinaryCommandEvent.h>
#include <opendnp3/app/AnalogCommandEvent.h>
#include <opendnp3/app/OctetString.h>
#include <opendnp3/app/DNPTime.h>
#include <opendnp3/app/SecurityStat.h>
#include <opendatacon/IOTypes.h>



namespace odc
{

CommandStatus ToODC(const opendnp3::CommandStatus dnp3);
opendnp3::CommandStatus FromODC(const CommandStatus stat);

std::shared_ptr<EventInfo> ToODC(const opendnp3::Binary& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::DoubleBitBinary& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::Analog& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::Counter& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::FrozenCounter& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryOutputStatus& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputStatus& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryQuality& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::DoubleBitBinaryQuality& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogQuality& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::CounterQuality& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryOutputStatusQuality& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::ControlRelayOutputBlock& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputInt16& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputInt32& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputFloat32& dnp3, const size_t ind = 0, const std::string& source = "");
std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputDouble64& dnp3, const size_t ind = 0, const std::string& source = "");

//Map EventTypes to opendnp3 types
template<EventType t> struct EventTypeDNP3 { typedef void type; };
#define EVENTDNP3(E,T)\
	template<> struct EventTypeDNP3<E> { typedef T type; };
EVENTDNP3(EventType::Binary                   , opendnp3::Binary)
EVENTDNP3(EventType::DoubleBitBinary          , opendnp3::DoubleBitBinary)
EVENTDNP3(EventType::Analog                   , opendnp3::Analog)
EVENTDNP3(EventType::Counter                  , opendnp3::Counter)
EVENTDNP3(EventType::FrozenCounter            , opendnp3::FrozenCounter)
EVENTDNP3(EventType::BinaryOutputStatus       , opendnp3::BinaryOutputStatus)
EVENTDNP3(EventType::AnalogOutputStatus       , opendnp3::AnalogOutputStatus)
EVENTDNP3(EventType::BinaryCommandEvent       , opendnp3::BinaryCommandEvent)
EVENTDNP3(EventType::AnalogCommandEvent       , opendnp3::AnalogCommandEvent)
EVENTDNP3(EventType::OctetString              , opendnp3::OctetString)
EVENTDNP3(EventType::TimeAndInterval          , opendnp3::TimeAndInterval)
EVENTDNP3(EventType::SecurityStat             , opendnp3::SecurityStat)
EVENTDNP3(EventType::ControlRelayOutputBlock  , opendnp3::ControlRelayOutputBlock)
EVENTDNP3(EventType::AnalogOutputInt16        , opendnp3::AnalogOutputInt16)
EVENTDNP3(EventType::AnalogOutputInt32        , opendnp3::AnalogOutputInt32)
EVENTDNP3(EventType::AnalogOutputFloat32      , opendnp3::AnalogOutputFloat32)
EVENTDNP3(EventType::AnalogOutputDouble64     , opendnp3::AnalogOutputDouble64)
EVENTDNP3(EventType::BinaryQuality            , opendnp3::BinaryQuality)
EVENTDNP3(EventType::DoubleBitBinaryQuality   , opendnp3::DoubleBitBinaryQuality)
EVENTDNP3(EventType::AnalogQuality            , opendnp3::AnalogQuality)
EVENTDNP3(EventType::CounterQuality           , opendnp3::CounterQuality)
EVENTDNP3(EventType::BinaryOutputStatusQuality, opendnp3::BinaryOutputStatusQuality)
EVENTDNP3(EventType::FrozenCounterQuality     , opendnp3::CounterQuality)
EVENTDNP3(EventType::AnalogOutputStatusQuality, opendnp3::AnalogQuality)

template<typename T> T FromODC(const QualityFlags& qual);
template<typename T> T FromODC(const std::shared_ptr<const EventInfo>& event);

} //namespace odc
