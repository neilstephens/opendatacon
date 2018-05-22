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
* MD3OutStationPort.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
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
#include <opendnp3/LogLevels.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3OutstationPort.h"


//TODO: Check out http://www.pantheios.org/ logging library..


MD3OutstationPort::MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	MD3Port(aName, aConfFilename, aConfOverrides)
{
	// Dont load conf here, do it in MD3Port
}

MD3OutstationPort::~MD3OutstationPort()
{
	Disable();
	// Remove any this pointers so they cant be accessed!
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
void MD3OutstationPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	if (pConnection.get() == nullptr)
		return;
	pConnection->Close(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
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
	//TODO: Do we re-read the conf file - so we can do a live reload? - How do we kill all the sockets and connections properly?


	std::string ChannelID = MyConf()->mAddrConf.IP + ":" + std::to_string(MyConf()->mAddrConf.Port);

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, isServer, MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms));	// Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection);	//Static method
	}

	pConnection->AddOutStation(MyConf()->mAddrConf.OutstationAddr,
		std::bind(&MD3OutstationPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3OutstationPort::SocketStateHandler, this, std::placeholders::_1) );
}


// The only method that sends to the TCP Socket
void MD3OutstationPort::SendResponse(std::vector<MD3BlockData> &CompleteMD3Message)
{
	if (CompleteMD3Message.size() == 0)
	{
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", "Tried to send an empty response");
		return;
	}

	// Turn the blocks into a binary string.
	std::string MD3Message;
	for (auto blk : CompleteMD3Message)
	{
		MD3Message += blk.ToBinaryString();
	}

	// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
	// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function due to differences between a method and a lambda
	if (SendTCPDataFn != nullptr)
		SendTCPDataFn(MD3Message);
	else
	{
		pConnection->Write(std::string(MD3Message));
	}
}

// Test only method for simulating input from the TCP Connection.
void MD3OutstationPort::InjectCommand(buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	pConnection->ReadCompletionHandler(readbuf);
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

bool MD3OutstationPort::GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta)
{
	// Change being update the last read value
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		delta = (int)res - (int)MD3PointMapIter->second->LastReadAnalog;
		MD3PointMapIter->second->LastReadAnalog = res;	// We assume it is sent to the master. May need better way to do this
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetCounterValueUsingODCIndex(const uint16_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->CounterODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->CounterODCPointMap.end())
	{
		ODCPointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}

bool MD3OutstationPort::GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		delta = (int)res - (int)MD3PointMapIter->second->LastReadAnalog;
		MD3PointMapIter->second->LastReadAnalog = res;	// We assume it is sent to the master. May need better way to do this
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		MD3PointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetAnalogValueUsingODCIndex(const uint16_t index, uint16_t &res)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetAnalogValueUsingODCIndex(const uint16_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}
// Gets and Clears changed flag
bool MD3OutstationPort::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Binary;
		if (MD3PointMapIter->second->Changed)
		{
			changed = true;
			MD3PointMapIter->second->Changed = false;
		}
		return true;
	}
	return false;
}
// Only gets value, does not clear changed flag
bool MD3OutstationPort::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Binary;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		if (MD3PointMapIter->second->Changed)
		{
			changed = MD3PointMapIter->second->Changed;
		}
		return true;
	}
	return false;
}

bool MD3OutstationPort::SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		MD3PointMapIter->second->Binary = meas;
		MD3PointMapIter->second->Changed = true;
		return true;
	}
	return false;
}
bool MD3OutstationPort::GetBinaryValueUsingODCIndex(const uint16_t index, uint8_t &res, bool &changed)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->Binary;
		if (ODCPointMapIter->second->Changed)
		{
			changed = true;
			ODCPointMapIter->second->Changed = false;
		}
		return true;
	}
	return false;
}
bool MD3OutstationPort::SetBinaryValueUsingODCIndex(const uint16_t index, const uint8_t meas, uint64_t eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->Binary = meas;
		ODCPointMapIter->second->Changed = true;

		// We now need to add the change to the separate digital/binary event list
		ODCPointMapIter->second->ChangedTime = eventtime;
		MD3BinaryPoint CopyOfPoint(*(ODCPointMapIter->second));
		AddToDigitalEvents(CopyOfPoint);
		return true;
	}
	return false;
}
bool MD3OutstationPort::CheckBinaryControlExistsUsingMD3Index(const uint16_t module, const uint8_t channel)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryControlMD3PointMap.find(Md3Index);
	return (MD3PointMapIter != MyPointConf()->BinaryControlMD3PointMap.end());
}

void MD3OutstationPort::AddToDigitalEvents(MD3BinaryPoint & pt)
{
	// Will fail if full, which is the defined MD3 behaviour. Push takes a copy
	MyPointConf()->BinaryTimeTaggedEventQueue.push(pt);

	// Have to collect all the bits in the module to which this point belongs into a uint16_t,
	// just to support COS Fn 11 where the whole 16 bits are returned for a possibly single bit change.
	// Do not effect the change flags which are needed for normal scanning
	bool ModuleFailed = false;
	uint16_t wordres = CollectModuleBitsIntoWord(pt.ModuleAddress, ModuleFailed);

	// Save it in the snapshot that is used for the Fn11 COS time tagged events.
	pt.ModuleBinarySnapShot = wordres;
	MyPointConf()->BinaryModuleTimeTaggedEventQueue.push(pt);
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
	else if (std::is_same<T, const Counter>::value)
	{
		if (!SetCounterValueUsingODCIndex(index, (uint16_t)meas.value))
		{
			LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Tried to set the value for an invalid counter point index " + std::to_string(index));
			return IOHandler::CommandFutureUndefined();
		}
	}
	else if (std::is_same<T, const Binary>::value)
	{
		// MD3 only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1 - Same as for MD3
		uint64_t eventtime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;

		if (!SetBinaryValueUsingODCIndex(index, (uint8_t)meas.value, eventtime))
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
