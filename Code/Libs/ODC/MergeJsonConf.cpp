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
 * MergeJsonConf.cpp
 *
 *  Created on: 23/06/2025
 *      Author: Neil Stephens
 */

#include <opendatacon/MergeJsonConf.h>
#include <json/json.h>
#include <unordered_map>
#include <string>

//Arrays are concatenated, unless they both contain objects that match on "Index" or "Name",
//	in which case, the matching objects are merged
void MergeArrays(Json::Value& baseArray, const Json::Value& overridingArray)
{
	// Create a map of objects in base array that could be matched
	std::unordered_map<std::string, Json::ArrayIndex> baseObjectMap;
	for (Json::ArrayIndex n = 0; n < baseArray.size(); n++)
	{
		if (!baseArray[n].isObject())
			continue;

		std::string key = "";
		if (baseArray[n].isMember("Index"))
			key = "Index:" + baseArray[n]["Index"].asString();
		else if (baseArray[n].isMember("Name"))
			key = "Name:" + baseArray[n]["Name"].asString();

		if(!key.empty())
			baseObjectMap[key] = n;
	}

	// Process overriding array
	for (Json::ArrayIndex n = 0; n < overridingArray.size(); n++)
	{
		const auto& overridingItem = overridingArray[n];
		if(!overridingItem.isObject())
		{
			baseArray.append(overridingItem);
			continue;
		}

		std::string key = "";
		if (overridingItem.isMember("Index"))
			key = "Index:" + overridingItem["Index"].asString();
		else if (overridingItem.isMember("Name"))
			key = "Name:" + overridingItem["Name"].asString();

		auto found = baseObjectMap.find(key);
		if (found != baseObjectMap.end())
			MergeJsonConf(baseArray[found->second], overridingItem);
		else // No matching object found - append
			baseArray.append(overridingItem);
	}
}

void MergeJsonConf(Json::Value& base, const Json::Value& overriding)
{
	if(base.isArray() && overriding.isArray())
	{
		MergeArrays(base, overriding);
		return;
	}
	if(base.isObject() && overriding.isObject())
	{
		for(const std::string& key : overriding.getMemberNames())
		{
			if (base.isMember(key))
				MergeJsonConf(base[key], overriding[key]);
			else
				base[key] = overriding[key];
		}
		return;
	}
	base = overriding;
}
