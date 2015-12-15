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
 * DNP3ClientPort.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <asiodnp3/DefaultMasterApplication.h>
#include <opendnp3/app/ClassField.h>
#include <opendnp3/app/MeasurementTypes.h>
#include "DNP3MasterPort.h"
#include "CommandCallbackPromise.h"
#include <openpal/logging/LogLevels.h>
#include <array>
#include <asiopal/UTCTimeSource.h>

void DNP3MasterPort::Enable()
{
	if(enabled)
		return;
	if(nullptr == pMaster)
	{
		std::string msg = Name + ": Port not configured.";
		auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		return;
	}

	enabled = true;
	//initialise as comms down - in case they never come up
	PortDown();

	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(!stack_enabled && pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
		EnableStack();

}
void DNP3MasterPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	if(stack_enabled)
	{
		PortDown();

		stack_enabled = false;
		pMaster->Disable();
	}
}

void DNP3MasterPort::PortUp()
{
	auto eventTime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
	{
		for (auto IOHandler_pair : Subscribers)
		{
			{
				std::string msg = Name + ": Updating comms state point to good";
				auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::DBG, "", msg.c_str(), -1);
				pLoggers->Log(log_entry);
			}
			opendnp3::Binary commsUpEvent(!pConf->pPointConf->mCommsPoint.first.value, static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE), opendnp3::DNPTime(eventTime));
			IOHandler_pair.second->Event(commsUpEvent, pConf->pPointConf->mCommsPoint.second, this->Name);
		}
	}
}

void DNP3MasterPort::PortDown()
{
	auto eventTime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	for (auto IOHandler_pair : Subscribers)
	{
		{
			std::string msg = Name + ": Setting point quality to COMM_LOST";
			auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::DBG, "", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}

		for (auto index : pConf->pPointConf->BinaryIndicies)
			IOHandler_pair.second->Event(opendnp3::BinaryQuality::COMM_LOST, index, this->Name);
		for (auto index : pConf->pPointConf->AnalogIndicies)
			IOHandler_pair.second->Event(opendnp3::AnalogQuality::COMM_LOST, index, this->Name);

		// Update the comms state point if configured
		if (pConf->pPointConf->mCommsPoint.first.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
		{
			{
				std::string msg = Name + ": Updating comms state point to bad";
				auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::DBG, "", msg.c_str(), -1);
				pLoggers->Log(log_entry);
			}
			opendnp3::Binary commsDownEvent(pConf->pPointConf->mCommsPoint.first.value, static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE), opendnp3::DNPTime(eventTime));
			IOHandler_pair.second->Event(commsDownEvent, pConf->pPointConf->mCommsPoint.second, this->Name);
		}
	}
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up)
void DNP3MasterPort::OnStateChange(opendnp3::LinkStatus status)
{
	this->status = status;
	if(link_dead)
	{

		link_dead = false;
		// Update the comms state point and qualities
		PortUp();
	}
	//TODO: track a statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3MasterPort::OnKeepAliveFailure()
{
	if(!link_dead)
	{
		link_dead = true;
		PortDown();

		// Notify subscribers that a disconnect event has occured
		for (auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::DISCONNECTED, 0, this->Name);
		}

		DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if (stack_enabled && pConf->mAddrConf.ServerType != server_type_t::PERSISTENT && !InDemand())
		{
			std::string msg = Name + ": disabling stack following disconnect on non-persistent port.";
			auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::INFO, "", msg.c_str(), -1);
			pLoggers->Log(log_entry);

			// For all but persistent connections, and in-demand ONDEMAND connections, disable the stack
			pIOS->post([&]()
			           {
			                 DisableStack();
				     });
		}
	}
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3MasterPort::OnKeepAliveSuccess()
{
	if(link_dead)
	{
		link_dead = false;
		// Update the comms state point and qualities
		PortUp();
	}
}

