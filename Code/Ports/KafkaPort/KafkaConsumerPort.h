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
 * KafkaConsumerPort.h
 *
 *  Created on: 26/07/2024
 *      Author: Neil Stephens
 */

#ifndef KAFKACONSUMERPORT_H
#define KAFKACONSUMERPORT_H

#include "KafkaPort.h"
#include <kafka/KafkaConsumer.h>

using namespace odc;
namespace KCC = kafka::clients::consumer;

class KafkaConsumerPort: public KafkaPort
{
public:
	KafkaConsumerPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides)
		:KafkaPort(Name,Filename,Overrides,false){};
	virtual ~KafkaConsumerPort()
	{
		if(pPollTimer)
		{
			pPollTimer->cancel();
			pPollTimer.reset();
		}
	};

	void Build() override;
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

protected:
	void PortUp() override;
	void PortDown() override;

private:
	std::shared_ptr<KCC::KafkaConsumer> pKafkaConsumer = nullptr;
	std::set<kafka::Topic> mTopics;
	bool subscribed = false;
	size_t PollBackoff_ms = 1;
	std::shared_ptr<asio::steady_timer> pPollTimer = nullptr;
	void BuildConsumer();
	void RebalanceCallback(kafka::clients::consumer::RebalanceEventType et, const kafka::TopicPartitions& tps);
	void Subscribe();
	void Poll(std::weak_ptr<asio::steady_timer> wTimer);
	void ProcessRecord(const KCC::ConsumerRecord& record);
	std::shared_ptr<EventInfo> TemplateDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<TemplateDeserialiser>& pTemplateDeserialiser);
	std::shared_ptr<EventInfo> CBORDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<CBORDeserialiser>& pCBORDeserialiser);
	std::shared_ptr<EventInfo> LuaDeserialise(const KCC::ConsumerRecord& record, const std::unique_ptr<LuaDeserialiser>& pLuaDeserialiser);
};

#endif // KAFKACONSUMERPORT_H
