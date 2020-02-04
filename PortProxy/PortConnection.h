/*	opendatacon
 *
 *	Copyright (c) 2018:
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
/*
 * PortProxyConnection.h
 *
 *  Created on: 11-09-2020
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#ifndef PORTPROXYCON
#define PORTPROXYCON

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>
#include "PortProxy.h"

using namespace odc;

class PortProxyConnection
{

public:
	PortProxyConnection
	(
		std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
		const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                 //Port to connect to if client, or listen on if server
		const std::string& aClientEndPoint,       //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aClientPort,
		uint16_t retry_time_ms = 0
	);

	void Open();
	void Close();

	~PortProxyConnection();

private:
	std::shared_ptr<odc::asio_service> pIOS;
	std::string ServerEndPoint;
	std::string ServerPort;
	std::string ClientEndPoint;
	std::string ClientPort;
	std::string InternalChannelID;

	std::shared_ptr<TCPSocketManager<std::string>> pServerSockMan;
	std::shared_ptr<TCPSocketManager<std::string>> pClientSockMan;

	void ClientSocketStateHandler(bool state);
	void ClientReadCompletionHandler(buf_t& readbuf);

	void ServerSocketStateHandler(bool state);
	void ServerReadCompletionHandler(buf_t& readbuf);
};
#endif

