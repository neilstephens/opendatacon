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
 * MD3PointConf.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef MD3POINTCONF_H_
#define MD3POINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <opendatacon/IOTypes.h>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>


#include <chrono>

using namespace odc;

class MD3PointConf: public ConfigParser
{
public:
	MD3PointConf(std::string FileName);

	void ProcessElements(const Json::Value& JSONRoot) override;

//	std::pair<Binary,size_t> mCommsPoint;

	unsigned LinkNumRetry = 0;
	unsigned LinkTimeoutms = 0;

	std::vector<uint32_t> BinaryIndicies;
	std::vector<uint32_t> AnalogIndicies;
	std::vector<uint32_t> ControlIndicies;

private:
	template<class T>
};

#endif /* MD3POINTCONF_H_ */
