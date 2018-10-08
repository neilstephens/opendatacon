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
 * CBConnection.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>

#include "CB.h"
#include "CBUtility.h"
#include "CBConnection.h"

using namespace odc;

std::unordered_map<uint64_t, std::shared_ptr<CBConnection>> CBConnection::ConnectionMap;
std::mutex CBConnection::MapMutex; // Control access

CBConnection::CBConnection (asio::io_service* apIOS, //pointer to an asio io_service
	bool aisServer,                                //Whether to act as a server or client
	const std::string& aEndPoint,                  //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                      //Port to connect to if client, or listen on if server
	bool isbakerdevice,
	uint16_t retry_time_ms):
	pIOS(apIOS),
	EndPoint(aEndPoint),
	Port(aPort),
	IsBakerDevice(isbakerdevice),
	IsServer(aisServer)
{
	pSockMan.reset(new TCPSocketManager<std::string>
			(pIOS, IsServer, EndPoint, Port,
			std::bind(&CBConnection::ReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&CBConnection::SocketStateHandler, this, std::placeholders::_1),
			std::numeric_limits<size_t>::max(),
			true,
			retry_time_ms));

	InternalChannelID = MakeStringChannelID(aEndPoint, aPort, aisServer);


	LOGDEBUG("Opened an CBConnection object " + InternalChannelID + " As a " + (IsServer ? "Server" : "Client") + (IsBakerDevice ? " Baker Device" : " Conitel Device"));
}

void CBConnection::AddConnection(asio::io_service* apIOS, //pointer to an asio io_service
	bool aisServer,                                     //Whether to act as a server or client
	const std::string& aEndPoint,                       //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                           //Port to connect to if client, or listen on if server
	bool isbakerdevice,
	uint16_t retry_time_ms)
{
	// This map is of weak pointers, so that if the Connection gets destroyed (it is a shared_ptr) the weak pointer will know that it no longer exists.
	// This can occur as the map is a static object.
	// It can then return a null_ptr. We need to make sure every call to GetConnection can cope with a null_ptr returned.

	uint64_t ChannelID = MakeChannelID(aEndPoint, aPort, aisServer);

	std::unique_lock<std::mutex> lck(CBConnection::MapMutex); // Access the Map

	// Only add if does not exist
	if (ConnectionMap.count(ChannelID) == 0)
	{
		auto pConnection = std::make_shared<CBConnection>(apIOS, aisServer, aEndPoint, aPort, isbakerdevice, retry_time_ms);

		ConnectionMap[ChannelID] = pConnection;
	}
	else
		LOGDEBUG("Connection already exists, using that connection - {}",ChannelID);
}

void CBConnection::AddOutstation(uint64_t ChannelID, uint8_t StationAddress, // For message routing, OutStation identification
	const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
	const std::function<void(bool)> aStateCallback,
	bool isbakerdevice)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
	{
		if (isbakerdevice != pConnection->IsBakerDevice)
		{
			if (pConnection->IsBakerDevice)
			{
				LOGERROR("Tried to add a Conitel outstation to a Baker Connection");
			}
			else
			{
				LOGERROR("Tried to add a Baker outstation to a Conitel Connection");
			}
		}
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[StationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[StationAddress] = aStateCallback;
	}
}
void CBConnection::RemoveOutstation(uint64_t ChannelID, uint8_t StationAddress)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
	{
		pConnection->ReadCallbackMap.erase(StationAddress);
		pConnection->StateCallbackMap.erase(StationAddress);

		// If we have removed the last StateCallBack - then we are done - delete the connection.
		if (pConnection->StateCallbackMap.size() == 0)
		{
			LOGDEBUG("Remove OutStation - Last OutStation Removed - Destroying the Connection - {}",ChannelID);
			ConnectionMap[ChannelID].reset(); // Destroy the object
			ConnectionMap.erase(ChannelID);   // Remove the map entry
		}
	}
}

