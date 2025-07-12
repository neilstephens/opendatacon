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

using namespace odc;

class ThresholdTransform: public Transform
{
public:
	ThresholdTransform(const std::string& Name, const Json::Value& params): Transform(Name,params),
		pass_on(false),
		already_under(false),
		threshold(-DBL_MAX)
	{
		SetLog("Connectors");
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

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		if(event->GetEventType() != EventType::Analog)
			return (*pAllow)(event);

		if(!params["points"].isArray())
			return (*pAllow)(event);

		if(event->GetIndex() == threshold_point_index)
		{
			pass_on = (event->GetPayload<EventType::Analog>() >= threshold) || (!already_under);
			already_under = (event->GetPayload<EventType::Analog>() < threshold);
		}

		if(!pass_on)
		{
			for(Json::ArrayIndex n = 0; n < params["points"].size(); ++n)
			{
				if(event->GetIndex() == params["points"][n].asUInt())
					return (*pAllow)(nullptr); //drop
			}
		}

		return (*pAllow)(event);
	}

	bool pass_on;
	bool already_under;
	uint16_t threshold_point_index;
	double threshold;
};

#endif /* THRESHOLDTRANSFORM_H_ */
