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
 * asio.cpp
 *
 *  Created on: 11/07/2019
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/asio.h>

//compile asio only in libODC
//ASIO_SEPARATE_COMPILATION lets other modules link to it
#include <asio/impl/src.hpp>
#ifdef ODC_ASIO_SSL
#include <asio/ssl/impl/src.hpp>
#endif

namespace odc
{

strand_t::~strand_t() = default;
work_guard::~work_guard() = default;

/*
 * Kind of singleton getter:
 * manages the lifetime of a single shared resource using smart pointers and atomic_flag
 * avoids the perils of non-trivial statics that a typical singleton pattern uses
 */
std::shared_ptr<asio_service> asio_service::Get(int concurrency_hint)
{
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<asio_service> weak_service;

	std::shared_ptr<asio_service> shared_service; //this is what we'll return

	//if the init flag isn't set, we need to initialise the service
	if(!init_flag.test_and_set(std::memory_order_acquire))
	{
		if(concurrency_hint < 1)
			concurrency_hint = 1;

		//make a custom deleter that will also clear the init flag
		auto deinit_del = [](asio_service* service_ptr)
					{init_flag.clear(std::memory_order_release); delete service_ptr;};
		shared_service = std::shared_ptr<asio_service>(new asio_service(concurrency_hint+2), deinit_del);
		weak_service = shared_service;
		shared_service->concurrency = concurrency_hint;
	}
	//otherwise just make sure it's finished initialising and take a shared_ptr
	else
	{
		while (!(shared_service = weak_service.lock()))
			std::this_thread::yield(); //init happens very seldom, so spin lock is good
	}

	return shared_service;
}

std::unique_ptr<work_guard, deleter> asio_service::make_work()
{
	return std::unique_ptr<work_guard, deleter>(new work_guard(io));
}
std::unique_ptr<strand_t, deleter> asio_service::make_strand()
{
	return std::unique_ptr<strand_t, deleter>(new strand_t(io));
}
std::unique_ptr<steady_timer, deleter> asio_service::make_steady_timer()
{
	return std::unique_ptr<steady_timer, deleter>(new steady_timer(io));
}
std::unique_ptr<steady_timer, deleter> asio_service::make_steady_timer(std::chrono::steady_clock::duration t)
{
	return std::unique_ptr<steady_timer, deleter>(new steady_timer(io, t));
}
std::unique_ptr<steady_timer, deleter> asio_service::make_steady_timer(std::chrono::steady_clock::time_point t)
{
	return std::unique_ptr<steady_timer, deleter>(new steady_timer(io, t));
}
std::unique_ptr<tcp::resolver, deleter> asio_service::make_tcp_resolver()
{
	return std::unique_ptr<tcp::resolver, deleter>(new tcp::resolver(io));
}
std::unique_ptr<tcp::socket, deleter> asio_service::make_tcp_socket()
{
	return std::unique_ptr<tcp::socket, deleter>(new tcp::socket(io));
}
std::unique_ptr<tcp::acceptor, deleter> asio_service::make_tcp_acceptor(const tcp::endpoint& endpoint)
{
	return std::unique_ptr<tcp::acceptor, deleter>(new tcp::acceptor(io, endpoint));
}
std::unique_ptr<tcp::acceptor, deleter> asio_service::make_tcp_acceptor()
{
	return std::unique_ptr<tcp::acceptor, deleter>(new tcp::acceptor(io));
}
std::unique_ptr<udp::resolver, deleter> asio_service::make_udp_resolver()
{
	return std::unique_ptr<udp::resolver, deleter>(new udp::resolver(io));
}
std::unique_ptr<udp::socket, deleter> asio_service::make_udp_socket()
{
	return std::unique_ptr<udp::socket, deleter>(new udp::socket(io));
}
std::unordered_set<std::thread::id> asio_service::threads_in_pool;
std::mutex asio_service::threads_in_pool_mtx;
void asio_service::run()
{
	{ //lock scope
		std::lock_guard lock(threads_in_pool_mtx);
		threads_in_pool.insert(std::this_thread::get_id());
	}
	io.run();
}
bool asio_service::current_thread_in_pool()
{
	std::lock_guard lock(threads_in_pool_mtx);
	return (threads_in_pool.find(std::this_thread::get_id()) != threads_in_pool.end());
}

} //namespace odc
