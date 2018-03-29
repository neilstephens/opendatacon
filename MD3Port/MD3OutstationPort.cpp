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
  * MD3OutstationPort.cpp
  *
  *  Created on: 16/10/2014
  *      Author: Alan Murray
  */

  /* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
   This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/MD3) will turn back into scada commands and send out to the "real" Outstation.
   So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
   So an Event to an outstation will be data that needs to be sent up to the scada master.
   An event from an outstation will be a master control signal to turn something on or off.
  */
#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "MD3Engine.h"
#include "MD3OutstationPort.h"
#include "LogMacro.h"
#include "MD3.h"
#include "CRC.h"

#include <opendnp3/LogLevels.h>

//TODO: Check out http://www.pantheios.org/ logging library..

MD3OutstationPort::MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	MD3Port(aName, aConfFilename, aConfOverrides)
{
	// Dont load conf here, do it in MD3Port
}

MD3OutstationPort::~MD3OutstationPort()
{
	Disable();
}

void MD3OutstationPort::SetSendTCPDataFn(std::function<void(std::string)> Send)
{
	SendTCPDataFn = Send;
}

void MD3OutstationPort::Enable()
{
	if (enabled) return;
	try
	{
		if (pSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitilised");
		pSockMan->Open();
		enabled = true;
	}
	catch (std::exception& e)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Problem opening connection : " + Name + " : " + e.what());
		return;
	}
}
void MD3OutstationPort::Disable()
{
	if (!enabled) return;
	enabled = false;
	if (pSockMan.get() == nullptr)
		return;
	pSockMan->Close();
}

void MD3OutstationPort::SocketStateHandler(bool state)
{
	std::string msg;
	if (state)
	{
		PublishEvent(ConnectState::CONNECTED, 0);
		msg = Name + ": Connection established.";
	}
	else
	{
		PublishEvent(ConnectState::DISCONNECTED, 0);
		msg = Name + ": Connection closed.";
	}
	LOG("MD3OutstationPort", openpal::logflags::INFO, "", msg);
}


void MD3OutstationPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	//TODO: Do we re-read the conf file - so we can do a live reload?

	//TODO: We need a static list of sockets, so that if the socket already exists, we dont try and create it again.
	// The socket id will be ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	// We also need a list of Outstations on this socket, so that we can call the appropriate

	//TODO: use event buffer size once socket manager supports it
	pSockMan.reset(new TCPSocketManager<std::string>
		(pIOS, isServer, MyConf()->mAddrConf.IP, std::to_string(MyConf()->mAddrConf.Port),
			std::bind(&MD3OutstationPort::ReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&MD3OutstationPort::SocketStateHandler, this, std::placeholders::_1),
			true,
			MyConf()->TCPConnectRetryPeriodms));
}

// The only method that sends to the TCP Socket
void MD3OutstationPort::SendResponse(std::vector<MD3Block> &CompleteMD3Message)
{
	// Turn the blocks into a binary string.
	std::string MD3Message;
	for (auto blk : CompleteMD3Message)
	{
		MD3Message += blk.ToBinaryString();
	}

	// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
	// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function due to difference between method and lambda
	if (SendTCPDataFn != nullptr)
		SendTCPDataFn(MD3Message);
	else
		pSockMan->Write(std::string(MD3Message));
}

void MD3OutstationPort::ReadCompletionHandler(buf_t& readbuf)
{
	// We are currently assuming a whole complete packet will turn up in one unit. If not it will be difficult to do the packet decoding and multidrop routing.
	// MD3 only has addressing information in the first block of the packet.

	// We should have a multiple of 6 bytes. 5 data bytes and one padding byte for every MD3 block, then possibkly mutiple blocks
	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() >= MD3BlockArraySize)
	{
		MD3BlockArray d;
		for (int i = 0; i < MD3BlockArraySize; i++)
		{
			d[i] = readbuf.sgetc();
			readbuf.consume(1);
		}

		auto md3block = MD3Block(d);	// Supposed to be a 6 byte array..

		if (md3block.CheckSumPasses())
		{
			if (md3block.IsFormattedBlock())
			{
				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if MD3Message is empty.
				if (MD3Message.size() != 0)
				{
					LOG("DNP3OutstationPort", openpal::logflags::WARN, "", "Received a start block when we have not got to an end block - discarding data blocks - " + std::to_string(MD3Message.size()));
					MD3Message.clear();
				}
				MD3Message.push_back(md3block);	// Takes a copy of the block
			}
			else if (MD3Message.size() == 0)
			{
				LOG("DNP3OutstationPort", openpal::logflags::WARN, "", "Received a non start block when we are waiting for a start block - discarding data - " + md3block.ToString());
			}
			else
			{
				MD3Message.push_back(md3block);	// Takes a copy of the block
			}

			// The start and any other block can be the end block
			if (md3block.IsEndOfMessageBlock())
			{
				// Once we have the last block, then hand off MD3Message to process.
				RouteMD3Message(MD3Message);
				MD3Message.clear();	// Empty message block queue
			}
		}
		else
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Checksum failure on received MD3 block - " + md3block.ToString());
	}

	// Check for and consume any not 6 byte block data - should never happen...
	if (readbuf.size() > 0)
	{
		int bytesleft = readbuf.size();
		LOG("DNP3OutstationPort", openpal::logflags::WARN, "", "Had data left over after reading blocks - " + std::to_string(bytesleft) +" bytes");
		readbuf.consume(readbuf.size());
	}
}

