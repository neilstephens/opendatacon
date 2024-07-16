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
#include "KafkaPortConf.h"
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaProducer.h>
#include <chrono>
#include <memory>

void KafkaProducerPort::Build()
{
	//create a kafka producer
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	if(!pConf->NativeKafkaProperties.contains("bootstrap.servers"))
	{
		pConf->NativeKafkaProperties.put("bootstrap.servers", "localhost:9092");
		if(auto log = spdlog::get("KafkaPort"))
			log->error("bootstrap.servers property not found, defaulting to localhost:9092");
	}

	if(pConf->NativeKafkaProperties.getProperty("enable.manual.events.poll") == "false")
		if(auto log = spdlog::get("KafkaPort"))
			log->warn("enable.manual.events.poll property is set to false, forcing to true");
	pConf->NativeKafkaProperties.put("enable.manual.events.poll", "true");

	pConf->NativeKafkaProperties.put("error_cb", [this](const kafka::Error& error)
		{
			if(auto log = spdlog::get("KafkaPort"))
				log->error("{}: {}",Name,error.toString());
		});

	pConf->NativeKafkaProperties.put("log_cb", [this](int level, const char* filename, int lineno, const char* msg)
		{
			auto spdlog_lvl = spdlog::level::level_enum(6-level);
			if(auto log = spdlog::get("KafkaPort"))
				log->log(spdlog_lvl,"{} ({}:{}): {}",Name,filename,lineno,msg);
		});

	pConf->NativeKafkaProperties.put("stats_cb", [this](const std::string& jsonString)
		{
			if(auto log = spdlog::get("KafkaPort"))
				log->info("{}: Statistics: {}",Name,jsonString);
		});

	//TODO: consider also forcing enable.idempotence=true depending on the retry model
	// see https://github.com/confluentinc/librdkafka/blob/master/INTRODUCTION.md#idempotent-producer
	//TODO: consider also setting the acks property to "all" depending on the retry model

	//FIXME: KafkaProducer ctor can throw - catch it, and at least log it
	if(pConf->ShareKafkaClient)
	{
		if(pConf->SharedKafkaClientKey == "")
		{
			auto bs_servers = pConf->NativeKafkaProperties.getProperty("bootstrap.servers").value();
			pConf->SharedKafkaClientKey.append("Producer:").append(bs_servers);
		}

		pKafkaProducer = pKafkaClientCache->GetClient<KCP::KafkaProducer>(
			pConf->SharedKafkaClientKey,
			pConf->NativeKafkaProperties);
	}
	else
		pKafkaProducer = std::make_shared<KCP::KafkaProducer>(pConf->NativeKafkaProperties);

	//TODO: set up a polling loop using asio to call pKafkaProducer->pollEvents() at a regular interval
	// to ensure we get callbacks in the case there's no following Event
	// would belong in the producer factory/manager class if sharing producers
	std::shared_ptr<asio::steady_timer> pTimer = odc::asio_service::Get()->make_steady_timer(std::chrono::milliseconds(pConf->MaxPollIntervalms));
	std::weak_ptr<KafkaProducerPort> weak_self = std::static_pointer_cast<KafkaProducerPort>(this->shared_from_this());
	auto polling_handler = [weak_self,pTimer,pConf](auto timer_handler)
				     {
					     auto self = weak_self.lock();
					     if(!self || !self->enabled) return;
					     self->pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
					     pTimer->expires_from_now(std::chrono::milliseconds(pConf->MaxPollIntervalms));
					     pTimer->async_wait([timer_handler](asio::error_code err)
						     {
							     if(err) return;
							     timer_handler(timer_handler);
						     });
				     };
	polling_handler(polling_handler);
}

void KafkaProducerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled) return;

	//TODO: build the KCP::ProducerRecord from the EventInfo
	KCP::ProducerRecord record(kafka::Topic("example-topic"), kafka::NullKey, kafka::Value("test"));

	auto deliveryCb = [](const KCP::RecordMetadata& metadata, const kafka::Error& error)
				{
					auto log = spdlog::get("KafkaPort");
					if (!error)
					{
						if(log && log->should_log(spdlog::level::trace))
							log->trace("Message delivered: {}", metadata.toString());
					}
					else
					{
						if(log)
							log->error("Message failed to be delivered: {}", error.message());
						//TODO: retry?
					}
				};

	//TODO: check if any of the pKafkaProducer calls can throw

	// Send a message
	// Use the non-blocking option, and the overload that catches exceptions
	pKafkaProducer->send(record, deliveryCb);

	// Poll events (e.g. message delivery callback)
	pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
}