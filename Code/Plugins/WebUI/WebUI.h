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
//  WebUI.h
//  opendatacon
//
//  Created by Alan Murray on 06/09/2014.
//
//

#ifndef __opendatacon__WebUI__
#define __opendatacon__WebUI__
#include "WebHelpers.h"
#include <opendatacon/IUI.h>
#include <opendatacon/TCPSocketManager.h>
#include <regex>
#include <shared_mutex>
#include <queue>
#include <string_view>

const char ROOTPAGE[] = "/index.html";

class WebUI: public IUI
{
public:
	WebUI(uint16_t port, const std::string& web_root, const std::string& web_crt, const std::string& web_key, const std::string& tcp_port, size_t log_q_size);

	/* Implement IUI interface */
	void AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc = "No description available\n") override;
	void Build() override;
	void Enable() override;
	void Disable() override;

private:
	void LoadRequestParams(std::shared_ptr<WebServer::Request> request);
	void DefaultRequestHandler(std::shared_ptr<WebServer::Response> response,
		std::shared_ptr<WebServer::Request> request);
	void ReturnFile(std::shared_ptr<WebServer::Response> response,
		std::shared_ptr<WebServer::Request> request);
	WebServer WebSrv;
	const int port;
	std::string cert_pem;
	std::string key_pem;
	std::string web_root;
	std::string tcp_port;
	std::unique_ptr<odc::TCPSocketManager> pSockMan;

	//TODO: these can be maps with entry per web session
	//the pairs in the Q hold:
	//	* a string view, and
	//	* a shared pointer that manages it's underlying memory
	std::string filter;
	bool filter_is_regex;
	std::unique_ptr<std::regex> pLogRegex;
	std::deque<std::pair<std::shared_ptr<void>,std::string_view>> log_queue;
	size_t log_q_size;
	const std::unique_ptr<asio::io_service::strand> log_q_sync = pIOS->make_strand();

	/*Param Collection with POST from client side*/
	ParamCollection params;
	/* UI response handlers */
	std::unordered_map<std::string, CmdFunc_t> RootCommands;
	void ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args, std::function<void (const Json::Value&&)> result_cb);
	void HandleCommand(const std::string& url, std::function<void (const Json::Value&&)> result_cb);
	void ConnectToTCPServer();
	void ReadCompletionHandler(odc::buf_t& readbuf);
	void ConnectionEvent(bool state);

	//TODO: These could be per web session
	Json::Value ApplyLogFilter(const std::string& new_filter, bool is_regex);
	void ParseURL(const std::string& url, std::string& responder, std::string& command, std::stringstream& ss);
	bool IsCommand(const std::string& url);
};

#endif /* defined(__opendatacon__WebUI__) */
