/*	opendatacon
 *
 *	Copyright (c) 2018:
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
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - Derived from  DNP3PointConf.h
 */

#ifndef MD3POINTCONF_H_
#define MD3POINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <bitset>
#include <chrono>

#include <opendatacon/IOTypes.h>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>

// This one has been given wraps: http://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++
// https://github.com/cameron314/concurrentqueue
// And is faster than the lockfree queue from boost.
// Good article http://www.codersblock.org/blog/2016/6/02/ditching-the-mutex

#include "MD3Engine.h"

// Also I have concerns about blocking checks on futures, which would block one of the asio threads - we may only have 4!
// This guys has some answers:
// http://code-slim-jim.blogspot.com/2014/07/async-usage-of-futures-and-promises-in.html
// So here is how you use std::promise and std::future in lambdas, binds, asio and anything else u cant move into.
// I also tossed in an example of how to use non-blocking checks on the futures which is another key tool for using futures in asio code.
// Apparently you can just call IOsvc.run() to yield the thread. It will come back when it has nothing to do.

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
	//TODO:  Split to parent and three child classes for functionality. MD3Point, MD3BinaryPoint, MD3AnalogPoint, MD3ControlPoint
	//TODO: Make MD3Point thread safe - look at strands
public:
	MD3Point() {};

	MD3Point(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup = 0) :
		Index(index),
		ModuleAddress(moduleaddress),
		Channel(channel),
		PollGroup(pollgroup)
	{};

	uint32_t Index = 0;
	uint8_t ModuleAddress = 0;
	bool ModuleFailed = false;	// Will be set to true if the connection to a master through ODC signals the master is not talking to its slave. For digitals we send a different response
	uint8_t Channel = 0;
	uint8_t PollGroup = 0;
};

class MD3BinaryPoint : public MD3Point
{
public:
	MD3BinaryPoint() {};

	MD3BinaryPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup) :MD3Point(index, moduleaddress, channel, pollgroup)
	{};

	MD3BinaryPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t binval, bool changed, uint64_t changedtime) :
		MD3Point(index, moduleaddress,  channel),
		Binary(binval),
		Changed(changed),
		ChangedTime(changedtime)
	{};

	// Only the values below will be changed in two places
	uint8_t Binary = 0x01;
	uint16_t ModuleBinarySnapShot = 0;	// Used for the queue necessary to handle Fn11 time tagged events. Have to remember all 16 bits when the event happened
	bool Changed = true;
	uint64_t ChangedTime = 0;	// msec since epoch. 1970,1,1 Only used for Fn9 and 11 queued data.
};

class MD3AnalogCounterPoint : public MD3Point
{

public:
	MD3AnalogCounterPoint() {};

	MD3AnalogCounterPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup) :MD3Point(index, moduleaddress, channel, pollgroup)
	{};

	// Only the values below will be changed in two places
	uint16_t Analog = 0x8000;
	uint16_t LastReadAnalog = 0x8000;
};

typedef std::map<uint32_t, std::shared_ptr<MD3BinaryPoint>>::iterator ODCBinaryPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>>::iterator MD3BinaryPointMapIterType;
typedef std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>>::iterator ODCAnalogCounterPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>>::iterator MD3AnalogCounterPointMapIterType;
typedef std::map<uint8_t, uint16_t> ModuleMapType;

enum PollGroupType { BinaryPoints, AnalogPoints };

class MD3PollGroup
{
public:
	MD3PollGroup() :
		ID(0),
		pollrate(0),
		polltype(BinaryPoints),
		UnconditionalRequired(true)
	{ }

	MD3PollGroup(uint32_t ID_, uint32_t pollrate_, PollGroupType polltype_) :
		ID(ID_),
		pollrate(pollrate_),
		polltype(polltype_),
		UnconditionalRequired(true)
	{ }

	uint32_t ID;
	uint32_t pollrate;
	PollGroupType polltype;
	bool UnconditionalRequired;	// Set to true on start up, and if other conditions are met
	ModuleMapType ModuleAddresses;	// The second value we do not use. Just using the sorted map properties.
									// As we load points we will build this list
};

class MD3PointConf: public ConfigParser
{
private:


public:
	MD3PointConf(std::string FileName, const Json::Value& ConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) override;

	void ProcessPollGroups(const Json::Value & JSONRoot);

	void ProcessBinaryPoints(const Json::Value & JSONNode, std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>>& MD3PointMap, std::map<uint32_t, std::shared_ptr<MD3BinaryPoint>>& ODCPointMap);
	void ProcessAnalogCounterPoints(const Json::Value & JSONNode, std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>>& MD3PointMap, std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>>& ODCPointMap);


	// We access the map using a Module:Channel combination, so that they will always be in order. Makes searching the next item easier.
	std::map<uint16_t , std::shared_ptr<MD3BinaryPoint>> BinaryMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t , std::shared_ptr<MD3BinaryPoint>> BinaryODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> CounterMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>> CounterODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>> BinaryControlMD3PointMap;	// ModuleAndChannel, MD3Point
	std::map<uint32_t, std::shared_ptr<MD3BinaryPoint>> BinaryControlODCPointMap;	// Index OpenDataCon, MD3Point

	std::map<uint32_t, MD3PollGroup> PollGroups;
};
#endif /* MD3POINTCONF_H_ */
