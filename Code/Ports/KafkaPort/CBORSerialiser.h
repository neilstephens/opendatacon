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
 * CBORSerialiser.h
 *
 *  Created on: 13/08/2024
 *      Author: Neil Stephens
 */

#ifndef CBORSERIALISER_H
#define CBORSERIALISER_H

#include <opendatacon/IOTypes.h>
#include <json/json.h>

using namespace odc;

static auto EVENTTYPE = [](const std::shared_ptr<const EventInfo>& event){};

using TranslationSequence = std::vector<decltype(EVENTTYPE)>;

class CBORSerialiser
{
public:
	CBORSerialiser(const Json::Value& JSONVal)
	{
		//the json value has the same structure as the CBOR will.
		//store a sequence of lambdas that will serialise an EventInfo into the CBOR format
		//the lambdas will be called in order to serialise the EventInfo in Encode(shared_ptr<const EventInfo> event)
		// iterate over the JSON object

	}

	//TODO: Implement the rest of CBORSerialiser
};

#endif // CBORSERIALISER_H