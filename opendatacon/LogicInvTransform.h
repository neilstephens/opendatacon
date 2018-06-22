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
 * LogicInvTransform.h
 *
 *  Created on: 19/10/2018
 *      Author: Andrew Gilbett <andrew@gilbett.net>
 *  This performs a logical not on binary measured values
 */

#ifndef LOGICINVTRANSFORM_H_
#define LOGICINVTRANSFORM_H_

#include <opendatacon/util.h>
#include <opendatacon/Transform.h>

class LogicInvTransform: public Transform
{
public:
	LogicInvTransform(const Json::Value& params):
		Transform(params)
	{}

	//pass through events that inversion doesn't apply to
	bool Event(DoubleBitBinary& meas, uint16_t& index) override {return true;}
	bool Event(Counter& meas, uint16_t& index) override {return true;}
	bool Event(FrozenCounter& meas, uint16_t& index) override {return true;}
	bool Event(AnalogOutputStatus& meas, uint16_t& index) override {return true;}
	bool Event(ControlRelayOutputBlock& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputInt16& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputInt32& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputFloat32& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputDouble64& arCommand, uint16_t index) override {return true;}
	bool Event(Analog& meas, uint16_t& index) override {return true;}

	//apply inversion to these events
	bool Event(BinaryOutputStatus& meas, uint16_t& index) override {return EventT(meas);}
	bool Event(Binary& meas, uint16_t& index) override {return EventT(meas);}

	template<typename T> bool EventT(T& meas)
	{
		meas.value = !(meas.value);
		return true;
	}


};

#endif /* LOGICINVTRANSFORM_H_ */
