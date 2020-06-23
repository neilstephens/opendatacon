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
 * IOHandlerTests.cpp
 *
 *  Created on: 2018-06-17
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "../opendatacon/DataConnector.h"
#include "TestPorts.h"
#include <catch.hpp>
#include <opendatacon/IOTypes.h>

using namespace odc;

#define SUITE(name) "IOHandlerTestSuite - " name

TEST_CASE(SUITE("StatusCallback"))
{
	/*
	 * Send events from one port to 7 other ports
	 *	- 3 ports via separate connector IOHandlers - check port (de)multiplexing
	 *	- 4 ports via single connector - check connector (de)multiplexing
	 * verify the status callback
	 */

	auto ios = odc::asio_service::Get();
	auto work = ios->make_work();

	PublicPublishPort Source("Null1","",Json::Value::nullSingleton());
	NullPort Null2("Null2","",Json::Value::nullSingleton());
	NullPort Null3("Null3","",Json::Value::nullSingleton());
	NullPort Null4("Null4","",Json::Value::nullSingleton());
	NullPort Null5("Null5","",Json::Value::nullSingleton());
	NullPort Null6("Null6","",Json::Value::nullSingleton());
	NullPort Null7("Null7","",Json::Value::nullSingleton());
	NullPort Null8("Null8","",Json::Value::nullSingleton());
	NullPort* Sinks[7] = {&Null2,&Null3,&Null4,&Null5,&Null6,&Null7,&Null8};

	Json::Value Conn1Conf;
	Conn1Conf["Connections"][0]["Name"] = "Null1toNull2";
	Conn1Conf["Connections"][0]["Port1"] = "Null1";
	Conn1Conf["Connections"][0]["Port2"] = "Null2";
	Json::Value Conn2Conf;
	Conn2Conf["Connections"][0]["Name"] = "Null1toNull3";
	Conn2Conf["Connections"][0]["Port1"] = "Null1";
	Conn2Conf["Connections"][0]["Port2"] = "Null3";
	Json::Value Conn3Conf;
	Conn3Conf["Connections"][0]["Name"] = "Null1toNull4";
	Conn3Conf["Connections"][0]["Port1"] = "Null1";
	Conn3Conf["Connections"][0]["Port2"] = "Null4";
	Json::Value Conn4Conf;
	Conn4Conf["Connections"][0]["Name"] = "Null1toNull5";
	Conn4Conf["Connections"][0]["Port1"] = "Null1";
	Conn4Conf["Connections"][0]["Port2"] = "Null5";
	Conn4Conf["Connections"][1]["Name"] = "Null1toNull6";
	Conn4Conf["Connections"][1]["Port1"] = "Null1";
	Conn4Conf["Connections"][1]["Port2"] = "Null6";
	Conn4Conf["Connections"][2]["Name"] = "Null1toNull7";
	Conn4Conf["Connections"][2]["Port1"] = "Null1";
	Conn4Conf["Connections"][2]["Port2"] = "Null7";
	Conn4Conf["Connections"][3]["Name"] = "Null1toNull8";
	Conn4Conf["Connections"][3]["Port1"] = "Null1";
	Conn4Conf["Connections"][3]["Port2"] = "Null8";

	DataConnector Conn1("Conn1","",Conn1Conf);
	DataConnector Conn2("Conn2","",Conn2Conf);
	DataConnector Conn3("Conn3","",Conn3Conf);
	DataConnector Conn4("Conn4","",Conn4Conf);
	DataConnector* Conns[4] = {&Conn1,&Conn2,&Conn3,&Conn4};

	for(auto& c :Conns)
		c->Enable();

	std::atomic_bool executed(false);
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&](CommandStatus status)
		{
			cb_status = status;
			executed = true;
		});

	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock);
	event->SetPayload<EventType::ControlRelayOutputBlock>(ControlRelayOutputBlock());

	for(unsigned char mask = 0x00; mask <= 0x7F; mask++)
	{
		for(int i = 0; i < 7; i++)
			(mask>>i & 0x01) ? Sinks[i]->Enable() : Sinks[i]->Disable();

		executed = false;
		event->SetIndex(mask);

		Source.PublicPublishEvent(event,StatusCallback);
		while(!executed)
		{
			ios->run_one();
		}
		if(mask == 0x00)
			REQUIRE(cb_status == CommandStatus::BLOCKED);
		else if(mask == 0x7F)
			REQUIRE(cb_status == CommandStatus::SUCCESS);
		else
			REQUIRE(cb_status == CommandStatus::UNDEFINED);
	}
}
