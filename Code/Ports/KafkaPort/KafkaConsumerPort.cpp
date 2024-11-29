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
#include "kafka/Types.h"
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaConsumer.h>
#include <chrono>
#include <memory>

//function to find all positions of a string in a string
std::vector<size_t> FindAll(const std::string& data, const std::string& toSearch)
{
	std::vector<size_t> positions;
	size_t pos = data.find(toSearch);
	while(pos != std::string::npos)
	{
		positions.push_back(pos);
		pos = data.find(toSearch, pos + toSearch.size());
	}
	return positions;
}

//function to find and replace all instances of a string in a string
void ReplaceAll(std::string& data, const std::string& toSearch, const std::string& replaceStr)
{
	size_t pos = data.find(toSearch);
	while(pos != std::string::npos)
	{
		data.replace(pos, toSearch.size(), replaceStr);
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}

void KafkaConsumerPort::Build()
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	//TODO:
	//check the translation method
	//  and in the case of template/cbor
	//    check the template/cbor structure to make sure it's valid for de-serialisation

	if(pConf->TranslationMethod == EventTranslationMethod::Template)
	{
		//turn the template into a regex pattern

		std::map<size_t,std::string> positions;
		for(const auto& place_holder : {"<EVENTTYPE>", "<EVENTTYPE_RAW>", "<INDEX>", "<TIMESTAMP>", "<DATETIME>", "<QUALITY>", "<QUALITY_RAW>", "<PAYLOAD>", "<SOURCEPORT>", "<SENDERNAME>"})
		{
			for(const auto& pos : FindAll(pConf->DefaultTemplate, place_holder))
				positions[pos] = place_holder;

			//TODO

			//TODO: handle "<POINT:...>"
		}
	}

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
	if(pConf->TranslationMethod == EventTranslationMethod::Template)
	{
		event = TemplateDeserialise(record);
	}
	else if(pConf->TranslationMethod == EventTranslationMethod::CBOR)
	{
		//TODO: Implement CBORDeserialiser
	}
	else if(pConf->TranslationMethod == EventTranslationMethod::Lua)
	{
		//TODO: Implement LuaDeserialiser
	}
	else
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Translation method {} not implemented", Name, static_cast<std::underlying_type_t<EventTranslationMethod>>(pConf->TranslationMethod));
	}

	if(!event)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to translate kafka record to EventInfo: '{}'", Name, record.toString());
		return;
	}

	PublishEvent(event);
}

std::shared_ptr<EventInfo> KafkaConsumerPort::TemplateDeserialise(const KCC::ConsumerRecord& record)
{
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	//TODO
}