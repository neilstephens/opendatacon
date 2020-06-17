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
* CBOutStationPort.h
*
*  Created on: 12/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

/* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
 This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/CB) will turn back into scada commands and send out to the "real" Outstation.
 So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
 So an Event to an outstation will be data that needs to be sent up to the scada master.
 An event from an outstation will be a master control signal to turn something on or off.
*/
#include "CB.h"
#include "CBOutstationPort.h"
#include "CBOutStationPortCollection.h"
#include "CBUtility.h"
#include <chrono>
#include <future>
#include <iostream>
#include <regex>
#include <random>


CBOutstationPort::CBOutstationPort(const std::string & aName, const std::string &aConfFilename, const Json::Value & aConfOverrides):
	CBPort(aName, aConfFilename, aConfOverrides),
	CBOutstationCollection(nullptr)
{
	UpdateOutstationPortCollection();

	// Don't load conf here, do it in CBPort
	std::string over = "None";
	if (aConfOverrides.isObject()) over = aConfOverrides.toStyledString();

	//TODO: Do we need these flags for Conitel/Baker
	SystemFlags.SetDigitalChangedFlagCalculationMethod(std::bind(&CBOutstationPort::DigitalChangedFlagCalculationMethod, this));
	SystemFlags.SetTimeTaggedDataAvailableFlagCalculationMethod(std::bind(&CBOutstationPort::TimeTaggedDataAvailableFlagCalculationMethod, this));

	IsOutStation = true;

	LOGDEBUG("CBOutstation Constructor - {} ", Name);
}

void CBOutstationPort::UpdateOutstationPortCollection()
{
	// These variables are effectively class static variables (i.e. only one ever exists.
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<CBOutstationPortCollection> weak_collection;

	//if we're the first/only one on the scene, init the CBOutstationPortCollection (a special version of an unordered_map)
	if (!init_flag.test_and_set(std::memory_order_acquire))
	{
		// Make a custom deleter for the PortCollection that will also clear the init flag
		// This will be called when the shared_ptr destructs (last ref gone)
		auto deinit_del = [](CBOutstationPortCollection* collection_ptr)
					{
						init_flag.clear();
						delete collection_ptr;
					};
		// Save a pointer to the collection in this object
		this->CBOutstationCollection = std::shared_ptr<CBOutstationPortCollection>(new CBOutstationPortCollection(), deinit_del);
		// Save a global weak pointer to our PortCollection (shared_ptr)
		weak_collection = this->CBOutstationCollection;
	}
	else
	{
		// PortCollection has already been created, so get a shared pointer to it.
		// The last shared_ptr to get destructed will control its destruction. The weak_ptr will just no longer return a pointer.
		while (!(this->CBOutstationCollection = weak_collection.lock()))
		{} //init happens very seldom, so spin lock is good
	}
}

CBOutstationPort::~CBOutstationPort()
{
	Disable();
	CBConnection::RemoveOutstation(pConnection,MyConf->mAddrConf.OutstationAddr);
}

