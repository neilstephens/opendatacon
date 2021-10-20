//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"
#include "request_handler.hpp"
#include <opendatacon/ODCLogMacros.h>
#include <fstream>
#include <sstream>
#include <string>

namespace http
{
request_handler::request_handler()
{}

void request_handler::register_handler(const std::string& uripattern, const pHandlerCallbackType& handler)
{
	if (HandlerMap.find(uripattern) != HandlerMap.end())
	{
		LOGDEBUG("Trying to insert a handler twice {} ", uripattern);
		return;
	}
	HandlerMap.emplace(std::make_pair(uripattern, handler));
}
size_t request_handler::deregister_handler(const std::string& uripattern)
{
	if (HandlerMap.find(uripattern) != HandlerMap.end())
	{
		LOGDEBUG("Removing a http handler {} ", uripattern);
		HandlerMap.erase(uripattern);
		return HandlerMap.size();
	}
	else
	{
		LOGDEBUG("Trying to remove a http handler that does not exist {} ", uripattern);
		return HandlerMap.size();
	}
}

// Strip any parameters before handing to this method
// The matching here will need to be better in future...
// We only look for the first part of the HandlerMap key to match
pHandlerCallbackType request_handler::find_matching_handler(const std::string& uripattern)
{
	pHandlerCallbackType result = nullptr;
	size_t matchlength = 0;
	std::string matchkey;

	for (const auto& key : HandlerMap)
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
		LOGTRACE("Found Handler {} for {}", matchkey, uripattern);
	}
	return result;
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

ParameterMapType request_handler::SplitParams(std::string &paramstring)
{
	// Split into a map of keys and values.
	ParameterMapType p;
	//LOGDEBUG("Splitting Params {}", paramstring);
	std::vector<std::string> tokens = split(paramstring, '&');

	for (auto token : tokens)
	{
		std::vector<std::string>keyval = split(token, '=');
		if (keyval.size() == 2)
		{
			std::string key = keyval[0];
			std::string value = keyval[1];
			p[key] = value;
		}
	}
	return p;
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
	//LOGDEBUG("Path {} - Params {}", request_path, request_params);

	// Request path must be absolute and not contain "..".
	if (request_path.empty() || request_path[0] != '/' || request_path.find("..") != std::string::npos)
	{
		rep = reply::stock_reply(reply::bad_request);
		LOGDEBUG("Invalid Path/Url");
		return;
	}

	// Do the method/path matching
	if (auto fn = find_matching_handler(req.method + " " + request_path))
	{
		// Pass everything so it can be handled/passed to python
		// What about req.headers[] - a vector
		ParameterMapType params = SplitParams(request_params);
		(*fn)(req.method + " " + req.uri, params, req.content, rep);
		return;
	}

	// If not found, return that
	rep = reply::stock_reply(reply::bad_request);
	LOGDEBUG("Matching Method/Url not found");
	return;
}

// Everything after the http://something:10000 and before the ?var1=value1&var2=value2&var3=value3 part (if it exists)
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
		else if ((in[i] == '?') && !inquery)
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
