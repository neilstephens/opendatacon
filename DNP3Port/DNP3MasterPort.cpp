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

#include "ChannelStateSubscriber.h"
#include "DNP3MasterPort.h"
#include "TypeConversion.h"
#include <array>
#include <asiopal/UTCTimeSource.h>
#include <opendatacon/util.h>
#include <opendnp3/app/ClassField.h>
#include <opendnp3/app/MeasurementTypes.h>


DNP3MasterPort::~DNP3MasterPort()
{
	ChannelStateSubscriber::Unsubscribe(this);
	if(IntegrityScan)
		IntegrityScan.reset();
	if(pMaster)
	{
		pMaster->Shutdown();
		pMaster.reset();
	}
	if(pChannel)
		pChannel.reset();
}

void DNP3MasterPort::Enable()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if(enabled)
		return;
	if(nullptr == pMaster)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: DNP3 stack not initialised.", Name);
		return;
	}
	if(!pCommsRideThroughTimer)
		pCommsRideThroughTimer = std::make_shared<CommsRideThroughTimer>(*pIOS,pConf->pPointConf->CommsPointRideThroughTimems,[this](){SetCommsGood();},[this](){SetCommsFailed();});

	enabled = true;

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
		stack_enabled = false;
		pMaster->Disable(); //this will trigger comms down
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: DNP3 stack disabled", Name);
		//don't delay setting comms down if we're disabled intentionally
		pCommsRideThroughTimer->FastForward();
	}
}

void DNP3MasterPort::PortUp()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: PortUp() called.", Name);

	//cancel the timer if we were riding through
	pCommsRideThroughTimer->Cancel();
}

void DNP3MasterPort::PortDown()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: PortDown() called.", Name);

	//trigger the ride through timer
	pCommsRideThroughTimer->Trigger();
}

void DNP3MasterPort::SetCommsGood()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Updating comms point (good).", Name);

		auto commsUpEvent = std::make_shared<EventInfo>(EventType::Binary, pConf->pPointConf->mCommsPoint.second, Name);
		auto failed_val = pConf->pPointConf->mCommsPoint.first.value;
		commsUpEvent->SetPayload<EventType::Binary>(!failed_val);
		PublishEvent(commsUpEvent);
	}
}

void DNP3MasterPort::SetCommsFailed()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if(pConf->pPointConf->SetQualityOnLinkStatus)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Setting point quality to COMM_LOST.", Name);

		for (auto index : pConf->pPointConf->BinaryIndicies)
		{
			auto event = std::make_shared<EventInfo>(EventType::BinaryQuality,index,Name);
			event->SetPayload<EventType::BinaryQuality>(QualityFlags::COMM_LOST);
			PublishEvent(event);
		}
		for (auto index : pConf->pPointConf->AnalogIndicies)
		{
			auto event = std::make_shared<EventInfo>(EventType::AnalogQuality,index,Name);
			event->SetPayload<EventType::AnalogQuality>(QualityFlags::COMM_LOST);
			PublishEvent(event);
		}
	}

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Updating comms point (failed).", Name);

		auto commsDownEvent = std::make_shared<EventInfo>(EventType::Binary, pConf->pPointConf->mCommsPoint.second, Name);
		auto failed_val = pConf->pPointConf->mCommsPoint.first.value;
		commsDownEvent->SetPayload<EventType::Binary>(std::move(failed_val));
		PublishEvent(commsDownEvent);
	}
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up / channel down)
void DNP3MasterPort::OnStateChange(opendnp3::LinkStatus status)
{
	this->status = status;

	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkStatus {}.", Name, opendnp3::LinkStatusToString(status));

	if(link_dead && !channel_dead) //must be on link up
	{
		link_dead = false;
		// Update the comms state point
		PortUp();
	}
	//TODO: track a statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3MasterPort::OnKeepAliveFailure()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveFailure() called.", Name);
	this->OnLinkDown();
}
void DNP3MasterPort::OnLinkDown()
{
	if(!link_dead)
	{
		link_dead = true;
		PortDown();

		// Notify subscribers that a disconnect event has occured
		PublishEvent(ConnectState::DISCONNECTED);

		auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if (stack_enabled && pConf->mAddrConf.ServerType != server_type_t::PERSISTENT && !InDemand())
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: Disabling stack following disconnect on non-persistent port.", Name);

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
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveSuccess() called.", Name);

	if(link_dead)
	{
		link_dead = false;
		// Update the comms state point
		PortUp();

		auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if(pConf->pPointConf->SetQualityOnLinkStatus)
		{
			// Trigger integrity scan to get point quality
			// Only way to get true state upstream
			// Can't just reset quality, because it would make new events for old values
			IntegrityScan->Demand();
		}
	}
}

TCPClientServer DNP3MasterPort::ClientOrServer()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::CLIENT;
	return pConf->mAddrConf.ClientServer;
}

