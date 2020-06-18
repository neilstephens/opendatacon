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
 * EventInfoTests.cpp
 *
 *  Created on: 05/06/2019
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "../opendatacon/DataConnector.h"
#include "TestPorts.h"
#include <atomic>
#include <catch.hpp>

using namespace odc;

#define SUITE(name) "EventInfoTestSuite - " name

TEST_CASE(SUITE("EventTypes"))
{
	auto event_type = EventType::BeforeRange;
	REQUIRE(ToString(event_type) == "<no_string_representation>");
	while((event_type+1) != EventType::AfterRange)
	{
		event_type = event_type+1;
		REQUIRE(ToString(event_type) != "<no_string_representation>");

		//construct
		std::shared_ptr<EventInfo> event;
		REQUIRE_NOTHROW(event = std::make_shared<EventInfo>(event_type););

		//set default payload
		REQUIRE_NOTHROW(event->SetPayload());

		//copy
		std::shared_ptr<EventInfo> event_copy;
		REQUIRE_NOTHROW(event_copy = std::make_shared<EventInfo>(*event););

		//destruct
		REQUIRE_NOTHROW(event.reset());
		REQUIRE_NOTHROW(event_copy.reset());
	}
}

TEST_CASE(SUITE("PayloadTransport"))
{
	//Generate a load of events
	//send them over a DataConnector
	//check they arrived intact

	auto ios = odc::asio_service::Get();
	auto work = ios->make_work();

	PublicPublishPort Source("Source","",Json::Value::nullSingleton());
	PayloadCheckPort Sink("Sink","",Json::Value::nullSingleton());

	Json::Value ConnConf;
	ConnConf["Connections"][0]["Name"] = "SourceToSink";
	ConnConf["Connections"][0]["Port1"] = "Source";
	ConnConf["Connections"][0]["Port2"] = "Sink";
	DataConnector Conn("Conn","",ConnConf);

	Source.Enable();
	Sink.Enable();
	Conn.Enable();

	std::atomic<uint16_t> cb_count(0);
	auto StatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&](CommandStatus status)
		{
			REQUIRE(status == CommandStatus::SUCCESS);
			cb_count++;
		});

	std::vector<std::shared_ptr<EventInfo>> events;
	for(int i = 0; i < 1000; i++)
	{
		auto time = msSinceEpoch();
		auto qual = QualityFlags::ONLINE;
		events.push_back(std::make_shared<EventInfo>(EventType::OctetString,i,"Source",qual,time));

		std::string payload = std::to_string(i)+"_"+
		                      "Source_"+
		                      ToString(qual)+"_"+
		                      std::to_string(time);
		events.back()->SetPayload<EventType::OctetString>(std::move(payload));
	}
	for(const auto& e : events)
	{
		Source.PublicPublishEvent(e,StatusCallback);
	}
	std::vector<std::thread> threads;
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		threads.emplace_back([&]() {ios->run(); });
	while(cb_count < 1000)
		ios->poll_one();
	work.reset();
	for(auto& t : threads)
		t.join();
}


