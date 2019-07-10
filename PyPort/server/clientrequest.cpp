//
// sync_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <sstream>
#include <string>
#include "../Py.h"
#include <asio.hpp>

using asio::ip::tcp;

bool DoHttpRequst(std::string server, std::string port, std::string path, std::string& callresp)
{
	std::stringstream sout;
	try
	{
		asio::io_context io_context;

		// Get a list of endpoints corresponding to the server name.
		tcp::resolver resolver(io_context);
		tcp::resolver::results_type endpoints = resolver.resolve(server, port);

		// Try each endpoint until we successfully establish a connection.
		tcp::socket socket(io_context);
		asio::connect(socket, endpoints);

		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		asio::streambuf request;
		std::ostream request_stream(&request);
		request_stream << "GET " << path << " HTTP/1.0\r\n";
		request_stream << "Host: " << path << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		asio::write(socket, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		asio::streambuf response;
		asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			callresp = "Invalid response";
			return false;
		}
		if (status_code != 200)
		{
			callresp = "Response returned with status code " + std::to_string(status_code);
			return false;
		}

		// Read the response headers, which are terminated by a blank line.
		asio::read_until(socket, response, "\r\n\r\n");

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
			sout << header << "\n";
		sout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
			sout << &response;

		// Read until EOF, writing data to output as we go.
		asio::error_code error;
		while (asio::read(socket, response,
			asio::transfer_at_least(1), error))
			sout << &response;
		if (error != asio::error::eof)
			throw asio::system_error(error);
	}
	catch (std::exception& e)
	{
		callresp = "Exception: " + std::string(e.what());
		return false;
	}
	callresp = sout.str();
	return true;
}