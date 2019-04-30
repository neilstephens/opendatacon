/*	opendatacon
*
*	Copyright (c) 2019:
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
* KafkaPort.cpp
*
*  Created on: 16/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include <iostream>
#include "KafkaPort.h"
#include "KafkaPortConf.h"
#include "StrandProtectedQueue.h"

KafkaPort::KafkaPort(const std::string &aName, const std::string & aConfFilename, const Json::Value & aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	//the creation of a new KafkaPortConf will get the point details
	pConf.reset(new KafkaPortConf(ConfFilename, ConfOverrides));

	// Just to save a lot of dereferencing..
	MyConf = static_cast<KafkaPortConf*>(this->pConf.get());
	MyPointConf = MyConf->pPointConf;

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

void KafkaPort::ProcessElements(const Json::Value& JSONRoot)
{
	// The points are handled by the KafkaPointConf class.
	// This is all the port specific stuff.

	if (!JSONRoot.isObject()) return;

	if (JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		LOGERROR("Warning: Kafka port serial device AND IP address specified - IP overrides");

	if (JSONRoot.isMember("IP"))
	{
		static_cast<KafkaPortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
	}

	if (JSONRoot.isMember("Port"))
		static_cast<KafkaPortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asString();
}

KafkaPort::~KafkaPort()
{
	Disable();
	//KafkaConnection::RemoveOutstation(pConnection, MyConf->mAddrConf.OutstationAddr);
}

void KafkaPort::Enable()
{
	if (enabled) return;
	try
	{
		//	KafkaConnection::Open(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection : {} : {}", Name, e.what());
		return;
	}
}
void KafkaPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	//KafkaConnection::Close(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void KafkaPort::SocketStateHandler(bool state)
{
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

void KafkaPort::Build()
{
	// Creates internally if necessary
	//pConnection = KafkaConnection::AddConnection(pIOS, IsServer(), MyConf->mAddrConf.IP, MyConf->mAddrConf.Port, MyPointConf->IsBakerDevice, MyConf->mAddrConf.TCPConnectRetryPeriodms); //Static method

	// The passed in method will be called whenever there are events in the queue.
	pKafkaEventQueue.reset(new StrandProtectedQueue<KafkaEvent>(*pIOS, 5000, std::bind(&KafkaPort::SendKafkaEvents, this)));
}

void KafkaPort::QueueKafkaEvent(const std::string &key, double measurement, QualityFlags quality)
{
	if (key.length() == 0)
	{
		LOGERROR("Tried to queue an empty key to Kafka");
		return;
	}
	std::string value = "{\"Value\" : " + std::to_string(measurement) + ", \"Quality\" : \"" + ToString(quality) + "\"}";
	KafkaEvent ev(key, value);
	pKafkaEventQueue->async_push(ev);

	LOGDEBUG("Queued Kafka Message - {}, {}", key, value);
	// Now actually send it to Kafka!!!
}

// Process everything in the queue, into a Kafka message, compress and send. The question is how we call this?
// Register it as a handler to be called every time new data is pushed into the queue?
// Use a flag in the push routine to handle if a call to this method has already been queued.

//TODO SendKafkaEvents
void KafkaPort::SendKafkaEvents()
{
	LOGDEBUG("SendKafkaEvents called");

	// Queue a lambda, that will be called with a vector of all the events currently in the queue. We process them and send them to the Kafka cluster

	auto pProcessCallback = std::make_shared<StrandProtectedQueue<KafkaEvent>::ProcessAllEventsCallbackFn>([&](std::vector<KafkaEvent> Events)
		{
			for (int i = 0; i < Events.size(); i++)
			{
			//Events[i]

			}
			LOGDEBUG("Kafka Message Send would happen now..");
		});

	pKafkaEventQueue->async_pop_all(pProcessCallback);
}

#ifdef _MSC_VER
#pragma region ODCEvents
#endif
// We received a change in data from an Event (from the opendatacon Connector). We need to then send it as a Kafka message

void KafkaPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
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
			double measurement = event->GetPayload<EventType::Analog>();
			QualityFlags quality = event->GetQuality();

			LOGDEBUG("OS - Received Event - Analog - Index {}  Value {}  Quality {}", std::to_string(ODCIndex), measurement, ToString(quality));
			std::string key;
			if (!MyPointConf->PointTable.GetAnalogKafkaKeyUsingODCIndex(ODCIndex, key))
			{
				LOGERROR("Tried to get the key for an invalid analog point index " + std::to_string(ODCIndex));
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			QueueKafkaEvent(key, measurement, quality);

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::Binary:
		{
			double measurement = event->GetPayload<EventType::Binary>();
			QualityFlags quality = event->GetQuality();

			LOGDEBUG("OS - Received Event - Binary - Index {}  Value {}  Quality {}", std::to_string(ODCIndex), measurement, ToString(quality));
			std::string key;
			if (!MyPointConf->PointTable.GetBinaryKafkaKeyUsingODCIndex(ODCIndex, key))
			{
				LOGERROR("Tried to get the key for an invalid abinary point index " + std::to_string(ODCIndex));
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			QueueKafkaEvent(key, measurement, quality);

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();

			// This will be fired by (typically) an KafkaOutMaster port on the "other" side of the ODC Event bus. i.e. something downstream has connected
			// We should probably send any Digital or Analog outputs, but we can't send POM (Pulse)

			if (state == ConnectState::CONNECTED)
			{
				LOGDEBUG("Upstream (other side of ODC) port enabled - So a Master will send us events - and we can send what we have over ODC ");
				// We dont know the state of the upstream data, so send event information for all points.

			}
			else if (state == ConnectState::DISCONNECTED)
			{
				// If we were an on demand connection, we would take down the connection . For Kafka we are using persistent connections only.
				// We have lost an ODC connection, so events we send don't go anywhere.
			}

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}

		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}
}
#ifdef _MSC_VER
#pragma endregion DataEvents
#endif