void CBOutstationPort::Enable()
{
	if (enabled.exchange(true)) return;
	try
	{
		CBConnection::Open(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
	}
	catch (std::exception& e)
	{
		LOGERROR("{} Problem opening connection : {}", Name, e.what());
		enabled = false;
		return;
	}
}
void CBOutstationPort::Disable()
{
	if (!enabled.exchange(false)) return;

	CBConnection::Close(pConnection); // The port will only close when all ports disable it
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void CBOutstationPort::SocketStateHandler(bool state)
{
	if (!enabled.load()) return; // Port Disabled so dont process
	std::string msg;
	if (state)
	{
		PublishEvent(ConnectState::CONNECTED);
		msg = Name + ": pConnection established.";
	}
	else
	{
		PublishEvent(ConnectState::DISCONNECTED);
		msg = Name + ": pConnection closed.";
	}
	LOGINFO(msg);
}

void CBOutstationPort::Build()
{
	// Add this port to the list of ports we can command.
	auto shared_this = std::static_pointer_cast<CBOutstationPort>(shared_from_this());
	this->CBOutstationCollection->Add(shared_this, this->Name);

	// Need a couple of things passed to the point table.
	MyPointConf->PointTable.Build(Name, IsOutStation, *pIOS, MyPointConf->SOEQueueSize, SOEBufferOverflowFlag);

	// Creates internally if necessary
	pConnection = CBConnection::AddConnection(pIOS, IsServer(), MyConf->mAddrConf.IP, MyConf->mAddrConf.Port, MyPointConf->IsBakerDevice, MyConf->mAddrConf.TCPConnectRetryPeriodms); //Static method


	std::function<void(CBMessage_t &CBMessage)> aReadCallback = std::bind(&CBOutstationPort::ProcessCBMessage, this, std::placeholders::_1);
	std::function<void(bool)> aStateCallback = std::bind(&CBOutstationPort::SocketStateHandler, this, std::placeholders::_1);

	CBConnection::AddOutstation(pConnection,MyConf->mAddrConf.OutstationAddr, aReadCallback, aStateCallback, MyPointConf->IsBakerDevice);
}

void CBOutstationPort::SendCBMessage(const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("{} - Tried to send an empty message to the TCP Port",Name);
		return;
	}
	if (BitFlipProbability != 0.0)
	{
		auto msg = CorruptCBMessage(CompleteCBMessage);
		LOGDEBUG("{} - Sending Corrupted Message - Correct {}, Corrupted {}", Name, CBMessageAsString(CompleteCBMessage), CBMessageAsString(msg));
		CBPort::SendCBMessage(msg);
	}
	else
	{
		LOGDEBUG("{} - Sending Message - {}", Name, CBMessageAsString(CompleteCBMessage));

		// Done this way just to get context into log messages.
		CBPort::SendCBMessage(CompleteCBMessage);
	}
	LastSentCBMessage = CompleteCBMessage; // Take a copy of last message
}
CBMessage_t CBOutstationPort::CorruptCBMessage(const CBMessage_t& CompleteCBMessage)
{
	if (BitFlipProbability != 0.0)
	{
		// Setup a random generator
		std::random_device rd;
		std::mt19937 e2(rd());
		std::uniform_real_distribution<> dist(0, 1);

		if (dist(e2) > BitFlipProbability)
		{
			CBMessage_t ResMsg = CompleteCBMessage;
			size_t messagelen = CompleteCBMessage.size();
			std::uniform_real_distribution<> bitdist(0, messagelen * 32 - 1);
			int bitnum = round(bitdist(e2));
			ResMsg[bitnum / 32].XORBit(bitnum % 32);
			return ResMsg;
		}
	}
	return CompleteCBMessage;
}
#ifdef _MCS_VER
#pragma region OpenDataConInteraction

#pragma region PerformEvents
#endif
// We are going to send a command to the opendatacon connector to do some kind of operation.
// If there is a master on that connector it will then send the command on down to the "real" outstation.
// This method will be called in response to data appearing on our TCP connection.
// Remember there can be multiple responders!
//

CommandStatus CBOutstationPort::Perform(const std::shared_ptr<EventInfo>& event, bool waitforresult)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	// The ODC calls will ALWAYS return (at some point) with success or failure. Time outs are done by the ports on the TCP communications that they do.
	if (!waitforresult)
	{
		PublishEvent(event); // Could have a callback, but we are not waiting, so don't give it one!
		return CommandStatus::SUCCESS;
	}

	//NEIL: enquire about the possibility of the opendnp3 API having a callback for the result that would avoid the below polling loop
	std::atomic_bool cb_executed(false);
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void(CommandStatus status)>>([&](CommandStatus status)
		{
			cb_status = status;
			cb_executed = true;
		});

	PublishEvent(event, StatusCallback);

	while (!cb_executed)
	{
		//This loop pegs a core and blocks the outstation strand,
		//	but there's no other way to wait for the result.
		//	We can maybe do some work while we wait.
		pIOS->poll_one();
	}
	return cb_status;
}
#ifdef _MSC_VER
#pragma endregion PerformEvents

