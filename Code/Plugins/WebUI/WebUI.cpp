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

#include "WebUI.h"
#include <opendatacon/util.h>
#include <opendatacon/asio.h>

WebUI::WebUI(uint16_t pPort, const std::string& web_root, const std::string& web_crt, const std::string& web_key, const std::string& tcp_port, size_t log_q_size):
	WebSrv(OPTIONAL_CERTS),
	port(pPort),
	web_root(web_root),
	tcp_port(tcp_port),
	pSockMan(nullptr),
	filter_is_regex(false),
	pLogRegex(nullptr),
	log_q_size(log_q_size)
{}

void WebUI::AddCommand(const std::string& name, std::function<Json::Value  (std::stringstream&)> callback, const std::string& desc)
{
	RootCommands[name] = callback;
}

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
	else
	{
		ReturnFile(response,request);
	}
}

void WebUI::ReturnFile(std::shared_ptr<WebServer::Response> response,
	std::shared_ptr<WebServer::Request> request)
{
	std::string filepath;
	if (request->path == "/")
		filepath = web_root + "/" + ROOTPAGE;
	else
		filepath = web_root + "/" + SimpleWeb::Percent::decode(request->path);

	auto ifs = std::make_shared<std::ifstream>();
	ifs->open(filepath, std::ifstream::in | std::ios::binary | std::ios::ate);
	if(!(*ifs))
	{
		const std::string msg = "Could not open path ("+ filepath +"): file open failed";
		response->write(SimpleWeb::StatusCode::client_error_bad_request, msg);
		if (auto log = odc::spdlog_get("WebUI"))
			log->error(msg);
	}
	else
	{
		//put length in the header
		SimpleWeb::CaseInsensitiveMultimap header;
		auto length = ifs->tellg();
		ifs->seekg(0, std::ios::beg);
		header.emplace("Content-Length", to_string(length));
		header.emplace("Content-Type", GetMimeType(filepath));
		response->write(header);

		// Read and send 128 KB at a time
		auto buffer = std::make_shared<std::vector<char>>(131072);
		read_and_send(response, ifs, buffer);
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

	const std::string url = "/RootCommand add_logsink tcp_web_ui info TCP localhost " + tcp_port + " SERVER";
	HandleCommand(url,[](const Json::Value&&){});
}

void WebUI::Enable()
{
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

	if(pSockMan)
		pSockMan->Open();
}

void WebUI::Disable()
{
	if(pSockMan)
		pSockMan->Close();

	WebSrv.stop();
}

void WebUI::HandleCommand(const std::string& url, std::function<void (const Json::Value&&)> result_cb)
{
	std::stringstream iss;
	std::string responder;
	std::string command;
	ParseURL(url, responder, command, iss);

	if (responder == "WebUICommand")
	{
		if (command == "tcp_logs_on")
		{
			if(!pSockMan)
				ConnectToTCPServer();

			log_q_sync->post([=]()
				{
					std::string log_str;
					for(const auto& pair : log_queue)
						log_str.append(pair.second);
					Json::Value value;
					value["tcp_data"] = std::move(log_str);
					result_cb(std::move(value));
				});
		}
		else if (command == "apply_log_filter")
		{
			std::string filter_type;
			iss >> filter_type;
			std::string new_filter;
			if (iss.str().size() > filter_type.size() + 1)
				new_filter = iss.str().substr(filter_type.size() + 1, iss.str().size() - filter_type.size() - 1);
			bool is_regex = (filter_type == "reg_ex");
			log_q_sync->post([=, new_filter{std::move(new_filter)}]()
				{
					result_cb(ApplyLogFilter(new_filter, is_regex));
				});
		}
	}
	else if (responder == "RootCommand")
	{
		Json::Value value;
		if (RootCommands.find(command) == RootCommands.end())
			value["Result"] = "Command " + command + " not found !!!";
		else
			RootCommands[command](iss);
		value["Result"] = "Success";
		result_cb(std::move(value));
	}
	else if(Responders.find(responder) != Responders.end())
	{
		ExecuteCommand(Responders[responder], command, iss, result_cb);
	}
	else
	{
		Json::Value value;
		value["Result"] = "Responder not available.";
		result_cb(std::move(value));
	}
}

void WebUI::ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args, std::function<void (const Json::Value&&)> result_cb)
{
	//Define first arg as Target regex
	std::string T_regex_str;
	odc::extract_delimited_string("\"'`",args,T_regex_str);

	//turn any regex it into a list of targets
	Json::Value target_list;
	if(!T_regex_str.empty() && command != "List") //List is a special case - handles it's own regex
	{
		params["Target"] = T_regex_str;
		target_list = pResponder->ExecuteCommand("List", params)["Items"];
	}

	int arg_num = 0;
	std::string Val;
	while(odc::extract_delimited_string("\"'`",args,Val))
		params[std::to_string(arg_num++)] = Val;

	Json::Value results;
	if(target_list.size() > 0) //there was a list resolved
	{
		for(auto& target : target_list)
		{
			params["Target"] = target.asString();
			results[target.asString()] = pResponder->ExecuteCommand(command, params);
		}
	}
	else
	{
		//There was no list - execute anyway
		results = pResponder->ExecuteCommand(command, params);
	}

	if (command == "List")
	{
		for(Json::Value::ArrayIndex i = 0; i < results["Commands"].size(); i++)
			if(results["Commands"][i].asString() == "List")
			{
				Json::Value discard;
				results["Commands"].removeIndex(i,&discard);
				break;
			}
	}
	result_cb(std::move(results));
}

