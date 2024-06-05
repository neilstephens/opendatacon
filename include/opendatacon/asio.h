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
 * asio.h
 *
 *  Created on: 2018-01-28
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ASIO_H
#define ASIO_H

#include <asio.hpp>
#include <unordered_set>

//use these to suppress warnings
typedef struct use_these_t
{
	const asio::error_category& use1 = asio::error::system_category;
	const asio::error_category& use2 = asio::error::netdb_category;
	const asio::error_category& use3 = asio::error::addrinfo_category;
	const asio::error_category& use4 = asio::error::misc_category;
}use_these_t;

namespace odc
{

//This thin wrapper/factory class for asio::io_service is important
//because it forces asio services to be created in the libODC memory
//space, avoiding problems that come from transferring ownership of objects
//across memory boundaries from dynamically loaded modules
class asio_service: private asio::io_service
{
public:
	using asio::io_service::poll;
	using asio::io_service::poll_one;
	using asio::io_service::run_one;
	using asio::io_service::run_one_for;
	using asio::io_service::post;
	using asio::io_service::dispatch;
	using asio::io_service::stopped;

	static std::shared_ptr<asio_service> Get(int concurrency_hint = std::thread::hardware_concurrency());

	std::unique_ptr<asio::io_service::work> make_work();
	std::unique_ptr<asio::io_service::strand> make_strand();
	std::unique_ptr<asio::steady_timer> make_steady_timer();
	std::unique_ptr<asio::steady_timer> make_steady_timer(std::chrono::steady_clock::duration t);
	std::unique_ptr<asio::steady_timer> make_steady_timer(std::chrono::steady_clock::time_point t);
	std::unique_ptr<asio::ip::tcp::resolver> make_tcp_resolver();
	std::unique_ptr<asio::ip::tcp::socket> make_tcp_socket();
	std::unique_ptr<asio::ip::tcp::acceptor> make_tcp_acceptor(const asio::ip::tcp::resolver::iterator& EndPoint);
	std::unique_ptr<asio::ip::tcp::acceptor> make_tcp_acceptor();
	std::unique_ptr<asio::ip::udp::resolver> make_udp_resolver();
	std::unique_ptr<asio::ip::udp::socket> make_udp_socket();
	void run();
	bool current_thread_in_pool();

	inline int GetConcurrency(){return concurrency;}

private:
	asio_service():
		asio::io_service()
	{}
	asio_service(int concurrency_hint):
		asio::io_service(concurrency_hint)
	{}

	int concurrency;
	asio::io_service* const unwrap_this = static_cast<asio::io_service*>(this);
	static std::mutex threads_in_pool_mtx;
	static std::unordered_set<std::thread::id> threads_in_pool;
};

//buffer to track a data container
class shared_const_buffer: public asio::const_buffer
{
public:
	template <typename T> //T must be a container with a data(), size() and get_allocator() members
	shared_const_buffer(std::shared_ptr<T> pCon):
		asio::const_buffer(pCon->data(),pCon->size()*sizeof(pCon->get_allocator())),
		con(pCon)
	{}
	//Implement the ConstBufferSequence requirements
	//(so I don't have to put a single one in a container)
	typedef const shared_const_buffer* const_iterator;
	const_iterator begin() const
	{
		return this;
	}
	const_iterator end() const
	{
		return this + 1;
	}

private:
	std::shared_ptr<void> con;
};

} //namespace odc

#endif // ASIO_H

