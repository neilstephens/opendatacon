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

DataConnector::DataConnector(std::string aName, std::string aConfFilename, std::string aConfOverrides):IOHandler(aName)
{
	ConfFilename = aConfFilename;
	ConfOverrides = aConfOverrides;

	ProcessFile(ConfFilename, ConfOverrides);
}

void DataConnector::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot["Connections"].isNull())
	{
		const Json::Value JConnections = JSONRoot["Connections"];

		for(Json::ArrayIndex n = 0; n < JConnections.size(); ++n)
		{
			if(JConnections[n]["Name"].isNull() || JConnections[n]["Port1"].isNull() || JConnections[n]["Port2"].isNull())
			{
				std::cout<<"Warning: invalid Connection config: need at least Name, From and To: \n'"<<JConnections[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}

			Connections[JConnections[n]["Name"].asString()] = std::make_pair(IOHandler::IOHandlers[JConnections[n]["Port1"].asString()],IOHandler::IOHandlers[JConnections[n]["Port2"].asString()]);
			//Subscribe to recieve events for the connection
			IOHandlers[JConnections[n]["Port1"].asString()]->Subscribe(this,this->Name);
			IOHandlers[JConnections[n]["Port2"].asString()]->Subscribe(this,this->Name);
			//Add to the lookup table
			SenderConnectionsLookup.insert(std::make_pair(JConnections[n]["Port1"].asString(), JConnections[n]["Name"].asString()));
			SenderConnectionsLookup.insert(std::make_pair(JConnections[n]["Port2"].asString(), JConnections[n]["Name"].asString()));
		}
	}
	if(!JSONRoot["Transforms"].isNull())
	{
		const Json::Value Transforms = JSONRoot["Transforms"];

		for(Json::ArrayIndex n = 0; n < Transforms.size(); ++n)
		{
			if(Transforms[n]["Type"].isNull() || Transforms[n]["Sender"].isNull())
			{
				std::cout<<"Warning: invalid Transform config: need at least Type and Sender: \n'"<<Transforms[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}

			if(Transforms[n]["Type"].asString() == "IndexOffset")
				ConnectionTransforms[Transforms[n]["Sender"].asString()].push_back(new IndexOffsetTransform(Transforms[n]["Parameters"].asString()));
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

std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<opendnp3::CommandStatus> DataConnector::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }


template<typename T>
inline std::future<opendnp3::CommandStatus> DataConnector::EventT(const T& event_obj, uint16_t index, const std::string& SenderName)
{
	if(!enabled)
	{
		auto cmd_promise = std::promise<opendnp3::CommandStatus>();
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	auto bounds = SenderConnectionsLookup.equal_range(SenderName);
	//Do we have a connection for this sender?
	if(bounds.first != bounds.second)//yes
	{
		for(auto aMatch_it = bounds.first;;)
		{
			IOHandler* pSendee = Connections[aMatch_it->second].second;
			if(pSendee->Name == SenderName)
				pSendee = Connections[aMatch_it->second].first;

			std::shared_ptr<T> new_event_obj(new T(event_obj));
			if(ConnectionTransforms.count(SenderName))
			{
				bool pass_on = true;
				for(Transform* Transform : ConnectionTransforms[SenderName])
				{
					if(!Transform->Event(*(new_event_obj.get()), index))
					{
						pass_on = false;
						break;
					}
				}
				if(!pass_on)
				continue;
			}

			//return on the last connection
			if(++aMatch_it != bounds.second)
				pSendee->Event(*new_event_obj.get(), index, this->Name);
			else
				return pSendee->Event(*new_event_obj.get(), index, this->Name);
		}
	}
	//no connection for sender if we get here
	std::string msg = "Connector '"+this->Name+"' discarding event from '"+SenderName+"' (No connection defined)";
	auto log_entry = openpal::LogEntry("DataConnector", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

void DataConnector::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{

}
void DataConnector::Enable()
{
	enabled = true;
}
void DataConnector::Disable()
{
	enabled = false;
}

