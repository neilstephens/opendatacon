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
* CBPointConf.cpp
*
*  Created on: 13/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#include "CBPointConf.h"
#include <algorithm>
#include <map>
#include <memory>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <regex>

using namespace odc;

CBPointConf::CBPointConf(const std::string & _FileName, const Json::Value& ConfOverrides):
	ConfigParser(_FileName, ConfOverrides),
	FileName(_FileName)
{
	PointTable.SetName(FileName); // Just to make error messages better on parsing. Remove .conf
	LOGDEBUG("Conf processing file - {}",FileName);
	ProcessFile(); // This should call process elements below?
}


void CBPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values
	LOGDEBUG("{} processing",FileName);

	try
	{
		// PollGroups must be processed first
		if (JSONRoot.isMember("PollGroups"))
		{
			const auto PollGroups = JSONRoot["PollGroups"];
			ProcessPollGroups(PollGroups);
		}
		if (JSONRoot.isMember("RemoteStatus"))
		{
			const auto RemoteStatus = JSONRoot["RemoteStatus"];
			ProcessStatusByte(RemoteStatus);
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

		if (JSONRoot.isMember("IsBakerDevice"))
		{
			IsBakerDevice = JSONRoot["IsBakerDevice"].asBool();
			LOGDEBUG("Conf processed - IsBakerDevice - {}", std::to_string(IsBakerDevice));
		}
		if (JSONRoot.isMember("OverrideOldTimeStamps"))
		{
			OverrideOldTimeStamps = JSONRoot["OverrideOldTimeStamps"].asBool();
			LOGDEBUG("Conf processed - OverrideOldTimeStamps - {}", std::to_string(OverrideOldTimeStamps));
		}
		if (JSONRoot.isMember("UpdateAnalogCounterTimeStamps"))
		{
			OverrideOldTimeStamps = JSONRoot["UpdateAnalogCounterTimeStamps"].asBool();
			LOGDEBUG("Conf processed - UpdateAnalogCounterTimeStamps - {}", std::to_string(UpdateAnalogCounterTimeStamps));
		}
		if (JSONRoot.isMember("StandAloneOutstation"))
		{
			StandAloneOutstation = JSONRoot["StandAloneOutstation"].asBool();
			LOGDEBUG("Conf processed - StandAloneOutstation - {}", std::to_string(StandAloneOutstation));
		}
		if (JSONRoot.isMember("CBCommandTimeoutmsec"))
		{
			CBCommandTimeoutmsec = JSONRoot["CBCommandTimeoutmsec"].asUInt();
			LOGDEBUG("Conf processed - CBCommandTimeoutmsec - {}", std::to_string(CBCommandTimeoutmsec));
		}
		if (JSONRoot.isMember("SOEQueueSize"))
		{
			SOEQueueSize = JSONRoot["SOEQueueSize"].asUInt();
			LOGDEBUG("Conf processed - SOEQueueSize - {}", std::to_string(SOEQueueSize));
		}
		if (JSONRoot.isMember("CBCommandRetries"))
		{
			CBCommandRetries = JSONRoot["CBCommandRetries"].asUInt();
			LOGDEBUG("Conf processed - CBCommandRetries - {}", std::to_string(CBCommandRetries));
		}
	}
	catch (const std::exception& e)
	{
		LOGERROR("Exception Caught while processing {}, {} - configuration not loaded", FileName, e.what() );
	}
	LOGDEBUG("{} End  processing",FileName);
}

