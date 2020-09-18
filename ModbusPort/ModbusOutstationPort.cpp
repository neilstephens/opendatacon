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
// This is a fix for a VS bug
#define NOMINMAX

#include "ModbusOutstationPort.h"
#include <chrono>
#include <iostream>
#include <opendatacon/util.h>
#include <regex>

ModbusOutstationPort::ModbusOutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	ModbusPort(aName, aConfFilename, aConfOverrides)
{}

ModbusOutstationPort::~ModbusOutstationPort()
{
	Disable();
	if (mb_mapping != nullptr)
		modbus_mapping_free(mb_mapping);
}

void ModbusOutstationPort::Enable()
{
	if(enabled) return;
	enabled = true;

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

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

	if (MBSync->isNull())
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("{}: Connect error: 'Modbus stack failed'", Name);
		return;
	}

	MBSync->Execute([this](modbus_t* mb)
		{
			int s = modbus_tcp_pi_listen(mb, 1);
			if (s == -1)
			{
			      if(auto log = odc::spdlog_get("ModbusPort"))
					log->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));
			      return;
			}

			int r = modbus_tcp_pi_accept(mb, &s);
			if (r == -1)
			{
			      if(auto log = odc::spdlog_get("ModbusPort"))
					log->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));
			      return;
			}
			PublishEvent(ConnectState::CONNECTED);
		});
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

	if(MBSync->isNull())
		return;
	MBSync->Execute([this](modbus_t* mb)
		{
			modbus_close(mb);
			PublishEvent(ConnectState::DISCONNECTED);
		});
}

void ModbusOutstationPort::Build()
{
	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	std::string log_id;

	if(pConf->mAddrConf.IP != "")
	{
		log_id = "outst_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);

		//TODO: collect these on a collection of modbus tcp connections
		MBSync = std::make_unique<ModbusExecutor>(
			modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), std::to_string(pConf->mAddrConf.Port).c_str()),*pIOS);
		if (MBSync->isNull())
		{
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error("{}: Stack error: 'Modbus stack creation failed'", Name);
			//TODO: should this throw an exception instead of return?
			return;
		}
	}
	else if(pConf->mAddrConf.SerialDevice != "")
	{
		log_id = "outst_" + pConf->mAddrConf.SerialDevice;
		MBSync = std::make_unique<ModbusExecutor>(
			modbus_new_rtu(pConf->mAddrConf.SerialDevice.c_str(),pConf->mAddrConf.BaudRate,(char)pConf->mAddrConf.Parity,pConf->mAddrConf.DataBits,pConf->mAddrConf.StopBits), *pIOS);
		if (MBSync->isNull())
		{
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error("{}: Stack error: 'Modbus stack creation failed'", Name);
			//TODO: should this throw an exception instead of return?
			return;
		}
		MBSync->Execute([this](modbus_t* mb)
			{
				if(modbus_rtu_set_serial_mode(mb,MODBUS_RTU_RS232))
				{
				      if(auto log = odc::spdlog_get("ModbusPort"))
						log->error("{}: Stack error: 'Failed to set Modbus serial mode to RS232'", Name);
				//TODO: should this throw an exception instead of return?
				      return;
				}
			});
	}
	else
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("{}: No IP interface or serial device defined", Name);
		//TODO: should this throw an exception instead of return?
		return;
	}


	//Allocate memory for bits, input bits, registers, and input registers */
	mb_mapping = modbus_mapping_new(pConf->pPointConf->BitIndicies.Total(),
		pConf->pPointConf->InputBitIndicies.Total(),
		pConf->pPointConf->RegIndicies.Total(),
		pConf->pPointConf->InputRegIndicies.Total());
	if (mb_mapping == nullptr)
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("{}: Failed to allocate the modbus register mapping: {}", Name, modbus_strerror(errno));
		//TODO: should this throw an exception instead of return?
		return;
	}
}

int find_index (const ModbusReadGroupCollection& aCollection, uint16_t index)
{
	for(const auto& group : aCollection)
		for(auto group_index = group.start; group_index < group.start + group.count; group_index++)
			if(group_index + group.index_offset == index)
				return (int)group_index;
	return -1;
}

void ModbusOutstationPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	auto event_type = event->GetEventType();
	auto index = event->GetIndex();

	if(event_type == EventType::Analog)
	{
		//TODO: scaling option in config - use 100 for now
		auto scaled_float = event->GetPayload<EventType::Analog>()*100;
		if(scaled_float > std::numeric_limits<int16_t>::max() || scaled_float < std::numeric_limits<int16_t>::min())
		{
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error("Scaled float overrange for 16-bit modbus load to index {}",index);
			return (*pStatusCallback)(CommandStatus::OUT_OF_RANGE);
		}

		int map_index = find_index(pConf->pPointConf->InputRegIndicies, index);
		if(map_index >= 0)
			*(mb_mapping->tab_input_registers + index) = static_cast<int16_t>(scaled_float);
		else
		{
			map_index = find_index(pConf->pPointConf->RegIndicies, index);
			if(map_index >= 0)
				*(mb_mapping->tab_registers + index) = static_cast<int16_t>(scaled_float);
		}

		if(map_index == 0)
		{
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
		}

		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}
	else if(event_type == EventType::Binary)
	{
		int map_index = find_index(pConf->pPointConf->InputBitIndicies, index);
		if(map_index >= 0)
			*(mb_mapping->tab_input_bits + index) = event->GetPayload<EventType::Binary>();
		else
		{
			map_index = find_index(pConf->pPointConf->BitIndicies, index);
			if(map_index >= 0)
				*(mb_mapping->tab_bits + index) = event->GetPayload<EventType::Binary>();
		}

		if(map_index == 0)
		{
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
		}

		(*pStatusCallback)(CommandStatus::SUCCESS);
		return;
	}
	//TODO: impl other types

	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}

