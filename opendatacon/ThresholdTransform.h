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
 * ThresholdTransform.h
 *
 *  Created on: 30/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef THRESHOLDTRANSFORM_H_
#define THRESHOLDTRANSFORM_H_

#include <cstdint>
#include <cfloat>
#include <opendatacon/Transform.h>

class ThresholdTransform: public Transform
{
public:
	ThresholdTransform(const Json::Value& params):
		Transform(params),
		pass_on(false),
		already_under(false),
		threshold(-DBL_MAX)
	{
		if(!params.isMember("threshold_point_index") || !params["threshold_point_index"].isUInt())
		{
			threshold_point_index = 0;
			pass_on = true;
		}
		else
		{
			threshold_point_index = params["threshold_point_index"].asUInt();
			if(params.isMember("threshold") && params["threshold"].isNumeric())
				threshold = params["threshold"].asDouble();
		}
	}

	bool Event(Binary& meas, uint16_t& index) override {return true;}
	bool Event(DoubleBitBinary& meas, uint16_t& index) override {return true;}
	bool Event(Counter& meas, uint16_t& index) override {return true;}
	bool Event(FrozenCounter& meas, uint16_t& index) override {return true;}
	bool Event(BinaryOutputStatus& meas, uint16_t& index) override {return true;}
	bool Event(AnalogOutputStatus& meas, uint16_t& index) override {return true;}
	bool Event(ControlRelayOutputBlock& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputInt16& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputInt32& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputFloat32& arCommand, uint16_t index) override {return true;}
	bool Event(AnalogOutputDouble64& arCommand, uint16_t index) override {return true;}

	bool Event(Analog& meas, uint16_t& index) override
	{
		if(!params["points"].isArray())
			return true;

		if(index == threshold_point_index)
		{
			pass_on = (meas.value >= threshold) || (!already_under);
			already_under = (meas.value < threshold);
		}

		if(!pass_on)
		{
			for(Json::ArrayIndex n = 0; n < params["points"].size(); ++n)
			{
				if(index == params["points"][n].asUInt())
					return false;
			}
		}

		return true;
	}

	bool pass_on;
	bool already_under;
	uint16_t threshold_point_index;
	double threshold;
};

#endif /* THRESHOLDTRANSFORM_H_ */
