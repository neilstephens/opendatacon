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
 * DNP3PointConf.h
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3POINTCONF_H_
#define DNP3POINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/outstation/EventResponseConfig.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>

class DNP3PointConf: public ConfigParser
{
public:
	DNP3PointConf(){};
	DNP3PointConf(std::string FileName);

	void ProcessElements(const Json::Value& JSONRoot);
	uint8_t GetUnsolClassMask();

	std::pair<opendnp3::Binary,size_t> mCommsPoint;
	std::vector<uint32_t> BinaryIndicies;
	std::map<size_t, opendnp3::Binary> BinaryStartVals;
	std::map<size_t, opendnp3::PointClass> BinaryClasses;
	std::vector<uint32_t> AnalogIndicies;
	std::map<size_t, opendnp3::Analog> AnalogStartVals;
	std::map<size_t, opendnp3::PointClass> AnalogClasses;
	std::map<size_t, double> AnalogDeadbands;
	std::vector<uint32_t> ControlIndicies;
	bool EnableUnsol;
	bool UnsolClass1;
	bool UnsolClass2;
	bool UnsolClass3;
	opendnp3::EventBinaryResponse EventBinaryResponse;
	opendnp3::EventAnalogResponse EventAnalogResponse;
	opendnp3::EventCounterResponse EventCounterResponse;
	bool DoUnsolOnStartup;
	bool UseConfirms;
	size_t IntegrityScanRateSec;
	size_t EventClass1ScanRateSec;
	size_t EventClass2ScanRateSec;
	size_t EventClass3ScanRateSec;
};

#endif /* DNP3POINTCONF_H_ */
