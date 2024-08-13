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

void KafkaProducerPort::Build()
{
	pKafkaProducer = KafkaPort::Build<KCP::KafkaProducer>("Producer");
	if(!pKafkaProducer)
		throw std::runtime_error(Name+": Failed to build KafkaProducer.");
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

static std::string FillTemplate(const std::string& template_str, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const std::unordered_map<std::string, std::string>& extra_fields)
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
	static auto DATETIME = [](const std::shared_ptr<const EventInfo>& event){ return odc::since_epoch_to_datetime(event->GetTimestamp()); };
	static auto QUALITY = [](const std::shared_ptr<const EventInfo>& event){ return ToString(event->GetQuality()); };
	static auto PAYLOAD = [](const std::shared_ptr<const EventInfo>& event){ return event->GetPayloadString(); };
	static auto SOURCEPORT = [](const std::shared_ptr<const EventInfo>& event){ return event->GetSourcePort(); };
	static auto EVENTTYPE_RAW = [](const std::shared_ptr<const EventInfo>& event)
					    { return std::to_string(static_cast<std::underlying_type<EventType>::type>(event->GetEventType())); };
	static auto QUALITY_RAW = [](const std::shared_ptr<const EventInfo>& event)
					  { return std::to_string(static_cast<std::underlying_type<QualityFlags>::type>(event->GetQuality())); };
	auto SENDERNAME = [&](const std::shared_ptr<const EventInfo>& event){ return SenderName; };

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

void KafkaProducerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled) return;
	if(!pKafkaProducer) return;

	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	auto point_mapping = CheckPointTranslationMap(event, SenderName);

	if(!point_mapping.has_value() && pConf->BlockUnknownPoints)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Event from unmapped point: SenderName({}), SourcePort({}), {}({})",
				Name, SenderName, event->GetSourcePort(), ToString(event->GetEventType()), event->GetIndex());
		(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
		return;
	}

	#define VAL_OR(X,D) (point_mapping.has_value() ? point_mapping->get().X ? *(point_mapping->get().X) : D : D)

	const auto& topic = VAL_OR(pTopic,pConf->DefaultTopic);
	const auto& key_buffer = VAL_OR(pKey,pConf->DefaultKey);
	OctetStringBuffer val_buffer;
	if(pConf->TranslationMethod == EventTranslationMethod::Template)
	{
		const auto& template_str = VAL_OR(pTemplate,pConf->DefaultTemplate);
		const auto& extra_fields = VAL_OR(pExtraFields,pConf->DefaultExtraFields);
		val_buffer = FillTemplate(template_str, event, SenderName, extra_fields);
	}
	else if(pConf->TranslationMethod == EventTranslationMethod::CBOR)
	{
		//TODO: Implement CBOR translation
		val_buffer = std::string("CBOR translation not implemented");
	}
	else //Lua
	{
		//TODO: Implement Lua translation
		val_buffer = std::string("Lua translation not implemented");
	}
	KCP::ProducerRecord record(topic, kafka::Key(key_buffer.data(),key_buffer.size()), kafka::Value(val_buffer.data(), val_buffer.size()));

	auto deliveryCb = [this,event,key_buffer,val_buffer,pStatusCallback](const KCP::RecordMetadata& metadata, const kafka::Error& error)
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
							log->error("{}: Message failed to be delivered: {} {} {}: {}",
								ToString(event->GetEventType()),event->GetIndex(),
								event->GetPayloadString(), error.message());
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
	pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
}