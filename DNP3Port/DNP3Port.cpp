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
#include <openpal/logging/LogLevels.h>

std::unordered_map<std::string, asiodnp3::IChannel*> DNP3Port::Channels;

DNP3Port::DNP3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	pChannel(nullptr),
	status(opendnp3::LinkStatus::UNRESET),
	link_dead(true),
	channel_dead(true)
{
	//the creation of a new DNP3PortConf will get the point details
	pConf.reset(new DNP3PortConf(ConfFilename, ConfOverrides));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

// Called by OpenDNP3 Thread Pool
void DNP3Port::StateListener(ChannelState state)
{
	if(state != ChannelState::OPEN)
	{
		channel_dead = true;
		OnLinkDown();
	}
	else
	{
		channel_dead = false;
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

	if(JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		std::cout<<"Warning: DNP3 port serial device AND IP address specified - IP overrides"<<std::endl;

	if(JSONRoot.isMember("SerialDevice"))
	{
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = JSONRoot["SerialDevice"].asString();
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.IP = "";

		if(JSONRoot.isMember("BaudRate"))
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.baud = JSONRoot["BaudRate"].asUInt();
		if(JSONRoot.isMember("Parity"))
		{
			if(JSONRoot["Parity"].asString() == "EVEN")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::EVEN;
			else if(JSONRoot["Parity"].asString() == "ODD")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::ODD;
			else if(JSONRoot["Parity"].asString() == "NONE")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::NONE;
			else
				std::cout << "Warning: Invalid serial parity: " << JSONRoot["Parity"].asString() << ", should be EVEN, ODD, or NONE."<< std::endl;
		}
		if(JSONRoot.isMember("DataBits"))
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.dataBits = JSONRoot["DataBits"].asUInt();
		if(JSONRoot.isMember("StopBits"))
		{
			if(JSONRoot["StopBits"].asFloat() == 0)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::NONE;
			else if(JSONRoot["StopBits"].asFloat() == 1)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::ONE;
			else if(JSONRoot["StopBits"].asFloat() == 1.5)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::ONE_POINT_FIVE;
			else if(JSONRoot["StopBits"].asFloat() == 2)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::TWO;
			else
				std::cout << "Warning: Invalid serial stop bits: " << JSONRoot["StopBits"].asFloat() << ", should be 0, 1, 1.5, or 2"<< std::endl;
		}
	}

	if(JSONRoot.isMember("IP"))
	{
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = "";
	}

	if(JSONRoot.isMember("Port"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(JSONRoot.isMember("TCPClientServer"))
	{
		if(JSONRoot["TCPClientServer"].asString() == "CLIENT")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::CLIENT;
		else if(JSONRoot["TCPClientServer"].asString() == "SERVER")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::SERVER;
		else if(JSONRoot["TCPClientServer"].asString() == "DEFAULT")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::DEFAULT;
		else
			std::cout << "Warning: Invalid TCP client/server type: " << JSONRoot["TCPClientServer"].asString() << ", should be CLIENT, SERVER, or DEFAULT."<< std::endl;
	}

	if(JSONRoot.isMember("OutstationAddr"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if(JSONRoot.isMember("MasterAddr"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.MasterAddr = JSONRoot["MasterAddr"].asUInt();

	if(JSONRoot.isMember("ServerType"))
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

asiodnp3::IChannel* DNP3Port::GetChannel(asiodnp3::DNP3Manager& DNP3Mgr)
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	std::string ChannelID;
	bool isSerial;
	if(pConf->mAddrConf.IP.empty())
	{
		ChannelID = pConf->mAddrConf.SerialSettings.deviceName;
		isSerial = true;
	}
	else
	{
		ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
		isSerial = false;
	}

	//create a new channel if needed
	if(!Channels.count(ChannelID))
	{
		if(isSerial)
		{
			Channels[ChannelID] = DNP3Mgr.AddSerial(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
									    opendnp3::ChannelRetry(
										    openpal::TimeDuration::Milliseconds(500),
										    openpal::TimeDuration::Milliseconds(5000)),
									    pConf->mAddrConf.SerialSettings);
		}
		else
		{
			switch (ClientOrServer())
			{
				case TCPClientServer::SERVER:
					Channels[ChannelID] = DNP3Mgr.AddTCPServer(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
												 opendnp3::ChannelRetry(
													 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPListenRetryPeriodMinms),
													 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPListenRetryPeriodMaxms)),
												 pConf->mAddrConf.IP,
												 pConf->mAddrConf.Port);
					break;

				case TCPClientServer::CLIENT:
					Channels[ChannelID] = DNP3Mgr.AddTCPClient(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
												 opendnp3::ChannelRetry(
													 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMinms),
													 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMaxms)),
												 pConf->mAddrConf.IP,
												 "0.0.0.0",
												 pConf->mAddrConf.Port);
					break;

				default:
					std::string msg = Name + ": Can't determine if TCP socket is client or server";
					auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
					pLoggers->Log(log_entry);
					throw std::runtime_error(msg);
			}
		}
	}

	return Channels[ChannelID];
}

