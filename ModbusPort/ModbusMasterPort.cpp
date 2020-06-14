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

#include "ModbusMasterPort.h"
#include <array>
#include <chrono>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <thread>

ModbusMasterPort::~ModbusMasterPort()
{
	Disable();
	if (modbus_read_buffer != nullptr)
		free(modbus_read_buffer);
}

void ModbusMasterPort::Enable()
{
	if(enabled) return;
	enabled = true;

	pTCPRetryTimer = pIOS->make_steady_timer();
	PollScheduler = std::make_unique<ASIOScheduler>(*pIOS);

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	// Only change stack state if it is a persistent server
	if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
	{
		MBSync->Execute([this](modbus_t* mb)
			{
				Connect(mb);
			});
	}
}

void ModbusMasterPort::Connect(modbus_t* mb)
{
	if(!enabled) return;
	if (stack_enabled) return;

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	if (mb == nullptr)
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("{}: Connect error: 'Modbus stack failed'", Name);
		return;
	}

	if (modbus_connect(mb) == -1)
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->warn("{}: Connect error: '{}'", Name, modbus_strerror(errno));

		//try again later - except for manual connections
		if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT || pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
			pTCPRetryTimer->async_wait(
				[this](asio::error_code err_code)
				{
					if(err_code != asio::error::operation_aborted)
						MBSync->Execute([this](modbus_t* mb)
							{
								Connect(mb);
							});
				});
		}
		return;
	}

	stack_enabled = true;
	if(auto log = odc::spdlog_get("ModbusPort"))
		log->info("{}: Connect success!", Name);

	modbus_set_slave(mb, pConf->mAddrConf.OutstationAddr);

