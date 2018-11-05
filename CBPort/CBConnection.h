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
	static void AddOutstation(uint64_t ChannelID,
		uint8_t StationAddress, // For message routing, OutStation identification
		const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
		const std::function<void(bool)> aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	static void RemoveOutstation(uint64_t ChannelID, uint8_t StationAddress);

	static void AddMaster(uint64_t ChannelID,
		uint8_t TargetStationAddress,
		const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
		const std::function<void(bool)> aStateCallback,
		bool isbakerdevice); // Check that we dont have different devices on the one connection!

	static void RemoveMaster(uint64_t ChannelID,uint8_t TargetStationAddress);

	static void AddConnection(asio::io_service* apIOS, //pointer to an asio io_service
		bool aisServer,                              //Whether to act as a server or client
		const std::string& aEndPoint,                //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                    //Port to connect to if client, or listen on if server
		bool isbakerdevice,
		uint16_t retry_time_ms = 0);

	static void Open(uint64_t ChannelID);

	static void Close(uint64_t ChannelID);

	static std::string MakeStringChannelID(std::string aEndPoint, std::string aPort, bool aisServer)
	{
		return aEndPoint + ":" + aPort + ":" + std::to_string(aisServer);
	}
	static uint64_t MakeChannelID(std::string aEndPoint, std::string aPort, bool aisServer)
	{
		// Top 32 bits is the IP address, the next 24 the port, the bottom 8 the isaserver flag.
		uint64_t result = 0;

		try
		{
			// Convert the 10.12.145.240 into 4 bytes.
			std::vector<std::string> tok = split(aEndPoint, '.');

			result |= numeric_cast<uint64_t>(std::stoi(tok[0])) << (64 - 8);
			result |= numeric_cast<uint64_t>(std::stoi(tok[1])) << (64 - 16);
			result |= numeric_cast<uint64_t>(std::stoi(tok[2])) << (64 - 24);
			result |= numeric_cast<uint64_t>(std::stoi(tok[3])) << (64 - 32);

			result |= numeric_cast<uint64_t>(std::stoi(aPort)) << 8; // Assume less than  2^24.

			result |= (aisServer ? 1 : 0);
		}
		catch(std::exception& e)
		{
			// We dont want to fail here, just log an error
			LOGERROR("Failed trying to Parse the V4 IP Address " + aEndPoint+ " Error "+e.what());
		}
		return result;
	}

	~CBConnection();

	// Will do Baker/Conitel swap if needed
	static void Write(uint64_t ChannelID, const CBMessage_t &CompleteCBMessage);

	static void InjectSimulatedTCPMessage(uint64_t ChannelID, buf_t&readbuf);

	static void SetSendTCPDataFn(uint64_t ChannelID, std::function<void(std::string)> f);

private:
	asio::io_service* pIOS;
	std::string EndPoint;
	std::string Port;
	std::string InternalChannelID;

	bool IsServer;
	std::atomic_bool enabled = false;
	bool IsBakerDevice; // Swap Station and Group if it is a Baker device. Set in constructor

	CBMessage_t CBMessage;

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr; // nullptr normally. Set to hook function for testing

	// Need maps for these two...
	std::unordered_map<uint8_t, std::function<void(CBMessage_t &CBMessage)>> ReadCallbackMap;
	std::unordered_map<uint8_t, std::function<void(bool)>> StateCallbackMap;

	std::shared_ptr<TCPSocketManager<std::string>> pSockMan;

	// A list of CBConnections, so that we can find if one for out port/address combination already exists.
	static std::unordered_map<uint64_t, std::shared_ptr<CBConnection>> ConnectionMap;
	static std::mutex MapMutex; // Control access

	void Open();
	void Close();
	void Write(std::string &msg);
	void RouteCBMessage(CBMessage_t &CompleteCBMessage);
	void SocketStateHandler(bool state);
	// Two static methods to manage the map of connections. Can only have one for an address/port combination.
	static bool GetConnection(uint64_t ChannelID, std::shared_ptr<CBConnection> &pConnection);

	// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
	// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
	void ReadCompletionHandler(buf_t& readbuf);

};
#endif

