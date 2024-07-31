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
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaProducer.h>
#include <chrono>
#include <memory>

void KafkaProducerPort::Build()
{
	pKafkaProducer = KafkaPort::Build<KCP::KafkaProducer>("Producer");
	if(!pKafkaProducer)
		throw std::runtime_error(Name+": Failed to build KafkaProducer.");
}

void KafkaProducerPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled) return;
	if(!pKafkaProducer) return;

	//TODO: build the KCP::ProducerRecord from the EventInfo
	KCP::ProducerRecord record(kafka::Topic("example-topic"), kafka::NullKey, kafka::Value("test"));

	auto deliveryCb = [this,event](const KCP::RecordMetadata& metadata, const kafka::Error& error)
				{
					auto log = odc::spdlog_get("KafkaPort");
					if (!error)
					{
						if(log && log->should_log(spdlog::level::trace))
							log->trace("{}: Message delivered: {}", Name, metadata.toString());
					}
					else
					{
						if(log)
							log->error("{}: Message failed to be delivered: {} {} {}: {}",
								ToString(event->GetEventType()),event->GetIndex(),
								event->GetPayloadString(), error.message());
					}
				};

	//TODO: check if any of the pKafkaProducer calls can throw

	// Send a message
	// Use the non-blocking option, and the overload that catches exceptions
	pKafkaProducer->send(record, deliveryCb);

	// Poll events (e.g. message delivery callback)
	pKafkaProducer->pollEvents(std::chrono::milliseconds::zero());
}