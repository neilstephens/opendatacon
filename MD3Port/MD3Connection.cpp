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
#include <opendnp3/LogLevels.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3Connection.h"

using namespace odc;

// Hide some of the code to make Logging cleaner - slightly different version to the rest of the code.
#define LOG(logger, filters, location, msg) \
	pParentPort->pLoggers->Log(openpal::LogEntry(logger, filters, location, std::string(msg).c_str(),-1));


std::unordered_map<std::string, std::shared_ptr<MD3Connection>> MD3Connection::ConnectionMap;

MD3Connection::MD3Connection (asio::io_service* apIOS,                   //pointer to an asio io_service
	bool aisServer,							//Whether to act as a server or client
	const std::string& aEndPoint,				//IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aPort,					//Port to connect to if client, or listen on if server
	const MD3Port *OutStationPortInstance,	// Messy, just used so we can access pLogger
	bool aauto_reopen,                 //Keeps the socket open (retry on error), unless you explicitly Close() it
	uint16_t aretry_time_ms) :
	EndPoint(aEndPoint),
	Port(aPort),
	pIOS(apIOS),
	isServer(aisServer),
	pParentPort(OutStationPortInstance),
	auto_reopen(aauto_reopen),
	retry_time_ms(aretry_time_ms)
{
	pSockMan.reset(new TCPSocketManager<std::string>
		(pIOS, isServer, EndPoint, Port,
			std::bind(&MD3Connection::ReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&MD3Connection::SocketStateHandler, this, std::placeholders::_1),
			true,
			retry_time_ms));
	ChannelID = EndPoint + ":" + Port;

	LOG("MD3Port", openpal::logflags::WARN, "", "Opened an MD3Connection object "+ChannelID);
}

void MD3Connection::AddOutstation(uint8_t StationAddress,	// For message routing, OutStation identification
	const std::function<void(std::vector < MD3BlockData > MD3Message)> aReadCallback,
	const std::function<void(bool)> aStateCallback)
{
	// Save the callbacks to two maps for the multidrop stations on this connection for quick access
	ReadCallbackMap[StationAddress] = aReadCallback;	// Will overwrite if duplicate
	StateCallbackMap[StationAddress] = aStateCallback;
}

void MD3Connection::AddMaster(uint8_t TargetStationAddress,	// For message routing, Master is expecting replies from what Outstation?
	const std::function<void(std::vector < MD3BlockData > MD3Message)> aReadCallback,
	const std::function<void(bool)> aStateCallback)
{
	// Save the callbacks to two maps for the multidrop stations on this connection for quick access
	ReadCallbackMap[TargetStationAddress] = aReadCallback;	// Will overwrite if duplicate
	StateCallbackMap[TargetStationAddress] = aStateCallback;
}

// Two static methods to manage the map of connections. Can only have one for an address/port combination.
//TODO: SJE How to remove/shut down the list of connections and outstations. How to do this on a rebuild?
std::shared_ptr<MD3Connection> MD3Connection::GetConnection(std::string ChannelID)
{
	// Check if the entry exists without adding to the map..
	if (ConnectionMap.count(ChannelID) == 0)
		return nullptr;

	return ConnectionMap[ChannelID];
}
void MD3Connection::AddConnection(std::string ChannelID, std::shared_ptr<MD3Connection> pConnection)
{
	ConnectionMap[ChannelID] = pConnection;
}

