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
#include "../../../opendatacon/NullPort.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>
#include <filesystem>

#define SUITE(name) "FileTransferPortTXTestSuite - " name

const unsigned int test_timeout = 10000;

TEST_CASE(SUITE("Triggers"))
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

		auto conf = GetConfigJSON(true);
		std::shared_ptr<DataPort> PUT(newPort("PortUnderTest", "", conf), deletePort);

		NullPort Null("Null","","");
		PUT->Subscribe(&Null,"Null");

		PUT->Build();

		//clear the slate
		std::remove("FileTxTest1.bin");
		std::remove("FileTxTest2.bin");
		std::remove("sub/FileTxTest3.bin");
		std::remove("sub/FileTxTest4.bin");
		std::remove("sub/sub/FileTxTest5.bin");
		std::remove("sub/sub/FileTxTest6.bin");
		std::remove("FileTxTest7.notbin");
		std::remove("FileTxTest8.notbin");
		std::remove("sub/FileTxTest9.notbin");
		std::remove("sub/FileTxTest10.notbin");
		std::remove("sub/sub/FileTxTest11.notbin");
		std::remove("sub/sub/FileTxTest12.notbin");

		PUT->Enable();

		//create some files that match our transfer pattern
		std::filesystem::create_directories(std::filesystem::path("sub/sub"));
		WriteFile("FileTxTest1.bin",1300); //1300 not a multiple of 255
		WriteFile("FileTxTest2.bin",5100); //5100 is a multiple of 255
		WriteFile("sub/FileTxTest3.bin",1300);
		WriteFile("sub/FileTxTest4.bin",5100);
		WriteFile("sub/sub/FileTxTest5.bin",1300);
		WriteFile("sub/sub/FileTxTest6.bin",5100);
		//and a few that don't match (just in case there aren't any)
		WriteFile("FileTxTest7.notbin",13);
		WriteFile("FileTxTest8.notbin",51);
		WriteFile("sub/FileTxTest9.notbin",13);
		WriteFile("sub/FileTxTest10.notbin",51);
		WriteFile("sub/sub/FileTxTest11.notbin",13);
		WriteFile("sub/sub/FileTxTest12.notbin",51);

		size_t count = 0;
		auto stats = PUT->GetStatistics();
		while((stats["TxFileMatchCount"].asUInt() < 6 || stats["FilesTransferred"].asUInt() < 6) && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = PUT->GetStatistics();
		}

		REQUIRE(stats["TxFileMatchCount"].asUInt() == 6);
		REQUIRE(stats["FilesTransferred"].asUInt() == 6);
		REQUIRE(stats["FileBytesTransferred"].asUInt() == 19200);
		REQUIRE((stats["TxFileDirCount"].asUInt() - stats["TxFileMatchCount"].asUInt()) >= 6);

		//Test binary control trigger
		auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock,0);
		PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
		count = 0;
		stats = PUT->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 6*2 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = PUT->GetStatistics();
		}
		REQUIRE(stats["FilesTransferred"].asUInt() == 6*2);
		REQUIRE(stats["FileBytesTransferred"].asUInt() == 19200*2);

		//Analog control trigger
		event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16,0);
		event->SetPayload<EventType::AnalogOutputInt16>({3,CommandStatus::SUCCESS});
		PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
		count = 0;
		stats = PUT->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 6*3 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = PUT->GetStatistics();
		}
		REQUIRE(stats["FilesTransferred"].asUInt() == 6*3);
		REQUIRE(stats["FileBytesTransferred"].asUInt() == 19200*3);

		//And request a specific path with OctetString event
		event = std::make_shared<EventInfo>(EventType::OctetString,0);
		event->SetPayload<EventType::OctetString>(OctetStringBuffer("sub/FileTxTest3.bin"));
		PUT->Event(event,"me",std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
		count = 0;
		stats = PUT->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 6*3+1 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = PUT->GetStatistics();
		}
		REQUIRE(stats["FilesTransferred"].asUInt() == 6*3+1);
		REQUIRE(stats["FileBytesTransferred"].asUInt() == 19200*3+1300);

		PUT->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	TestTearDown();
	UnLoadModule(portlib);
}