void CBConnection::AddMaster(uint64_t ChannelID, uint8_t TargetStationAddress, // For message routing, Master is expecting replies from what Outstation?
	const std::function<void(CBMessage_t &CBMessage)> aReadCallback,
	const std::function<void(bool)> aStateCallback,
	bool isbakerdevice)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
	{
		if (isbakerdevice != pConnection->IsBakerDevice)
		{
			if (pConnection->IsBakerDevice)
			{
				LOGERROR("Tried to add a Conitel Master to a Baker Connection");
			}
			else
			{
				LOGERROR("Tried to add a Baker Master to a Conitel Connection");
			}
		}
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[TargetStationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[TargetStationAddress] = aStateCallback;
	}
}
void CBConnection::RemoveMaster(uint64_t ChannelID, uint8_t TargetStationAddress)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
	{
		pConnection->ReadCallbackMap.erase(TargetStationAddress);
		pConnection->StateCallbackMap.erase(TargetStationAddress);

		// If we have removed the last StateCallBack - then we are done - delete the connection.
		if (pConnection->StateCallbackMap.size() == 0)
		{
			LOGDEBUG("Remove Master - Last Master Removed - Destroying the Connection - " + ChannelID);
			ConnectionMap[ChannelID].reset(); // Destroy the object
			ConnectionMap.erase(ChannelID);   // Remove the map entry
		}
	}
}

bool CBConnection::GetConnection(uint64_t ChannelID, std::shared_ptr<CBConnection> &pConnection)
{
	// Check if the entry exists without adding to the map..
	std::unique_lock<std::mutex> lck(CBConnection::MapMutex);

	if (ConnectionMap.count(ChannelID) == 0)
	{
		LOGERROR("Connection Does not Exist - GetConnection - " + ChannelID);
		return false;
	}
	// Check if the connection is expired
	pConnection = ConnectionMap[ChannelID];
	return true;
}

void CBConnection::Open(uint64_t ChannelID)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
		pConnection->Open();
}

void CBConnection::Open()
{
	if (enabled) return;

	try
	{
		if (pSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitialised for - " + InternalChannelID);

		pSockMan->Open();
		enabled = true;
		LOGDEBUG("Connection Opened: " + InternalChannelID);
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection : " + InternalChannelID + " - " + e.what());
		return;
	}
}
void CBConnection::Close(uint64_t ChannelID)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID,pConnection))
		pConnection->Close();
}

void CBConnection::Close()
{
	if (!enabled) return;
	enabled = false;

	if (pSockMan.get() == nullptr)
		return;
	pSockMan->Close();

	LOGDEBUG("Connection Closed: " + InternalChannelID);
}

CBConnection::~CBConnection()
{
	Close();
}

void CBConnection::Write(uint64_t ChannelID,const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
		return;
	}
	// Turn the blocks into a binary string.

	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
	{
		std::string CBMessageString;

		CBBlockData blk = CompleteCBMessage[0];

		if (pConnection->IsBakerDevice)
		{
			// If Is a Baker device, swap Station and Group values before sending...
			blk.DoBakerConitelSwap();
		}
		CBMessageString += blk.ToBinaryString();

		// Done the first block, now do the rest
		for (uint8_t i = 1; i < CompleteCBMessage.size(); i++)
		{
			CBMessageString += CompleteCBMessage[i].ToBinaryString();
		}

		// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
		// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function due to differences between a method and a lambda
		if (pConnection->SendTCPDataFn != nullptr)
		{
			pConnection->SendTCPDataFn(CBMessageString);
		}
		else
		{
			pConnection->Write(CBMessageString);
		}
	}
}

void CBConnection::SetSendTCPDataFn(uint64_t ChannelID, std::function<void(std::string)> f)
{
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
		pConnection->SendTCPDataFn = f;
}

// We don't need to know who is doing the writing. Just pass to the socket
void CBConnection::Write(std::string &msg)
{
	pSockMan->Write(std::string(msg)); // Strange, it requires the std::string() constructor to be passed otherwise the templating fails.
}

void CBConnection::InjectSimulatedTCPMessage(uint64_t ChannelID, buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	std::shared_ptr<CBConnection> pConnection;
	if (CBConnection::GetConnection(ChannelID, pConnection))
		pConnection->ReadCompletionHandler(readbuf);
}

