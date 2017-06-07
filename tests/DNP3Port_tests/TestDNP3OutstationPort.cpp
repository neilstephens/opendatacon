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
#include <opendatacon/ODCManager.h>

#define SUITE(name) "DNP3OutstationPortTestSuite - " name

TEST_CASE(SUITE("ConstructEnableDisableDestroy"))
{
	std::shared_ptr<odc::IOManager> IOMgr(new odc::ODCManager(std::thread::hardware_concurrency()));
	odc::DataPortFactoryCollection DataPortFactories(IOMgr);
	odc::DataPortFactory* factory = DataPortFactories.GetFactory("DNP3Port");
	REQUIRE(factory);
	{
		odc::DataPort* OPUT = factory->CreateDataPort("DNP3Outstation","OutstationUnderTest", "", "");
		REQUIRE(OPUT);
		
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");
		OPUT->Enable();
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");
		
		delete OPUT;
	}
	{
		odc::DataPort* OPUT = factory->CreateDataPort("DNP3Outstation","OutstationUnderTest", "", "");
		REQUIRE(OPUT);
		
		OPUT->BuildOrRebuild();
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");
		OPUT->Enable();
		REQUIRE(OPUT->GetStatus()["Result"].asString() != "Port disabled");
		OPUT->Disable();
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");
		
		delete OPUT;
	}
	/// Test the destruction of an enabled port
	{
		odc::DataPort* OPUT = factory->CreateDataPort("DNP3Outstation","OutstationUnderTest", "", "");
		REQUIRE(OPUT);
		
		OPUT->BuildOrRebuild();
		REQUIRE(OPUT->GetStatus()["Result"].asString() == "Port disabled");
		OPUT->Enable();
		REQUIRE(OPUT->GetStatus()["Result"].asString() != "Port disabled");
		
		delete OPUT;
	}

}
