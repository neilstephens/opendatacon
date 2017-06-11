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
	IndexMapTransform(Json::Value params): Transform(params)
	{
		auto load_map = [&](const std::string map_name, std::unordered_map<uint16_t,uint16_t>& map)
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

	bool Event(Binary& meas, uint16_t& index){return EventT(meas,index,BinaryMap);}
	bool Event(DoubleBitBinary& meas, uint16_t& index){return EventT(meas,index,BinaryMap);}
	bool Event(Analog& meas, uint16_t& index){return EventT(meas,index,AnalogMap);}
	bool Event(Counter& meas, uint16_t& index){return EventT(meas,index,AnalogMap);}
	bool Event(FrozenCounter& meas, uint16_t& index){return EventT(meas,index,AnalogMap);}
	bool Event(BinaryOutputStatus& meas, uint16_t& index){return EventT(meas,index,ControlMap);}
	bool Event(AnalogOutputStatus& meas, uint16_t& index){return EventT(meas,index,ControlMap);}
	bool Event(ControlRelayOutputBlock& arCommand, uint16_t index){return EventT(arCommand,index,ControlMap);}
	bool Event(AnalogOutputInt16& arCommand, uint16_t index){return EventT(arCommand,index,ControlMap);}
	bool Event(AnalogOutputInt32& arCommand, uint16_t index){return EventT(arCommand,index,ControlMap);}
	bool Event(AnalogOutputFloat32& arCommand, uint16_t index){return EventT(arCommand,index,ControlMap);}
	bool Event(AnalogOutputDouble64& arCommand, uint16_t index){return EventT(arCommand,index,ControlMap);}

	template<typename T> bool EventT(T& meas, uint16_t& index, const std::unordered_map<uint16_t,uint16_t>& map)
	{
		if(map.count(index))
		{
			index = map.at(index);
			return true;
		}
		return false;
	}

	std::unordered_map<uint16_t,uint16_t> AnalogMap;
	std::unordered_map<uint16_t,uint16_t> BinaryMap;
	std::unordered_map<uint16_t,uint16_t> ControlMap;
};

#endif /* INDEXMAPTRANSFORM_H_ */
