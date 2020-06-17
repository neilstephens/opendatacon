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
#include "MD3.h"
#include "MD3Utility.h"
#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>

/*
This class is used to manage and share a TCPSocket for MD3 OutStations
It maintains a list of the MD3OutStations that are using it, so that it can route received MD3 messages to the correct instance based on the Station address
The different instances on a multidrop configuration will all send through this class.
If the last using class is closing, then this class will close the socket.
The TCPSocketManager already does strand protection of the socket, so we will not have to worry about that.
The MD3Connection class manages a static list of its own instances, so the OutStation can decide if it needs to create a new MD3Connection instance or not.
*/

using namespace odc;

class MD3Connection;

// We want to pass this token into the static CBConnection methods, so that the use of the connection pointer is contained
class ConnectionTokenType
{
	friend class MD3Connection; // So the connection class can access the Connection pointer.
public:
	ConnectionTokenType():
		ChannelID(""),
		pConnection()
	{}
	ConnectionTokenType(std::string channelid, std::shared_ptr<MD3Connection> connection):
		ChannelID(channelid),
		pConnection(connection)
	{}
	~ConnectionTokenType();

	std::string ChannelID;
private:
	std::shared_ptr<MD3Connection> pConnection;
};

class MD3Connection
{
	friend class ConnectionTokenType;

public:
	MD3Connection
		(std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
		bool aisServer,                            //Whether to act as a server or client
		const std::string& aEndPoint,              //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                  //Port to connect to if client, or listen on if server
		uint16_t retry_time_ms = 0);

	// These next two actually do the same thing at the moment, just establish a route for messages with a given station address
	static void AddOutstation(const ConnectionTokenType &ConnectionTok,
		uint8_t StationAddress, // For message routing, OutStation identification
		const std::function<void(MD3Message_t &MD3Message)>& aReadCallback,
		const std::function<void(bool)>& aStateCallback); // Check that we dont have different devices on the one connection!

	static void RemoveOutstation(const ConnectionTokenType &ConnectionTok, uint8_t StationAddress);

	static void AddMaster(const ConnectionTokenType &ConnectionTok,
		uint8_t TargetStationAddress,
		const std::function<void(MD3Message_t &MD3Message)>& aReadCallback,
		const std::function<void(bool)>& aStateCallback); // Check that we dont have different devices on the one connection!

	static void RemoveMaster(const ConnectionTokenType &ConnectionTok,uint8_t TargetStationAddress);

	static ConnectionTokenType AddConnection
	(
		std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
		bool aisServer,                           //Whether to act as a server or client
		const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                 //Port to connect to if client, or listen on if server
		uint16_t retry_time_ms = 0
	);

	static void Open(const ConnectionTokenType &ConnectionTok);

	static void Close(const ConnectionTokenType &ConnectionTok);

	static std::string MakeChannelID(std::string aEndPoint, std::string aPort, bool aisServer)
	{
		return aEndPoint + ":" + aPort + ":" + std::to_string(aisServer);
	}

	~MD3Connection();

	static void Write(const ConnectionTokenType &ConnectionTok, const MD3Message_t &CompleteMD3Message);

	static void InjectSimulatedTCPMessage(const ConnectionTokenType &ConnectionTok, buf_t&readbuf);

	static void SetSendTCPDataFn(const ConnectionTokenType &ConnectionTok, std::function<void(std::string)> f);

private:
	std::shared_ptr<odc::asio_service> pIOS;
	std::string EndPoint;
	std::string Port;
	std::string InternalChannelID;

	bool isServer;
	std::atomic_bool successfullyopened{ false }; // There is a possible race condition we need to deal with on a failed opening of the connection
	std::atomic<int> opencount{ 0 };              // So we only disconnect the port when everyone has disconnected.
	MD3Message_t MD3Message;

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr; // nullptr normally. Set to hook function for testing

	// Need maps for these two...
	std::unordered_map<uint8_t, std::function<void(MD3Message_t &MD3Message)>> ReadCallbackMap;
	std::unordered_map<uint8_t, std::function<void(bool)>> StateCallbackMap;

	std::shared_ptr<TCPSocketManager<std::string>> pSockMan;

	// A list of CBConnections, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<std::string, std::shared_ptr<MD3Connection>> ConnectionMap;
	static std::mutex MapMutex; // Control access to the map - add connection and remove connection.

	void RemoveConnectionFromMap(); // Called by ConnectionToken destructor
	void Open();
	void Close();
	void Write(std::string &msg);
	void RouteMD3Message(MD3Message_t &CompleteCBMessage);
	void SocketStateHandler(bool state);

	// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
	// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
	void ReadCompletionHandler(buf_t& readbuf);
	MD3BlockData ReadCompletionHandlerMD3block = MD3BlockData(0); // This remains across multiple calls to this method in a given class instance. Starts empty.
};
#endif

