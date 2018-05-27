/*	opendatacon
*
*	Copyright (c) 2018:
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
* MD3MasterPort.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include <opendnp3/LogLevels.h>
#include <thread>
#include <chrono>
#include <array>
#include <opendnp3/app/MeasurementTypes.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3MasterPort.h"


MD3MasterPort::MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	MD3Port(aName, aConfFilename, aConfOverrides),
	PollScheduler(nullptr)
{}

MD3MasterPort::~MD3MasterPort()
{
	Disable();
	//TODO: SJE Remove any connections that reference this Master so they cant be accessed!
}

void MD3MasterPort::Enable()
{
	if (enabled) return;
	try
	{
		if (pConnection.get() == nullptr)
			throw std::runtime_error("Connection manager uninitilised");

		pConnection->Open();	// Any outstation can take the port down and back up - same as OpenDNP operation for multidrop

		enabled = true;
	}
	catch (std::exception& e)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Problem opening connection : " + Name + " : " + e.what());
		return;
	}
}
void MD3MasterPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	if (pConnection.get() == nullptr)
		return;
	pConnection->Close(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void MD3MasterPort::SocketStateHandler(bool state)
{
	std::string msg;
	if (state)
	{
		PollScheduler->Start();
		PublishEvent(ConnectState::CONNECTED, 0);
		msg = Name + ": Connection established.";
	}
	else
	{
		PollScheduler->Stop();
		// MD3 Binary
		//for(auto index : pConf->pPointConf->BinaryIndicies)
		//for(uint16_t index = range.start; index < range.start + range.count; index++ )
		//			PublishEvent(BinaryQuality::COMM_LOST, index);
		// Do Analogs and Controls.

		PublishEvent(ConnectState::DISCONNECTED, 0);
		msg = Name + ": Connection closed.";
	}
	LOG("MD3OutstationPort", openpal::logflags::INFO, "", msg);
}

void MD3MasterPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	//TODO: Do we re-read the conf file - so we can do a live reload? - How do we kill all the sockets and connections properly?
	std::string ChannelID = MyConf()->mAddrConf.IP + ":" + std::to_string(MyConf()->mAddrConf.Port);

	if (PollScheduler == nullptr)
		PollScheduler.reset(new ASIOScheduler(*pIOS));

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, isServer, MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms));	// Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection);	//Static method
	}

	pConnection->AddMaster(MyConf()->mAddrConf.OutstationAddr,
		std::bind(&MD3MasterPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3MasterPort::SocketStateHandler, this, std::placeholders::_1));

	PollScheduler->Stop();
	PollScheduler->Clear();
	for (auto pg : MyPointConf()->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=]() {
			this->DoPoll(id);
		};
		PollScheduler->Add(pg.second.pollrate, action);
	}
	//	PollScheduler->Start(); Is started and stopped in the socket state handler
}

// Modbus code
void MD3MasterPort::HandleError(int errnum, const std::string& source)
{
	std::string msg = Name + ": " + source + " error: '";// +MD3_strerror(errno) + "'";
	auto log_entry = openpal::LogEntry("MD3MasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	// If not a MD3 error, tear down the connection?
//    if (errnum < MD3_ENOBASE)
//    {
//        this->Disconnect();

//        MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

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
//Modbus code
CommandStatus MD3MasterPort::HandleWriteError(int errnum, const std::string& source)
{
	HandleError(errnum, source);
	switch (errno)
	{
		/*
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
			*/
		default:
			return CommandStatus::UNDEFINED;
	}
}

