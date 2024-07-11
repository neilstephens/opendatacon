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
#include <kafka/KafkaProducer.h>

void KafkaProducerPort::Build()
{
	//create a kafka producer
	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

	if(pConf->NativeKafkaProperties.getProperty("enable.manual.events.poll") == "false")
		if(auto log = spdlog::get("KafkaPort"))
			log->warn("enable.manual.events.poll property is set to false, forcing to true");
	pConf->NativeKafkaProperties.put("enable.manual.events.poll", "true");

	//TODO: consider also forcing enable.idempotence=true depending on the retry model
	// see https://github.com/confluentinc/librdkafka/blob/master/INTRODUCTION.md#idempotent-producer
	//TODO: consider also setting the acks property to "all" depending on the retry model

	//TODO: use a factory function to create the producer or return an existing one
	// depending on the configuration, ports could share a producer or have their own
	pKafkaProducer = std::make_shared<KCP::KafkaProducer>(pConf->NativeKafkaProperties); // <--- TODO: check if this can throw

	//TODO: set up a polling loop using asio to call pKafkaProducer->pollEvents() at a regular interval
	// to ensure we get callbacks in the case there's no following Event
	// would belong in the producer factory/manager class if sharing producers
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
	pKafkaProducer->send(record, deliveryCb);

	// Poll events (e.g. message delivery callback)
	pKafkaProducer->pollEvents(std::chrono::milliseconds(0));
}