#pragma region DataEvents
#endif
// We received a change in data from an Event (from the opendatacon Connector) now store it so that it can be produced when the Scada master polls us
// for a group or individually on our TCP connection.
// What we return here is not used in anyway that I can see.
void CBOutstationPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		return (*pStatusCallback)(CommandStatus::UNDEFINED);
	}

	size_t ODCIndex = event->GetIndex();

	switch (event->GetEventType())
	{
		case EventType::Analog:
		{
			// ODC Analog is a double by default...
			auto analogmeas = static_cast<uint16_t>(event->GetPayload<EventType::Analog>());

			LOGTRACE("{} - Received Event - Analog - Index {}  Value 0x{}",Name, ODCIndex, to_hexstring(analogmeas));
			if (!MyPointConf->PointTable.SetAnalogValueUsingODCIndex(ODCIndex, analogmeas))
			{
				LOGERROR("Tried to set the value for an invalid analog point index {}, {}", SenderName,ODCIndex);
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::Counter:
		{
			auto countermeas = numeric_cast<uint16_t>(event->GetPayload<EventType::Counter>());

			LOGDEBUG("{} - Received Event - Counter - Index {}  Value 0x{}",Name, ODCIndex, to_hexstring(countermeas));
			if (!MyPointConf->PointTable.SetCounterValueUsingODCIndex(ODCIndex, countermeas))
			{
				LOGERROR("Tried to set the value for an invalid counter point index {} {}", SenderName,ODCIndex);
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::Binary:
		{
			CBTime now = CBNowUTC(); // msec since epoch.
			CBTime eventtime = event->GetTimestamp();
			uint8_t meas = event->GetPayload<EventType::Binary>();

			LOGDEBUG("{} - Received Event - Binary - Index {}, Bit Value {}",Name,ODCIndex,meas);

			// Check that the passed time is within 30 minutes of the actual time, if not use the current time
			if (MyPointConf->OverrideOldTimeStamps)
			{
				if (abs(static_cast<int64_t>(now / 1000) - static_cast<int64_t>(eventtime / 1000)) < 60 * 30)
				{
					eventtime = now; // msec since epoch.
					LOGDEBUG("{} Binary time tag value is too far from current time (>30min) changing to current time. Point index {}",Name, ODCIndex);
				}
			}

			if (!MyPointConf->PointTable.SetBinaryValueUsingODCIndex(ODCIndex, meas, eventtime))
			{
				LOGERROR("{} Tried to set the value for an invalid binary point index {} {}",Name, SenderName,ODCIndex);
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();

			// This will be fired by (typically) an CBOutMaster port on the "other" side of the ODC Event bus. i.e. something downstream has connected
			// We should probably send any Digital or Analog outputs, but we can't send POM (Pulse)

			if (state == ConnectState::CONNECTED)
			{
				LOGDEBUG("{} Upstream (other side of ODC) port enabled - So a Master will send us events - and we can send what we have over ODC ",Name);
				// We dont know the state of the upstream data, so send event information for all points.

			}
			else if (state == ConnectState::DISCONNECTED)
			{
				// If we were an on demand connection, we would take down the connection . For CB we are using persistent connections only.
				// We have lost an ODC connection, so events we send don't go anywhere.
			}

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::AnalogQuality:
		{
			if ((event->GetQuality() != QualityFlags::ONLINE))
			{
				if (!MyPointConf->PointTable.SetAnalogValueUsingODCIndex(ODCIndex, static_cast<uint16_t>(MISSINGVALUE)))
				{
					LOGERROR("{} Tried to set the failure value for an invalid analog point index {} {}", Name, SenderName,ODCIndex);
					return (*pStatusCallback)(CommandStatus::UNDEFINED);
				}
			}
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::CounterQuality:
		{
			if ((event->GetQuality() != QualityFlags::ONLINE))
			{
				if (!MyPointConf->PointTable.SetCounterValueUsingODCIndex(ODCIndex, static_cast<uint16_t>(MISSINGVALUE)))
				{
					LOGERROR("{} Tried to set the failure value for an invalid counter point index {} {}", Name, SenderName,ODCIndex);
					return (*pStatusCallback)(CommandStatus::UNDEFINED);
				}
			}
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}
}
#ifdef _MSC_VER
#pragma endregion

#pragma endregion OpenDataConInteraction
#endif
