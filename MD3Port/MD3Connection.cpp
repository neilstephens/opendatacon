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
 * MD3Connection.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>

#include "MD3.h"
#include "MD3Utility.h"
#include "MD3Connection.h"

using namespace odc;


std::unordered_map<std::string, std::shared_ptr<MD3Connection>> MD3Connection::ConnectionMap;
std::mutex MD3Connection::MapMutex;

ConnectionTokenType::~ConnectionTokenType()
{
	// If we are the last connectiontoken for this connection (usecount == 2 , the Map and us) - delete the connection.
	if (pConnection)
	{
		if (pConnection.use_count() <= 2)
		{
			LOGDEBUG("Use Count On Connection Shared_ptr down to 2 - Destroying the Map Connection - {}", ChannelID);
			pConnection->RemoveConnectionFromMap();
			// Now release our shared_ptr - the last one..The CBConnection destructor should now be called.
			pConnection.reset();
		}
		else
		{
			LOGDEBUG("Use Count On Connection Shared_ptr at {} - The Map Connection Stays - {}", pConnection.use_count(), ChannelID);
		}
	}
}

MD3Connection::MD3Connection (asio::io_service* apIOS, //pointer to an asio io_service
	bool aisServer,                                  //Whether to act as a server or client
	const std::string& aEndPoint,                    //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                        //Port to connect to if client, or listen on if server
	uint16_t retry_time_ms):
	pIOS(apIOS),
	EndPoint(aEndPoint),
	Port(aPort),
	isServer(aisServer)
{
	pSockMan.reset(new TCPSocketManager<std::string>
			(pIOS, isServer, EndPoint, Port,
			std::bind(&MD3Connection::ReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&MD3Connection::SocketStateHandler, this, std::placeholders::_1),
			std::numeric_limits<size_t>::max(),
			true,
			retry_time_ms));

	ChannelID = MakeChannelID(aEndPoint, aPort, aisServer);

	LOGDEBUG("Opened an MD3Connection object " + ChannelID + " As a " + (isServer ? "Server" : "Client"));
}

// Static Method
ConnectionTokenType MD3Connection::AddConnection(asio::io_service* apIOS, //pointer to an asio io_service
	bool aisServer,                                                     //Whether to act as a server or client
	const std::string& aEndPoint,                                       //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                                           //Port to connect to if client, or listen on if server
	uint16_t retry_time_ms)
{
	std::string ChannelID = MakeChannelID(aEndPoint, aPort, aisServer);

	std::unique_lock<std::mutex> lck(MD3Connection::MapMutex); // Access the Map - only here and in the destructor do we do this.

	// Only add if does not exist
	if (ConnectionMap.count(ChannelID) == 0)
	{
		// If we give each connectiontoken a shared_ptr to the connection, then in the Connectiontoken destructors,
		// when the use_count is 2 (the map and the connectiontoken), then it is time to destroy the connection.

		ConnectionMap[ChannelID] = std::make_shared<MD3Connection>(apIOS, aisServer, aEndPoint, aPort, retry_time_ms);
	}
	else
		LOGDEBUG("Connection already exists, using that connection - {}", ChannelID);

	return ConnectionTokenType(ChannelID, ConnectionMap[ChannelID]); // the use count on the Connection Map shared_ptr will go up by one.
}

void MD3Connection::AddOutstation(const ConnectionTokenType &ConnectionTok,
	uint8_t StationAddress, // For message routing, OutStation identification
	const std::function<void(MD3Message_t &MD3Message)> aReadCallback,
	const std::function<void(bool)> aStateCallback)
{

	if (auto pConnection = ConnectionTok.pConnection)
	{
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[StationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[StationAddress] = aStateCallback;
	}
	else
	{
		LOGERROR("Tried to add an Outstation when the connection was no longer valid");
	}
}

void MD3Connection::RemoveOutstation(const ConnectionTokenType &ConnectionTok, uint8_t StationAddress)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->ReadCallbackMap.erase(StationAddress);
		pConnection->StateCallbackMap.erase(StationAddress);
	}
	else
	{
		LOGERROR("Tried to remove an Outstation when the connection was no longer valid");
	}
}