void MD3OutstationPort::RouteMD3Message(std::vector<MD3Block> &CompleteMD3Message)
{
	// Only passing in the variable to make unit testing simpler.
	// We have a full set of MD3 message blocks from a minimum of 1.
	assert(CompleteMD3Message.size() != 0);

	uint8_t ExpectedStationAddress = MyConf()->mAddrConf.OutstationAddr;
	uint8_t StationAddress = CompleteMD3Message[0].GetStationAddress();

	// Change this to route to the correct outstation in future.
	if (ExpectedStationAddress != StationAddress)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Recevied non-matching outstation address - Expected " + std::to_string(ExpectedStationAddress) + " Got - " + std::to_string(StationAddress));
		return;
	}
	//TODO: Replace with a call to the same method on the correct instance of this class.
	// We will have a static list of instances and their address to look up.
	this->ProcessMD3Message(CompleteMD3Message);
}

void MD3OutstationPort::ProcessMD3Message(std::vector<MD3Block> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	uint8_t ExpectedStationAddress = MyConf()->mAddrConf.OutstationAddr;

	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.
	// All are included to allow better error reporting.
	switch (CompleteMD3Message[0].GetFunctionCode())
	{
	case ANALOG_UNCONDITIONAL:	// Command and reply
		DoAnalogUnconditional(CompleteMD3Message);
		break;
	case ANALOG_DELTA_SCAN:		// Command and reply
		break;
	case DIGITAL_UNCONDITIONAL_OBS:
		break;
	case DIGITAL_DELTA_SCAN:
		break;
	case HRER_LIST_SCAN:
		break;
	case DIGITAL_CHANGE_OF_STATE:
		break;
	case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
		break;
	case DIGITAL_UNCONDITIONAL:
		DoDigitalUnconditional(CompleteMD3Message);
		break;
	case ANALOG_NO_CHANGE_REPLY:
		break;
	case DIGITAL_NO_CHANGE_REPLY:
		break;
	case CONTROL_REQUEST_OK:
		break;
	case FREEZE_AND_RESET:
		break;
	case POM_TYPE_CONTROL :
		break;
	case DOM_TYPE_CONTROL:
		break;
	case INPUT_POINT_CONTROL:
		break;
	case RAISE_LOWER_TYPE_CONTROL :
		break;
	case AOM_TYPE_CONTROL :
		break;
	case CONTROL_OR_SCAN_REQUEST_REJECTED:
		break;
	case COUNTER_SCAN :
		break;
	case SYSTEM_SIGNON_CONTROL:
		break;
	case SYSTEM_SIGNOFF_CONTROL:
		break;
	case SYSTEM_RESTART_CONTROL:
		break;
	case SYSTEM_SET_DATETIME_CONTROL:
		break;
	case FILE_DOWNLOAD:
		break;
	case FILE_UPLOAD :
		break;
	case SYSTEM_FLAG_SCAN:
		break;
	case LOW_RES_EVENTS_LIST_SCAN :
		break;
	default:
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Unknown Command Function - " + std::to_string(CompleteMD3Message[0].GetFunctionCode()) + " On Station Address - " + std::to_string(CompleteMD3Message[0].GetStationAddress()));
		break;
	}
}
// Function 5
void MD3OutstationPort::DoAnalogUnconditional(std::vector<MD3Block> &CompleteMD3Message)
{
	// This has only one response
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, (MD3_FUNCTION_CODE)CompleteMD3Message[0].GetFunctionCode(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	int NumberOfDataBlocks = CompleteMD3Message[0].GetChannels() / 2 + CompleteMD3Message[0].GetChannels() % 2;	// 2 --> 1, 3 -->2

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i+1 == NumberOfDataBlocks);
		uint16_t Res1 = 0x8000;
		uint16_t Res2 = 0x8000;

		// Could check the result, but the value is not changed if not found, so not necessary.
		GetAnalogValueUsingMD3Index(CompleteMD3Message[0].GetModuleAddress(), 2 * i, Res1);

		//TODO :Should just be the next entry, we could increment the iterator and check - would be quicker
		GetAnalogValueUsingMD3Index(CompleteMD3Message[0].GetModuleAddress(), 2 * i + 1, Res2);

		auto block = MD3Block(Res1, Res2, lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendResponse(ResponseMD3Message);
}
// Function 7
void MD3OutstationPort::DoDigitalUnconditional(std::vector<MD3Block> &CompleteMD3Message)
{
	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, (MD3_FUNCTION_CODE)CompleteMD3Message[0].GetFunctionCode(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	int NumberOfDataBlocks = CompleteMD3Message[0].GetChannels(); // Actually the number of modules - 0 numbered, does not make sense to ask for none...

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		// Have to collect all the bits into a uint16_t
		uint16_t wordres = 0;
		bool missingdata = false;

		for (int j = 0; j < 16; j++)
		{
			uint8_t bitres = 0;

			if ( !GetBinaryValueUsingMD3Index(CompleteMD3Message[0].GetModuleAddress()+i, j, bitres))
			{
				// Data is missing, need to send the error block for this module address.
				missingdata = true;
			}
			else
			{
				// TODO: Check the bit order here of the binaries
				wordres |= (uint16_t)bitres << (15 - j);
			}
		}
		if (missingdata)
		{
			// Queue the error block
			uint8_t errorflags = 0;		// Application dependent, depends on the outstation implementation/master expectations.
			uint16_t lowword = (uint16_t)errorflags << 8 | (CompleteMD3Message[0].GetModuleAddress() + i);
			auto block = MD3Block((uint16_t)CompleteMD3Message[0].GetStationAddress() << 8, lowword, lastblock);	// Read the MD3 Data Structure - Module and Channel - dummy at the moment...
			ResponseMD3Message.push_back(block);
		}
		else
		{
			// Queue the data block
			uint16_t address = (uint16_t)CompleteMD3Message[0].GetStationAddress() << 8 | (CompleteMD3Message[0].GetModuleAddress() + i);
			auto block = MD3Block(address, wordres, lastblock);
			ResponseMD3Message.push_back(block);
		}
	}
	SendResponse(ResponseMD3Message);
}


#pragma region  PointTableAccess
MD3PortConf* MD3OutstationPort::MyConf()
{
	return static_cast<MD3PortConf*>(this->pConf.get());
};
std::shared_ptr<MD3PointConf> MD3OutstationPort::MyPointConf()
{
	return MyConf()->pPointConf;
};

bool MD3OutstationPort::GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3PointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3PointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		MD3PointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetAnalogValueUsingODCIndex(const uint16_t index, uint16_t &res)
{
	ODCPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetAnalogValueUsingODCIndex(const uint16_t index, const uint16_t meas)
{
	ODCPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3PointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Binary;
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3PointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		MD3PointMapIter->second->Binary = meas;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetBinaryValueUsingODCIndex(const uint16_t index, uint8_t &res)
{
	ODCPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->Binary;
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetBinaryValueUsingODCIndex(const uint16_t index, const uint8_t meas)
{
	ODCPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->Binary = meas;
		return true;
	}
	return false;
}
#pragma endregion



#pragma region OpenDataConInteraction

//Similar to the command below, but this one is just asking if something is supported.
//At the moment, I assume we respond based on how we are configured (controls and data points) and dont wait to see what happens down the line.
template<typename T>
inline CommandStatus MD3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	//FIXME: this is meant to return if we support the type of command
	//at the moment we just return success if it's configured as a control
	/*
		auto pConf = static_cast<MD3PortConf*>(this->pConf.get());
		if(std::is_same<T,ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
		{
						for(auto index : pConf->MyPointConf->ControlIndicies)
								if(index == aIndex)
										return CommandStatus::SUCCESS;
		}
	*/
	return CommandStatus::NOT_SUPPORTED;
}

// We are going to send a command to the opendatacon connector to do some kind of operation.
// If there is a master on that connector it will then send the command on down to the "real" outstation.
// This method will be called in response to data appearing on our TCP connection.
// TODO: SJE The question is, how do we respond up the line - do we need to wait for a response from down the line first?
template<typename T>
inline CommandStatus MD3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	auto future_results = PublishCommand(arCommand, aIndex);

	for (auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while (future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if (future_result.get() != CommandStatus::SUCCESS)
			return CommandStatus::UNDEFINED;
	}

	return CommandStatus::SUCCESS;
}

std::future<CommandStatus> MD3OutstationPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }

// We received a change in data from an Event (from the opendatacon Connector) now store it so that it can be produced when the Scada master polls us
// for a group or iindividually on our TCP connection.
// What we return here is not used in anyway that I can see.
template<typename T>
inline std::future<CommandStatus> MD3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	if (!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	if (std::is_same<T, const Analog>::value)
	{
		if ( !SetAnalogValueUsingODCIndex(index, (uint16_t)meas.value) )
		{
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Tried to set the value for an invalid analog point index " + std::to_string(index));
			return IOHandler::CommandFutureUndefined();
		}
	}
	else if (std::is_same<T, const Binary>::value)
	{
		if (!SetBinaryValueUsingODCIndex(index, (uint8_t)meas.value))
		{
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Tried to set the value for an invalid binary point index " + std::to_string(index));
			return IOHandler::CommandFutureUndefined();
		}
	}
	else //TODO: MD3OutstationPort impl other types
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Type is not implemented " + std::to_string(index));
		return IOHandler::CommandFutureUndefined();
	}

	// As we are taking the events and storing them, we can return now with sucess or failure. The Master has to wait for responses and will be different.
	return IOHandler::CommandFutureSuccess();
}

#pragma endregion
