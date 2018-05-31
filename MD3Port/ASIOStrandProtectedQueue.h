#pragma once
#include <thread>
#include <future>
#include <functional>
#include <iostream>
#include <opendatacon/asio.h>
//#include <boost/bind.hpp>
//#include <boost/thread.hpp>

// Start of an asio protected queue. Async write, sync read.

class Foo
{
public:

	Foo(asio::io_service& io_service)
		: m_io_service(io_service),
		m_bar_strand(io_service)
	{}

public:

	void async_get_bar(std::function<void(int)> handler)
	{
		m_bar_strand.post(bind(&Foo::do_get_bar, this, handler));
	}

	void async_set_bar(int value, std::function<void()> handler)
	{
		m_bar_strand.post(bind(&Foo::do_set_bar, this, value, handler));
	}

	int bar()
	{
		typedef std::promise<int> promise_type;
		promise_type promise;

		// Pass the handler to async operation that will set the promise.
		void (promise_type::*setter)(const int&) = &promise_type::set_value;
		async_get_bar(std::bind(setter, &promise, _1));

		// Synchronously wait for promise to be fulfilled.
		return promise.get_future().get();
	}

	void bar(int value)
	{
		typedef std::promise<void> promise_type;
		promise_type promise;

		// Pass the handler to async operation that will set the promise.
		async_set_bar(value, std::bind(&promise_type::set_value, &promise));

		// Synchronously wait for the future to finish.
		promise.get_future().wait();
	}

private:

	void do_get_bar(std::function<void(int)> handler)
	{
		// This is only called from within the m_bar_strand, so we are safe.

		// Run the handler to notify the caller.
		handler(m_bar);
	}

	void do_set_bar(int value, std::function<void()> handler)
	{
		// This is only called from within the m_bar_strand, so we are safe.
		m_bar = value;

		// Run the handler to notify the caller.
		handler();
	}

	int m_bar;
	asio::io_service& m_io_service;
	asio::strand m_bar_strand;
};
/*
Exceptions can even be passed to future via promise::set_exception(), supporting an elegant way to provide exceptions or errors to the caller.
*/
int main2()
{
	asio::io_service io_service;
	asio::io_service::work work(io_service);
//	std::thread t(std::bind(&asio::io_service::run, std::ref(io_service)));

	Foo foo(io_service);
	foo.bar(21);
	std::cout << "foo.bar is " << foo.bar() << std::endl;
	foo.bar(2 * foo.bar());
	std::cout << "foo.bar is " << foo.bar() << std::endl;

	io_service.stop();
//	t.join();
}
