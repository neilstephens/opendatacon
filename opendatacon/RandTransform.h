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
 * IndexOffsetTransform.h
 *
 *  Created on: 30/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef RANDTRANSFORM_H_
#define RANDTRANSFORM_H_

#include <opendatacon/util.h>
#include <opendatacon/Transform.h>
#include <random>

using namespace odc;

class RandTransform: public Transform
{
public:
	RandTransform(const Json::Value& params):
		Transform(params)
	{}

	bool Event(std::shared_ptr<EventInfo> event) override
	{
		thread_local std::mt19937 RandNumGenerator = std::mt19937(std::random_device()());
		uint16_t random_number = std::uniform_int_distribution<unsigned int>(0, 100)(RandNumGenerator);
		if(event->GetEventType() != EventType::Analog)
			return true;
		event->SetPayload<EventType::Analog>(std::move(random_number));
		return true;
	}
};

#endif /* RANDTRANSFORM_H_ */
