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

#include "DNP3MasterPort.h"
#include "ChannelStateSubscriber.h"
#include "ChannelHandler.h"
#include "TypeConversion.h"
#include <array>
#include <limits>
#include <opendnp3/master/IUTCTimeSource.h>
#include <opendatacon/util.h>
#include <opendnp3/app/ClassField.h>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/IINField.h>


DNP3MasterPort::~DNP3MasterPort()
{
	if(pMaster)
	{
		pMaster->Shutdown();
		pMaster.reset();
	}
	ChannelStateSubscriber::Unsubscribe(pChanH.get());
	pChanH.reset();
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
		pCommsRideThroughTimer = std::make_shared<CommsRideThroughTimer>(*pIOS,
			pConf->pPointConf->CommsPointRideThroughTimems,
			[this](){SetCommsGood();},
			[this](){SetCommsFailed();},
			[this](bool f){CommsHeartBeat(f);},
			pConf->pPointConf->CommsPointHeartBeatTimems);

	pCommsRideThroughTimer->StartHeartBeat();

	enabled = true;

	//initialise as comms down - in case they never come up
	PortDown();

	if(!stack_enabled && !(pConf->mAddrConf.ServerType == server_type_t::MANUAL))
	{
		if(pConf->mAddrConf.ServerType == server_type_t::PERSISTENT || InDemand())
			EnableStack();
	}

}
void DNP3MasterPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	pCommsRideThroughTimer->StopHeartBeat();

	if(stack_enabled)
	{
		DisableStack(); //this will trigger comms down
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: DNP3 stack disabled", Name);
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
	//but we don't want to wait if we're intentionally disabled
	if(!enabled)
		pCommsRideThroughTimer->FastForward();
}

void DNP3MasterPort::UpdateCommsPoint(bool isFailed)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	// Update the comms state point if configured
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Updating comms point ({}).", Name, isFailed ? "failed" : "good");

		auto commsEvent = std::make_shared<EventInfo>(EventType::Binary, pConf->pPointConf->mCommsPoint.second, Name);
		auto failed_val = pConf->pPointConf->mCommsPoint.first.value;
		commsEvent->SetPayload<EventType::Binary>(isFailed ? failed_val : !failed_val);
		PublishEvent(commsEvent);
		pDB->Set(commsEvent);
	}
}

