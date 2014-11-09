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
 * ModbusClientPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <opendnp3/LogLevels.h>
#include <thread>
#include <chrono>

#include <opendnp3/app/MeasurementTypes.h>
#include "ModbusMasterPort.h"
#include <array>

void ModbusMasterPort::Enable()
{
	if(enabled) return;
    enabled = true;
    
    pTCPRetryTimer.reset(new Timer_t(*pIOS));
    PollScheduler.reset(new ASIOScheduler(*pIOS));

    ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

    // Only change stack state if it is a persistent server
    if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
    {
        this->Connect();
    }
}

void ModbusMasterPort::Connect()
{
    if(!enabled) return;
    if (stack_enabled) return;

    ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

    if (mb == NULL)
    {
        std::string msg = Name+": Connect error: 'Modbus stack failed'";
        auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }
    
    if (modbus_connect(mb) == -1)
    {
        std::string msg = Name+": Connect error: '" + modbus_strerror(errno) + "'";
        auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        
        //try again later
        pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
        pTCPRetryTimer->async_wait(
                                   [this](asio::error_code err_code)
                                   {
                                       if(err_code != asio::error::operation_aborted)
                                           this->Connect();
                                   });
        return;
    };

    stack_enabled = true;

    {
        std::string msg = Name + ": Connect success!";
        auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::INFO,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
    }
    
    modbus_set_slave(mb, pConf->mAddrConf.OutstationAddr);
    
    uint8_t tab_bytes[64];
    int rc = modbus_report_slave_id(mb, 64, tab_bytes);
    if (rc > 1)
    {
        std::string msg = Name + "Run Status Indicator: %s" + (tab_bytes[1] ? "ON" : "OFF");
        auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::INFO,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
    }
    
    PollScheduler->Clear();
    for(auto pg : pConf->pPointConf->PollGroups)
    {
        auto id = pg.second.ID;
        auto action = [=](){
            this->DoPoll(id);
        };
        PollScheduler->Add(pg.second.pollrate, action);
    }
    
    PollScheduler->Start();
}

void ModbusMasterPort::Disable()
{
    Disconnect();
	enabled = false;
}

void ModbusMasterPort::Disconnect()
{
    if (!stack_enabled) return;
    stack_enabled = false;
    
    //cancel the timers (otherwise it would tie up the io_service on shutdown)
    pTCPRetryTimer->cancel();
    PollScheduler->Stop();
    
    if(mb != nullptr) modbus_close(mb);
}

void ModbusMasterPort::HandleError(int errnum, const std::string& source)
{
    std::string msg = Name + ": " + source + " error: '" + modbus_strerror(errno) + "'";
    auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
    pLoggers->Log(log_entry);

    // If not a modbus error, tear down the connection
    if (errnum < MODBUS_ENOBASE)
    {
        this->Disconnect();
        
        ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

        // Try and re-connect if a persistent connection
        if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
        {
            //try again later
            pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
            pTCPRetryTimer->async_wait(
                                       [this](asio::error_code err_code)
                                       {
                                           if(err_code != asio::error::operation_aborted)
                                               this->Connect();
                                       });
        }
    }
}

void ModbusMasterPort::StateListener(opendnp3::ChannelState state)
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	for(auto IOHandler_pair : Subscribers)
	{
		bool failed;
		if(state == opendnp3::ChannelState::CLOSED || state == opendnp3::ChannelState::SHUTDOWN || state == opendnp3::ChannelState::WAITING)
		{
            /*
			for(auto index : pConf->pPointConf->AnalogIndicies)
				IOHandler_pair.second->Event(opendnp3::Analog(0.0,static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST)),index,this->Name);
			for(auto index : pConf->pPointConf->BinaryIndicies)
				IOHandler_pair.second->Event(opendnp3::Binary(false,static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST)),index,this->Name);
             */
			failed = pConf->pPointConf->mCommsPoint.first.value;
		}
		else
			failed = !pConf->pPointConf->mCommsPoint.first.value;

		if(pConf->pPointConf->mCommsPoint.first.quality == static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
			IOHandler_pair.second->Event(opendnp3::Binary(failed),pConf->pPointConf->mCommsPoint.second,this->Name);
	}
	if(state == opendnp3::ChannelState::OPEN)
	{

        
	}
}

void ModbusMasterPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	auto IPPort = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	auto log_id = "mast_"+IPPort;

    //TODO: collect these on a collection of modbus tcp connections
    char service[6];
    sprintf(service, "%i", pConf->mAddrConf.Port);
    mb = modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), service);
    if (mb == NULL) {
        std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
        auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
        pLoggers->Log(log_entry);
        return;
    }
}

