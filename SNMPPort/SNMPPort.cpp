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
 * SNMPPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */
#include "SNMPPort.h"

std::unordered_map<std::string, asiodnp3::IChannel*> SNMPPort::TCPChannels;

SNMPPort::SNMPPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	stack_enabled(false)
{
	//the creation of a new SNMPPortConf will get the point details
	pConf.reset(new SNMPPortConf(ConfFilename));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

void SNMPPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	if(!JSONRoot["IP"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();

	if(!JSONRoot["Port"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(!JSONRoot["ServerType"].isNull())
	{
		if (JSONRoot["ServerType"].asString()=="PERSISTENT")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if (JSONRoot["ServerType"].asString()=="ONDEMAND")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else //if (JSONRoot["ServerType"].asString()=="MANUAL")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::MANUAL;
	}
}

