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

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>
#include "CB.h"
#include "CBUtility.h"
// #include "CBPort.h"

/*
This class is used to manage and share a TCPSocket for CB OutStations
It maintains a list of the CBOutStations that are using it, so that it can route received CB messages to the correct instance based on the Station address
The different instances on a multidrop configuration will all send through this class.
If the last using class is closing, then this class will close the socket.
The TCPSocketManager already does strand protection of the socket, so we will not have to worry about that.
The CBConnection class manages a static list of its own instances, so the OutStation can decide if it needs to create a new CBConnection instance or not.
*/

using namespace odc;

class CBPort;

class CBConnection
{
public:
	CBConnection
		(asio::io_service* apIOS,     //pointer to an asio io_service
		bool aisServer,               //Whether to act as a server or client
		const std::string& aEndPoint, //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,     //Port to connect to if client, or listen on if server
		bool isbakerdevice,
		uint16_t retry_time_ms = 0);

	// These next two actually do the same thing at the moment, just establish a route for messages with a given station address
	void AddOutstation(uint8_t StationAddress, // For message routing, OutStation identification
		const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
		const std::function<void(bool)> aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	void RemoveOutstation(uint8_t StationAddress);

	void AddMaster(uint8_t TargetStationAddress,
		const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
		const std::function<void(bool)> aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	void RemoveMaster(uint8_t TargetStationAddress);

	// Two static methods to manage the map of connections. Can only have one for an address/port combination.
	static std::shared_ptr<CBConnection> GetConnection(std::string ChannelID);

	static void AddConnection(std::string ChannelID, std::shared_ptr<CBConnection> pConnection);

	void Open();

	void Close();

	~CBConnection();

	// Will do Baker/Conitel swap if needed
	void Write(const CBMessage_t &CompleteCBMessage);

	// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
	// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
	void ReadCompletionHandler(buf_t& readbuf);

	void RouteCBMessage(CBMessage_t &CompleteCBMessage);

	void SocketStateHandler(bool state);

	void SetSendTCPDataFn(std::function<void(std::string)> f)
	{
		SendTCPDataFn = f;
	}

private:
	asio::io_service* pIOS;
	std::string EndPoint;
	std::string Port;
	std::string ChannelID;

	bool IsServer;
	bool IsBakerDevice; // Swap Station and Group if it is a Baker device. Set in constructor
	bool enabled = false;
	CBMessage_t CBMessage;

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr; // nullptr normally. Set to hook function for testing

	// Need maps for these two...
	std::unordered_map<uint8_t, std::function<void(CBMessage_t &CBMessage)>> ReadCallbackMap;
	std::unordered_map<uint8_t, std::function<void(bool)>> StateCallbackMap;

	std::shared_ptr<TCPSocketManager<std::string>> pSockMan;

	// A list of CBConnections, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<std::string, std::shared_ptr<CBConnection>> ConnectionMap;

	// We don't need to know who is doing the writing. Just pass to the socket
	void Write(std::string &msg);
};
#endif

