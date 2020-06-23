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

#include "MD3PointConf.h"
#include <algorithm>
#include <map>
#include <memory>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <regex>

using namespace odc;

MD3PointConf::MD3PointConf(const std::string & _FileName, const Json::Value& ConfOverrides):
	ConfigParser(_FileName, ConfOverrides),
	FileName(_FileName)
{
	LOGDEBUG("Conf processing file - {}",FileName);
	ProcessFile(); // This should call process elements below?
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values
	LOGDEBUG("MD3 Conf processing");

	try
	{
		// PollGroups must be processed first
		if (JSONRoot.isMember("PollGroups"))
		{
			const auto PollGroups = JSONRoot["PollGroups"];
			ProcessPollGroups(PollGroups);
		}

		if (JSONRoot.isMember("Analogs"))
		{
			const auto Analogs = JSONRoot["Analogs"];
			LOGDEBUG("Conf processed - Analog Points");
			ProcessAnalogCounterPoints(Analog, Analogs);
		}
		if (JSONRoot.isMember("Counters"))
		{
			const auto Counters = JSONRoot["Counters"];
			LOGDEBUG("Conf processed - Counter Points");
			ProcessAnalogCounterPoints(Counter, Counters);
		}
		if (JSONRoot.isMember("AnalogControls"))
		{
			const auto AnalogControls = JSONRoot["AnalogControls"];
			LOGDEBUG("Conf processed - AnalogControls");
			ProcessAnalogCounterPoints(AnalogControl, AnalogControls);
		}

		if (JSONRoot.isMember("Binaries"))
		{
			const auto Binaries = JSONRoot["Binaries"];
			LOGDEBUG("Conf processed - Binary Points");
			ProcessBinaryPoints(Binary, Binaries);
		}

		if (JSONRoot.isMember("BinaryControls"))
		{
			const auto BinaryControls = JSONRoot["BinaryControls"];
			LOGDEBUG("Conf processed -Binary Controls");
			ProcessBinaryPoints(BinaryControl, BinaryControls);
		}

		if (JSONRoot.isMember("NewDigitalCommands"))
		{
			NewDigitalCommands = JSONRoot["NewDigitalCommands"].asBool();
			LOGDEBUG("Conf processed - NewDigitalCommands - {}",NewDigitalCommands);
		}
		if (JSONRoot.isMember("OverrideOldTimeStamps"))
		{
			OverrideOldTimeStamps = JSONRoot["OverrideOldTimeStamps"].asBool();
			LOGDEBUG("Conf processed - OverrideOldTimeStamps - {}",OverrideOldTimeStamps);
		}
		if (JSONRoot.isMember("UpdateAnalogCounterTimeStamps"))
		{
			OverrideOldTimeStamps = JSONRoot["UpdateAnalogCounterTimeStamps"].asBool();
			LOGDEBUG("Conf processed - UpdateAnalogCounterTimeStamps - {}",UpdateAnalogCounterTimeStamps);
		}
		if (JSONRoot.isMember("StandAloneOutstation"))
		{
			StandAloneOutstation = JSONRoot["StandAloneOutstation"].asBool();
			LOGDEBUG("Conf processed - StandAloneOutstation - {}",StandAloneOutstation);
		}
		if (JSONRoot.isMember("MD3CommandTimeoutmsec"))
		{
			MD3CommandTimeoutmsec = JSONRoot["MD3CommandTimeoutmsec"].asUInt();
			LOGDEBUG("Conf processed - MD3CommandTimeoutmsec - {}",MD3CommandTimeoutmsec);
		}
		if (JSONRoot.isMember("MD3CommandRetries"))
		{
			MD3CommandRetries = JSONRoot["MD3CommandRetries"].asUInt();
			LOGDEBUG("Conf processed - MD3CommandRetries - {}",MD3CommandRetries);
		}
	}
	catch (const std::exception& e)
	{
		LOGERROR("Exception Caught while processing {}, {} - configuration not loaded", FileName, e.what());
	}
	LOGDEBUG("End Conf processing");
}

