#include <memory>
#include <unordered_map>
#include <string>
#include <kafka/KafkaClient.h>

class KafkaClientCache
{
private:
	KafkaClientCache() = default;
	~KafkaClientCache() = default;

public:

	template<typename ClientType>
	std::shared_ptr<ClientType> GetClient(const std::string& client_key, const kafka::Properties& properties)
	{
		if(auto client = clients[client_key].lock())
			return std::static_pointer_cast<ClientType>(client);

		auto new_client = std::make_shared<ClientType>(properties);
		clients[client_key] = new_client;
		return new_client;
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
};