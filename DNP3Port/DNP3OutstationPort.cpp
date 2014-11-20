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
 * DNP3ServerPort.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/app/DynamicPointIndexes.h>
#include <opendnp3/outstation/Database.h>
#include <opendnp3/outstation/TimeTransaction.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "DNP3OutstationPort.h"


DNP3OutstationPort::DNP3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DNP3Port(aName, aConfFilename, aConfOverrides),
	lastRx(0),
	pPollStatTimer(new Timer_t(*pIOS, std::chrono::milliseconds(200)))
{
	pPollStatTimer->async_wait(std::bind(&DNP3OutstationPort::PollStats,this));
};

void DNP3OutstationPort::Enable()
{
	if(enabled)
		return;
	pOutstation->Enable();
	enabled = true;
	PollStats();
	for (auto IOHandler_pair : Subscribers)
	{
		IOHandler_pair.second->Event(ConnectState::PORT_UP, 0, this->Name);
	}
}
void DNP3OutstationPort::Disable()
{
	if(!enabled)
		return;
	pOutstation->Disable();
	enabled = false;
}
void DNP3OutstationPort::StateListener(opendnp3::ChannelState state)
{
	if(!enabled)
		return;

	//This has been replaced by a stack statistics poller - so connect events are sent on application layer connection instead of comms layer
	//for(auto IOHandler_pair : Subscribers)
	//{
		//IOHandler_pair.second->Event((state == opendnp3::ChannelState::OPEN), 0, this->Name);
	//}
}
void DNP3OutstationPort::PollStats()
{
	if(!enabled)
		return;
	auto stats = pOutstation->GetStackStatistics();
	if(stats.numTransportRx > lastRx)
	{
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
		}
	}
	lastRx = stats.numTransportRx;
	pPollStatTimer->expires_from_now(std::chrono::milliseconds(200));
	pPollStatTimer->async_wait(std::bind(&DNP3OutstationPort::PollStats,this));
}
void DNP3OutstationPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	auto IPPort = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	auto log_id = "outst_"+IPPort;

	//create a new channel if one isn't already up
	if(!TCPChannels.count(IPPort))
	{
		TCPChannels[IPPort] = DNP3Mgr.AddTCPServer(log_id.c_str(), LOG_LEVEL.GetBitfield(),
											openpal::TimeDuration::Seconds(5),
											openpal::TimeDuration::Seconds(300),
											pConf->mAddrConf.IP,
											pConf->mAddrConf.Port);
	}

	//FIXME: change this to listen to the state of the outstation, not it's channel, if there is a callback interface added to the outstation in the future
	TCPChannels[IPPort]->AddStateListener(std::bind(&DNP3OutstationPort::StateListener,this,std::placeholders::_1));

	opendnp3::OutstationStackConfig StackConfig;

	// Link layer configuration
	StackConfig.link.LocalAddr = pConf->mAddrConf.OutstationAddr;
	StackConfig.link.NumRetry = pConf->pPointConf->LinkNumRetry;
	StackConfig.link.RemoteAddr = pConf->mAddrConf.MasterAddr;
	StackConfig.link.Timeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->LinkTimeoutms);
	StackConfig.link.UseConfirms = pConf->pPointConf->LinkUseConfirms;

	// Outstation parameters
	StackConfig.outstation.params.allowUnsolicited = pConf->pPointConf->EnableUnsol;
	StackConfig.outstation.params.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.outstation.params.maxControlsPerRequest = pConf->pPointConf->MaxControlsPerRequest; 	/// The maximum number of controls the outstation will attempt to process from a single APDU
	StackConfig.outstation.params.maxTxFragSize = pConf->pPointConf->MaxTxFragSize; /// The maximum fragment size the outstation will use for fragments it sends
	StackConfig.outstation.params.selectTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->SelectTimeoutms);/// How long the outstation will allow an operate to proceed after a prior select
	StackConfig.outstation.params.solConfirmTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->SolConfirmTimeoutms);/// Timeout for solicited confirms
	StackConfig.outstation.params.unsolConfirmTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->UnsolConfirmTimeoutms); /// Timeout for unsolicited confirms

	// TODO: Expose default event responses for any new event types to be supported by opendatacon
	StackConfig.outstation.defaultEventResponses.analog = pConf->pPointConf->EventAnalogResponse;
	StackConfig.outstation.defaultEventResponses.binary = pConf->pPointConf->EventBinaryResponse;
	StackConfig.outstation.defaultEventResponses.counter = pConf->pPointConf->EventCounterResponse;

	// TODO: Expose event limits for any new event types to be supported by opendatacon
	StackConfig.outstation.eventBufferConfig.maxBinaryEvents = pConf->pPointConf->MaxBinaryEvents; /// The number of binary events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxAnalogEvents = pConf->pPointConf->MaxAnalogEvents;	/// The number of analog events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxCounterEvents = pConf->pPointConf->MaxCounterEvents;	/// The number of counter events the outstation will buffer before overflowing

	//contiguous points
