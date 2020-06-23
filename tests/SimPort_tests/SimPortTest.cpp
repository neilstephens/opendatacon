/*	opendatacon
*
*	Copyright (c) 2018:
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
* SimTest.cpp
*
*  Created on: 22/04/2020
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#include "../PortLoader.h"
#include <catch.hpp>
#include <opendatacon/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <array>
#include <sstream>

#define SUITE(name) "SimTests - " name

inline Json::Value GetTestConfigJSON()
{
	// We actually have the conf file here to match the tests it is used in below.
	static const char* conf = R"001(
	{
		"HttpIP" : "0.0.0.0",
		"HttpPort" : 9000,
		"Version" : "Dummy Version 2-3-2020",

		//-------Point conf--------#
		"Binaries" :
		[
			{"Index": 0},{"Index": 1},{"Index": 5},{"Index": 6},{"Index": 7},{"Index": 8},{"Index": 10},{"Index": 11},{"Index": 12},{"Index": 13},{"Index": 14},{"Index": 15}
		],

		"Analogs" :
		[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5}
			//,{"Index" : 6, "SQLite3" : { "File" : "test2.db", "Query" : "select timestamp,(value+:INDEX) from events", "TimestampHandling" : "RELATIVE_TOD_FASTFORWARD"}}
		],

		"BinaryControls" :
		[
			{
				"Index" : 0,
				"FeedbackBinaries":
				[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			},
			{
				"Index" : 1,
				"FeedbackBinaries":
				[
					{"Index":5,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":6,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			},
			{
				"Index" : 2,
				"FeedbackPosition": {"Type": "Analog", "Index" : 7, "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}
			},
			{
				"Index" : 3,
				"FeedbackPosition": {"Type": "Analog", "Index" : 7,"FeedbackMode":"PULSE", "Action":"DOWN", "Limit":0}
			},
			{
				"Index" : 4,
				"FeedbackPosition": {"Type": "Binary", "Indexes" : "10,11,12", "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}
			},
			{
				"Index" : 5,
				"FeedbackPosition": {"Type": "Binary", "Indexes" : "10,11,12","FeedbackMode":"PULSE", "Action":"DOWN", "Limit":0}
			},
			{
				"Index" : 6,
				"FeedbackPosition":	{ "Type": "BCD", "Indexes" : "10,11,12", "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}
			},
			{
				"Index" : 7,
				"FeedbackPosition": {"Type": "BCD", "Indexes" : "10,11,12","FeedbackMode":"PULSE", "Action":"DOWN", "Limit":0}
			}
		]
	})001";

	std::istringstream iss(conf);
	Json::CharReaderBuilder JSONReader;
	std::string err_str;
	Json::Value json_conf;
	bool parse_success = Json::parseFromStream(JSONReader,iss, &json_conf, &err_str);
	if (!parse_success)
	{
		FAIL("Failed to parse configuration : " + err_str);
	}
	return json_conf;
}

std::shared_ptr<odc::asio_service> TestSetup(spdlog::level::level_enum loglevel)
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("SimPort", console_sink);
	pLibLogger->set_level(loglevel);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pODCLogger->set_level(loglevel);
	odc::spdlog_register_logger(pODCLogger);

	InitLibaryLoading();

	return odc::asio_service::Get();
}

void TestTearDown()
{
	odc::spdlog_drop_all(); // Close off everything
}

TEST_CASE("TestConfigLoad")
{
	auto portlib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(portlib);

	//scope for port, ios lifetime
	{
		auto IOS = TestSetup(spdlog::level::level_enum::warn);
		newptr newSim = GetPortCreator(portlib, "Sim");
		REQUIRE(newSim);
		delptr deleteSim = GetPortDestroyer(portlib, "Sim");
		REQUIRE(deleteSim);

		auto SimPort1 = std::shared_ptr<DataPort>(newSim("OutstationUnderTest", "", GetTestConfigJSON()), deleteSim);

		SimPort1->Build();
		SimPort1->Enable();

		// Set up a callback for the result
		std::atomic_bool executed(false);
		CommandStatus cb_status;
		auto pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&](CommandStatus status)
			{
				cb_status = status;
				executed = true;
			});


		EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
		auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, 1);
		event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

		SimPort1->Event(event, "TestHarness", pStatusCallback);

		while(!executed)
		{
			IOS->run_one();
		}

		REQUIRE(cb_status == CommandStatus::SUCCESS);
	}

	UnLoadModule(portlib);
	TestTearDown();
}
