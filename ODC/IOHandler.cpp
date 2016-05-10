/*	opendatacon
 *
 *	Copyright (c) 2014:
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
 * IOHandler.cpp
 *
 *  Created on: 20/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <openpal/logging/LogLevels.h>
#include <opendatacon/IOHandler.h>

std::unordered_map<std::string,IOHandler*> IOHandler::IOHandlers;

std::unordered_map<std::string, IOHandler*>& IOHandler::GetIOHandlers()
{
	return IOHandler::IOHandlers;
}

IOHandler::IOHandler(std::string aName): Name(aName),
	pLoggers(new asiopal::LogFanoutHandler()),
	LOG_LEVEL(openpal::logflags::WARN),
	enabled(false),
	InitState(InitState_t::ENABLED),
	EnableDelayms(0)
{
	IOHandlers[Name]=this;
}

void IOHandler::Subscribe(IOHandler* pIOHandler, std::string aName)
{
	this->Subscribers[aName] = pIOHandler;
}

void IOHandler::AddLogSubscriber(openpal::ILogHandler* logger)
{
	pLoggers->Subscribe(*logger);
}
void IOHandler::SetLogLevel(openpal::LogFilters LOG_LEVEL)
{
	this->LOG_LEVEL = LOG_LEVEL;
}
void IOHandler::SetIOS(asio::io_service* ios_ptr)
{
	pIOS = ios_ptr;
}

bool IOHandler::InDemand()
{
	for(auto demand : connection_demands)
		if(demand.second)
			return true;
	return false;
}

bool IOHandler::MuxConnectionEvents(ConnectState state, const std::string& SenderName)
{
	if (state == ConnectState::DISCONNECTED)
	{
		connection_demands[SenderName] = false;
		return !InDemand();
	}
	else if (state == ConnectState::CONNECTED)
	{
		bool new_demand = !connection_demands[SenderName];
		connection_demands[SenderName] = true;
		return new_demand;
	}
	return true;
}
