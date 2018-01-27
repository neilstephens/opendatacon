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
		NullJV(Json::Value::nullSingleton()),
		JV(aJV),
		ind_ref(find_marker(ind_marker,JV)),
		name_ref(find_marker(name_marker,JV)),
		val_ref(find_marker(val_marker,JV)),
		qual_ref(find_marker(qual_marker,JV)),
		time_ref(find_marker(time_marker,JV))
	{}
	template<typename T>
	Json::Value Instantiate(const T& event, uint16_t index, const std::string Name = "")
	{
		Json::Value instance = JV;
		if(!ind_ref.isNull())
			find_marker(ind_ref.asString(), instance) = index;
		if(!name_ref.isNull())
			find_marker(name_ref.asString(), instance) = Name;
		if(!val_ref.isNull())
			find_marker(val_ref.asString(), instance) = event.value;
		if(!qual_ref.isNull())
			find_marker(qual_ref.asString(), instance) = event.quality;
		if(!time_ref.isNull())
			find_marker(time_ref.asString(), instance) = (Json::UInt64)event.time.Get();
		return instance;
	}
private:
	Json::Value NullJV;
	const Json::Value JV;
	const Json::Value &ind_ref, &name_ref, &val_ref, &qual_ref, &time_ref;
	template<typename T>
	T& find_marker(const std::string& marker, T& val)
	{
		for(auto it = val.begin(); it != val.end(); it++)
		{
			if((*it).isString() && (*it).asString() == marker)
				return *it;
			if((*it).isObject() || (*it).isArray())
			{
				T& look_deeper = find_marker(marker, *it);
				if(!look_deeper.isNull())
					return look_deeper;
			}
		}
		return NullJV;
	}
};

#endif // JSONOUTPUTTEMPLATE_H
