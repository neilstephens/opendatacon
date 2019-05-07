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
	KafkaConnection.reset(); // Remove our ref count from the sheared ptr, so it is destructed.
}

void KafkaPort::Enable()
{
	if (enabled) return;
	try
	{
		//KafkaConnection->
		//	KafkaConnection::Open(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening Kafka connection : {} : {}", Name, e.what());
		return;
	}
}
void KafkaPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	//KafkaConnection->
}

void KafkaPort::Build()
{
	// The passed in method will be called whenever there are events in the queue.
	pKafkaEventQueue.reset(new StrandProtectedQueue<KafkaEvent>(*pIOS, 5000, std::bind(&KafkaPort::SendKafkaEvents, this)));

	libkafka_asio::Connection::Configuration configuration;
	configuration.auto_connect = true;
	configuration.client_id = "libkafka_asio_example"; // MyPointConf->ClientIDString
	configuration.socket_timeout = 10000;              //MyConf->mAddrConf.TCPConnectRetryPeriodms
	configuration.SetBroker("localhost",9092);         //MyConf->mAddrConf.IP, MyConf->mAddrConf.Port

	KafkaConnection.reset( new libkafka_asio::Connection (*pIOS, configuration));
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
	// Define the lambda to be called her, it is called with an async method below.

	StrandProtectedQueue<KafkaEvent>::ProcessAllEventsCallbackFnPtr pProcessCallback =
		std::make_shared<StrandProtectedQueue<KafkaEvent>::ProcessAllEventsCallbackFn>([&](std::vector<KafkaEvent> Events)
			{
				LOGDEBUG("Packaging up Kafka Message..");

				if (Events.size() == 0)
				{
				      LOGDEBUG("No Events to Send to Kafka");
				      return; // Nothing to do
				}

				libkafka_asio::MessageSet messageset; // Build multiple messages (each holding an event) into the messageset. Then we compress to a message.

				for (int i = 0; i < Events.size(); i++)
				{
				      LOGDEBUG("Message Key {}, Value {}", Events[i].Key, Events[i].Value);

				      libkafka_asio::Message message;
				      message.mutable_value().reset(new libkafka_asio::Bytes::element_type(Events[i].Value.begin(), Events[i].Value.end()));
				      message.mutable_key().reset(new libkafka_asio::Bytes::element_type(Events[i].Key.begin(), Events[i].Key.end()));

				      libkafka_asio::MessageAndOffset messageandoffset(message, i+1); // Offset is 1 based??

				      messageset.push_back(messageandoffset);
				}

				// Compress the message
				asio::error_code ec;
				libkafka_asio::Message smallmessage = CompressMessageSet(messageset, libkafka_asio::constants::Compression::kCompressionSnappy, ec);

				if (libkafka_asio::kErrorSuccess != ec)
				{
				// Compression failed...
				      LOGDEBUG("Kafka Compression of message failed! Error Code {} - {}", ec.value(), asio::system_error(ec).what());
				      return;
				}

				libkafka_asio::ProduceRequest request;
				request.set_required_acks(1); // This is the default anyway. We can set to 0 so we dont need an ack, or more than 1 of there is more than one message in a message (I think)

				std::string MessageTopic("Test");

				request.AddMessage(smallmessage, MessageTopic, 0);

				// Send the prepared produce request.
				// The connection will attempt to automatically connect to one of the brokers, specified in the configuration.
				KafkaConnection->AsyncRequest(
					request,
					[&](const libkafka_asio::Connection::ErrorCodeType & err,
					    const libkafka_asio::ProduceResponse::OptionalType & response)
					{
						if (err)
						{
						      LOGDEBUG("Kafka Produce Message Failed {} ", asio::system_error(err).what());
						}
						else
							LOGDEBUG("Kafka Produce Message Succeeded ");
					});
			});

	pKafkaEventQueue->async_pop_all(pProcessCallback);
}



