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
#include "DNP3PortConf.h"
#include "Log.h"
#include <opendatacon/util.h>
#include <opendnp3/gen/Parity.h>
#include <opendnp3/logging/LogLevels.h>

DNP3Port::DNP3Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool isMaster):
	DataPort(aName, aConfFilename, aConfOverrides),
	pChanH(std::make_unique<ChannelHandler>(this)),
	pConnectionStabilityTimer(pIOS->make_steady_timer()),
	stack_enabled(false),
	pStackSyncStrand(pIOS->make_strand()),
	connection_notification_pending(false)
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
	pConf = std::make_unique<DNP3PortConf>(ConfFilename, ConfOverrides, isMaster);

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
		pConf->pPointConf->OctetStringIndexes.size()+
		pConf->pPointConf->ControlIndexes.size()+
		pConf->pPointConf->AnalogControlIndexes.size()+
		pConf->pPointConf->AnalogOutputStatusIndexes.size()+
		pConf->pPointConf->BinaryOutputStatusIndexes.size());

	for(auto index : pConf->pPointConf->AnalogIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Analog,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->BinaryIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Binary,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->OctetStringIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::OctetString,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->ControlIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::ControlRelayOutputBlock,index,"",QualityFlags::RESTART,0));
	for (auto index : pConf->pPointConf->AnalogControlIndexes)
	{
		// Need to work out which type of event we should be queuing - using the information from the configuration
		auto evttype = pConf->pPointConf->AnalogControlTypes[index];
		init_events.emplace_back(std::make_shared<const EventInfo>(evttype, index, "", QualityFlags::RESTART, 0));
	}
	for(auto index : pConf->pPointConf->AnalogOutputStatusIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::AnalogOutputStatus,index,"",QualityFlags::RESTART,0));
	for(auto index : pConf->pPointConf->BinaryOutputStatusIndexes)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::BinaryOutputStatus,index,"",QualityFlags::RESTART,0));
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Binary,pConf->pPointConf->mCommsPoint.second,"",QualityFlags::RESTART,0));

	init_events.emplace_back(std::make_shared<const EventInfo>(EventType::ConnectState,0,"",QualityFlags::RESTART,0));

	pDB = std::make_unique<EventDB>(init_events);
}

// These NotifyOf(Dis)Connection functions are already called on the channel handler strand
//	but since we fire off a timer, we have to sync the callback
void DNP3Port::NotifyOfConnection()
{
	connection_notification_pending = true;
	auto notify = [this]()
			  {
				  if(connection_notification_pending)
				  {
					  PublishEvent(ConnectState::CONNECTED);
					  connection_notification_pending = false;
				  }
			  };

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	pConnectionStabilityTimer->expires_from_now(std::chrono::milliseconds(pConf->mAddrConf.ConnectionStabilityTimems));
	pConnectionStabilityTimer->async_wait([this,notify](asio::error_code err_code)
		{
			if(!err_code) pChanH->Post(notify);
		});
}
void DNP3Port::NotifyOfDisconnection()
{
	if(connection_notification_pending)
	{
		connection_notification_pending = false;
		pConnectionStabilityTimer->cancel();
		return;
	}
	PublishEvent(ConnectState::DISCONNECTED);
}

void DNP3Port::ChannelWatchdogTrigger(bool on)
{
	Log.Debug("{}: ChannelWatchdogTrigger({}) called.", Name, on);
	if(stack_enabled)
	{
		if(on)                //don't mark the stack as disabled, because this is just a restart
			DisableStack(); //it will be enabled again shortly when the trigger is off
		else
			EnableStack();
	}
}

void DNP3Port::CheckStackState()
{
	auto weak_self = weak_from_this();
	pStackSyncStrand->post([weak_self,this]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

			if(!enabled || (pConf->OnDemand && !InDemand()))
			{
				if(stack_enabled)
				{
					stack_enabled = false;
					DisableStack(); //this will trigger comms down
					Log.Debug("{}: DNP3 stack disabled", Name);
				}
				return;
			}

			if(enabled && (!pConf->OnDemand || InDemand()))
			{
				if(!stack_enabled)
				{
					EnableStack();
					stack_enabled = true;
					Log.Debug("{}: DNP3 stack enabled", Name);
				}
				return;
			}

		});
}