void MD3MasterPort::ProcessMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	uint8_t ExpectedStationAddress = MyConf()->mAddrConf.OutstationAddr;
	int ExpectedMessageSize = 1;	// Only set in switch statement if not 1

	MD3BlockFormatted Header = CompleteMD3Message[0];
	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.

	//TODO: SJE Check that the flag to master in the message is set.

	// All are included to allow better error reporting.
	switch (Header.GetFunctionCode())
	{
	case ANALOG_UNCONDITIONAL:	// Command and reply
	//	DoAnalogUnconditional(Header);
		break;
	case ANALOG_DELTA_SCAN:		// Command and reply
	//	DoAnalogDeltaScan(Header);
		break;
	case DIGITAL_UNCONDITIONAL_OBS:
	//	DoDigitalUnconditionalObs(Header);
		break;
	case DIGITAL_DELTA_SCAN:
	//	DoDigitalChangeOnly(Header);
		break;
	case HRER_LIST_SCAN:
		// Can be a size of 1 or 2
		ExpectedMessageSize = CompleteMD3Message.size();
	//	if ((ExpectedMessageSize == 1) || (ExpectedMessageSize == 2))
	//		DoDigitalHRER(static_cast<MD3BlockFn9&>(Header), CompleteMD3Message);
	//	else
	//		ExpectedMessageSize = 1;
		break;
	case DIGITAL_CHANGE_OF_STATE:
	//	DoDigitalCOSScan(static_cast<MD3BlockFn10&>(Header));
		break;
	case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
	//	DoDigitalScan(static_cast<MD3BlockFn11MtoS&>(Header));
		break;
	case DIGITAL_UNCONDITIONAL:
	//	DoDigitalUnconditional(static_cast<MD3BlockFn12MtoS&>(Header));
		break;
	case ANALOG_NO_CHANGE_REPLY:
		// Master Only
		break;
	case DIGITAL_NO_CHANGE_REPLY:
		// Master Only
		break;
	case CONTROL_REQUEST_OK:
		// Master Only
		break;
	case FREEZE_AND_RESET:
	//	DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
		break;
	case POM_TYPE_CONTROL:
		ExpectedMessageSize = 2;
	//	DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
		break;
	case DOM_TYPE_CONTROL:
		ExpectedMessageSize = 2;
	//	DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
		break;
	case INPUT_POINT_CONTROL:
		break;
	case RAISE_LOWER_TYPE_CONTROL:
		break;
	case AOM_TYPE_CONTROL:
		ExpectedMessageSize = 2;
	//	DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
		break;
	case CONTROL_OR_SCAN_REQUEST_REJECTED:
		// Master Only
		break;
	case COUNTER_SCAN:
	//	DoCounterScan(Header);
		break;
	case SYSTEM_SIGNON_CONTROL:
	//	DoSystemSignOnControl(static_cast<MD3BlockFn40&>(Header));
		break;
	case SYSTEM_SIGNOFF_CONTROL:
		break;
	case SYSTEM_RESTART_CONTROL:
		break;
	case SYSTEM_SET_DATETIME_CONTROL:
		ExpectedMessageSize = 2;
	//	DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
		break;
	case FILE_DOWNLOAD:
		break;
	case FILE_UPLOAD:
		break;
	case SYSTEM_FLAG_SCAN:
		ExpectedMessageSize = CompleteMD3Message.size();	// Variable size
	//	DoSystemFlagScan(Header, CompleteMD3Message);
		break;
	case LOW_RES_EVENTS_LIST_SCAN:
		break;
	default:
		LOG("MD3MasterPort", openpal::logflags::ERR, "", "Unknown Message Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		break;
	}

	if (ExpectedMessageSize != CompleteMD3Message.size())
	{
		LOG("MD3MasterPort", openpal::logflags::ERR, "", "Unexpected Message Size - " + std::to_string(CompleteMD3Message.size()) +
			" On Station Address - " + std::to_string(Header.GetStationAddress()) +
			" Function - " + std::to_string(Header.GetFunctionCode()));
	}
}

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void MD3MasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;

	bool NewCommands = MyPointConf()->NewDigitalCommands;

