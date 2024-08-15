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
#include "CBORSerialiser.h"
#include "EventTranslation.h"
#include <kafka/Types.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <cstddef>

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
			else if(auto log = odc::spdlog_get("KafkaPort"))
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
	if(JSONRoot.isMember("MaxPollIntervalms"))
	{
		pConf->SharedKafkaClientKey = JSONRoot["MaxPollIntervalms"].asUInt();
	}
	if(JSONRoot.isMember("DefaultTopic"))
	{
		pConf->DefaultTopic = JSONRoot["DefaultTopic"].asString();
	}
	if(JSONRoot.isMember("DefaultKey"))
	{
		pConf->DefaultKey = JSONRoot["DefaultKey"].asString();
	}
	if(JSONRoot.isMember("DefaultTemplate"))
	{
		pConf->DefaultTemplate = JSONRoot["DefaultTemplate"].asString();
	}
	if(JSONRoot.isMember("TranslationMethod"))
	{
		if(JSONRoot["TranslationMethod"].asString() == "Lua")
			pConf->TranslationMethod = EventTranslationMethod::Lua;
		else if(JSONRoot["TranslationMethod"].asString() == "Template")
			pConf->TranslationMethod = EventTranslationMethod::Template;
		else if(JSONRoot["TranslationMethod"].asString() == "CBOR")
			pConf->TranslationMethod = EventTranslationMethod::CBOR;
		else
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Unknown TranslationMethod '{}'. Defaulting to Template.", JSONRoot["TranslationMethod"].asString());
			pConf->TranslationMethod = EventTranslationMethod::Template;
		}
	}
	if(JSONRoot.isMember("BlockUnknownPoints"))
	{
		pConf->BlockUnknownPoints = JSONRoot["BlockUnknownPoints"].asBool();
	}
	if(JSONRoot.isMember("PointTraslationSource"))
	{
		if(JSONRoot["PointTraslationSource"].asString() == "SenderName")
			pConf->PointTraslationSource = SourceLookupMethod::SenderName;
		else if(JSONRoot["PointTraslationSource"].asString() == "SourcePort")
			pConf->PointTraslationSource = SourceLookupMethod::SourcePort;
		else if(JSONRoot["PointTraslationSource"].asString() == "None")
			pConf->PointTraslationSource = SourceLookupMethod::None;
		else
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Unknown PointTraslationSource '{}'. Defaulting to None.", JSONRoot["PointTraslationSource"].asString());
			pConf->PointTraslationSource = SourceLookupMethod::None;
		}
	}
	if(JSONRoot.isMember("OverridesCreateNewPTMEntries"))
	{
		pConf->OverridesCreateNewPTMEntries = JSONRoot["OverridesCreateNewPTMEntries"].asBool();
	}

	//Process PointTranslationMap
	if(JSONRoot.isMember("PointTranslationMap"))
	{
		bool overriding = !!pConf->pPointMap;
		PointTranslationMap ptm;
		//Copy the contents of the existing map if there is one
		if(overriding)
		{
			for(const auto& [tid,pte] : *pConf->pPointMap)
			{
				ptm[tid].pCBORer = pte.pCBORer ? std::make_unique<CBORSerialiser>(std::move(*pte.pCBORer)) : nullptr;
				ptm[tid].pExtraFields = pte.pExtraFields ? std::make_unique<ExtraPointFields>(*pte.pExtraFields) : nullptr;
				ptm[tid].pKey = pte.pKey ? std::make_unique<odc::OctetStringBuffer>(std::move(*pte.pKey)) : nullptr;
				ptm[tid].pTemplate = pte.pTemplate ? std::make_unique<std::string>(std::move(*pte.pTemplate)) : nullptr;
				ptm[tid].pTopic = pte.pTopic ? std::make_unique<kafka::Topic>(std::move(*pte.pTopic)) : nullptr;
			}
		}

		auto& JSON_PTM = JSONRoot["PointTranslationMap"];
		for(auto EventTypeStr : JSON_PTM.getMemberNames())
		{
			auto eventType = EventTypeFromString(EventTypeStr);
			if(eventType == odc::EventType::AfterRange)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
				{
					log->error("Unknown EventType '{}' in PointTranslationMap. Ignoring.", EventTypeStr);
				}
				continue;
			}
			if(!JSON_PTM[EventTypeStr].isArray())
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
				{
					log->error("PointTranslationMap member '{}' should be an array. Ignoring.", EventTypeStr);
				}
				continue;
			}
			for(auto& entry : JSON_PTM[EventTypeStr])
			{
				size_t start, stop, inc=1;
				if(entry.isMember("Index"))
					start = stop = entry["Index"].asUInt();
				else if(entry.isMember("Range") && entry["Range"].isMember("Start") && entry["Range"].isMember("Stop"))
				{
					start = entry["Range"]["Start"].asUInt();
					stop = entry["Range"]["Stop"].asUInt();
					if(entry["Range"].isMember("Increment"))
						inc = entry["Range"]["Increment"].asUInt();
				}
				else
				{
					if(auto log = odc::spdlog_get("KafkaPort"))
						log->error("A PointTranslationMap entry needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : skipping '{}'", entry.toStyledString());
					continue;
				}
				for(size_t idx = start; idx <= stop; idx+=inc)
				{
					//Work out the TranslationID
					TranslationID tid; auto& [source,point_idx,evt_type] = tid;
					point_idx = idx; evt_type = eventType;
					if(entry.isMember("Source"))
						source = entry["Source"].asString();
					else
						source = "";

					bool existing_point = ptm.find(tid) != ptm.end();
					if(overriding && !existing_point && !pConf->OverridesCreateNewPTMEntries)
						continue;

					//Take a copy and update the existing entry, or create a new one
					PointTranslationEntry pte = existing_point ? std::move(ptm.at(tid)) : PointTranslationEntry();

					for(auto pte_member : entry.getMemberNames())
					{
						//Skip members that are already processed
						if(pte_member == "Index" || pte_member == "Range" || pte_member == "Source")
							continue;

						if(pte_member == "Topic")
							pte.pTopic = std::make_unique<kafka::Topic>(entry["Topic"].asString());
						else if(pte_member == "Key")
							pte.pKey = std::make_unique<odc::OctetStringBuffer>(entry["Key"].asString());
						else if(pte_member == "Template")
							pte.pTemplate = std::make_unique<std::string>(entry["Template"].asString());
						else if(pte_member == "CBORStructure")
						{
							try
							{
								pte.pCBORer = std::make_unique<CBORSerialiser>(entry["CBORStructure"].toStyledString());
							}
							catch(std::exception& e)
							{
								if(auto log = odc::spdlog_get("KafkaPort"))
									log->error("Failed to process 'CBORStructure': {}",e.what());
							}
						}
						else
						{
							if(!pte.pExtraFields)
								pte.pExtraFields = std::make_unique<ExtraPointFields>();
							pte.pExtraFields->insert_or_assign(pte_member, entry[pte_member].asString());
						}
					}
					ptm[tid] = std::move(pte);
				}
			}
		}
		pConf->pPointMap = std::make_unique<PointTranslationMap>(std::move(ptm));
	}
}