void MD3Connection::AddMaster(const ConnectionTokenType &ConnectionTok, uint8_t TargetStationAddress, // For message routing, Master is expecting replies from what Outstation?
	const std::function<void(MD3Message_t &MD3Message)> aReadCallback,
	const std::function<void(bool)> aStateCallback)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[TargetStationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[TargetStationAddress] = aStateCallback;
	}
	else
	{
		LOGERROR("Tried to add a Master when the connection was no longer valid");
	}
}

void MD3Connection::RemoveMaster(const ConnectionTokenType &ConnectionTok, uint8_t TargetStationAddress)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->ReadCallbackMap.erase(TargetStationAddress);
		pConnection->StateCallbackMap.erase(TargetStationAddress);
	}
	else
	{
		LOGERROR("Tried to remove a Master when the connection was no longer valid");
	}
}

// Use this method only to shut down a connection and destroy it. Only used internally
void MD3Connection::RemoveConnectionFromMap()
{
	std::unique_lock<std::mutex> lck(MD3Connection::MapMutex);
	ConnectionMap.erase(ChannelID); // Remove the map entry shared ptr - so that shared ptr no longer points to us.
}

// Static Method
void MD3Connection::Open(const ConnectionTokenType &ConnectionTok)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->Open();
	}
	else
	{
		LOGERROR("Tried to open a connection when the connection was no longer valid");
	}
}

void MD3Connection::Open()
{
	if (enabled) return;

	try
	{
		if (pSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitialised for - " + ChannelID);

		pSockMan->Open();
		enabled = true;
		LOGDEBUG("Connection Opened: " + ChannelID);
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection : " + ChannelID + " - " + e.what());
		return;
	}
}

// Static Method
void MD3Connection::Close(const ConnectionTokenType &ConnectionTok)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->Close();
	}
	else
	{
		LOGERROR("Tried to close a connection when the connection was no longer valid");
	}
}

void MD3Connection::Close()
{
	if (!enabled) return;
	enabled = false;

	if (!pSockMan) // Could be empty if a connection was never added (opened)
		return;
	pSockMan->Close();

	LOGDEBUG("Connection Closed: " + ChannelID);
}

MD3Connection::~MD3Connection()
{
	Close();

	if (!pSockMan) // Could be empty if a connection was never added (opened)
		return;

	pSockMan.reset(); // Release our object - should be done anyway when the shared_ptr is destructed, but just to make sure...
}

// Static method
void MD3Connection::Write(const ConnectionTokenType &ConnectionTok, const MD3Message_t &CompleteMD3Message)
{
	if (CompleteMD3Message.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
		return;
	}
	// Turn the blocks into a binary string.

	if (auto pConnection = ConnectionTok.pConnection)
	{
		std::string MD3MessageString;

		MD3BlockData blk = CompleteMD3Message[0];

		MD3MessageString += blk.ToBinaryString();

		// Done the first block, now do the rest
		for (uint8_t i = 1; i < CompleteMD3Message.size(); i++)
		{
			MD3MessageString += CompleteMD3Message[i].ToBinaryString();
		}

		// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
		// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function due to differences between a method and a lambda
		if (pConnection->SendTCPDataFn != nullptr)
		{
			pConnection->SendTCPDataFn(MD3MessageString);
		}
		else
		{
			pConnection->Write(MD3MessageString);
		}
	}
	else
	{
		LOGERROR("Tried to write to a connection when the connection was no longer valid");
	}
}

// We don't need to know who is doing the writing. Just pass to the socket
void MD3Connection::Write(std::string &msg)
{
	pSockMan->Write(std::string(msg)); // Strange, it requires the std::string() constructor to be passed otherwise the templating fails.
}

//Static method
void MD3Connection::SetSendTCPDataFn(const ConnectionTokenType &ConnectionTok, std::function<void(std::string)> f)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->SendTCPDataFn = f;
	}
	else
	{
		LOGERROR("Tried to SetSendTCPdataFn when the connection was no longer valid");
	}
}
//Static method
void MD3Connection::InjectSimulatedTCPMessage(const ConnectionTokenType &ConnectionTok, buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->ReadCompletionHandler(readbuf);
	}
	else
	{
		LOGERROR("Tried to InjectSimulatedTCPMessage when the connection was no longer valid");
	}
}

// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
// We do some basic MD3 block identification and processing, enough to give us complete blocks and StationAddresses
void MD3Connection::ReadCompletionHandler(buf_t&readbuf)
{
	// We are currently assuming a whole complete packet will turn up in one unit. If not it will be difficult to do the packet decoding and multi-drop routing.
	// MD3 only has addressing information in the first block of the packet.
	// We should have a multiple of 6 bytes. 5 data bytes and one padding byte for every MD3 block, then possibly multiple blocks
	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() > 0)
	{
		// Add another byte to our 4 byte block.

		ReadCompletionHandlerMD3block.AddByteToBlock(static_cast<uint8_t>(readbuf.sgetc()));
		readbuf.consume(1);

		if (ReadCompletionHandlerMD3block.IsValidBlock()) // Check checksum padding byte and number of chars we have stuffed into the block
		{
			if (ReadCompletionHandlerMD3block.IsFormattedBlock())
			{
				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if MD3Message is empty.
				if (MD3Message.size() != 0)
				{
					LOGDEBUG("Received a start block when we have not got to an end block - discarding data blocks - " + std::to_string(MD3Message.size()));
					MD3Message.clear();
				}
				MD3Message.push_back(ReadCompletionHandlerMD3block); // Takes a copy of the block
			}
			else if (MD3Message.size() == 0)
			{
				LOGDEBUG("Received a non start block when we are waiting for a start block - discarding data - " + ReadCompletionHandlerMD3block.ToString());
			}
			else
			{
				MD3Message.push_back(ReadCompletionHandlerMD3block); // Takes a copy of the block
			}

			// The start and any other block can be the end block
			// We might get an end block out of order, so just ignore.
			if (ReadCompletionHandlerMD3block.IsEndOfMessageBlock() && (MD3Message.size() != 0))
			{
				// Once we have the last block, then hand off MD3Message to process.
				RouteMD3Message(MD3Message);
				MD3Message.clear(); // Empty message block queue
			}

			// We have got a valid block, so empty the collection block so we can get the next one.
			ReadCompletionHandlerMD3block.Clear();
		}
		else
		{
			// The block was not valid, we will push another byte in (and one out) and try again (think shifting 4 byte window)
		}
	}
}
void MD3Connection::RouteMD3Message(MD3Message_t &CompleteMD3Message)
{
	// Only passing in the variable to make unit testing simpler.
	// We have a full set of MD3 message blocks from a minimum of 1.
	assert(CompleteMD3Message.size() != 0);

	uint8_t StationAddress = MD3BlockFormatted(CompleteMD3Message[0]).GetStationAddress();

	if (StationAddress == 0)
	{
		// If zero, route to all outstations!
		// Most zero station address functions do not send a response - the SystemSignOnMessage is an exception.
		LOGDEBUG("Received a zero station address routing to all outstations - " + MD3MessageAsString(CompleteMD3Message));
		for (auto it = ReadCallbackMap.begin(); it != ReadCallbackMap.end(); ++it)
			it->second(CompleteMD3Message);
	}
	else if (ReadCallbackMap.count(StationAddress) != 0)
	{
		// We have found a matching outstation, do read callback
		LOGDEBUG("Routing Message to station - " +std::to_string(StationAddress)+" Message - " + MD3MessageAsString(CompleteMD3Message));
		ReadCallbackMap.at(StationAddress)(CompleteMD3Message);
	}
	else
	{
		// NO match
		LOGDEBUG("Received non-matching outstation address - " + std::to_string(StationAddress) + " Message - " + MD3MessageAsString(CompleteMD3Message));
	}
}

void MD3Connection::SocketStateHandler(bool state)
{
	LOGDEBUG("Connection changed state " + ChannelID + " As a " + (isServer ? "Open" : "Close"));

	// Call all the OutStation State Callbacks
	for (auto it = StateCallbackMap.begin(); it != StateCallbackMap.end(); ++it)
		it->second(state);
}