// This method must be processed before points are loaded
void CBPointConf::ProcessPollGroups(const Json::Value & JSONNode)
{
	LOGDEBUG("Conf processing - PollGroups");

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		if (!JSONNode[n].isMember("ID"))
		{
			LOGERROR("Poll group missing ID : " + JSONNode[n].toStyledString());
			error = true;
		}
		if (!JSONNode[n].isMember("PollRate"))
		{
			LOGERROR("Poll group missing PollRate : "+ JSONNode[n].toStyledString());
			error = true;
		}

		uint32_t PollID = JSONNode[n]["ID"].asUInt();
		uint32_t group = JSONNode[n]["Group"].asUInt();
		uint32_t pollrate = JSONNode[n]["PollRate"].asUInt();

		if ((pollrate == 0) || (pollrate > 9000000))
		{
			LOGERROR("Illegal PollRate, ignoring poll group {}. PollRate :{}",PollID,pollrate);
			error = true;
		}

		if (PollGroups.count(PollID) > 0)
		{
			LOGERROR("Duplicate poll group ignored : "+ JSONNode[n].toStyledString());
			error = true;
		}

		PollGroupType polltype = Scan; // Scan

		if (iequals(JSONNode[n]["PollType"].asString(), "Scan"))
		{
			polltype = Scan;
		}
		else if (iequals(JSONNode[n]["PollType"].asString(), "TimeSetCommand"))
		{
			polltype = TimeSetCommand;
		}
		else if (iequals(JSONNode[n]["PollType"].asString(), "SystemFlagScan"))
		{
			polltype = SystemFlagScan;
		}
		else if (iequals(JSONNode[n]["PollType"].asString(), "SOEScan"))
		{
			polltype = SOEScan;
		}
		else
		{
			LOGERROR("Poll scantype missing : " + JSONNode[n].toStyledString());
			error = true;
		}

		if (!JSONNode[n].isMember("Group") && (polltype != TimeSetCommand) && (polltype != SystemFlagScan))
		{
			// Dont need a group for the time set command, or flag scan
			LOGERROR("Poll group missing Group : " + JSONNode[n].toStyledString());
			error = true;
		}

		bool ForceUnconditional = false;
		if (JSONNode[n].isMember("ForceUnconditional"))
		{
			ForceUnconditional = JSONNode[n]["ForceUnconditional"].asBool();
		}

		if (!error)
		{
			LOGDEBUG("Conf processed - PollID - {} Rate {} Type {} Group {} Force Unconditional PendingCommand {}", std::to_string(PollID), std::to_string(pollrate), std::to_string(polltype), std::to_string(group), std::to_string(ForceUnconditional));

			PollGroups[PollID] = CBPollGroup(PollID, pollrate, polltype, numeric_cast<uint8_t>(group), ForceUnconditional);
		}
		else
		{
			LOGDEBUG("Poll JSON entry missing information - ignoring this entry {}", JSONNode.toStyledString());
		}
	}
	LOGDEBUG("Conf processing - PollGroups - Finished");
}

