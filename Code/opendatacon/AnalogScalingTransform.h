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
 * AnalogScalingTransform.h
 *
 *  Created on: 06/06/2017
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef AnalogScalingTransform_H_
#define AnalogScalingTransform_H_

#include <opendatacon/Transform.h>

using namespace odc;

class AnalogScalingTransform: public Transform
{
public:
	AnalogScalingTransform(const Json::Value& params): Transform(params)
	{
		if(!params.isMember("Indexes") || !params["Indexes"].isArray())
		{
			if(auto log = odc::spdlog_get("Connectors"))
				log->error("AnalogScalingTransform requires 'Indexes' Array Parameter");
			return;
		}
		if(params.isMember("ScaleFactors") && params["ScaleFactors"].isArray()
		   && params["Indexes"].size() == params["ScaleFactors"].size())
		{
			for(Json::ArrayIndex n = 0; n < params["Indexes"].size(); ++n)
			{
				auto val = params["ScaleFactors"][n].asDouble();
				if(val != 1)
					ScaleFactors[params["Indexes"][n].asUInt()] = val;
			}
		}
		else if(params.isMember("ScaleFactors"))
		{
			if(auto log = odc::spdlog_get("Connectors"))
				log->error("AnalogScalingTransform requires optional 'ScaleFactors' Array Parameter to have equal length as 'Indexes'");
		}
		if(params.isMember("Offsets") && params["Offsets"].isArray()
		   && params["Indexes"].size() == params["Offsets"].size())
		{
			for(Json::ArrayIndex n = 0; n < params["Indexes"].size(); ++n)
			{
				auto val = params["Offsets"][n].asDouble();
				if(val != 0)
					Offsets[params["Indexes"][n].asUInt()] = val;
			}
		}
		else if(params.isMember("Offsets"))
		{
			if(auto log = odc::spdlog_get("Connectors"))
				log->error("AnalogScalingTransform requires optional 'Offsets' Array Parameter to have equal length as 'Indexes'");
		}
	}

	bool Event(std::shared_ptr<EventInfo> event) override
	{
		if(event->GetEventType() != EventType::Analog)
			return true;

		auto scale_it = ScaleFactors.find(event->GetIndex());
		if(scale_it != ScaleFactors.end())
			event->SetPayload<EventType::Analog>(event->GetPayload<EventType::Analog>() * scale_it->second);

		auto offset_it = Offsets.find(event->GetIndex());
		if(offset_it != Offsets.end())
			event->SetPayload<EventType::Analog>(event->GetPayload<EventType::Analog>() + offset_it->second);

		return true;
	}

	std::unordered_map<uint16_t,double> ScaleFactors;
	std::unordered_map<uint16_t,double> Offsets;
};

#endif /* AnalogScalingTransform_H_ */
