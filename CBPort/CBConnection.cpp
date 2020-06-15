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

#include "CB.h"
#include "CBConnection.h"
#include "CBUtility.h"
#include <functional>
#include <opendatacon/TCPSocketManager.h>
#include <opendatacon/asio.h>
#include <string>
#include <unordered_map>
#include <utility>

using namespace odc;

std::unordered_map<std::string, std::shared_ptr<CBConnection>> CBConnection::ConnectionMap;
std::mutex CBConnection::MapMutex; // Control access


ConnectionTokenType::~ConnectionTokenType()
{
	// If we are the last connectiontoken for this connection (usecount == 2 , the Map and us) - delete the connection.
	if (pConnection)
	{
		if (pConnection.use_count() <= 2)
		{
			LOGDEBUG("Use Count On ConnectionTok Shared_ptr down to 2 - Destroying the Map Connection - {}", ChannelID);
			pConnection->Close();
			pConnection->RemoveConnectionFromMap();
			// Now release our shared_ptr - the last one.The CBConnection destructor should now be called.
			pConnection.reset();
			LOGDEBUG("Map Connection Destroyed - {}", ChannelID);
		}
		else
		{
			LOGDEBUG("Use Count On Connection Shared_ptr at {} - The Map Connection Stays - {}", pConnection.use_count(), ChannelID);
		}
	}
}

CBConnection::CBConnection
(
	std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
	bool aisServer,                           //Whether to act as a server or client
	const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                 //Port to connect to if client, or listen on if server
	bool isbakerdevice,
	uint16_t retry_time_ms
):
	pIOS(std::move(apIOS)),
	EndPoint(aEndPoint),
	Port(aPort),
	IsServer(aisServer),
	IsBakerDevice(isbakerdevice)
{
	pSockMan = std::make_shared<TCPSocketManager<std::string>>
		           (pIOS, IsServer, EndPoint, Port,
		           std::bind(&CBConnection::ReadCompletionHandler, this, std::placeholders::_1),
		           std::bind(&CBConnection::SocketStateHandler, this, std::placeholders::_1),
		           std::numeric_limits<size_t>::max(),
		           true,
		           retry_time_ms);

	InternalChannelID = MakeChannelID(aEndPoint, aPort, aisServer);


	LOGDEBUG("Opened an CBConnection object {} As a {} - {}",InternalChannelID, (IsServer ? "Server" : "Client"), (IsBakerDevice ? " Baker Device" : " Conitel Device"));
}

// Static Method
ConnectionTokenType CBConnection::AddConnection
(
	std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
	bool aisServer,                           //Whether to act as a server or client
	const std::string& aEndPoint,             //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,                 //Port to connect to if client, or listen on if server
	bool isbakerdevice,
	uint16_t retry_time_ms
)
{
	std::string ChannelID = MakeChannelID(aEndPoint, aPort, aisServer);

	std::unique_lock<std::mutex> lck(CBConnection::MapMutex); // Access the Map - only here and in the destructor do we do this.

	// Only add if does not exist
	if (ConnectionMap.count(ChannelID) == 0)
	{
		// If we give each connectiontoken a shared_ptr to the connection, then in the Connectiontoken destructors,
		// when the use_count is 2 (the map and the connectiontoken), then it is time to destroy the connection.

		ConnectionMap[ChannelID] = std::make_shared<CBConnection>(apIOS, aisServer, aEndPoint, aPort, isbakerdevice, retry_time_ms);
	}
	else
		LOGDEBUG("ConnectionTok already exists, using that connection - {}",ChannelID);

	return ConnectionTokenType(ChannelID, ConnectionMap[ChannelID]); // the use count on the Connection Map shared_ptr will go up by one.
}

void CBConnection::AddOutstation(const ConnectionTokenType &ConnectionTok, uint8_t StationAddress, // For message routing, OutStation identification
	const std::function<void(CBMessage_t &CBMessage)>& aReadCallback,
	const std::function<void(bool)>& aStateCallback,
	bool isbakerdevice)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		if (isbakerdevice != pConnection->IsBakerDevice)
		{
			if (pConnection->IsBakerDevice)
			{
				LOGERROR("Tried to add a Conitel outstation to a Baker ConnectionTok");
			}
			else
			{
				LOGERROR("Tried to add a Baker outstation to a Conitel ConnectionTok");
			}
		}
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[StationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[StationAddress] = aStateCallback;
	}
	else
	{
		LOGERROR("Tried to add an Outstation when the connection was no longer valid");
	}
}
//Static Method
void CBConnection::RemoveOutstation(const ConnectionTokenType &ConnectionTok, uint8_t StationAddress)
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

