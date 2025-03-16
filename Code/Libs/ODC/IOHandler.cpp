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

#include <opendatacon/IOHandler.h>
 #include <utility>
namespace odc
{

std::unordered_map<std::string,IOHandler*> IOHandler::IOHandlers;

std::unordered_map<std::string, IOHandler*>& IOHandler::GetIOHandlers()
{
	return IOHandler::IOHandlers;
}

IOHandler::IOHandler(const std::string& aName):
	InitState(InitState_t::ENABLED),
	EnableDelayms(0),
	Name(aName),
	pIOS(asio_service::Get()),
	enabled(false)
{
	IOHandlers[Name]=this;
}

void IOHandler::Subscribe(IOHandler* pIOHandler, const std::string& aName)
{
	this->Subscribers[aName] = pIOHandler;
}
void IOHandler::UnSubscribe(const std::string& aName)
{
	this->Subscribers.erase(aName);
}

bool DemandMap::InDemand(const std::string& ReceiverName) const
{
	std::lock_guard<std::mutex> lck (mtx);
	if(connection_demands.find(ReceiverName) == connection_demands.end())
		return false;
	for(const auto& demand : connection_demands.at(ReceiverName))
		if(demand.second)
			return true;
	return false;
}

bool DemandMap::MuxConnectionEvents(ConnectState state, const std::string& SenderName, const std::string& ReceiverName)
{
	if (state == ConnectState::DISCONNECTED)
	{
		{
			std::lock_guard<std::mutex> lck (mtx);
			connection_demands[ReceiverName][SenderName] = false;
		}
		return !InDemand(ReceiverName);
	}
	else if (state == ConnectState::CONNECTED)
	{
		std::lock_guard<std::mutex> lck (mtx);
		bool new_demand = !connection_demands[ReceiverName][SenderName];
		connection_demands[ReceiverName][SenderName] = true;
		return new_demand;
	}
	return true;
}

std::map<std::string, Demands_t> DemandMap::GetDemands()
{
	std::lock_guard<std::mutex> lck (mtx);
	return connection_demands;
}

SharedStatusCallback_t IOHandler::SyncMultiCallback (const size_t cb_number, SharedStatusCallback_t pStatusCallback) const
{
	if(pIOS == nullptr)
	{
		throw std::runtime_error("Uninitialised io_service on enabled IOHandler");
	}

	if(cb_number < 2)
		return pStatusCallback;

	//We must keep the io_service active for the life of the strand/handler we're about to create
	std::shared_ptr<asio::io_service::work> work = pIOS->make_work();
	auto pCombinedStatus = std::make_shared<CommandStatus>(CommandStatus::SUCCESS);
	auto pExecCount = std::make_shared<size_t>(0);
	std::shared_ptr<asio::io_service::strand> pCB_sync = pIOS->make_strand();
	auto pOneShotCb = OneShotFunc<void(CommandStatus)>::Wrap(pStatusCallback);
	auto multi_cb = [work, pCB_sync, pCombinedStatus, pExecCount, cb_number, pOneShotCb](CommandStatus status)
			    {
				    if(*pCombinedStatus == CommandStatus::UNDEFINED)
					    return;

				    if(++(*pExecCount) == 1)
				    {
					    *pCombinedStatus = status;
					    if(*pCombinedStatus == CommandStatus::UNDEFINED)
					    {
						    (*pOneShotCb)(*pCombinedStatus);
						    return;
					    }
				    }
				    else if(status != *pCombinedStatus)
				    {
					    *pCombinedStatus = CommandStatus::UNDEFINED;
					    (*pOneShotCb)(*pCombinedStatus);
					    return;
				    }

				    if(*pExecCount >= cb_number)
				    {
					    (*pOneShotCb)(*pCombinedStatus);
				    }
			    };
	return std::make_shared<std::function<void (CommandStatus status)>>(pCB_sync->wrap(std::move(multi_cb)));
}

} // namespace odc