//DataPort function for UI
const Json::Value DNP3Port::GetCurrentState() const
{
	auto time_str = since_epoch_to_datetime(msSinceEpoch());
	Json::Value ret;
	ret[time_str]["Analogs"] = Json::arrayValue;
	ret[time_str]["Binaries"] = Json::arrayValue;
	ret[time_str]["OctetStrings"] = Json::arrayValue;
	ret[time_str]["BinaryControls"] = Json::arrayValue;
	ret[time_str]["AnalogControls"] = Json::arrayValue;
	ret[time_str]["AnalogOutputStatuses"] = Json::arrayValue;
	ret[time_str]["BinaryOutputStatuses"] = Json::arrayValue;

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	//TODO: this wouldn't be needed if the overriden timestamp was stored in the point database
	auto time_correction = [=](const auto& event)
				     {
					     auto ts = event->GetTimestamp();
					     std::unordered_set<EventType> s = { EventType::ControlRelayOutputBlock, EventType::AnalogOutputInt16, EventType::AnalogOutputInt32, EventType::AnalogOutputFloat32, EventType::AnalogOutputDouble64 };
					     if (s.count(event->GetEventType()))
						     return since_epoch_to_datetime(ts);
					     if ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS)
					         || ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (ts == 0)))
						     ts = msSinceEpoch();

					     return since_epoch_to_datetime(ts);
				     };

	//TODO: change the structure to match the spoof_event command syntax - there are already JSON (de)serialisation helper functions
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
	for(const auto index : pConf->pPointConf->OctetStringIndexes)
	{
		auto event = pDB->Get(EventType::OctetString,index);
		auto& state = ret[time_str]["OctetStrings"].append(Json::Value());
		state["Index"] =  Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = ToString(event->GetPayload<EventType::OctetString>(),DataToStringMethod::Base64);
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
		// Get the dnp3 type for the point, then get the ODC event type, then create an event of that type
		auto evttype = pConf->pPointConf->AnalogControlTypes[index];
		auto event = pDB->Get(evttype, index);
		auto& state = ret[time_str]["AnalogControls"].append(Json::Value());
		state["Index"] = Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayloadString();
		}
		catch (std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	for (const auto index : pConf->pPointConf->AnalogOutputStatusIndexes)
	{
		auto event = pDB->Get(EventType::AnalogOutputStatus, index);
		auto& state = ret[time_str]["AnalogOutputStatuses"].append(Json::Value());
		state["Index"] = Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayload<EventType::AnalogOutputStatus>();
		}
		catch (std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	for (const auto index : pConf->pPointConf->BinaryOutputStatusIndexes)
	{
		auto event = pDB->Get(EventType::BinaryOutputStatus, index);
		auto& state = ret[time_str]["BinaryOutputStatuses"].append(Json::Value());
		state["Index"] = Json::UInt(event->GetIndex());
		try
		{
			state["Value"] = event->GetPayload<EventType::BinaryOutputStatus>();
		}
		catch (std::runtime_error&)
		{}
		state["Quality"] = ToString(event->GetQuality());
		state["Timestamp"] = time_correction(event);
		state["SourcePort"] = event->GetSourcePort();
	}
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
	{
		auto event = pDB->Get(EventType::Binary,pConf->pPointConf->mCommsPoint.second);
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

	auto event = pDB->Get(EventType::ConnectState,0);
	ret[time_str]["LastUpstreamConnection"] = event->HasPayload() ? event->GetPayloadString() : "";
	ret[time_str]["InDemand"] = InDemand();

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
			Log.Warn("Invalid LOG_LEVEL setting: '{}' - defaulting to 'NORMAL' log level.", value);
		}
	}

	if(JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		Log.Warn("Serial device AND IP address specified - IP wins");

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
				Log.Warn("Invalid serial parity: {}, should be EVEN, ODD, or NONE.", JSONRoot["Parity"].asString());
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
				Log.Warn("Invalid serial stop bits: {}, should be 0, 1, 1.5, or 2", JSONRoot["StopBits"].asFloat());
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
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.PeerCertFile =
						JSONRoot["TLSFiles"]["PeerCert"].asString();
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.LocalCertFile =
						JSONRoot["TLSFiles"]["LocalCert"].asString();
					static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.PrivateKeyFile =
						JSONRoot["TLSFiles"]["PrivateKey"].asString();
				}
				else
				{
					Json::Value Eg;
					Eg["TLSFiles"]["PeerCert"] = "/path/to/file";
					Eg["TLSFiles"]["LocalCert"] = "/path/to/file";
					Eg["TLSFiles"]["PrivateKey"] = "/path/to/file";
					Log.Warn("Reverting to TCP: TLS IPTransport requires TLSFiles config, Eg '{}'", Eg.asString());
				}
				if(JSONRoot.isMember("TLSAllowedVersions"))
				{
					if(JSONRoot["TLSAllowedVersions"].isArray())
					{
						static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv10 = false;
						static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv11 = false;
						static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv12 = false;
						static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv13 = false;
						for(Json::ArrayIndex n = 0; n < JSONRoot["TLSAllowedVersions"].size(); ++n)
						{
							bool invalidVer = false;
							if(JSONRoot["TLSAllowedVersions"][n].isUInt())
							{
								auto Ver = JSONRoot["TLSAllowedVersions"][n].asUInt();
								if(Ver == 10)
									static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv10 = true;
								else if (Ver == 11)
									static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv11 = true;
								else if (Ver == 12)
									static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv12 = true;
								else if (Ver == 13)
									static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.allowTLSv13 = true;
								else
									invalidVer = true;
							}
							else
								invalidVer = true;

							if(invalidVer)
							{
								Log.Error("Invalid entry in TLSAllowedVersions: Valid versions are 10,11,12,13.");
							}
						}
					}
					else
					{
						Log.Error("TLSAllowedVersions should be a JSON array.");
					}
				}
				if(JSONRoot.isMember("TLSCiphers"))
				{
					if(JSONRoot["TLSCiphers"].isString())
						static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.TLSConf.cipherList = JSONRoot["TLSCiphers"].asString();
					else Log.Error("TLSCiphers should be a string.");
				}
			}
			else if(JSONRoot["IPTransport"].asString() != "TCP")
			{
				Log.Warn("Invalid IP transport protocol: {}, should be TCP, UDP, or TLS. Using TCP", JSONRoot["IPTransport"].asString());
			}
			//else TCP, already default
		}
	}

	if(JSONRoot.isMember("Port"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(JSONRoot.isMember("BindIP"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.BindIP = JSONRoot["BindIP"].asString();

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
			Log.Warn("Invalid TCP client/server type: {}, should be CLIENT, SERVER, or DEFAULT.", JSONRoot["TCPClientServer"].asString());
		}
	}

	if(JSONRoot.isMember("OutstationAddr"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if(JSONRoot.isMember("MasterAddr"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.MasterAddr = JSONRoot["MasterAddr"].asUInt();

	if(JSONRoot.isMember("ServerType"))
	{
		if(JSONRoot["ServerType"].asString() == "ONDEMAND")
			static_cast<DNP3PortConf*>(pConf.get())->OnDemand = true;
		else if(JSONRoot["ServerType"].asString() == "PERSISTENT")
			static_cast<DNP3PortConf*>(pConf.get())->OnDemand = false;
		else
		{
			Log.Warn("Invalid DNP3 Port server type: '{}'.", JSONRoot["ServerType"].asString());
		}
	}
	if(JSONRoot.isMember("ChannelLinksWatchdogBark"))
	{
		const auto bark = JSONRoot["ChannelLinksWatchdogBark"].asString();
		if(bark == "ONFIRST")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ChannelLinksWatchdogBark = WatchdogBark::ONFIRST;
		else if(bark == "ONFINAL")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ChannelLinksWatchdogBark = WatchdogBark::ONFINAL;
		else if(bark == "NEVER")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ChannelLinksWatchdogBark = WatchdogBark::NEVER;
		else if(bark == "DEFAULT")
			static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ChannelLinksWatchdogBark = WatchdogBark::DEFAULT;
		else
		{
			Log.Warn("Invalid DNP3 Port ChannelLinksWatchdogBark: '{}'.", bark);
		}
	}
	if(JSONRoot.isMember("ConnectionStabilityTimems"))
		static_cast<DNP3PortConf*>(pConf.get())->mAddrConf.ConnectionStabilityTimems = JSONRoot["ConnectionStabilityTimems"].asUInt();
}

