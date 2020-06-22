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
#include "MhdWrapper.h"
#include <opendatacon/IUI.h>
#include <opendatacon/TCPSocketManager.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex>
#include <shared_mutex>
#include <queue>
#include <string_view>

const char ROOTPAGE[] = "/index.html";

class WebUI: public IUI
{
public:
	WebUI(uint16_t port, const std::string& web_root, const std::string& tcp_port, size_t log_q_size);

	/* Implement IUI interface */
	void AddCommand(const std::string& name, std::function<void (std::stringstream&)> callback, const std::string& desc = "No description available\n") override;
	void AddResponder(const std::string& name, const IUIResponder& pResponder) override;
	void Build() override;
	void Enable() override;
	void Disable() override;

	/* HTTP response handler call back */
	int http_ahc(void *cls,
		struct MHD_Connection *connection,
		const std::string& url,
		const std::string& method,
		const std::string& version,
		const std::string& upload_data,
		size_t& upload_data_size,
		void **ptr);

private:
	struct MHD_Daemon * d;
	const int port;
	std::string cert_pem;
	std::string key_pem;
	std::string web_root;
	std::string tcp_port;
	std::unique_ptr<odc::TCPSocketManager<std::string>> pSockMan;

	//TODO: these can be maps with entry per web session
	//the pairs in the Q hold:
	//	* a string view, and
	//	* a shared pointer that manages it's underlying memory
	std::deque<std::pair<std::shared_ptr<void>,std::string_view>> log_queue;
	const std::unique_ptr<asio::io_service::strand> log_q_sync = pIOS->make_strand();
	size_t log_q_size;

	mutable std::shared_timed_mutex LogRegexMutex;
	std::unique_ptr<std::regex> pLogRegex;

	bool useSSL = false;
	/* UI response handlers */
	std::unordered_map<std::string, const IUIResponder*> Responders;
	std::unordered_map<std::string, std::function<void (std::stringstream&)>> RootCommands;

	std::string HandleSimControl(const std::string& url);
	Json::Value ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args);
	std::string HandleOpenDataCon(const std::string& url);
	void ConnectToTCPServer();
	void ReadCompletionHandler(odc::buf_t& readbuf);
	void ConnectionEvent(bool state);

	//TODO: These could be per web session
	void ApplyLogFilter(const std::string& regex_filter);
	std::unique_ptr<std::regex> GetLogFilter();
};

#endif /* defined(__opendatacon__WebUI__) */
