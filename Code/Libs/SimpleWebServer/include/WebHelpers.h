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
//
//  WebHelpers.h
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#ifndef __opendatacon__WebHelpers__
#define __opendatacon__WebHelpers__

#include <opendatacon/asio.h>
#include <opendatacon/util.h>
#include <opendatacon/ParamCollection.h>
#ifdef USE_HTTPS
#include <server_https.hpp>
using WebServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
#define OPTIONAL_CERTS web_crt, web_key
#else
#include <server_http.hpp>
using WebServer = SimpleWeb::Server<SimpleWeb::HTTP>;
#define OPTIONAL_CERTS
#endif

#include <memory>
#include <string>
#include <unordered_map>

inline const std::unordered_map<std::string, const std::string> MimeTypeMap {
	{ "json", "application/json" },
	{ "js", "text/javascript" },
	{ "html", "text/html"},
	{ "jpg", "image/jpeg"},
	{ "css", "text/css"},
	{ "txt", "text/plain"},
	{ "svg", "image/svg+xml"},
	{ "default", "application/octet-stream"}
};

inline const std::string& GetMimeType(const std::string& rUrl)
{
	auto last = rUrl.find_last_of("/\\.");
	if (last == std::string::npos) return MimeTypeMap.at("default");
	const std::string ext = rUrl.substr(last+1);

	if(MimeTypeMap.count(ext) != 0)
	{
		return MimeTypeMap.at(ext);
	}
	return MimeTypeMap.at("default");
}

//"address:port" of the peer that sent the request - "unknown" if it can't be determined
inline std::string RemoteEndpointString(const std::shared_ptr<WebServer::Request>& request)
{
	if(!request)
		return "unknown";
	try
	{
		const auto endpoint = request->remote_endpoint();
		return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
	}
	catch(const std::exception&)
	{
		return "unknown";
	}
}

//Single line summary of a request, for logging.
//Deliberately leaves out the request body and any credential bearing headers
//(Authorization, Proxy-Authorization, Cookie) so that logs can't leak secrets.
inline std::string RequestDetailString(const std::shared_ptr<WebServer::Request>& request)
{
	if(!request)
		return "<no request>";

	const auto header_or_dash = [&request](const char* const name) -> std::string
				    {
					    const auto it = request->header.find(name);
					    return it == request->header.end() ? "-" : it->second;
				    };

	std::string detail = "from " + RemoteEndpointString(request)
	                     + " HTTP/" + request->http_version
	                     + " " + request->method
	                     + " " + request->path;

	if(!request->query_string.empty())
		detail += "?" + request->query_string;

	return detail
	       + " Host:" + header_or_dash("Host")
	       + " X-Forwarded-For:" + header_or_dash("X-Forwarded-For")
	       + " User-Agent:" + header_or_dash("User-Agent")
	       + " Content-Type:" + header_or_dash("Content-Type")
	       + " Content-Length:" + header_or_dash("Content-Length");
}

inline void read_and_send(const std::shared_ptr<WebServer::Response> response, const std::shared_ptr<std::ifstream> ifs, const std::shared_ptr<std::vector<char>> buffer)
{
	std::streamsize read_length;
	if((read_length = ifs->read(buffer->data(), static_cast<std::streamsize>(buffer->size())).gcount()) > 0)
	{
		response->write(buffer->data(), read_length);
		if(read_length == static_cast<std::streamsize>(buffer->size()))
		{
			response->send([response, ifs, buffer](const SimpleWeb::error_code &ec)
				{
					if(!ec)
						read_and_send(response, ifs, buffer);
					else if (auto log = odc::spdlog_get("opendatacon"))
						log->error("Connection interrupted. SimpleWeb::error_code {}",ec);
				});
		}
	}
}

#endif /* defined(__opendatacon__WebHelpers__) */
