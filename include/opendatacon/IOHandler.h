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
 * IOHandler.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef IOHANDLER_H_
#define IOHANDLER_H_

#include <functional>
#include <unordered_map>
#include <map>
#include <atomic>
#include <opendatacon/asio.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>

namespace odc
{

enum class  InitState_t { ENABLED, DISABLED, DELAYED };

typedef std::shared_ptr<std::function<void (CommandStatus status)>> SharedStatusCallback_t;
using Demands_t = std::map<std::string,bool>;

//class to synchronise access to connection demand map
class DemandMap
{
public:
	bool InDemand(const std::string& ReceiverName) const;
	bool MuxConnectionEvents(ConnectState state, const std::string& SenderName, const std::string& ReceiverName);
	std::map<std::string,Demands_t> GetDemands();
private:
	std::map<std::string,Demands_t> connection_demands;
	mutable std::mutex mtx;
	//TODO: do it using asio
	//asio::io_service::strand sync;
};

class IOHandler
{
public:
	IOHandler(const std::string& aName);
	virtual ~IOHandler(){}

	//Connection events:
	virtual void Event(ConnectState state, const std::string& SenderName) = 0;

	//Event events
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) = 0;

	virtual void Enable() = 0;
	virtual void Disable() = 0;

	void Subscribe(IOHandler* pIOHandler, const std::string& aName);
	void UnSubscribe(const std::string& aName);
	inline const std::unordered_map<std::string,IOHandler*>& GetSubscribers(){return Subscribers;}
	inline std::map<std::string,Demands_t> GetDemands(){return mDemandMap.GetDemands();}

	inline const std::string& GetName(){return Name;}
	inline const bool Enabled(){return enabled;}
	InitState_t InitState;
	uint16_t EnableDelayms;

	static std::unordered_map<std::string, IOHandler*>& GetIOHandlers();

protected:
	std::string Name;
	const std::shared_ptr<odc::asio_service> pIOS;
	std::atomic_bool enabled;

	inline bool InDemand() const { return mDemandMap.InDemand(""); }
	inline bool MuxConnectionEvents(ConnectState state, const std::string& SenderName, const std::string& ReceiverName = "")
	{ return mDemandMap.MuxConnectionEvents(state, SenderName, ReceiverName); }

	inline void PublishEvent(ConnectState state)
	{
		auto event = std::make_shared<EventInfo>(EventType::ConnectState,0,Name);
		event->SetPayload<EventType::ConnectState>(std::move(state));
		PublishEvent(event);
	}

	inline void PublishEvent(std::shared_ptr<const EventInfo> event, SharedStatusCallback_t pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([] (CommandStatus status){})) const
	{
		if(pIOS == nullptr)
			throw std::runtime_error("Uninitialised io_service on enabled IOHandler");
		auto shouldPost = !pIOS->current_thread_in_pool();
		if(!pStatusCallback)
			pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([] (CommandStatus status){});
		if(event->GetEventType() == EventType::ConnectState)
		{
			//call the special connection Event() function separately,
			//	so it can keep track of upsteam demand
			for(const auto& IOHandler_pair: Subscribers)
			{
				if(shouldPost)
					pIOS->post([=](){IOHandler_pair.second->Event(event->GetPayload<EventType::ConnectState>(), Name);});
				else
					IOHandler_pair.second->Event(event->GetPayload<EventType::ConnectState>(), Name);
			}
		}
		auto multi_callback = SyncMultiCallback(Subscribers.size(),pStatusCallback);
		for(const auto& IOHandler_pair: Subscribers)
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->trace("{} {} Payload {} Event {} => {}", ToString(event->GetEventType()),event->GetIndex(), event->GetPayloadString(), Name, IOHandler_pair.first);
			if(shouldPost)
				pIOS->post([=](){IOHandler_pair.second->Event(event, Name, multi_callback);});
			else
				IOHandler_pair.second->Event(event, Name, multi_callback);
		}
	}

	SharedStatusCallback_t SyncMultiCallback (const size_t cb_number, SharedStatusCallback_t pStatusCallback) const;

private:
	std::unordered_map<std::string,IOHandler*> Subscribers;
	DemandMap mDemandMap;

	// Important that this is private - for inter process memory management
	static std::unordered_map<std::string, IOHandler*> IOHandlers;
};

}

#endif /* IOHANDLER_H_ */
