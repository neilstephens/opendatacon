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
 *  Created on: 07/02/2023
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "Helpers.h"
#include "../PortLoader.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "FileTransferPortRXTestSuite - " name

TEST_CASE(SUITE("Integrity"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("FileTransferPort"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		newptr newPort = GetPortCreator(portlib, "FileTransfer");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, "FileTransfer");
		REQUIRE(deletePort);

		//TODO: put something in the config
		std::shared_ptr<DataPort> PUT(newPort("PortUnderTest", "", ""), deletePort);

		PUT->Build();
		PUT->Enable();

		//TODO - test something

		PUT->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	TestTearDown();
	UnLoadModule(portlib);
}
