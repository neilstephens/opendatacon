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
* KafkaPointTableAccess.h
*
*  Created on: 17/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef KafkaPOINTTABLEACCESS_H_
#define KafkaPOINTTABLEACCESS_H_

#include <unordered_map>
#include <vector>
#include <functional>


#include "Kafka.h"

using namespace odc;

// Collect the access routines into the point table here.
typedef std::map<size_t, std::string> KeyMapType;
typedef std::map<size_t, std::string>::iterator KeyMapIterType;

class KafkaPointTableAccess
{
public:
	KafkaPointTableAccess();

	// We have two operations that we need to do. We need to store the Kafka Key details against the ODC index,
	// Then when we get the ODC index, we need to be able to retreive the Kafka key (quickly!)

	bool AddAnalogPointToPointTable(const size_t & index, const std::string & key);
	bool AddBinaryPointToPointTable(const size_t & index, const std::string & key);


	bool GetAnalogKafkaKeyUsingODCIndex(const size_t index, std::string & key);
	bool GetBinaryKafkaKeyUsingODCIndex(const size_t index, std::string & key);

protected:

	KeyMapType BinaryODCKeyMap; // ODC Index, Kafka Key
	KeyMapType AnalogODCKeyMap; // ODC Index, Kafka Key
};
#endif
