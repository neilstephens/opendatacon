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
 * FileTransferPort.cpp
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#include "FileTransferPort.h"
#include <opendatacon/util.h>

FileTransferPort::FileTransferPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<FileTransferPortConf>();
	ProcessFile();
}

void FileTransferPort::Enable()
{}
void FileTransferPort::Disable()
{}
void FileTransferPort::Build()
{}
void FileTransferPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(auto log = spdlog::get("FileTransferPort"))
		log->trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);

	(*pStatusCallback)(CommandStatus::SUCCESS);
}

void FileTransferPort::ProcessElements(const Json::Value& JSONRoot)
{
	auto log = spdlog::get("FileTransferPort");
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(JSONRoot.isMember("Direction"))
	{
		const auto dir = JSONRoot["Direction"].asString();
		if(dir == "RX")
			pConf->Direction = TransferDirection::RX;
		else if(dir == "TX")
			pConf->Direction = TransferDirection::TX;
		else if(log)
			log->error("{}: Direction should be either 'Rx' or 'Tx', not '{}'", Name, dir);
	}
	if(JSONRoot.isMember("Directory"))
	{
		pConf->Directory = JSONRoot["Directory"].asString();
	}
	if(JSONRoot.isMember("FilenameRegex"))
	{
		auto regx_string = JSONRoot["FilenameRegex"].asString();
		try
		{
			pConf->pFilenameRegex = std::make_unique<std::regex>(regx_string,std::regex::extended);
		}
		catch (std::exception& e)
		{
			if(log) log->error("{}: Problem using '{}' as regex: {}", Name, regx_string,e.what());
			pConf->pFilenameRegex = nullptr;
		}
	}
	if(JSONRoot.isMember("Recursive"))
	{
		pConf->Recursive = JSONRoot["Recursive"].asBool();
	}
	if(JSONRoot.isMember("FileNameTransmission"))
	{
		Json::Value jFNTX = JSONRoot["FileNameTransmission"];
		if(jFNTX.isMember("Index") && jFNTX.isMember("MatchGroup"))
		{
			pConf->FileNameTransmissionEvent = std::make_shared<EventInfo>(EventType::OctetString, jFNTX["Index"].asUInt());
			pConf->FileNameTransmissionMatchGroup = jFNTX["MatchGroup"].asUInt();
		}
		else if(log)
			log->error("{}: FileNameTransmission needs 'Index' and 'MatchGroup' configuration members. Got: {}", Name, jFNTX.toStyledString());
	}
	if(JSONRoot.isMember("SequenceIndexRange"))
	{
		Json::Value jSIR = JSONRoot["SequenceIndexRange"];
		if(jSIR.isMember("Start") && jSIR.isMember("Stop"))
		{
			pConf->SequenceIndexStart = jSIR["Start"].asUInt();
			pConf->SequenceIndexStop = jSIR["Stop"].asUInt();
		}
		else if(log)
			log->error("{}: SequenceIndexRange needs 'Start' and 'Stop' configuration members. Got: {}", Name, jSIR.toStyledString());
	}
	if(JSONRoot.isMember("TransferTriggers"))
	{
		auto jTXTrigss = JSONRoot["TransferTriggers"];
		if(jTXTrigss.isArray())
		{
			for(auto jTXTrig : jTXTrigss)
			{
				if(jTXTrig.isMember("Type"))
				{
					const auto typ = jTXTrig["Type"].asString();
					if(typ == "Periodic")
					{
						if(jTXTrig.isMember("Periodms"))
						{
							pConf->TransferTriggers.emplace_back();
							pConf->TransferTriggers.back().Type = TriggerType::Periodic;
							pConf->TransferTriggers.back().Periodms = jTXTrig["Periodms"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type Periodic needs 'Periodms'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "BinaryControl")
					{
						if(jTXTrig.isMember("Index"))
						{
							pConf->TransferTriggers.emplace_back();
							pConf->TransferTriggers.back().Type = TriggerType::BinaryControl;
							pConf->TransferTriggers.back().Index = jTXTrig["Index"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type BinaryControl needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "AnalogControl")
					{
						if(jTXTrig.isMember("Index") && jTXTrig.isMember("Value"))
						{
							pConf->TransferTriggers.emplace_back();
							pConf->TransferTriggers.back().Type = TriggerType::AnalogControl;
							pConf->TransferTriggers.back().Index = jTXTrig["Index"].asUInt();
							pConf->TransferTriggers.back().Value = jTXTrig["Value"].asInt64();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type BinaryControl needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "OctetStringPath")
					{
						if(jTXTrig.isMember("Index"))
						{
							pConf->TransferTriggers.emplace_back();
							pConf->TransferTriggers.back().Type = TriggerType::OctetStringPath;
							pConf->TransferTriggers.back().Index = jTXTrig["Index"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type OctetStringPath needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else
					{
						if(log)
							log->error("{}: Invalid TransferTrigger Type '{}'", Name, typ);
						continue;
					}
					if(jTXTrig.isMember("OnlyWhenModified"))
					{
						pConf->TransferTriggers.back().OnlyWhenModified = jTXTrig["OnlyWhenModified"].asBool();
					}
				}
				else if(log)
					log->error("{}: A TransferTrigger needs a 'Type'. Got: {}", Name, jTXTrig.toStyledString());
			}
		}
		else if (log)
			log->error("{}: TransferTriggers is meant to be a JSON array, got {}", Name, jTXTrigss.toStyledString());

	}
	if(JSONRoot.isMember("Filename"))
	{
		const auto jFN = JSONRoot["Filename"];
		if(jFN.isMember("InitialName"))
		{
			pConf->FilenameInfo.InitialName = jFN["InitialName"].asString();
		}
		if(jFN.isMember("Template"))
		{
			pConf->FilenameInfo.Template = jFN["Template"].asString();
		}
		if(jFN.isMember("Date"))
		{
			if(jFN["Date"].isMember("Token"))
			{
				pConf->FilenameInfo.DateToken = jFN["Date"]["Token"].asString();
				if(jFN["Date"].isMember("Format"))
					pConf->FilenameInfo.DateFormat = jFN["Date"]["Format"].asString();
			}
			else if (log)
				log->error("{}: Filename Date option needs 'Token', got {}", Name, jFN["Date"].toStyledString());
		}
		if(jFN.isMember("Event"))
		{
			if(jFN["Event"].isMember("Index"))
			{
				pConf->FilenameInfo.Event = std::make_shared<const EventInfo>(EventType::OctetString, jFN["Event"]["Index"].asUInt());
				if(jFN["Event"].isMember("Token"))
					pConf->FilenameInfo.EventToken = jFN["Event"]["Token"].asString();
			}
			else if (log)
				log->error("{}: Filename Event option needs 'Index', got {}", Name, jFN["Event"].toStyledString());
		}
	}
	if(JSONRoot.isMember("OverwriteMode"))
	{
		const auto modestr = JSONRoot["OverwriteMode"].asString();
		if(modestr == "OVERWRITE")
			pConf->Mode = OverwriteMode::OVERWRITE;
		else if(modestr == "APPEND")
			pConf->Mode = OverwriteMode::APPEND;
		else if(modestr == "FAIL")
			pConf->Mode = OverwriteMode::FAIL;
		else if (log)
			log->error("{}: Invalid OverwriteMode '{}'", Name, modestr);
	}
}

