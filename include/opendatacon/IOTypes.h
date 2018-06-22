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
 * IOTypes.h
 *
 *  Created on: 20/12/2015
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef IOTYPES_H_
#define IOTYPES_H_

#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>
#include <opendnp3/app/AnalogOutput.h>
#include <opendatacon/util.h>

namespace odc
{

typedef opendnp3::CommandStatus CommandStatus;
typedef opendnp3::ControlCode ControlCode;

/// Timestamp
typedef     opendnp3::DNPTime Timestamp;

/// Measurement types
typedef opendnp3::Binary Binary;
typedef opendnp3::DoubleBitBinary DoubleBitBinary;
typedef opendnp3::Analog Analog;
typedef opendnp3::Counter Counter;
typedef opendnp3::FrozenCounter FrozenCounter;
typedef opendnp3::BinaryOutputStatus BinaryOutputStatus;
typedef opendnp3::AnalogOutputStatus AnalogOutputStatus;

/// Output types
typedef opendnp3::ControlRelayOutputBlock ControlRelayOutputBlock;
typedef opendnp3::AnalogOutputInt16 AnalogOutputInt16;
typedef opendnp3::AnalogOutputInt32 AnalogOutputInt32;
typedef opendnp3::AnalogOutputFloat32 AnalogOutputFloat32;
typedef opendnp3::AnalogOutputDouble64 AnalogOutputDouble64;

/// Quality types
typedef opendnp3::BinaryQuality BinaryQuality;
typedef opendnp3::DoubleBitBinaryQuality DoubleBitBinaryQuality;
typedef opendnp3::AnalogQuality AnalogQuality;
typedef opendnp3::CounterQuality CounterQuality;
typedef opendnp3::BinaryOutputStatusQuality BinaryOutputStatusQuality;
enum class FrozenCounterQuality: uint8_t {};
enum class AnalogOutputStatusQuality: uint8_t {};

//enumerate all the different type of events that can pass through opendatacon
//As a starting point, define values to correspond with all the dnp3 measurment and output types,
// that way it should be easy to migrate from using actual opendnp3 library types
enum class EventType: uint8_t
{
	//Inputs
	Binary                    = 1,
	DoubleBitBinary           = 2,
	Analog                    = 3,
	Counter                   = 4,
	FrozenCounter             = 5,
	BinaryOutputStatus        = 6,
	AnalogOutputStatus        = 7,

	//Outputs
	ControlRelayOutputBlock   = 8,
	AnalogOutputInt16         = 9,
	AnalogOutputInt32         = 10,
	AnalogOutputFloat32       = 11,
	AnalogOutputDouble64      = 12,

	//Quality (for when the quality changes, but not the value)
	BinaryQuality             = 13,
	DoubleBitBinaryQuality    = 14,
	AnalogQuality             = 15,
	CounterQuality            = 16,
	BinaryOutputStatusQuality = 17,
	FrozenCounterQuality      = 18,
	AnalogOutputStatusQuality = 19
};

//Quatilty flags that can be used for any EventType
//Start with a superset of all the dnp3 type qualities
enum class QualityFlags: uint16_t
{
	ONLINE            = 1<<0,
	RESTART           = 1<<1,
	COMM_LOST         = 1<<2,
	REMOTE_FORCED     = 1<<3,
	LOCAL_FORCED      = 1<<4,
	OVERRANGE         = 1<<5,
	REFERENCE_ERR     = 1<<6,
	ROLLOVER          = 1<<7,
	DISCONTINUITY     = 1<<8,
	CHATTER_FILTER    = 1<<9,
	STATE             = 1<<10,
	STATE1            = 1<<11,
	STATE2            = 1<<12
};

enum class eCommandStatus : uint8_t
{
	SUCCESS = 0,
	TIMEOUT = 1,
	NO_SELECT = 2,
	FORMAT_ERROR = 3,
	NOT_SUPPORTED = 4,
	ALREADY_ACTIVE = 5,
	HARDWARE_ERROR = 6,
	LOCAL = 7,
	TOO_MANY_OPS = 8,
	NOT_AUTHORIZED = 9,
	AUTOMATION_INHIBIT = 10,
	PROCESSING_LIMITED = 11,
	OUT_OF_RANGE = 12,
	DOWNSTREAM_LOCAL = 13,
	ALREADY_COMPLETE = 14,
	BLOCKED = 15,
	CANCELLED = 16,
	BLOCKED_OTHER_MASTER = 17,
	DOWNSTREAM_FAIL = 18,
	NON_PARTICIPATING = 126,
	UNDEFINED = 127
};

template <typename Payload_t>
class EventInfo
{
public:
	EventInfo(EventType t, Payload_t&& p):
		Type(t),
		Payload(std::move(p))
	{}

	//Getters
	const EventType& GetType(){ return Type; }
	const Payload_t& GetPayload(){ return Payload; }
	const msSinceEpoch_t& GetTimestamp(){ return Timestamp; }
	const QualityFlags& GetQuality(){ return Quality; }

	//Setters
	void SetType(EventType t){ Type = t; }
	void SetTimestamp(msSinceEpoch_t t){ Timestamp = t; }
	void SetQuality(QualityFlags q){ Quality = q; }
	void SetPayload(Payload_t&& p){Payload = std::move(p); }

private:
	EventType Type;
	msSinceEpoch_t Timestamp = msSinceEpoch();
	QualityFlags Quality = (QualityFlags::ONLINE | QualityFlags::RESTART);
	Payload_t Payload;
};

}

#endif
