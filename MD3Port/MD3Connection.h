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
 * MD3Connection.h
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#ifndef MD3CONNECTION
#define MD3CONNECTION

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>
#include "MD3.h"
#include "MD3Engine.h"
#include "MD3Port.h"

/*
This class is used to manage and share a TCPSocket for MD3 OutStations
It maintains a list of the MD3OutStations that are using it, so that it can route received MD3 messages to the correct instance based on the Station address
The different instances on a multidrop configuration will all send through this class.
If the last using class is closing, then this class will close the socket.
The TCPSocketManager already does strand protection of the socket, so we will not have to worry about that.
The MD3Connection class manages a static list of its own instances, so the OutStation can decide if it needs to create a new MD3Connection instance or not.
*/

using namespace odc;

class MD3Port;

class MD3Connection
{
public:
	MD3Connection
		(asio::io_service* apIOS,              //pointer to an asio io_service
		bool aisServer,                        //Whether to act as a server or client
		const std::string& aEndPoint,          //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,              //Port to connect to if client, or listen on if server
		const MD3Port *OutStationPortInstance, // Messy, just used so we can access pLogger
		bool aauto_reopen = false,             //Keeps the socket open (retry on error), unless you explicitly Close() it
		uint16_t aretry_time_ms = 0);

	// These next two actually do the same thing at the moment, just establish a route for messages with a given station address
	void AddOutstation(uint8_t StationAddress, // For message routing, OutStation identification
		const std::function<void(MD3Message_t MD3Message)> aReadCallback,
		const std::function<void(bool)> aStateCallback);

	void RemoveOutstation(uint8_t StationAddress);

	void AddMaster(uint8_t TargetStationAddress,
		const std::function<void(MD3Message_t MD3Message)> aReadCallback,
		const std::function<void(bool)> aStateCallback);

	void RemoveMaster(uint8_t TargetStationAddress);

	// Two static methods to manage the map of connections. Can only have one for an address/port combination.
	static std::shared_ptr<MD3Connection> GetConnection(std::string ChannelID);

	static void AddConnection(std::string ChannelID, std::shared_ptr<MD3Connection> pConnection);

	void Open();

	void Close();

	~MD3Connection();

	// We dont need to know who is doing the writing. Just pass to the socket
	void Write(std::string &msg);

	// We need one read completion handler hooked to each address/port combination. This method is reentrant,
	// We do some basic MD3 block identifiaction and procesing, enough to give us complete blocks and StationAddresses
	void ReadCompletionHandler(buf_t& readbuf);

	void RouteMD3Message(MD3Message_t &CompleteMD3Message);

	void SocketStateHandler(bool state);

private:
	asio::io_service* pIOS;
	std::string EndPoint;
	std::string Port;
	std::string ChannelID;

	bool isServer;
	bool enabled = false;
	const MD3Port* pParentPort = nullptr;
	bool auto_reopen;
	uint16_t retry_time_ms;
	MD3Message_t MD3Message;

	// Need maps for these two...
	std::unordered_map<uint8_t, std::function<void(MD3Message_t MD3Message)>> ReadCallbackMap;
	std::unordered_map<uint8_t, std::function<void(bool)>> StateCallbackMap;

	std::shared_ptr<TCPSocketManager<std::string>> pSockMan;

	// A list of MD3Connections, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<std::string, std::shared_ptr<MD3Connection>> ConnectionMap;
};

#endif // MD3CONNECTION

