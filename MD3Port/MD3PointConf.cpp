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

MD3PointConf::MD3PointConf(std::string FileName, const Json::Value& ConfOverrides):
	ConfigParser(FileName, ConfOverrides)
{
	ProcessFile(); // This should call process elements below?
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values

	// PollGroups must be processed first
	if (JSONRoot.isMember("PollGroups"))
	{
		const auto PollGroups = JSONRoot["PollGroups"];
		ProcessPollGroups(PollGroups);
	}

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
	if (JSONRoot.isMember("AnalogControls"))
	{
		const auto AnalogControls = JSONRoot["AnalogControls"];
		ProcessAnalogCounterPoints(AnalogControls, AnalogControlMD3PointMap, AnalogControlODCPointMap);
	}

	// TimeSet Point Configuration
	if (JSONRoot.isMember("TimeSetPoint") && JSONRoot["TimeSetPoint"].isMember("Index"))
	{
		TimeSetPoint.first = opendnp3::AnalogOutputDouble64(0); // Default to 0 - we know as unset - will never be used in operation.
		TimeSetPoint.second = JSONRoot["TimeSetPoint"]["Index"].asUInt();
	}
	else
	{
		TimeSetPoint.first = opendnp3::AnalogOutputDouble64(0); // Default to 0 - we know as unset - will never be used in operation.
		TimeSetPoint.second = 64000;
		LOGERROR("TimeSetPoint must be defined and have an Index value - defaulting to 64000");
	}

	// SystemSignOnPoint Point Configuration
	if (JSONRoot.isMember("SystemSignOnPoint") && JSONRoot["SystemSignOnPoint"].isMember("Index"))
	{
		SystemSignOnPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		SystemSignOnPoint.second = JSONRoot["SystemSignOnPoint"]["Index"].asUInt();
	}
	else
	{
		SystemSignOnPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		SystemSignOnPoint.second = 64001;
		LOGERROR("SystemSignOnPoint must be defined and have an Index value - defaulting to 64001");
	}
	// FreezeResetCountersPoint Point Configuration
	if (JSONRoot.isMember("FreezeResetCountersPoint") && JSONRoot["FreezeResetCountersPoint"].isMember("Index"))
	{
		FreezeResetCountersPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		FreezeResetCountersPoint.second = JSONRoot["FreezeResetCountersPoint"]["Index"].asUInt();
	}
	else
	{
		FreezeResetCountersPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		FreezeResetCountersPoint.second = 64002;
		LOGERROR("FreezeResetCountersPoint must be defined and have an Index value - defaulting to 64002");
	}
	// POMControlPoint Point Configuration
	if (JSONRoot.isMember("POMControlPoint") && JSONRoot["POMControlPoint"].isMember("Index"))
	{
		POMControlPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		POMControlPoint.second = JSONRoot["POMControlPoint"]["Index"].asUInt();
	}
	else
	{
		POMControlPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		POMControlPoint.second = 64003;
		LOGERROR("POMControlPoint must be defined and have an Index value - defaulting to 64003");
	}
	// DOMControlPoint Point Configuration
	if (JSONRoot.isMember("DOMControlPoint") && JSONRoot["DOMControlPoint"].isMember("Index"))
	{
		DOMControlPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		DOMControlPoint.second = JSONRoot["DOMControlPoint"]["Index"].asUInt();
	}
	else
	{
		DOMControlPoint.first = opendnp3::AnalogOutputInt32(0); // Default to 0 - we know as unset - will never be used in operation.
		DOMControlPoint.second = 64004;
		LOGERROR("DOMControlPoint must be defined and have an Index value - defaulting to 64004");
	}

	if (JSONRoot.isMember("NewDigitalCommands"))
	{
		NewDigitalCommands = JSONRoot["NewDigitalCommands"].asBool();
	}
	if (JSONRoot.isMember("StandAloneOutstation"))
	{
		StandAloneOutstation = JSONRoot["StandAloneOutstation"].asBool();
	}
	if (JSONRoot.isMember("MD3CommandTimeoutmsec"))
	{
		MD3CommandTimeoutmsec = JSONRoot["MD3CommandTimeoutmsec"].asUInt();
	}
	if (JSONRoot.isMember("MD3CommandRetries"))
	{
		MD3CommandRetries = JSONRoot["MD3CommandRetries"].asUInt();
	}
}

