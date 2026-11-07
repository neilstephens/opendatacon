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
#include <opendatacon/TranslationEngine.h>
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

	//resolve keys and build the deserialisers
	try
	{
		static const PointTranslationMap empty_ptm;
		pConf->pEngine = TranslationEngine::Create(pConf->Translation, pConf->pPointMap ? *pConf->pPointMap : empty_ptm, TranslationEngine::Direction::Reverse);
	}
	catch(const std::exception& e)
	{
		Log.Error("{}: Failed to build the event translation: {}", Name, e.what());
		throw;
	}

	if(!pConf->NativeKafkaProperties.contains("client.id"))
	{
		Log.Warn("{}: Consumer client.id is not set in the properties, this may cause issues when reloading/restarting (can't resume from the same offset)", Name);
	}

	if(pConf->ShareKafkaClient)
	{
		Log.Error("{}: Consumer ports cannot share a KafkaClient.", Name);
		pConf->ShareKafkaClient = false;
	}

	//find all the topics we need to subscribe to
	mTopics = pConf->pEngine->Channels();
}

void KafkaConsumerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
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

	(*pStatusCallback)(odc::CommandStatus::NOT_SUPPORTED);
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
		Log.Error("{}: Failed to create Kafka Consumer: {}", Name, e.what());
		return;
	}
}

void KafkaConsumerPort::RebalanceCallback(kafka::clients::consumer::RebalanceEventType et, const kafka::TopicPartitions& tps)
{
	Log.Debug("Partitions {}: {}", et == kafka::clients::consumer::RebalanceEventType::PartitionsAssigned ? "assigned" : "revoked", kafka::toString(tps));
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	if(pConf->ConsumerFastForwardOffset != 0
	   && et == kafka::clients::consumer::RebalanceEventType::PartitionsAssigned)
	{
		Log.Info("{}: Fast forwarding consumer partitions by {} records", Name, pConf->ConsumerFastForwardOffset);
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
			Log.Error("{}: Failed to seek consumer partitions: {}", Name, e.what());
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
		Log.Error("{}: Failed to subscribe to topics: {}", Name, e.what());
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

	pPollTimer.reset();
	pKafkaConsumer.reset();
	subscribed = false;
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
		}
		catch(const std::exception& e)
		{
			Log.Error("{}: Failed to poll records: {}", Name, e.what());
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
		if(Log.ShouldLog(spdlog::level::trace))
			Log.Trace("{}: Polled {} records from Kafka", Name, Records.size());

		try
		{
			//TODO: provide callback with captured sequence number
			//	the callback can retry the commit if the sequence number hasn't moved on
			pKafkaConsumer->commitAsync();
		}
		catch(const std::exception& e)
		{
			Log.Error("{}: Failed to async commit offsets for {} records: {}", Name, Records.size(), e.what());
		}

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

	//the engine looks up the deserialiser using the topic and key
	bool matched = false;
	std::shared_ptr<EventInfo> event = pConf->pEngine->Deserialise(
		record.topic(),
		static_cast<const uint8_t*>(record.key().data()), record.key().size(),
		static_cast<const uint8_t*>(record.value().data()), record.value().size(),
		&matched);

	if(!matched)
		Log.Warn("{}: No deserialiser found for topic '{}' and key '{}'", Name, record.topic(), record.key().toString());

	if(!event)
	{
		Log.Error("{}: Failed to translate kafka record to EventInfo: '{}'", Name, record.toString());
		return;
	}

	//if the source port is not set, claim it
	if(event->GetSourcePort().empty())
		event->SetSource(Name);

	PublishEvent(event);
}
