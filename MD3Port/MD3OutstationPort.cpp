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
//#include <opendnp3/outstation/IOutstationApplication.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3OutstationPort.h"


//TODO: Check out http://www.pantheios.org/ logging library..


MD3OutstationPort::MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	MD3Port(aName, aConfFilename, aConfOverrides)
{
	// Don't load conf here, do it in MD3Port
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
			throw std::runtime_error("Connection manager uninitialised");

		pConnection->Open(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection : " + Name + " : " + e.what());
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
	LOGINFO(msg);
}

void MD3OutstationPort::BuildOrRebuild()
{
	//TODO: Do we re-read the conf file - so we can do a live reload? - How do we kill all the sockets and connections properly?
	std::string ChannelID = MyConf()->mAddrConf.ChannelID();

	pBinaryTimeTaggedEventQueue.reset(new StrandProtectedQueue<MD3BinaryPoint>(*pIOS, 256));
	pBinaryModuleTimeTaggedEventQueue.reset(new StrandProtectedQueue<MD3BinaryPoint>(*pIOS, 256));

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, IsServer(), MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms)); // Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection); //Static method
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

#pragma region PerformEvents

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
template<typename T>
CommandStatus MD3OutstationPort::PerformT(T& arCommand, uint16_t aIndex, bool waitforresult)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	// This function (in IOHandler) goes through the list of subscribed events and calls them.
	// Our callback will be called only once - either after all have been called, or after one has failed.

	// The ODC calls will ALWAYS return (at some point) with success or failure. Time outs are done by the ports on the TCP communications that they do.
	if (!waitforresult)
	{
		PublishEvent(arCommand, aIndex); // Could have a callback, but we are not waiting, so don't give it one!
		return CommandStatus::SUCCESS;
	}

	//TODO: enquire about the possibility of the opendnp3 API having a callback for the result that would avoid the below polling loop
	std::atomic_bool cb_executed;
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void(CommandStatus status)>>([&](CommandStatus status)
		{
			cb_status = status;
			cb_executed = true;
		});
	PublishEvent(arCommand, aIndex, StatusCallback);
	while (!cb_executed)
	{
		//This loop pegs a core and blocks the outstation strand,
		//	but there's no other way to wait for the result.
		//	We can maybe do some work while we wait.
		pIOS->poll_one();
	}
	return cb_status;
}

#pragma endregion PerformEvents

#pragma region QualityEvents

void MD3OutstationPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventQ(qual, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventQ(qual, index, SenderName, pStatusCallback); }

template<typename T>
inline void MD3OutstationPort::EventQ(T& qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	if (std::is_same<T, const AnalogQuality>::value)
	{
		if ((uint8_t)qual != static_cast<uint8_t>(AnalogQuality::ONLINE))
		{
			if (!SetAnalogValueUsingODCIndex(index, (uint16_t)0x8000))
			{
				LOGERROR("Tried to set the failure value for an invalid analog point index " + std::to_string(index));
				(*pStatusCallback)(CommandStatus::UNDEFINED);
				return;
			}
		}
	}
	else if (std::is_same<T, const CounterQuality>::value)
	{
		if ((uint8_t)qual != static_cast<uint8_t>(CounterQuality::ONLINE))
		{
			if (!SetCounterValueUsingODCIndex(index, (uint16_t)0x8000))
			{
				LOGERROR("Tried to set the failure value for an invalid counter point index " + std::to_string(index));
				(*pStatusCallback)(CommandStatus::UNDEFINED);
				return;
			}
		}
	}
	else // No other quality types make sense at this stage.
	{
		LOGERROR("Type is not implemented " + std::to_string(index));
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	// As we are taking the events and storing them, we can return now with success or failure. The Master has to wait for responses and will be different.
	(*pStatusCallback)(CommandStatus::SUCCESS);
}
#pragma endregion QualityEvents

#pragma region DataEvents

void MD3OutstationPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }
void MD3OutstationPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { return EventT(meas, index, SenderName, pStatusCallback); }


// We received a change in data from an Event (from the opendatacon Connector) now store it so that it can be produced when the Scada master polls us
// for a group or individually on our TCP connection.
// What we return here is not used in anyway that I can see.
template<typename T>
inline void MD3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	if (std::is_same<T, const Analog>::value)
	{
		if ( !SetAnalogValueUsingODCIndex(index, (uint16_t)meas.value) )
		{
			LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));
			(*pStatusCallback)(CommandStatus::UNDEFINED);
			return;
		}
	}
	else if (std::is_same<T, const Counter>::value)
	{
		if (!SetCounterValueUsingODCIndex(index, (uint16_t)meas.value))
		{
			LOGERROR("Tried to set the value for an invalid counter point index " + std::to_string(index));
			(*pStatusCallback)(CommandStatus::UNDEFINED);
			return;
		}
	}
	else if (std::is_same<T, const Binary>::value)
	{
		// MD3 only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1 - Same as for MD3
		MD3Time eventtime = MD3Now();

		//TODO: if we are passed a time, check if within range and if OK use it.

		if (!SetBinaryValueUsingODCIndex(index, (uint8_t)meas.value, eventtime)) //TODO: Use meas.time
		{
			LOGERROR("Tried to set the value for an invalid binary point index " + std::to_string(index));
			(*pStatusCallback)(CommandStatus::UNDEFINED);
			return;
		}
	}
	else //TODO: MD3OutstationPort impl other types
	{
		LOGERROR("Type is not implemented " + std::to_string(index));
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	// As we are taking the events and storing them, we can return now with success or failure. The Master has to wait for responses and will be different.
	(*pStatusCallback)(CommandStatus::SUCCESS);
}

void MD3OutstationPort::ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	//something upstream has connected
	if (state == ConnectState::CONNECTED)
	{
		LOGDEBUG("Upstream (other side of ODC) port enabled - So a Master will send us events - and we can send what we have over ODC ");
		// We don’t know the state of the upstream data, so send event information for all points.
		// SendAllPointEvents();
	}
	else // ConnectState::DISCONNECTED
	{
		// If we were an on demand connection, we would take down the connection . For MD3 we are using persistent connections only.
		// We have lost an ODC connection, so events we send don't go anywhere.

	}

	(*pStatusCallback)(CommandStatus::SUCCESS);
}
#pragma endregion

#pragma endregion OpenDataConInteraction