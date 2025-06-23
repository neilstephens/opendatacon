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
 * ConsumerTranslation.h
 *
 *  Created on: 2024-12-02
 *      Author: Neil Stephens
 */

#ifndef CONSUMERTRANSLATION_H
#define CONSUMERTRANSLATION_H

#include "TemplateDeserialiser.h"
#include "CBORDeserialiser.h"
#include "LuaDeserialiser.h"
#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <string_view>

struct ConsumerTranslationEntry
{
	std::unique_ptr<TemplateDeserialiser> pTemplateDeserialiser = nullptr;
	std::unique_ptr<CBORDeserialiser> pCBORDeserialiser = nullptr;
	std::unique_ptr<LuaDeserialiser> pLuaDeserialiser = nullptr;
};

using ConsumptionID = std::pair<kafka::Topic,kafka::Key>;

//declare ConsumptionIDLess, because std::less doesn't work with ConsumptionID
struct ConsumptionIDLess
{
	bool operator()(const ConsumptionID& lhs, const ConsumptionID& rhs) const
	{
		if(lhs.first < rhs.first) return true;
		if(rhs.first < lhs.first) return false;
		return std::string_view((char*)lhs.second.data(),lhs.second.size())
		       < std::string_view((char*)rhs.second.data(),rhs.second.size());
	}
};

using ConsumerTranslationMap = std::map<ConsumptionID,ConsumerTranslationEntry,ConsumptionIDLess>;

#endif // CONSUMERTRANSLATION_H