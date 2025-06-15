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
#include "CBORSerialiser.h"
#include "EventTranslation.h"
#include "ConsumerTranslation.h"
#include <kafka/Types.h>
#include <kafka/Properties.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/DataPortConf.h>
#include <string>
#include <cstddef>

inline std::string JSONwPlaceholders()
{
	return R"(
{
	"EventType": "<EVENTTYPE>",
	"NumericEventType": <EVENTTYPE_RAW>,
	"Index": <INDEX>,
	"Description": "<POINT:Name>",
	"Timestamp": <TIMESTAMP>,
	"DateTime": "<DATETIME>",
	"Quality": "<QUALITY>",
	"NumericQuality": <QUALITY_RAW>,
	"Payload": <PAYLOAD>,
	"SourcePort": "<SOURCEPORT>",
	"SenderName": "<SENDERNAME>"
}
)";
}
inline std::string CBORStructure()
{
	return R"( ["TIMESTAMP","EVENTTYPE_RAW","SOURCEPORT","INDEX","QUALITY_RAW","PAYLOAD"] )";
}
enum class server_type_t {ONDEMAND,PERSISTENT,MANUAL};

class KafkaPortConf: public DataPortConf
{
public:
	explicit KafkaPortConf(const bool isProducer):
		isProducer(isProducer)
	{}
	bool isProducer;

	bool ShareKafkaClient = isProducer;
	server_type_t ServerType = isProducer ? server_type_t::PERSISTENT : server_type_t::ONDEMAND;
	std::string SharedKafkaClientKey = "";
	kafka::Properties NativeKafkaProperties;
	size_t MaxPollIntervalms = 100;
	kafka::Topic DefaultTopic = "opendatacon";
	EventTranslationMethod TranslationMethod = EventTranslationMethod::Template;
	std::string DefaultTemplate = JSONwPlaceholders();
	DataToStringMethod OctetStringFormat = DataToStringMethod::Base64;
	std::string DateTimeFormat = "%Y-%m-%d %H:%M:%S.%e";

	//Producer
	odc::OctetStringBuffer DefaultKey;
	CBORSerialiser DefaultCBORSerialiser = CBORStructure();
	const ExtraPointFields DefaultExtraFields = {};
	bool BlockUnknownPoints = false;
	SourceLookupMethod PointTraslationSource = SourceLookupMethod::None;
	//Use pointer to const map, because it will be populated at DataPort::ProcessElements/Build time
	//	then accessed by multiple threads in Event, so it needs to be const
	std::unique_ptr<const PointTranslationMap> pPointMap = nullptr;
	bool OverridesCreateNewPTMEntries = false;
	//TODO: make key dynamic, like the value
	//TODO: Support for Lua translation
	//TODO: Support for Template {} vs <> for std::format specifiers

	//Consumer
	size_t ConsumerFastForwardOffset = 0;
	//Use pointer to const map, because it will be populated at DataPort::ProcessElements/Build time
	//	then accessed by multiple threads in Event, so it needs to be const
	std::unique_ptr<const ConsumerTranslationMap> pKafkaMap = nullptr;
	bool RegexEscapeTemplates = true;
};

#endif /* KafkaPortConf_H_ */
