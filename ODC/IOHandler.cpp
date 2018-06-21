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

namespace odc
{

std::unordered_map<std::string,IOHandler*> IOHandler::IOHandlers;

std::unordered_map<std::string, IOHandler*>& IOHandler::GetIOHandlers()
{
	return IOHandler::IOHandlers;
}

IOHandler::IOHandler(const std::string& aName): Name(aName),
	pIOS(nullptr),
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

SharedStatusCallback_t IOHandler::SyncMultiCallback (const size_t cb_number, SharedStatusCallback_t pStatusCallback)
{
	if(pIOS == nullptr)
	{
		throw std::runtime_error("Uninitialised io_service on enabled IOHandler");
	}
	if(cb_number == 1)
		return pStatusCallback;

	auto pCombinedStatus = std::make_shared<CommandStatus>(CommandStatus::SUCCESS);
	auto pExecCount = std::make_shared<size_t>(0);
	auto pCB_sync = std::make_shared<asio::strand>(*pIOS);
	return std::make_shared<std::function<void (CommandStatus status)>>
		       (pCB_sync->wrap(
				 [pCB_sync,
				  pCombinedStatus,
				  pExecCount,
				  cb_number,
				  pStatusCallback](CommandStatus status)
				 {
					 if(*pCombinedStatus == CommandStatus::UNDEFINED)
						 return;

					 if(++(*pExecCount) == 1)
					 {
					       *pCombinedStatus = status;
					       if(*pCombinedStatus == CommandStatus::UNDEFINED)
					       {
					             (*pStatusCallback)(*pCombinedStatus);
					             return;
						 }
					 }
					 else if(status != *pCombinedStatus)
					 {
					       *pCombinedStatus = CommandStatus::UNDEFINED;
					       (*pStatusCallback)(*pCombinedStatus);
					       return;
					 }

					 if(*pExecCount >= cb_number)
					 {
					       (*pStatusCallback)(*pCombinedStatus);
					 }
				 }));
}

}
