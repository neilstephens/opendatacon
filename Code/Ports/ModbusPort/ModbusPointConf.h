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
#include <opendatacon/IOTypes.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>
#include <chrono>

using namespace odc;

class ModbusPollGroup
{
public:
	ModbusPollGroup():
		ID(0),
		pollrate(0)
	{ }

	ModbusPollGroup(uint32_t ID_, uint32_t pollrate_):
		ID(ID_),
		pollrate(pollrate_)
	{ }

	uint32_t ID;
	uint32_t pollrate;
};

class ModbusReadGroup
{
public:
	ModbusReadGroup(uint32_t start_, uint32_t count_, uint32_t pollgroup_, std::shared_ptr<const EventInfo> startval_, uint32_t offset):
		start(start_),
		count(count_),
		pollgroup(pollgroup_),
		startval(startval_),
		index_offset(offset)
	{ }

	bool operator<(const ModbusReadGroup& other) const
	{
		return (this->start < other.start);
	}

	uint32_t start;
	uint32_t count;
	uint32_t pollgroup;
	std::shared_ptr<const EventInfo> startval;
	uint32_t index_offset;
};

class ModbusReadGroupCollection: public std::vector<ModbusReadGroup>
{
public:
	size_t Total()
	{
		size_t total = 0;
		for(const auto& element : *this)
		{
			total += element.count;
		}
		return total;
	}
};

class ModbusPointConf: public ConfigParser
{
public:
	ModbusPointConf(const std::string& FileName);

	void ProcessElements(const Json::Value& JSONRoot) override;
	uint8_t GetUnsolClassMask();

	ModbusReadGroupCollection BitIndicies;
	ModbusReadGroupCollection InputBitIndicies;
	ModbusReadGroupCollection RegIndicies;
	ModbusReadGroupCollection InputRegIndicies;

	std::map<uint32_t, ModbusPollGroup> PollGroups;

private:
	template<EventType T>
	void ProcessReadGroup(const Json::Value& Ranges,ModbusReadGroupCollection& ReadGroup);
};

#endif /* ModbusPOINTCONF_H_ */
