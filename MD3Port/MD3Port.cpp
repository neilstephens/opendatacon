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
 * MD3Port.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */
#include "MD3Port.h"

// std::unordered_map<std::string, asiodnp3::IChannel*> MD3Port::TCPChannels;

MD3Port::MD3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	stack_enabled(false)
{
	//the creation of a new MD3PortConf will get the point details
	pConf.reset(new MD3PortConf(ConfFilename));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

void MD3Port::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	if(JSONRoot.isMember("SerialDevice"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialDevice = JSONRoot["SerialDevice"].asString();
	if(JSONRoot.isMember("BaudRate"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.BaudRate = JSONRoot["BaudRate"].asUInt();
	if(JSONRoot.isMember("Parity"))
	{
		if(JSONRoot["Parity"].asString() == "EVEN")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Parity = SerialParity::EVEN;
		else if(JSONRoot["Parity"].asString() == "ODD")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Parity = SerialParity::ODD;
		else if(JSONRoot["Parity"].asString() == "NONE")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Parity = SerialParity::NONE;
		else
			std::cout << "Warning: Invalid MD3 port parity: " << JSONRoot["Parity"].asString() << ", should be EVEN, ODD, or NONE."<< std::endl;
	}
	if(JSONRoot.isMember("DataBits"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.DataBits = JSONRoot["DataBits"].asUInt();
	if(JSONRoot.isMember("StopBits"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.StopBits = JSONRoot["StopBits"].asUInt();

	if(JSONRoot.isMember("IP"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();

	if(JSONRoot.isMember("Port"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(JSONRoot.isMember("OutstationAddr"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if(JSONRoot.isMember("ServerType"))
	{
		if (JSONRoot["ServerType"].asString()=="PERSISTENT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if (JSONRoot["ServerType"].asString()=="ONDEMAND")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else //if (JSONRoot["ServerType"].asString()=="MANUAL")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::MANUAL;
	}
}