// doesn't work - at least not with my serial RTU
//    uint8_t tab_bytes[64];
//    int rc = modbus_report_slave_id(mb, tab_bytes);
//    if (rc > 1)
//    {
//	    //log
//    }

	PollScheduler->Clear();
	for(auto pg : pConf->pPointConf->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=]()
				  {
					  MBSync->Execute([=](modbus_t* mb)
						  {
							  DoPoll(id,mb);
						  });
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

	if(!MBSync->isNull())
		MBSync->Execute([](modbus_t* mb)
			{
				modbus_close(mb);
			});

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	//TODO: implement a comms point

	auto event = std::make_shared<EventInfo>(EventType::BinaryQuality,0,Name,QualityFlags::COMM_LOST);
	event->SetPayload<EventType::BinaryQuality>(QualityFlags::COMM_LOST);

	// Modbus function code 0x01 (read coil status)
	for(const auto& range : pConf->pPointConf->BitIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
		{
			event->SetIndex(index);
			PublishEvent(event);
		}

	// Modbus function code 0x02 (read input status)
	for(const auto& range : pConf->pPointConf->InputBitIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
		{
			event->SetIndex(index);
			PublishEvent(event);
		}

	// Modbus function code 0x03 (read holding registers)
	for(const auto& range : pConf->pPointConf->RegIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
		{
			event->SetIndex(index);
			PublishEvent(event);
		}

	// Modbus function code 0x04 (read input registers)
	for(const auto& range : pConf->pPointConf->InputRegIndicies)
		for(uint16_t index = range.start; index < range.start + range.count; index++ )
		{
			event->SetIndex(index);
			PublishEvent(event);
		}
}

void ModbusMasterPort::HandleError(int errnum, const std::string& source)
{
	if(auto log = odc::spdlog_get("ModbusPort"))
		log->warn("{}: {} error: '{}'", Name, source, modbus_strerror(errno));

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

void ModbusMasterPort::Build()
{
	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	std::string log_id;

	if(pConf->mAddrConf.IP != "")
	{
		log_id = "mast_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);

		//TODO: collect these on a collection of modbus tcp connections
		MBSync = std::make_unique<ModbusExecutor>(
			modbus_new_tcp_pi(pConf->mAddrConf.IP.c_str(), std::to_string(pConf->mAddrConf.Port).c_str()), *pIOS);

		if (MBSync->isNull())
		{
			std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error(msg);
			throw std::runtime_error(msg);
		}
	}
	else if(pConf->mAddrConf.SerialDevice != "")
	{
		log_id = "mast_" + pConf->mAddrConf.SerialDevice;
		MBSync = std::make_unique<ModbusExecutor>(
			modbus_new_rtu(pConf->mAddrConf.SerialDevice.c_str(),pConf->mAddrConf.BaudRate,(char)pConf->mAddrConf.Parity,pConf->mAddrConf.DataBits,pConf->mAddrConf.StopBits), *pIOS);

		if (MBSync->isNull())
		{
			std::string msg = Name + ": Stack error: 'Modbus stack creation failed'";
			if(auto log = odc::spdlog_get("ModbusPort"))
				log->error(msg);
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
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error(msg);
		throw std::runtime_error(msg);
	}
}

void ModbusMasterPort::DoPoll(uint32_t pollgroup, modbus_t* mb)
{
	if(!enabled) return;

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	int rc;

	// Modbus function code 0x01 (read coil status)
	for(const auto& range : pConf->pPointConf->BitIndicies)
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
				auto event = std::make_shared<EventInfo>(EventType::BinaryOutputStatus,index,Name,QualityFlags::ONLINE);
				event->SetPayload<EventType::BinaryOutputStatus>(((uint8_t*)modbus_read_buffer)[i] != false);
				PublishEvent(event);
				++index;
			}
		}
	}

	// Modbus function code 0x02 (read input status)
	for(const auto& range : pConf->pPointConf->InputBitIndicies)
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
				auto event = std::make_shared<EventInfo>(EventType::Binary,index,Name,QualityFlags::ONLINE);
				event->SetPayload<EventType::Binary>(((uint8_t*)modbus_read_buffer)[i] != false);
				PublishEvent(event);
				++index;
			}
		}
	}

	// Modbus function code 0x03 (read holding registers)
	for(const auto& range : pConf->pPointConf->RegIndicies)
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
				auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16,index,Name,QualityFlags::ONLINE);
				auto payload = AO16(((uint16_t*)modbus_read_buffer)[i],CommandStatus::SUCCESS);
				event->SetPayload<EventType::AnalogOutputInt16>(std::move(payload));
				PublishEvent(event);
				++index;
			}
		}
	}

	// Modbus function code 0x04 (read input registers)
	for(const auto& range : pConf->pPointConf->InputRegIndicies)
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
				auto event = std::make_shared<EventInfo>(EventType::Analog,index,Name,QualityFlags::ONLINE);
				event->SetPayload<EventType::Analog>(double(((uint16_t*)modbus_read_buffer)[i]));
				PublishEvent(event);
				++index;
			}
		}
	}
}

template <EventType t>
ModbusReadGroup *ModbusMasterPort::GetRange(uint16_t index)
{
	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());
	ModbusReadGroupCollection collection;
	switch(t)
	{
		case EventType::Analog:
			collection = pConf->pPointConf->RegIndicies; break;
		case EventType::Binary:
			collection = pConf->pPointConf->BitIndicies; break;
	}
	for(auto& range : collection)
	{
		if ((index >= range.start) && (index < range.start + range.count))
			return &range;
	}
	return nullptr;
}

CommandStatus ModbusMasterPort::WriteObject(modbus_t *mb, const ControlRelayOutputBlock& command, uint16_t index)
{
	if (
		(command.functionCode == ControlCode::NUL) ||
		(command.functionCode == ControlCode::UNDEFINED)
		)
	{
		return CommandStatus::NOT_SUPPORTED;
	}

	// Modbus function code 0x01 (read coil status)
	ModbusReadGroup* TargetRange = GetRange<EventType::Binary>(index);
	if (TargetRange == nullptr) return CommandStatus::NOT_SUPPORTED;

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
		MBSync->Execute([=](modbus_t* mb)
			{
				DoPoll(TargetRange->pollgroup,mb);
			});

	if (rc == -1) return HandleWriteError(errno, "write bit");
	return CommandStatus::SUCCESS;
}

