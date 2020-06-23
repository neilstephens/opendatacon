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
 * IndexMapTransform.h
 *
 *  Created on: 06/06/2017
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef INDEXMAPTRANSFORM_H_
#define INDEXMAPTRANSFORM_H_

#include <opendatacon/Transform.h>

using namespace odc;

class IndexMapTransform: public Transform
{
public:
	IndexMapTransform(const Json::Value& params): Transform(params)
	{
		auto load_map = [&](const std::string& map_name, std::unordered_map<uint16_t,uint16_t>& map)
				    {
					    if(params.isMember(map_name) && params[map_name].isMember("From") && params[map_name].isMember("To")
					       && params[map_name]["From"].isArray() && params[map_name]["To"].isArray() && params[map_name]["From"].size() == params[map_name]["To"].size())
					    {
						    for(Json::ArrayIndex n = 0; n < params[map_name]["From"].size(); ++n)
						    {
							    map[params[map_name]["From"][n].asUInt()] = params[map_name]["To"][n].asUInt();
						    }
					    }
				    };
		load_map("AnalogMap",AnalogMap);
		load_map("BinaryMap",BinaryMap);
		load_map("ControlMap",ControlMap);
	}

	bool Event(std::shared_ptr<EventInfo> event) override
	{
		auto map = &AnalogMap;
		switch(event->GetEventType())
		{
			case EventType::Analog:
				break;
			case EventType::Binary:
				map = &BinaryMap;
				break;
			case EventType::ControlRelayOutputBlock:
				map = &ControlMap;
				break;
			default:
				return true;
		}
		if(map->count(event->GetIndex()))
		{
			event->SetIndex(map->at(event->GetIndex()));
			return true;
		}
		return false;
	}

	std::unordered_map<uint16_t,uint16_t> AnalogMap;
	std::unordered_map<uint16_t,uint16_t> BinaryMap;
	std::unordered_map<uint16_t,uint16_t> ControlMap;
};

#endif /* INDEXMAPTRANSFORM_H_ */
