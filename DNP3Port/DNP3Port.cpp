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
 * DNP3Port.cpp
 *
 *  Created on: 18/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "DNP3Port.h"
#include <iostream>
#include "DNP3PortConf.h"

std::unordered_map<std::string, asiodnp3::IChannel*> DNP3Port::TCPChannels;

DNP3Port::DNP3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	pChannel(nullptr),
	status(opendnp3::LinkStatus::UNRESET),
	link_dead(true)
{
	//the creation of a new DNP3PortConf will get the point details
	pConf.reset(new DNP3PortConf(ConfFilename));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

// Called by OpenDNP3 Thread Pool
void DNP3Port::StateListener(opendnp3::ChannelState state)
{
	if(state != opendnp3::ChannelState::OPEN)
	{
		OnLinkDown();
	}
}

//DataPort function for UI
const Json::Value DNP3Port::GetStatus() const
{
	auto ret_val = Json::Value();

	if(!enabled)
		ret_val["Result"] = "Port disabled";
	else if(link_dead)
		ret_val["Result"] = "Port enabled - link down";
	else if(status == opendnp3::LinkStatus::RESET)
		ret_val["Result"] = "Port enabled - link up (reset)";
	else if(status == opendnp3::LinkStatus::UNRESET)
		ret_val["Result"] = "Port enabled - link up (unreset)";
	else
		ret_val["Result"] = "Port enabled - link status unknown";

	return ret_val;
}

void DNP3Port::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;
	if(!JSONRoot["IP"].isNull())
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();

	if(!JSONRoot["Port"].isNull())
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(!JSONRoot["OutstationAddr"].isNull())
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if(!JSONRoot["MasterAddr"].isNull())
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.MasterAddr = JSONRoot["MasterAddr"].asUInt();

	if(!JSONRoot["ServerType"].isNull())
	{
		if(JSONRoot["ServerType"].asString() == "ONDEMAND")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else if(JSONRoot["ServerType"].asString() == "PERSISTENT")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if(JSONRoot["ServerType"].asString() == "MANUAL")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::MANUAL;
		else
			std::cout<<"Invalid DNP3 Port server type: '"<<JSONRoot["ServerType"].asString()<<"'."<<std::endl;
	}

}

