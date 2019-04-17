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
* KafkaPointConf.cpp
*
*  Created on:16/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef KafkaPointConf_H_
#define KafkaPointConf_H_

#include <regex>
#include <algorithm>
#include <memory>
#include <map>
#include "KafkaPointConf.h"


using namespace odc;

KafkaPointConf::KafkaPointConf(const std::string & FileName, const Json::Value& ConfOverrides):
	ConfigParser(FileName, ConfOverrides)
{
	LOGDEBUG("Conf processing file - {}",FileName);
	ProcessFile(); // This should call process elements below?
}


void KafkaPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values
	LOGDEBUG("Conf processing");

	if (JSONRoot.isMember("Analogs"))
	{
		const auto Analogs = JSONRoot["Analogs"];
		LOGDEBUG("Conf processed - Analog Points");
		ProcessPoints(PointType::Analog, Analogs);
	}
	if (JSONRoot.isMember("Binaries"))
	{
		const auto Binaries = JSONRoot["Binaries"];
		LOGDEBUG("Conf processed - Binary Points");
		ProcessPoints(PointType::Binary, Binaries);
	}

	if (JSONRoot.isMember("KafkaCommandTimeoutmsec"))
	{
		//	KafkaCommandTimeoutmsec = JSONRoot["KafkaCommandTimeoutmsec"].asUInt();
		//	LOGDEBUG("Conf processed - KafkaCommandTimeoutmsec - {}", std::to_string(KafkaCommandTimeoutmsec));
	}
	if (JSONRoot.isMember("KafkaCommandRetries"))
	{
		//	KafkaCommandRetries = JSONRoot["KafkaCommandRetries"].asUInt();
		//	LOGDEBUG("Conf processed - KafkaCommandRetries - {}", std::to_string(KafkaCommandRetries));
	}
	LOGDEBUG("End Conf processing");
}

// This method loads both Binary points. The Key looks like "HS01234|ANA|56"
void KafkaPointConf::ProcessPoints(PointType pt, const Json::Value& JSONNode)
{
	LOGDEBUG("Conf processing - {}", GetPointTypeString(pt));

	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;
		uint32_t start, stop; // Will set index from these later
		std::string eqtype = "";
		uint8_t SOEIndex = 0;

		if (JSONNode[n].isMember("Index"))
		{
			start = stop = JSONNode[n]["Index"].asUInt();
		}
		else if (JSONNode[n]["Range"].isMember("Start") && JSONNode[n]["Range"].isMember("Stop"))
		{
			start = JSONNode[n]["Range"]["Start"].asUInt();
			stop = JSONNode[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			LOGERROR("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : {}", JSONNode[n].toStyledString());
			start = 1;
			stop = 0;
			error = true;
		}

		if (JSONNode[n].isMember("EQType"))
			eqtype = JSONNode[n]["Group"].asUInt();
		else
		{
			LOGERROR(" A point needs a \"EQType\" : {}",JSONNode[n].toStyledString());
			error = true;
		}

		if (!error)
		{
			for (uint32_t index = start; index <= stop; index++)
			{
				std::string key = GetKafkaKey(eqtype,pt,index);

				if (pt == PointType::Binary)
				{
					if (PointTable.AddBinaryPointToPointTable(index, key))
					{
						LOGDEBUG("Adding a Binary Point - Index: {}, Key: {}", std::to_string(index), key);
					}
				}
				else
				{
					if (PointTable.AddAnalogPointToPointTable(index, key))
					{
						LOGDEBUG("Adding a Analog Point - Index: {}, Key: {}", std::to_string(index), key);
					}
				}
			}
		}
	}
	LOGDEBUG("Conf processing - {} - Finished", GetPointTypeString(pt));
}

#endif