// This method must be processed before points are loaded
void MD3PointConf::ProcessPollGroups(const Json::Value & JSONNode)
{
	LOGDEBUG("Conf processing - PollGroups");

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		if (!JSONNode[n].isMember("ID"))
		{
			LOGERROR("Poll group missing ID : {}",JSONNode[n].toStyledString() );
			continue;
		}
		if (!JSONNode[n].isMember("PollRate"))
		{
			LOGERROR("Poll group missing PollRate : {}", JSONNode[n].toStyledString());
			continue;
		}
		if (!JSONNode[n].isMember("PointType"))
		{
			LOGERROR("Poll group missing PollType (Binary, Analog or TimeSetCommand, NewTimeSetCommand, SystemFlagScan) : {}", JSONNode[n].toStyledString());
			continue;
		}

		uint32_t PollGroupID = JSONNode[n]["ID"].asUInt();
		uint32_t pollrate = JSONNode[n]["PollRate"].asUInt();

		if (PollGroupID == 0)
		{
			LOGERROR("Poll group 0 is reserved (do not poll) : {}", JSONNode[n].toStyledString());
			continue;
		}

		if (PollGroups.count(PollGroupID) > 0)
		{
			LOGERROR("Duplicate poll group ignored : {}", JSONNode[n].toStyledString());
			continue;
		}

		PollGroupType polltype = BinaryPoints; // Default to Binary

		if (iequals(JSONNode[n]["PointType"].asString(), "Analog"))
		{
			polltype = AnalogPoints;
		}
		if (iequals(JSONNode[n]["PointType"].asString(), "Counter"))
		{
			polltype = CounterPoints;
		}
		if (iequals(JSONNode[n]["PointType"].asString(), "TimeSetCommand"))
		{
			polltype = TimeSetCommand;
		}
		if (iequals(JSONNode[n]["PointType"].asString(), "NewTimeSetCommand"))
		{
			polltype = NewTimeSetCommand;
		}
		if (iequals(JSONNode[n]["PointType"].asString(), "SystemFlagScan"))
		{
			polltype = SystemFlagScan;
		}

		bool ForceUnconditional = false;
		if (JSONNode[n].isMember("ForceUnconditional"))
		{
			ForceUnconditional = JSONNode[n]["ForceUnconditional"].asBool();
		}

		bool TimeTaggedDigital = false;
		if (JSONNode[n].isMember("TimeTaggedDigital"))
		{
			TimeTaggedDigital = JSONNode[n]["TimeTaggedDigital"].asBool();
		}

		LOGDEBUG("Conf processed - PollGroup - {}, Rate {}, Type {}, TimeTaggedDigital {}, Force Unconditional Command {}",
			PollGroupID, pollrate, polltype, TimeTaggedDigital, ForceUnconditional);
		PollGroups.emplace(std::piecewise_construct, std::forward_as_tuple(PollGroupID), std::forward_as_tuple(PollGroupID, pollrate, polltype, ForceUnconditional, TimeTaggedDigital));
	}
	LOGDEBUG("Conf processing - PollGroups - Finished");
}

