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
 * JSONOutputTemplate.h
 *
 *  Created on: 22/10/2016
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONOUTPUTTEMPLATE_H
#define JSONOUTPUTTEMPLATE_H

#include <json/json.h>

class JSONOutputTemplate
{
public:
	//non-copyable because it references itself.
	JSONOutputTemplate(const JSONOutputTemplate&) = delete;
	JSONOutputTemplate& operator=(const JSONOutputTemplate&) = delete;
	JSONOutputTemplate(const Json::Value& aJV, const std::string& ind_marker, const std::string& name_marker, const std::string& val_marker, const std::string& qual_marker, const std::string& time_marker):
		NullJV(Json::Value::null),
		JV(aJV),
		ind_ref(find_marker(ind_marker,JV)),
		name_ref(find_marker(name_marker,JV)),
		val_ref(find_marker(val_marker,JV)),
		qual_ref(find_marker(qual_marker,JV)),
		time_ref(find_marker(time_marker,JV))
	{}
	template<typename T>
	Json::Value Instantiate(const T& event, u_int16_t index, const std::string Name = "")
	{
		if(!ind_ref.isNull())
			ind_ref = index;
		if(!name_ref.isNull())
			name_ref = Name;
		if(!val_ref.isNull())
			val_ref = event.value;
		if(!qual_ref.isNull())
			qual_ref = event.quality;
		if(!time_ref.isNull())
			time_ref = (Json::UInt64)event.time.Get();
		return JV;
	}
private:
	Json::Value NullJV;
	Json::Value JV;
	Json::Value &ind_ref, &name_ref, &val_ref, &qual_ref, &time_ref;
	Json::Value& find_marker(const std::string& marker, Json::Value& val)
	{
		for(auto it = val.begin(); it != val.end(); it++)
		{
			if((*it).isString() && (*it).asString() == marker)
				return *it;
			if((*it).isObject() || (*it).isArray())
				return find_marker(marker, *it);
		}
		return NullJV;
	}
};

#endif // JSONOUTPUTTEMPLATE_H
