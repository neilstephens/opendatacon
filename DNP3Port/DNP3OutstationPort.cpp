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

#include "ChannelStateSubscriber.h"
#include "DNP3OutstationPort.h"
#include "DNP3PortConf.h"
#include "OpenDNP3Helpers.h"
#include "TypeConversion.h"
#include <asiodnp3/UpdateBuilder.h>
#include <asiodnp3/Updates.h>
#include <asiopal/UTCTimeSource.h>
#include <chrono>
#include <iostream>
#include <opendatacon/util.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include <openpal/logging/LogLevels.h>
#include <regex>

DNP3OutstationPort::DNP3OutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DNP3Port(aName, aConfFilename, aConfOverrides),
	pOutstation(nullptr)
{}

DNP3OutstationPort::~DNP3OutstationPort()
{
	ChannelStateSubscriber::Unsubscribe(this);
	if(pOutstation)
	{
		pOutstation->Shutdown();
		pOutstation.reset();
	}
	if(pChannel)
		pChannel.reset();
}

void DNP3OutstationPort::Enable()
{
	if(enabled)
		return;
	if(nullptr == pOutstation)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: DNP3 stack not configured.", Name);

		return;
	}
	pOutstation->Enable();
	enabled = true;

	PublishEvent(ConnectState::PORT_UP);
}
void DNP3OutstationPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	pOutstation->Disable();
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: DNP3 stack disabled", Name);
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up / channel down)
void DNP3OutstationPort::OnStateChange(opendnp3::LinkStatus status)
{
	this->status = status;

	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkStatus {}.", Name, opendnp3::LinkStatusToString(status));

	if(link_dead && !channel_dead) //must be on link up
	{
		link_dead = false;
		PublishEvent(ConnectState::CONNECTED);
	}
	//TODO: track a new statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3OutstationPort::OnKeepAliveFailure()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveFailure() called.", Name);
	this->OnLinkDown();
}
void DNP3OutstationPort::OnLinkDown()
{
	if(!link_dead)
	{
		link_dead = true;
		PublishEvent(ConnectState::DISCONNECTED);
	}
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3OutstationPort::OnKeepAliveSuccess()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveSuccess() called.", Name);
	if(link_dead)
	{
		link_dead = false;
		PublishEvent(ConnectState::CONNECTED);
	}
}

TCPClientServer DNP3OutstationPort::ClientOrServer()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::SERVER;
	return pConf->mAddrConf.ClientServer;
}

