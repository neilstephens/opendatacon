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
* MD3PointConf.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3CLIENTPORT_H_
#define MD3CLIENTPORT_H_

#include <regex>
#include <algorithm>
#include <memory>
#include <map>
#include "MD3PointConf.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

MD3PointConf::MD3PointConf(std::string FileName, const Json::Value& ConfOverrides) :
	ConfigParser(FileName, ConfOverrides)
{
	ProcessFile();
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values

	if (JSONRoot.isMember("Analogs"))
	{
		const auto Analogs = JSONRoot["Analogs"];
		ProcessAnalogCounterPoints(Analogs, AnalogMD3PointMap, AnalogODCPointMap);
	}

	if (JSONRoot.isMember("Binaries"))
	{
		const auto Binaries = JSONRoot["Binaries"];
		ProcessBinaryPoints(Binaries, BinaryMD3PointMap, BinaryODCPointMap);
	}

	if (JSONRoot.isMember("BinaryControls"))
	{
		const auto BinaryControls = JSONRoot["BinaryControls"];
		ProcessBinaryPoints(BinaryControls, BinaryControlMD3PointMap, BinaryControlODCPointMap);
	}
	if (JSONRoot.isMember("Counters"))
	{
		const auto Counters = JSONRoot["Counters"];
		ProcessAnalogCounterPoints(Counters, CounterMD3PointMap, CounterODCPointMap);
	}
	if (JSONRoot.isMember("PollGroups"))
	{
		auto jPollGroups = JSONRoot["PollGroups"];
		for (Json::ArrayIndex n = 0; n < jPollGroups.size(); ++n)
		{
			if (!jPollGroups[n].isMember("ID"))
			{
				std::cout << "Poll group missing ID : '" << jPollGroups[n].toStyledString() << "'" << std::endl;
				continue;
			}
			if (!jPollGroups[n].isMember("PollRate"))
			{
				std::cout << "Poll group missing PollRate : '" << jPollGroups[n].toStyledString() << "'" << std::endl;
				continue;
			}
			if (!jPollGroups[n].isMember("PointType"))
			{
				std::cout << "Poll group missing PollType (Binary or Analog) : '" << jPollGroups[n].toStyledString() << "'" << std::endl;
				continue;
			}

			uint32_t PollGroupID = jPollGroups[n]["ID"].asUInt();
			uint32_t pollrate = jPollGroups[n]["PollRate"].asUInt();

			if (PollGroupID == 0)
			{
				std::cout << "Poll group 0 is reserved (do not poll) : '" << jPollGroups[n].toStyledString() << "'" << std::endl;
				continue;
			}

			if (PollGroups.count(PollGroupID) > 0)
			{
				std::cout << "Duplicate poll group ignored : '" << jPollGroups[n].toStyledString() << "'" << std::endl;
				continue;
			}

			PollGroupType polltype = BinaryPoints;	// Default to Binary

			if (iequals(jPollGroups[n]["PointType"].asString(), "Analog"))
			{
				polltype = AnalogPoints;
			}

			PollGroups.emplace(std::piecewise_construct, std::forward_as_tuple(PollGroupID), std::forward_as_tuple(PollGroupID, pollrate, polltype));
		}
	}
}

