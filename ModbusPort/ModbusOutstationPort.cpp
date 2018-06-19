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
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "ModbusOutstationPort.h"

#include <spdlog/spdlog.h>

ModbusOutstationPort::ModbusOutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	ModbusPort(aName, aConfFilename, aConfOverrides)
{}

ModbusOutstationPort::~ModbusOutstationPort()
{
	Disable();
	if (mb != nullptr)
		modbus_free(mb);
	if (mb_mapping != nullptr)
		modbus_mapping_free(mb_mapping);
}

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
		spdlog::get("ModbusPort")->error("{}: Connect error: 'Modbus stack failed'", Name);
		return;
	}

	int s = modbus_tcp_pi_listen(mb, 1);
	if (s == -1)
	{
		spdlog::get("ModbusPort")->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));
		return;
	}

	int r = modbus_tcp_pi_accept(mb, &s);
	if (r == -1)
	{
		spdlog::get("ModbusPort")->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));
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

void ModbusOutstationPort::StateListener(ChannelState state)
{
	if(!enabled)
		return;

	if(state == ChannelState::OPEN)
	{
		PublishEvent(ConnectState::CONNECTED, 0);
	}
	else
	{
		PublishEvent(ConnectState::DISCONNECTED, 0);
	}
}
void ModbusOutstationPort::BuildOrRebuild()
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	std::string log_id;

	if(pConf->mAddrConf.IP != "")
	{
		log_id = "outst_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);

		//TODO: collect these on a collection of modbus tcp connections
		mb = modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), std::to_string(pConf->mAddrConf.Port).c_str());
		if (mb == NULL)
		{
			spdlog::get("ModbusPort")->error("{}: Stack error: 'Modbus stack creation failed'", Name);
			//TODO: should this throw an exception instead of return?
			return;
		}
	}
	else if(pConf->mAddrConf.SerialDevice != "")
	{
		log_id = "outst_" + pConf->mAddrConf.SerialDevice;
		mb = modbus_new_rtu(pConf->mAddrConf.SerialDevice.c_str(),pConf->mAddrConf.BaudRate,(char)pConf->mAddrConf.Parity,pConf->mAddrConf.DataBits,pConf->mAddrConf.StopBits);
		if (mb == NULL)
		{
			spdlog::get("ModbusPort")->error("{}: Stack error: 'Modbus stack creation failed'", Name);
			//TODO: should this throw an exception instead of return?
			return;
		}
		if(modbus_rtu_set_serial_mode(mb,MODBUS_RTU_RS232))
		{
			spdlog::get("ModbusPort")->error("{}: Stack error: 'Failed to set Modbus serial mode to RS232'", Name);
			//TODO: should this throw an exception instead of return?
			return;
		}
	}
	else
	{
		spdlog::get("ModbusPort")->error("{}: No IP interface or serial device defined", Name);
		//TODO: should this throw an exception instead of return?
		return;
	}


	//Allocate memory for bits, input bits, registers, and input registers */
	mb_mapping = modbus_mapping_new(pConf->pPointConf->BitIndicies.Total(),
		pConf->pPointConf->InputBitIndicies.Total(),
		pConf->pPointConf->RegIndicies.Total(),
		pConf->pPointConf->InputRegIndicies.Total());
	if (mb_mapping == NULL)
	{
		spdlog::get("ModbusPort")->error("{}: Failed to allocate the modbus register mapping: {}", Name, modbus_strerror(errno));
		//TODO: should this throw an exception instead of return?
		return;
	}
}

void ModbusOutstationPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }
void ModbusOutstationPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(meas, index, SenderName, pStatusCallback); }

template<typename T>
int find_index (const ModbusReadGroupCollection<T>& aCollection, uint16_t index)
{
	for(auto group : aCollection)
		for(auto group_index = group.start; group_index < group.start + group.count; group_index++)
			if(group_index + group.index_offset == index)
				return (int)group_index;
	return -1;
}

template<typename T>
inline void ModbusOutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	if(std::is_same<T,Analog>::value)
	{
		int map_index = find_index(pConf->pPointConf->InputRegIndicies, index);
		if(map_index >= 0)
			*(mb_mapping->tab_input_registers + map_index) = (uint16_t)meas.value;
		else
		{
			map_index = find_index(pConf->pPointConf->RegIndicies, index);
			if(map_index >= 0)
				*(mb_mapping->tab_registers + map_index) = (uint16_t)meas.value;
		}
		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}
	else if(std::is_same<T,Binary>::value)
	{
		int map_index = find_index(pConf->pPointConf->InputBitIndicies, index);
		if(map_index >= 0)
			*(mb_mapping->tab_input_bits + index) = (uint8_t)meas.value;
		else
		{
			map_index = find_index(pConf->pPointConf->BitIndicies, index);
			if(map_index >= 0)
				*(mb_mapping->tab_bits + index) = (uint8_t)meas.value;
		}
		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}
	//TODO: impl other types

	(*pStatusCallback)(CommandStatus::UNDEFINED);
}