void DNP3MasterPort::RePublishEvents()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: Re-publishing events.", Name);
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	for (auto index : pConf->pPointConf->BinaryIndexes)
	{
		auto ev = pDB->Get(EventType::Binary,index);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
	for (auto index : pConf->pPointConf->AnalogIndexes)
	{
		auto ev = pDB->Get(EventType::Analog,index);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
	for (auto index : pConf->pPointConf->OctetStringIndexes)
	{
		auto ev = pDB->Get(EventType::OctetString,index);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
	for (auto index : pConf->pPointConf->AnalogOutputStatusIndexes)
	{
		auto ev = pDB->Get(EventType::AnalogOutputStatus,index);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
	for (auto index : pConf->pPointConf->BinaryOutputStatusIndexes)
	{
		auto ev = pDB->Get(EventType::BinaryOutputStatus,index);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
	if (pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
	{
		auto ev = pDB->Get(EventType::Binary,pConf->pPointConf->mCommsPoint.second);
		if(ev->HasPayload())
			PublishEvent(ev);
	}
}

void DNP3MasterPort::SetCommsGood()
{
	UpdateCommsPoint(false);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->pPointConf->SetQualityOnLinkStatus)
	{
		// Trigger integrity scan to get point quality
		// Only way to get true state upstream
		// Can't just reset quality, because it would make new events for old values
		pChanH->Post([this]()
			{
				if(!IntegrityScanDone && pChanH->GetLinkDeadness() == LinkDeadness::LinkUpChannelUp)
				{
					if(auto log = odc::spdlog_get("DNP3Port"))
						log->debug("{}: Setting IntegrityScanNeeded for SetQualityOnLinkStatus.",Name);
					IntegrityScanNeeded = true;
				}
			});
	}
}

void DNP3MasterPort::SetCommsFailed()
{
	UpdateCommsPoint(true);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->pPointConf->SetQualityOnLinkStatus)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Setting {}, clearing {}, point quality.", Name, ToString(pConf->pPointConf->FlagsToSetOnLinkStatus), ToString(pConf->pPointConf->FlagsToClearOnLinkStatus));

		SetCommsFailedQuality<EventType::Binary, EventType::BinaryQuality>(pConf->pPointConf->BinaryIndexes);
		SetCommsFailedQuality<EventType::Analog, EventType::AnalogQuality>(pConf->pPointConf->AnalogIndexes);
		SetCommsFailedQuality<EventType::AnalogOutputStatus, EventType::AnalogOutputStatusQuality>(pConf->pPointConf->AnalogOutputStatusIndexes);
		SetCommsFailedQuality<EventType::BinaryOutputStatus, EventType::BinaryOutputStatusQuality>(pConf->pPointConf->BinaryOutputStatusIndexes);
	}
}

template <EventType etype, EventType qtype>
void DNP3MasterPort::SetCommsFailedQuality(std::vector<uint16_t>& indexes)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	for (auto index : indexes)
	{
		auto last_event = pDB->Get(etype,index);
		auto new_qual = (last_event->GetQuality() | pConf->pPointConf->FlagsToSetOnLinkStatus) & ~pConf->pPointConf->FlagsToClearOnLinkStatus;

		auto event = std::make_shared<EventInfo>(qtype,index,Name);
		event->SetPayload<qtype>(QualityFlags(new_qual));
		PublishEvent(event);

		//update the EventDB event with the quality as well
		auto new_event = std::make_shared<EventInfo>(*last_event);
		new_event->SetQuality(std::move(new_qual));
		pDB->Set(new_event);
	}
}

void DNP3MasterPort::CommsHeartBeat(bool isFailed)
{
	if(isFailed)
		SetCommsFailed();
	else
		UpdateCommsPoint(isFailed); //don't call SetCommsFailed() because it does an integrity scan
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up / channel down)
void DNP3MasterPort::OnStateChange(opendnp3::LinkStatus status)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkStatus {}.", Name, opendnp3::LinkStatusSpec::to_human_string(status));
	pChanH->SetLinkStatus(status);
	//TODO: track a statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3MasterPort::OnKeepAliveFailure()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveFailure() called.", Name);
	pChanH->LinkDown();
}

//calls to this will be synchronised
void DNP3MasterPort::LinkDeadnessChange(LinkDeadness from, LinkDeadness to)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if(to == LinkDeadness::LinkUpChannelUp) //must be on link up
	{
		// Update the comms state point
		PortUp();

		if(pConf->pPointConf->LinkUpIntegrityTrigger == DNP3PointConf::LinkUpIntegrityTrigger_t::ON_EVERY)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->debug("{}: Setting IntegrityScanNeeded for Link-up.",Name);
			IntegrityScanNeeded = true;
		}

		// Notify subscribers that a connect event has occured
		NotifyOfConnection();

		return;
	}

	if(from == LinkDeadness::LinkUpChannelUp) //must be on link down
	{
		PortDown();

		// Notify subscribers that a disconnect event has occured
		NotifyOfDisconnection();

		if (stack_enabled && pConf->mAddrConf.ServerType != server_type_t::PERSISTENT && !InDemand())
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: Disabling stack following disconnect on non-persistent port.", Name);

			// For all but persistent connections, and in-demand ONDEMAND connections, disable the stack
			pIOS->post([this]()
				{
					DisableStack();
				});
		}
		IntegrityScanDone = false;
		return;
	}
}

void DNP3MasterPort::ChannelWatchdogTrigger(bool on)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: ChannelWatchdogTrigger({}) called.", Name, on);
	if(stack_enabled)
	{
		if(on)                    //don't mark the stack as disabled, because this is just a restart
			pMaster->Disable(); //it will be enabled again shortly when the trigger is off
		else
			pMaster->Enable();
	}
}

// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3MasterPort::OnKeepAliveSuccess()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveSuccess() called.", Name);
	pChanH->LinkUp();
}
// Called by OpenDNP3 Thread Pool
// Called when a valid response resets the keep alive timer
void DNP3MasterPort::OnKeepAliveReset()
{
	pChanH->LinkUp();
}
void DNP3MasterPort::OnReceiveIIN(const opendnp3::IINField& iin)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->trace("{}: OnReceiveIIN(MSB {}, LSB {}) called.", Name, iin.MSB, iin.LSB);
	pChanH->LinkUp();
	pChanH->Post([this,iin]()
		{
			if(pChanH->GetLinkDeadness() == LinkDeadness::LinkUpChannelUp)
			{
				auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
				if((iin.IsSet(opendnp3::IINBit::EVENT_BUFFER_OVERFLOW) && pConf->pPointConf->IntegrityOnEventOverflowIIN) ||
				   (iin.IsSet(opendnp3::IINBit::DEVICE_RESTART) && pConf->pPointConf->IgnoreRestartIIN == false))
				{
					if(auto log = odc::spdlog_get("DNP3Port"))
						log->debug("{}: Stack executed integrity scan for this link session.",Name);
					IntegrityScanDone = true;
				}
				if(IntegrityScanNeeded)
				{
					IntegrityScanNeeded = false;
					if(IntegrityScanDone)
					{
						if(auto log = odc::spdlog_get("DNP3Port"))
							log->debug("{}: Skipping startup integrity scan (stack already did one).",Name);
						return;
					}
					if(auto log = odc::spdlog_get("DNP3Port"))
						log->debug("{}: Executing startup integrity scan.",Name);
					pMaster->ScanClasses(pConf->pPointConf->GetStartupIntegrityClassMask(),ISOEHandle);
					IntegrityScanDone = true;
				}
			}
		});
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

	if (!pChanH->SetChannel())
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Channel not found for masterstation.", Name);
		return;
	}

	InitEventDB();

	opendnp3::MasterStackConfig StackConfig;
	opendnp3::LinkConfig link(true,pConf->pPointConf->LinkUseConfirms);

	// Link layer configuration
	link.LocalAddr = pConf->mAddrConf.MasterAddr;
	link.RemoteAddr = pConf->mAddrConf.OutstationAddr;
	link.Timeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->LinkTimeoutms);
	if(pConf->pPointConf->LinkKeepAlivems == 0)
		link.KeepAliveTimeout = opendnp3::TimeDuration::Max();
	else
		link.KeepAliveTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->LinkKeepAlivems);

	StackConfig.link = link;

	// Master station configuration
	StackConfig.master.responseTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->MasterResponseTimeoutms);
	StackConfig.master.timeSyncMode = pConf->pPointConf->MasterRespondTimeSync ?
	                                  (pConf->pPointConf->LANModeTimeSync ? opendnp3::TimeSyncMode::LAN : opendnp3::TimeSyncMode::NonLAN)
	                                  : opendnp3::TimeSyncMode::None;
	StackConfig.master.disableUnsolOnStartup = !pConf->pPointConf->DoUnsolOnStartup;
	StackConfig.master.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();

	//set the internal sizes of the ADPU buffers - we make it symetric by using MaxTxFragSize for both
	//	but maybe we should have separate setting???
	StackConfig.master.maxTxFragSize = pConf->pPointConf->MaxTxFragSize;
	StackConfig.master.maxRxFragSize = pConf->pPointConf->MaxTxFragSize;

	//Don't set a startup integ scan here, because we handle it ourselves in the link state machine
	StackConfig.master.startupIntegrityClassMask = opendnp3::ClassField::None();

	//Configure mandatory integrity scans separately to the disabled startup scan
	StackConfig.master.useAlternateMaskForForcedIntegrity = true;
	StackConfig.master.alternateIntegrityClassMask = pConf->pPointConf->GetForcedIntegrityClassMask();
	StackConfig.master.integrityOnEventOverflowIIN = pConf->pPointConf->IntegrityOnEventOverflowIIN;
	StackConfig.master.ignoreRestartIIN = pConf->pPointConf->IgnoreRestartIIN;
	StackConfig.master.retryForcedIntegrity = pConf->pPointConf->RetryForcedIntegrity;

	//TODO?: have a separate max retry time to pass to opendnp3
	StackConfig.master.taskRetryPeriod = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->TaskRetryPeriodms);
	StackConfig.master.maxTaskRetryPeriod = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->TaskRetryPeriodms);

	//FIXME?: hack to create a toothless shared_ptr
	//	this is needed because the main exe manages our memory
	auto wont_free = std::shared_ptr<DNP3MasterPort>(this,[](void*){});
	ISOEHandle = std::dynamic_pointer_cast<opendnp3::ISOEHandler>(wont_free);
	MasterApp = std::dynamic_pointer_cast<opendnp3::IMasterApplication>(wont_free);

	pMaster = pChanH->GetChannel()->AddMaster(Name, ISOEHandle, MasterApp, StackConfig);

	if (pMaster == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Error creating masterstation.", Name);
		return;
	}

	// Master Station scanning configuration
	if(pConf->pPointConf->IntegrityScanRatems > 0)
		pMaster->AddClassScan(pConf->pPointConf->GetStartupIntegrityClassMask(), opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IntegrityScanRatems),ISOEHandle);
	if(pConf->pPointConf->EventClass1ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class1), opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->EventClass1ScanRatems),ISOEHandle);
	if(pConf->pPointConf->EventClass2ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class2), opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->EventClass2ScanRatems),ISOEHandle);
	if(pConf->pPointConf->EventClass3ScanRatems > 0)
		pMaster->AddClassScan(opendnp3::ClassField(opendnp3::PointClass::Class3), opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->EventClass3ScanRatems),ISOEHandle);
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
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas)
{
	if(info.gv == opendnp3::GroupVariation::Group111Var0) //TODO: implement another ODC type for static data if there's any use for it
		LoadT(meas);
}
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent> >& meas){ /*LoadT(meas);*/ }

