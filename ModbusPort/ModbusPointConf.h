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
 * ModbusPointConf.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef ModbusPOINTCONF_H_
#define ModbusPOINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendnp3/outstation/EventResponseConfig.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>

class ModbusPointConf: public ConfigParser
{
public:
	ModbusPointConf(std::string FileName);

	void ProcessElements(const Json::Value& JSONRoot);
	uint8_t GetUnsolClassMask();

	std::pair<opendnp3::Binary,size_t> mCommsPoint;
    
	std::vector<uint32_t> BitIndicies;
    std::vector<uint32_t> InputBitIndicies;
    std::vector<uint32_t> RegIndicies;
    std::vector<uint32_t> InputRegIndicies;
};

#endif /* ModbusPOINTCONF_H_ */
