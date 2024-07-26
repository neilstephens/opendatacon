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
 * KafkaClientCache.h
 *
 *  Created on: 19/07/2024
 *      Author: Neil Stephens
 */

#ifndef KAFKACLIENTCACHE_H
#define KAFKACLIENTCACHE_H

#include <kafka/KafkaClient.h>
#include <opendatacon/asio.h>
#include <opendatacon/util.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

class KafkaClientCache
{
private:
	// Private constructor and destructor to ensure the cache is only created and destroyed by the singleton pattern
	KafkaClientCache() = default;
	~KafkaClientCache() = default;

	// Polling function that will recall itself every MaxPollIntervalms until the client is destroyed or the timer is cancelled/destroyed
	static void Poll(std::weak_ptr<kafka::clients::KafkaClient> weak_client, std::weak_ptr<asio::steady_timer> weak_timer, const size_t MaxPollIntervalms)
	{
		auto client = weak_client.lock();
		if(!client) return;
		auto pTimer = weak_timer.lock();
		if(!pTimer) return;

		client->pollEvents(std::chrono::milliseconds::zero());

		pTimer->expires_from_now(std::chrono::milliseconds(MaxPollIntervalms));
		pTimer->async_wait([weak_client,weak_timer,MaxPollIntervalms](asio::error_code err)
			{
				if(err) return;
				Poll(weak_client, weak_timer, MaxPollIntervalms);
			});
	}

	//Called every time a client is retrieved, to ensure the polling timer is set to the correct value
	void MaxPollTime(const std::string& client_key, const size_t MaxPollIntervalms)
	{
		//we know client exists and mutex is locked, because this is only called from GetClient
		if(MaxPollIntervalms < poll_timers[client_key].first)
		{
			//reassigning the timer pointer will cancel+destroy the old timer, and expire weak pointer in the old polling handler
			poll_timers[client_key].second = odc::asio_service::Get()->make_steady_timer(std::chrono::milliseconds(MaxPollIntervalms));
			poll_timers[client_key].first = MaxPollIntervalms;
			Poll(clients[client_key], poll_timers[client_key].second, MaxPollIntervalms);
		}
	}

	// Clean up any expired clients
	void Clean()
	{
		//we know mutex is locked, because this is only called from GetClient
		std::set<std::string> to_delete;
		for(auto& client : clients)
		{
			if(client.second.expired())
				to_delete.insert(client.first);
		}
		for(auto& key : to_delete)
		{
			clients.erase(key);
			poll_timers.erase(key);
		}
	}

public:

	// Factory method to get a client from the cache, or create a new one if it doesn't exist
	template<typename ClientType>
	std::shared_ptr<ClientType> GetClient(const std::string& client_key, const kafka::Properties& properties, const size_t MaxPollIntervalms = std::numeric_limits<size_t>::max())
	{
		std::lock_guard lock(mtx);

		if(auto client = clients[client_key].lock())
		{
			MaxPollTime(client_key, MaxPollIntervalms);
			return std::static_pointer_cast<ClientType>(client);
		}

		try
		{
			auto new_client = std::make_shared<ClientType>(properties);
			clients[client_key] = new_client;
			poll_timers[client_key] = {std::numeric_limits<size_t>::max(),nullptr};
			MaxPollTime(client_key, MaxPollIntervalms);
			Clean();
			return new_client;
		}
		catch(const kafka::KafkaException& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("{}: Failed to create KafkaClient: {}", client_key, e.what());
			return nullptr;
		}
	}

	// Singleton pattern to manage the cache
	// Only one instance of the cache will exist at any one time
	// but it will be destroyed as soon as the weak_ptr expires
	static std::shared_ptr<KafkaClientCache> Get()
	{
		static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
		static std::weak_ptr<KafkaClientCache> weak_cache;

		std::shared_ptr<KafkaClientCache> shared_cache; //this is what we'll return

		//if the init flag isn't set, we need to initialise the service
		if(!init_flag.test_and_set(std::memory_order_acquire))
		{
			//make a custom deleter that will also clear the init flag
			auto deinit_del = [](KafkaClientCache* cache_ptr)
						{init_flag.clear(std::memory_order_release); delete cache_ptr;};
			shared_cache = std::shared_ptr<KafkaClientCache>(new KafkaClientCache(), deinit_del);
			weak_cache = shared_cache;
		}
		//otherwise just make sure it's finished initialising and take a shared_ptr
		else
		{
			while (!(shared_cache = weak_cache.lock()))
			{} //init happens very seldom, so spin lock is good
		}

		return shared_cache;
	}

private:
	std::mutex mtx;
	std::unordered_map<std::string, std::weak_ptr<kafka::clients::KafkaClient>> clients;
	std::unordered_map<std::string, std::pair<size_t,std::shared_ptr<asio::steady_timer>>> poll_timers;
};

#endif // KAFKACLIENTCACHE_H