void DNP3MasterPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	auto IPPort = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	auto log_id = "mast_"+IPPort;

	//create a new channel if one isn't already up
	if(!TCPChannels.count(IPPort))
	{
		TCPChannels[IPPort] = DNP3Mgr.AddTCPClient(log_id.c_str(), LOG_LEVEL.GetBitfield(),
		                                           opendnp3::ChannelRetry::Default(),
		                                           pConf->mAddrConf.IP,
		                                           "0.0.0.0",
		                                           pConf->mAddrConf.Port);
	}
	pChannel = TCPChannels[IPPort];
	if (pChannel == nullptr)
	{
		std::string msg = Name + ": TCP channel not found for masterstation.";
		auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		return;
	}

	opendnp3::MasterStackConfig StackConfig;

	// Link layer configuration
	StackConfig.link.LocalAddr = pConf->mAddrConf.MasterAddr;
	StackConfig.link.RemoteAddr = pConf->mAddrConf.OutstationAddr;
	StackConfig.link.NumRetry = pConf->pPointConf->LinkNumRetry;
	StackConfig.link.Timeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->LinkTimeoutms);
	if(pConf->pPointConf->LinkKeepAlivems == 0)
		StackConfig.link.KeepAliveTimeout = openpal::TimeDuration::Max();
	else
		StackConfig.link.KeepAliveTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->LinkKeepAlivems);
	StackConfig.link.UseConfirms = pConf->pPointConf->LinkUseConfirms;

	// Master station configuration
	StackConfig.master.responseTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->MasterResponseTimeoutms);
	StackConfig.master.timeSyncMode = pConf->pPointConf->MasterRespondTimeSync ? TimeSyncMode::SerialTimeSync : TimeSyncMode::None;
	StackConfig.master.disableUnsolOnStartup = !pConf->pPointConf->DoUnsolOnStartup;
	StackConfig.master.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.master.startupIntegrityClassMask = pConf->pPointConf->GetStartupIntegrityClassMask(); //TODO: report/investigate bug - doesn't recognise response to integrity scan if not ALL_CLASSES
	StackConfig.master.integrityOnEventOverflowIIN = pConf->pPointConf->IntegrityOnEventOverflowIIN;
	StackConfig.master.taskRetryPeriod = openpal::TimeDuration::Milliseconds(pConf->pPointConf->TaskRetryPeriodms);

	pMaster = pChannel->AddMaster(Name.c_str(), *this, asiodnp3::DefaultMasterApplication::Instance(), StackConfig);
	if (pMaster == nullptr)
	{
		std::string msg = Name + ": Error creating masterstation.";
		auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		return;
	}

	// Master Station scanning configuration
	if(pConf->pPointConf->IntegrityScanRatems > 0)
		IntegrityScan = pMaster->AddClassScan(opendnp3::ClassField::ALL_CLASSES, openpal::TimeDuration::Milliseconds(pConf->pPointConf->IntegrityScanRatems));
	else
		IntegrityScan = pMaster->AddClassScan(opendnp3::ClassField::ALL_CLASSES, openpal::TimeDuration::Minutes(600000000)); //ten million hours
	if(pConf->pPointConf->EventClass1ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField::CLASS_1, openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass1ScanRatems));
	if(pConf->pPointConf->EventClass2ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField::CLASS_2, openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass2ScanRatems));
	if(pConf->pPointConf->EventClass3ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField::CLASS_3, openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass3ScanRatems));
}

// Called by OpenDNP3 Thread Pool
//implement ISOEHandler
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<Binary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<DoubleBitBinary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<Analog> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<Counter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<FrozenCounter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<BinaryOutputStatus> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<AnalogOutputStatus> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<OctetString> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<TimeAndInterval> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<BinaryCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<AnalogCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const HeaderInfo& info, const ICollection<Indexed<SecurityStat> >& meas){ /*LoadT(meas);*/ }

template<typename T>
inline void DNP3MasterPort::LoadT(const ICollection<Indexed<T> >& meas)
{
	meas.ForeachItem([&](const Indexed<T>&pair)
	                 {
	                       for(auto IOHandler_pair: Subscribers)
	                       {
	                             IOHandler_pair.second->Event(pair.value,pair.index,this->Name);
				     }
			     });
}