//	StackConfig.dbTemplate.numAnalog = pConf->pPointConf->AnalogIndicies.back();
//	StackConfig.dbTemplate.numBinary = pConf->pPointConf->BinaryIndicies.back();

	//noncontiguous points
	auto AnaIndexable = openpal::Indexable<uint32_t, uint32_t>(pConf->pPointConf->AnalogIndicies.data(),pConf->pPointConf->AnalogIndicies.size());
	auto BinIndexable = openpal::Indexable<uint32_t, uint32_t>(pConf->pPointConf->BinaryIndicies.data(),pConf->pPointConf->BinaryIndicies.size());
	auto AnaIndexes = opendnp3::DynamicPointIndexes(AnaIndexable);
	auto BinIndexes = opendnp3::DynamicPointIndexes(BinIndexable);
	StackConfig.dbTemplate = opendnp3::DatabaseTemplate(BinIndexes, opendnp3::PointIndexes::EMPTYINDEXES, AnaIndexes);

    auto TargetChan = TCPChannels[IPPort];
    
    if (TargetChan == nullptr)
    {
        std::cout << "TCP channel not found for outstation '" << Name << std::endl;
        return;
    }
    
	pOutstation = TargetChan->AddOutstation(Name.c_str(), *this, opendnp3::DefaultOutstationApplication::Instance(), StackConfig);
    
    if (pOutstation == nullptr)
    {
        std::cout << "Error creating outstation '" << Name << std::endl;
        return;
    }

	for(auto index : pConf->pPointConf->AnalogIndicies)
	{
		auto pos = AnaIndexes.operator opendnp3::PointIndexes().GetPosition(index);
		pOutstation->GetDatabase().staticData.analogs.metadata[pos].clazz = pConf->pPointConf->AnalogClasses[index];
		pOutstation->GetDatabase().staticData.analogs.metadata[pos].deadband = pConf->pPointConf->AnalogDeadbands[index];
	}
	for(auto index : pConf->pPointConf->BinaryIndicies)
	{
		auto pos = BinIndexes.operator opendnp3::PointIndexes().GetPosition(index);
		pOutstation->GetDatabase().staticData.binaries.metadata[pos].clazz = pConf->pPointConf->BinaryClasses[index];
	}
}

template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//FIXME: this is meant to return if we support the type of command
	//at the moment we just return success if it's configured as a control
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(std::is_same<T,opendnp3::ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	{
		for(auto index : pConf->pPointConf->ControlIndicies)
			if(index == aIndex)
				return opendnp3::CommandStatus::SUCCESS;
	}
	return opendnp3::CommandStatus::NOT_SUPPORTED;
}
template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//container to store our async futures
	std::vector<std::future<opendnp3::CommandStatus>> future_results;

	for(auto IOHandler_pair : Subscribers)
	{
		future_results.push_back((IOHandler_pair.second->Event(arCommand, aIndex, this->Name)));
	}

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if (!pConf->pPointConf->WaitForCommandResponses)
	{
		return opendnp3::CommandStatus::SUCCESS;
	}

	for(auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while(future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if(future_result.get() != opendnp3::CommandStatus::SUCCESS)
			return opendnp3::CommandStatus::UNDEFINED;
	}

	return opendnp3::CommandStatus::SUCCESS;
}

std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };

template<typename T>
inline std::future<opendnp3::CommandStatus> DNP3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	{//transaction scope
		opendnp3::TimeTransaction tx(pOutstation->GetDatabase(), asiopal::UTCTimeSource::Instance().Now());
		tx.Update(meas, index);
	}
	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(ConnectState state, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	auto cmd_future = cmd_promise.get_future();

	if (!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_future;
	}

	if (state == ConnectState::DISCONNECTED)
	{
		//stub		
	}

	cmd_promise.set_value(opendnp3::CommandStatus::SUCCESS);
	return cmd_future;
}