// We need one read completion handler hooked to each address/port combination. This method is re-entrant,
// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
void CBConnection::ReadCompletionHandler(buf_t&readbuf)
{
	// We are currently assuming a whole complete packet will turn up in one unit. If not it will be difficult to do the packet decoding and multi-drop routing.
	// CB only has addressing information in the first block of the packet.
	// We should have a multiple of 6 bytes. 5 data bytes and one padding byte for every CB block, then possibly multiple blocks
	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() >= CBBlockArraySize)
	{
		CBBlockArray d;
		for (size_t i = 0; i < CBBlockArraySize; i++)
		{
			d[i] = static_cast<uint8_t>(readbuf.sgetc());
			readbuf.consume(1);
		}

		auto CBblock = CBBlockData(d); // Supposed to be a 6 byte array..

		if (CBblock.BCHPasses())
		{
			if (CBblock.IsAddressBlock())
			{
				if (IsBakerDevice)
				{
					// If Is a Baker device, swap Station and Group values before passing up the line to where the address is checked and routed...
					CBblock.DoBakerConitelSwap();
				}

				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if CBMessage is empty.
				if (CBMessage.size() != 0)
				{
					LOGDEBUG("Received a start block when we have not got to an end block - discarding data blocks - " + std::to_string(CBMessage.size()));
					CBMessage.clear();
				}
				CBMessage.push_back(CBblock); // Takes a copy of the block
			}
			else if (CBMessage.size() == 0)
			{
				LOGDEBUG("Received a non start block when we are waiting for a start block - discarding data - " + CBblock.ToString());
			}
			else if (CBMessage.size() >= MAX_BLOCK_COUNT)
			{
				LOGDEBUG("Received more than 16 blocks in a single CB message - discarding data - " + CBblock.ToString());
				CBMessage.clear(); // Empty message block queue
			}
			else
			{
				CBMessage.push_back(CBblock); // Takes a copy of the block
			}

			// The start and any other block can be the end block
			// We might get an end block out of order, so just ignore.
			if (CBblock.IsEndOfMessageBlock() && (CBMessage.size() != 0))
			{
				// Once we have the last block, then hand off CBMessage to process.
				RouteCBMessage(CBMessage);
				CBMessage.clear(); // Empty message block queue
			}
		}
		else
		{
			LOGERROR("Checksum failure on received CB block - " + CBblock.ToString());
		}
	}

	// Check for and consume any not 6 byte block data - should never happen...
	if (readbuf.size() > 0)
	{
		size_t bytesleft = readbuf.size();
		LOGDEBUG("Had data left over after reading blocks - " + std::to_string(bytesleft) + " bytes");
		readbuf.consume(readbuf.size());
	}
}
void CBConnection::RouteCBMessage(CBMessage_t &CompleteCBMessage)
{
	// Only passing in the variable to make unit testing simpler.
	// We have a full set of CB message blocks from a minimum of 1.

	// At this point we have already swapped the Station and Group if it is a Baker device

	assert(CompleteCBMessage.size() != 0);

	uint8_t StationAddress = CompleteCBMessage[0].GetStationAddress();

	if (StationAddress == 0)
	{
		// If zero, route to all outstations!
		// Most zero station address functions do not send a response
		LOGDEBUG("Received a zero station address routing to all outstations - " + CBMessageAsString(CompleteCBMessage));
		for (auto it = ReadCallbackMap.begin(); it != ReadCallbackMap.end(); ++it)
			it->second(CompleteCBMessage);
	}
	else if (ReadCallbackMap.count(StationAddress) != 0)
	{
		// We have found a matching outstation, do read callback
		LOGDEBUG("Routing Message to station - " +std::to_string(StationAddress)+" Message - " + CBMessageAsString(CompleteCBMessage));
		ReadCallbackMap.at(StationAddress)(CompleteCBMessage);
	}
	else
	{
		// NO match
		LOGDEBUG("Received non-matching outstation address - " + std::to_string(StationAddress) + " Message - " + CBMessageAsString(CompleteCBMessage));
	}
}

void CBConnection::SocketStateHandler(bool state)
{
	LOGDEBUG("Connection changed state " + InternalChannelID + " As a " + (state ? "Open" : "Close"));

	// Call all the OutStation State Callbacks
	for (auto it = StateCallbackMap.begin(); it != StateCallbackMap.end(); ++it)
		it->second(state);
}


