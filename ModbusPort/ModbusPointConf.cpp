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
 * ModbusPointConf.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include "ModbusPointConf.h"
#include <algorithm>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <regex>

using namespace odc;

ModbusPointConf::ModbusPointConf(const std::string& FileName):
	ConfigParser(FileName)
{
	ProcessFile();
}

template<EventType T>
std::shared_ptr<EventInfo> GetStartVal(const Json::Value& value);

template<>
std::shared_ptr<EventInfo> GetStartVal<EventType::Analog>(const Json::Value& value)
{
	auto event = std::make_shared<EventInfo>(EventType::Analog);

	if(value.asString() == "X")
		event->SetQuality(QualityFlags::COMM_LOST);
	else
	{
		event->SetQuality(QualityFlags::ONLINE);
		event->SetPayload<EventType::Analog>(std::stod(value.asString()));
	}
	return event;
}

template<>
std::shared_ptr<EventInfo> GetStartVal<EventType::Binary>(const Json::Value& value)
{
	auto event = std::make_shared<EventInfo>(EventType::Binary);

	if(value.asString() == "X")
		event->SetQuality(QualityFlags::COMM_LOST);
	else
	{
		event->SetQuality(QualityFlags::ONLINE);
		event->SetPayload<EventType::Binary>(value.asBool());
	}
	return event;
}

template<EventType T>
void ModbusPointConf::ProcessReadGroup(const Json::Value& Ranges, ModbusReadGroupCollection& ReadGroup)
{
	for(Json::ArrayIndex n = 0; n < Ranges.size(); ++n)
	{
		uint32_t pollgroup = 0;
		if(Ranges[n].isMember("PollGroup"))
		{
			pollgroup = Ranges[n]["PollGroup"].asUInt();
		}

		std::shared_ptr<const EventInfo> startval;
		if(Ranges[n].isMember("StartVal"))
		{
			startval = GetStartVal<T>(Ranges[n]["StartVal"]);
		}

		size_t start, stop, offset = 0;
		if(Ranges[n].isMember("IndexOffset"))
			offset = Ranges[n]["IndexOffset"].asInt();

		if(Ranges[n].isMember("Index"))
			start = stop = Ranges[n]["Index"].asUInt();
		else if(Ranges[n]["Range"].isMember("Start") && Ranges[n]["Range"].isMember("Stop"))
		{
			start = Ranges[n]["Range"]["Start"].asUInt();
			stop = Ranges[n]["Range"]["Stop"].asUInt();
			if (start > stop)
			{
				if(auto log = odc::spdlog_get("ModbusPort"))
					log->error("Invalid range: Start > Stop: '{}'", Ranges[n].toStyledString());
				continue;
			}
		}
		else
		{
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", Ranges[n].toStyledString());
			continue;
		}

		ReadGroup.emplace_back(start,stop-start+1,pollgroup,startval,offset);
	}
}

void ModbusPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	if(JSONRoot.isMember("BitIndicies"))
		ProcessReadGroup<EventType::Binary>(JSONRoot["BitIndicies"], BitIndicies);
	if(JSONRoot.isMember("InputBitIndicies"))
		ProcessReadGroup<EventType::Binary>(JSONRoot["InputBitIndicies"], InputBitIndicies);
	if(JSONRoot.isMember("RegIndicies"))
		ProcessReadGroup<EventType::Analog>(JSONRoot["RegIndicies"], RegIndicies);
	if(JSONRoot.isMember("InputRegIndicies"))
		ProcessReadGroup<EventType::Analog>(JSONRoot["InputRegIndicies"], InputRegIndicies);

	if(JSONRoot.isMember("PollGroups"))
	{
		auto jPollGroups = JSONRoot["PollGroups"];
		for(Json::ArrayIndex n = 0; n < jPollGroups.size(); ++n)
		{
			if(!jPollGroups[n].isMember("ID"))
			{
				if(auto log = odc::spdlog_get("ModbusPort"))
					log->error("Poll group missing ID : '{}'", jPollGroups[n].toStyledString());
				continue;
			}
			if(!jPollGroups[n].isMember("PollRate"))
			{
				if(auto log = odc::spdlog_get("ModbusPort"))
					log->error("Poll group missing PollRate : '{}'", jPollGroups[n].toStyledString());
				continue;
			}

			uint32_t PollGroupID = jPollGroups[n]["ID"].asUInt();
			uint32_t pollrate = jPollGroups[n]["PollRate"].asUInt();

			if(PollGroupID == 0)
			{
				if(auto log = odc::spdlog_get("ModbusPort"))
					log->error("Poll group 0 is reserved (do not poll) : '{}'", jPollGroups[n].toStyledString());
				continue;
			}

			if(PollGroups.count(PollGroupID) > 0)
			{
				if(auto log = odc::spdlog_get("ModbusPort"))
					log->error("Duplicate poll group ignored : '{}'", jPollGroups[n].toStyledString());
				continue;
			}

			PollGroups.emplace(std::piecewise_construct,std::forward_as_tuple(PollGroupID),std::forward_as_tuple(PollGroupID,pollrate));
		}
	}
}



