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

#include "PortLoader.h"

#define SUITE(name) "DNP3MasterPortTestSuite - " name

TEST_CASE(SUITE("ConstructEnableDisableDestroy"))
{
	//Load the library
	InitLibaryLoading();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		auto ios = std::make_shared<asio::io_service>();
		newptr newMaster = GetPortCreator(portlib, "DNP3Master");
		REQUIRE(newMaster);
		delptr deleteMaster = GetPortDestroyer(portlib, "DNP3Master");
		REQUIRE(deleteMaster);
		DataPort* MPUT = newMaster("MasterUnderTest", "", "");

		MPUT->SetIOS(ios);
		MPUT->Enable();
		MPUT->Disable();

		deleteMaster(MPUT);
	}
	/// Test the destruction of an enabled port
	{
		auto ios = std::make_shared<asio::io_service>();
		newptr newMaster = GetPortCreator(portlib, "DNP3Master");
		REQUIRE(newMaster);
		delptr deleteMaster = GetPortDestroyer(portlib, "DNP3Master");
		REQUIRE(deleteMaster);
		DataPort* MPUT = newMaster("MasterUnderTest", "", "");

		MPUT->SetIOS(ios);
		MPUT->Enable();

		deleteMaster(MPUT);
	}
	UnLoadModule(portlib);
}
