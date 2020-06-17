//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"
#include <utility>

namespace http
{


server::server(std::shared_ptr<odc::asio_service> _pIOS, const std::string& _address, const std::string& _port)
	: pIOS(std::move(_pIOS)),
	acceptor_(nullptr),
	connection_manager_(),
	request_handler_(),
	address(_address),
	port(_port)
{
	acceptor_ = pIOS->make_tcp_acceptor();
	acceptor_->close();
}

void server::start()
{
	if (acceptor_->is_open())
		return;

	auto resolver = pIOS->make_tcp_resolver();
	asio::ip::tcp::endpoint endpoint = *resolver->resolve(address, port).begin(); // Only use the first resolved address - should only be one!
	acceptor_->open(endpoint.protocol());
	acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_->bind(endpoint);
	acceptor_->listen();
	do_accept();
}

void server::stop()
{
	// The server is stopped by cancelling all outstanding asynchronous
	// operations. Once all operations have finished the io_context::run() call will exit.
	// Does not matter that this could get called on a closed acceptor - it is just ignored in that case.
	acceptor_->close();
	connection_manager_.stop_all();
}

void server::do_accept()
{
	acceptor_->async_accept(
		[this](std::error_code ec, asio::ip::tcp::socket socket)
		{
			// Check whether the server was stopped by a signal before this
			// completion handler had a chance to run.
			if (!acceptor_->is_open())
				return;

			if (!ec)
				connection_manager_.start(std::make_shared<connection>(std::move(socket), connection_manager_, request_handler_));

			do_accept();
		});
}

} // namespace http
