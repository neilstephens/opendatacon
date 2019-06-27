//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http
{
request_handler::request_handler()
{}

void request_handler::register_handler(const std::string& uripattern, pHandlerCallbackType handler)
{
	if (HandlerMap.find(uripattern) != HandlerMap.end())
	{
		LOGDEBUG("Trying to insert a handler twice {} ", uripattern);
		return;
	}
	HandlerMap.emplace(std::make_pair(uripattern, handler));
}

// Strip any parameters before handing to this method
// The matching here will need to be better in future...
// We only look for the first part of the HandlerMap key to match
pHandlerCallbackType request_handler::find_matching_handler(const std::string& uripattern)
{
	pHandlerCallbackType result = nullptr;
	size_t matchlength = 0;
	std::string matchkey;

	for (auto key : HandlerMap)
	{
		// The key match string should be at the start of the path.
		// We need to find the longest match.
		if ((uripattern.find(key.first) == 0) && (key.first.length() > matchlength))
		{
			matchlength = key.first.length();
			matchkey = key.first;
			result = key.second;
		}
	}
	if (matchlength == 0)
	{
		LOGDEBUG("Failed to find a Handler {} ", uripattern);
	}
	else
	{
		LOGDEBUG("Found Handler {} for {} ", matchkey, uripattern);
	}
	return result;
}

// We only need to do simple decoding, the first part of the url after the address will give us the port name, we look for this in a map
// once we find it call the registered method. What is returned is what we send back as the response.
// Remembering that we are going to pass this into and back from Python code, so a little different than if we were planning to do everything
void request_handler::handle_request(const request& req, reply& rep)
{
	LOGDEBUG("Request Method {} - Uri {}", req.method, req.uri);

	// Decode url to path.
	std::string request_path, request_params;
	if (!url_decode(req.uri, request_path, request_params))
	{
		rep = reply::stock_reply(reply::bad_request);
		LOGDEBUG("Bad Request");
		return;
	}
	LOGDEBUG("Path {} - Params {}", request_path, request_params);

	// Request path must be absolute and not contain "..".
	if (request_path.empty() || request_path[0] != '/' || request_path.find("..") != std::string::npos)
	{
		rep = reply::stock_reply(reply::bad_request);
		LOGDEBUG("Invalid Path/Url");
		return;
	}
	// ParameterMapType params;
	// Translate request_params into ParameterMapType??

	// Do the method/path matching
	if (auto fn = find_matching_handler(req.method + " " + request_path))
	{
		// Pass everything so it can be handled/passed to python
		// What about req.headers[] - a vector
		(*fn)(req.method + " " + req.uri, rep);
		return;
	}

	// If not found, return that
	rep = reply::stock_reply(reply::bad_request);
	LOGDEBUG("Matching Method/Url not found");
	return;
}

// Everything after the http://something:8000 and before the ?var1=value1&var2=value2&var3=value3 part (if it exists)
bool request_handler::url_decode(const std::string& in, std::string& out, std::string& params)
{
	out.clear();
	params.clear();

	out.reserve(in.size());
	params.reserve(in.size());
	bool inquery = false;

	std::string* p = &out;

	for (std::size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] == '%')
		{
			if (i + 3 <= in.size())
			{
				int value = 0;
				std::istringstream is(in.substr(i + 1, 2));
				if (is >> std::hex >> value)
				{
					*p += static_cast<char>(value);
					i += 2;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if (in[i] == '+')
		{
			*p += ' ';
		}
		else if (in[i] == '?')
		{
			inquery = true;
			p = &params;
		}
		else
			*p += in[i];
	}
	return true;
}
} // namespace http