// This method must be processed before points are loaded
void MD3PointConf::ProcessPollGroups(const Json::Value & JSONNode)
{
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		if (!JSONNode[n].isMember("ID"))
		{
			LOGERROR("Poll group missing ID : "+JSONNode[n].toStyledString() );
			continue;
		}
		if (!JSONNode[n].isMember("PollRate"))
		{
			LOGERROR("Poll group missing PollRate : "+ JSONNode[n].toStyledString());
			continue;
		}
		if (!JSONNode[n].isMember("PointType"))
		{
			LOGERROR("Poll group missing PollType (Binary or Analog) : "+ JSONNode[n].toStyledString());
			continue;
		}

		uint32_t PollGroupID = JSONNode[n]["ID"].asUInt();
		uint32_t pollrate = JSONNode[n]["PollRate"].asUInt();

		if (PollGroupID == 0)
		{
			LOGERROR("Poll group 0 is reserved (do not poll) : "+ JSONNode[n].toStyledString());
			continue;
		}

		if (PollGroups.count(PollGroupID) > 0)
		{
			LOGERROR("Duplicate poll group ignored : "+ JSONNode[n].toStyledString());
			continue;
		}

		PollGroupType polltype = BinaryPoints; // Default to Binary

		if (iequals(JSONNode[n]["PointType"].asString(), "Analog"))
		{
			polltype = AnalogPoints;
		}

		PollGroups.emplace(std::piecewise_construct, std::forward_as_tuple(PollGroupID), std::forward_as_tuple(PollGroupID, pollrate, polltype));
	}
}

// This method loads both Binary read points, and Binary Control points.
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
			LOGERROR("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : "+ JSONNode[n].toStyledString());
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
			LOGERROR("A point needs an \"Module\" : "+JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			LOGERROR("A point needs an \"Offset\" : "+ JSONNode[n].toStyledString());
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
				uint8_t moduleaddress = module + ((uint16_t)index - (uint16_t)start + offset) / 16;
				uint8_t channel = (uint8_t)((offset + ((uint16_t)index- (uint16_t)start)) % 16);

				uint16_t md3index = ((uint16_t)moduleaddress << 8) | channel;

				if (MD3PointMap.find(md3index) != MD3PointMap.end())
				{
					LOGERROR("Duplicate MD3 Index "+ JSONNode[n].toStyledString());
				}
				else if (ODCPointMap.find(index) != ODCPointMap.end())
				{
					LOGERROR("Duplicate ODC Index : "+JSONNode[n].toStyledString());
				}
				else
				{
					auto pt = std::make_shared<MD3BinaryPoint>(index, moduleaddress, channel, (uint8_t)pollgroup);
					MD3PointMap[md3index] = pt;
					ODCPointMap[index] = pt;

					// If the point is part of a scan group, add the module address. Don't duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							LOGERROR("Poll Group Must Be Defined for use in a Binary point : "+ JSONNode[n].toStyledString());
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

// This method loads both Analog and Counter/Timers. They look functionally similar in MD3
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
			LOGERROR("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : "+ JSONNode[n].toStyledString());
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
			LOGERROR("A point needs an \"Module\" : "+ JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			LOGERROR("A point needs an \"Offset\" : "+ JSONNode[n].toStyledString());
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
				uint8_t moduleaddress = module + ((uint16_t)index - (uint16_t)start + offset) / 16;
				uint8_t channel = (uint8_t)((offset + ((uint16_t)index - (uint16_t)start)) % 16);

				uint16_t md3index = ((uint16_t)moduleaddress << 8) | channel;

				if (MD3PointMap.find(md3index) != MD3PointMap.end())
				{
					LOGERROR("Duplicate MD3 Index "+ JSONNode[n].toStyledString());
				}
				else if (ODCPointMap.find(index) != ODCPointMap.end())
				{
					LOGERROR("Duplicate ODC Index : "+ JSONNode[n].toStyledString());
				}
				else
				{
					auto pt = std::make_shared<MD3AnalogCounterPoint>(index, moduleaddress, channel, pollgroup);
					MD3PointMap[md3index] = pt;
					ODCPointMap[index] = pt;

					// If the point is part of a scan group, add the module address. Don't duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							LOGERROR("Poll Group Must Be Defined for use in an Analog point : "+ JSONNode[n].toStyledString());
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