void MD3PointConf::ProcessBinaryPoints(const Json::Value& JSONNode, std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>> &MD3PointMap,
																std::map<uint32_t, std::shared_ptr<MD3BinaryPoint>> &ODCPointMap)
{
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		size_t start, stop;
		if (JSONNode[n].isMember("Index"))
		{
			start = stop = JSONNode[n]["Index"].asUInt();
		}
		else if (JSONNode[n]["Range"].isMember("Start") && JSONNode[n]["Range"].isMember("Stop"))
		{
			start = JSONNode[n]["Range"]["Start"].asUInt();
			stop = JSONNode[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
			error = true;
		}

		uint32_t module = 0;
		uint32_t offset = 0;
		uint32_t pollgroup = 0;

		if (JSONNode[n].isMember("Module"))
			module = JSONNode[n]["Module"].asUInt();
		else
		{
			std::cout << "A point needs an \"Module\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			std::cout << "A point needs an \"Offset\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (JSONNode[n].isMember("PollGroup"))
		{
			pollgroup = JSONNode[n]["PollGroup"].asUInt();
		}

		if (!error)
		{
			for (auto index = start; index <= stop; index++)
			{
				uint16_t moduleaddress = module + ((uint16_t)index - (uint16_t)start + offset) / 16;
				uint8_t channel = (uint8_t)((offset + ((uint16_t)index- (uint16_t)start)) % 16);

				uint16_t md3index = (moduleaddress << 8) | channel;

				if (MD3PointMap.find(md3index) != MD3PointMap.end())
				{
					std::cout << "Duplicate MD3 Index '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else if (ODCPointMap.find(index) != ODCPointMap.end())
				{
					std::cout << "Duplicate ODC Index : '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else
				{
					auto pt = std::make_shared<MD3BinaryPoint>(index, moduleaddress, channel, (uint8_t)pollgroup);
					MD3PointMap[md3index] = pt;
					ODCPointMap[index] = pt;

					// If the point is part of a scan group, add the module address. Dont duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							std::cout << "Poll Group Must Be Defined for use in a point : '" << JSONNode[n].toStyledString() << "'" << std::endl;
						}
						else
						{
							// Control points and binary inputs are processed here.
							// If the map does have an entry for moduleaddress, we just set the second element of the pair (to a non value).
							// If it does not, add the moduleaddress,0 pair to the map - which will be sorted.
							PollGroups[pollgroup].ModuleAddresses[moduleaddress] = 0;
						}
					}
				}
			}
		}
	}
}
void MD3PointConf::ProcessAnalogCounterPoints(const Json::Value& JSONNode, std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> &MD3PointMap,
	std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>> &ODCPointMap)
{
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		size_t start, stop;
		if (JSONNode[n].isMember("Index"))
		{
			start = stop = JSONNode[n]["Index"].asUInt();
		}
		else if (JSONNode[n]["Range"].isMember("Start") && JSONNode[n]["Range"].isMember("Stop"))
		{
			start = JSONNode[n]["Range"]["Start"].asUInt();
			stop = JSONNode[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
			error = true;
		}

		uint32_t module = 0;
		uint32_t offset = 0;
		uint32_t pollgroup = 0;

		if (JSONNode[n].isMember("Module"))
			module = JSONNode[n]["Module"].asUInt();
		else
		{
			std::cout << "A point needs an \"Module\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			std::cout << "A point needs an \"Offset\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (JSONNode[n].isMember("PollGroup"))
		{
			pollgroup = JSONNode[n]["PollGroup"].asUInt();
		}

		if (!error)
		{
			for (auto index = start; index <= stop; index++)
			{
				uint16_t moduleaddress = module + ((uint16_t)index - (uint16_t)start + offset) / 16;
				uint8_t channel = (uint8_t)((offset + ((uint16_t)index - (uint16_t)start)) % 16);

				uint16_t md3index = (moduleaddress << 8) | channel;

				if (MD3PointMap.find(md3index) != MD3PointMap.end())
				{
					std::cout << "Duplicate MD3 Index '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else if (ODCPointMap.find(index) != ODCPointMap.end())
				{
					std::cout << "Duplicate ODC Index : '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else
				{
					auto pt = std::make_shared<MD3AnalogCounterPoint>(index, moduleaddress, channel, pollgroup);
					MD3PointMap[md3index] = pt;
					ODCPointMap[index] = pt;

					// If the point is part of a scan group, add the module address. Dont duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							std::cout << "Poll Group Must Be Defined for use in a point : '" << JSONNode[n].toStyledString() << "'" << std::endl;
						}
						else
						{
							// Control points and binary inputs are processed here.
							// If the map does have an entry for moduleaddress, we just set the second element of the pair (to a non value).
							// If it does not, add the moduleaddress,0 pair to the map - which will be sorted.
							PollGroups[pollgroup].ModuleAddresses[moduleaddress] = 0;
						}
					}
				}
			}
		}
	}
}
#endif