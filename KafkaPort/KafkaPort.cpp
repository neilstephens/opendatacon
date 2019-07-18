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
#include <gsl/gsl_util.h>
#include "KafkaPort.h"
#include "KafkaPortConf.h"
#include "StrandProtectedQueue.h"

KafkaPort::KafkaPort(const std::string &aName, const std::string & aConfFilename, const Json::Value & aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	UniqueMessageIndex(0)
{
	//the creation of a new KafkaPortConf will get the point details
	pConf = std::make_unique<KafkaPortConf>(ConfFilename, ConfOverrides);

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
	pKafkaEventQueue = std::make_shared<StrandProtectedQueue<KafkaEvent>>(*pIOS, 100, std::bind(&KafkaPort::SendKafkaEvents, this));

	libkafka_asio::Connection::Configuration configuration;
	configuration.auto_connect = true;
	configuration.client_id = "odckafkaport"; // MyPointConf->ClientIDString
	configuration.socket_timeout = 10000;     //MyConf->mAddrConf.TCPConnectRetryPeriodms
	configuration.SetBroker(MyConf->mAddrConf.IP, MyConf->mAddrConf.Port);

	KafkaConnection = std::make_shared<libkafka_asio::Connection>(*pIOS->ReallyWantTheIOSPtr(), configuration);
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

	QualityFlags quality = event->GetQuality();
	odc::msSinceEpoch_t timestamp = event->GetTimestamp();

	switch (event->GetEventType())
	{
		case EventType::Analog:
		{
			double measurement = event->GetPayload<EventType::Analog>();

			LOGDEBUG("OS - Received Event - Analog - Index {}  Value {}  Quality {}", std::to_string(ODCIndex), measurement, ToString(quality));
			std::string key;
			if (!MyPointConf->PointTable.GetAnalogKafkaKeyUsingODCIndex(ODCIndex, key))
			{
				LOGERROR("Tried to get the key for an invalid analog point index " + std::to_string(ODCIndex));
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			QueueKafkaEvent(key, measurement, quality, timestamp);

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		case EventType::Binary:
		{
			double measurement = event->GetPayload<EventType::Binary>();

			LOGDEBUG("OS - Received Event - Binary - Index {}  Value {}  Quality {}", std::to_string(ODCIndex), measurement, ToString(quality));
			std::string key;
			if (!MyPointConf->PointTable.GetBinaryKafkaKeyUsingODCIndex(ODCIndex, key))
			{
				LOGERROR("Tried to get the key for an invalid abinary point index " + std::to_string(ODCIndex));
				return (*pStatusCallback)(CommandStatus::UNDEFINED);
			}
			QueueKafkaEvent(key, measurement, quality, timestamp);

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

std::string KafkaPort::to_ISO8601_TimeString(odc::msSinceEpoch_t timestamp)
{
	time_t tp = timestamp / 1000; // time_t is normally seconds since epoch. We deal in msec!
	int msec = timestamp % 1000;

	// Convert to UTC
	std::tm* t = std::gmtime(&tp);
	if (t != nullptr)
	{
		char buf[sizeof "2011-10-08T07:07:09.000Z++++"];
		std::strftime(buf, sizeof(buf), "%FT%T", t);
		std::string result = fmt::format("{}.{:03}Z",buf, msec);
		return result;
	}
	LOGERROR("Failed to convert timestamp to UTC ISO8601 time string");
	return "";
}

std::string KafkaPort::CreateKafkaPayload(const std::string& key,  double measurement, odc::QualityFlags quality, const odc::msSinceEpoch_t& timestamp)
{
	try
	{
		std::string value = fmt::format("{{\"PITag\" : \"{}\", \"Index\" : {}, \"Value\" : {}, \"Quality\" : \"{}\", \"TimeStamp\" : \"{}\"}}",
			key, UniqueMessageIndex++, measurement, ToString(quality), to_ISO8601_TimeString(timestamp));
		return value;
	}
	catch (std::exception & e)
	{
		LOGERROR("Format Problem in CreateKafkaPayload : {} - WE LOST DATA", e.what());
	}
	return "";
}

void KafkaPort::QueueKafkaEvent(const std::string &key, double measurement, QualityFlags quality, odc::msSinceEpoch_t timestamp)
{
	if (key.length() == 0)
	{
		LOGERROR("Tried to queue an empty key to Kafka");
		return;
	}
	std::string value = CreateKafkaPayload(key, measurement, quality, timestamp );
	KafkaEvent ev(key, value);
	pKafkaEventQueue->async_push(ev); // Takes a copy

	LOGDEBUG("Queued Kafka Message - {}, {}", key, value);
	// Now actually send it to Kafka!!!
}

// Process everything in the queue, into a Kafka message, compress and send.
// This method is registered with the queue, and will be called when:
// New data is pushed into the queue - provided is not waiting for a response to a previously sent Kafka message.
// As there should be a steady stream of messages - dont worry about timeouts to trigger this call.
// The queue manages the syncronisation of when this is called - it is only called on the queue strand.
//
void KafkaPort::SendKafkaEvents()
{
	LOGDEBUG("SendKafkaEvents called");

	// Setup a lambda, that will be called with a vector of all the events currently in the queue.
	// We process them and send them to the Kafka cluster.
	// Define the lambda to be called here, it is called with an async method below.

	StrandProtectedQueue<KafkaEvent>::ProcessAllEventsCallbackFnPtr pProcessCallback =
		std::make_shared<StrandProtectedQueue<KafkaEvent>::ProcessAllEventsCallbackFn>([&](std::vector<KafkaEvent> Events)
			{
				libkafka_asio::ProduceRequest request;
				try
				{
				      if (Events.size() == 0)
				      {
				            LOGERROR("No Events to Send to Kafka");
				            pKafkaEventQueue->finished_pop_all(); // Otherwise we get stuck!
				            return;                               // Nothing to do
					}

				      LOGDEBUG("Packaging up Kafka Message.. {} Events", Events.size());

				      libkafka_asio::MessageSet messageset; // Build multiple messages (each holding an event) into the messageset. Then we compress to a message.

				      for (int i = 0; i < Events.size(); i++)
				      {
				//LOGDEBUG("Message Key {}, Value {}", Events[i].Key, Events[i].Value);

				            libkafka_asio::Message message;
				            message.mutable_value() = std::make_shared<libkafka_asio::Bytes::element_type>(Events[i].Value.begin(), Events[i].Value.end());
				            message.mutable_key() = std::make_shared<libkafka_asio::Bytes::element_type>(Events[i].Key.begin(), Events[i].Key.end());

				            libkafka_asio::MessageAndOffset messageandoffset(message, i + 1); // Offset is 1 based??

				            messageset.push_back(messageandoffset);
					}

				// Compress the message
				      asio::error_code ec;
				      libkafka_asio::Message smallmessage = CompressMessageSet(messageset, libkafka_asio::constants::Compression::kCompressionSnappy, ec);

				      if (libkafka_asio::kErrorSuccess != ec)
				      {
				            LOGERROR("Kafka Compression of message failed! Error Code {} - {} - WE LOST DATA", ec.value(), asio::system_error(ec).what());
				            pKafkaEventQueue->finished_pop_all(); // Otherwise we get stuck!
				            return;
					}

				      request.set_required_acks(1); // This is the default anyway. We can set to 0 so we dont need an ack, or more than 1 of there is more than one message in a message (I think)

				      request.AddMessage(smallmessage, GetTopic(), 0);
				}
				catch(std::exception& e)
				{
				      LOGERROR("Problem Packaging up Kafka Message : {} - WE LOST DATA", e.what());
				      pKafkaEventQueue->finished_pop_all(); // Otherwise we get stuck!
				      return;
				}
				// Send the prepared produce request.
				// The connection will attempt to automatically connect to one of the brokers, specified in the configuration.
				// We cannot get back into this lambda until the pKafkaEventQueue->finished_pop_all(); method is called. That does not happen
				// until the request is finished.
				// We DO NOT do any timeout processing. We wait until forever for the request to finish.

				#ifdef DOPOST
				LOGDEBUG("Posting Async Kafka Request");
				KafkaConnection->AsyncRequest(
					request,
					[&](const libkafka_asio::Connection::ErrorCodeType & err,
					    const libkafka_asio::ProduceResponse::OptionalType & response)
					{
						if (err)
						{
						      LOGERROR("Kafka Produce (Send) Message Failed {} ", asio::system_error(err).what());
						}
						else
						{
						      LOGDEBUG("Kafka Produce (Send) Message Succeeded ");
						}
						pKafkaEventQueue->finished_pop_all();
					});
				#else
				LOGERROR("NOT POSTING Async Kafka Request - check DOPOST compiler flag");
				pKafkaEventQueue->finished_pop_all();
				#endif
			});

	// This produces the events vector and passes it to the passed lambda (code above). Kind of convoluted....
	pKafkaEventQueue->async_pop_all(pProcessCallback);
}