void DNP3OutstationPort::Build()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	pChannel = GetChannel();

	if (pChannel == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Channel not found for outstation.", Name);
		return;
	}

	asiodnp3::OutstationStackConfig StackConfig(opendnp3::DatabaseSizes(
		pConf->pPointConf->BinaryIndicies.size(), //numBinary
		0,                                        //numDoubleBinary
		pConf->pPointConf->AnalogIndicies.size(), //numAnalog
		0,                                        //numCounter
		0,                                        //numFrozenCounter
		0,                                        //numBinaryOutputStatus
		0,                                        //numAnalogOutputStatus
		0,                                        //numTimeAndInterval
		0));                                      //numOctetString

	uint16_t rawIndex = 0;
	for (auto index : pConf->pPointConf->AnalogIndicies)
	{
		StackConfig.dbConfig.analog[rawIndex].vIndex = index;
		StackConfig.dbConfig.analog[rawIndex].svariation = pConf->pPointConf->StaticAnalogResponses[index];
		StackConfig.dbConfig.analog[rawIndex].evariation = pConf->pPointConf->EventAnalogResponses[index];
		StackConfig.dbConfig.analog[rawIndex].clazz = pConf->pPointConf->AnalogClasses[index];
		StackConfig.dbConfig.analog[rawIndex].deadband = pConf->pPointConf->AnalogDeadbands[index];
		++rawIndex;
	}
	rawIndex = 0;
	for (auto index : pConf->pPointConf->BinaryIndicies)
	{
		StackConfig.dbConfig.binary[rawIndex].vIndex = index;
		StackConfig.dbConfig.binary[rawIndex].svariation = pConf->pPointConf->StaticBinaryResponses[index];
		StackConfig.dbConfig.binary[rawIndex].evariation = pConf->pPointConf->EventBinaryResponses[index];
		StackConfig.dbConfig.binary[rawIndex].clazz = pConf->pPointConf->BinaryClasses[index];
		++rawIndex;
	}

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

	//FIXME?: hack to create a toothless shared_ptr
	//	this is needed because the main exe manages our memory
	auto wont_free = std::shared_ptr<DNP3OutstationPort>(this,[](void*){});
	auto pCommandHandle = std::dynamic_pointer_cast<opendnp3::ICommandHandler>(wont_free);
	auto pApplication = std::dynamic_pointer_cast<opendnp3::IOutstationApplication>(wont_free);

	pOutstation = pChannel->AddOutstation(Name, pCommandHandle, pApplication, StackConfig);

	if (pOutstation == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Error creating outstation.", Name);
		return;
	}

	pStateSync = pIOS->make_strand();
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetCurrentState() const
{
	std::atomic_bool stateExecuted(false);
	Json::Value temp_value;
	pStateSync->post([this, &stateExecuted, &temp_value]()
		{
			temp_value = state;
			stateExecuted = true;
		});

	while (!stateExecuted)
	{
		pIOS->poll_one();
	}

	return temp_value;
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetStatistics() const
{
	Json::Value event;
	if (pChannel != nullptr)
	{
		auto ChanStats = this->pChannel->GetStatistics();
		event["parser"]["numHeaderCrcError"] = ChanStats.parser.numHeaderCrcError;
		event["parser"]["numBodyCrcError"] = ChanStats.parser.numBodyCrcError;
		event["parser"]["numLinkFrameRx"] = ChanStats.parser.numLinkFrameRx;
		event["parser"]["numBadLength"] = ChanStats.parser.numBadLength;
		event["parser"]["numBadFunctionCode"] = ChanStats.parser.numBadFunctionCode;
		event["parser"]["numBadFCV"] = ChanStats.parser.numBadFCV;
		event["parser"]["numBadFCB"] = ChanStats.parser.numBadFCB;
		event["channel"]["numOpen"] = ChanStats.channel.numOpen;
		event["channel"]["numOpenFail"] = ChanStats.channel.numOpenFail;
		event["channel"]["numClose"] = ChanStats.channel.numClose;
		event["channel"]["numBytesRx"] = ChanStats.channel.numBytesRx;
		event["channel"]["numBytesTx"] = ChanStats.channel.numBytesTx;
		event["channel"]["numLinkFrameTx"] = ChanStats.channel.numLinkFrameTx;
	}
	if (pOutstation != nullptr)
	{
		auto StackStats = this->pOutstation->GetStackStatistics();
		event["link"]["numBadMasterBit"] = StackStats.link.numBadMasterBit;
		event["link"]["numUnexpectedFrame"] = StackStats.link.numUnexpectedFrame;
		event["link"]["numUnknownDestination"] = StackStats.link.numUnknownDestination;
		event["link"]["numUnknownSource"] = StackStats.link.numUnknownSource;
		event["transport"]["numTransportBufferOverflow"] = StackStats.transport.rx.numTransportBufferOverflow;
		event["transport"]["numTransportDiscard"] = StackStats.transport.rx.numTransportDiscard;
		event["transport"]["numTransportErrorRx"] = StackStats.transport.rx.numTransportErrorRx;
		event["transport"]["numTransportIgnore"] = StackStats.transport.rx.numTransportIgnore;
		event["transport"]["numTransportRx"] = StackStats.transport.rx.numTransportRx;
		event["transport"]["numTransportTx"] = StackStats.transport.tx.numTransportTx;
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

	auto event = ToODC(arCommand, aIndex, Name);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if (!pConf->pPointConf->WaitForCommandResponses)
	{
		PublishEvent(event);
		return opendnp3::CommandStatus::SUCCESS;
	}

	//TODO: enquire about the possibility of the opendnp3 API having a callback for the result
	// Or if the outstation supported Group13Var1/2 (BinaryCommandEvent), we could use that (maybe the latest opendnp3 already does...?)
	// either one would avoid the below polling loop
	std::atomic_bool cb_executed(false);
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&](CommandStatus status)
		{
			cb_status = status;
			cb_executed = true;
		});
	PublishEvent(event, StatusCallback);
	while(!cb_executed)
	{
		//This loop pegs a core and blocks the outstation strand,
		//	but there's no other way to wait for the result.
		//	We can maybe do some work while we wait.
		pIOS->poll_one();
	}
	return FromODC(cb_status);
}

void DNP3OutstationPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	std::string type;
	switch(event->GetEventType())
	{
		case EventType::Binary:
			type = "BinaryCurrent";
			EventT(FromODC<opendnp3::Binary>(event), event->GetIndex());
			break;
		case EventType::Analog:
			type = "AnalogCurrent";
			EventT(FromODC<opendnp3::Analog>(event), event->GetIndex());
			break;
		case EventType::BinaryQuality:
			type = "BinaryQuality";
			EventQ<opendnp3::Binary>(FromODC<opendnp3::BinaryQuality>(event), event->GetIndex(), opendnp3::FlagsType::BinaryInput);
			break;
		case EventType::AnalogQuality:
			type = "AnalogQuality";
			EventQ<opendnp3::Analog>(FromODC<opendnp3::AnalogQuality>(event), event->GetIndex(), opendnp3::FlagsType::AnalogInput);
			break;
		case EventType::ConnectState:
			break;
		default:
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
	}

	const std::string index = std::to_string(event->GetIndex());
	const std::string payload = event->GetPayloadString();
	SetState(type, index, payload);
	(*pStatusCallback)(CommandStatus::SUCCESS);
}

template<typename T, typename Q>
inline void DNP3OutstationPort::EventQ(Q qual, uint16_t index, opendnp3::FlagsType FT)
{
	asiodnp3::UpdateBuilder builder;
	builder.Modify(FT, index, index, static_cast<uint8_t>(qual));
	pOutstation->Apply(builder.Build());
}

template<typename T>
inline void DNP3OutstationPort::EventT(T meas, uint16_t index)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if (
		(pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS) ||
		((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (meas.time == 0))
		)
	{
		meas.time = opendnp3::DNPTime(msSinceEpoch());
	}

	asiodnp3::UpdateBuilder builder;
	builder.Update(meas, index);
	pOutstation->Apply(builder.Build());
}

inline void DNP3OutstationPort::SetState(const std::string& type, const std::string& index, const std::string& payload)
{
	pStateSync->post([=]()
		{
			state[type][index] = payload;
		});
}
