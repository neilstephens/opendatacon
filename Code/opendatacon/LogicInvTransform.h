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
 * LogicInvTransform.h
 *
 *  Created on: 19/10/2018
 *      Author: Andrew Gilbett <andrew@gilbett.net>
 *  This performs a logical not on binary measured values
 */

#ifndef LOGICINVTRANSFORM_H_
#define LOGICINVTRANSFORM_H_

#include <opendatacon/util.h>
#include <opendatacon/Transform.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

class LogicInvTransform: public Transform
{
public:
	LogicInvTransform(const std::string& Name, const Json::Value& params): Transform(Name,params)
	{
		SetLog("Connectors");
	}

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		switch(event->GetEventType())
		{
			case EventType::BinaryOutputStatus:
				event->SetPayload<EventType::BinaryOutputStatus>(!event->GetPayload<EventType::BinaryOutputStatus>());
				break;
			case EventType::Binary:
				event->SetPayload<EventType::Binary>(!event->GetPayload<EventType::Binary>());
				break;
			default:
				break;
		}
		return (*pAllow)(event);
	}


};

#endif /* LOGICINVTRANSFORM_H_ */
