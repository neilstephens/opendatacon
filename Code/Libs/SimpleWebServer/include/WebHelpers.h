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
