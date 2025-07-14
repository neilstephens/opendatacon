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
 * EventTranslation.h
 *
 *  Created on: 15/08/2024
 *      Author: Neil Stephens
 */

#ifndef EVENTTRANSLATION_H
#define EVENTTRANSLATION_H

#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <unordered_map>

enum class EventTranslationMethod
{
	Lua,
	Template,
	CBOR
};

enum class SourceLookupMethod
{
	SenderName,
	SourcePort,
	None
};

using ExtraPointFields = std::unordered_map<std::string, std::string>;

class CBORSerialiser;
struct PointTranslationEntry
{
	bool override = false;
	std::unique_ptr<kafka::Topic> pTopic = nullptr;
	std::unique_ptr<odc::OctetStringBuffer> pKey = nullptr;
	std::unique_ptr<std::string> pTemplate = nullptr;
	std::unique_ptr<CBORSerialiser> pCBORer = nullptr;
	std::unique_ptr<ExtraPointFields> pExtraFields;
};

using SourceID = std::string;
using PointIndex = size_t;
using TranslationID = std::tuple<SourceID,PointIndex,odc::EventType>;
using PointTranslationMap = std::map<TranslationID, PointTranslationEntry>;

#endif // EVENTTRANSLATION_H
