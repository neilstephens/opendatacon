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
 * DataConnector.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "DataConnector.h"
#include "IndexMapTransform.h"
#include "IndexOffsetTransform.h"
#include "LogicInvTransform.h"
#include "RandTransform.h"
#include "RateLimitTransform.h"
#include "ThresholdTransform.h"
#include "BlackHoleTransform.h"
#include "AnalogScalingTransform.h"
#include <iostream>
#include <opendatacon/Platform.h>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>

DataConnector::DataConnector(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	IOHandler(aName),
	ConfigParser(aConfFilename, aConfOverrides)
{
	ProcessFile();
}

void DataConnector::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;
	if(JSONRoot.isMember("Connections"))
	{
		const Json::Value JConnections = JSONRoot["Connections"];

		for(Json::ArrayIndex n = 0; n < JConnections.size(); ++n)
		{
			try
			{
				if(!JConnections[n].isMember("Port1") || !JConnections[n].isMember("Port2"))
				{
					if(auto log = odc::spdlog_get("Connectors"))
						log->error("Invalid Connection config: need at least Port1 and Port2: \n'{}\n' : ignoring", JConnections[n].toStyledString());
					continue;
				}
				auto ConName = JConnections[n]["Name"].asString();
				auto ConPort1 = JConnections[n]["Port1"].asString();
				auto ConPort2 = JConnections[n]["Port2"].asString();
				if ((GetIOHandlers().count(ConPort1) == 0) || (GetIOHandlers().count(ConPort2) == 0))
				{
					if(auto log = odc::spdlog_get("Connectors"))
						log->error("Invalid port on connection '{}' skipping...", ConName);
					continue;
				}
				//insert the connections in both directions
				SenderConnections.insert({ConPort1, std::make_pair(ConPort2, GetIOHandlers()[ConPort2])});
				SenderConnections.insert({ConPort2, std::make_pair(ConPort1, GetIOHandlers()[ConPort1])});
				//Subscribe to recieve events for the connection
				GetIOHandlers()[ConPort1]->Subscribe(this, this->Name);
				GetIOHandlers()[ConPort2]->Subscribe(this, this->Name);
			}
			catch (std::exception& e)
			{
				if(auto log = odc::spdlog_get("Connectors"))
					log->error("Exception raised when creating Connection from config: \n'{}\n' : ignoring", JConnections[n].toStyledString());
			}
		}
	}
	if(JSONRoot.isMember("Transforms"))
	{
		const Json::Value Transforms = JSONRoot["Transforms"];

		for(Json::ArrayIndex n = 0; n < Transforms.size(); ++n)
		{
			try
			{
				if(!Transforms[n].isMember("Type") || !Transforms[n].isMember("Sender"))
				{
					if(auto log = odc::spdlog_get("Connectors"))
						log->error("Invalid Transform config: need at least Type and Sender: \n'{}\n' : ignoring", Transforms[n].toStyledString());
					continue;
				}
				std::string txname = Transforms[n].isMember("Name") ? Transforms[n]["Name"].asString() : Name+" Transform "+std::to_string(n);

				auto normal_delete = [] (Transform* pTx){delete pTx;};

				if(Transforms[n]["Type"].asString() == "IndexOffset")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new IndexOffsetTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "IndexMap")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new IndexMapTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Threshold")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new ThresholdTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Rand")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new RandTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "RateLimit")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new RateLimitTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "LogicInv")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new LogicInvTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if (Transforms[n]["Type"].asString() == "BlackHole")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new BlackHoleTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if (Transforms[n]["Type"].asString() == "AnalogScaling")
				{
					SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new AnalogScalingTransform(txname,Transforms[n]["Parameters"]), normal_delete));
					continue;
				}

				//Looks for a specific library (for libs that implement more than one class)
				std::string libname;
				if(Transforms[n].isMember("Library"))
				{
					libname = Transforms[n]["Library"].asString();
				}
				//Otherwise use the naming convention lib<Type>Transform.so to find the default lib that implements a type of transform
				else
				{
					libname = Transforms[n]["Type"].asString()+"Transform";
				}
				auto libfilename = GetLibFileName(libname);

				if (auto log = odc::spdlog_get("Connectors"))
					log->debug("Attempting to load library: {}, {}", libname, libfilename);

				//try to load the lib
				auto txlib = LoadModule(libfilename);

				if(txlib == nullptr)
				{
					if(auto log = odc::spdlog_get("Connectors"))
					{
						log->error("{}",LastSystemError());
						log->error("Failed to load library '{}' skipping transform...", libfilename);
					}
					continue;
				}

				//Our API says the library should export a creation function: Transform* new_<Type>Transform(Params)
				//it should return a pointer to a heap allocated instance of a descendant of Transform
				std::string new_funcname = "new_"+Transforms[n]["Type"].asString()+"Transform";
				auto new_tx_func = reinterpret_cast<Transform*(*)(const std::string&,const Json::Value&)>(LoadSymbol(txlib, new_funcname));
				std::string delete_funcname = "delete_"+Transforms[n]["Type"].asString()+"Transform";
				auto delete_tx_func = reinterpret_cast<void (*)(Transform*)>(LoadSymbol(txlib, delete_funcname));

				if(new_tx_func == nullptr)
					if(auto log = odc::spdlog_get("Connectors"))
						log->info("Failed to load symbol '{}' from library '{}' - {}" , new_funcname, libfilename, LastSystemError());
				if(delete_tx_func == nullptr)
					if(auto log = odc::spdlog_get("Connectors"))
						log->info("Failed to load symbol '{}' from library '{}' - {}" , delete_funcname, libfilename, LastSystemError());
				if(new_tx_func == nullptr || delete_tx_func == nullptr)
				{
					if(auto log = odc::spdlog_get("Connectors"))
						log->error("Failed to load transform '{}' : ignoring", Transforms[n]["Type"].asString());
					continue;
				}

				//Create a logger if we haven't already
				if(!odc::spdlog_get(libname))
				{
					if(auto log = odc::spdlog_get("opendatacon"))
					{
						auto pLogger = std::make_shared<spdlog::async_logger>(libname, log->sinks().begin(), log->sinks().end(),
							odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
						pLogger->set_level(spdlog::level::trace);
						odc::spdlog_register_logger(pLogger);
					}
				}

				auto tx_cleanup = [=](Transform* tx)
							{
								delete_tx_func(tx);
								UnLoadModule(txlib);
							};

				//call the creation function and wrap the returned pointer
				SenderTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new_tx_func(txname,Transforms[n]["Parameters"]),tx_cleanup));
			}
			catch (std::exception& e)
			{
				if(auto log = odc::spdlog_get("Connectors"))
					log->error("Exception raised when creating Transform from config: \n'{}\n' Error: '{}' - skipping transform", Transforms[n].toStyledString(), e.what());
			}
		}
	}
}

