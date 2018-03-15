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
 *  Created on: 10/03/2018
 *      Author: Scott Ellis - Derived from  DNP3PointConf.h
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

// So the MD3 Addressing structure requires us to support a Module addressing scheme 1 to 256 for digital IO with 16 bits at each location
// The Analogs are effectively the same, with 16 channels at each Module address.
// ModBus uses an Index/Offset addressing scheme. We will do similar Index/Channel.
// Sometimes we will return 16 bits at an Index, othertimes will return just time tagged chanes, so need to support this.

class MD3PointConf: public ConfigParser
{
public:
	MD3PointConf(std::string FileName, const Json::Value& ConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) override;

	void ProcessBinaryControls(const Json::Value& JSONRoot);

	void ProcessBinaries(const Json::Value& JSONRoot);

	void ProcessAnalogs(const Json::Value& JSONRoot);

	unsigned LinkNumRetry = 0;
	unsigned LinkTimeoutms = 0;

	std::vector<uint32_t> BinaryIndicies;
	std::vector<uint32_t> AnalogIndicies;
	std::vector<uint32_t> ControlIndicies;

private:
	//template<class T>
};

#endif /* MD3POINTCONF_H_ */
