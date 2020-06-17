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
 * HttpServerManager.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */


#include "HttpServerManager.h"
#include <opendatacon/ODCLogMacros.h>
#include <functional>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::weak_ptr<HttpServerManager>> HttpServerManager::ServerMap;

std::mutex HttpServerManager::ManagementMutex; // Allow only one management operation at a time. Not a performance issue


ServerTokenType::~ServerTokenType()
{}

HttpServerManager::HttpServerManager(std::shared_ptr<odc::asio_service> apIOS, const std::string& aEndPoint, const std::string& aPort):
	pIOS(std::move(apIOS)),
	EndPoint(aEndPoint),
	Port(aPort)
{
	// This is only called by the method below, which is already protected.
	pServer = std::make_shared<http::server>(pIOS, EndPoint, Port);

	InternalServerID = MakeServerID(aEndPoint, aPort);

	LOGDEBUG("Opened an HttpServerManager object {} ", InternalServerID);
}

// Static Method
ServerTokenType HttpServerManager::AddConnection(std::shared_ptr<odc::asio_service> apIOS, const std::string& aEndPoint, const std::string& aPort)
{
	std::unique_lock<std::mutex> lck(HttpServerManager::ManagementMutex); // Only allow one static op at a time

	std::string ServerID = MakeServerID(aEndPoint, aPort);

	// Only add if does not exist, or has expired
	std::shared_ptr<HttpServerManager> pSM;
	if (ServerMap.count(ServerID) == 0 || !(pSM = ServerMap[ServerID].lock()))
	{
		LOGDEBUG("First ServerTok for connection - {}", ServerID);
		// If we give each ServerToken a shared_ptr to the connection, then the
		//connection gets destoyed with the last token
		pSM = std::make_shared<HttpServerManager>(apIOS, aEndPoint, aPort);
		ServerMap[ServerID] = pSM;
	}
	else
		LOGDEBUG("ServerTok already exists, using that connection - {}", ServerID);

	return ServerTokenType(ServerID, pSM);
}

void HttpServerManager::StartConnection(const ServerTokenType& ServerTok)
{
	std::unique_lock<std::mutex> lck(HttpServerManager::ManagementMutex); // Only allow one static op at a time
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		pServerMgr->pServer->start(); // Ok to call if already running
	}
	else
	{
		LOGERROR("Tried to start httpserver when the connection token was no longer valid");
	}
}

void HttpServerManager::StopConnection(const ServerTokenType& ServerTok)
{
	std::unique_lock<std::mutex> lck(HttpServerManager::ManagementMutex); // Only allow one static op at a time
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		pServerMgr->pServer->stop(); // Ok to call if already stopped
	}
	else
	{
		LOGERROR("Tried to stop httpserver when the connection token was no longer valid");
	}
}

void HttpServerManager::AddHandler(const ServerTokenType& ServerTok, const std::string &urlpattern, http::pHandlerCallbackType urihandler)
{
	std::unique_lock<std::mutex> lck(HttpServerManager::ManagementMutex); // Only allow one static op at a time
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		pServerMgr->pServer->register_handler(urlpattern, std::move(urihandler)); // Will overwrite if duplicate
	}
	else
	{
		LOGERROR("Tried to add a urihandler when the httpserver was no longer valid");
	}
}

HttpServerManager::~HttpServerManager()
{}
