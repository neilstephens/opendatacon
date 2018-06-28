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

#include <opendnp3/app/ClassField.h>
#include <opendnp3/app/MeasurementTypes.h>
#include "DNP3MasterPort.h"
#include "ChannelStateSubscriber.h"
#include <array>
#include <asiopal/UTCTimeSource.h>
#include <spdlog/spdlog.h>

#include "TypeConversion.h"


DNP3MasterPort::~DNP3MasterPort()
{
	//pMaster->Shutdown();
	ChannelStateSubscriber::Unsubscribe(this);
}

void DNP3MasterPort::Enable()
{
	if(enabled)
		return;
	if(nullptr == pMaster)
	{
		if(auto log = spdlog::get("DNP3Port"))
			log->error("{}: DNP3 stack not initialised.", Name);
		return;
	}

	enabled = true;

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
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.quality & static_cast<uint8_t>(BinaryQuality::ONLINE))
	{
		if(auto log = spdlog::get("DNP3Port"))
			log->debug("{}: Updating comms state point to good.", Name);

		auto commsUpEvent = std::make_shared<EventInfo>(EventType::Binary, pConf->pPointConf->mCommsPoint.second, Name);
		auto failed_val = pConf->pPointConf->mCommsPoint.first.value;
		commsUpEvent->SetPayload<EventType::Binary>(std::move(!failed_val));
		PublishEvent(commsUpEvent);
	}
}

void DNP3MasterPort::PortDown()
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if(auto log = spdlog::get("DNP3Port"))
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

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.quality & static_cast<uint8_t>(BinaryQuality::ONLINE))
	{
		if(auto log = spdlog::get("DNP3Port"))
			log->debug("{}: Setting comms point to failed.", Name);

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
	if(link_dead && !channel_dead) //must be on link up
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

		DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if (stack_enabled && pConf->mAddrConf.ServerType != server_type_t::PERSISTENT && !InDemand())
		{
			if(auto log = spdlog::get("DNP3Port"))
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
	if(link_dead)
	{
		link_dead = false;
		// Update the comms state point and qualities
		PortUp();
	}
}

TCPClientServer DNP3MasterPort::ClientOrServer()
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::CLIENT;
	return pConf->mAddrConf.ClientServer;
}

void DNP3MasterPort::BuildOrRebuild()
{
	DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	pChannel = GetChannel();

	if (pChannel == nullptr)
	{
		if(auto log = spdlog::get("DNP3Port"))
			log->error("{}: Channel not found for masterstation.", Name);
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
	StackConfig.master.timeSyncMode = pConf->pPointConf->MasterRespondTimeSync ? opendnp3::TimeSyncMode::SerialTimeSync : opendnp3::TimeSyncMode::None;
	StackConfig.master.disableUnsolOnStartup = !pConf->pPointConf->DoUnsolOnStartup;
	StackConfig.master.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.master.startupIntegrityClassMask = pConf->pPointConf->GetStartupIntegrityClassMask(); //TODO: report/investigate bug - doesn't recognise response to integrity scan if not ALL_CLASSES
	StackConfig.master.integrityOnEventOverflowIIN = pConf->pPointConf->IntegrityOnEventOverflowIIN;
	StackConfig.master.taskRetryPeriod = openpal::TimeDuration::Milliseconds(pConf->pPointConf->TaskRetryPeriodms);

	pMaster = pChannel->AddMaster(Name.c_str(), *this, *this, StackConfig);
	ChannelStateSubscriber::Subscribe(this,pChannel);
	if (pMaster == nullptr)
	{
		if(auto log = spdlog::get("DNP3Port"))
			log->error("{}: Error creating masterstation.", Name);
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
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<Binary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<DoubleBitBinary> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<Analog> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<Counter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<FrozenCounter> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<BinaryOutputStatus> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<AnalogOutputStatus> >& meas){ LoadT(meas); }
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
			if(auto log = spdlog::get("DNP3Port"))
				log->info("{}: Upstream port enabled, performing integrity scan.", Name);

			IntegrityScan.Demand();
		}

		DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());

		// If an upstream port is connected, attempt a connection (if on demand)
		if (!stack_enabled && state == ConnectState::CONNECTED && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			if(auto log = spdlog::get("DNP3Port"))
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
					PortDown();
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
			if(auto log = spdlog::get("DNP3Port"))
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
	if(auto log = spdlog::get("DNP3Port"))
		log->warn("{}: Control sent to invalid DNP3 index: {}", Name, index);

	(*pStatusCallback)(CommandStatus::UNDEFINED);
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
