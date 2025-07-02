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
#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaProducer.h>
#include <chrono>
#include <memory>
#include <string>

static std::string FillTemplate(const std::string& template_str, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields, const std::string& dt_fmt, const DataToStringMethod D2S);

void KafkaProducerPort::Build()
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	auto log = odc::spdlog_get("KafkaPort");
	std::unordered_map<std::string,std::vector<std::shared_ptr<const EventInfo>>> init_events;
	for(const auto& [tid, pte] : *pConf->pPointMap)
	{
		const auto& [source,index,ev_type] = tid;
		init_events[source].emplace_back(std::make_shared<const EventInfo>(ev_type,index,"",QualityFlags::RESTART,0));

		//Log a message for each PTM entry for verification purposes
		if(log && log->should_log(spdlog::level::trace))
		{
			auto dummy_event = std::make_shared<EventInfo>(*init_events[source].back());
			dummy_event->SetPayload();
			auto ev_str = FillTemplate(pConf->DefaultTemplate, dummy_event, "", *pte.pExtraFields, pConf->DateTimeFormat, pConf->OctetStringFormat);
			log->trace("{} -> {}: {}", source, Name, ev_str);
		}
	}
	for(const auto& [source,events] : init_events)
		pDBs[source] = std::make_unique<EventDB>(events);
}

OptionalPoint KafkaProducerPort::CheckPointTranslationMap(std::shared_ptr<const EventInfo> event, const std::string& SenderName)
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	OptionalPoint mapping;
	if(pConf->pPointMap)
	{
		std::string Source = "";
		if(pConf->PointTraslationSource == SourceLookupMethod::SenderName)
			Source = SenderName;
		else if(pConf->PointTraslationSource == SourceLookupMethod::SourcePort)
			Source = event->GetSourcePort();

		TranslationID tid(Source,event->GetIndex(),event->GetEventType());
		auto it = pConf->pPointMap->find(tid);
		if(it != pConf->pPointMap->end())
			mapping = it->second;
	}
	return mapping;
}