//Implement some IOHandler - parent DNP3Port implements the rest to return NOT_SUPPORTED
std::future<opendnp3::CommandStatus> DNP3MasterPort::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3MasterPort::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3MasterPort::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3MasterPort::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3MasterPort::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }

std::future<opendnp3::CommandStatus> DNP3MasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	// If an upstream port has been enabled after the stack has already been enabled, do an integrity scan
	if (stack_enabled && state == ConnectState::PORT_UP)
	{
		std::string msg = Name + ": upstream port enabled, performing integrity scan.";
		auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::INFO, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		IntegrityScan.Demand();
	}

	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	// If an upstream port is connected, attempt a connection (if on demand)
	if (!stack_enabled && state == ConnectState::CONNECTED && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
	{
		std::string msg = Name + ": upstream port connected, performing on-demand connection.";
		auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::INFO, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		pIOS->post([&]()
		           {
		                 EnableStack();
			     });
	}

	// If an upstream port is disconnected, disconnect ourselves if it was the last active connection (if on demand)
	if (stack_enabled && state == ConnectState::DISCONNECTED && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
	{
		pIOS->post([&]()
		           {
		                 DisableStack();
		                 PortDown();
			     });
	}

	return IOHandler::CommandFutureSuccess();
}

template<typename T>
inline std::future<opendnp3::CommandStatus> DNP3MasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName)
{
	// If the port is disabled, fail the command
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	// If the stack is disabled, fail the command
	if (!stack_enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	for(auto i : pConf->pPointConf->ControlIndicies)
	{
		if(i == index)
		{
			auto cmd_promise = std::promise<opendnp3::CommandStatus>();
			auto cmd_future = cmd_promise.get_future();

			//make a copy of the command, so we can change it if needed
			auto lCommand = arCommand;
			//this will change the control code if the command is binary, and there's a defined override
			DoOverrideControlCode(lCommand);

			std::string msg = "Executing direct operate to index: " + std::to_string(index);
			auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::INFO, "", msg.c_str(), -1);
			pLoggers->Log(log_entry);

			this->pMaster->DirectOperate(lCommand,index,*CommandCorrespondant::GetCallback(std::move(cmd_promise)));

			return cmd_future;
		}
	}
	std::string msg = "Control sent to invalid DNP3 index: " + std::to_string(index);
	auto log_entry = openpal::LogEntry("DNP3MasterPort", openpal::logflags::WARN, "", msg.c_str(), -1);
	pLoggers->Log(log_entry);
	return IOHandler::CommandFutureUndefined();
}

//DataPort function for UI
const Json::Value DNP3MasterPort::GetStatistics() const
{
	Json::Value event;

	if (pChannel != nullptr)
	{
		auto ChanStats = this->pChannel->GetChannelStatistics();
		event["numCrcError"] = ChanStats.numCrcError;             /// Number of frames discared due to CRC errors
		event["numLinkFrameTx"] = ChanStats.numLinkFrameTx;       /// Number of frames transmitted
		event["numLinkFrameRx"] = ChanStats.numLinkFrameRx;       /// Number of frames received
		event["numBadLinkFrameRx"] = ChanStats.numBadLinkFrameRx; /// Number of frames detected with bad / malformed contents
		event["numBytesRx"] = ChanStats.numBytesRx;
		event["numBytesTx"] = ChanStats.numBytesTx;
		event["numClose"] = ChanStats.numClose;
		event["numOpen"] = ChanStats.numOpen;
		event["numOpenFail"] = ChanStats.numOpenFail;
	}
	if (pMaster != nullptr)
	{
		auto StackStats = this->pMaster->GetStackStatistics();
		event["numTransportErrorRx"] = StackStats.numTransportErrorRx;
		event["numTransportRx"] = StackStats.numTransportRx;
		event["numTransportTx"] = StackStats.numTransportTx;
	}

	return event;
}
