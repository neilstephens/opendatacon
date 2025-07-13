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
	KafkaPort(const std::string& Name, const std::string& Filename, const Json::Value& Overrides, const bool isProducer);
	virtual ~KafkaPort(){};

	virtual void ProcessElements(const Json::Value& JSONRoot) override;
	virtual void Enable() override final;
	virtual void Disable() override final;

	virtual void PortUp() = 0;
	virtual void PortDown() = 0;
	virtual void Build() override = 0;
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

private:
	std::unique_ptr<asio::io_service::strand> pStateSync = odc::asio_service::Get()->make_strand();

protected:
	std::atomic_bool enabled {false};
	std::shared_ptr<KafkaClientCache> pKafkaClientCache = KafkaClientCache::Get();
	std::unordered_map<std::string,std::unique_ptr<EventDB>> pDBs;
	void ConnectionEvent(std::shared_ptr<const EventInfo> event, SharedStatusCallback_t pStatusCallback);
	template <class KafkaClientType> std::shared_ptr<KafkaClientType> Build(std::string TypeString = "") //TODO: make TypeString an enum with to_string helper
	{
		auto pConf = static_cast<KafkaPortConf*>(this->pConf.get());

		if(!pConf->NativeKafkaProperties.contains("bootstrap.servers"))
		{
			pConf->NativeKafkaProperties.put("bootstrap.servers", "localhost:9092");
			Log.Error("{}: bootstrap.servers property not found, defaulting to localhost:9092", Name);
		}

		if(pConf->NativeKafkaProperties.getProperty("enable.manual.events.poll") == "false")
			Log.Warn("{}: enable.manual.events.poll property is set to false, forcing to true", Name);
		pConf->NativeKafkaProperties.put("enable.manual.events.poll", "true");

		auto LogEntryName = Name;
		if(pConf->ShareKafkaClient)
		{
			if(pConf->SharedKafkaClientKey == "")
			{
				auto bs_servers = pConf->NativeKafkaProperties.getProperty("bootstrap.servers").value();
				pConf->SharedKafkaClientKey = TypeString+":"+bs_servers;
			}
			LogEntryName = pConf->SharedKafkaClientKey;
		}

		pConf->NativeKafkaProperties.put("error_cb", [LogEntryName](const kafka::Error& error)
			{
				Log.Error("{}: {}",LogEntryName,error.toString());
			});

		pConf->NativeKafkaProperties.put("log_cb", [LogEntryName](int level, const char* filename, int lineno, const char* msg)
			{
				auto spdlog_lvl = spdlog::level::level_enum(6-level);
				if(auto log = Log.GetLog())
					log->log(spdlog_lvl,"{} ({}:{}): {}",LogEntryName,filename,lineno,msg);
			});

		pConf->NativeKafkaProperties.put("stats_cb", [LogEntryName](const std::string& jsonString)
			{
				Log.Info("{}: Statistics: {}",LogEntryName,jsonString);
			});


		return pKafkaClientCache->GetClient<KafkaClientType>(
			pConf->ShareKafkaClient ? pConf->SharedKafkaClientKey : Name,
			pConf->NativeKafkaProperties,
			pConf->MaxPollIntervalms);
	}
};

#endif // KAFKAPORT_H
