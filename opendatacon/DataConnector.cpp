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

#include <iostream>
#include <opendnp3/LogLevels.h>
#include "DataConnector.h"
#include "IndexOffsetTransform.h"
#include "IndexMapTransform.h"
#include "ThresholdTransform.h"
#include "RandTransform.h"
#include "RateLimitTransform.h"
#include "LogicInvTransform.h"
#include <opendatacon/Platform.h>
#include <opendatacon/IOManager.h>

DataConnector::DataConnector(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
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
				if(!JConnections[n].isMember("Name") || !JConnections[n].isMember("Port1") || !JConnections[n].isMember("Port2"))
				{
					std::cout<<"Warning: invalid Connection config: need at least Name, From and To: \n'"<<JConnections[n].toStyledString()<<"\n' : ignoring"<<std::endl;
					continue;
				}
				auto ConName = JConnections[n]["Name"].asString();
				if(Connections.count(ConName))
				{
					std::cout<<"Warning: invalid Connection config: Duplicate Name: \n'"<<JConnections[n].toStyledString()<<"\n' : ignoring"<<std::endl;
					continue;
				}
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
	if(JSONRoot.isMember("Transforms"))
	{
		const Json::Value Transforms = JSONRoot["Transforms"];

		for(Json::ArrayIndex n = 0; n < Transforms.size(); ++n)
		{
			try
			{
				if(!Transforms[n].isMember("Type") || !Transforms[n].isMember("Sender"))
				{
					std::cout<<"Warning: invalid Transform config: need at least Type and Sender: \n'"<<Transforms[n].toStyledString()<<"\n' : ignoring"<<std::endl;
					continue;
				}

				auto normal_delete = [] (Transform* pTx){delete pTx;};

				if(Transforms[n]["Type"].asString() == "IndexOffset")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new IndexOffsetTransform(Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "IndexMap")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new IndexMapTransform(Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Threshold")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new ThresholdTransform(Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "Rand")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new RandTransform(Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "RateLimit")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new RateLimitTransform(Transforms[n]["Parameters"]), normal_delete));
					continue;
				}
				if(Transforms[n]["Type"].asString() == "LogicInv")
				{
					ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new LogicInvTransform    (Transforms[n]["Parameters"]), normal_delete));
					continue;
				}

				//Looks for a specific library (for libs that implement more than one class)
				std::string libname;
				if(Transforms[n].isMember("Library"))
				{
					libname = GetLibFileName(Transforms[n]["Library"].asString());
				}
				//Otherwise use the naming convention lib<Type>Port.so to find the default lib that implements a type of port
				else
				{
					libname = GetLibFileName(Transforms[n]["Type"].asString());
				}

				//try to load the lib
				auto* txlib = LoadModule(libname.c_str());

				if(txlib == nullptr)
				{
					std::cout << "Warning: failed to load library '"<<libname<<"' skipping transform..."<<std::endl;
					continue;
				}

				//Our API says the library should export a creation function: Transform* new_<Type>Transform(Params)
				//it should return a pointer to a heap allocated instance of a descendant of Transform
				std::string new_funcname = "new_"+Transforms[n]["Type"].asString()+"Transform";
				auto new_tx_func = (Transform*(*)(const Json::Value&))LoadSymbol(txlib, new_funcname.c_str());
				std::string delete_funcname = "delete_"+Transforms[n]["Type"].asString()+"Transform";
				auto delete_tx_func = (void (*)(Transform*))LoadSymbol(txlib, delete_funcname.c_str());

				if(new_tx_func == nullptr)
					std::cout << "Warning: failed to load symbol '"<<new_funcname<< "' from library '" << libname << "' - " << LastSystemError() << std::endl;
				if(delete_tx_func == nullptr)
					std::cout << "Warning: failed to load symbol '"<<delete_funcname<< "' from library '" << libname << "' - " << LastSystemError() << std::endl;
				if(new_tx_func == nullptr || delete_tx_func == nullptr)
				{
					std::cout<<"'Skipping transform '"<<Transforms[n]["Type"].asString()<<"'..."<<std::endl;
					continue;
				}

				//call the creation function and wrap the returned pointer
				ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(std::unique_ptr<Transform, void (*)(Transform*)>(new_tx_func(Transforms[n]["Params"].asString()),delete_tx_func));
			}
			catch (std::exception& e)
			{
				std::cout<<"Warning: Exception raised when creating Transform from config: \n'"<<Transforms[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}
		}
	}
}

void DataConnector::Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(meas, index, SenderName, pStatusCallback); }

void DataConnector::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(qual, index, SenderName, pStatusCallback); }

void DataConnector::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(arCommand, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(arCommand, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(arCommand, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(arCommand, index, SenderName, pStatusCallback); }
void DataConnector::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback){EventT(arCommand, index, SenderName, pStatusCallback); }

void DataConnector::Event(ConnectState state, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback)
{
	if(MuxConnectionEvents(state, SenderName))
	{
		EventT(state, index, SenderName, pStatusCallback);
		return;
	}
	(*pStatusCallback)(CommandStatus::UNDEFINED);
}

template<typename T>
inline void DataConnector::EventT(const T& event_obj, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	auto connection_count = SenderConnectionsLookup.count(SenderName);
	//Do we have a connection for this sender?
	if(connection_count > 0)
	{
		auto new_event_obj(event_obj);
		if(ConnectionTransforms.count(SenderName))
		{
			for(auto& Transform : ConnectionTransforms[SenderName])
			{
				if(!Transform->Event(new_event_obj, index))
				{
					(*pStatusCallback)(CommandStatus::UNDEFINED);
					return;
				}
			}
		}

		auto multi_callback = SyncMultiCallback(connection_count,pStatusCallback);
		auto bounds = SenderConnectionsLookup.equal_range(SenderName);
		for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; aMatch_it++)
		{
			//guess which one is the sendee
			IOHandler* pSendee = Connections[aMatch_it->second].second;

			//check if we were right and correct if need be
			if(pSendee->Name == SenderName)
				pSendee = Connections[aMatch_it->second].first;

			pSendee->Event(new_event_obj, index, this->Name, multi_callback);
		}
	}
	//no connection for sender if we get here
	std::string msg = "Connector '"+this->Name+"' discarding event from '"+SenderName+"' (No connection defined)";
	auto log_entry = openpal::LogEntry("DataConnector", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	(*pStatusCallback)(CommandStatus::UNDEFINED);
}

void DataConnector::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{}
void DataConnector::Enable()
{
	enabled = true;
}
void DataConnector::Disable()
{
	enabled = false;
}

