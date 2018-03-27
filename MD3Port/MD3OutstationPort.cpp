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
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	//TODO: We need a static list of sockets, so that if the socket already exists, we dont try and create it again.
	// The socket id will be ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
	// We also need a list of Outstations on this socket, so that we can call the appropriate

	//TODO: use event buffer size once socket manager supports it
	pSockMan.reset(new TCPSocketManager<std::string>
		(pIOS, isServer, pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port),
			std::bind(&MD3OutstationPort::ReadCompletionHandler, this, std::placeholders::_1),
			std::bind(&MD3OutstationPort::SocketStateHandler, this, std::placeholders::_1),
			true,
			pConf->TCPConnectRetryPeriodms));

	//Allocate memory for bits, input bits, registers, and input registers */
	// Do this in a read config file kind of way - adding more than one station to the tcp port

	/*	mb_mapping = MD3_mapping_new(pConf->pPointConf->BitIndicies.Total(),
	pConf->pPointConf->InputBitIndicies.Total(),
	pConf->pPointConf->RegIndicies.Total(),
	pConf->pPointConf->InputRegIndicies.Total());
	if (mb_mapping == NULL)
	{
	LOG("MD3OutstationPort", openpal::logflags::ERR, "", Name + ": Failed to allocate the MD3 register mapping: "); // +std::string(MD3_strerror(errno));
	//TODO: should this throw an exception instead of return?
	return;
	}*/
}

void MD3OutstationPort::ReadCompletionHandler(buf_t& readbuf)
{
	// We are currently assuming a whole complete packet will turn up in one unit. If not it will be difficult to do the packet decoding and multidrop routing.
	// MD3 only has addressing information in the first block of the packet.

	// We should have a multiple of 5 bytes + one padding byte.
	// We need to know enough about the packets to work out the first and last, and the station address, so we can pass them to the correct station.

	while (readbuf.size() >= 5)
	{
		MD3BlockArray d;
		for (int i = 0; i < 5; i++)
		{
			d[i] = readbuf.sgetc();
			readbuf.consume(1);
		}

		auto md3block = MD3Block(d);	// Supposed to be a 5 byte array..

		if (md3block.CheckSumPasses())
		{
			if (md3block.IsFormattedBlock())
			{
				// This only occurs for the first block. So if we are not expecting it we need to clear out the Message Block Vector
				// We know we are looking for the first block if MD3Message is empty.
				if (MD3Message.size() != 0)
				{
					LOG("DNP3OutstationPort", openpal::logflags::WARN, "", "Received a start block when we have not got to an end block - discarding data blocks - " + MD3Message.size());
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
				MD3Message.clear();
			}
		}
		else
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Checksum failure on received MD3 block - " + md3block.ToString());
//		for (auto b : d)			std::cout << b << " ";
	}

	// Check for and consume last byte 00 padding.
	if (readbuf.size() == 1)
	{
		readbuf.consume(1);
	}
}

void MD3OutstationPort::RouteMD3Message(std::vector<MD3Block> &CompleteMD3Message)
{
	// Only passing in the variable to make unit testing simpler.
	// We have a full set of MD3 message blocks from a minimum of 1.
	assert(CompleteMD3Message.size() != 0);

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	uint8_t ExpectedStationAddress = (*pConf).mAddrConf.OutstationAddr;
	uint8_t StationAddress = CompleteMD3Message[0].GetStationAddress();

	// Change this to route to the correct outstation in future.
	if (ExpectedStationAddress != StationAddress)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Recevied non-matching outstation address - Expected " + std::to_string(ExpectedStationAddress) + " Got - " + std::to_string(StationAddress));
		return;
	}
	// Replace with a call to the same method on the correct instance of this class.
	// We will have a static list of instances and their address to look up.
	this->ProcessMD3Message(CompleteMD3Message);
}

