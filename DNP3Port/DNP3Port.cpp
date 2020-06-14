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

#include "ChannelStateSubscriber.h"
#include "DNP3Port.h"
#include "DNP3PortConf.h"
#include <opendatacon/util.h>
#include <opendnp3/gen/Parity.h>
#include <openpal/logging/LogLevels.h>

DNP3Port::DNP3Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	pChannel(nullptr),
	status(opendnp3::LinkStatus::UNRESET),
	link_dead(true),
	channel_dead(true)
{
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<asiodnp3::DNP3Manager> weak_mgr;

	//if we're the first/only one on the scene,
	// init the DNP3Manager
	if(!init_flag.test_and_set(std::memory_order_acquire))
	{
		//make a custom deleter for the DNP3Manager that will also clear the init flag
		auto deinit_del = [](asiodnp3::DNP3Manager* mgr_ptr)
					{init_flag.clear(); delete mgr_ptr;};
		this->IOMgr = std::shared_ptr<asiodnp3::DNP3Manager>(
			new asiodnp3::DNP3Manager(std::thread::hardware_concurrency(),std::make_shared<DNP3Log2spdlog>()),
			deinit_del);
		weak_mgr = this->IOMgr;
	}
	//otherwise just make sure it's finished initialising and take a shared_ptr
	else
	{
		while (!(this->IOMgr = weak_mgr.lock()))
		{} //init happens very seldom, so spin lock is good
	}

	//the creation of a new DNP3PortConf will get the point details
	pConf = std::make_unique<DNP3PortConf>(ConfFilename, ConfOverrides);

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

DNP3Port::~DNP3Port()
{}

// Called by OpenDNP3 Thread Pool
void DNP3Port::StateListener(opendnp3::ChannelState state)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: ChannelState {}.", Name, opendnp3::ChannelStateToString(state));

	if(state != opendnp3::ChannelState::OPEN)
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

	if(JSONRoot.isMember("LOG_LEVEL"))
	{
		auto& LOG_LEVEL = static_cast<DNP3PortConf*>(pConf.get())->LOG_LEVEL;
		std::string value = JSONRoot["LOG_LEVEL"].asString();
		if(value == "ALL")
			LOG_LEVEL = opendnp3::levels::ALL;
		else if(value == "ALL_COMMS")
			LOG_LEVEL = opendnp3::levels::ALL_COMMS;
		else if(value == "NORMAL")
			LOG_LEVEL = opendnp3::levels::NORMAL;
		else if(value == "NOTHING")
			LOG_LEVEL = opendnp3::levels::NOTHING;
		else
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->warn("Invalid LOG_LEVEL setting: '{}' - defaulting to 'NORMAL' log level.", value);
		}
	}

	if(JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->warn("Serial device AND IP address specified - IP wins");

	if(JSONRoot.isMember("SerialDevice"))
	{
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = JSONRoot["SerialDevice"].asString();
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.IP = "";

		if(JSONRoot.isMember("BaudRate"))
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.baud = JSONRoot["BaudRate"].asUInt();
		if(JSONRoot.isMember("Parity"))
		{
			if(JSONRoot["Parity"].asString() == "EVEN")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = opendnp3::Parity::Even;
			else if(JSONRoot["Parity"].asString() == "ODD")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = opendnp3::Parity::Odd;
			else if(JSONRoot["Parity"].asString() == "NONE")
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = opendnp3::Parity::None;
			else
			{
				if(auto log = odc::spdlog_get("DNP3Port"))
					log->warn("Invalid serial parity: {}, should be EVEN, ODD, or NONE.", JSONRoot["Parity"].asString());
			}
		}
		if(JSONRoot.isMember("DataBits"))
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.dataBits = JSONRoot["DataBits"].asUInt();
		if(JSONRoot.isMember("StopBits"))
		{
			if(JSONRoot["StopBits"].asFloat() == 0)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = opendnp3::StopBits::None;
			else if(JSONRoot["StopBits"].asFloat() == 1)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = opendnp3::StopBits::One;
			else if(JSONRoot["StopBits"].asFloat() == 1.5)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = opendnp3::StopBits::OnePointFive;
			else if(JSONRoot["StopBits"].asFloat() == 2)
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = opendnp3::StopBits::Two;
			else
			{
				if(auto log = odc::spdlog_get("DNP3Port"))
					log->warn("Invalid serial stop bits: {}, should be 0, 1, 1.5, or 2", JSONRoot["StopBits"].asFloat());
			}
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
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->warn("Invalid TCP client/server type: {}, should be CLIENT, SERVER, or DEFAULT.", JSONRoot["TCPClientServer"].asString());
		}
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
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->warn("Invalid DNP3 Port server type: '{}'.", JSONRoot["ServerType"].asString());
		}
	}

}

class ChannelListener: public asiodnp3::IChannelListener
{
public:
	ChannelListener(const std::string& aChanID, DNP3Port* pPort):
		ChanID(aChanID)
	{
		ChannelStateSubscriber::Subscribe(pPort,ChanID);
	}
	//Receive callbacks for state transitions on the channels from opendnp3
	void OnStateChange(opendnp3::ChannelState state) override
	{
		ChannelStateSubscriber::StateListener(ChanID,state);
	}
private:
	const std::string ChanID;
};

std::shared_ptr<asiodnp3::IChannel> DNP3Port::GetChannel()
{
	static std::unordered_map<std::string, std::weak_ptr<asiodnp3::IChannel>> Channels;
	std::shared_ptr<asiodnp3::IChannel> chan(nullptr);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

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
	if(!Channels.count(ChannelID) || !(chan = Channels[ChannelID].lock()))
	{
		auto listener = std::make_shared<ChannelListener>(ChannelID,this);
		if(isSerial)
		{
			chan = IOMgr->AddSerial(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
				asiopal::ChannelRetry(
					openpal::TimeDuration::Milliseconds(500),
					openpal::TimeDuration::Milliseconds(5000)),
				pConf->mAddrConf.SerialSettings,listener);
		}
		else
		{
			switch (ClientOrServer())
			{
				case TCPClientServer::SERVER:
				{
					chan = IOMgr->AddTCPServer(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
						pConf->pPointConf->ServerAcceptMode,
						pConf->mAddrConf.IP,
						pConf->mAddrConf.Port,listener);
					break;
				}

				case TCPClientServer::CLIENT:
				{
					chan = IOMgr->AddTCPClient(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
						asiopal::ChannelRetry(
							openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMinms),
							openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMaxms)),
						pConf->mAddrConf.IP,
						"0.0.0.0",
						pConf->mAddrConf.Port,listener);
					break;
				}

				default:
				{
					const std::string msg(Name + ": Can't determine if TCP socket is client or server");
					if(auto log = odc::spdlog_get("DNP3Port"))
						log->error(msg);
					throw std::runtime_error(msg);
				}
			}
		}
		Channels[ChannelID] = chan;
	}

	return chan;
}

