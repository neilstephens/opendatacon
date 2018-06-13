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
	pConnection->RemoveOutstation(MyConf()->mAddrConf.OutstationAddr);
	// The pConnection will be closed by this point, so is not holding any resources, and will be freed on program close when the static list is destroyed.
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
	std::string ChannelID = MyConf()->mAddrConf.ChannelID();

	pBinaryTimeTaggedEventQueue.reset(new StrandProtectedQueue<MD3BinaryPoint>(*pIOS, 256));
	pBinaryModuleTimeTaggedEventQueue.reset(new StrandProtectedQueue<MD3BinaryPoint>(*pIOS, 256));

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, IsServer(), MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms));	// Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection);	//Static method
	}

	pConnection->AddOutstation(MyConf()->mAddrConf.OutstationAddr,
		std::bind(&MD3OutstationPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3OutstationPort::SocketStateHandler, this, std::placeholders::_1) );
}


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

CommandStatus MD3OutstationPort::Perform(const AnalogOutputDouble64& arCommand, uint16_t index, bool waitforresult) { return PerformT(arCommand, index, waitforresult); }
CommandStatus MD3OutstationPort::Perform(const AnalogOutputInt32& arCommand, uint16_t index, bool waitforresult) { return PerformT(arCommand, index, waitforresult); }
CommandStatus MD3OutstationPort::Perform(const AnalogOutputInt16& arCommand, uint16_t index, bool waitforresult) { return PerformT(arCommand, index, waitforresult); }
CommandStatus MD3OutstationPort::Perform(const ControlRelayOutputBlock& arCommand, uint16_t index, bool waitforresult) { return PerformT(arCommand, index, waitforresult); }


// We are going to send a command to the opendatacon connector to do some kind of operation.
// If there is a master on that connector it will then send the command on down to the "real" outstation.
// This method will be called in response to data appearing on our TCP connection.
// Remember there can be multiple responders!
//
//TODO: This is the blocking code that Neil has talked about rewriting to use an async callback, so we don’t get stuck here.
//TODO: The destructor of a future will wait until it gets a result. So we CANNOT do a time out here, or exit on first failure - we wait anyway....
template<typename T>
CommandStatus MD3OutstationPort::PerformT(T& arCommand, uint16_t aIndex, bool waitforresult)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	//MD3Time ExpireTime = MD3Now() + (MD3Time)MyPointConf()->ODCCommandTimeoutmsec;

	// This function (in IOHandler) goes through the list of subscribed events and calls them, putting their returned future result into the vector that we get
	// back here. If the Event chooses to do everything synchronously then we get an answer straight away.
	// THE EVENT MUST TIME OUT AND COME BACK TO US, OTHERWISE WE COULD HANG HERE.
	// If the event chooses to POST the processing code to ASIO, then the future will be set when that finishes.
	// If we try and exit here - the destructor on the future will wait for it to be set before exiting, so we can't exit early..
	// So the event handlers are the ones that should always have a time out, so that a result is always returned so we cannot get stuck waiting for Event future results.
	auto future_results = PublishCommand(arCommand, aIndex);

	if (waitforresult)
	{
		// We loop through the future results of each "subscriber" to out command.
		// We end up waiting on the longest one
		for (auto& future_result : future_results)
		{
			//if results aren't ready, we'll try to do some work instead of blocking
			// We stay in this loop until we get an answer.
			while (future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
			{
			/*	if (ExpireTime < MD3Now())
				{
					// We have timed out...
					return CommandStatus::TIMEOUT;	//TODO: WILL NOT WORK
				}
*/
				//not ready - let's lend a hand to speed things up
				this->pIOS->poll_one();
			}

			//first one that isn't a success, we can return
			if (future_result.get() != CommandStatus::SUCCESS)
				return CommandStatus::UNDEFINED;	//TODO: Will wait for other futures to finish in the futures destructor.
		}
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
		MD3Time eventtime = MD3Now();

		//TODO: if we are passed a time, check if within range and if OK use it.

		if (!SetBinaryValueUsingODCIndex(index, (uint8_t)meas.value, eventtime))	//TODO: Use meas.time
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