static std::string FillTemplate(const std::string& template_str, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields, const std::string& dt_fmt, const DataToStringMethod D2S)
{
	std::string message = template_str;
	auto find_and_replace = [&](const std::string& fnd, const auto& gen_replacement)
					{
						size_t pos = 0;
						while((pos = message.find(fnd, pos)) != std::string::npos)
						{
							auto rpl = gen_replacement(event);
							message.replace(pos, fnd.length(), rpl);
							pos += rpl.length();
						}
					};
	//Passing a generator lambda to the find_and_replace function means that the expensive string generation is only done if there is a match
	static auto EVENTTYPE = [](const std::shared_ptr<const EventInfo>& event){ return ToString(event->GetEventType()); };
	static auto INDEX = [](const std::shared_ptr<const EventInfo>& event){ return std::to_string(event->GetIndex()); };
	static auto TIMESTAMP = [](const std::shared_ptr<const EventInfo>& event){ return std::to_string(event->GetTimestamp()); };
	static auto QUALITY = [](const std::shared_ptr<const EventInfo>& event){ return ToString(event->GetQuality()); };
	static auto SOURCEPORT = [](const std::shared_ptr<const EventInfo>& event){ return event->GetSourcePort(); };
	static auto EVENTTYPE_RAW = [](const std::shared_ptr<const EventInfo>& event)
					    { return std::to_string(static_cast<std::underlying_type<EventType>::type>(event->GetEventType())); };
	static auto QUALITY_RAW = [](const std::shared_ptr<const EventInfo>& event)
					  { return std::to_string(static_cast<std::underlying_type<QualityFlags>::type>(event->GetQuality())); };

	//these aren't static because they need to capture
	auto SENDERNAME = [&](const std::shared_ptr<const EventInfo>& event){ return SenderName; };
	auto DATETIME = [&](const std::shared_ptr<const EventInfo>& event){ return odc::since_epoch_to_datetime(event->GetTimestamp(),dt_fmt); };
	auto PAYLOAD = [&](const std::shared_ptr<const EventInfo>& event){ return event->GetPayloadString(D2S); };

	find_and_replace("<EVENTTYPE>", EVENTTYPE);
	find_and_replace("<INDEX>", INDEX);
	find_and_replace("<TIMESTAMP>", TIMESTAMP);
	find_and_replace("<DATETIME>", DATETIME);
	find_and_replace("<QUALITY>", QUALITY);
	find_and_replace("<PAYLOAD>", PAYLOAD);
	find_and_replace("<SOURCEPORT>", SOURCEPORT);
	find_and_replace("<EVENTTYPE_RAW>", EVENTTYPE_RAW);
	find_and_replace("<QUALITY_RAW>", QUALITY_RAW);
	find_and_replace("<SENDERNAME>", SENDERNAME);
	for(const auto& kv : extra_fields)
		find_and_replace("<POINT:"+kv.first+">", [&](std::shared_ptr<const EventInfo> event){ return kv.second; });

	return message;
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
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to create Kafka Producer: {}", Name, e.what());
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

	auto DBkey = pConf->PointTraslationSource == SourceLookupMethod::None ? ""
	                  : pConf->PointTraslationSource == SourceLookupMethod::SenderName ? SenderName
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
	}
	else
		ultimate_event = std::make_shared<EventInfo>(*event);

	auto point_mapping = CheckPointTranslationMap(ultimate_event, SenderName);

	if(!point_mapping.has_value() && pConf->BlockUnknownPoints)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Event from unmapped point: SenderName({}), SourcePort({}), {}({})",
				Name, SenderName, ultimate_event->GetSourcePort(), ToString(ultimate_event->GetEventType()), ultimate_event->GetIndex());
		(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
		return;
	}

	//TODO: De-duplication option:
	//	could initialise and use the point database DataPort::pDB
	//	pDB->Swap() out the event and see if the quality or payload changed (optionally configurable change detection fields)
	//	then only send to kafka if it changed

	#define VAL_OR(X,D) (point_mapping.has_value() ? point_mapping->get().X ? *(point_mapping->get().X) : D : D)

	const auto& topic = VAL_OR(pTopic,pConf->DefaultTopic);
	const auto& key_buffer = VAL_OR(pKey,pConf->DefaultKey);
	const auto& extra_fields = VAL_OR(pExtraFields,pConf->DefaultExtraFields);
	if(pConf->TranslationMethod == EventTranslationMethod::Template)
	{
		const auto& template_str = VAL_OR(pTemplate,pConf->DefaultTemplate);
		auto buf = FillTemplate(template_str, ultimate_event, SenderName, extra_fields, pConf->DateTimeFormat, pConf->OctetStringFormat);
		Send(topic,key_buffer,std::move(buf),pStatusCallback);
	}
	else if(pConf->TranslationMethod == EventTranslationMethod::CBOR)
	{
		const auto& CBORer = VAL_OR(pCBORer,pConf->DefaultCBORSerialiser);
		auto buf = CBORer.Encode(ultimate_event, SenderName, extra_fields, pConf->DateTimeFormat, pConf->OctetStringFormat);
		Send(topic,key_buffer,std::move(buf),pStatusCallback);
	}
	else //Lua
	{
		//TODO: Implement Lua translation
		auto buf = std::string("Lua translation not implemented");
		Send(topic,key_buffer,std::move(buf),pStatusCallback);
	}
}

void KafkaProducerPort::Send(const kafka::Topic& topic, const OctetStringBuffer& key_buffer, const OctetStringBuffer& val_buffer, SharedStatusCallback_t pStatusCallback)
{
	KCP::ProducerRecord record(topic, kafka::Key(key_buffer.data(),key_buffer.size()), kafka::Value(val_buffer.data(), val_buffer.size()));

	auto deliveryCb = [this,key_buffer,val_buffer,pStatusCallback](const KCP::RecordMetadata& metadata, const kafka::Error& error)
				{
					auto log = odc::spdlog_get("KafkaPort");
					if (!error)
					{
						if(log && log->should_log(spdlog::level::trace))
							log->trace("{}: Message delivered: {}", Name, metadata.toString());
						(*pStatusCallback)(odc::CommandStatus::SUCCESS);
					}
					else
					{
						if(log)
							log->error("{}: Message failed to be delivered: {}", metadata.toString());
						(*pStatusCallback)(odc::CommandStatus::DOWNSTREAM_FAIL);
					}
				};

	kafka::Error err;
	pKafkaProducer->send(record, deliveryCb, err);
	if(err)
	{
		auto log = odc::spdlog_get("KafkaPort");
		if(log)
			log->error("{}: Failed to send message: {}", Name, err.toString());
		(*pStatusCallback)(odc::CommandStatus::DOWNSTREAM_FAIL);
	}

	// Poll events (e.g. message delivery callback)
	try
	{
		pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
	}
	catch (const std::exception& e)
	{
		auto log = odc::spdlog_get("KafkaPort");
		if(log)
			log->error("{}: Failed to poll events: {}", Name, e.what());
	}
}