CommandStatus ModbusMasterPort::WriteObject(modbus_t* mb, const int16_t output, uint16_t index)
{
	ModbusReadGroup* TargetRange = GetRange<EventType::Analog>(index);
	if (TargetRange == nullptr) return CommandStatus::NOT_SUPPORTED;

	int rc = modbus_write_register(mb, index, output);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		MBSync->Execute([=](modbus_t* mb)
			{
				DoPoll(TargetRange->pollgroup,mb);
			});

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

CommandStatus ModbusMasterPort::WriteObject(modbus_t* mb, const int32_t output, uint16_t index)
{
	ModbusReadGroup* TargetRange = GetRange<EventType::Analog>(index);
	if (TargetRange == nullptr) return CommandStatus::NOT_SUPPORTED;

	if(output > std::numeric_limits<int16_t>::max() || output < std::numeric_limits<int16_t>::min())
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("Analog overrange for 16-bit modbus write to index {}",index);
		return CommandStatus::OUT_OF_RANGE;
	}

	int rc = modbus_write_register(mb, index, static_cast<int16_t>(output));

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		MBSync->Execute([=](modbus_t* mb)
			{
				DoPoll(TargetRange->pollgroup,mb);
			});

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

CommandStatus ModbusMasterPort::WriteObject(modbus_t* mb, const double output, uint16_t index)
{
	ModbusReadGroup* TargetRange = GetRange<EventType::Analog>(index);
	if (TargetRange == nullptr) return CommandStatus::NOT_SUPPORTED;

	//TODO: implement scaling in the config - hard code for now:
	auto scaled_float = output * 100;
	if(scaled_float > std::numeric_limits<int16_t>::max() || scaled_float < std::numeric_limits<int16_t>::min())
	{
		if(auto log = odc::spdlog_get("ModbusPort"))
			log->error("Scaled float overrange for 16-bit modbus write to index {}",index);
		return CommandStatus::OUT_OF_RANGE;
	}

	uint16_t scaled_output = static_cast<int16_t>(scaled_float);
	int rc = modbus_write_register(mb, index, scaled_output);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		MBSync->Execute([=](modbus_t* mb)
			{
				DoPoll(TargetRange->pollgroup,mb);
			});

	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}
CommandStatus ModbusMasterPort::WriteObject(modbus_t *mb, const float output, uint16_t index)
{
	return WriteObject(mb, static_cast<double>(output),index);
}

void ModbusMasterPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	auto pConf = static_cast<ModbusPortConf*>(this->pConf.get());

	auto write = [=](auto payload)
			 {
				 MBSync->Execute([=](modbus_t* mb)
					 {
						 (*pStatusCallback)(WriteObject(mb, payload, event->GetIndex()));
					 });
			 };

	switch(event->GetEventType())
	{
		case EventType::ControlRelayOutputBlock:
			return write(event->GetPayload<EventType::ControlRelayOutputBlock>());
		case EventType::AnalogOutputInt16:
			return write(event->GetPayload<EventType::AnalogOutputInt16>().first);
		case EventType::AnalogOutputInt32:
			return write(event->GetPayload<EventType::AnalogOutputInt32>().first);
		case EventType::AnalogOutputFloat32:
			return write(event->GetPayload<EventType::AnalogOutputFloat32>().first);
		case EventType::AnalogOutputDouble64:
			return write(event->GetPayload<EventType::AnalogOutputDouble64>().first);
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();

			//something upstream has connected
			if(state == ConnectState::CONNECTED)
			{
				// Only change stack state if it is an on demand server
				if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
				{
					MBSync->Execute([=](modbus_t* mb)
						{
							Connect(mb);
						});
				}
			}
			else if (state == ConnectState::DISCONNECTED)
			{
				// Only change stack state if it is an on demand server - and we're no longer in-demand
				if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND && !InDemand())
				{
					Disconnect();
				}
			}
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}
}




