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
 * main.cpp
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#include "KafkaProducerPort.h"
#include "KafkaConsumerPort.h"

extern "C" KafkaProducerPort* new_KafkaProducerPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new KafkaProducerPort(Name,File,Overrides);
}

extern "C" void delete_KafkaProducerPort(KafkaProducerPort* aKafkaProducerPort_ptr)
{
	delete aKafkaProducerPort_ptr;
	return;
}

extern "C" KafkaConsumerPort* new_KafkaConsumerPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new KafkaConsumerPort(Name,File,Overrides);
}

extern "C" void delete_KafkaConsumerPort(KafkaConsumerPort* aKafkaConsumerPort_ptr)
{
	delete aKafkaConsumerPort_ptr;
	return;
}
