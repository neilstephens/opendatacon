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
#include <opendatacon/EventConversion.h>
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
	pChanH.reset();
}

void DNP3MasterPort::Enable()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if(enabled)
		return;
	if(nullptr == pMaster)
	{
		Log.Error("{}: DNP3 stack not initialised.", Name);
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
	if(pConf->OnDemand
	   && pConf->pPointConf->CommsPointRideThroughTimems > 0
	   && pConf->pPointConf->CommsPointRideThroughDemandPause)
		pCommsRideThroughTimer->Pause();

	CheckStackState();

	PublishEvent(ConnectState::PORT_UP);
}
void DNP3MasterPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	pCommsRideThroughTimer->StopHeartBeat();
	pCommsRideThroughTimer->FastForward();

	CheckStackState();

	PublishEvent(ConnectState::PORT_DOWN);
}

void DNP3MasterPort::PortUp()
{
	Log.Debug("{}: PortUp() called.", Name);

	//cancel the timer if we were riding through
	pCommsRideThroughTimer->Cancel();
}

void DNP3MasterPort::PortDown()
{
	Log.Debug("{}: PortDown() called.", Name);

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
		Log.Debug("{}: Updating comms point ({}).", Name, isFailed ? "failed" : "good");

		auto commsEvent = std::make_shared<EventInfo>(EventType::Binary, pConf->pPointConf->mCommsPoint.second, Name);
		auto failed_val = pConf->pPointConf->mCommsPoint.first.value;
		commsEvent->SetPayload<EventType::Binary>(isFailed ? failed_val : !failed_val);
		PublishEvent(commsEvent);
		pDB->Set(commsEvent);
	}
}

void DNP3MasterPort::RePublishEvents()
{
	Log.Debug("{}: Re-publishing events.", Name);
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
}

void DNP3MasterPort::SetCommsFailed()
{
	UpdateCommsPoint(true);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->pPointConf->SetQualityOnLinkStatus)
	{
		Log.Debug("{}: Setting {}, clearing {}, point quality.", Name, ToString(pConf->pPointConf->FlagsToSetOnLinkStatus), ToString(pConf->pPointConf->FlagsToClearOnLinkStatus));

		SetCommsFailedQuality<EventType::Binary, EventType::BinaryQuality>(pConf->pPointConf->BinaryIndexes);
		SetCommsFailedQuality<EventType::Analog, EventType::AnalogQuality>(pConf->pPointConf->AnalogIndexes);
		SetCommsFailedQuality<EventType::AnalogOutputStatus, EventType::AnalogOutputStatusQuality>(pConf->pPointConf->AnalogOutputStatusIndexes);
		SetCommsFailedQuality<EventType::BinaryOutputStatus, EventType::BinaryOutputStatusQuality>(pConf->pPointConf->BinaryOutputStatusIndexes);
		SetCommsFailedQuality<EventType::OctetString, EventType::OctetStringQuality>(pConf->pPointConf->OctetStringIndexes);

		// An integrity scan will be needed when/if the link comes back
		// It's the only way to get the true state upstream
		// Can't just reset quality, because it would make new events for old values
		pChanH->Post([this]()
			{
				Log.Debug("{}: Setting IntegrityScanNeeded for SetQualityOnLinkStatus.",Name);
				IntegrityScanNeeded = true;
			});
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

	//just in case
	CheckStackState();
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up / channel down)
void DNP3MasterPort::OnStateChange(opendnp3::LinkStatus status)
{
	Log.Debug("{}: LinkStatus {}.", Name, opendnp3::LinkStatusSpec::to_human_string(status));
	pChanH->SetLinkStatus(status);
	//TODO: track a statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3MasterPort::OnKeepAliveFailure()
{
	Log.Debug("{}: KeepAliveFailure() called.", Name);
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
			Log.Debug("{}: Setting IntegrityScanNeeded for Link-up.",Name);
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

		IntegrityScanDone = false;
		return;
	}
}

// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3MasterPort::OnKeepAliveSuccess()
{
	Log.Debug("{}: KeepAliveSuccess() called.", Name);
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
	if(Log.ShouldLog(spdlog::level::trace))
		Log.Trace("{}: OnReceiveIIN(MSB {}, LSB {}) called.", Name, iin.MSB, iin.LSB);
	pChanH->LinkUp();
	pChanH->Post([this,iin]()
		{
			if(pChanH->GetLinkDeadness() == LinkDeadness::LinkUpChannelUp)
			{
				auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
				if((iin.IsSet(opendnp3::IINBit::EVENT_BUFFER_OVERFLOW) && pConf->pPointConf->IntegrityOnEventOverflowIIN) ||
				   (iin.IsSet(opendnp3::IINBit::DEVICE_RESTART) && pConf->pPointConf->IgnoreRestartIIN == false))
				{
					Log.Debug("{}: Stack executed IIN triggered integrity scan for this link session.",Name);
					IntegrityScanDone = true;
				}
				if(IntegrityScanNeeded)
				{
					IntegrityScanNeeded = false;
					if(IntegrityScanDone)
					{
						Log.Debug("{}: Skipping startup integrity scan (stack already did one).",Name);
						return;
					}
					Log.Debug("{}: Executing startup integrity scan.",Name);
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
		Log.Error("{}: Channel not found for masterstation.", Name);
		return;
	}

	InitEventDB();

	opendnp3::MasterStackConfig StackConfig;
	opendnp3::LinkConfig link(true);

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
	StackConfig.master.disableUnsolOnStartup = pConf->pPointConf->DisableUnsolOnStartup;
	StackConfig.master.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();

	//set the internal sizes of the ADPU buffers - we make it symetric by using MaxTxFragSize for both
	//	but maybe we should have separate setting???
	StackConfig.master.maxTxFragSize = pConf->pPointConf->MaxTxFragSize;
	StackConfig.master.maxRxFragSize = pConf->pPointConf->MaxTxFragSize;

	StackConfig.master.eventScanOnEventsAvailableClassMask = pConf->pPointConf->GetEventScanOnEventsAvailableClassMask();

	if(pConf->pPointConf->LinkUpIntegrityTrigger == DNP3PointConf::LinkUpIntegrityTrigger_t::NEVER)
		StackConfig.master.startupIntegrityClassMask = opendnp3::ClassField::None();
	else
		StackConfig.master.startupIntegrityClassMask = pConf->pPointConf->GetStartupIntegrityClassMask();

	//Configure mandatory integrity scans separately to the startup scan
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
		Log.Error("{}: Error creating masterstation.", Name);
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
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas){ LoadT(meas); }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent> >& meas){ /*LoadT(meas);*/ }
void DNP3MasterPort::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent> >& meas){ /*LoadT(meas);*/ }

template<typename T>
inline void DNP3MasterPort::LoadT(const opendnp3::ICollection<opendnp3::Indexed<T> >& meas)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	meas.ForeachItem([this,pConf](const opendnp3::Indexed<T>&pair)
		{
			auto TSO = pConf->pPointConf->TimestampOverride;
			auto event = ToODC(pair.value, pair.index, Name);
			if constexpr(!std::is_same<decltype(pair.value),opendnp3::OctetString>()) //OctetString has no time
				if (TSO == DNP3PointConf::TimestampOverride_t::ALWAYS
				    || (TSO == DNP3PointConf::TimestampOverride_t::ZERO && pair.value.time.value == 0))
					event->SetTimestamp();

			bool unknown_point = !pDB->Set(event);
			bool publish = true;
			if(unknown_point)
			{
				auto log_level = spdlog::level::err;
				if(pConf->pPointConf->AllowUnknownIndexes)
					log_level = spdlog::level::warn;
				else
					publish = false;

				if(auto log = Log.GetLog())
					log->log(log_level, "{}: Received {} from stack for unconfigured index ({})",
						Name, ToString(event->GetEventType()), event->GetIndex());
			}

			//log an error if we get a binary that clashes with the index of our comms point
			if(pConf->pPointConf->mCommsPoint.first.flags.IsSet(opendnp3::BinaryQuality::ONLINE)
			   && event->GetEventType() == EventType::Binary
			   && event->GetIndex() == pConf->pPointConf->mCommsPoint.second)
			{
				publish = false;
				Log.Error("{}: Received Binary that clashes with comms point index ({})", Name, event->GetIndex());
			}

			if(publish)
				PublishEvent(event);
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
		auto state = event->GetPayload<EventType::ConnectState>();

		if(state == ConnectState::PORT_UP)
		{
			Log.Info("{}: Upstream port enabled.", Name);
			RePublishEvents();
		}

		CheckStackState();

		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}

	auto index = event->GetIndex();
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

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

	switch(event->GetEventType())
	{
		case odc::EventType::ControlRelayOutputBlock:
			if(std::find(pConf->pPointConf->ControlIndexes.begin(), pConf->pPointConf->ControlIndexes.end(), index) != pConf->pPointConf->ControlIndexes.end())
			{
				Log.Debug("{}: Executing direct operate to index: {}", Name, index);

				auto lCommand = FromODC<opendnp3::ControlRelayOutputBlock>(event);
				DoOverrideControlCode(lCommand);
				this->pMaster->DirectOperate(lCommand, index, DNP3Callback);
				return;
			}
			Log.Warn("{}: CROB Control sent to invalid DNP3 index: {}", Name, index);
			break;
		case odc::EventType::AnalogOutputInt16:
		case odc::EventType::AnalogOutputInt32:
		case odc::EventType::AnalogOutputFloat32:
		case odc::EventType::AnalogOutputDouble64:
			if(std::find(pConf->pPointConf->AnalogControlIndexes.begin(), pConf->pPointConf->AnalogControlIndexes.end(), index) != pConf->pPointConf->AnalogControlIndexes.end())
			{
				Log.Debug("{}: Executing analog control to index: {}", Name, index);

				// Here we may get a 16 bit event, but the master station may be configured to send a 32 bit command, for example
				// We need to convert the event to the correct type
				auto new_event_type = pConf->pPointConf->AnalogControlTypes[index];
				try
				{
					auto newevent = ConvertEvent(event, new_event_type);
					switch(new_event_type)
					{
						case EventType::AnalogOutputInt16:
							this->pMaster->DirectOperate(FromODC<EventType::AnalogOutputInt16>(newevent), index, DNP3Callback);
							break;
						case EventType::AnalogOutputInt32:
							this->pMaster->DirectOperate(FromODC<EventType::AnalogOutputInt32>(newevent), index, DNP3Callback);
							break;
						case EventType::AnalogOutputFloat32:
							this->pMaster->DirectOperate(FromODC<EventType::AnalogOutputFloat32>(newevent), index, DNP3Callback);
							break;
						case EventType::AnalogOutputDouble64:
							this->pMaster->DirectOperate(FromODC<EventType::AnalogOutputDouble64>(newevent), index, DNP3Callback);
							break;
						default:
							(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
					}
				}
				catch(const std::exception& e)
				{
					Log.Error("{}: Error converting event for analog control: {}", Name, e.what());
					(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
				}
				return;
			}
			Log.Warn("{}: Analog Control sent to invalid DNP3 index: {}", Name, index);
			break;
		default:
			break;
	}
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
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
