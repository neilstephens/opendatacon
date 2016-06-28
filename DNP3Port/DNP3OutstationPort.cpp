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
#include <asiodnp3/MeasUpdate.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include <openpal/logging/LogLevels.h>
#include "DNP3OutstationPort.h"
#include "DNP3PortConf.h"
#include "ChannelStateSubscriber.h"

#include "OpenDNP3Helpers.h"

DNP3OutstationPort::DNP3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DNP3Port(aName, aConfFilename, aConfOverrides),
	pOutstation(nullptr)
{}

DNP3OutstationPort::~DNP3OutstationPort()
{
	//pOutstation->Shutdown();
	ChannelStateSubscriber::Unsubscribe(this);
}

void DNP3OutstationPort::Enable()
{
	if(enabled)
		return;
	if(nullptr == pOutstation)
	{
		std::string msg = Name + ": Port not configured.";
		auto log_entry = openpal::LogEntry("DNP3OutstationPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);

		return;
	}
	pOutstation->Enable();
	enabled = true;

	for (auto IOHandler_pair : Subscribers)
	{
		IOHandler_pair.second->Event(ConnectState::PORT_UP, 0, this->Name);
	}
}
void DNP3OutstationPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	pOutstation->Disable();
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up)
void DNP3OutstationPort::OnStateChange(opendnp3::LinkStatus status)
{
	this->status = status;
	if(link_dead)
	{
		link_dead = false;
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
		}
	}
	//TODO: track a new statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3OutstationPort::OnKeepAliveFailure()
{
	this->OnLinkDown();
}
void DNP3OutstationPort::OnLinkDown()
{
	if(!link_dead)
	{
		link_dead = true;
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::DISCONNECTED, 0, this->Name);
		}
	}
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3OutstationPort::OnKeepAliveSuccess()
{
	if(link_dead)
	{
		link_dead = false;
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
		}
	}
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
									 opendnp3::ChannelRetry(
										 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPListenRetryPeriodMinms),
										 openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPListenRetryPeriodMaxms)),
		                                           pConf->mAddrConf.IP,
		                                           pConf->mAddrConf.Port);
	}

	opendnp3::OutstationStackConfig StackConfig;

	// Link layer configuration
	StackConfig.link.LocalAddr = pConf->mAddrConf.OutstationAddr;
	StackConfig.link.RemoteAddr = pConf->mAddrConf.MasterAddr;
	StackConfig.link.NumRetry = pConf->pPointConf->LinkNumRetry;
	StackConfig.link.Timeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->LinkTimeoutms);
	if(pConf->pPointConf->LinkKeepAlivems == 0)
		StackConfig.link.KeepAliveTimeout = openpal::TimeDuration::Max();
	else
		StackConfig.link.KeepAliveTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->LinkKeepAlivems);
	StackConfig.link.UseConfirms = pConf->pPointConf->LinkUseConfirms;

	// Outstation parameters
	StackConfig.outstation.params.indexMode = opendnp3::IndexMode::Discontiguous;
	StackConfig.outstation.params.allowUnsolicited = pConf->pPointConf->EnableUnsol;
	StackConfig.outstation.params.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.outstation.params.typesAllowedInClass0 = opendnp3::StaticTypeBitField::AllTypes();                                     /// TODO: Create parameter
	StackConfig.outstation.params.maxControlsPerRequest = pConf->pPointConf->MaxControlsPerRequest;                                    /// The maximum number of controls the outstation will attempt to process from a single APDU
	StackConfig.outstation.params.maxTxFragSize = pConf->pPointConf->MaxTxFragSize;                                                    /// The maximum fragment size the outstation will use for fragments it sends
	StackConfig.outstation.params.maxRxFragSize = pConf->pPointConf->MaxTxFragSize;                                                    /// The maximum fragment size the outstation will use for fragments it sends
	StackConfig.outstation.params.selectTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->SelectTimeoutms);             /// How long the outstation will allow an operate to proceed after a prior select
	StackConfig.outstation.params.solConfirmTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->SolConfirmTimeoutms);     /// Timeout for solicited confirms
	StackConfig.outstation.params.unsolConfirmTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->UnsolConfirmTimeoutms); /// Timeout for unsolicited confirms
	StackConfig.outstation.params.unsolRetryTimeout = openpal::TimeDuration::Milliseconds(pConf->pPointConf->UnsolConfirmTimeoutms);   /// Timeout for unsolicited retried

	// TODO: Expose event limits for any new event types to be supported by opendatacon
	StackConfig.outstation.eventBufferConfig.maxBinaryEvents = pConf->pPointConf->MaxBinaryEvents;   /// The number of binary events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxAnalogEvents = pConf->pPointConf->MaxAnalogEvents;   /// The number of analog events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxCounterEvents = pConf->pPointConf->MaxCounterEvents; /// The number of counter events the outstation will buffer before overflowing

	StackConfig.dbTemplate = opendnp3::DatabaseTemplate(pConf->pPointConf->BinaryIndicies.size(), 0, pConf->pPointConf->AnalogIndicies.size());

	pChannel = TCPChannels[IPPort];

	if (pChannel == nullptr)
	{
		std::string msg = Name + ": TCP channel not found for outstation.";
		auto log_entry = openpal::LogEntry("DNP3OutstationPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}

	pOutstation = pChannel->AddOutstation(Name.c_str(), *this, *this, StackConfig);
	ChannelStateSubscriber::Subscribe(this,pChannel);

	if (pOutstation == nullptr)
	{
		std::string msg = Name + ": Error creating outstation.";
		auto log_entry = openpal::LogEntry("DNP3OutstationPort", openpal::logflags::ERR, "", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}

	auto configView = pOutstation->GetConfigView();

	{
		uint16_t rawIndex = 0;
		for (auto index : pConf->pPointConf->AnalogIndicies)
		{
			configView.analogs[rawIndex].vIndex = index;
			configView.analogs[rawIndex].variation = pConf->pPointConf->StaticAnalogResponse;
			configView.analogs[rawIndex].metadata.clazz = pConf->pPointConf->AnalogClasses[index];
			configView.analogs[rawIndex].metadata.variation = pConf->pPointConf->EventAnalogResponse;
			configView.analogs[rawIndex].metadata.deadband = pConf->pPointConf->AnalogDeadbands[index];
			++rawIndex;
		}
	}
	{
		uint16_t rawIndex = 0;
		for (auto index : pConf->pPointConf->BinaryIndicies)
		{
			configView.binaries[rawIndex].vIndex = index;
			configView.binaries[rawIndex].variation = pConf->pPointConf->StaticBinaryResponse;
			configView.binaries[rawIndex].metadata.clazz = pConf->pPointConf->BinaryClasses[index];
			configView.binaries[rawIndex].metadata.variation = pConf->pPointConf->EventBinaryResponse;
			++rawIndex;
		}
	}
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetCurrentState() const
{
	Json::Value event;
	Json::Value analogValues;
	Json::Value binaryValues;
	if (pOutstation == nullptr)
		return IUIResponder::GenerateResult("Bad port");

	auto configView = pOutstation->GetConfigView();

	for (auto point : configView.analogs)
	{
		analogValues[std::to_string(point.vIndex)] = point.value.value;
	}
	for (auto point : configView.binaries)
	{
		binaryValues[std::to_string(point.vIndex)] = point.value.value;
	}

	event["AnalogCurrent"] = analogValues;
	event["BinaryCurrent"] = binaryValues;

	return event;
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetStatistics() const
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
	if (pOutstation != nullptr)
	{
		auto StackStats = this->pOutstation->GetStackStatistics();
		event["numTransportErrorRx"] = StackStats.numTransportErrorRx;
		event["numTransportRx"] = StackStats.numTransportRx;
		event["numTransportTx"] = StackStats.numTransportTx;
	}

	return event;
}

template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(std::is_same<T,opendnp3::ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	{
		for(auto index : pConf->pPointConf->ControlIndicies)
			if(index == aIndex)
				return opendnp3::CommandStatus::SUCCESS;
	}
	return opendnp3::CommandStatus::NOT_SUPPORTED;
}

// Called by OpenDNP3 Thread Pool
template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//container to store our async futures
	std::vector<std::future<opendnp3::CommandStatus> > future_results;

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

std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::Binary>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::DoubleBitBinary>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::Analog>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::Counter>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::FrozenCounter>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::BinaryOutputStatus>(qual,index,SenderName);}
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName){return EventQ<opendnp3::AnalogOutputStatus>(qual,index,SenderName);}

template<typename T, typename Q>
inline std::future<opendnp3::CommandStatus> DNP3OutstationPort::EventQ(Q& qual, uint16_t index, const std::string& SenderName)
{
	if (!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}
	auto eventTime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;
	auto lambda = [=](const T &existing)
	{
		//TODO: break out specialised templates for Binary types. The state bit for binary quality is 'reserved' for other currently supported types - preserving it will be OK for now
		uint8_t state = existing.quality & static_cast<uint8_t>(opendnp3::BinaryQuality::STATE);

		T updated = existing;
		updated.quality = static_cast<uint8_t>(qual) | state;
		updated.time = opendnp3::DNPTime(eventTime);
		return updated;
	};
	const auto modify = openpal::Function1<const T&, T>::Bind(lambda);
	{ //transaction scope
		asiodnp3::MeasUpdate tx(pOutstation);
		// TODO: confirm the timestamp used for the modify
		tx.Modify(modify, index, opendnp3::EventMode::Force);
	}
	return IOHandler::CommandFutureSuccess();
}

std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DNP3OutstationPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }

template<typename T>
inline std::future<opendnp3::CommandStatus> DNP3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}
	auto eventTime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	{ //transaction scope
		asiodnp3::MeasUpdate tx(pOutstation);

		if (
		      (pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS) ||
		      ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (meas.time == 0))
		      )
		{
			T newmeas(meas.value, meas.quality, opendnp3::DNPTime(eventTime));
			tx.Update(newmeas, index);
		}
		else
		{
			tx.Update(meas, index);
		}
	}
	return IOHandler::CommandFutureSuccess();
}

std::future<opendnp3::CommandStatus> DNP3OutstationPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	if (!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	if (state == ConnectState::DISCONNECTED)
	{
		//stub
	}

	return IOHandler::CommandFutureSuccess();
}

