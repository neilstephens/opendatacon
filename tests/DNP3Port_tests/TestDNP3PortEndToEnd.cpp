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
#include "TestDNP3Helpers.h"
#include "../PortLoader.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "DNP3PortEndToEndTestSuite - " name

TEST_CASE(SUITE("TCP link"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		//make an outstation port
		newptr newOutstation = GetPortCreator(portlib, "DNP3Outstation");
		REQUIRE(newOutstation);
		delptr delOutstation = GetPortDestroyer(portlib, "DNP3Outstation");
		REQUIRE(delOutstation);

		Json::Value conf;
		auto OPUT = std::shared_ptr<DataPort>(newOutstation("OutstationUnderTest", "", conf), delOutstation);
		REQUIRE(OPUT);

		//make a master port
		newptr newMaster = GetPortCreator(portlib, "DNP3Master");
		REQUIRE(newMaster);
		delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
		REQUIRE(delMaster);

		conf["ServerType"] = "PERSISTENT";
		auto MPUT = std::unique_ptr<DataPort,delptr>(newMaster("MasterUnderTest", "", conf), delMaster);
		REQUIRE(MPUT);

		//get them to build themselves using their configs
		OPUT->Build();
		MPUT->Build();

		//turn them on
		OPUT->Enable();
		MPUT->Enable();

		//TODO: write a better way to wait for GetStatus
		unsigned int count = 0;
		while((MPUT->GetStatus()["Result"].asString() == "Port enabled - link down" || OPUT->GetStatus()["Result"].asString() == "Port enabled - link down") && count < 20000)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			count++;
		}

		REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");

		//wait to actually recieve something
		count = 0;
		while(MPUT->GetStatistics()["transport"]["numTransportRx"].asUInt() == 0 && count < 20000)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			count++;
		}
		REQUIRE(MPUT->GetStatistics()["transport"]["numTransportRx"].asUInt() > 0);

		//turn outstation off
		OPUT->Disable();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		count = 0;
		while(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)" && count < 20000)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			count++;
		}

		REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link down");
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");

		MPUT->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}


TEST_CASE(SUITE("Serial link"))
{
	if(system("socat -h > /dev/null"))
	{
		WARN("Failed to execute socat (for virtual serial ports) - skipping test");
	}
	else
	{
		TestSetup();

		auto portlib = LoadModule(GetLibFileName("DNP3Port"));
		REQUIRE(portlib);
		{
			auto ios = odc::asio_service::Get();
			auto work = ios->make_work();
			std::thread t([ios](){ios->run();});

			if(system("socat pty,raw,echo=0,link=SerialEndpoint1 pty,raw,echo=0,link=SerialEndpoint2 &"))
			{
				WARN("socat system call failed");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			//make an outstation port
			newptr newOutstation = GetPortCreator(portlib, "DNP3Outstation");
			REQUIRE(newOutstation);
			delptr delOutstation = GetPortDestroyer(portlib, "DNP3Outstation");
			REQUIRE(delOutstation);

			Json::Value Oconf;
			Oconf["SerialDevice"] = "SerialEndpoint1";
			Oconf["LOG_LEVEL"] = "ALL";
			auto OPUT = std::shared_ptr<DataPort>(newOutstation("OutstationUnderTest", "", Oconf), delOutstation);
			REQUIRE(OPUT);

			//make a master port
			newptr newMaster = GetPortCreator(portlib, "DNP3Master");
			REQUIRE(newMaster);
			delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
			REQUIRE(delMaster);

			Json::Value Mconf;
			Mconf["ServerType"] = "PERSISTENT";
			Mconf["SerialDevice"] = "SerialEndpoint2";
			Mconf["LOG_LEVEL"] = "ALL";
			Mconf["LinkKeepAlivems"] = 200;
			Mconf["LinkTimeoutms"] = 100;
			auto MPUT = std::unique_ptr<DataPort,delptr>(newMaster("MasterUnderTest", "", Mconf), delMaster);
			REQUIRE(MPUT);

			//get them to build themselves using their configs
			OPUT->Build();
			MPUT->Build();

			//turn them on
			OPUT->Enable();
			MPUT->Enable();

			//TODO: write a better way to wait for GetStatus
			unsigned int count = 0;
			while((MPUT->GetStatus()["Result"].asString() == "Port enabled - link down" || OPUT->GetStatus()["Result"].asString() == "Port enabled - link down") && count < 20000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count++;
			}

			REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");
			REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");

			//wait to actually recieve something
			count = 0;
			while(MPUT->GetStatistics()["transport"]["numTransportRx"].asUInt() == 0 && count < 20000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count++;
			}
			REQUIRE(MPUT->GetStatistics()["transport"]["numTransportRx"].asUInt() > 0);

			//turn outstation off
			OPUT->Disable();

			count = 0;
			std::string new_status;
			while((new_status = MPUT->GetStatus()["Result"].asString()) == "Port enabled - link up (unreset)" && count < 20000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count++;
			}

			REQUIRE(new_status == "Port enabled - link down");
			REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");

			MPUT->Disable();

			if(system("killall socat"))
			{
				WARN("kill socat system call failed");
			}

			work.reset();
			ios->run();
			t.join();
			ios.reset();
		}
		//Unload the library
		UnLoadModule(portlib);
		TestTearDown();
	}
}

