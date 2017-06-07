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
#include <catch.hpp>

#include "DNP3PortFactory.h"
#include <opendatacon/DataPortFactoryCollection.h>

#define SUITE(name) "DNP3PortEndToEndTestSuite - " name

TEST_CASE(SUITE("TCP link"))
{
	std::shared_ptr<odc::IOManager> IOMgr(new odc::ODCManager(std::thread::hardware_concurrency()));
	odc::DataPortFactoryCollection DataPortFactories(IOMgr);
	odc::DataPortFactory* factory = DataPortFactories.GetFactory("DNP3Port");
	REQUIRE(factory);
	
	//make an outstation port
	Json::Value Oconf;
	Oconf["IP"] = "0.0.0.0";
	Oconf["ServerType"] = "PERSISTENT";
	Oconf["TCPClientServer"] = "SERVER";
	odc::DataPort* OPUT = factory->CreateDataPort("DNP3Outstation","OutstationUnderTest", "", Oconf);
	REQUIRE(OPUT);

	//make a master port
	Json::Value Mconf;
	Mconf["ServerType"] = "PERSISTENT";
	odc::DataPort* MPUT = factory->CreateDataPort("DNP3Master","MasterUnderTest", "", Mconf);
	REQUIRE(MPUT);

	//get them to build themselves using their configs
	OPUT->BuildOrRebuild();
	MPUT->BuildOrRebuild();

	//turn them on
	OPUT->Enable();
	REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port enabled - link down");
	
	MPUT->Enable();
	REQUIRE(MPUT->GetStatus()["Result"].asString() != "Port disabled");
	
	//TODO: write a better way to wait for GetStatus and timeout (when decouple gets merged)
	unsigned int count = 0;
	while(count < 5000 &&
		  (MPUT->GetStatus()["Result"].asString() == "Port enabled - link down" ||
		   OPUT->GetStatus()["Result"].asString() == "Port enabled - link down"))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");
	REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");

	//turn outstation off
	OPUT->Disable();

	count = 0;
	while(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)" && count < 5000)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link down");
	REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");

	delete MPUT;
	delete OPUT;
}

TEST_CASE(SUITE("Serial link"))
{
	if (!system(NULL))
	{
		WARN("Can't get system shell to execute socat (for virtual serial ports) - skipping test");
		return;
	}
	if(system("socat -V"))
	{
		WARN("Failed to locate socat binary (for virtual serial ports) - skipping test");
		return;
	}
	if(system("socat pty,raw,echo=0,link=SerialEndpoint1 pty,raw,echo=0,link=SerialEndpoint2 &"))
	{
		WARN("Failed to execute socat (for virtual serial ports) - skipping test");
		return;
	}
	std::shared_ptr<odc::IOManager> IOMgr(new odc::ODCManager(std::thread::hardware_concurrency()));
	odc::DataPortFactoryCollection DataPortFactories(IOMgr);
	odc::DataPortFactory* factory = DataPortFactories.GetFactory("DNP3Port");
	REQUIRE(factory);
	
	//make an outstation port
	Json::Value Oconf;
	Oconf["SerialDevice"] = "SerialEndpoint1";
	odc::DataPort* OPUT = factory->CreateDataPort("DNP3Outstation","OutstationUnderTest", "", Oconf);
	REQUIRE(OPUT);
	
	//make a master port
	Json::Value Mconf;
	Mconf["ServerType"] = "PERSISTENT";
	Mconf["SerialDevice"] = "SerialEndpoint2";
	Mconf["LinkKeepAlivems"] = 200;
	Mconf["LinkTimeoutms"] = 100;
	odc::DataPort* MPUT = factory->CreateDataPort("DNP3Master","MasterUnderTest", "", Mconf);
	REQUIRE(MPUT);

	//get them to build themselves using their configs
	OPUT->BuildOrRebuild();
	MPUT->BuildOrRebuild();

	//turn them on
	OPUT->Enable();
	MPUT->Enable();

	//TODO: write a better way to wait for GetStatus and timeout (when decouple gets merged)
	unsigned int count = 0;
	while((MPUT->GetStatus()["Result"].asString() == "Port enabled - link down" || OPUT->GetStatus()["Result"].asString() == "Port enabled - link down") && count < 5000)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");
	REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");

	//turn outstation off
	OPUT->Disable();

	count = 0;
	while(MPUT->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)" && count < 5000)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(MPUT->GetStatus()["Result"].asString() == "Port enabled - link down");
	REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");

	delete MPUT;
	delete OPUT;
}
