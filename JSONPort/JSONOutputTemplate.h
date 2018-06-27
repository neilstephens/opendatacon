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
#include <opendatacon/IOTypes.h>

//TODO: make the constructor and instantiator variadic so it's completely generic

class JSONOutputTemplate
{
public:
	//non-copyable because it references itself.
	JSONOutputTemplate(const JSONOutputTemplate&) = delete;
	JSONOutputTemplate& operator=(const JSONOutputTemplate&) = delete;

	JSONOutputTemplate(const Json::Value& aJV, const std::string& ind_marker,const std::string& val_marker,
		const std::string& qual_marker, const std::string& time_marker, const std::string& name_marker,
		const std::string& source_marker, const std::string& sender_marker):
		NullJV(Json::Value::nullSingleton()),
		JV(aJV),
		ind_ref(find_marker(ind_marker,JV)),
		val_ref(find_marker(val_marker,JV)),
		qual_ref(find_marker(qual_marker,JV)),
		time_ref(find_marker(time_marker,JV)),
		name_ref(find_marker(name_marker,JV)),
		source_ref(find_marker(source_marker,JV)),
		sender_ref(find_marker(sender_marker,JV))
	{}
	template<typename T>
	Json::Value Instantiate(uint16_t index, const T& value, odc::QualityFlags qual,
		odc::msSinceEpoch_t time, const std::string& PointName = "",
		const std::string& SourcePort = "", const std::string& Sender = "")
	{
		Json::Value instance = JV;
		if(!ind_ref.isNull())
			find_marker(ind_ref.asString(), instance) = index;
		if(!val_ref.isNull())
		{
			if(std::is_same<T,odc::QualityFlags>::value || std::is_same<T,odc::eControlRelayOutputBlock>::value)
				find_marker(val_ref.asString(), instance) = odc::ToString(value);
			else
				find_marker(val_ref.asString(), instance) = value;
		}
		if(!qual_ref.isNull())
			find_marker(qual_ref.asString(), instance) = odc::ToString(qual);
		if(!time_ref.isNull())
			find_marker(time_ref.asString(), instance) = time;
		if(!name_ref.isNull())
			find_marker(name_ref.asString(), instance) = PointName;
		if(!source_ref.isNull())
			find_marker(source_ref.asString(), instance) = SourcePort;
		if(!sender_ref.isNull())
			find_marker(sender_ref.asString(), instance) = Sender;
		return instance;
	}
private:
	Json::Value NullJV;
	const Json::Value JV;
	const Json::Value &ind_ref, &val_ref, &qual_ref, &time_ref, &name_ref, &source_ref, &sender_ref;
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
