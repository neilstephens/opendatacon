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
#include <opendnp3/gen/ChannelState.h>

#include <opendatacon/Timestamp.h>

namespace odc
{
	
	typedef enum { PORT_UP, CONNECTED, DISCONNECTED, PORT_DOWN } ConnectState;
	typedef enum { ENABLED, DISABLED, DELAYED } InitState_t;
	
	typedef opendnp3::ChannelState ChannelState;
	typedef opendnp3::CommandStatus CommandStatus;
	
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
	
	enum class FrozenCounterQuality: uint8_t
	{
		/// set when the data is "good", meaning that rest of the system can trust the value
		ONLINE = 0x1,
		/// the quality all points get before we have established communication (or populated) the point
		RESTART = 0x2,
		/// set if communication has been lost with the source of the data (after establishing contact)
		COMM_LOST = 0x4,
		/// set if the value is being forced to a "fake" value somewhere in the system
		REMOTE_FORCED = 0x8,
		/// set if the value is being forced to a "fake" value on the original device
		LOCAL_FORCED = 0x10,
		/// Deprecated flag that indicates value has rolled over
		ROLLOVER = 0x20,
		/// indicates an unusual change in value
		DISCONTINUITY = 0x40,
		/// reserved bit
		RESERVED = 0x80
	};
	
	/**
	 Quality field bitmask for AnalogOutputStatus values
	 */
	enum class AnalogOutputStatusQuality: uint8_t
	{
		/// set when the data is "good", meaning that rest of the system can trust the value
		ONLINE = 0x1,
		/// the quality all points get before we have established communication (or populated) the point
		RESTART = 0x2,
		/// set if communication has been lost with the source of the data (after establishing contact)
		COMM_LOST = 0x4,
		/// set if the value is being forced to a "fake" value somewhere in the system
		REMOTE_FORCED = 0x8,
		/// set if the value is being forced to a "fake" value on the original device
		LOCAL_FORCED = 0x10,
		/// set if a hardware input etc. is out of range and we are using a place holder value
		OVERRANGE = 0x20,
		/// set if calibration or reference voltage has been lost meaning readings are questionable
		REFERENCE_ERR = 0x40,
		/// reserved bit
		RESERVED = 0x80
	};

}

#endif
