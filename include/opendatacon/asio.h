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

#ifdef ODC_ASIO_GUARD
#undef ASIO_HPP
#endif
#include <asio.hpp>
#include <unordered_set>
#include <memory>
#include <tuple>

//use these to suppress warnings
//FIXME: asio 1.36+ removed steady_timer::expires_from_now().
//  Short-term macro so port code keeps `pTimer->expires_from_now(ms)` unchanged
//  while feature merges are pending. Remove once merge traffic settles.
#define expires_from_now expires_after

typedef struct use_these_t
{
	const asio::error_category& use1 = asio::error::system_category;
	const asio::error_category& use2 = asio::error::netdb_category;
	const asio::error_category& use3 = asio::error::addrinfo_category;
	const asio::error_category& use4 = asio::error::misc_category;
}use_these_t;

namespace odc
{

struct deleter
{
	template<typename T>
	void operator()(T* p) const noexcept
	{
		delete p;
	}
};

// strand_wrap_t<F>: result type of strand_t::wrap(f).
//
// Restores the dispatch-on-direct-call semantics of the old asio::strand::wrap()
// that was removed in asio 1.32.0.  asio::bind_executor (its replacement) is
// correct for async completion-handler use but does NOT dispatch when operator()
// is invoked directly; this struct does.
//
// get_executor() is provided so asio can still deliver async completions on the
// correct strand without an extra dispatch hop.
//
// Each operator() call copies handler_ into the dispatch lambda (multi-shot).
// asio::dispatch runs the handler immediately when called from within the strand,
// otherwise queues it — exactly the "already on strand" optimisation the old
// wrapped_handler relied on.
template<typename F>
struct strand_wrap_t
{
	asio::io_context::strand strand_;
	F handler_;

	asio::io_context::strand get_executor() const noexcept { return strand_; }

	template<typename ... Args>
	void operator()(Args&&... args) const
	{
		asio::dispatch(strand_,
			[h=handler_, t=std::make_tuple(std::forward<Args>(args)...)]() mutable
			{
				std::apply(std::move(h), std::move(t));
			});
	}
};

//Thin wrapper that preserves the ->post(f), ->dispatch(f), ->wrap(f) syntax
//removed from asio::io_context::strand in asio 1.32.0.
class strand_t
{
	asio::io_context::strand strand_;
public:
	~strand_t();
	strand_t(strand_t&&) = default;
	strand_t& operator=(strand_t&&) = delete;

	template<typename F> void post(F&& f)                { asio::post(strand_, std::forward<F>(f)); }
	template<typename F> void dispatch(F&& f)            { asio::dispatch(strand_, std::forward<F>(f)); }
	template<typename F> auto wrap(F&& f)                { return strand_wrap_t<std::decay_t<F>>{strand_, std::forward<F>(f)}; }
	bool running_in_this_thread() const { return strand_.running_in_this_thread(); }

private:
	friend class asio_service;
	strand_t(asio::io_context& io): strand_(io) {}
};

class work_guard
{
	asio::executor_work_guard<asio::io_context::executor_type> guard_;
public:
	~work_guard();
	work_guard(work_guard&&) = default;
	work_guard& operator=(work_guard&&) = delete;
private:
	friend class asio_service;
	work_guard(asio::io_context& io): guard_(io.get_executor()) {}
};

//Typedefs for asio types with stable APIs
using steady_timer = asio::steady_timer;

namespace tcp
{
using socket = asio::ip::tcp::socket;
using acceptor = asio::ip::tcp::acceptor;
using resolver = asio::ip::tcp::resolver;
using endpoint = asio::ip::tcp::endpoint;
}

namespace udp
{
using socket = asio::ip::udp::socket;
using resolver = asio::ip::udp::resolver;
using endpoint = asio::ip::udp::endpoint;
}

//This thin wrapper/factory class for asio::io_context is important
//because it forces asio services to be created in the libODC memory
//space, avoiding problems that come from transferring ownership of objects
//across memory boundaries from dynamically loaded modules
class asio_service
{
public:
	static std::shared_ptr<asio_service> Get(int concurrency_hint = std::thread::hardware_concurrency());

	void run();
	bool stopped() { return io.stopped(); }
	std::size_t poll() { return io.poll(); }
	std::size_t poll_one() { return io.poll_one(); }
	std::size_t run_one() { return io.run_one(); }
	template<typename R> std::size_t run_one_for(R&& d) { return io.run_one_for(std::forward<R>(d)); }
	template<typename R> std::size_t run_for(R&& d) { return io.run_for(std::forward<R>(d)); }

	template<typename ... Args> auto co_spawn(Args&&... args) { return asio::co_spawn(io,std::forward<Args>(args)...); }

	template<typename F> void post(F&& f)        { asio::post(io, std::forward<F>(f)); }
	template<typename F> void dispatch(F&& f)    { asio::dispatch(io, std::forward<F>(f)); }

	bool current_thread_in_pool();
	int GetConcurrency() { return concurrency; }

	std::unique_ptr<strand_t, deleter> make_strand();
	std::unique_ptr<work_guard, deleter> make_work();
	std::unique_ptr<steady_timer, deleter> make_steady_timer();
	std::unique_ptr<steady_timer, deleter> make_steady_timer(std::chrono::steady_clock::duration t);
	std::unique_ptr<steady_timer, deleter> make_steady_timer(std::chrono::steady_clock::time_point t);
	std::unique_ptr<tcp::resolver, deleter> make_tcp_resolver();
	std::unique_ptr<tcp::socket, deleter> make_tcp_socket();
	std::unique_ptr<tcp::acceptor, deleter> make_tcp_acceptor(const tcp::endpoint& endpoint);
	std::unique_ptr<tcp::acceptor, deleter> make_tcp_acceptor();
	std::unique_ptr<udp::resolver, deleter> make_udp_resolver();
	std::unique_ptr<udp::socket, deleter> make_udp_socket();

private:
	asio_service():
		io(std::thread::hardware_concurrency())
	{}
	asio_service(int concurrency_hint):
		io(concurrency_hint)
	{}

	int concurrency;
	asio::io_context io;
	static std::mutex threads_in_pool_mtx;
	static std::unordered_set<std::thread::id> threads_in_pool;
};

//buffer to track a data container
class shared_const_buffer: public asio::const_buffer
{
public:
	template <typename T> //T must be a container with a data(), size() and get_allocator() members
	shared_const_buffer(std::shared_ptr<T> pCon):
		asio::const_buffer(pCon->data(),pCon->size()*sizeof(typename decltype(pCon->get_allocator())::value_type)),
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