// This method loads both Binary read points, and Binary Control points.
void MD3PointConf::ProcessBinaryPoints(PointType ptype, const Json::Value& JSONNode)
{
	LOGDEBUG("Conf processing - Binary");

	std::string BinaryName;
	if (ptype == Binary)
		BinaryName = "Binary";
	if (ptype == BinaryControl)
		BinaryName = "BinaryControl";

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		uint32_t start, stop;
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
			LOGERROR("{} A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : {}", BinaryName ,JSONNode[n].toStyledString());
			start = 1;
			stop = 0;
			error = true;
		}

		uint32_t module = 0;
		uint32_t offset = 0;
		uint32_t pollgroup = 0;
		BinaryPointType pointtype = BASICINPUT;
		std::string pointtypestring;

		if (JSONNode[n].isMember("Module"))
			module = JSONNode[n]["Module"].asUInt();
		else
		{
			LOGERROR("{} A point needs an \"Module\" : {}", BinaryName ,JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			LOGERROR("{} A point needs an \"Offset\" : {}", BinaryName, JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("PointType"))
		{
			pointtypestring = JSONNode[n]["PointType"].asString();
			if (pointtypestring == "BASICINPUT")
				pointtype = BASICINPUT;
			else if (pointtypestring == "TIMETAGGEDINPUT")
				pointtype = TIMETAGGEDINPUT;
			else if (pointtypestring == "DOMOUTPUT")
				pointtype = DOMOUTPUT;
			else if (pointtypestring == "POMOUTPUT")
				pointtype = POMOUTPUT;
			else if (pointtypestring == "DIMOUTPUT")
				pointtype = DIMOUTPUT;
			else
			{
				LOGERROR("{} A point needs a valid \"PointType\" : {}", BinaryName, JSONNode[n].toStyledString());
				error = true;
			}
		}
		else
		{
			LOGERROR("{} A point needs an \"PointType\" : {}", BinaryName, JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("PollGroup"))
		{
			pollgroup = JSONNode[n]["PollGroup"].asUInt();
		}

		if (!error)
		{
			for (uint32_t index = start; index <= stop; index++)
			{
				auto moduleaddress = static_cast<uint8_t>(module + (index - start + offset) / 16);
				auto channel = static_cast<uint8_t>((offset + (index - start)) % 16);

				bool res = false;

				LOGDEBUG("Adding a {} - Index: {} Module: {} Channel: {}  Point Type : {}", BinaryName, index, moduleaddress, channel, pointtypestring);
				if (ptype == Binary)
				{
					res = PointTable.AddBinaryPointToPointTable(index, moduleaddress, channel, pointtype, pollgroup);
				}
				else if (ptype == BinaryControl)
				{
					res = PointTable.AddBinaryControlPointToPointTable(index, moduleaddress, channel, pointtype, pollgroup);
				}
				else
					LOGERROR("Illegal point type passed to ProcessBinaryPoints");

				if (res)
				{
					// If the point is part of a scan group, add the module address. Don't duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							LOGERROR("{} Poll Group Must Be Defined for use in a Binary point : {}", BinaryName, JSONNode[n].toStyledString());
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
	LOGDEBUG("Conf processing - Binary - Finished");
}

// This method loads both Analog and Counter/Timers. They look functionally similar in MD3
void MD3PointConf::ProcessAnalogCounterPoints(PointType ptype, const Json::Value& JSONNode)
{
	LOGDEBUG("Conf processing - Analog/Counter");
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
			LOGERROR("An analog/counter point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : {}", JSONNode[n].toStyledString());
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
			LOGERROR("A point needs an \"Module\" : {}", JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			LOGERROR("A point needs an \"Offset\" : {}", JSONNode[n].toStyledString());
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
				auto moduleaddress = static_cast<uint8_t>(module + (index - start + offset) / 16);
				auto channel = static_cast<uint8_t>((offset + (index - start)) % 16);
				bool res = false;

				if (ptype == Analog)
				{
					res = PointTable.AddAnalogPointToPointTable(index, moduleaddress, channel, pollgroup);
				}
				else if (ptype == Counter)
				{
					res = PointTable.AddCounterPointToPointTable(index, moduleaddress, channel, pollgroup);
				}
				else if (ptype == AnalogControl)
				{
					res = PointTable.AddAnalogControlPointToPointTable(index, moduleaddress, channel, pollgroup);
				}
				else
					LOGERROR("Illegal point type passed to ProcessAnalogCounterPoints");

				if (res)
				{
					// If the point is part of a scan group, add the module address. Don't duplicate the address.
					if (pollgroup != 0)
					{
						if (PollGroups.count(pollgroup) == 0)
						{
							LOGERROR("Poll Group Must Be Defined for use in an Analog/Counter point : {}",JSONNode[n].toStyledString());
						}
						else
						{
							// This will add the module address to the poll group if missing, and then we add a channel to the current count (->second field)
							// Assume second is 0 the first time it is referenced?
							uint16_t channels = PollGroups[pollgroup].ModuleAddresses[moduleaddress];
							PollGroups[pollgroup].ModuleAddresses[moduleaddress] = channels + 1;
							LOGDEBUG("Added Point {}, {} To Poll Group {}",moduleaddress, channel,pollgroup);

							if (PollGroups[pollgroup].ModuleAddresses.size() > 1)
							{
								LOGERROR("Analog or Counter Poll group {} is configured for more than one MD3 Module address. To scan another address, another poll group must be used.",pollgroup);
							}
						}
					}
				}
			}
		}
	}
	LOGDEBUG("Conf processing - Analog/Counter - Finished");
}
