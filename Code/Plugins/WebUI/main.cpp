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
//  main.cpp
//  opendatacon
//
//  Created by Alan Murray on 30/11/2014.
//
//

#include "WebUI.h"

extern "C" WebUI* new_WebUIPlugin(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	std::string ip = "0.0.0.0";
	uint16_t port = 443;
	std::string web_root = "www";
	std::string tcp_port = "10593";
	size_t log_q_size = 200;
	if(Overrides.isObject())
	{
		if(Overrides.isMember("IP"))
			ip= Overrides["IP"].asString();

		if(Overrides.isMember("Port"))
			port = Overrides["Port"].asUInt();

		if (Overrides.isMember("WebRoot"))
			web_root = Overrides["WebRoot"].asString();
		if (Overrides.isMember("LogPort"))
			tcp_port = Overrides["LogPort"].asString();
		if (Overrides.isMember("MaxLogMessages"))
			log_q_size = Overrides["MaxLogMessages"].asUInt();
	}

	return new WebUI(port, web_root, tcp_port, log_q_size);
}

extern "C" void delete_WebUIPlugin(WebUI* aIUI_ptr)
{
	delete aIUI_ptr;
	return;
}
