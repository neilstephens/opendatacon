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
 * ServerManager.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */


#include <string>
#include <functional>
#include <unordered_map>

#include "PyPort.h"
#include "ServerManager.h"


std::unordered_map<std::string, std::shared_ptr<ServerManager>> ServerManager::ServerMap;
std::mutex ServerManager::MapMutex; // Control access


ServerTokenType::~ServerTokenType()
{
	// If we are the last ServerToken for this connection (usecount == 2 , the Map and us) - delete the connection.
	if (pServerManager)
	{
		if (pServerManager.use_count() <= 2)
		{
			LOGDEBUG("Use Count On ServerTok Shared_ptr down to 2 - Destroying the Map Server - {}", ServerID);
			pServerManager->RemoveServerFromMap();
			// Now release our shared_ptr - the last one..The ServerManager destructor should now be called.
			pServerManager.reset();
		}
		else
		{
			LOGDEBUG("Use Count On Connection Shared_ptr at {} - The Map Server Stays - {}", pServerManager.use_count(), ServerID);
		}
	}
}

ServerManager::ServerManager(std::shared_ptr<asio::io_service> apIOS, const std::string& aEndPoint, const std::string& aPort):
	pIOS(apIOS),
	EndPoint(aEndPoint),
	Port(aPort)
{

	pServer.reset(new http::server (pIOS, EndPoint, Port));

	InternalServerID = MakeServerID(aEndPoint, aPort);

	LOGDEBUG("Opened an ServerManager object {} ", InternalServerID);
}

// Static Method
ServerTokenType ServerManager::AddConnection(std::shared_ptr<asio::io_service> apIOS, const std::string& aEndPoint,     const std::string& aPort)
{
	std::string ServerID = MakeServerID(aEndPoint, aPort);

	std::unique_lock<std::mutex> lck(ServerManager::MapMutex); // Access the Map - only here and in the destructor do we do this.

	// Only add if does not exist
	if (ServerMap.count(ServerID) == 0)
	{
		// If we give each ServerToken a shared_ptr to the connection, then in the ServerToken destructors,
		// when the use_count is 2 (the map and the ServerToken), then it is time to destroy the connection.

		ServerMap[ServerID] = std::make_shared<ServerManager>(apIOS, aEndPoint, aPort);
	}
	else
		LOGDEBUG("ServerTok already exists, using that connection - {}", ServerID);

	return ServerTokenType(ServerID, ServerMap[ServerID]); // the use count on the Connection Map shared_ptr will go up by one.
}

void ServerManager::AddHandler(const ServerTokenType& ServerTok, const std::string &urlpattern, http::pHandlerCallbackType urihandler)
{
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		pServerMgr->pServer->register_handler(urlpattern, urihandler); // Will overwrite if duplicate
	}
	else
	{
		LOGERROR("Tried to add a urihandler when the httpserver was no longer valid");
	}
}


// Use this method only to shut down a connection and destroy it. Only used internally
void ServerManager::RemoveServerFromMap()
{
	std::unique_lock<std::mutex> lck(ServerManager::MapMutex);
	ServerMap.erase(InternalServerID); // Remove the map entry shared ptr - so that shared ptr no longer points to us.
}


ServerManager::~ServerManager()
{

	if (!pServer) // Could be empty if a connection was never added (opened)
		return;

	pServer.reset(); // Release our object - should be done anyway when the shared_ptr is destructed, but just to make sure...
}