/*	auto pConf = static_cast<MD3PortConf*>(this->pConf.get());
	int rc;

	// MD3 function code 0x01 (read coil status)
	for(auto range : pConf->pPointConf->BitIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count > MD3_read_buffer_size)
		{
			if(MD3_read_buffer != nullptr)
				free(MD3_read_buffer);
			MD3_read_buffer = malloc(range.count);
			MD3_read_buffer_size = range.count;
		}
//		rc = MD3_read_bits(mb, range.start, range.count, (uint8_t*)MD3_read_buffer);
//		if (rc == -1)
		{
			HandleError(errno, "read bits poll");
			if(!enabled) return;
		}
	//	else
		{
			uint16_t index = range.start;
			for(uint16_t i = 0; i < rc; i++ )
			{
				PublishEvent(BinaryOutputStatus(((uint8_t*)MD3_read_buffer)[i] != false),index);
				++index;
			}
		}
	}

	// MD3 function code 0x02 (read input status)
	for(auto range : pConf->pPointConf->InputBitIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;

		if (range.count > MD3_read_buffer_size)
		{
			if(MD3_read_buffer != nullptr)
				free(MD3_read_buffer);
			MD3_read_buffer = malloc(range.count);
			MD3_read_buffer_size = range.count;
		}
	//	rc = MD3_read_input_bits(mb, range.start, range.count, (uint8_t*)MD3_read_buffer);
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
				PublishEvent(Binary(((uint8_t*)MD3_read_buffer)[i] != false),index);
				++index;
			}
		}
	}

	// MD3 function code 0x03 (read holding registers)
	for(auto range : pConf->pPointConf->RegIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;

		if (range.count*2 > MD3_read_buffer_size)
		{
			if(MD3_read_buffer != nullptr)
				free(MD3_read_buffer);
			MD3_read_buffer = malloc(range.count*2);
			MD3_read_buffer_size = range.count*2;
		}
	//	rc = MD3_read_registers(mb, range.start, range.count, (uint16_t*)MD3_read_buffer);
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
				PublishEvent(AnalogOutputInt16(((uint16_t*)MD3_read_buffer)[i]),index);
				++index;
			}
		}
	}

	// MD3 function code 0x04 (read input registers)
	for(auto range : pConf->pPointConf->InputRegIndicies)
	{
		if (pollgroup && (range.pollgroup != pollgroup))
			continue;
		if (range.count*2 > MD3_read_buffer_size)
		{
			if(MD3_read_buffer != nullptr)
				free(MD3_read_buffer);
			MD3_read_buffer = malloc(range.count*2);
			MD3_read_buffer_size = range.count*2;
		}
	//	rc = MD3_read_input_registers(mb, range.start, range.count, (uint16_t*)MD3_read_buffer);
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
				PublishEvent(Analog(((uint16_t*)MD3_read_buffer)[i]),index);
				++index;
			}
		}
	}
	*/
}


//Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
std::future<CommandStatus> MD3MasterPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName){ return EventT(arCommand, index, SenderName); }

std::future<CommandStatus> MD3MasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	auto cmd_promise = std::promise<CommandStatus>();
	auto cmd_future = cmd_promise.get_future();

	if(!enabled)
	{
		cmd_promise.set_value(CommandStatus::UNDEFINED);
		return cmd_future;
	}

	//something upstream has connected
	if(state == ConnectState::CONNECTED)
	{
		// Only change stack state if it is an on demand server
		if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			//this->Connect();
		}
	}

	cmd_promise.set_value(CommandStatus::SUCCESS);
	return cmd_future;
}

/*MD3ReadGroup<Binary>* MD3MasterPort::GetRange(uint16_t index)
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());
	for(auto& range : pConf->pPointConf->BitIndicies)
	{
		if ((index >= range.start) && (index < range.start + range.count))
			return &range;
	}
	return nullptr;
}
*/

/*
template<>
CommandStatus MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, uint16_t index)
{
	if (
	      (command.functionCode == ControlCode::NUL) ||
	      (command.functionCode == ControlCode::UNDEFINED)
	      )
	{
		return CommandStatus::FORMAT_ERROR;
	}

	// MD3 function code 0x01 (read coil status)
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc;
	if (
	      (command.functionCode == ControlCode::LATCH_OFF) ||
	      (command.functionCode == ControlCode::TRIP_PULSE_ON)
	      )
	{
//		rc = MD3_write_bit(mb, index, false);
	}
	else
	{
		//ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
//		rc = MD3_write_bit(mb, index, true);
	}

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write bit");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt16& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt32& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputFloat32& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputDouble64& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}
*/

template<typename T>
inline std::future<CommandStatus> MD3MasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName)
{
	std::unique_ptr<std::promise<CommandStatus> > cmd_promise { new std::promise<CommandStatus>() };
	auto cmd_future = cmd_promise->get_future();

	if(!enabled)
	{
		cmd_promise->set_value(CommandStatus::UNDEFINED);
		return cmd_future;
	}

//	cmd_promise->set_value(WriteObject(arCommand, index));
	/*
	auto lambda = capture( std::move(cmd_promise),
	                      [=]( std::unique_ptr<std::promise<CommandStatus>> & cmd_promise ) {
	                          cmd_promise->set_value(WriteObject(arCommand, index));
	                      } );
	pIOS->post([&](){ lambda(); });
	*/
	return cmd_future;
}



