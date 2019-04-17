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
 * KafkaPointConf.h
 *
 *  Created on: 16/04/2019
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef KafkaPOINTCONF_H_
#define KafkaPOINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <bitset>
#include <chrono>

#include <opendatacon/IOTypes.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>


#include "Kafka.h"
#include "KafkaUtility.h"
#include "KafkaPointTableAccess.h"

using namespace odc;

// OpenDataCon uses an Index to refer to Analogs and Binaries.

class KafkaPointConf: public ConfigParser
{
public:
	KafkaPointConf(const std::string& FileName, const Json::Value& ConfOverrides);

	// JSON File section processing commands
	void ProcessElements(const Json::Value& JSONRoot) override;
	void ProcessPoints(PointType pt, const Json::Value & JSONNode);

	KafkaPointTableAccess PointTable; // All access to point table through this.
};
#endif
