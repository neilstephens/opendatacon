#include <kafka/KafkaClient.h>
#include <opendatacon/asio.h>
#include <memory>
#include <unordered_map>
#include <string>

class KafkaClientCache
{
private:
	KafkaClientCache() = default;
	~KafkaClientCache() = default;

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

public:

	template<typename ClientType>
	std::shared_ptr<ClientType> GetClient(const std::string& client_key, const kafka::Properties& properties)
	{
		if(auto client = clients[client_key].lock())
			return std::static_pointer_cast<ClientType>(client);

		auto new_client = std::make_shared<ClientType>(properties);
		clients[client_key] = new_client;
		poll_timers[client_key] = {std::numeric_limits<size_t>::max(),nullptr};
		return new_client;
	}

	void MaxPollTime(const std::string& client_key, const size_t MaxPollIntervalms)
	{
		if(clients.find(client_key) == clients.end())
			return;

		auto client = clients[client_key].lock();
		if(!client)
			return;

		if(MaxPollIntervalms < poll_timers[client_key].first)
		{
			//cancel the existing timer, and reassign the pointer (which will expire weak pointer in handler)
			if(auto pTimer = poll_timers[client_key].second)
				pTimer->cancel();
			poll_timers[client_key].second = odc::asio_service::Get()->make_steady_timer(std::chrono::milliseconds(MaxPollIntervalms));
			poll_timers[client_key].first = MaxPollIntervalms;

			Poll(client, poll_timers[client_key].second, MaxPollIntervalms);
		}
	}

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
	std::unordered_map<std::string, std::weak_ptr<kafka::clients::KafkaClient>> clients;
	std::unordered_map<std::string, std::pair<size_t,std::shared_ptr<asio::steady_timer>>> poll_timers;
};