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
 * ServerManager.h
 *
 *  Created on: 11-09-2018
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#ifndef ServerManagerh
#define ServerManagerh

#include "Py.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include "server/server.hpp"
#include "server/request_handler.hpp"


/*
This class is used to manage and share a TCPSocket for http::server objects
The ServerManager class manages a static list of its own instances, so the PyPort can decide if it needs to create a new ServerManager instance or not.
*/

class ServerManager;

// We want to pass this token into the static ServerManager methods, so that the use of the connection pointer is contained
class ServerTokenType
{
	friend class ServerManager; // So the connection class can access the Connection pointer.
public:
	ServerTokenType():
		ServerID(""),
		pServerManager()
	{}
	ServerTokenType(std::string serverid, std::shared_ptr<ServerManager> servermanager):
		ServerID(serverid),
		pServerManager(servermanager)
	{}
	~ServerTokenType();

	std::string ServerID;
private:
	std::shared_ptr<ServerManager> pServerManager;
};

class ServerManager
{
	friend class ServerTokenType;

public:
	ServerManager(std::shared_ptr<asio::io_service> apIOS, const std::string& aEndPoint, const std::string& aPort);

	// These next two actually do the same thing at the moment, just establish a route for messages with a given station address
	static void AddHandler(const ServerTokenType& ServerTok, const std::string& urlpattern, http::pHandlerCallbackType urihandler);

	static ServerTokenType AddConnection(std::shared_ptr<asio::io_service> apIOS, const std::string& aEndPoint, const std::string& aPort);

	static std::string MakeServerID(std::string aEndPoint, std::string aPort)
	{
		return aEndPoint + ":" + aPort;
	}

	~ServerManager();

private:
	std::shared_ptr<asio::io_service> pIOS;
	std::string EndPoint;
	std::string Port;
	std::string InternalServerID;

	std::shared_ptr<http::server> pServer;

	// A list of ServerManagers, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<std::string, std::shared_ptr<ServerManager>> ServerMap;
	static std::mutex MapMutex; // Control access to the map - add connection and remove connection.

	void RemoveServerFromMap(); // Called by ServerToken destructor
};
#endif