void MD3OutstationPort::ProcessMD3Message(std::vector<MD3Block> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	uint8_t ExpectedStationAddress = (*pConf).mAddrConf.OutstationAddr;

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
void MD3OutstationPort::DoAnalogUnconditional(std::vector<MD3Block> &CompleteMD3Message)
{
	std::vector<MD3Block> ResponseMD3Message;
	std::map<uint16_t, std::shared_ptr<MD3Point>>::iterator MD3PointMapIter;
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, (MD3_FUNCTION_CODE)CompleteMD3Message[0].GetFunctionCode(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	int NumberOfDataBlocks = CompleteMD3Message[0].GetChannels() / 2 + CompleteMD3Message[0].GetChannels() % 2;	// 2 --> 1, 3 -->2

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i+1 == NumberOfDataBlocks);
		uint16_t Md3Index1 = (CompleteMD3Message[0].GetModuleAddress() << 8) | 2*i;
		uint16_t Md3Index2 = (CompleteMD3Message[0].GetModuleAddress() << 8) | 2*i+1;
		uint16_t Res1 = 0x8000;
		uint16_t Res2 = 0x8000;

		MD3PointMapIter = pConf->pPointConf->AnalogMD3PointMap.find(Md3Index1);
		if (MD3PointMapIter != pConf->pPointConf->AnalogMD3PointMap.end())
			Res1 = (uint16_t)MD3PointMapIter->second->Analog;

		//TODO :Should just be the next entry, we could increment the iterator and check - would be quicker
		MD3PointMapIter = pConf->pPointConf->AnalogMD3PointMap.find(Md3Index2);
		if (MD3PointMapIter != pConf->pPointConf->AnalogMD3PointMap.end())
			Res2 = (uint16_t)MD3PointMapIter->second->Analog;

		auto block = MD3Block(Res1, Res2, lastblock);	// Read the MD3 Data Structure - Module and Channel - dummy at the moment...
		ResponseMD3Message.push_back(block);
	}
	SendResponse(ResponseMD3Message);
}

void MD3OutstationPort::SendResponse(std::vector<MD3Block> &CompleteMD3Message)
{
	// Turn the blocks into a binary string.
	std::string MD3Message;
	for (auto blk : CompleteMD3Message)
	{
		MD3Message += blk.ToBinaryString();
	}
	MD3Message += (char)0x00;	// Padding byte that seems to be on every message on the TCP network

	// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
	// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function
	if (SendTCPDataFn != nullptr)
		SendTCPDataFn(MD3Message);
	else
		pSockMan->Write(std::string(MD3Message));
}



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
						for(auto index : pConf->pPointConf->ControlIndicies)
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

int find_index(const std::vector<uint32_t> &aCollection, uint16_t index)
{
	for (auto group : aCollection)
		if (group == index)
			return (int)index;
	return -1;
}

// We received a change in data from an Event (from the opendatacon Connector) now store it so that it can be produced when the Scada master polls us
// for a group or iindividually on our TCP connection.
// What we return here is not used in anyway that I can see.
template<typename T>
inline std::future<CommandStatus> MD3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	std::map<uint32_t, std::shared_ptr<MD3Point>>::iterator ODCPointMapIter;

	if (!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	if (std::is_same<T, const Analog>::value)
	{
		ODCPointMapIter = pConf->pPointConf->AnalogODCPointMap.find(index);
		if (ODCPointMapIter != pConf->pPointConf->AnalogODCPointMap.end())
			ODCPointMapIter->second->Analog = (uint16_t)meas.value;
		else
		{
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Tried to set the value for an invalid analog point index " + std::to_string(index));
			return IOHandler::CommandFutureUndefined();
		}
	}
	else if (std::is_same<T, const Binary>::value)
	{
		ODCPointMapIter = pConf->pPointConf->BinaryODCPointMap.find(index);
		if (ODCPointMapIter != pConf->pPointConf->BinaryODCPointMap.end())
			ODCPointMapIter->second->Binary = (uint8_t)meas.value;
		else
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