//If the datacon re-creates a port with a new address,
//	it calls this so we can update our records
void DataConnector::ReplaceAddress(std::string PortName, IOHandler* Addr)
{
	for(auto& connection : SenderConnections)
	{
		if(connection.second.first == PortName)
			connection.second.second = Addr;
	}
}

void DataConnector::Event(ConnectState state, const std::string& SenderName)
{
	//handled in the main Event()
}

void DataConnector::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	auto connection_count = SenderConnections.count(SenderName);
	//Do we have a connection for this sender?
	if(connection_count > 0)
	{
		EvtHandler_ptr ToDestination = std::make_shared<EvtHandler_ptr::element_type>([=](std::shared_ptr<EventInfo> evt)
			{
				if(!evt)
				{
					if(auto log = odc::spdlog_get("opendatacon"))
						log->trace("{} {} {} Payload {} Event {} => DROPPED by transform", event->GetSourcePort(), ToString(event->GetEventType()),event->GetIndex(), event->GetPayloadString(), Name);
					(*pStatusCallback)(CommandStatus::UNDEFINED);
					return;
				}

				auto multi_callback = SyncMultiCallback(connection_count,pStatusCallback);
				auto bounds = SenderConnections.equal_range(SenderName);
				for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; aMatch_it++)
				{
					if(auto log = odc::spdlog_get("opendatacon"))
						log->trace("{} {} {} Payload {} Event {} => {}", evt->GetSourcePort(), ToString(evt->GetEventType()),evt->GetIndex(), evt->GetPayloadString(), Name, aMatch_it->second.second->GetName());

					if(evt->GetEventType() == EventType::ConnectState)
					{
						auto state = evt->GetPayload<EventType::ConnectState>();
						auto& ReceiverName = aMatch_it->second.second->GetName();
						if(MuxConnectionEvents(state, SenderName,ReceiverName))
							aMatch_it->second.second->Event(state, Name);
					}

					aMatch_it->second.second->Event(evt, this->Name, OneShotWrap(multi_callback));
				}
			});
		ToDestination = OneShotWrap(ToDestination);
		if(SenderTransforms.find(SenderName) != SenderTransforms.end())
		{
			//create a chain of callbacks to the destination
			auto Tx_it = SenderTransforms.at(SenderName).rbegin();
			const auto rend = SenderTransforms.at(SenderName).rend();
			while(Tx_it != rend)
			{
				ToDestination = std::make_shared<EvtHandler_ptr::element_type>([=](std::shared_ptr<EventInfo> evt)
					{
						auto src = (Tx_it+1 == rend) ? Name : (*(Tx_it+1))->Name;
						if(evt)
						{
							if(auto log = odc::spdlog_get("opendatacon"))
								log->trace("{} {} {} Payload {} Event {} => {}", evt->GetSourcePort(), ToString(evt->GetEventType()),evt->GetIndex(), evt->GetPayloadString(), src, (*Tx_it)->Name);
						}
						(*Tx_it)->Event(evt,ToDestination);
					});
				ToDestination = OneShotWrap(ToDestination);
				Tx_it++;
			}
		}
		auto new_event_obj = std::make_shared<EventInfo>(*event);
		(*ToDestination)(new_event_obj);
		return;
	}
	//no connection for sender if we get here
	if(auto log = odc::spdlog_get("Connectors"))
		log->warn("{}: discarding event from '{}' (No connection defined)", Name, SenderName);

	(*pStatusCallback)(CommandStatus::UNDEFINED);
}

void DataConnector::Build()
{}
void DataConnector::Enable()
{
	enabled = true;
}
void DataConnector::Disable()
{
	enabled = false;
}