void DNP3MasterPort::Build()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	pChannel = GetChannel();

	if (pChannel == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Channel not found for masterstation.", Name);
		return;
	}

	asiodnp3::MasterStackConfig StackConfig;

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
	StackConfig.master.timeSyncMode = pConf->pPointConf->MasterRespondTimeSync ? opendnp3::TimeSyncMode::NonLAN : opendnp3::TimeSyncMode::None;
	StackConfig.master.disableUnsolOnStartup = !pConf->pPointConf->DoUnsolOnStartup;
	StackConfig.master.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.master.startupIntegrityClassMask = pConf->pPointConf->GetStartupIntegrityClassMask();
	StackConfig.master.integrityOnEventOverflowIIN = pConf->pPointConf->IntegrityOnEventOverflowIIN;
	StackConfig.master.taskRetryPeriod = openpal::TimeDuration::Milliseconds(pConf->pPointConf->TaskRetryPeriodms);

	//FIXME?: hack to create a toothless shared_ptr
	//	this is needed because the main exe manages our memory
	auto wont_free = std::shared_ptr<DNP3MasterPort>(this,[](void*){});
	auto ISOEHandle = std::dynamic_pointer_cast<opendnp3::ISOEHandler>(wont_free);
	auto MasterApp = std::dynamic_pointer_cast<opendnp3::IMasterApplication>(wont_free);

	pMaster = pChannel->AddMaster(Name, ISOEHandle, MasterApp, StackConfig);

	if (pMaster == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Error creating masterstation.", Name);
		return;
	}

	// Master Station scanning configuration
	if(pConf->pPointConf->IntegrityScanRatems > 0)
		IntegrityScan = pMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::ALL_CLASSES), openpal::TimeDuration::Milliseconds(pConf->pPointConf->IntegrityScanRatems));
	else
		IntegrityScan = pMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::ALL_CLASSES), openpal::TimeDuration::Minutes(600000000)); //ten million hours
	if(pConf->pPointConf->EventClass1ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class1), openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass1ScanRatems));
	if(pConf->pPointConf->EventClass2ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class2), openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass2ScanRatems));
	if(pConf->pPointConf->EventClass3ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class3), openpal::TimeDuration::Milliseconds(pConf->pPointConf->EventClass3ScanRatems));
}

// Called by OpenDNP3 Thread Pool
//implement ISOEHandler
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::DoubleBitBinary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Counter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::FrozenCounter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryOutputStatus> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogOutputStatus> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::SecurityStat> >& meas){ /*LoadT(meas);*/ }

template<typename T>
inline void DNP3MasterPort::LoadT(const opendnp3::ICollection<opendnp3::Indexed<T> >& meas)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	meas.ForeachItem([&](const opendnp3::Indexed<T>&pair)
		{
			auto event = ToODC(pair.value, pair.index, Name);
			if ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS) ||
			    ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (pair.value.time == 0)))
			{
			      event->SetTimestamp();
			}
			PublishEvent(event);
		});
}

void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values)
{
	values.ForeachItem([](const opendnp3::DNPTime time)
		{
			//TODO: master recieved time...
		});
}

void DNP3MasterPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	// If the port is disabled, fail the command
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	if(event->GetEventType() == EventType::ConnectState)
	{
		auto state = event->GetPayload<EventType::ConnectState>();

		// If an upstream port has been enabled after the stack has already been enabled, do an integrity scan
		if (stack_enabled && state == ConnectState::PORT_UP)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: Upstream port enabled, performing integrity scan.", Name);

			IntegrityScan->Demand();
		}

		auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

		// If an upstream port is connected, attempt a connection (if on demand)
		if (!stack_enabled && state == ConnectState::CONNECTED && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: upstream port connected, performing on-demand connection.", Name);

			pIOS->post([&]()
				{
					EnableStack();
				});
		}

		// If an upstream port is disconnected, disconnect ourselves if it was the last active connection (if on demand)
		if (stack_enabled && !InDemand() && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			pIOS->post([&]()
				{
					DisableStack();
				});
		}

		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}

	// If the stack is disabled, fail the command
	if (!stack_enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	auto index = event->GetIndex();
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	for(auto i : pConf->pPointConf->ControlIndicies)
	{
		if(i == index)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->debug("{}: Executing direct operate to index: {}", Name, index);

			auto DNP3Callback = [=](const opendnp3::ICommandTaskResult& response)
						  {
							  auto status = CommandStatus::UNDEFINED;
							  switch(response.summary)
							  {
								  case opendnp3::TaskCompletion::SUCCESS:
									  status = CommandStatus::SUCCESS;
									  break;
								  case opendnp3::TaskCompletion::FAILURE_RESPONSE_TIMEOUT:
									  status = CommandStatus::TIMEOUT;
									  break;
								  case opendnp3::TaskCompletion::FAILURE_BAD_RESPONSE:
								  case opendnp3::TaskCompletion::FAILURE_NO_COMMS:
								  default:
									  status = CommandStatus::UNDEFINED;
									  break;
							  }
							  (*pStatusCallback)(status);
							  return;
						  };

			switch(event->GetEventType())
			{
				case EventType::ControlRelayOutputBlock:
				{
					auto lCommand = FromODC<opendnp3::ControlRelayOutputBlock>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand,index,DNP3Callback);
					break;
				}
				case EventType::AnalogOutputInt16:
				{
					auto lCommand = FromODC<opendnp3::AnalogOutputInt16>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand,index,DNP3Callback);
					break;
				}
				case EventType::AnalogOutputInt32:
				{
					auto lCommand = FromODC<opendnp3::AnalogOutputInt32>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand,index,DNP3Callback);
					break;
				}
				case EventType::AnalogOutputFloat32:
				{
					auto lCommand = FromODC<opendnp3::AnalogOutputFloat32>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand,index,DNP3Callback);
					break;
				}
				case EventType::AnalogOutputDouble64:
				{
					auto lCommand = FromODC<opendnp3::AnalogOutputDouble64>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand,index,DNP3Callback);
					break;
				}
				default:
					(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
					break;
			}

			return;
		}
	}
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->warn("{}: Control sent to invalid DNP3 index: {}", Name, index);

	(*pStatusCallback)(CommandStatus::UNDEFINED);
}

//DataPort function for UI
const Json::Value DNP3MasterPort::GetStatistics() const
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
	if (pMaster != nullptr)
	{
		auto StackStats = this->pMaster->GetStackStatistics();
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
