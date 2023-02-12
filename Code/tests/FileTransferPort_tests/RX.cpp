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
#include "../../../include/opendatacon/IOTypes.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>
#include <filesystem>
#include <fstream>

#define SUITE(name) "FileTransferPortRXTestSuite - " name

const unsigned int test_timeout = 10000;

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

		std::filesystem::create_directories(std::filesystem::path("RX"));
		auto conf = GetConfigJSON(false);
		std::shared_ptr<DataPort> PUT(newPort("PortUnderTest", "", conf), deletePort);

		PUT->Build();
		PUT->Enable();

		//send the filename event
		auto event = std::make_shared<EventInfo>(EventType::OctetString,0);
		event->SetPayload<EventType::OctetString>(OctetStringBuffer("FileRxTest.txt"));
		PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));

		//start sending data (with the wrong sequence number)
		size_t seq = 1;
		for(size_t i = 0; i < 45; i++)
		{
			event = std::make_shared<EventInfo>(EventType::OctetString,(seq = ++seq > 15 ? 1 : seq));
			event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::to_string(i)+" "));
			PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
		}

		//send EOF event
		event = std::make_shared<EventInfo>(EventType::OctetString,16);
		event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::to_string(1)));
		PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));

		size_t count = 0;
		auto stats = PUT->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 1 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = PUT->GetStatistics();
		}

		REQUIRE(std::filesystem::exists("RX/dateformat_FileRxTest.txt"));
		std::ifstream fin("RX/dateformat_FileRxTest.txt");
		REQUIRE(!fin.fail());

		std::string file_contents;
		char ch;
		while(fin.get(ch))
			file_contents.push_back(ch);

		REQUIRE(file_contents == "14 0 1 2 3 4 5 6 7 8 9 10 11 12 13 29 15 16 17 18 19 20 21 22 23 24 25 26 27 28 44 30 31 32 33 34 35 36 37 38 39 40 41 42 43 ");

		PUT->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	TestTearDown();
	UnLoadModule(portlib);
}
