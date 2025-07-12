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
 * IDeserialiser.h
 *
 *  Created on: 2024-12-08
 *      Author: Neil Stephens
 */

#ifndef IDESERIALISER_H
#define IDESERIALISER_H

#include <opendatacon/LogHelpers.h>
#include <opendatacon/IOTypes.h>
#include <kafka/ConsumerRecord.h>
#include <string>

using namespace odc;
namespace KCC = kafka::clients::consumer;

class Deserialiser: public LogHelpers
{
public:
	Deserialiser(const std::string& datetime_format, const bool utc):
		datetime_format(datetime_format),
		utc(utc)
	{
		SetLog("KafkaPort");
	}
	virtual ~Deserialiser()
	{};
	virtual std::shared_ptr<EventInfo> Deserialise(const KCC::ConsumerRecord& record) = 0;

protected:
	const std::string datetime_format;
	const bool utc;
};

#endif // IDESERIALISER_H
