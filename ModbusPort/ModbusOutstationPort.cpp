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
 * ModbusOutstationPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/app/DynamicPointIndexes.h>
#include <opendnp3/outstation/Database.h>
#include <opendnp3/outstation/TimeTransaction.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "ModbusOutstationPort.h"

#include <opendnp3/LogLevels.h>

ModbusOutstationPort::ModbusOutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	ModbusPort(aName, aConfFilename, aConfOverrides)
{};


void ModbusOutstationPort::Enable()
{
    if(enabled) return;
    enabled = true;
    
    ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());
    
    // Only change stack state if it is a persistent server
    if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
    {
        this->Connect();
    }
    
}

void ModbusOutstationPort::Connect()
{
    if(!enabled) return;
    if (stack_enabled) return;
    
    stack_enabled = true;
    
    if (mb == NULL)
    {
        std::string msg = Name+": Connect error: 'Modbus stack failed'";
        auto log_entry = openpal::LogEntry("ModbusOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }
    
    int s = modbus_tcp_pi_listen(mb, 1);
    if (s == -1)
    {
        std::string msg = Name+": Connect error: '" + modbus_strerror(errno) + "'";
        auto log_entry = openpal::LogEntry("ModbusOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }
    
    int r = modbus_tcp_pi_accept(mb, &s);
    if (r == -1)
    {
        std::string msg = Name+": Connect error: '" + modbus_strerror(errno) + "'";
        auto log_entry = openpal::LogEntry("ModbusOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }
}

void ModbusOutstationPort::Disable()
{
    //cancel the retry timers (otherwise it would tie up the io_service on shutdown)
    Disconnect();
    enabled = false;
}

void ModbusOutstationPort::Disconnect()
{
    if (!stack_enabled) return;
    stack_enabled = false;
    
    if(mb != nullptr) modbus_close(mb);
}

void ModbusOutstationPort::StateListener(opendnp3::ChannelState state)
{
	if(!enabled)
		return;

	for(auto IOHandler_pair : Subscribers)
	{
		IOHandler_pair.second->Event((state == opendnp3::ChannelState::OPEN), 0, this->Name);
	}
}
void ModbusOutstationPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	auto IPPort = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	auto log_id = "outst_"+IPPort;

    //TODO: collect these on a collection of modbus tcp connections
    char service[6];
    sprintf(service, "%i", pConf->mAddrConf.Port);
    mb = modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), service);
    if (mb == NULL) {
        std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
        auto log_entry = openpal::LogEntry("ModbusOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }

    /* The fist value of each array is accessible from the 0 address. */
    /*
    mb_mapping = modbus_mapping_new(BITS_ADDRESS + BITS_NB,
                                    INPUT_BITS_ADDRESS + INPUT_BITS_NB,
                                    REGISTERS_ADDRESS + REGISTERS_NB,
                                    INPUT_REGISTERS_ADDRESS + INPUT_REGISTERS_NB);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    */
    //TODO: modbus_mapping_free
}

template<typename T>
inline opendnp3::CommandStatus ModbusOutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//FIXME: this is meant to return if we support the type of command
	//at the moment we just return success if it's configured as a control
	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	if(std::is_same<T,opendnp3::ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	{
        /*
		for(auto index : pConf->pPointConf->ControlIndicies)
			if(index == aIndex)
				return opendnp3::CommandStatus::SUCCESS;
         */
	}
	return opendnp3::CommandStatus::NOT_SUPPORTED;
}
template<typename T>
inline opendnp3::CommandStatus ModbusOutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//container to store our async futures
	std::vector<std::future<opendnp3::CommandStatus>> future_results;

	for(auto IOHandler_pair : Subscribers)
	{
		future_results.push_back((IOHandler_pair.second->Event(arCommand, aIndex, this->Name)));
	}

	for(auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while(future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if(future_result.get() != opendnp3::CommandStatus::SUCCESS)
			return opendnp3::CommandStatus::UNDEFINED;
	}

	return opendnp3::CommandStatus::SUCCESS;
}

std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusOutstationPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); };

template<typename T>
inline std::future<opendnp3::CommandStatus> ModbusOutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	{//transaction scope

    }
	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

