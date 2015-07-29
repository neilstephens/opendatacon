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
 * Transform.h
 *
 *  Created on: 30/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef TRANSFORM_H_
#define TRANSFORM_H_

#include <opendatacon/IOHandler.h>
#include <opendnp3/app/MeasurementTypes.h>
#include <json/json.h>
#include <string>

class Transform
{
public:
	Transform(Json::Value params): params(params){};
	virtual ~Transform(){};

	virtual bool Event(opendnp3::Binary& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::DoubleBitBinary& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::Analog& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::Counter& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::FrozenCounter& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::BinaryOutputStatus& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::AnalogOutputStatus& meas, uint16_t& index) { return true; };
	virtual bool Event(opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index) { return true; };
	virtual bool Event(opendnp3::AnalogOutputInt16& arCommand, uint16_t index) { return true; };
	virtual bool Event(opendnp3::AnalogOutputInt32& arCommand, uint16_t index) { return true; };
	virtual bool Event(opendnp3::AnalogOutputFloat32& arCommand, uint16_t index) { return true; };
	virtual bool Event(opendnp3::AnalogOutputDouble64& arCommand, uint16_t index) { return true; };

	virtual bool Event(::BinaryQuality qual, uint16_t index) { return true; };
	virtual bool Event(::DoubleBitBinaryQuality qual, uint16_t index) { return true; };
	virtual bool Event(::AnalogQuality qual, uint16_t index) { return true; };
	virtual bool Event(::CounterQuality qual, uint16_t index) { return true; };
	virtual bool Event(::FrozenCounterQuality qual, uint16_t index) { return true; };
	virtual bool Event(::BinaryOutputStatusQuality qual, uint16_t index) { return true; };
	virtual bool Event(::AnalogOutputStatusQuality qual, uint16_t index) { return true; };

	virtual bool Event(::ConnectState state, uint16_t index){ return true; };

	Json::Value params;
};

#endif /* TRANSFORM_H_ */