void ModbusMasterPort::DoPoll(uint32_t pollgroup)
{
    uint8_t tab_bits[64];
    uint16_t tab_reg[64];
    if(!enabled) return;
    
    auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
    int rc;

    // Modbus function code 0x01 (read coil status)
    for(auto range : pConf->pPointConf->BitIndicies)
    {
        if (pollgroup && (range.pollgroup != pollgroup)) continue;
        rc = modbus_read_bits(mb, range.start, range.count, tab_bits);
        if (rc == -1)
        {
            HandleError(errno, "read bits poll");
            if(!enabled) return;
        }
        else
        {
            uint16_t index = range.start;
            for(uint16_t i = 0; i < rc; i++ )
            {
                for(auto IOHandler_pair : Subscribers)
                {
                    IOHandler_pair.second->Event(opendnp3::BinaryOutputStatus(tab_bits[i] != false),index,this->Name);
                }
                ++index;
            }
        }
    }
    
    // Modbus function code 0x02 (read input status)
    for(auto range : pConf->pPointConf->BitIndicies)
    {
        if (pollgroup && (range.pollgroup != pollgroup)) continue;
        rc = modbus_read_input_bits(mb, range.start, range.count, tab_bits);
        if (rc == -1)
        {
            HandleError(errno, "read input bits poll");
            if(!enabled) return;
        }
        else
        {
            uint16_t index = range.start;
            for(uint16_t i = 0; i < rc; i++ )
            {
                for(auto IOHandler_pair : Subscribers)
                {
                    IOHandler_pair.second->Event(opendnp3::Binary(tab_bits[i] != false),index,this->Name);
                }
                ++index;
            }
        }
    }
    
    // Modbus function code 0x03 (read holding registers)
    for(auto range : pConf->pPointConf->RegIndicies)
    {
        if (pollgroup && (range.pollgroup != pollgroup)) continue;
        rc = modbus_read_registers(mb, range.start, range.count, tab_reg);
        if (rc == -1)
        {
            HandleError(errno, "read registers poll");
            if(!enabled) return;
        }
        else
        {
            uint16_t index = range.start;
            for(uint16_t i = 0; i < rc; i++ )
            {
                for(auto IOHandler_pair : Subscribers)
                {
                    IOHandler_pair.second->Event(opendnp3::AnalogOutputInt16(tab_reg[i]),index,this->Name);
                }
                ++index;
            }
        }
    }
    
    // Modbus function code 0x04 (read input registers)
    for(auto range : pConf->pPointConf->InputRegIndicies)
    {
        if (pollgroup && (range.pollgroup != pollgroup)) continue;
        rc = modbus_read_input_registers(mb, range.start, range.count, tab_reg);
        if (rc == -1)
        {
            HandleError(errno, "read input registers poll");
            if(!enabled) return;
        }
        else
        {
            uint16_t index = range.start;
            for(uint16_t i = 0; i < rc; i++ )
            {
                for(auto IOHandler_pair : Subscribers)
                {
                    IOHandler_pair.second->Event(opendnp3::Analog(tab_reg[i]),index,this->Name);
                }
                ++index;
            }
        }
    }
}

//Implement some IOHandler - parent ModbusPort implements the rest to return NOT_SUPPORTED
std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); };
std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); };

std::future<opendnp3::CommandStatus> ModbusMasterPort::Event(bool connected, uint16_t index, const std::string& SenderName)
{
    ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	auto cmd_future = cmd_promise.get_future();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_future;
	}

	//connected == true means something upstream has connected
	if(connected)
	{
        // Only change stack state if it is an on demand server
        if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
        {
            this->Connect();
        }
    }

	cmd_promise.set_value(opendnp3::CommandStatus::SUCCESS);
	return cmd_future;
}

template<typename T>
inline std::future<opendnp3::CommandStatus> ModbusMasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	auto cmd_future = cmd_promise.get_future();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_future;
	}

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
    
	//for(auto i : pConf->pPointConf->ControlIndicies)
	{
		//if(i == index)
		{
            /*
             // Modbus function code 0x0F (force multiple coils)
             modbus_write_bits
             // Modbus function code 0x10 (preset multiple registers)
             modbus_write_registers
             */
			//cmd_proc->DirectOperate(lCommand,index, *CommandCorrespondant::GetCallback(std::move(cmd_promise)));
			//return cmd_future;
		}
	}
	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_future;
}



