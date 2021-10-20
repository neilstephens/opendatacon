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

#include <string>
#include <opendatacon/asio.h>
#include "connection.hpp"
#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {

	/// The top-level class of the HTTP server.
	class server
	{
	private:
		/// The io_context used to perform asynchronous operations.
		std::shared_ptr<odc::asio_service> pIOS;

		/// The strand used to synchronise access to the underlying asio service(s)
		std::unique_ptr<asio::io_service::strand> pServiceStrand;

	public:
		server(const server&) = delete;
		server& operator=(const server&) = delete;

		/// Construct the server to listen on the specified TCP address and port, and
		/// serve up files from the given directory.
		explicit server(std::shared_ptr<odc::asio_service> _pIOS, const std::string& _address, const std::string& _port);

		std::function<void()> start;
		std::function<void()> stop;

		void register_handler(const std::string& uripattern, pHandlerCallbackType handler)
		{
			request_handler_.register_handler(uripattern, handler);
		};
		size_t deregister_handler(const std::string& uripattern)
		{
			return request_handler_.deregister_handler(uripattern);
		};

	private:

		void start_();
		void stop_();

		std::function<void()> do_accept;
		/// Perform an asynchronous accept operation.
		void do_accept_();

		/// Acceptor used to listen for incoming connections.
		std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;

		/// The connection manager which owns all live connections.
		connection_manager connection_manager_;

		/// The handler for all incoming requests.
		request_handler request_handler_;
		std::string address;
		std::string port;
	};

} // namespace http

#endif // HTTP_SERVER_HPP
