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
 * TransformConnectorTests.cpp
 *
 *  Created on: 2/7/2023
 *      Author: Neil Stephens
 */

#include "TestTransforms.h"
#include "TestODCHelpers.h"
#include "../../../opendatacon/DataConnector.h"
#include "TestPorts.h"
#include <catch.hpp>
#include <opendatacon/IOTypes.h>

class DataConcentrator //because DataConcentrator is a freind class of DataConnector
{
public:
	DataConcentrator(DataConnector& Con)
	{
		auto tx_cleanup = [=](Transform* tx)
					{
						delete tx;
					};
		Json::Value Tx2conf; Tx2conf["Add"]=2;
		Json::Value Tx3conf; Tx3conf["Add"]=3;
		Con.SenderTransforms["P1"].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new AddShiftTransform("Tx1",Json::Value::nullSingleton()),tx_cleanup));
		Con.SenderTransforms["P1"].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new AddShiftTransform("Tx2",Tx2conf),tx_cleanup));
		Con.SenderTransforms["P1"].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new AddShiftTransform("Tx3",Tx3conf),tx_cleanup));
	}
};

using namespace odc;

TEST_CASE("TransformTests")
{
	TestSetup();

	auto ios = odc::asio_service::Get();
	auto work = ios->make_work();

	PublicPublishPort P1("P1","",Json::Value::nullSingleton());
	AnalogCheckPort P2("P2","",Json::Value::nullSingleton(),1230);
	AnalogCheckPort P3("P3","",Json::Value::nullSingleton(),1230);
	AnalogCheckPort P4("P4","",Json::Value::nullSingleton(),1230);
	AnalogCheckPort P5("P5","",Json::Value::nullSingleton(),1230);

	Json::Value ConnConf;
	ConnConf["Connections"][0]["Name"] = "P1toP2";
	ConnConf["Connections"][0]["Port1"] = "P1";
	ConnConf["Connections"][0]["Port2"] = "P2";
	ConnConf["Connections"][1]["Name"] = "P1toP3";
	ConnConf["Connections"][1]["Port1"] = "P1";
	ConnConf["Connections"][1]["Port2"] = "P3";
	ConnConf["Connections"][2]["Name"] = "P1toP4";
	ConnConf["Connections"][2]["Port1"] = "P1";
	ConnConf["Connections"][2]["Port2"] = "P4";
	ConnConf["Connections"][3]["Name"] = "P1toP5";
	ConnConf["Connections"][3]["Port1"] = "P1";
	ConnConf["Connections"][3]["Port2"] = "P5";

	DataConnector Conn("Conn","",ConnConf);

	DataConcentrator DC(Conn);

	Conn.Enable();

	std::atomic_bool executed(false);
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&cb_status,&executed](CommandStatus status)
		{
			cb_status = status;
			executed = true;
		});

	auto event = std::make_shared<EventInfo>(EventType::Analog);
	event->SetPayload<EventType::Analog>(0);

	executed = false;
	P1.PublicPublishEvent(event,StatusCallback);
	while(!executed)
	{
		ios->run_one_for(std::chrono::milliseconds(10));
	}
	REQUIRE(cb_status == CommandStatus::SUCCESS);

	TestTearDown();
}