void CBConnection::AddMaster(const ConnectionTokenType &ConnectionTok, uint8_t TargetStationAddress, // For message routing, Master is expecting replies from what Outstation?
	const std::function<void(CBMessage_t &CBMessage)>& aReadCallback,
	const std::function<void(bool)>& aStateCallback,
	bool isbakerdevice)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		if (isbakerdevice != pConnection->IsBakerDevice)
		{
			if (pConnection->IsBakerDevice)
			{
				LOGERROR("Tried to add a Conitel Master to a Baker ConnectionTok");
			}
			else
			{
				LOGERROR("Tried to add a Baker Master to a Conitel ConnectionTok");
			}
		}
		// Save the callbacks to two maps for the multidrop stations on this connection for quick access
		pConnection->ReadCallbackMap[TargetStationAddress] = aReadCallback; // Will overwrite if duplicate
		pConnection->StateCallbackMap[TargetStationAddress] = aStateCallback;
	}
	else
	{
		LOGERROR("Tried to add a Master when the connection was no longer valid");
	}
}
// Static Method
void CBConnection::RemoveMaster(const ConnectionTokenType &ConnectionTok, uint8_t TargetStationAddress)
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
void CBConnection::RemoveConnectionFromMap()
{
	std::unique_lock<std::mutex> lck(CBConnection::MapMutex);
	ConnectionMap.erase(InternalChannelID); // Remove the map entry shared ptr - so that shared ptr no longer points to us.
}

// Static Method
void CBConnection::Open(const ConnectionTokenType &ConnectionTok)
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

void CBConnection::Open()
{
	if (!pSockMan)
		throw std::runtime_error("Socket manager uninitialised for - " + InternalChannelID);

	// We have a potential lock/race condition on a failed open, hence the two atomics to track this. Could not work out how to do it with one...
	if (!successfullyopened.exchange(true))
	{
		// Port has not been opened yet.
		try
		{
			pSockMan->Open();
			opencount.fetch_add(1);
			LOGDEBUG("ConnectionTok Opened: {}", InternalChannelID);
		}
		catch (std::exception& e)
		{
			LOGERROR("Problem opening connection :{} - {}", InternalChannelID, e.what());
			successfullyopened.exchange(false);
			return;
		}
	}
	else
	{
		opencount.fetch_add(1);
		LOGDEBUG("Connection increased open count: {} {}", opencount.load(), InternalChannelID);
	}
}
// Static Method
void CBConnection::Close(const ConnectionTokenType &ConnectionTok)
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

void CBConnection::Close()
{
	if (!pSockMan) // Could be empty if a connection was never added (opened)
		return;

	// If we are down to the last connection that needs it open, then close it
	if ((opencount.fetch_sub(1) == 1) && successfullyopened.load())
	{
		pSockMan->Close();
		successfullyopened.exchange(false);

		LOGDEBUG("Connection open count 0, Closed: {}", InternalChannelID);
	}
	else
	{
		LOGDEBUG("Connection reduced open count: {} {}", opencount.load(), InternalChannelID);
	}
}

CBConnection::~CBConnection()
{
	Close();

	if (!pSockMan) // Could be empty if a connection was never added (opened)
	{
		LOGDEBUG("Connection Destructor pSockMan null: {}", InternalChannelID);
		return;
	}
	pSockMan->Close();

	pSockMan.reset(); // Release our object - should be done anyway when as soon as we exit this method...
	LOGDEBUG("Connection Destructor Complete : {}", InternalChannelID);
}

// Static method
void CBConnection::Write(const ConnectionTokenType &ConnectionTok,const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
		return;
	}
	// Turn the blocks into a binary string.

	if (auto pConnection = ConnectionTok.pConnection) // Dont do if connection not valid
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
	else
	{
		LOGERROR("Tried to write to a connection when the connection was no longer valid");
	}
}
//Static method
void CBConnection::SetSendTCPDataFn(const ConnectionTokenType &ConnectionTok, std::function<void(std::string)> f)
{
	if (auto pConnection = ConnectionTok.pConnection)
	{
		pConnection->SendTCPDataFn = std::move(f);
	}
	else
	{
		LOGERROR("Tried to setsendTCPdataFn when the connection was no longer valid");
	}
}

