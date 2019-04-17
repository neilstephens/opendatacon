/*	opendatacon
*
*	Copyright (c) 2019:
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
* KafkaPointTableAccess.cpp
*
*  Created on: 17/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "KafkaPortConf.h"
#include "KafkaPointTableAccess.h"

KafkaPointTableAccess::KafkaPointTableAccess()
{}


bool KafkaPointTableAccess::AddAnalogPointToPointTable(const size_t & index, const std::string & key)
{
	std::string dummykey;
	if (GetAnalogKafkaKeyUsingODCIndex(index, dummykey))
	{
		LOGERROR("Error Duplicate Analog ODC Index {} - {}", std::to_string(index),key );
		return false;
	}

	AnalogODCKeyMap[index] = key;

	return true;
}
bool KafkaPointTableAccess::AddBinaryPointToPointTable(const size_t & index, const std::string & key)
{
	std::string dummykey;
	if (GetBinaryKafkaKeyUsingODCIndex(index, dummykey))
	{
		LOGERROR("Error Duplicate Binary ODC Index {} - {}", std::to_string(index), key);
		return false;
	}

	BinaryODCKeyMap[index] = key;

	return true;
}

bool KafkaPointTableAccess::GetAnalogKafkaKeyUsingODCIndex(const size_t index, std::string &key)
{
	KeyMapIterType MapIter = AnalogODCKeyMap.find(index);
	if (MapIter != AnalogODCKeyMap.end())
	{
		key = MapIter->second;
		return true;
	}
	return false;
}

bool KafkaPointTableAccess::GetBinaryKafkaKeyUsingODCIndex(const size_t index, std::string &key)
{
	KeyMapIterType MapIter = BinaryODCKeyMap.find(index);
	if (MapIter != BinaryODCKeyMap.end())
	{
		key = MapIter->second;
		return true;
	}
	return false;
}

