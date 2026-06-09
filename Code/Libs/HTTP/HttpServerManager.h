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
 * HttpServerManager.h
 *
 *  Created on: 11-09-2018
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#ifndef ServerManagerh
#define ServerManagerh

// opendatacon/asio.h must precede server_http.hpp so asio is included correctly
// through our direct-include guard mechanism.
#include <opendatacon/asio.h>
// *INDENT-OFF*
#include <server_http.hpp>
// *INDENT-ON*
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HandlerCallbackType = std::function<void (std::shared_ptr<HttpServer::Response>, std::shared_ptr<HttpServer::Request>)>;

class HttpServerManager;

class ServerTokenType
{
	friend class HttpServerManager;
public:
	ServerTokenType():
		ServerID(""),
		pServerManager(nullptr)
	{}
	ServerTokenType(std::string serverid, std::shared_ptr<HttpServerManager> servermanager):
		ServerID(serverid),
		pServerManager(servermanager)
	{}
	~ServerTokenType();

	std::string ServerID;
private:
	std::shared_ptr<HttpServerManager> pServerManager;
};

class HttpServerManager
{
	friend class ServerTokenType;

public:
	HttpServerManager(std::shared_ptr<odc::asio_service> apIOS, const std::string& aEndPoint, const std::string& aPort);
	~HttpServerManager();

	static void AddHandler(const ServerTokenType& ServerTok, const std::string& urlpattern, HandlerCallbackType handler);
	static size_t RemoveHandler(const ServerTokenType& ServerTok, const std::string& urlpattern);

	static ServerTokenType AddConnection(std::shared_ptr<odc::asio_service> apIOS, const std::string& aEndPoint, const std::string& aPort);

	static void StartConnection(const ServerTokenType& ServerTok);
	static void StopConnection(const ServerTokenType& ServerTok);

	static std::string MakeServerID(std::string aEndPoint, std::string aPort)
	{
		return aEndPoint + ":" + aPort;
	}

	HttpServerManager& operator=(const HttpServerManager&) = delete;
	HttpServerManager(const HttpServerManager&) = delete;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;

	std::string EndPoint;
	std::string Port;
	std::string InternalServerID;

	static std::unordered_map<std::string, std::weak_ptr<HttpServerManager>> ServerMap;
	static std::mutex ManagementMutex;
};

#endif
