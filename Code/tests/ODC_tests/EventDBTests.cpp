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
 * EventDBTests.cpp
 *
 *  Created on: 27/04/2021
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "TestODCHelpers.h"
#include <opendatacon/EventDB.h>
#include <catch.hpp>
#include <vector>
#include <memory>
#include <cstdlib>

using namespace odc;

#define SUITE(name) "EventDBTestSuite - " name

std::shared_ptr<EventDB> GetTestEventDB()
{
	//create a database to hold 100 events
	std::vector<std::shared_ptr<const EventInfo>> init_events;
	for(int i=0; i<100; i++)
	{
		auto event = std::make_shared<EventInfo>(EventType::OctetString,i,"EventDBTestSuite");
		event->SetPayload<EventType::OctetString>(std::to_string(i));
		init_events.push_back(event);
	}
	return std::make_shared<EventDB>(init_events);
}

TEST_CASE(SUITE("Get Set Swap"))
{
	TestSetup();
	auto pDB = GetTestEventDB();
	for(int i=0; i<100; i++)
	{
		auto event = pDB->Get(EventType::OctetString,i);
		REQUIRE(event->GetPayload<EventType::OctetString>() == std::to_string(i));
		event.reset();

		auto new_event = std::make_shared<EventInfo>(EventType::OctetString,i,"EventDBTestSuite");
		new_event->SetPayload<EventType::OctetString>(std::to_string(i*2));
		pDB->Set(new_event);
		new_event.reset();
		REQUIRE(pDB->Get(EventType::OctetString,i)->GetPayload<EventType::OctetString>() == std::to_string(i*2));

		auto swap_in = std::make_shared<EventInfo>(EventType::OctetString,i,"EventDBTestSuite");
		swap_in->SetPayload<EventType::OctetString>(std::to_string(i*3));
		auto swap_out = pDB->Swap(swap_in);
		swap_in.reset();
		REQUIRE(pDB->Get(EventType::OctetString,i)->GetPayload<EventType::OctetString>() == std::to_string(i*3));
		REQUIRE(swap_out->GetPayload<EventType::OctetString>() == std::to_string(i*2));
	}
	TestTearDown();
}

TEST_CASE(SUITE("Thread Safety"))
{
	TestSetup();
	auto pDB = GetTestEventDB();
	//Access the element concurrently
	std::vector<std::thread> threads;
	for(int i = 0; i<10; i++)
	{
		threads.emplace_back([pDB]()
			{
				for(int j = 0; j<10000000; j++)
				{
				//replace a random element
				      auto new_evt = std::make_shared<EventInfo>(EventType::OctetString, rand()%100, "EventDBTestSuite");
				      new_evt->SetPayload<EventType::OctetString>(std::to_string(j));
				      auto old_evt = pDB->Swap(new_evt);

				      REQUIRE(old_evt);

				//access the data - randomly print
				      if(std::stoi(old_evt->GetPayload<EventType::OctetString>())%10000 == 0 && old_evt->GetIndex() == 5)
						odc::spdlog_get("opendatacon")->info("Replaced {} with {}",
							old_evt->GetPayloadString(),
							new_evt->GetPayloadString());
				}
			});
	}
	for(auto& t : threads)
		t.join();

	TestTearDown();
}

