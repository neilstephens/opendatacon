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
 * KafkaConsumerPort.cpp
 *
 *  Created on: 26/07/2024
 *      Author: Neil Stephens
 */

#include "KafkaConsumerPort.h"
#include "KafkaPort.h"
#include "TemplateDeserialiser.h"
#include "CBORDeserialiser.h"
#include "LuaDeserialiser.h"
#include "kafka/Types.h"
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaConsumer.h>
#include <chrono>
#include <memory>
#include <cstddef>

void KafkaConsumerPort::Build()
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	//iterate over the PointTranslationMap and build a ConsumerTranslationMap
	ConsumerTranslationMap CTM;
	for(const auto& [tid, pte] : *pConf->pPointMap)
	{
		auto key_buffer = pte.pKey ? *pte.pKey : pConf->DefaultKey;
		ConsumptionID cid(pte.pTopic ? *pte.pTopic : pConf->DefaultTopic, kafka::Key(key_buffer.data(),key_buffer.size()));
		ConsumerTranslationEntry cte;

		//TODO: check the template/cbor structure to make sure it's valid for de-serialisation
		//  or leave it to the deserialisers to throw exceptions

		if(pte.pTemplate)
			cte.pTemplateDeserialiser = std::make_unique<TemplateDeserialiser>(*pte.pTemplate, pConf->DateTimeFormat);

		if(pte.pCBORer)
			cte.pCBORDeserialiser = std::make_unique<CBORDeserialiser>(/*FIXME*/ "", pConf->DateTimeFormat);

		if(pConf->TranslationMethod == EventTranslationMethod::Lua)
			cte.pLuaDeserialiser = std::make_unique<LuaDeserialiser>(/*FIXME*/ "","", pConf->DateTimeFormat);

		CTM.emplace(std::move(cid), std::move(cte));
	}
	pConf->pKafkaMap = std::make_unique<ConsumerTranslationMap>(std::move(CTM));

	if(!pConf->NativeKafkaProperties.contains("client.id"))
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Consumer client.id is not set in the properties, this may cause issues when reloading/restarting (can't resume from the same offset)", Name);
	}

	pKafkaConsumer = KafkaPort::Build<KCC::KafkaConsumer>("Consumer");
	if(!pKafkaConsumer)
		throw std::runtime_error(Name+": Failed to create Kafka Consumer");

	//subscribe to topics
	mTopics.insert(pConf->DefaultTopic);
	if(pConf->pPointMap)
	{
		for(const auto& [key, trans_entry] : *pConf->pPointMap)
		{
			if(trans_entry.pTopic)
				mTopics.insert(*trans_entry.pTopic);
		}
	}
	pKafkaConsumer->subscribe(mTopics);

	pKafkaConsumer->pause(); //pause on start, resume on Enable()
	Poll();                  //Need to poll whether paused or not
}

void KafkaConsumerPort::Enable()
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	if(pConf->ConsumerFastForwardOffset != 0)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->info("{}: Fast forwarding consumer by {} records", Name, pConf->ConsumerFastForwardOffset);

		//TODO: check what happens if the we seek past the end/beginning

		std::map<kafka::TopicPartition,kafka::Offset> offsets;
		if(pConf->ConsumerFastForwardOffset < 0)
			pKafkaConsumer->endOffsets(pKafkaConsumer->assignment());
		else
			pKafkaConsumer->beginningOffsets(pKafkaConsumer->assignment());

		for(const auto& [tp, offset] : offsets)
			pKafkaConsumer->seek(tp, offset+pConf->ConsumerFastForwardOffset);
	}
	pKafkaConsumer->resume();
}

void KafkaConsumerPort::Disable()
{
	pKafkaConsumer->pause();
}

void KafkaConsumerPort::Poll()
{
	//poll() should be called even when paused (to handle rebalances, etc)
	//  only bail out if we're being destroyed (which resets pPollTimer)
	if(!pPollTimer)
		return;

	//Polling timer chain
	//	fires immediately if events returned, otherwise exponentially back off to MaxPollIntervalms
	auto Records = pKafkaConsumer->poll(std::chrono::milliseconds::zero());
	pKafkaConsumer->commitAsync();

	auto poll_delay = std::chrono::milliseconds(PollBackoff_ms);
	if(Records.size() == 0)
	{
		PollBackoff_ms *= 2;
		auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
		if(PollBackoff_ms > pConf->MaxPollIntervalms)
			PollBackoff_ms = pConf->MaxPollIntervalms;
	}
	else
	{
		poll_delay = std::chrono::milliseconds(0);
		PollBackoff_ms = 1;
	}

	pPollTimer->expires_from_now(poll_delay);
	pPollTimer->async_wait([this](asio::error_code err)
		{
			if(!err)
				Poll();
		});

	for(const auto& record : Records)
		ProcessRecord(record);
}

void KafkaConsumerPort::ProcessRecord(const KCC::ConsumerRecord& record)
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	std::shared_ptr<EventInfo> event = nullptr;

	//build a consumer key and look up the deserialiser
	ConsumptionID cid(record.topic(), kafka::Key(record.key().data(),record.key().size()));
	auto it = pConf->pKafkaMap->find(cid);
	if(it != pConf->pKafkaMap->end())
	{
		if(pConf->TranslationMethod == EventTranslationMethod::Template)
			event = TemplateDeserialise(record,it->second.pTemplateDeserialiser);
		else if(pConf->TranslationMethod == EventTranslationMethod::CBOR)
			event = CBORDeserialise(record,it->second.pCBORDeserialiser);
		else if(pConf->TranslationMethod == EventTranslationMethod::Lua)
			event = LuaDeserialise(record,it->second.pLuaDeserialiser);
		else
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("{}: Translation method {} not implemented", Name, static_cast<std::underlying_type_t<EventTranslationMethod>>(pConf->TranslationMethod));
		}
	}
	else
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: No deserialiser found for topic '{}' and key '{}'", Name, record.topic(), record.key().toString());
	}

	if(!event)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to translate kafka record to EventInfo: '{}'", Name, record.toString());
		return;
	}

	PublishEvent(event);
}

std::shared_ptr<EventInfo> KafkaConsumerPort::TemplateDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<TemplateDeserialiser>& pTemplateDeserialiser)
{
	if(!pTemplateDeserialiser)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Null template deserialiser found for topic '{}' and key '{}'", Name, record.topic(), record.key().toString());
		return nullptr;
	}
	return pTemplateDeserialiser->Deserialise(record);
}

std::shared_ptr<EventInfo> KafkaConsumerPort::CBORDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<CBORDeserialiser>& pCBORDeserialiser)
{
	if(!pCBORDeserialiser)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Null CBOR deserialiser found for topic '{}' and key '{}'", Name, record.topic(), record.key().toString());
		return nullptr;
	}
	return pCBORDeserialiser->Deserialise(record);
}

std::shared_ptr<EventInfo> KafkaConsumerPort::LuaDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<LuaDeserialiser>& pLuaDeserialiser)
{
	if(!pLuaDeserialiser)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->warn("{}: Null Lua deserialiser found for topic '{}' and key '{}'", Name, record.topic(), record.key().toString());
		return nullptr;
	}
	return pLuaDeserialiser->Deserialise(record);
}