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

#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

#include <opendnp3/app/MeasurementTypes.h>
#include "ModbusMasterPort.h"
#include <array>

ModbusMasterPort::~ModbusMasterPort()
{
	Disable();
	if (mb != nullptr)
		modbus_free(mb);
	if (modbus_read_buffer != nullptr)
		free(modbus_read_buffer);
}

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
		spdlog::get("ModbusPort")->error("{}: Connect error: 'Modbus stack failed'", Name);
		return;
	}

	if (modbus_connect(mb) == -1)
	{
		spdlog::get("ModbusPort")->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));

		//try again later - except for manual connections
		if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT || pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
			pTCPRetryTimer->async_wait(
				[this](asio::error_code err_code)
				{
					if(err_code != asio::error::operation_aborted)
						this->Connect();
				});
		}
		return;
	}

	stack_enabled = true;
	spdlog::get("ModbusPort")->info("{}: Connect success!", Name);

	modbus_set_slave(mb, pConf->mAddrConf.OutstationAddr);

// doesn't work - at least not with my serial RTU
//    uint8_t tab_bytes[64];
//    int rc = modbus_report_slave_id(mb, tab_bytes);
//    if (rc > 1)
//    {
//	    std::string msg = Name + "Run Status Indicator: %s" + (tab_bytes[1] ? "ON" : "OFF");
//	    auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::INFO,"", msg.c_str(), -1);
//	    pLoggers->Log(log_entry);
//    }

	PollScheduler->Clear();
	for(auto pg : pConf->pPointConf->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=]()
				  {
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

	//Update the quality of point
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	// Modbus function code 0x01 (read coil status)
	for(auto range : pConf->pPointConf->BitIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
			PublishEvent(BinaryQuality::COMM_LOST, index);

	// Modbus function code 0x02 (read input status)
	for(auto range : pConf->pPointConf->InputBitIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
			PublishEvent(BinaryQuality::COMM_LOST, index);

	// Modbus function code 0x03 (read holding registers)
	for(auto range : pConf->pPointConf->RegIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
			PublishEvent(AnalogQuality::COMM_LOST,index);

	// Modbus function code 0x04 (read input registers)
	for(auto range : pConf->pPointConf->InputRegIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
			PublishEvent(AnalogQuality::COMM_LOST,index);
}

void ModbusMasterPort::HandleError(int errnum, const std::string& source)
{
	spdlog::get("ModbusPort")->warn("{}: {} error: '{}'", Name, source, modbus_strerror(errno));

	// If not a modbus error, tear down the connection?
//    if (errnum < MODBUS_ENOBASE)
//    {
//        this->Disconnect();

//        ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

//        // Try and re-connect if a persistent connection
//        if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
//        {
//            //try again later
//            pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
//            pTCPRetryTimer->async_wait(
//                                       [this](asio::error_code err_code)
//                                       {
//                                           if(err_code != asio::error::operation_aborted)
//                                               this->Connect();
//                                       });
//        }
//    }
}

CommandStatus ModbusMasterPort::HandleWriteError(int errnum, const std::string& source)
{
	HandleError(errnum, source);
	switch (errno)
	{
		case EMBXILFUN: //return "Illegal function";
			return CommandStatus::NOT_SUPPORTED;
		case EMBBADCRC:  //return "Invalid CRC";
		case EMBBADDATA: //return "Invalid data";
		case EMBBADEXC:  //return "Invalid exception code";
		case EMBXILADD:  //return "Illegal data address";
		case EMBXILVAL:  //return "Illegal data value";
		case EMBMDATA:   //return "Too many data";
			return CommandStatus::FORMAT_ERROR;
		case EMBXSFAIL:  //return "Slave device or server failure";
		case EMBXMEMPAR: //return "Memory parity error";
			return CommandStatus::HARDWARE_ERROR;
		case EMBXGTAR: //return "Target device failed to respond";
			return CommandStatus::TIMEOUT;
		case EMBXACK:   //return "Acknowledge";
		case EMBXSBUSY: //return "Slave device or server is busy";
		case EMBXNACK:  //return "Negative acknowledge";
		case EMBXGPATH: //return "Gateway path unavailable";
		default:
			return CommandStatus::UNDEFINED;
	}
}

void ModbusMasterPort::BuildOrRebuild()
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	std::string log_id;

	if(pConf->mAddrConf.IP != "")
	{
		log_id = "mast_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);

		//TODO: collect these on a collection of modbus tcp connections
		mb = modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), std::to_string(pConf->mAddrConf.Port).c_str());
		if (mb == NULL)
		{
			std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
			spdlog::get("ModbusPort")->error(msg);
			throw std::runtime_error(msg);
		}
	}
	else if(pConf->mAddrConf.SerialDevice != "")
	{
		log_id = "mast_" + pConf->mAddrConf.SerialDevice;
		mb = modbus_new_rtu(pConf->mAddrConf.SerialDevice.c_str(),pConf->mAddrConf.BaudRate,(char)pConf->mAddrConf.Parity,pConf->mAddrConf.DataBits,pConf->mAddrConf.StopBits);
		if (mb == NULL)
		{
			std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
			spdlog::get("ModbusPort")->error(msg);
			throw std::runtime_error(msg);
		}

//Not needed? - doesn't work at least
//		if(modbus_rtu_set_serial_mode(mb,MODBUS_RTU_RS232) == -1)
//		{
//			std::string msg = Name + ": Stack error: 'Failed to set Modbus serial mode to RS232'";
//			auto log_entry = openpal::LogEntry("ModbusMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
//			pLoggers->Log(log_entry);
//			throw std::runtime_error(msg);
//		}
	}
	else
	{
		std::string msg = Name + ": No IP address or serial device defined";
		spdlog::get("ModbusPort")->error(msg);
		throw std::runtime_error(msg);
	}
}

void ModbusMasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	int rc;

	// Modbus function code 0x01 (read coil status)
	for(auto range : pConf->pPointConf->BitIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count > modbus_read_buffer_size)
		{
			if(modbus_read_buffer != nullptr)
				free(modbus_read_buffer);
			modbus_read_buffer = malloc(range.count);
			modbus_read_buffer_size = range.count;
		}
		rc = modbus_read_bits(mb, range.start, range.count, (uint8_t*)modbus_read_buffer);
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
				PublishEvent(BinaryOutputStatus(((uint8_t*)modbus_read_buffer)[i] != false),index);
				++index;
			}
		}
	}

	// Modbus function code 0x02 (read input status)
	for(auto range : pConf->pPointConf->InputBitIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count > modbus_read_buffer_size)
		{
			if(modbus_read_buffer != nullptr)
				free(modbus_read_buffer);
			modbus_read_buffer = malloc(range.count);
			modbus_read_buffer_size = range.count;
		}
		rc = modbus_read_input_bits(mb, range.start, range.count, (uint8_t*)modbus_read_buffer);
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
				PublishEvent(Binary(((uint8_t*)modbus_read_buffer)[i] != false),index);
				++index;
			}
		}
	}

	// Modbus function code 0x03 (read holding registers)
	for(auto range : pConf->pPointConf->RegIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count*2 > modbus_read_buffer_size)
		{
			if(modbus_read_buffer != nullptr)
				free(modbus_read_buffer);
			modbus_read_buffer = malloc(range.count*2);
			modbus_read_buffer_size = range.count*2;
		}
		rc = modbus_read_registers(mb, range.start, range.count, (uint16_t*)modbus_read_buffer);
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
				PublishEvent(AnalogOutputInt16(((uint16_t*)modbus_read_buffer)[i]),index);
				++index;
			}
		}
	}

	// Modbus function code 0x04 (read input registers)
	for(auto range : pConf->pPointConf->InputRegIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count*2 > modbus_read_buffer_size)
		{
			if(modbus_read_buffer != nullptr)
				free(modbus_read_buffer);
			modbus_read_buffer = malloc(range.count*2);
			modbus_read_buffer_size = range.count*2;
		}
		rc = modbus_read_input_registers(mb, range.start, range.count, (uint16_t*)modbus_read_buffer);
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
				PublishEvent(Analog(((uint16_t*)modbus_read_buffer)[i]),index);
				++index;
			}
		}
	}
}