template<typename T>
inline void DNP3MasterPort::LoadT(const opendnp3::ICollection<opendnp3::Indexed<T> >& meas)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	meas.ForeachItem([this,pConf](const opendnp3::Indexed<T>&pair)
		{
			auto event = ToODC(pair.value, pair.index, Name);
			if ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS) ||
			    ((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (pair.value.time.value == 0)))
			{
				event->SetTimestamp();
			}
			PublishEvent(event);
			pDB->Set(event);
		});
}

template<>
inline void DNP3MasterPort::LoadT<opendnp3::OctetString>(const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas)
{
	meas.ForeachItem([this](const opendnp3::Indexed<opendnp3::OctetString>&pair)
		{
			auto event = ToODC(pair.value, pair.index, Name);
			PublishEvent(event);
			pDB->Set(event);
		});
}

void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values)
{
	values.ForeachItem([](const opendnp3::DNPTime time)
		{
			//TODO: master received time...
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
	pDB->Set(event);

	if(event->GetEventType() == EventType::ConnectState)
	{
		auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

		auto state = event->GetPayload<EventType::ConnectState>();

		if(state == ConnectState::PORT_UP)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: Upstream port enabled.", Name);
			RePublishEvents();
		}

		// If an upstream port is connected, attempt a connection (if on demand)
		if (!stack_enabled && state == ConnectState::CONNECTED && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: Upstream port connected, performing on-demand connection.", Name);

			pIOS->post([this]()
				{
					EnableStack();
				});
		}

		// If an upstream port is disconnected, disconnect ourselves if it was the last active connection (if on demand)
		if (stack_enabled && !InDemand() && pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->info("{}: No upstream connections left, performing on-demand disconnection.", Name);

			pIOS->post([this]()
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

	//FIXME: ControlIndexes are checked before AnalogControlIndexes, without checking event type
	//	so if a point is in both, it will be treated as a binary control and error out.
	//	Small refactor needed to check event type first, then check control indexes

	// TODO: SJE Need to process the analogcontrols in a separate loop.
	for (auto i : pConf->pPointConf->ControlIndexes)
	{
		if (i == index)
		{
			if (auto log = odc::spdlog_get("DNP3Port"))
				log->debug("{}: Executing direct operate to index: {}", Name, index);

			auto DNP3Callback = [=](const opendnp3::ICommandTaskResult& response)
						  {
							  auto status = CommandStatus::UNDEFINED;
							  switch (response.summary)
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

			switch (event->GetEventType())
			{
				case EventType::ControlRelayOutputBlock:
				{
					auto lCommand = FromODC<opendnp3::ControlRelayOutputBlock>(event);
					DoOverrideControlCode(lCommand);
					this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
					break;
				}
				default:
					(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
					break;
			}
			return;
		}
	}
	for (auto i : pConf->pPointConf->AnalogControlIndexes)
	{
		if (i == index)
		{
			if (auto log = odc::spdlog_get("DNP3Port"))
				log->debug("{}: Executing analog control to index: {}", Name, index);

			auto DNP3Callback = [=](const opendnp3::ICommandTaskResult& response)
						  {
							  auto status = CommandStatus::UNDEFINED;
							  switch (response.summary)
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

			// Here we may get a 16 bit event, but the master station may be configured to send a 32 bit command.
			// So we need to do the translation (without triggering any exceptions)
			// Create the basis of the target event
			auto evttype = pConf->pPointConf->AnalogControlTypes[index];
			// Now create an event of this type and copy the quality and time from the source event
			auto newevent = std::make_shared<const EventInfo>(evttype, index, "", event->GetQuality(), event->GetTimestamp());
			// Still have to set the payload (possibley with conversion)
			std::unordered_set<EventType> s = { EventType::AnalogOutputInt16, EventType::AnalogOutputInt32, EventType::AnalogOutputFloat32, EventType::AnalogOutputDouble64 };
			if (s.count(event->GetEventType()))
			{
				// Turn the payload into double64, then convert to the target.
				double value = 0;
				CommandStatus status = CommandStatus::UNDEFINED;
				switch (event->GetEventType())
				{
					case EventType::AnalogOutputInt16:
					{
						auto pld = event->GetPayload<EventType::AnalogOutputInt16>();
						value = pld.first; // Int16 to Double64
						status = pld.second;
						break;
					}
					case EventType::AnalogOutputInt32:
					{
						auto pld = event->GetPayload<EventType::AnalogOutputInt32>();
						value = pld.first; // Int32 to Double64
						status = pld.second;
						break;
					}
					case EventType::AnalogOutputFloat32:
					{
						auto pld = event->GetPayload<EventType::AnalogOutputFloat32>();
						value = pld.first; // Float32 to Double64
						status = pld.second;
						break;
					}
					case EventType::AnalogOutputDouble64:
					{
						auto pld = event->GetPayload<EventType::AnalogOutputDouble64>();
						value = pld.first; // Double64 to Double64
						status = pld.second;
						break;
					}
					default:
						break;
				}

				// Now need to create the new payload, of the correct type
				switch (evttype)
				{
					case EventType::AnalogOutputInt16:
					{
						if (value > std::numeric_limits<int16_t>::max())
							value = std::numeric_limits<int16_t>::max();
						if (value < std::numeric_limits<int16_t>::min())
							value = std::numeric_limits<int16_t>::min();
						opendnp3::AnalogOutputInt16 lCommand;
						lCommand.value = static_cast<int16_t>(value);
						lCommand.status = FromODC(status);
						DoOverrideControlCode(lCommand);
						this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
						break;
					}
					case EventType::AnalogOutputInt32:
					{
						if (value > std::numeric_limits<int32_t>::max())
							value = std::numeric_limits<int32_t>::max();
						if (value < std::numeric_limits<int32_t>::min())
							value = std::numeric_limits<int32_t>::min();
						opendnp3::AnalogOutputInt32 lCommand;
						lCommand.value = static_cast<int32_t>(value);
						lCommand.status = FromODC(status);
						DoOverrideControlCode(lCommand);
						this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
						break;
					}
					case EventType::AnalogOutputFloat32:
					{
						if (value > std::numeric_limits<float>::max())
							value = std::numeric_limits<float>::max();
						if (value < std::numeric_limits<float>::lowest())
							value = std::numeric_limits<float>::lowest();
						opendnp3::AnalogOutputFloat32 lCommand;
						lCommand.value = static_cast<float>(value);
						lCommand.status = FromODC(status);
						DoOverrideControlCode(lCommand);
						this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
						break;
					}
					case EventType::AnalogOutputDouble64:
					{
						opendnp3::AnalogOutputDouble64 lCommand;
						lCommand.value = value;
						lCommand.status = FromODC(status);
						DoOverrideControlCode(lCommand);
						this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
						break;
					}
					default:
						break;
				}
			}
			else
			{
				(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
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

	if (auto pChan = pChanH->GetChannel())
	{
		auto ChanStats = pChan->GetStatistics();
		event["parser"]["numHeaderCrcError"] = Json::UInt(ChanStats.parser.numHeaderCrcError);
		event["parser"]["numBodyCrcError"] = Json::UInt(ChanStats.parser.numBodyCrcError);
		event["parser"]["numLinkFrameRx"] = Json::UInt(ChanStats.parser.numLinkFrameRx);
		event["parser"]["numBadLength"] = Json::UInt(ChanStats.parser.numBadLength);
		event["parser"]["numBadFunctionCode"] = Json::UInt(ChanStats.parser.numBadFunctionCode);
		event["parser"]["numBadFCV"] = Json::UInt(ChanStats.parser.numBadFCV);
		event["parser"]["numBadFCB"] = Json::UInt(ChanStats.parser.numBadFCB);
		event["channel"]["numOpen"] = Json::UInt(ChanStats.channel.numOpen);
		event["channel"]["numOpenFail"] = Json::UInt(ChanStats.channel.numOpenFail);
		event["channel"]["numClose"] = Json::UInt(ChanStats.channel.numClose);
		event["channel"]["numBytesRx"] = Json::UInt(ChanStats.channel.numBytesRx);
		event["channel"]["numBytesTx"] = Json::UInt(ChanStats.channel.numBytesTx);
		event["channel"]["numLinkFrameTx"] = Json::UInt(ChanStats.channel.numLinkFrameTx);
	}
	if (pMaster != nullptr)
	{
		auto StackStats = this->pMaster->GetStackStatistics();
		event["link"]["numBadMasterBit"] = Json::UInt(StackStats.link.numBadMasterBit);
		event["link"]["numUnexpectedFrame"] = Json::UInt(StackStats.link.numUnexpectedFrame);
		event["link"]["numUnknownDestination"] = Json::UInt(StackStats.link.numUnknownDestination);
		event["link"]["numUnknownSource"] = Json::UInt(StackStats.link.numUnknownSource);
		event["transport"]["numTransportBufferOverflow"] = Json::UInt(StackStats.transport.rx.numTransportBufferOverflow);
		event["transport"]["numTransportDiscard"] = Json::UInt(StackStats.transport.rx.numTransportDiscard);
		event["transport"]["numTransportErrorRx"] = Json::UInt(StackStats.transport.rx.numTransportErrorRx);
		event["transport"]["numTransportIgnore"] = Json::UInt(StackStats.transport.rx.numTransportIgnore);
		event["transport"]["numTransportRx"] = Json::UInt(StackStats.transport.rx.numTransportRx);
		event["transport"]["numTransportTx"] = Json::UInt(StackStats.transport.tx.numTransportTx);
	}

	return event;
}
