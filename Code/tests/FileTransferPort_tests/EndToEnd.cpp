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
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "FileTransferPortEndToEndTestSuite - " name

TEST_CASE(SUITE("Integrity"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("FileTransferPort"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		//make a TX port
		newptr newPort = GetPortCreator(portlib, "FileTransfer");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, "FileTransfer");
		REQUIRE(deletePort);

		Json::Value conf;
		conf["Direction"] = "TX";
		//TODO: put something in the config
		std::shared_ptr<DataPort> TX(newPort("TX", "", conf), deletePort);
		REQUIRE(TX);

		//make an RX port
		conf["Direction"] = "RX";
		//TODO: put something in the config
		std::shared_ptr<DataPort> RX(newPort("RX", "", conf), deletePort);
		REQUIRE(RX);

		//TODO: connect them

		//get them to build themselves using their configs
		RX->Build();
		TX->Build();

		//turn them on
		RX->Enable();
		TX->Enable();

		//TODO: make stuff happen


		//turn them off
		RX->Disable();
		TX->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}


