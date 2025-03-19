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
 * TestTransforms.h
 *
 *  Created on: 2/7/2023
 *      Author: Neil Stephens
 */
#ifndef TESTTRANSFORMS_H
#define TESTTRANSFORMS_H

#include <opendatacon/Transform.h>

namespace odc
{

class AddShiftTransform: public Transform
{
public:
	AddShiftTransform(const std::string& Name, const Json::Value& params):
		Transform(Name,params)
	{
		if(!params.isMember("Add") || !params["Add"].isNumeric())
			this->params["Add"] = Json::Int(1);
		if(!params.isMember("Shift") || !params["Shift"].isNumeric())
			this->params["Shift"] = Json::Int(10);
	}
	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow)
	{
		if(event->GetEventType() == EventType::Analog)
			event->SetPayload<EventType::Analog>((event->GetPayload<EventType::Analog>()+params["Add"].asDouble())*params["Shift"].asDouble());
		return (*pAllow)(event);
	}
};

class NopTransform: public Transform
{
public:
	NopTransform(const std::string& Name, const Json::Value& params):
		Transform(Name,params)
	{}
	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow)
	{
		return (*pAllow)(event);
	}
};

class BadTransform: public Transform
{
public:
	BadTransform(const std::string& Name, const Json::Value& params):
		Transform(Name,params)
	{}
	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow)
	{
		//BAD behaviour!
		//	call the callback to 'drop' the event
		//	but then call it again and 'allow'
		(*pAllow)(nullptr);
		return (*pAllow)(event);
	}
};

} //namespace odc

#endif // TESTTRANSFORMS_H