void WebUI::ConnectToTCPServer()
{
	//Use the ODC TCP manager
	// Client connection to localhost on the port we set up for log sinking
	// Automatically retry to connect on error
	pSockMan = std::make_unique<odc::TCPSocketManager>
		           (pIOS, false, "localhost", tcp_port,
		           [this](odc::buf_t& readbuf){ReadCompletionHandler(readbuf);},
		           [this](bool state){ConnectionEvent(state);},
		           1000,
		           true);
	pSockMan->Open(); //Auto re-open is enabled, so it's set and forget
}

void WebUI::ReadCompletionHandler(odc::buf_t& readbuf)
{
	const auto len = readbuf.size();

	//copy all the data into a managed buffer
	auto pBuffer = std::shared_ptr<char>(new char[readbuf.size()],[](char* p){delete[] p;}); //TODO: use make_shared<char[]>(size), instead of new (once c++20 comes along)
	readbuf.sgetn(pBuffer.get(),len);

	//a string view of the whole buffer
	std::string_view full_str(pBuffer.get(),len);

	//find each line and process without copying
	size_t start_line_pos = 0;
	size_t end_line_pos = 0;
	while(start_line_pos < len && (end_line_pos = full_str.find_first_of('\n', start_line_pos)) != std::string_view::npos)
	{
		//end position minus start position == one less than length
		auto line_str = full_str.substr(start_line_pos, (end_line_pos-start_line_pos)+1);

		log_q_sync->post([this,line_str,pBuffer]()
			{
				if((filter_is_regex && (!pLogRegex || std::regex_match(line_str.begin(),line_str.end(),*pLogRegex))) ||
				   (!filter_is_regex && (line_str.find(filter) != std::string::npos)))
				{
				      log_queue.emplace_front(pBuffer,line_str);
				      if(log_queue.size() > log_q_size)
						log_queue.pop_back();
				}
			});

		//start from one past the end for the next match
		start_line_pos = end_line_pos+1;
	}

	//put back partial line
	if(start_line_pos < len)
	{
		auto leftover = full_str.substr(start_line_pos);
		readbuf.sputn(leftover.data(), leftover.length());
	}
}

void WebUI::ConnectionEvent(bool state)
{
	if (auto log = odc::spdlog_get("WebUI"))
		log->debug("Log sink connection on port {} {}",tcp_port,state ? "opened" : "closed");
}

Json::Value WebUI::ApplyLogFilter(const std::string& new_filter, bool is_regex)
{
	Json::Value value;
	value["Result"] = "Success";
	if (is_regex)
	{
		if (new_filter.empty())
		{
			pLogRegex.reset();
			filter.clear();
			filter_is_regex = true;
		}
		else
		{
			try
			{
				pLogRegex = std::make_unique<std::regex>(new_filter,std::regex::extended);
				filter = new_filter;
				filter_is_regex = true;
			}
			catch (std::exception& e)
			{
				if (auto log = odc::spdlog_get("WebUI"))
					log->error("Problem using '{}' as regex: {}",new_filter,e.what());
				value["Result"] = "Fail: " + std::string(e.what());
			}
		}
	}
	else
	{
		filter = new_filter;
		filter_is_regex = false;
	}
	return value;
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

bool WebUI::IsCommand(const std::string& url)
{
	std::stringstream iss(url);
	std::string responder;
	std::string command;
	iss.get(); //throw away leading '/'
	iss >> responder >> command;
	bool result = false;
	if ((Responders.find(responder) != Responders.end()) ||
	    (RootCommands.find(command) != RootCommands.end()) ||
	    responder == "WebUICommand")
	{
		result = true;
	}
	return result;
}
