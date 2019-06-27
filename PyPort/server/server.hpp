//
// server.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "../Py.h"
#include <string>
#include "connection.hpp"
#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {

	/// The top-level class of the HTTP server.
	class server
	{
	public:
		server(const server&) = delete;
		server& operator=(const server&) = delete;

		/// Construct the server to listen on the specified TCP address and port, and
		/// serve up files from the given directory.
		explicit server(std::shared_ptr<asio::io_context> _pIOS, const std::string& address, const std::string& port);

		void register_handler(const std::string& uripattern, pHandlerCallbackType handler)
		{
			request_handler_.register_handler(uripattern, handler);
		};

	private:
		/// Perform an asynchronous accept operation.
		void do_accept();

		/// Wait for a request to stop the server.
		void do_await_stop();

		/// The io_context used to perform asynchronous operations.
		std::shared_ptr<asio::io_context> pIOS;

		/// The signal_set is used to register for process termination notifications.
		asio::signal_set signals_;

		/// Acceptor used to listen for incoming connections.
		asio::ip::tcp::acceptor acceptor_;

		/// The connection manager which owns all live connections.
		connection_manager connection_manager_;

		/// The handler for all incoming requests.
		request_handler request_handler_;
	};

} // namespace http

#endif // HTTP_SERVER_HPP
