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
#include <bitset>
#include <opendatacon/IOTypes.h>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>


#include <chrono>

using namespace odc;

// So the MD3 Addressing structure requires us to support a Module addressing scheme 1 to 256 for digital IO with 16 bits at each location
// The Analogs are effectively the same, with 16 channels at each Module address.

// OpenDataCon uses an Index to refer to Analogs and Binaries. MD3 Uses a Module/Channel addressing mode. So our config file needs to contain information about how
// we map the Index exposed to OpenDataCon to the Module/Channel required by MD3. The Channel we assume is zero based. (ie 0 to 15 valid values)
// So for a given Index (or Range) we define a Module and an Offset.
// For example a module with address 20, Channel 5 would look like "Binaries" : [{"Index": 0, "Module" : 20, "Offset" : 5}]
//
// Or for a range which we require to be contiguous. If more than 16 channels are in a range, then the Module Address will be incremented for the next 16 and so on.
//
// "Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0}],
//
// For each Point, we are going to need to store its Value, LastReadValue (Station only), LastChangeTime
// This may have to get more complex to return a time change queue of data.
// The time scan function also requires us to remember the last sent packet, so that we can resend it if we get the same sequence number from the master.
// This is actually a good way to do it, as the data structure above does not have to deal with the re-try.

class MD3Point
{
public:
	MD3Point(uint32_t index, uint8_t moduleaddress, uint8_t channel) :
		Index(index),
		ModuleAddress(moduleaddress),
		Channel(channel)
	{};

	uint32_t Index;
	uint8_t ModuleAddress;
	uint8_t Channel;
	uint16_t Analog = 0x8000;
	uint16_t LastReadAnalog = 0x8000;
	uint8_t Binary = 0x01;
	bool Changed = true;	//TODO: Concurrency issues around the MD3 Point object?? Use this for Binary only?
};

typedef std::map<uint32_t, std::shared_ptr<MD3Point>>::iterator ODCPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<MD3Point>>::iterator MD3PointMapIterType;

class MD3PointConf: public ConfigParser
{
public:
	MD3PointConf(std::string FileName, const Json::Value& ConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) override;

	void ProcessPoints(const Json::Value& JSONNode, std::map<uint16_t, std::shared_ptr<MD3Point>> &MD3PointMap, std::map<uint32_t, std::shared_ptr<MD3Point>> &ODCPointMap);

	unsigned LinkNumRetry = 0;
	unsigned LinkTimeoutms = 0;

	// We access the map using a Module:Channel combination, so that they will always be in order. Makes searching the next item easier.
	std::map<uint16_t , std::shared_ptr<MD3Point>> BinaryMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t , std::shared_ptr<MD3Point>> BinaryODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3Point>> AnalogMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t, std::shared_ptr<MD3Point>> AnalogODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3Point>> BinaryControlMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t, std::shared_ptr<MD3Point>> BinaryControlODCPointMap;	// Index OpenDataCon, MD3Point

private:
	//template<class T>
};

#endif /* MD3POINTCONF_H_ */
