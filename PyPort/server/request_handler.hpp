//
// request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

#include <string>
#include <map>
#include <functional>
#include <memory>

namespace http 
{

		struct reply;
		struct request;

		typedef std::map<std::string, std::string> ParameterMapType;
		typedef std::function<void(const std::string& absoluteuri, reply& rep)> HandlerCallbackType;
		typedef std::shared_ptr<HandlerCallbackType> pHandlerCallbackType;

		/// The common handler for all incoming requests.
		class request_handler
		{
		public:
			// Non-copyable
			request_handler(const request_handler&) = delete;
			request_handler& operator=(const request_handler&) = delete;

			/// Construct with a directory containing files to be served.
			explicit request_handler();

			void register_handler(const std::string& uripattern, pHandlerCallbackType handler);

			/// Handle a request and produce a reply.
			void handle_request(const request& req, reply& rep);
			pHandlerCallbackType find_matching_handler(const std::string& uripattern);

		private:
			/// The directory containing the files to be served.
			std::map<std::string, pHandlerCallbackType> HandlerMap;

			/// Perform URL-decoding on a string. Returns false if the encoding was
			/// invalid.
			static bool url_decode(const std::string& in, std::string& out, std::string& query);
		};

} // namespace http

#endif // HTTP_REQUEST_HANDLER_HPP
