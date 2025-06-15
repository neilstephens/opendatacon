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
		//  or leave it to the deserialisers to throw errors/exceptions

		if(pConf->TranslationMethod == EventTranslationMethod::Template)
		{
			const auto& Template = pte.pTemplate ? *pte.pTemplate : pConf->DefaultTemplate;
			cte.pTemplateDeserialiser = std::make_unique<TemplateDeserialiser>(Template, pConf->DateTimeFormat, !pConf->RegexEscapeTemplates);
		}

		if(pConf->TranslationMethod == EventTranslationMethod::CBOR)
		{
			auto pSerialiser = pte.pCBORer ? pte.pCBORer.get() : &pConf->DefaultCBORSerialiser;
			cte.pCBORDeserialiser = std::make_unique<CBORDeserialiser>(pSerialiser, pConf->DateTimeFormat);
		}

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

	if(pConf->ShareKafkaClient)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Consumer ports cannot share a KafkaClient.", Name);
		pConf->ShareKafkaClient = false;
	}

	//find all the topics we need to subscribe to
	mTopics.insert(pConf->DefaultTopic);
	if(pConf->pPointMap)
	{
		for(const auto& [key, trans_entry] : *pConf->pPointMap)
		{
			if(trans_entry.pTopic)
				mTopics.insert(*trans_entry.pTopic);
		}
	}
}

void KafkaConsumerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(odc::CommandStatus::UNDEFINED);
		return;
	}

	std::weak_ptr<KafkaConsumerPort> weak_self = std::static_pointer_cast<KafkaConsumerPort>(shared_from_this());
	pStateSync->post([weak_self,event,pStatusCallback]()
		{
			if(auto self = weak_self.lock())
			{
				if(event->GetEventType() == EventType::ConnectState)
				{
					auto pConf = static_cast<KafkaPortConf*>(self->pConf.get());
					if(pConf->ServerType == server_type_t::ONDEMAND)
					{
						if(event->GetPayload<EventType::ConnectState>() == ConnectState::CONNECTED)
							self->PortUp();
						else if(!self->InDemand())
							self->PortDown();
					}
					(*pStatusCallback)(odc::CommandStatus::SUCCESS);
				}
				else
					(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
			}
			else
				(*pStatusCallback)(odc::CommandStatus::UNDEFINED);
		});
}

void KafkaConsumerPort::BuildConsumer()
{
	try
	{
		pKafkaConsumer = KafkaPort::Build<KCC::KafkaConsumer>("Consumer");
		if(!pKafkaConsumer)
			throw std::runtime_error("Build<KCC::KafkaConsumer> returned nullptr");
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to create Kafka Consumer: {}", Name, e.what());
		return;
	}
}

void KafkaConsumerPort::RebalanceCallback(kafka::clients::consumer::RebalanceEventType et, const kafka::TopicPartitions& tps)
{
	if(auto log = odc::spdlog_get("KafkaPort"))
	{
		const auto action_str = et == kafka::clients::consumer::RebalanceEventType::PartitionsAssigned ? "assigned" : "revoked";
		log->debug("Partitions {}: {}", action_str, kafka::toString(tps));
	}
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	if(pConf->ConsumerFastForwardOffset != 0
	   && et == kafka::clients::consumer::RebalanceEventType::PartitionsAssigned)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->info("{}: Fast forwarding consumer partitions by {} records", Name, pConf->ConsumerFastForwardOffset);
		try
		{
			//TODO: check what happens if the we seek past the end/beginning
			std::map<kafka::TopicPartition,kafka::Offset> offsets;
			if(pConf->ConsumerFastForwardOffset < 0)
				offsets = pKafkaConsumer->endOffsets(tps);
			else
				offsets = pKafkaConsumer->beginningOffsets(tps);
			for(const auto& [tp, offset] : offsets)
				pKafkaConsumer->seek(tp, offset+pConf->ConsumerFastForwardOffset);
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("{}: Failed to seek consumer partitions: {}", Name, e.what());
			return;
		}
	}
}

void KafkaConsumerPort::Subscribe()
{
	if(subscribed) return;
	try
	{
		//FIXME: this is blocking and throws on timeout!
		pKafkaConsumer->subscribe(mTopics,[this](kafka::clients::consumer::RebalanceEventType et, const kafka::TopicPartitions& tps){RebalanceCallback(et,tps);});
		subscribed = true;
		PublishEvent(ConnectState::CONNECTED);
		pKafkaConsumer->resume();
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to subscribe to topics: {}", Name, e.what());
	}
}

void KafkaConsumerPort::Unsubscribe()
{
	if(!subscribed) return;
	subscribed = false;
	PublishEvent(ConnectState::DISCONNECTED);
	try
	{
		//pausing before unsubscribing seems to avoid a crash in the kafka library
		pKafkaConsumer->pause();
		//FIXME: this is blocking and throws on timeout!
		pKafkaConsumer->unsubscribe();
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to unsubscribe from topics: {}", Name, e.what());
	}
}

//only call this on the pStateSync strand
void KafkaConsumerPort::PortUp()
{
	if(pPollTimer || pKafkaConsumer)
		return;

	BuildConsumer();
	Subscribe();

	pPollTimer = odc::asio_service::Get()->make_steady_timer();
	pIOS->post([this](){Poll(pPollTimer);});
}

//only call this on the pStateSync strand
void KafkaConsumerPort::PortDown()
{
	if(!pPollTimer || !pKafkaConsumer)
		return;

	pPollTimer->cancel();
	pPollTimer.reset();

	Unsubscribe();

	try
	{
		pKafkaConsumer->close();
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->error("{}: Failed to close Kafka consumer: {}", Name, e.what());
	}

	pKafkaConsumer.reset();
}

void KafkaConsumerPort::Poll(std::weak_ptr<asio::steady_timer> wTimer)
{
	//Polling timer chain
	//	fires immediately if events returned, otherwise exponentially back off to MaxPollIntervalms

	auto pTimer = wTimer.lock();
	if(!pTimer)
		return;

	if(!subscribed)
		Subscribe();

	std::vector<KCC::ConsumerRecord> Records;

	if(subscribed)
	{
		try
		{
			Records = pKafkaConsumer->poll(std::chrono::milliseconds::zero());
			pKafkaConsumer->commitAsync();
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("{}: Failed to poll records: {}", Name, e.what());
			return;
		}
	}

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

	pTimer->expires_from_now(poll_delay);
	pTimer->async_wait([this,wTimer](asio::error_code err)
		{
			if(!err)
				Poll(wTimer);
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

	//if the source port is not set, claim it
	if(event->GetSourcePort().empty())
		event->SetSource(Name);

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

