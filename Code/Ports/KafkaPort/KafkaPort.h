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
 * KafkaPort.h
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#ifndef KAFKAPORT_H
#define KAFKAPORT_H

#include "KafkaClientCache.h"
#include <atomic>
#include <opendatacon/DataPort.h>

using namespace odc;

class KafkaPort: public DataPort
{
public:
	KafkaPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides);
	virtual ~KafkaPort(){};

	virtual void ProcessElements(const Json::Value& JSONRoot) override;
	virtual void Enable() override;
	virtual void Disable() override;

	virtual void Build() override = 0;
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

protected:
	std::atomic_bool enabled {false};
	std::shared_ptr<KafkaClientCache> pKafkaClientCache = KafkaClientCache::Get();
};

#endif // KAFKAPORT_H