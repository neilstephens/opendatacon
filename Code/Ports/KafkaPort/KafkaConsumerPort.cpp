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
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <kafka/KafkaConsumer.h>
#include <chrono>
#include <memory>

void KafkaConsumerPort::Build()
{
	pKafkaConsumer = KafkaPort::Build<KCC::KafkaConsumer>("Consumer");
	if(!pKafkaConsumer)
		throw std::runtime_error(Name+": Failed to create Kafka Consumer");

	//TODO: set up the consumer
	//warn if the client id isn't set in the properties,
	//	so that the consumer can be restarted without losing its place (kafka takes care of this I think)
	//manually commitAsync() offsets in event callbacks - depending on "enable.auto.commit=true" in the config
	//	provide a rebalance callback to commit offsets before rebalancing (if in a group and manual commit is enabled)

	//Two options for what the consumer consumes:
	//	1. "subscribe" to whole topics (optionally as part of a consumer group)
	//	2. "assign" partitions to consume from manually (not compatible with consumer groups)

	//Pause on start - resume on Enable()

	//Set up polling timer chain
	//	poll() should be called even when paused
	//	re-call immediately fill events returned, otherwise exponentially back off to MaxPollIntervalms
	pKafkaConsumer->poll(std::chrono::milliseconds::zero());
}

//TODO: override Disable/Enable methods to "pause"/"continue" the consumer, or destroy/recreate it (depending on group config)
//	Option to fast forward to the end offset on enable, if not in group (minus 1 to get latest value, or configurable buffer)