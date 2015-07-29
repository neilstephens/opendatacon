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

#include <future>
#include <iostream>
#include <asiodnp3/DNP3Manager.h>
#include <opendnp3/LogLevels.h>
#include "DataConnector.h"
#include "IndexOffsetTransform.h"
#include "ThresholdTransform.h"
#include "RandTransform.h"
#include "RateLimitTransform.h"
#include <opendatacon/Platform.h>

DataConnector::DataConnector(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	IOHandler(aName),
	ConfigParser(aConfFilename, aConfOverrides)
{
	ProcessFile();
}

void DataConnector::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;
	if(!JSONRoot["Connections"].isNull())
	{
		const Json::Value JConnections = JSONRoot["Connections"];

		for(Json::ArrayIndex n = 0; n < JConnections.size(); ++n)
		{
			try
			{
				if(JConnections[n]["Name"].isNull() || JConnections[n]["Port1"].isNull() || JConnections[n]["Port2"].isNull())
				{
					std::cout<<"Warning: invalid Connection config: need at least Name, From and To: \n'"<<JConnections[n].toStyledString()<<"\n' : ignoring"<<std::endl;
					continue;
				}
				auto ConName = JConnections[n]["Name"].asString();
				auto ConPort1 = JConnections[n]["Port1"].asString();
				auto ConPort2 = JConnections[n]["Port2"].asString();
				if ((GetIOHandlers().count(ConPort1) == 0) || (GetIOHandlers().count(ConPort2) == 0))
				{
					std::cout << "Warning: invalid port on connection '" << ConName << "' skipping..." << std::endl;
					continue;
				}
				Connections[ConName] = std::make_pair(GetIOHandlers()[ConPort1], GetIOHandlers()[ConPort2]);
				//Subscribe to recieve events for the connection
				GetIOHandlers()[ConPort1]->Subscribe(this, this->Name);
				GetIOHandlers()[ConPort2]->Subscribe(this, this->Name);
				//Add to the lookup table
				SenderConnectionsLookup.insert(std::make_pair(ConPort1, ConName));
				SenderConnectionsLookup.insert(std::make_pair(ConPort2, ConName));
			}
			catch (std::exception& e)
			{
				std::cout<<"Warning: Exception raised when creating Connection from config: \n'"<<JConnections[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}
		}
	}
	if(!JSONRoot["Transforms"].isNull())
	{
		const Json::Value Transforms = JSONRoot["Transforms"];

		for(Json::ArrayIndex n = 0; n < Transforms.size(); ++n)
		{
			try
			{
				if(Transforms[n]["Type"].isNull() || Transforms[n]["Sender"].isNull())
				{
					std::cout<<"Warning: invalid Transform config: need at least Type and Sender: \n'"<<Transforms[n].toStyledString()<<"\n' : ignoring"<<std::endl;
					continue;
				}

				if(Transforms[n]["Type"].asString() == "IndexOffset")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform>(new IndexOffsetTransform(Transforms[n]["Parameters"])));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Threshold")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform>(new ThresholdTransform(Transforms[n]["Parameters"])));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Rand")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform>(new RandTransform(Transforms[n]["Parameters"])));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "RateLimit")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform>(new RateLimitTransform(Transforms[n]["Parameters"])));
					continue;
				}

				//Looks for a specific library (for libs that implement more than one class)
				std::string libname;
				if(!Transforms[n]["Library"].isNull())
				{
					libname = GetLibFileName(Transforms[n]["Library"].asString());
				}
				//Otherwise use the naming convention lib<Type>Port.so to find the default lib that implements a type of port
				else
				{
					libname = GetLibFileName(Transforms[n]["Type"].asString());
				}

				//try to load the lib
				auto* txlib = DYNLIBLOAD(libname.c_str());

				if(txlib == nullptr)
				{
					std::cout << "Warning: failed to load library '"<<libname<<"' skipping transform..."<<std::endl;
					continue;
				}

				//Our API says the library should export a creation function: Transform* new_<Type>Transform(Params)
				//it should return a pointer to a heap allocated instance of a descendant of Transform
				std::string new_funcname = "new_"+Transforms[n]["Type"].asString()+"Transform";
				auto new_tx_func = (Transform*(*)(const Json::Value))DYNLIBGETSYM(txlib, new_funcname.c_str());

				if(new_tx_func == nullptr)
				{
					std::cout << "Warning: failed to load symbol '"<<new_funcname<<"' for transform type '"<<Transforms[n]["Type"].asString()<<"' skipping transform..."<<std::endl;
					continue;
				}

				//call the creation function and wrap the returned pointer to a new port
				ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform>(new_tx_func(Transforms[n]["Params"].asString())));
			}
			catch (std::exception& e)
			{
				std::cout<<"Warning: Exception raised when creating Transform from config: \n'"<<Transforms[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}
		}
	}
}

std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }

std::future<opendnp3::CommandStatus> DataConnector::Event(const ::BinaryQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::AnalogQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::CounterQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::FrozenCounterQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const ::AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName){ return EventT(qual, index, SenderName); }

std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }

std::future<opendnp3::CommandStatus> DataConnector::Event(ConnectState state, uint16_t index, const std::string& SenderName)
{
	if(MuxConnectionEvents(state, SenderName))
		return EventT(state, index, SenderName);
	else
		return IOHandler::CommandFutureUndefined();
}

template<typename T>
inline std::future<opendnp3::CommandStatus> DataConnector::EventT(const T& event_obj, uint16_t index, const std::string& SenderName)
{
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	auto bounds = SenderConnectionsLookup.equal_range(SenderName);
	//Do we have a connection for this sender?
	if(bounds.first != bounds.second)	//yes
	{
		auto new_event_obj(event_obj);
		if(ConnectionTransforms.count(SenderName))
		{
			for(auto& Transform : ConnectionTransforms[SenderName])
			{
				if(!Transform->Event(new_event_obj, index))
					return IOHandler::CommandFutureUndefined();
			}
		}

		std::vector<std::future<opendnp3::CommandStatus> > returns;
		for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; aMatch_it++)
		{
			//guess which one is the sendee
			IOHandler* pSendee = Connections[aMatch_it->second].second;

			//check if we were right and correct if need be
			if(pSendee->Name == SenderName)
				pSendee = Connections[aMatch_it->second].first;

			returns.push_back(pSendee->Event(new_event_obj, index, this->Name));
		}
		for(auto& ret : returns)
		{
			if(ret.get() != opendnp3::CommandStatus::SUCCESS)
				return IOHandler::CommandFutureUndefined();
		}
		return std::move(returns.back());
	}
	//no connection for sender if we get here
	std::string msg = "Connector '"+this->Name+"' discarding event from '"+SenderName+"' (No connection defined)";
	auto log_entry = openpal::LogEntry("DataConnector", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	return IOHandler::CommandFutureUndefined();
}

void DataConnector::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{}
void DataConnector::Enable()
{
	enabled = true;
}
void DataConnector::Disable()
{
	enabled = false;
}

