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
#include <signal.h>
#include <utility>

namespace http
{


server::server(std::shared_ptr<odc::asio_service> _pIOS, const std::string& address, const std::string& port)
	: pIOS(_pIOS),
	acceptor_(nullptr),
	connection_manager_(),
	request_handler_()
{
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	auto resolver = pIOS->make_tcp_resolver();
	acceptor_ = pIOS->make_tcp_acceptor(resolver->resolve(address, port));
	acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_->listen();

	do_accept();
}


void server::do_accept()
{
	acceptor_->async_accept(
		[this](std::error_code ec, asio::ip::tcp::socket socket)
		{
			// Check whether the server was stopped by a signal before this
			// completion handler had a chance to run.
			if (!acceptor_->is_open())
			{
			      return;
			}

			if (!ec)
			{
			      connection_manager_.start(std::make_shared<connection>(
					std::move(socket), connection_manager_, request_handler_));
			}

			do_accept();
		});
}

} // namespace http
