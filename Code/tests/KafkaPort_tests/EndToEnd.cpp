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
/**
 */
#include "Helpers.h"
#include "../PortLoader.h"
#include "../ThreadPool.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>
#include <map>

#define SUITE(name) "KafkaPortEndToEndTestSuite - " name

TEST_CASE(SUITE("Produce"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("KafkaPort"));
	REQUIRE(portlib);
	{
		newptr newPort = GetPortCreator(portlib, "KafkaProducer");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, "KafkaProducer");
		REQUIRE(deletePort);

		//TODO: spin up a kafka cluster

		//TODO: put something in the config
		std::shared_ptr<DataPort> PUT(newPort("PortUnderTest", "", ""), deletePort);
		PUT->Build();

		ThreadPool thread_pool(1);

		PUT->Enable();

		//TODO: load a message into the port

		//TODO: spin up a consumer to check the message was received

		PUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

