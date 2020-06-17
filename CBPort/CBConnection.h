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
 * CBConnection.h
 *
 *  Created on: 11-09-2018
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#ifndef CBCONNECTION
#define CBCONNECTION
#include "CB.h"
#include "CBUtility.h"
#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>

/*
This class is used to manage and share a TCPSocket for CB OutStations
It maintains a list of the CBOutStations that are using it, so that it can route received CB messages to the correct instance based on the Station address
The different instances on a multidrop configuration will all send through this class.
If the last using class is closing, then this class will close the socket.
The TCPSocketManager already does strand protection of the socket, so we will not have to worry about that.
The CBConnection class manages a static list of its own instances, so the OutStation can decide if it needs to create a new CBConnection instance or not.
*/

using namespace odc;

class CBConnection;

// We want to pass this token into the static CBConnection methods, so that the use of the connection pointer is contained
class ConnectionTokenType
{
	friend class CBConnection; // So the connection class can access the Connection pointer.
public:
	ConnectionTokenType():
		ChannelID(""),
		pConnection()
	{}
	ConnectionTokenType(std::string channelid, std::shared_ptr<CBConnection> connection):
		ChannelID(channelid),
		pConnection(connection)
	{}
	~ConnectionTokenType();

	std::string ChannelID;
private:
	std::shared_ptr<CBConnection> pConnection;
};

class CBConnection
{
	friend class ConnectionTokenType;

public:
	CBConnection
	(
		std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
		bool aisServer,                           //Whether to act as a server or client
		const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                 //Port to connect to if client, or listen on if server
		bool isbakerdevice,
		uint16_t retry_time_ms = 0
	);

	// These next two actually do the same thing at the moment, just establish a route for messages with a given station address
	static void AddOutstation(const ConnectionTokenType &pConnection,
		uint8_t StationAddress, // For message routing, OutStation identification
		const std::function<void(CBMessage_t &CBMessage)>& aReadCallback,
		const std::function<void(bool)>& aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	static void RemoveOutstation(const ConnectionTokenType &ConnectionTok, uint8_t StationAddress);

	static void AddMaster(const ConnectionTokenType &ConnectionTok,
		uint8_t TargetStationAddress,
		const std::function<void(CBMessage_t &CBMessage)>& aReadCallback,
		const std::function<void(bool)>& aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	static void RemoveMaster(const ConnectionTokenType &ConnectionTok,uint8_t TargetStationAddress);

	static ConnectionTokenType AddConnection
	(
		std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
		bool aisServer,                           //Whether to act as a server or client
		const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                 //Port to connect to if client, or listen on if server
		bool isbakerdevice,
		uint16_t retry_time_ms = 0
	);

	static void Open(const ConnectionTokenType &ConnectionTok);

	static void Close(const ConnectionTokenType &ConnectionTok);

	static std::string MakeChannelID(std::string aEndPoint, std::string aPort, bool aisServer)
	{
		return aEndPoint + ":" + aPort + ":" + std::to_string(aisServer);
	}

	~CBConnection();

	// Will do Baker/Conitel swap if needed
	static void Write(const ConnectionTokenType &ConnectionTok, const CBMessage_t &CompleteCBMessage);

	static void InjectSimulatedTCPMessage(const ConnectionTokenType &ConnectionTok, buf_t&readbuf);

	static void SetSendTCPDataFn(const ConnectionTokenType &ConnectionTok, std::function<void(std::string)> f);

private:
	std::shared_ptr<odc::asio_service> pIOS;
	std::string EndPoint;
	std::string Port;
	std::string InternalChannelID;

	bool IsServer;
	std::atomic_bool successfullyopened{ false }; // There is a possible race condition we need to deal with on a failed opening of the connection
	std::atomic<int> opencount{ 0 };              // So we only disconnect the port when everyone has disconnected.
	bool IsBakerDevice;                           // Swap Station and Group if it is a Baker device. Set in constructor

	CBMessage_t CBMessage;

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr; // nullptr normally. Set to hook function for testing

	// Need maps for these two...
	//TODO: Callbacks - how do we prevent calling them during shutdown? Need protection around checking they are valid and using them, from removal during use. If the port is disabled - should ne no traffic.
	std::unordered_map<uint8_t, std::function<void(CBMessage_t &CBMessage)>> ReadCallbackMap;
	std::unordered_map<uint8_t, std::function<void(bool)>> StateCallbackMap;

	std::shared_ptr<TCPSocketManager<std::string>> pSockMan;

	// A list of CBConnections, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<std::string, std::shared_ptr<CBConnection>> ConnectionMap;
	static std::mutex MapMutex; // Control access to the map - add connection and remove connection.

	void RemoveConnectionFromMap(); // Called by ConnectionToken destructor
	void Open();
	void Close();
	void Write(std::string &msg);
	void RouteCBMessage(CBMessage_t &CompleteCBMessage);
	void SocketStateHandler(bool state);

	// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
	// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
	void ReadCompletionHandler(buf_t& readbuf);
	CBBlockData ReadCompletionHandlerCBblock = CBBlockData(0); // This remains across multiple calls to this method in a given class instance. Starts empty.
};
#endif