// We don't need to know who is doing the writing. Just pass to the socket
void CBConnection::Write(std::string &msg)
{
	if (pSockMan)
	{
		pSockMan->Write(std::string(msg)); // Strange, it requires the std::string() constructor to be passed otherwise the templating fails.
	}
}
//Static method
void CBConnection::InjectSimulatedTCPMessage(const ConnectionTokenType &ConnectionTok, buf_t&readbuf)
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

// We need one read completion handler hooked to each address/port combination.
// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
// The TCPSocketManager class ensures that this callback is not called again with more data until it is complete.
// Does not make sense for it to do anything else on a TCP stream which must (should) be sequentially processed.
void CBConnection::ReadCompletionHandler(buf_t&readbuf)
{
	// We need to treat the TCP data as a stream, just like a serial stream. The first block (4 bytes) is probably the start block, but we cannot assume this.
	// Also we could get more than one message in a TCP block so need to handle this correctly.
	// We will build a CBMessage until we get the end condition. If we get a new start block, we will have to dump anything in the CBMessage and start again.

	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	if (!successfullyopened.load())
	{
		LOGDEBUG("CBConnection called ReadCompletionHandler when not opened - ignoring");
		return;
	}

	while (readbuf.size() > 0)
	{
		// Add another byte to our 4 byte block.

		ReadCompletionHandlerCBblock.AddByteToBlock(static_cast<uint8_t>(readbuf.sgetc()));
		readbuf.consume(1);

		if (ReadCompletionHandlerCBblock.IsValidBlock()) // Check checksum and B bit.
		{
			if (ReadCompletionHandlerCBblock.IsAddressBlock())
			{
				if (IsBakerDevice)
				{
					// If Is a Baker device, swap Station and Group values before passing up the line to where the address is checked and routed...
					ReadCompletionHandlerCBblock.DoBakerConitelSwap();
				}

				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if CBMessage is empty.
				if (CBMessage.size() != 0)
				{
					LOGDEBUG("Received a start block {} when we have not got to an end block - discarding data blocks - {} and storing this start block", ReadCompletionHandlerCBblock.ToString(), std::to_string(CBMessage.size()));
					CBMessage.clear();
				}
				CBMessage.push_back(ReadCompletionHandlerCBblock); // Takes a copy of the block
			}
			else if (CBMessage.size() == 0)
			{
				LOGDEBUG("Received a non start block when we are waiting for a start block - discarding data - {}", ReadCompletionHandlerCBblock.ToString());
			}
			else if (CBMessage.size() >= MAX_BLOCK_COUNT)
			{
				LOGDEBUG("Received more than 16 blocks in a single CB message - discarding data - {}", ReadCompletionHandlerCBblock.ToString());
				CBMessage.clear(); // Empty message block queue
			}
			else
			{
				CBMessage.push_back(ReadCompletionHandlerCBblock); // Takes a copy of the block
			}

			// The start and any other block can be the end block
			// We might get an end block out of order, so just ignore.
			if (ReadCompletionHandlerCBblock.IsEndOfMessageBlock() && (CBMessage.size() != 0))
			{
				// Once we have the last block, then hand off CBMessage to process.
				RouteCBMessage(CBMessage);
				CBMessage.clear(); // Empty message block queue
			}

			// We have got a valid block, so empty the collection block so we can get the next one.
			ReadCompletionHandlerCBblock.Clear();
		}
		else
		{
			// The block was not valid, we will push another byte in (and one out) and try again (think shifting 4 byte window)
		}
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
		LOGDEBUG("Received a zero station address routing to all outstations - {}", CBMessageAsString(CompleteCBMessage));
		for (auto it = ReadCallbackMap.begin(); it != ReadCallbackMap.end(); ++it)
			it->second(CompleteCBMessage);
	}
	else if (ReadCallbackMap.count(StationAddress) != 0)
	{
		// We have found a matching outstation, do read callback
		LOGDEBUG("Routing Message to station - {} Message - {}",std::to_string(StationAddress),CBMessageAsString(CompleteCBMessage));
		ReadCallbackMap.at(StationAddress)(CompleteCBMessage);
	}
	else
	{
		// NO match
		LOGDEBUG("Received non-matching outstation address - {} Message - {}", std::to_string(StationAddress), CBMessageAsString(CompleteCBMessage));
	}
}

void CBConnection::SocketStateHandler(bool state)
{
	LOGDEBUG("ConnectionTok changed state {} As a {}", InternalChannelID , (state ? "Open" : "Close"));

	// Call all the OutStation State Callbacks
	for (auto it = StateCallbackMap.begin(); it != StateCallbackMap.end(); ++it)
	{
		if (it->second) // Check the validity of the callback pointer first
			it->second(state);
	}
}


