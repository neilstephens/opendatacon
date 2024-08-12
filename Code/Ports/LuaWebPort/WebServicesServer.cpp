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
//  WebUI.cpp
//  opendatacon
//
//  Created by Alan Murray on 06/09/2014.
//
//

#include "WebServicesServer.h"
#include <opendatacon/util.h>
#include <opendatacon/asio.h>
#define pass (void)0

WebUI::WebUI(uint16_t pPort, const std::string& web_crt, const std::string& web_key):
	WebSrv(OPTIONAL_CERTS),
	port(pPort)
{}

// Can SimpleWebServer do this for us?
void WebUI::LoadRequestParams(std::shared_ptr<WebServer::Request> request)
{
	params.clear();
	if(request->method == "POST" && request->content.size() > 0)
	{
		auto type_pair_it = request->header.find("Content-Type");
		if(type_pair_it != request->header.end())
		{
			if(type_pair_it->second.find("application/x-www-form-urlencoded") != type_pair_it->second.npos)
			{
				auto content = SimpleWeb::QueryString::parse(request->content.string());
				for(auto& content_pair : content)
					params[content_pair.first] = content_pair.second;
			}
			else if(type_pair_it->second.find("application/json") != type_pair_it->second.npos)
			{
				Json::CharReaderBuilder JSONReader;
				std::string err_str;
				Json::Value Payload;
				bool parse_success = Json::parseFromStream(JSONReader,request->content, &Payload, &err_str);
				if (parse_success)
				{
					try
					{
						if(Payload.isObject())
						{
							for(auto& membername : Payload.getMemberNames())
								params[membername] = Payload[membername].asString();
						}
						else
							throw std::runtime_error("Payload isn't object");
					}
					catch(std::exception& e)
					{
						if(auto log = odc::spdlog_get("WebUI"))
							log->error("JSON POST paylod not suitable: {} : '{}'",e.what(),Payload.toStyledString());
					}
				}
				else if(auto log = odc::spdlog_get("WebUI"))
					log->error("Failed to parse POST payload as JSON: {}",err_str);
			}
			else if(auto log = odc::spdlog_get("WebUI"))
				log->error("unsupported POST 'Content-Type' : '{}'", type_pair_it->second);
		}
		else if(auto log = odc::spdlog_get("WebUI"))
			log->error("POST has no 'Content-Type'");
	}
}

void WebUI::DefaultRequestHandler(std::shared_ptr<WebServer::Response> response,
	std::shared_ptr<WebServer::Request> request)
{
	LoadRequestParams(request);

	auto raw_path = SimpleWeb::Percent::decode(request->path);
	if (IsCommand(raw_path))
	{
		HandleCommand(raw_path,[response](const Json::Value&& json_resp)
			{
				SimpleWeb::CaseInsensitiveMultimap header;
				header.emplace("Content-Type", "application/json");

				Json::StreamWriterBuilder wbuilder;
				wbuilder["commentStyle"] = "None";
				auto str_resp = Json::writeString(wbuilder, json_resp);

				response->write(str_resp,header);
			});
	}
}

void WebUI::Build()
{
	WebSrv.config.port = port;

	//make a handler that simply posts the work and returns
	// we don't want the web server thread doing any actual work
	// because ODC needs full control via the main thread pool
	auto request_handler = [this](std::shared_ptr<WebServer::Response> response,
	                              std::shared_ptr<WebServer::Request> request)
				     {
					     pIOS->post([this,response,request](){DefaultRequestHandler(response,request);});
				     };

	//TODO: we could use non-default resources to regex match the URL
	// then we could get rid of the URL parsing code in the handler
	// in favour of the regex match groups, and post the ultimate handlers directly
	WebSrv.default_resource["GET"] = request_handler;
	WebSrv.default_resource["POST"] = request_handler;
}

void WebUI::Enable()
{
	// Starts a thread for the webserver to process stuff on. See comment below.
	std::thread server_thread([this]()
		{
			// Start server
			WebSrv.start([](unsigned short port)
				{
					if (auto log = odc::spdlog_get("WebUI"))
						log->info("Simple Web Server listening on port {}.",port);
				});
		});

	//TODO/FIXME: make thread a member, so we can join it on disable/shutdown
	//detaching not so bad for the moment
	server_thread.detach();
}

void WebUI::Disable()
{
	WebSrv.stop();
}

void WebUI::HandleCommand(const std::string& url, std::function<void (const Json::Value&&)> result_cb)
{
	std::stringstream iss;
	std::string responder;
	std::string command;
	ParseURL(url, responder, command, iss);

	if(Responders.find(responder) != Responders.end())
	{
		//ExecuteCommand(Responders[responder], command, iss, result_cb);
		pass;
	}
	else
	{
		Json::Value value;
		value["Result"] = "Responder not available.";
		result_cb(std::move(value));
	}
}

void WebUI::ParseURL(const std::string& url, std::string& responder, std::string& command, std::stringstream& ss)
{
	std::stringstream iss(url);
	iss.get(); //throw away leading '/'
	iss >> responder;
	iss >> command;
	std::string params;
	std::string w;
	while (iss >> w)
		params += w + " ";
	if (!params.empty() && params[params.size() - 1] == ' ')
	{
		params = params.substr(0, params.size() - 1);
	}
	std::stringstream ss_temp(params);
	ss << ss_temp.rdbuf();
}