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
//  MhdWrapper.cpp
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#include "WebHelpers.h"
#include "Log.h"

const std::unordered_map<std::string, const std::string> MimeTypeMap {
	{ "json", "application/json" },
	{ "js", "text/javascript" },
	{ "html", "text/html"},
	{ "jpg", "image/jpeg"},
	{ "css", "text/css"},
	{ "txt", "text/plain"},
	{ "svg", "image/svg+xml"},
	{ "default", "application/octet-stream"}
};

const std::string& GetMimeType(const std::string& rUrl)
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

void read_and_send(const std::shared_ptr<WebServer::Response> response, const std::shared_ptr<std::ifstream> ifs, const std::shared_ptr<std::vector<char>> buffer)
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
					else
						Log.Error("Connection interrupted");
				});
		}
	}
}
