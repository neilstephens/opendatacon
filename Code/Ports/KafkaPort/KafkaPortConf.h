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
 * KafkaPortConf.h
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#ifndef KafkaPortConf_H_
#define KafkaPortConf_H_
#include <kafka/Types.h>
#include <kafka/Properties.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/DataPortConf.h>
#include <opendatacon/TranslationEngine.h>
#include <string>
#include <cstddef>

enum class server_type_t {ONDEMAND,PERSISTENT,MANUAL};

class KafkaPortConf: public DataPortConf
{
public:
	explicit KafkaPortConf(const bool isProducer):
		isProducer(isProducer)
	{
		//a Kafka topic is the translation 'channel'
		Translation.DefaultChannel = "opendatacon";
		Translation.Config.LogName = "KafkaPort";
	}
	bool isProducer;

	bool ShareKafkaClient = isProducer;
	server_type_t ServerType = isProducer ? server_type_t::PERSISTENT : server_type_t::ONDEMAND;
	std::string SharedKafkaClientKey = "";
	kafka::Properties NativeKafkaProperties;
	size_t MaxPollIntervalms = 100;

	//Payload and key translation options, and defaults for points
	//	(DefaultTopic is Translation.DefaultChannel)
	odc::TranslationSettings Translation;
	//Use pointer to const map, because it will be populated at DataPort::ProcessElements/Build time
	//	then accessed by multiple threads in Event, so it needs to be const
	std::unique_ptr<const odc::PointTranslationMap> pPointMap = nullptr;
	//Built from Translation and pPointMap at Build time. Const, for the same reason.
	std::unique_ptr<const odc::TranslationEngine> pEngine = nullptr;

	//Producer
	bool BlockUnknownPoints = false;
	bool OverridesCreateNewPTMEntries = false;
	bool MustOverridePTMEntries = false;
	bool DeduplicateEvents = true;
	//TODO: Support for Template {} vs <> for std::format specifiers

	//Consumer
	int64_t ConsumerFastForwardOffset = 0;
};

#endif /* KafkaPortConf_H_ */
