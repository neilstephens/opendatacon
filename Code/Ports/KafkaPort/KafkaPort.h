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
 * KafkaPort.h
 *
 *  Created on: 09/07/2024
 *      Author: Neil Stephens
 */

//FIXME: all the kafka library calls need to be audited for possible exceptions and wrapped in try/catch/retry etc.

#ifndef KAFKAPORT_H
#define KAFKAPORT_H

#include "KafkaClientCache.h"
#include "KafkaPortConf.h"
#include <opendatacon/DataPort.h>
#include <atomic>

using namespace odc;

class KafkaPort: public DataPort
{
public:
	KafkaPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides);
	virtual ~KafkaPort(){};

	virtual void ProcessElements(const Json::Value& JSONRoot) override;
	virtual void Enable() override;
	virtual void Disable() override;

	virtual void Build() override = 0;
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

protected:
	std::atomic_bool enabled {false};
	std::shared_ptr<KafkaClientCache> pKafkaClientCache = KafkaClientCache::Get();
	template <class KafkaClientType> std::shared_ptr<KafkaClientType> Build(std::string TypeString = "")
	{
		auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

		if(!pConf->NativeKafkaProperties.contains("bootstrap.servers"))
		{
			pConf->NativeKafkaProperties.put("bootstrap.servers", "localhost:9092");
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("{}: bootstrap.servers property not found, defaulting to localhost:9092", Name);
		}

		if(pConf->NativeKafkaProperties.getProperty("enable.manual.events.poll") == "false")
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->warn("{}: enable.manual.events.poll property is set to false, forcing to true", Name);
		pConf->NativeKafkaProperties.put("enable.manual.events.poll", "true");

		pConf->NativeKafkaProperties.put("error_cb", [this](const kafka::Error& error)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->error("{}: {}",Name,error.toString());
			});

		pConf->NativeKafkaProperties.put("log_cb", [this](int level, const char* filename, int lineno, const char* msg)
			{
				auto spdlog_lvl = spdlog::level::level_enum(6-level);
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->log(spdlog_lvl,"{} ({}:{}): {}",Name,filename,lineno,msg);
			});

		pConf->NativeKafkaProperties.put("stats_cb", [this](const std::string& jsonString)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->info("{}: Statistics: {}",Name,jsonString);
			});

		if(pConf->ShareKafkaClient)
		{
			if(pConf->SharedKafkaClientKey == "")
			{
				auto bs_servers = pConf->NativeKafkaProperties.getProperty("bootstrap.servers").value();
				pConf->SharedKafkaClientKey = TypeString+":"+bs_servers;
			}

			return pKafkaClientCache->GetClient<KafkaClientType>(
				pConf->SharedKafkaClientKey,
				pConf->NativeKafkaProperties);
		}
		return pKafkaClientCache->GetClient<KafkaClientType>(Name, pConf->NativeKafkaProperties);
	}
};

#endif // KAFKAPORT_H