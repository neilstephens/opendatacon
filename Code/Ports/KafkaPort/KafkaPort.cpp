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
#include "Log.h"
#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <cstddef>

KafkaPort::KafkaPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides, const bool isProducer):
	DataPort(Name, Filename, Overrides)
{
	pConf = std::make_unique<KafkaPortConf>(isProducer);
	ProcessFile();

	auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
	if(pConf->pPointMap && pConf->MustOverridePTMEntries)
	{
		PointTranslationMap ptm;
		for(const auto& [tid,pte] : *pConf->pPointMap)
		{
			if(pte.override)
				ptm[tid] = pte;
		}
		pConf->pPointMap = std::make_unique<const PointTranslationMap>(std::move(ptm));
	}
}

void KafkaPort::Enable()
{
	auto weak_self = weak_from_this();
	pStateSync->post([weak_self,this]()
		{
			if(auto self = weak_self.lock())
			{
				if(enabled)
					return;
				auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
				if(pConf->ServerType == server_type_t::PERSISTENT || (pConf->ServerType == server_type_t::ONDEMAND && InDemand()))
					PortUp();
				enabled = true;
				PublishEvent(ConnectState::PORT_UP);
			}
		});
}

void KafkaPort::Disable()
{
	auto weak_self = weak_from_this();
	pStateSync->post([weak_self,this]()
		{
			if(auto self = weak_self.lock())
			{
				if(!enabled)
					return;
				enabled = false;
				PortDown();
				PublishEvent(ConnectState::PORT_DOWN);
			}
		});
}

void KafkaPort::ConnectionEvent(std::shared_ptr<const EventInfo> event, SharedStatusCallback_t pStatusCallback)
{
	auto weak_self = weak_from_this();
	pStateSync->post([weak_self,this,event,pStatusCallback]()
		{
			if(auto self = weak_self.lock())
			{
				auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());
				if(pConf->ServerType == server_type_t::ONDEMAND)
				{
					if(event->GetPayload<EventType::ConnectState>() == ConnectState::CONNECTED)
						PortUp();
					else if(!InDemand())
						PortDown();
					(*pStatusCallback)(odc::CommandStatus::SUCCESS);
					return;
				}
			}
			(*pStatusCallback)(odc::CommandStatus::UNDEFINED);
		});
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
			else
				Log.Error("NativeKafkaProperties member '{}' is not a simple value; ignoring.", memberName);
		}
	}
	if(JSONRoot.isMember("ShareKafkaClient"))
	{
		pConf->ShareKafkaClient = JSONRoot["ShareKafkaClient"].asBool();
	}
	if(JSONRoot.isMember("ServerType"))
	{
		if(JSONRoot["ServerType"].asString() == "ONDEMAND")
			pConf->ServerType = server_type_t::ONDEMAND;
		else if(JSONRoot["ServerType"].asString() == "PERSISTENT")
			pConf->ServerType = server_type_t::PERSISTENT;
		else if(JSONRoot["ServerType"].asString() == "MANUAL")
			pConf->ServerType = server_type_t::MANUAL;
		else
		{
			Log.Warn("Invalid KafkaPort server type: '{}'.", JSONRoot["ServerType"].asString());
		}
	}
	if(JSONRoot.isMember("SharedKafkaClientKey"))
	{
		pConf->SharedKafkaClientKey = JSONRoot["SharedKafkaClientKey"].asString();
	}
	if(JSONRoot.isMember("MaxPollIntervalms"))
	{
		pConf->MaxPollIntervalms = JSONRoot["MaxPollIntervalms"].asUInt();
	}
	if(JSONRoot.isMember("DefaultTopic"))
	{
		pConf->Translation.DefaultChannel = JSONRoot["DefaultTopic"].asString();
	}

	//The payload/key translation options, ie. TranslationMethod, DefaultTemplate, DefaultKey, OctetStringFormat etc.
	ParseTranslationSettings(JSONRoot, pConf->Translation);

	if(JSONRoot.isMember("BlockUnknownPoints"))
	{
		pConf->BlockUnknownPoints = JSONRoot["BlockUnknownPoints"].asBool();
	}
	if(JSONRoot.isMember("OverridesCreateNewPTMEntries"))
	{
		pConf->OverridesCreateNewPTMEntries = JSONRoot["OverridesCreateNewPTMEntries"].asBool();
	}
	if(JSONRoot.isMember("MustOverridePTMEntries"))
	{
		pConf->MustOverridePTMEntries = JSONRoot["MustOverridePTMEntries"].asBool();
	}
	if(JSONRoot.isMember("DeduplicateEvents"))
	{
		pConf->DeduplicateEvents = JSONRoot["DeduplicateEvents"].asBool();
	}
	if(JSONRoot.isMember("ConsumerFastForwardOffset"))
	{
		pConf->ConsumerFastForwardOffset = JSONRoot["ConsumerFastForwardOffset"].asInt64();
	}

	//Process PointTranslationMap
	if(JSONRoot.isMember("PointTranslationMap"))
	{
		bool overriding = !!pConf->pPointMap;
		PointTranslationMap ptm;
		//Copy the contents of the existing map if there is one
		if(overriding)
			ptm = *pConf->pPointMap;

		//A point's Kafka topic is its translation channel
		auto handler = [](const std::string& member, const Json::Value& entry, PointTranslationEntry& pte)
				   {
					   if(member != "Topic")
						   return false;
					   pte.Channel = entry["Topic"].asString();
					   return true;
				   };
		ParsePointTranslationMap(JSONRoot["PointTranslationMap"], ptm, overriding, pConf->OverridesCreateNewPTMEntries, "KafkaPort", handler);
		pConf->pPointMap = std::make_unique<const PointTranslationMap>(std::move(ptm));
	}
}
