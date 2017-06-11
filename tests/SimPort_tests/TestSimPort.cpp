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

#include "SimPortFactory.h"
#include <opendatacon/DataPortFactoryCollection.h>
#include <opendatacon/ODCManager.h>

#define SUITE(name) "SimPortTestSuite - " name

TEST_CASE(SUITE("ConstructEnableDisableDestroy"))
{
	for(int i=0;i<100;i++){
		std::shared_ptr<odc::IOManager> IOMgr(new odc::ODCManager(std::thread::hardware_concurrency()));
		{
			odc::DataPortFactoryCollection DataPortFactories(IOMgr);
			odc::DataPortFactory* factory(DataPortFactories.GetFactory("SimPort"));
			REQUIRE(factory);
			{
				std::unique_ptr<odc::DataPort> SPUT(factory->CreateDataPort("Sim","SimPortUnderTest", "", ""));
				REQUIRE(SPUT);
				
				//REQUIRE(SPUT->GetStatus()["Result"].asString() == "Port disabled");
				SPUT->Enable();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() == "Port disabled");
			}
			{
				std::unique_ptr<odc::DataPort> SPUT(factory->CreateDataPort("Sim","SimPortUnderTest", "", ""));
				REQUIRE(SPUT);
				
				SPUT->BuildOrRebuild();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() == "Port disabled");
				SPUT->Enable();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() != "Port disabled");
				SPUT->Disable();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() == "Port disabled");
			}
			/// Test the destruction of an enabled port
			{
				std::unique_ptr<odc::DataPort> SPUT(factory->CreateDataPort("Sim","SimPortUnderTest", "", ""));
				REQUIRE(SPUT);
				
				SPUT->BuildOrRebuild();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() == "Port disabled");
				SPUT->Enable();
				//REQUIRE(SPUT->GetStatus()["Result"].asString() != "Port disabled");
			}
		}
	}
}