//Implement some IOHandler - parent ModbusPort implements the rest to return NOT_SUPPORTED
void ModbusMasterPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(arCommand, index, SenderName, pStatusCallback); }
void ModbusMasterPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(arCommand, index, SenderName, pStatusCallback); }
void ModbusMasterPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(arCommand, index, SenderName, pStatusCallback); }
void ModbusMasterPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(arCommand, index, SenderName, pStatusCallback); }
void ModbusMasterPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback){ return EventT(arCommand, index, SenderName, pStatusCallback); }

void ModbusMasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	//something upstream has connected
	if(state == ConnectState::CONNECTED)
	{
		// Only change stack state if it is an on demand server
		if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			this->Connect();
		}
	}

	(*pStatusCallback)(CommandStatus::SUCCESS);
}

ModbusReadGroup<Binary>* ModbusMasterPort::GetRange(uint16_t index)
{
	ModbusPortConf* pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	for(auto& range : pConf->pPointConf->BitIndicies)
	{
		if ((index >= range.start) && (index < range.start + range.count))
			return &range;
	}
	return nullptr;
}

template<>
CommandStatus ModbusMasterPort::WriteObject(const ControlRelayOutputBlock& command, uint16_t index)
{
	if (
		(command.functionCode == ControlCode::NUL) ||
		(command.functionCode == ControlCode::UNDEFINED)
		)
	{
		return CommandStatus::FORMAT_ERROR;
	}

	// Modbus function code 0x01 (read coil status)
	ModbusReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc;
	if (
		(command.functionCode == ControlCode::LATCH_OFF) ||
		(command.functionCode == ControlCode::TRIP_PULSE_ON)
		)
	{
		rc = modbus_write_bit(mb, index, false);
	}
	else
	{
		//ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
		rc = modbus_write_bit(mb, index, true);
	}

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

	if (rc == -1) return HandleWriteError(errno, "write bit");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus ModbusMasterPort::WriteObject(const AnalogOutputInt16& command, uint16_t index)
{
	ModbusReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc = modbus_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus ModbusMasterPort::WriteObject(const AnalogOutputInt32& command, uint16_t index)
{
	ModbusReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc = modbus_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus ModbusMasterPort::WriteObject(const AnalogOutputFloat32& command, uint16_t index)
{
	ModbusReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc = modbus_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus ModbusMasterPort::WriteObject(const AnalogOutputDouble64& command, uint16_t index)
{
	ModbusReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc = modbus_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<typename T>
inline void ModbusMasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	(*pStatusCallback)(WriteObject(arCommand, index));
}



