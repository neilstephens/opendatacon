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

#ifndef __opendatacon__WebServicesServer__
#define __opendatacon__WebServicesServer__
#include "WebHelpers.h"
#include <opendatacon/IUI.h>
#include <opendatacon/TCPSocketManager.h>
#include <regex>
#include <shared_mutex>
#include <queue>
#include <string_view>

class WebUI: public IUI
{
public:
	WebUI(uint16_t pPort, const std::string& web_crt, const std::string& web_key);

	/* Implement IUI interface */
	//void AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc = "No description available\n") override;
	void Build() override;
	void Enable() override;
	void Disable() override;

private:
	void LoadRequestParams(std::shared_ptr<WebServer::Request> request);
	void DefaultRequestHandler(std::shared_ptr<WebServer::Response> response,
		std::shared_ptr<WebServer::Request> request);

	WebServer WebSrv;
	const int port;
	std::string cert_pem;
	std::string key_pem;

	/*Param Collection with POST from client side*/
	ParamCollection params;
	void HandleCommand(const std::string& url, std::function<void (const Json::Value&&)> result_cb);

	//TODO: These could be per web session
	void ParseURL(const std::string& url, std::string& responder, std::string& command, std::stringstream& ss);
	bool IsCommand(const std::string& url);
};

#endif /* defined(__opendatacon__WebServicesServer__) */
