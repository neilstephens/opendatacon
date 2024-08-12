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
#include <opendatacon/IOTypes.h>
#include <cstddef>
#include <opendatacon/DataPortConf.h>
#include <kafka/Properties.h>
#include <string>

enum class EventTranslationMethod
{
	Lua,
	Template
};

enum class SourceLookupMethod
{
	SenderName,
	SourcePort,
	None
};

struct PointTranslationEntry
{
	std::unique_ptr<kafka::Topic> pTopic = nullptr;
	std::unique_ptr<odc::OctetStringBuffer> pKey = nullptr;
	std::unique_ptr<std::string> pTemplate = nullptr;
	std::unique_ptr<std::unordered_map<std::string, std::string>> pExtraFields;
};

using SourceID = std::string;
using PointIndex = size_t;
using TranslationID = std::tuple<SourceID,PointIndex,odc::EventType>;
using PointTranslationMap = std::map<TranslationID, PointTranslationEntry>;

inline std::string JSONwPlaceholders()
{
	return R"(
{
	"EventType": "<EVENTTYPE>",
	"Index": <INDEX>,
	"Description": "<POINT:Name>",
	"Timestamp": <TIMESTAMP>,
	"DateTime": "<DATETIME>",
	"Quality": "<QUALITY>",
	"NumericQuality": <RAWQUALITY>,
	"Value": <VALUE>,
	"SourcePort": "<SOURCEPORT>",
	"SenderName": "<SENDERNAME>"
}
)";
}

class KafkaPortConf: public DataPortConf
{
public:
	bool ShareKafkaClient = true;
	std::string SharedKafkaClientKey = "";
	kafka::Properties NativeKafkaProperties;
	size_t MaxPollIntervalms = 100;
	kafka::Topic DefaultTopic = "opendatacon";
	odc::OctetStringBuffer DefaultKey;
	std::string DefaultTemplate = JSONwPlaceholders();
	const std::unordered_map<std::string, std::string> DefaultExtraFields = {};
	EventTranslationMethod TranslationMethod = EventTranslationMethod::Template;
	bool BlockUnknownPoints = false;
	SourceLookupMethod PointTraslationSource = SourceLookupMethod::None;

	//Use pointer to const map, because it will be populated at DataPort::ProcessElements/Build time
	//	then accessed by multiple threads in Event, so it needs to be const
	std::unique_ptr<const PointTranslationMap> pPointMap = nullptr;

	//TODO: Option for DateTime formatting string
	//TODO: Option for OctetString formatting mode
};

#endif /* KafkaPortConf_H_ */