void MD3Connection::Open()
{
	std::string ChannelID = EndPoint + ":" + Port;
	if (enabled) return;
	try
	{
		if (pSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitilised for - " + ChannelID);

		pSockMan->Open();
		enabled = true;
	}
	catch (std::exception& e)
	{
		LOG("MD3Port", openpal::logflags::ERR, "", "Problem opening connection : " + ChannelID + " - " + e.what());
		return;
	}
}

void MD3Connection::Close()
{
	if (!enabled) return;
	enabled = false;

	if (pSockMan.get() == nullptr)
		return;
	pSockMan->Close();
}

MD3Connection::~MD3Connection()
{
	Close();
	// Remove outselves from the static list of connections
	ConnectionMap.erase(ChannelID);
}

// We dont need to know who is doing the writing. Just pass to the socket
void MD3Connection::Write(std::string &msg)
{
	pSockMan->Write(std::string(msg));  // Strange, it requires the std::string() constructor to be passed otherwise the templating fails.
}

// We need one read completion handler hooked to each address/port combination. This method is reentrant,
// We do some basic MD3 block identification and procesing, enough to give us complete blocks and StationAddresses
void MD3Connection::ReadCompletionHandler(buf_t&readbuf)
{
	// We are currently assuming a whole complete packet will turn up in one unit. If not it will be difficult to do the packet decoding and multidrop routing.
	// MD3 only has addressing information in the first block of the packet.

	// We should have a multiple of 6 bytes. 5 data bytes and one padding byte for every MD3 block, then possibkly mutiple blocks
	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.
	static std::vector<MD3BlockData> MD3Message;

	while (readbuf.size() >= MD3BlockArraySize)
	{
		MD3BlockArray d;
		for (int i = 0; i < MD3BlockArraySize; i++)
		{
			d[i] = readbuf.sgetc();
			readbuf.consume(1);
		}

		auto md3block = MD3BlockData(d);    // Supposed to be a 6 byte array..

		if (md3block.CheckSumPasses())
		{
			if (md3block.IsFormattedBlock())
			{
				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if MD3Message is empty.
				if (MD3Message.size() != 0)
				{
					LOG("MD3Port", openpal::logflags::WARN, "", "Received a start block when we have not got to an end block - discarding data blocks - " + std::to_string(MD3Message.size()));
					MD3Message.clear();
				}
				MD3Message.push_back(md3block); // Takes a copy of the block
			}
			else if (MD3Message.size() == 0)
			{
				LOG("MD3Port", openpal::logflags::WARN, "", "Received a non start block when we are waiting for a start block - discarding data - " + md3block.ToPrintString());
			}
			else
			{
				MD3Message.push_back(md3block); // Takes a copy of the block
			}

			// The start and any other block can be the end block
			// We might get an end block outof order, so just ignore.
			if (md3block.IsEndOfMessageBlock() && (MD3Message.size() != 0))
			{
				// Once we have the last block, then hand off MD3Message to process.
				RouteMD3Message(MD3Message);
				MD3Message.clear(); // Empty message block queue
			}
		}
		else
		{
			LOG("MD3Port", openpal::logflags::ERR, "", "Checksum failure on received MD3 block - " + md3block.ToPrintString());
		}
	}

	// Check for and consume any not 6 byte block data - should never happen...
	if (readbuf.size() > 0)
	{
		int bytesleft = readbuf.size();
		LOG("MD3Port", openpal::logflags::WARN, "", "Had data left over after reading blocks - " + std::to_string(bytesleft) + " bytes");
		readbuf.consume(readbuf.size());
	}
}
void MD3Connection::RouteMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
{
	// Only passing in the variable to make unit testing simpler.
	// We have a full set of MD3 message blocks from a minimum of 1.
	assert(CompleteMD3Message.size() != 0);

	uint8_t StationAddress = ((MD3BlockFormatted)CompleteMD3Message[0]).GetStationAddress();

	if (StationAddress == 0)
	{
		// If zero, route to all outstations!
		// Most zero station address functions do not send a response - the SystemSignOnMessage is an exception.

		for (auto it = ReadCallbackMap.begin(); it != ReadCallbackMap.end(); ++it)
			it->second(CompleteMD3Message);
	}
	else if (ReadCallbackMap.count(StationAddress) != 0)
	{
		// We have found a matching outstation, do read callback
		ReadCallbackMap.at(StationAddress)(CompleteMD3Message);
	}
	else
	{
		// NO match
		LOG("MD3Port", openpal::logflags::ERR, "", "Recevied non-matching outstation address - " + std::to_string(StationAddress));

	}
}

void MD3Connection::SocketStateHandler(bool state)
{
	// Call all the OutStation State Callbacks
	for (auto it = StateCallbackMap.begin(); it != StateCallbackMap.end(); ++it)
		it->second(state);
}


