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

#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/Database.h>
#include <opendnp3/outstation/TimeTransaction.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "DNP3OutstationPort.h"

DNP3OutstationPort::DNP3OutstationPort(std::string aName, std::string aConfFilename, std::string aConfOverrides):
	DNP3Port(aName, aConfFilename, aConfOverrides)
{};

void DNP3OutstationPort::Enable()
{
	if(enabled)
		return;
	pOutstation->Enable();
	enabled = true;
}
void DNP3OutstationPort::Disable()
{
	if(!enabled)
		return;
	pOutstation->Disable();
	enabled = false;
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
	opendnp3::OutstationStackConfig StackConfig;
	StackConfig.link.LocalAddr = pConf->mAddrConf.OutstationAddr;
	StackConfig.link.RemoteAddr = pConf->mAddrConf.MasterAddr;
	StackConfig.link.NumRetry = 5;
	StackConfig.link.Timeout = openpal::TimeDuration::Seconds(5);
	StackConfig.link.UseConfirms = pConf->pPointConf->UseConfirms;
	StackConfig.outstation.defaultEventResponses.analog = pConf->pPointConf->EventAnalogResponse;
	StackConfig.outstation.defaultEventResponses.binary = pConf->pPointConf->EventBinaryResponse;
	StackConfig.outstation.params.allowUnsolicited = pConf->pPointConf->EnableUnsol;
	StackConfig.outstation.params.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.outstation.params.selectTimeout = openpal::TimeDuration::Seconds(300);/// How long the outstation will allow an operate to proceed after a prior select
	StackConfig.outstation.params.solConfirmTimeout = openpal::TimeDuration::Seconds(300);/// Timeout for solicited confirms
	StackConfig.outstation.params.unsolConfirmTimeout = openpal::TimeDuration::Seconds(300); /// Timeout for unsolicited confirms
	StackConfig.outstation.params.unsolRetryTimeout = openpal::TimeDuration::Seconds(300); /// Timeout for unsolicited retries
	StackConfig.outstation.eventBufferConfig.maxAnalogEvents = 1000;//TODO: event buf size config item
	StackConfig.outstation.eventBufferConfig.maxBinaryEvents = 1000;

	//contiguous points
//	StackConfig.dbTemplate.numAnalog = pConf->pPointConf->AnalogIndicies.back();
//	StackConfig.dbTemplate.numBinary = pConf->pPointConf->BinaryIndicies.back();

	//noncontiguous points
	auto AnaIndexable = openpal::Indexable<uint32_t, uint32_t>(pConf->pPointConf->AnalogIndicies.data(),pConf->pPointConf->AnalogIndicies.size());
	auto BinIndexable = openpal::Indexable<uint32_t, uint32_t>(pConf->pPointConf->BinaryIndicies.data(),pConf->pPointConf->BinaryIndicies.size());
	auto AnaIndexes = opendnp3::DynamicPointIndexes(AnaIndexable);
	auto BinIndexes = opendnp3::DynamicPointIndexes(BinIndexable);
	StackConfig.dbTemplate = opendnp3::DatabaseTemplate(BinIndexes, opendnp3::PointIndexes::EMPTYINDEXES, AnaIndexes);

	pOutstation = TCPChannels[IPPort]->AddOutstation(Name.c_str(), *this, opendnp3::DefaultOutstationApplication::Instance(), StackConfig);
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

