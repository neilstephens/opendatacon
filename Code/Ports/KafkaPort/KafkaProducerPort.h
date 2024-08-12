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
 * KafkaProducerPort.h
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#ifndef KAFKAPRODUCERPORT_H
#define KAFKAPRODUCERPORT_H

#include "KafkaPort.h"
#include <kafka/KafkaProducer.h>

using namespace odc;
namespace KCP = kafka::clients::producer;
using OptionalPoint = std::optional<std::reference_wrapper<const PointTranslationEntry>>;

class KafkaProducerPort: public KafkaPort
{
public:
	KafkaProducerPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides)
		:KafkaPort(Name,Filename,Overrides){};
	virtual ~KafkaProducerPort(){};

	virtual void Build() override;
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

private:
	OptionalPoint CheckPointTranslationMap(std::shared_ptr<const EventInfo> event, const std::string& SenderName);
	std::shared_ptr<KCP::KafkaProducer> pKafkaProducer;
};

#endif // KAFKAPRODUCERPORT_H
