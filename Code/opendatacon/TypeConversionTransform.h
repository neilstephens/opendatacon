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
 * TypeConversionTransform.h
 *
 *	Converts events of one EventType to another (e.g. Counter <-> Analog) as they pass through a connection
 */

#ifndef TypeConversionTransform_H_
#define TypeConversionTransform_H_

#include "Log.h"
#include <opendatacon/EventConversion.h>
#include <opendatacon/Transform.h>
#include <unordered_set>

using namespace odc;

class TypeConversionTransform: public Transform
{
public:
	TypeConversionTransform(const std::string& Name, const Json::Value& params): Transform(Name,params)
	{
		if(!params.isMember("FromType") || !params["FromType"].isString()
		   || !params.isMember("ToType") || !params["ToType"].isString())
		{
			Log.Error("TypeConversionTransform requires 'FromType' and 'ToType' String Parameters");
			return;
		}
		FromType = EventTypeFromString(params["FromType"].asString());
		ToType = EventTypeFromString(params["ToType"].asString());
		if(FromType == EventType::BeforeRange || ToType == EventType::BeforeRange)
		{
			Log.Error("TypeConversionTransform has invalid 'FromType' or 'ToType': '{}' => '{}'", params["FromType"].asString(), params["ToType"].asString());
			return;
		}
		if(params.isMember("Indexes"))
		{
			if(!params["Indexes"].isArray())
			{
				Log.Error("TypeConversionTransform requires optional 'Indexes' Parameter to be an Array");
				return;
			}
			for(Json::ArrayIndex n = 0; n < params["Indexes"].size(); ++n)
				Indexes.insert(params["Indexes"][n].asUInt());
			IndexFilter = true;
		}
		Valid = true;
	}

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		if(!event || !Valid || event->GetEventType() != FromType
		   || (IndexFilter && !Indexes.count(event->GetIndex())))
			return (*pAllow)(event);

		try
		{
			auto converted = ConvertEvent(event,ToType);
			//ConvertEvent returns const - make a mutable deep copy
			return (*pAllow)(std::make_shared<EventInfo>(*converted));
		}
		catch(const std::exception& e)
		{
			Log.Error("TypeConversionTransform: dropping event: {}", e.what());
		}
		return (*pAllow)(nullptr);
	}

private:
	bool Valid = false;
	bool IndexFilter = false;
	EventType FromType = EventType::BeforeRange;
	EventType ToType = EventType::BeforeRange;
	std::unordered_set<uint16_t> Indexes;
};

#endif /* TypeConversionTransform_H_ */