// This method loads both Binary read points, and Binary Control points.
void CBPointConf::ProcessBinaryPoints(PointType ptype, const Json::Value& JSONNode)
{
	LOGDEBUG("Conf processing - Binary");

	std::string BinaryName;
	if (ptype == Binary)
		BinaryName = "Binary";
	if (ptype == BinaryControl)
		BinaryName = "BinaryControl";

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		try
		{
			//LOGDEBUG("Digital processing {}, {} ", n, JSONNode[n].toStyledString());

			bool error = false;
			uint32_t start, stop; // Will set index from these later
			uint32_t group = 0;
			uint32_t channel = 0;
			BinaryPointType pointtype = DIG;
			std::string pointtypestring = "";
			bool IsSOE = false;
			uint8_t SOEIndex = 0;

			PayloadLocationType payloadlocation;

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
				LOGERROR("A {} point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : : {}", BinaryName, JSONNode[n].toStyledString());
				start = 1;
				stop = 0;
				error = true;
			}

			if (ptype == Binary) // Control points do not have a payload location.
			{
				if (JSONNode[n].isMember("PayloadLocation"))
				{
					error = !ParsePayloadString(JSONNode[n]["PayloadLocation"].asString(), payloadlocation);
				}
				else
				{
					LOGERROR("A Binary point needs a \"PayloadLocation\" : {}", JSONNode[n].toStyledString());
					error = true;
				}
			}
			else
			{
				// For a control point, the payload location is always 1B - the command comes as one block only
				ParsePayloadString("1B", payloadlocation);
			}

			if (JSONNode[n].isMember("Group"))
				group = JSONNode[n]["Group"].asUInt();
			else
			{
				LOGERROR("A {} point needs a \"Group\" : {}", BinaryName, JSONNode[n].toStyledString());
				error = true;
			}

			if (JSONNode[n].isMember("Channel"))
			{
				channel = JSONNode[n]["Channel"].asUInt();
			}
			else
			{
				LOGERROR("A {} point needs a \"Channel\" : {}", BinaryName, JSONNode[n].toStyledString());
				error = true;
			}

			if (JSONNode[n].isMember("Type"))
			{
				pointtypestring = JSONNode[n]["Type"].asString();
				if (pointtypestring == "DIG")
					pointtype = DIG;
				else if (pointtypestring == "MCA")
					pointtype = MCA;
				else if (pointtypestring == "MCB")
					pointtype = MCB;
				else if (pointtypestring == "MCC")
					pointtype = MCC;
				else if (pointtypestring == "CONTROL")
					pointtype = BINCONTROL;
				else
				{
					LOGERROR("A {} point needs a valid \"Type\" : {}", BinaryName, JSONNode[n].toStyledString());
					error = true;
				}
			}
			else
			{
				LOGERROR("A {} point needs a \"Type\" : {}", BinaryName, JSONNode[n].toStyledString());
				error = true;
			}

			if (pointtype != BINCONTROL) // Do SOE for MCx or DIG
			{
				if (JSONNode[n].isMember("SOE"))
				{
					if (JSONNode[n]["SOE"].isMember("Index"))
					{
						IsSOE = true;
						SOEIndex = JSONNode[n]["SOE"]["Index"].asUInt();

						if (SOEIndex > 120)
						{
							LOGERROR("\"SOEIndex\" must be 0 to 120 : {}", JSONNode[n].toStyledString());
							error = true;
						}
					}
					else
					{
						LOGERROR("{} point - If SOE exists, it needs an \"Index\" : {}", BinaryName, JSONNode[n].toStyledString());
						error = true;
					}
				}
			}
			if (!error)
			{
				for (uint32_t index = start; index <= stop; index++)
				{
					auto currentchannel = static_cast<uint8_t>(channel + (index - start));

					bool res = false;

					if (ptype == Binary)
					{
						// Do some sanity checks
						if ((pointtype == DIG) && ((currentchannel < 1) || (currentchannel > 12)))
						{
							LOGERROR("A binary point channel for point type DIG must be between 1 and 12 {}", std::to_string(currentchannel));
						}
						else if (((pointtype == MCA) || (pointtype == MCB) || (pointtype == MCC)) && ((currentchannel < 1) || (currentchannel > 6)))
						{
							LOGERROR("A binary point channel for point type MCA/MCB/MCC must be between 1 and 6 {}", std::to_string(currentchannel));
						}
						else if (pointtype == BINCONTROL)
						{
							LOGERROR("A binary input cannot have type CONTROL {}", std::to_string(currentchannel));
						}
						else
						{
							// Only add the point if it passes
							res = PointTable.AddBinaryPointToPointTable(index, numeric_cast<uint8_t>(group), currentchannel, payloadlocation, pointtype, IsSOE, SOEIndex);
						}
					}
					else if (ptype == BinaryControl)
					{
						if (pointtype != BINCONTROL)
						{
							LOGERROR("A binary control can only have type CONTROL {}", std::to_string(currentchannel));
						}
						else if ((currentchannel < 1) || (currentchannel > 12))
						{
							LOGERROR("A binary control channel must be between 1 and 12 {}", std::to_string(currentchannel));
						}
						else
						{
							// Only add the point if it passes
							res = PointTable.AddBinaryControlPointToPointTable(index, numeric_cast<uint8_t>(group), currentchannel, pointtype);
						}
					}
					else
						LOGERROR("Illegal point type passed to ProcessBinaryPoints");

					if (res)
					{
						std::string SOEString = "";
						if (IsSOE)
							SOEString = " SOEIndex : " + std::to_string(SOEIndex);

						std::string payloadstring = (BinaryName == "BinaryControl") ? "C" : payloadlocation.to_string();
						// The poll group now only has a group number. We need a Group structure to have links to all the points so we can collect them easily.
						LOGDEBUG("Adding a {} - Index: {} Group: {} Channel: {}  Point Type : {} Payload Location : {} {}",
							BinaryName, std::to_string(index), std::to_string(group), std::to_string(currentchannel), pointtypestring, payloadstring, SOEString);
					}
					SOEIndex++;
				}
			}
		}
		catch (const std::exception& e)
		{
			LOGERROR("Exception Caught while processing {}, {} - configuration not loaded", JSONNode[n].toStyledString(), e.what());
			throw e;
		}
	}
	LOGDEBUG("Conf processing - Binary - Finished");
}
// This method loads status byte location in the group.
void CBPointConf::ProcessStatusByte(const Json::Value& JSONNode)
{
	LOGDEBUG("Conf processing - Remote Status Byte");

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		uint32_t group = 0;
		uint32_t channel = 0;
		PayloadLocationType payloadlocation;

		if (JSONNode[n].isMember("PayloadLocation"))
		{
			error = !ParsePayloadString(JSONNode[n]["PayloadLocation"].asString(), payloadlocation);
		}
		else
		{
			LOGERROR("A point needs a \"PayloadLocation\" : " + JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Group"))
			group = JSONNode[n]["Group"].asUInt();
		else
		{
			LOGERROR(" A point needs a \"Group\" : " + JSONNode[n].toStyledString());
			error = true;
		}

		if (JSONNode[n].isMember("Channel"))
			channel = JSONNode[n]["Channel"].asUInt();
		else
		{
			LOGERROR(" A point needs a \"Channel\" : " + JSONNode[n].toStyledString());
			error = true;
		}

		if (!error)
		{
			if (PointTable.AddStatusByteToCBMap(numeric_cast<uint8_t>(group), numeric_cast<uint8_t>(channel), payloadlocation))
			{
				// The poll group now only has a group number. We need a Group structure to have links to all the points so we can collect them easily.
				LOGDEBUG("Adding a Status Byte - Group: " + std::to_string(group) + " Channel: " + std::to_string(channel) + " Payload Location: " + payloadlocation.to_string());
			}
		}
	}
	LOGDEBUG("Conf processing - Status Byte - Finished");
}

// This method loads both Analog and Counter/Timers. They look functionally similar in CB
void CBPointConf::ProcessAnalogCounterPoints(PointType ptype, const Json::Value& JSONNode)
{
	std::string Name("None");

	if (ptype == Analog)
		Name = "Analog";
	if (ptype == Counter)
		Name = "Counter";
	if (ptype == AnalogControl)
		Name = "AnalogControl";

	LOGDEBUG("Conf processing - {}",Name);
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		size_t index = 0;
		uint32_t group = 0;
		uint32_t channel = 0;
		PayloadLocationType payloadlocation;
		std::string pointtypestring = "";
		AnalogCounterPointType pointtype = ANA;

		if (JSONNode[n].isMember("Index"))
		{
			index = JSONNode[n]["Index"].asUInt();
		}
		else
		{
			throw std::invalid_argument("A point needs an \"Index\" : " + JSONNode[n].toStyledString());
		}

		if (ptype != AnalogControl) // Control points do not have a payload location.
		{
			if (JSONNode[n].isMember("PayloadLocation"))
			{
				if (!ParsePayloadString(JSONNode[n]["PayloadLocation"].asString(), payloadlocation))
				{
					throw std::invalid_argument("Failed to Parse \"PayloadLocation\" : " + JSONNode[n].toStyledString());
				}
			}
			else
			{
				throw std::invalid_argument("A point needs a \"PayloadLocation\" : " + JSONNode[n].toStyledString());
			}
		}

		if (JSONNode[n].isMember("Group"))
			group = JSONNode[n]["Group"].asUInt();
		else
		{
			throw std::invalid_argument("A point needs a \"Group\" : " + JSONNode[n].toStyledString());
		}

		if (JSONNode[n].isMember("Channel"))
			channel = JSONNode[n]["Channel"].asUInt();
		else
		{
			throw std::invalid_argument("A point needs a \"Channel\" : " + JSONNode[n].toStyledString());
		}

		if (JSONNode[n].isMember("Type"))
		{
			pointtypestring = JSONNode[n]["Type"].asString();
			if (pointtypestring == "ANA")
				pointtype = ANA;
			else if (pointtypestring == "ANA6")
				pointtype = ANA6;
			else if (pointtypestring == "ACC12")
				pointtype = ACC12;
			else if (pointtypestring == "ACC24")
				pointtype = ACC24;
			else if (pointtypestring == "CONTROL")
				pointtype = ANACONTROL;
			else
			{
				throw std::invalid_argument("A point needs a valid \"Type\" : " + JSONNode[n].toStyledString());
			}
		}
		else
		{
			throw std::invalid_argument("A point needs a \"Type\" : " + JSONNode[n].toStyledString());
		}

		bool res = false;

		if (ptype == Analog)
		{
			// Do some sanity checks
			if ((pointtype == ANA) && (channel != 1))
			{
				LOGERROR("An Analog point channel for point type ANA must be 1 - " + JSONNode[n].toStyledString());
			}
			else if ((pointtype == ANA6) && ((channel < 1) || (channel > 2)))
			{
				LOGERROR("An analog point channel for point type ANA6 must be between 1 and 2 - " + JSONNode[n].toStyledString());
			}
			else if ((pointtype == ACC12) || (pointtype == ACC24))
			{
				LOGERROR("An Analog input cannot have type ACC12 or ACC24 " + JSONNode[n].toStyledString());
			}
			else
			{
				// Only add the point if it passes
				res = PointTable.AddAnalogPointToPointTable(index, numeric_cast<uint8_t>(group), numeric_cast<uint8_t>(channel), payloadlocation, pointtype);
			}
		}
		else if (ptype == Counter)
		{
			// Do some sanity checks
			if (((pointtype == ACC12) || (pointtype == ACC24)) && (channel != 1))
			{
				LOGERROR("A Counter input only have a channel of 1 - " + JSONNode[n].toStyledString());
			}
			else if ((pointtype == ANA6) || (pointtype == ANA))
			{
				LOGERROR("A Counter point cannot have a type ANA6 or ANA - " + JSONNode[n].toStyledString());
			}
			else
			{
				// Only add the point if it passes
				res = PointTable.AddCounterPointToPointTable(index, numeric_cast<uint8_t>(group), numeric_cast<uint8_t>(channel), payloadlocation, pointtype);
			}
		}
		else if (ptype == AnalogControl)
		{
			if (pointtype != ANACONTROL)
			{
				LOGERROR("An analogcontrol point must be type CONTROL - "+JSONNode[n].toStyledString());
			}
			else if ((channel < 1) || (channel > 2))
			{
				LOGERROR("An analogcontrol point channel for point type CONTROL must be between 1 and 2 -"+JSONNode[n].toStyledString());
			}
			else
			{
				res = PointTable.AddAnalogControlPointToPointTable(index, numeric_cast<uint8_t>(group), numeric_cast<uint8_t>(channel), pointtype);
			}
		}
		else
			throw std::invalid_argument("Illegal point type passed to ProcessAnalogCounterPoints - " + JSONNode[n].toStyledString());

		if (res)
		{
			std::string payloadstring = (Name == "AnalogControl") ? "C" : payloadlocation.to_string();

			// The poll group now only has a group number. We need a Group structure to have links to all the points so we can collect them easily.
			LOGDEBUG("Adding an {} - Index: {} Group: {} Channel: {}  Point Type : {} Payload Location : {}",
				Name, std::to_string(index), std::to_string(group), std::to_string(channel), pointtypestring, payloadstring);
		}
	}
	LOGDEBUG("Conf processing - Analog/Counter - Finished");
}
// The string will have "2", or "2B" or "3A" or "15A". The number 1 to 15, the letter A/B/or nothing
bool CBPointConf::ParsePayloadString(const std::string &pl, PayloadLocationType& payloadlocation)
{
	payloadlocation = PayloadLocationType(); // Error value by default

	if (pl.size() < 2)
	{
		LOGERROR("Payload string needs to be a minimum of 2 characters. Needs to be '2A' or '15B' in format");
		return false;
	}

	// First character
	if ((pl[0] >= '1') && (pl[0] <= '9'))
	{
		payloadlocation.Packet = numeric_cast<uint8_t>(pl[0] - '0'); // 1 to 9
	}
	else
	{
		LOGERROR("Payload string first character needs to be 1 to 9 Needs to be '3' or '2B' in format");
		return false;
	}

	// So second character
	if ((pl[1] >= '0') && (pl[1] <= '6') && (pl[0] == '1'))
	{
		// Have a second number, the first must be '1'
		payloadlocation.Packet = numeric_cast<uint8_t>(pl[1] - '0' + 10);
	}
	else if (pl[1] == 'A')
	{
		payloadlocation.Position = PayloadABType::PositionA;
		if (payloadlocation.Packet == 1)
		{
			LOGERROR("Cannot have a payload location of '1A'. This is reserved for the Station/Group/Function information");
			return false;
		}
		return true;
	}
	else if (pl[1] == 'B')
	{
		payloadlocation.Position = PayloadABType::PositionB;
		return true;
	}
	else
	{
		LOGERROR("Payload string second character not correct. Needs to be '2B' or '16B' in format");
		return false;
	}

	// Do we have a third character? To get to here we must...
	if (pl[2] == 'A')
	{
		payloadlocation.Position = PayloadABType::PositionA;
		return true;
	}
	if (pl[2] == 'B')
	{
		payloadlocation.Position = PayloadABType::PositionB;
		return true;
	}

	LOGERROR("Payload string third character not correct. Needs to be A or B as in '16B' in format");
	return false;
}

