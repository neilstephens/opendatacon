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
 * KafkaProducerPort.cpp
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#include "KafkaProducerPort.h"
#include "KafkaPort.h"
#include "KafkaPortConf.h"
#include "opendatacon/util.h"
#include <opendatacon/TranslationEngine.h>
#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaProducer.h>
#include <chrono>
#include <memory>
#include <string>

void KafkaProducerPort::Build()
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	//resolve keys and build the serialisers
	try
	{
		static const PointTranslationMap empty_ptm;
		pConf->pEngine = TranslationEngine::Create(pConf->Translation, pConf->pPointMap ? *pConf->pPointMap : empty_ptm, TranslationEngine::Direction::Forward);
	}
	catch(const std::exception& e)
	{
		Log.Error("{}: Failed to build the event translation: {}", Name, e.what());
		throw;
	}

	std::unordered_map<std::string,std::vector<std::shared_ptr<const EventInfo>>> init_events;
	if(pConf->pPointMap)
	{
		for(const auto& [tid, pte] : *pConf->pPointMap)
		{
			const auto& [source,index,ev_type] = tid;
			init_events[source].emplace_back(std::make_shared<const EventInfo>(ev_type,index,"",QualityFlags::RESTART));

			//Log a message for each PTM entry for verification purposes
			if(Log.ShouldLog(spdlog::level::trace))
			{
				auto dummy_event = std::make_shared<EventInfo>(*init_events[source].back());
				dummy_event->SetPayload();
				try
				{
					auto msg = pConf->pEngine->Serialise(dummy_event, "");
					Log.Trace("{} -> {}: {} {}: {}", source, Name, msg.Channel, std::string(msg.Key.begin(),msg.Key.end()), std::string(msg.Value.begin(),msg.Value.end()));
				}
				catch(const std::exception& e)
				{
					Log.Trace("{} -> {}: Failed to serialise: {}", source, Name, e.what());
				}
			}
		}
	}
	for(const auto& [source,events] : init_events)
		pDBs[source] = std::make_unique<EventDB>(events);
}

//only call this on the pStateSync strand
void KafkaProducerPort::PortUp()
{
	if(pKafkaProducer) return;
	PublishEvent(ConnectState::CONNECTED);
	try
	{
		pKafkaProducer = KafkaPort::Build<KCP::KafkaProducer>("Producer");
		if(!pKafkaProducer)
			throw std::runtime_error(Name+": Failed to build KafkaProducer.");
	}
	catch(const std::exception& e)
	{
		Log.Error("{}: Failed to create Kafka Producer: {}", Name, e.what());
		return;
	}
}

//only call this on the pStateSync strand
void KafkaProducerPort::PortDown()
{
	if(!pKafkaProducer) return;
	PublishEvent(ConnectState::DISCONNECTED);
	pKafkaProducer.reset();
}


void KafkaProducerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(odc::CommandStatus::UNDEFINED);
		return;
	}

	if(event->GetEventType() == EventType::ConnectState)
	{
		ConnectionEvent(event,pStatusCallback);
		return;
	}

	if(!pKafkaProducer)
	{
		(*pStatusCallback)(odc::CommandStatus::UNDEFINED);
		return;
	}

	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	std::shared_ptr<EventInfo> ultimate_event;

	auto DBkey = pConf->Translation.PointTranslationSource == SourceLookupMethod::None ? ""
	                  : pConf->Translation.PointTranslationSource == SourceLookupMethod::SenderName ? SenderName
	                        : event->GetSourcePort();
	auto pDB_it = pDBs.find(DBkey);

	if(pDB_it != pDBs.end())
	{
		switch(event->GetEventType())
		{
			case EventType::BinaryQuality:
			{
				auto old_event = pDB_it->second->Get(EventType::Binary,event->GetIndex());
				ultimate_event = std::make_shared<EventInfo>(old_event ? *old_event : EventInfo(EventType::Binary,event->GetIndex()));
				ultimate_event->SetQuality(event->GetPayload<EventType::BinaryQuality>());
				break;
			}
			case EventType::AnalogQuality:
			{
				auto old_event = pDB_it->second->Get(EventType::Analog,event->GetIndex());
				ultimate_event = std::make_shared<EventInfo>(old_event ? *old_event : EventInfo(EventType::Analog,event->GetIndex()));
				ultimate_event->SetQuality(event->GetPayload<EventType::AnalogQuality>());
				break;
			}
			case EventType::AnalogOutputStatusQuality:
			{
				auto old_event = pDB_it->second->Get(EventType::AnalogOutputStatus,event->GetIndex());
				ultimate_event = std::make_shared<EventInfo>(old_event ? *old_event : EventInfo(EventType::AnalogOutputStatus,event->GetIndex()));
				ultimate_event->SetQuality(event->GetPayload<EventType::AnalogOutputStatusQuality>());
				break;
			}
			case EventType::BinaryOutputStatusQuality:
			{
				auto old_event = pDB_it->second->Get(EventType::BinaryOutputStatus,event->GetIndex());
				ultimate_event = std::make_shared<EventInfo>(old_event ? *old_event : EventInfo(EventType::BinaryOutputStatus,event->GetIndex()));
				ultimate_event->SetQuality(event->GetPayload<EventType::BinaryOutputStatusQuality>());
				break;
			}
			case EventType::OctetStringQuality:
			{
				auto old_event = pDB_it->second->Get(EventType::OctetString,event->GetIndex());
				ultimate_event = std::make_shared<EventInfo>(old_event ? *old_event : EventInfo(EventType::OctetString,event->GetIndex()));
				ultimate_event->SetQuality(event->GetPayload<EventType::OctetStringQuality>());
				break;
			}
			default:
				ultimate_event = std::make_shared<EventInfo>(*event);
				break;
		}
		auto prev_event = pDB_it->second->Swap(ultimate_event);
		//TODO: provide more options for de-duplication. For example,
		//	the current logic compares timestamp, so an otherwise identical payload/quality isn't a duplicate
		//	if timestamp is the only exception, maybe overload the spaceship operator...
		if(pConf->DeduplicateEvents && prev_event && *ultimate_event == *prev_event)
		{
			(*pStatusCallback)(odc::CommandStatus::SUCCESS);
			return;
		}
	}
	else
		ultimate_event = std::make_shared<EventInfo>(*event);

	if(!ultimate_event->HasPayload())
	{
		// if the point isn't ONLINE or if it's RESTART qual, then it's OK to not have a payload,
		//	but we'll default it to avoid serialisation issues
		if((ultimate_event->GetQuality() & QualityFlags::ONLINE) == QualityFlags::NONE
		   || (ultimate_event->GetQuality() & QualityFlags::RESTART) != QualityFlags::NONE)
			ultimate_event->SetPayload();
		else //it doesn't have a payload when it should (probably), so we'll warn and drop it
		{
			Log.Warn("{}: Event from {}(Source: {}) does not have a payload: {}({}) Quality({})",
				Name, SenderName, ultimate_event->GetSourcePort(), ToString(ultimate_event->GetEventType()), ultimate_event->GetIndex(), ToString(ultimate_event->GetQuality()));
			(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
			return;
		}
	}

	if(pConf->BlockUnknownPoints && !pConf->pEngine->IsMapped(ultimate_event, SenderName))
	{
		Log.Warn("{}: Event from unmapped point: SenderName({}), SourcePort({}), {}({})",
			Name, SenderName, ultimate_event->GetSourcePort(), ToString(ultimate_event->GetEventType()), ultimate_event->GetIndex());
		(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
		return;
	}

	TranslationEngine::Message msg;
	try
	{
		msg = pConf->pEngine->Serialise(ultimate_event, SenderName);
	}
	catch(const std::exception& e)
	{
		Log.Error("{}: Failed to serialise event from {}(Source: {}): {}({}): {}",
			Name, SenderName, ultimate_event->GetSourcePort(), ToString(ultimate_event->GetEventType()), ultimate_event->GetIndex(), e.what());
		(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
		return;
	}
	Send(msg.Channel, OctetStringBuffer(std::move(msg.Key)), OctetStringBuffer(std::move(msg.Value)), pStatusCallback);
}

void KafkaProducerPort::Send(const kafka::Topic& topic, const OctetStringBuffer& key_buffer, const OctetStringBuffer& val_buffer, SharedStatusCallback_t pStatusCallback)
{
	KCP::ProducerRecord record(topic, kafka::Key(key_buffer.data(),key_buffer.size()), kafka::Value(val_buffer.data(), val_buffer.size()));

	auto deliveryCb = [this,key_buffer,val_buffer,pStatusCallback](const KCP::RecordMetadata& metadata, const kafka::Error& error)
				{
					if (!error)
					{
						if(Log.ShouldLog(spdlog::level::trace))
							Log.Trace("{}: Message delivered: {}", Name, metadata.toString());
						(*pStatusCallback)(odc::CommandStatus::SUCCESS);
					}
					else
					{
						Log.Error("{}: Message failed to be delivered: {}: {}", Name, metadata.toString(), error.toString());
						(*pStatusCallback)(odc::CommandStatus::DOWNSTREAM_FAIL);
					}
				};

	kafka::Error err;
	pKafkaProducer->send(record, deliveryCb, err);
	if(err)
	{
		Log.Error("{}: Failed to send message: {}", Name, err.toString());
		(*pStatusCallback)(odc::CommandStatus::DOWNSTREAM_FAIL);
	}

	// Poll events (e.g. message delivery callback)
	try
	{
		pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
	}
	catch (const std::exception& e)
	{
		Log.Error("{}: Failed to poll events: {}", Name, e.what());
	}
}
