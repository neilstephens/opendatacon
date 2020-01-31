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
 * PortConnection.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#include <opendatacon/asio.h>
#include <opendatacon/TCPSocketManager.h>
#include <string>
#include <functional>
#include <unordered_map>

#include "PortConnection.h"

using namespace odc;


PortProxyConnection::PortProxyConnection
(
	std::shared_ptr<odc::asio_service> apIOS, //pointer to an asio io_service
	const std::string& aServerEndPoint,       //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aServerPort,           //Port to connect to if client, or listen on if server
	const std::string& aClientEndPoint,       //IP addr or hostname (to connect to if client, or bind to if server)
	const std::string& aClientPort,           //Port to connect to if client, or listen on if server
	uint16_t retry_time_ms
):
	pIOS(apIOS),
	ServerEndPoint(aServerEndPoint),
	ServerPort(aServerPort),
	ClientEndPoint(aClientEndPoint),
	ClientPort(aClientPort)
{
	pServerSockMan.reset(new TCPSocketManager<std::string>
			(pIOS, true, ServerEndPoint, ServerPort,
			std::bind(&PortProxyConnection::ServerReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&PortProxyConnection::ServerSocketStateHandler, this, std::placeholders::_1),
			std::numeric_limits<size_t>::max(),
			true,
			retry_time_ms));

	pClientSockMan.reset(new TCPSocketManager<std::string>
			(pIOS, false, ClientEndPoint, ClientPort,
			std::bind(&PortProxyConnection::ClientReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&PortProxyConnection::ClientSocketStateHandler, this, std::placeholders::_1),
			std::numeric_limits<size_t>::max(),
			true,
			retry_time_ms));

	InternalChannelID = fmt::format("{}:{} to {}:{}", ServerEndPoint, ServerPort,ClientEndPoint,ClientPort);

	LOGDEBUG("Opened a Port Proxy Listener object {}", InternalChannelID);
}


void PortProxyConnection::Open()
{
	try
	{
		if (pServerSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitialised for - " + InternalChannelID);

		pServerSockMan->Open();
		LOGDEBUG("Connection Opened: {}", InternalChannelID);
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection :{} - {}",InternalChannelID, e.what());
		return;
	}
}

void PortProxyConnection::Close()
{
	if (pClientSockMan)
		pClientSockMan->Close();
	if (pServerSockMan)
		pServerSockMan->Close();

	LOGDEBUG("Connection Closed: {}", InternalChannelID);
}

PortProxyConnection::~PortProxyConnection()
{
	Close();

	if (!pServerSockMan) // Could be empty if a connection was never added (opened)
	{
		LOGDEBUG("Connection Destructor pSockMan null: {}", InternalChannelID);
		return;
	}
	pServerSockMan->Close();

	pServerSockMan.reset(); // Release our object - should be done anyway when as soon as we exit this method...
	LOGDEBUG("Connection Destructor Complete : {}", InternalChannelID);
}

// We need one read completion handler hooked to each address/port combination.
// We do some basic CB block identification and processing, enough to give us complete blocks and StationAddresses
// The TCPSocketManager class ensures that this callback is not called again with more data until it is complete.
// Does not make sense for it to do anything else on a TCP stream which must (should) be sequentially processed.
void PortProxyConnection::ServerReadCompletionHandler(buf_t&readbuf)
{
	// We need to treat the TCP data as a stream, just like a serial stream. The first block (4 bytes) is probably the start block, but we cannot assume this.
	// Also we could get more than one message in a TCP block so need to handle this correctly.
	// We will build a CBMessage until we get the end condition. If we get a new start block, we will have to dump anything in the CBMessage and start again.

	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() > 0)
	{
		size_t len = readbuf.size();
		std::string buffer(len, ' ');

		for (size_t i = 0; i < len; i++)
		{
			buffer[i] = readbuf.sgetc();
			readbuf.consume(1);
		}
		LOGDEBUG("Server received {}", buffer);
		if (pClientSockMan && pClientSockMan->IsConnected())
		{
			pClientSockMan->Write(std::string(buffer));
		}
	}
}

void PortProxyConnection::ServerSocketStateHandler(bool state)
{
	LOGDEBUG("Server changed state {} As a {}", InternalChannelID , (state ? "Open" : "Close"));

	if (state)
	{
		LOGDEBUG("Opening Client Connection");
		//pIOS->post([&]()
		//	{
		pClientSockMan->Open();
		//	});
	}
	else
	{
		LOGDEBUG("Closing Client Connection");
		pClientSockMan->Close();
	}
}
void PortProxyConnection::ClientReadCompletionHandler(buf_t& readbuf)
{
	// We need to treat the TCP data as a stream, just like a serial stream. The first block (4 bytes) is probably the start block, but we cannot assume this.
	// Also we could get more than one message in a TCP block so need to handle this correctly.
	// We will build a CBMessage until we get the end condition. If we get a new start block, we will have to dump anything in the CBMessage and start again.

	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() > 0)
	{
		size_t len = readbuf.size();
		std::string buffer(len, ' ');

		for (size_t i = 0; i < len; i++)
		{
			buffer[i] = readbuf.sgetc();
			readbuf.consume(1);
		}
		LOGDEBUG("Client received {}", buffer);
		if (pServerSockMan && pServerSockMan->IsConnected())
		{
			//pServerSockMan->Write(std::string(buffer));
		}
	}
}

void PortProxyConnection::ClientSocketStateHandler(bool state)
{
	LOGDEBUG("Client changed state {} As a {}", InternalChannelID, (state ? "Open" : "Close"));
	// If client closed, do we let it just try and reconnect?
}


