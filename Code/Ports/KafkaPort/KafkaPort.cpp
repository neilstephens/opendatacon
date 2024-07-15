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
 * KafkaPort.cpp
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

#include "KafkaPort.h"
#include "KafkaPortConf.h"

KafkaPort::KafkaPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides):
	DataPort(Name, Filename, Overrides)
{
	pConf = std::make_unique<KafkaPortConf>();
	ProcessFile();
}

void KafkaPort::Enable()
{
	enabled = true;
}

void KafkaPort::Disable()
{
	enabled = false;
}

void KafkaPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	//JSON member NativeKafkaProperties has all the kafka property strings that are directly passed to the kafka stack
	if(JSONRoot.isMember("NativeKafkaProperties"))
	{
		for(auto memberName : JSONRoot["NativeKafkaProperties"].getMemberNames())
		{
			if(JSONRoot["NativeKafkaProperties"][memberName].isString()
			   || JSONRoot["NativeKafkaProperties"][memberName].isNumeric()
			   || JSONRoot["NativeKafkaProperties"][memberName].isBool())
			{
				pConf->NativeKafkaProperties.put(memberName, JSONRoot["NativeKafkaProperties"][memberName].asString());
			}
			else if(auto log = spdlog::get("KafkaPort"))
			{
				log->error("NativeKafkaProperties member '{}' is not a simple value; ignoring.", memberName);
			}
		}
	}
	if(JSONRoot.isMember("ShareKafkaClient"))
	{
		pConf->ShareKafkaClient = JSONRoot["ShareKafkaClient"].asBool();
	}
	if(JSONRoot.isMember("SharedKafkaClientKey"))
	{
		pConf->SharedKafkaClientKey = JSONRoot["SharedKafkaClientKey"].asString();
	}
	if(JSONRoot.isMember("MinPollIntervalms"))
	{
		pConf->SharedKafkaClientKey = JSONRoot["MinPollIntervalms"].asUInt();
	}
}
