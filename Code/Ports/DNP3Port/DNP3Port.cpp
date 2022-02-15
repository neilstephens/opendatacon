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
#include <opendnp3/logging/LogLevels.h>

DNP3Port::DNP3Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	pChanH(std::make_unique<ChannelHandler>(this))
{
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<opendnp3::DNP3Manager> weak_mgr;

	//if we're the first/only one on the scene,
	// init the DNP3Manager
	if(!init_flag.test_and_set(std::memory_order_acquire))
	{
		//make a custom deleter for the DNP3Manager that will also clear the init flag
		auto deinit_del = [](opendnp3::DNP3Manager* mgr_ptr)
					{init_flag.clear(std::memory_order_release); delete mgr_ptr;};
		this->IOMgr = std::shared_ptr<opendnp3::DNP3Manager>(
			new opendnp3::DNP3Manager(pIOS->GetConcurrency(),std::make_shared<DNP3Log2spdlog>()),
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

void DNP3Port::InitEventDB()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	std::vector<std::shared_ptr<const EventInfo>> init_events;
	init_events.reserve(pConf->pPointConf->AnalogIndexes.size()+
		pConf->pPointConf->BinaryIndexes.size()+
		pConf->pPointConf->ControlIndexes.size());

	for(auto index : pConf->pPointConf->AnalogIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Analog,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->BinaryIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Binary,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->ControlIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::ControlRelayOutputBlock,index,"",QualityFlags::RESTART,0));
	for (auto index : pConf->pPointConf->AnalogControlIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::AnalogOutputInt32, index, "", QualityFlags::RESTART, 0));
	
	pDB = std::make_unique<EventDB>(init_events);
}

//DataPort function for UI
const Json::Value DNP3Port::GetCurrentState() const
{
	auto time_str = since_epoch_to_datetime(msSinceEpoch());
	Json::Value ret;
	ret[time_str]["Analogs"] = Json::arrayValue;
	ret[time_str]["Binaries"] = Json::arrayValue;
	ret[time_str]["BinaryControls"] = Json::arrayValue;

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	auto time_correction = [=](const auto& event)
	{
		auto ts = event->GetTimestamp();
		if ((event->GetEventType() == EventType::ControlRelayOutputBlock) || (event->GetEventType() == EventType::AnalogOutputInt32))
						     return since_epoch_to_datetime(ts);
					     if ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS)
					         || ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (ts == 0)))
						     ts = msSinceEpoch();

					     return since_epoch_to_datetime(ts);
				     };

	for(const auto index : pConf->pPointConf->BinaryIndexes)
	{
		auto event = pDB->Get(EventType::Binary,index);
		auto& state = ret[time_str]["Binaries"].append(Json::Value());
		state["Index"] =  Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayload<EventType::Binary>();
		}
		catch(std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	for(const auto index : pConf->pPointConf->AnalogIndexes)
	{
		auto event = pDB->Get(EventType::Analog,index);
		auto& state = ret[time_str]["Analogs"].append(Json::Value());
		state["Index"] =  Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayload<EventType::Analog>();
		}
		catch(std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	for(const auto index : pConf->pPointConf->ControlIndexes)
	{
		auto event = pDB->Get(EventType::ControlRelayOutputBlock,index);
		auto& state = ret[time_str]["BinaryControls"].append(Json::Value());
		state["Index"] =  Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayloadString();
		}
		catch(std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	for (const auto index : pConf->pPointConf->AnalogControlIndexes)
	{
		auto event = pDB->Get(EventType::AnalogOutputInt32, index);
		auto& state = ret[time_str]["AnalogControls"].append(Json::Value());
		state["Index"] = Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayloadString();
		}
		catch (std::runtime_error&)
		{
		}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}

	ExtendCurrentState(ret[time_str]);
	return ret;
}

//DataPort function for UI
const Json::Value DNP3Port::GetStatus() const
{
	auto ret_val = Json::Value();

	if(!enabled)
		ret_val["Result"] = "Port disabled";
	else if(pChanH->GetLinkDeadness() != LinkDeadness::LinkUpChannelUp)
		ret_val["Result"] = "Port enabled - link down";
	else if(pChanH->GetLinkStatus() == opendnp3::LinkStatus::RESET)
		ret_val["Result"] = "Port enabled - link up (reset)";
	else if(pChanH->GetLinkStatus() == opendnp3::LinkStatus::UNRESET)
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

		if(JSONRoot.isMember("IPTransport"))
		{
			if(JSONRoot["IPTransport"].asString() == "UDP")
			{
				static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.Transport = IPTransport::UDP;
				if(JSONRoot.isMember("UDPListenPort"))
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.UDPListenPort = JSONRoot["UDPListenPort"].asUInt();
			}
			else if(JSONRoot["IPTransport"].asString() == "TLS")
			{
				if(JSONRoot.isMember("TLSFiles")
				   && JSONRoot["TLSFiles"].isMember("PeerCert")
				   && JSONRoot["TLSFiles"].isMember("LocalCert")
				   && JSONRoot["TLSFiles"].isMember("PrivateKey"))
				{
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.Transport = IPTransport::TLS;
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSFiles.PeerCertFile =
						JSONRoot["TLSFiles"]["PeerCert"].asString();
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSFiles.LocalCertFile =
						JSONRoot["TLSFiles"]["LocalCert"].asString();
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSFiles.PrivateKeyFile =
						JSONRoot["TLSFiles"]["PrivateKey"].asString();
				}
				else
				{
					if(auto log = odc::spdlog_get("DNP3Port"))
					{
						Json::Value Eg;
						Eg["TLSFiles"]["PeerCert"] = "/path/to/file";
						Eg["TLSFiles"]["LocalCert"] = "/path/to/file";
						Eg["TLSFiles"]["PrivateKey"] = "/path/to/file";
						log->warn("Reverting to TCP: TLS IPTransport requires TLSFiles config, Eg '{}'", Eg.asString());
					}
				}
			}
			else if(JSONRoot["IPTransport"].asString() != "TCP")
			{
				if(auto log = odc::spdlog_get("DNP3Port"))
					log->warn("Invalid IP transport protocol: {}, should be TCP, UDP, or TLS. Using TCP", JSONRoot["IPTransport"].asString());
			}
			//else TCP, already default
		}
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

