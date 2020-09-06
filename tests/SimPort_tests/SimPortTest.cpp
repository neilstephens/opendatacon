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
#include <random>
#include <sstream>

#define SUITE(name) "SimTests - " name

const int START = 0;
const int FINISH = 100;
const std::vector<ControlCode> LATCH_ON_CODES = {ControlCode::LATCH_ON, ControlCode::CLOSE_PULSE_ON, ControlCode::PULSE_ON};
const std::vector<ControlCode> LATCH_OFF_CODES = {ControlCode::LATCH_OFF, ControlCode::TRIP_PULSE_ON, ControlCode::PULSE_OFF};
const std::vector<std::size_t> ANALOG_INDEXES = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 110, 120, 1110, 1293, 119201, 118281, 1782718, 19281919};

/*
  function     : GetTestConfigJSON
  description  : this function will return the json configuration for sim port
                 ; as it is a test we have a constant configuration for sim port
  param        : NA
  return       : Json::Value the jason configuration of test sim port
*/
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
			{"Index": 0},
            {"Index": 1},
            {"Index": 5},
            {"Index": 6},
            {"Index": 7},
            {"Index": 8},
            {"Index": 10, "StartVal" : false},
            {"Index": 11, "StartVal" : false},
            {"Index": 12, "StartVal" : true},
            {"Index": 13, "StartVal" : false},
            {"Index": 14, "StartVal" : true},
            {"Index": 15}
		],

		"Analogs" :
		[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 8, "StartVal" : 240, "UpdateIntervalms" : 10000 ,"StdDev" : 5},
			{"Index" : 9, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 10, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 110, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 120, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 1110, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 1293, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 119201, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 118281, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 1782718, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
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
            // this is the testing for DNP3
			{
				"Index" : 2,
				"FeedbackPosition": {"Type": "Analog", "Index" : 7, "OnAction":"RAISE", "RaiseLimit":10}
			},
			{
				"Index" : 3,
				"FeedbackPosition": {"Type": "Analog", "Index" : 7, "OnAction":"LOWER", "LowerLimit":0}
			},
			{
				"Index" : 4,
				"FeedbackPosition": {"Type": "Binary", "Indexes" : [11,12,13,14], "OnAction":"RAISE", "RaiseLimit":10}
			},
			{
				"Index" : 5,
				"FeedbackPosition": {"Type": "Binary", "Indexes" : [11,12,13,14], "OnAction":"LOWER", "LowerLimit":0}
			},
			{
				"Index" : 6,
				"FeedbackPosition":	{ "Type": "BCD", "Indexes" : [10,11,12,13,14], "OnAction":"RAISE", "RaiseLimit":10}
			},
			{
				"Index" : 7,
				"FeedbackPosition": {"Type": "BCD", "Indexes" : [10,11,12,13,14], "OnAction":"LOWER", "LowerLimit":0}
			},
            // this is the testing for Conitel
			{
				"Index" : 8,
				"FeedbackPosition": {
                                       "Type": "Analog",
                                       "Index" : 7,
                                       "OnAction":"RAISE",
                                       "OffAction":"LOWER",
                                       "RaiseLimit":10,
                                       "LowerLimit":0}
			},
			{
				"Index" : 9,
				"FeedbackPosition": {
                                       "Type": "Binary",
                                       "Indexes" : [11,12,13,14],
                                       "OnAction":"RAISE",
                                       "OffAction":"LOWER",
                                       "RaiseLimit":10,
                                       "LowerLimit":0}
			},
			{
				"Index" : 10,
				"FeedbackPosition":	{
                                       "Type": "BCD",
                                       "Indexes" : [10,11,12,13,14],
                                       "OnAction":"RAISE",
                                       "OffAction":"LOWER",
                                       "RaiseLimit":10,
                                       "LowerLimit":0}
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

/*
  function     : TestSetup
  description  : this function is responsible for starting up the opendatacon
                 routines like logs, libraries etc.
  param        : loglevel, log level
  return       : void
*/
inline void TestSetup(spdlog::level::level_enum loglevel)
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("SimPort", console_sink);
	pLibLogger->set_level(loglevel);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pODCLogger->set_level(loglevel);
	odc::spdlog_register_logger(pODCLogger);

	static std::once_flag once_flag;
	std::call_once(once_flag,[]()
		{
			InitLibaryLoading();
		});
}

/*
  function     : TestTearDown
  description  : this function tests the closing down of opendatacon routines like logs
  param        : NA
  return       : void
*/
inline void TestTearDown()
{
	odc::spdlog_drop_all(); // Close off everything
}

/*
  function     : BuildParams
  description  : this function build the param collection
  param        : a, param[0]
  param        : b, param[1]
  param        : c, param[2]
  return       : ParamCollection
*/
inline ParamCollection BuildParams(const std::string& a,
	const std::string& b,
	const std::string& c)
{
	ParamCollection params;
	params["0"] = a;
	params["1"] = b;
	params["2"] = c;
	params["Target"] = "OutstationUnderTest";
	return params;
}

/*
  function     : SendEvent
  description  : this function will send the event to simulator
  param        : code, control code of the event
  param        : index, index of the binary control
  param        : sim_port, pointer to the simulator port
  param        : status, event response from the simulator
  return       : void
*/
inline void SendEvent(ControlCode code, std::size_t index, std::shared_ptr<DataPort> sim_port, CommandStatus status)
{
	auto IOS = odc::asio_service::Get();
	// Set up a callback for the result
	std::atomic_bool executed(false);
	CommandStatus cb_status;
	auto pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&cb_status,&executed](CommandStatus status)
		{
			cb_status = status;
			executed = true;
		});

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = code;
	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, index);
	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	sim_port->Event(event, "TestHarness", pStatusCallback);
	while(!executed)
	{
		IOS->run_one();
	}
	REQUIRE(cb_status == status);
}

/*
  function     : GetBinaryEncodedString
  description  : this function will return the binary encoded string from
                 getting the binary values from simulator
  param        : indexes, indexes of the binary control
  param        : sim_port, pointer to the simulator port
  return       : binary encoded string
*/
inline std::string GetBinaryEncodedString(const std::vector<std::size_t>& indexes, const std::shared_ptr<DataPort>& sim_port)
{
	std::string binary;
	for (std::size_t index : indexes)
		binary += sim_port->GetCurrentState()["BinaryCurrent"][std::to_string(index)].asString();
	return binary;
}

/*
  function     : GetBCDEncodedString
  description  : this function will return the BCD encoded string from
                 getting the binary values from simulator
  param        : indexes, indexes of the binary control
  param        : sim_port, pointer to the simulator port
  return       : binary encoded string
*/
std::size_t GetBCDEncodedString(const std::vector<std::size_t>& indexes, const std::shared_ptr<DataPort>& sim_port)
{
	std::string bcd_str;
	for (std::size_t index : indexes)
		bcd_str += sim_port->GetCurrentState()["BinaryCurrent"][std::to_string(index)].asString();
	return odc::bcd_encoded_to_decimal(bcd_str);
}

/*
  function     : RandomNumber
  description  : this function generate a random number between start and end both inclusive
  param        : s, start of the set limit
  param        : e, end of the set limit
  return       : random number
*/
inline int RandomNumber()
{
	std::random_device rd;
	std::uniform_int_distribution<> dt(START, FINISH);
	return dt(rd);
}

/*
  function     : TEST_CASE
  description  : tests loading and creation of the sim port
  param        : TestConfigLoad, name of the test case
  return       : NA
*/
TEST_CASE("TestConfigLoad")
{
	TestSetup(spdlog::level::level_enum::warn);

	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();
		const bool result = sim_port->GetCurrentState()["AnalogCurrent"].isMember("0");
		REQUIRE(result == true);
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests forcing a data point
  param        : TestForcedPoint, name of the test case
  return       : NA
*/
TEST_CASE("TestForcedPoint")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);

		sim_port->Build();
		sim_port->Enable();

		std::shared_ptr<IUIResponder> resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", "0", "12345.6789");
		Json::Value value = resp->ExecuteCommand("ForcePoint", params);
		REQUIRE(value["RESULT"].asString() == "Success");
		REQUIRE(sim_port->GetCurrentState()["AnalogCurrent"]["0"] == "12345.678900");
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests releaseing a data point
  param        : TestReleasePoint, name of the test case
  return       : NA
*/
TEST_CASE("TestReleasePoint")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);

		sim_port->Build();
		sim_port->Enable();

		std::shared_ptr<IUIResponder> resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", "0", "");
		Json::Value value = resp->ExecuteCommand("ReleasePoint", params);
		REQUIRE(value["RESULT"].asString() == "Success");
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests sending an event to all analog points
  param        : TestAnalogEventToAll
  return       : NA
*/
TEST_CASE("TestAnalogEventToAll")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);

		sim_port->Build();
		sim_port->Enable();

		std::shared_ptr<IUIResponder> resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", ".*", "12345.6789");
		Json::Value value = resp->ExecuteCommand("SendEvent", params);
		REQUIRE(value["RESULT"].asString() == "Success");
		for (std::size_t index : ANALOG_INDEXES)
			REQUIRE(sim_port->GetCurrentState()["AnalogCurrent"][std::to_string(index)] == "12345.678900");
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests latch on
  param        : TestLatchOn, name of the test case
  return       : NA
*/
TEST_CASE("TestLatchOn")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["BinaryCurrent"]["0"].asString();
		REQUIRE(result == "0");
		SendEvent(ControlCode::LATCH_ON, 0, sim_port, CommandStatus::SUCCESS);
		result = sim_port->GetCurrentState()["BinaryCurrent"]["0"].asString();
		REQUIRE(result == "1");
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests latch off
  param        : TestLatchOff, name of the test case
  return       : NA
*/
TEST_CASE("TestLatchOff")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["BinaryCurrent"]["0"].asString();
		REQUIRE(result == "0");
		SendEvent(ControlCode::TRIP_PULSE_ON, 0, sim_port, CommandStatus::SUCCESS);
		result = sim_port->GetCurrentState()["BinaryCurrent"]["0"].asString();
		REQUIRE(result == "0");
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*-------------------------------------------------------------------------------
 *
 *                            DNP3 tests
 *
 *-------------------------------------------------------------------------------*/

/*
  function     : TEST_CASE
  description  : tests tap changer raise for analog types
  param        : DNP3TestAnalogTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestAnalogTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString();
		REQUIRE(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 2, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 2, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(10 == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for analog type
  param        : DNP3TestAnalogTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestAnalogTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString();
		REQUIRE(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 3, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 3, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(0 == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for binary type
  param        : DNP3TestBinaryTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestBinaryTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {11, 12, 13, 14};
		std::string binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(5 == odc::to_decimal(binary));

		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 4, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			REQUIRE(i == static_cast<int>(odc::to_decimal(binary)));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 4, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(10 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for binary type
  param        : DNP3TestBinaryTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestBinaryTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {11, 12, 13, 14};
		std::string binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(5 == odc::to_decimal(binary));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 5, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			REQUIRE(i == static_cast<int>(odc::to_decimal(binary)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 5, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(0 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for BCD type
  param        : DNP3TestBCDTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestBCDTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {10, 11, 12, 13, 14};
		REQUIRE(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 6, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 6, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(10 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for BCD type
  param        : DNP3TestBCDTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("DNP3TestBCDTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {10, 11, 12, 13, 14};
		REQUIRE(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 7, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 7, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(0 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*-------------------------------------------------------------------------------
 *
 *                            Conitel tests
 *
 *-------------------------------------------------------------------------------*/

/*
  function     : TEST_CASE
  description  : tests tap changer raise for analog types
  param        : ConitelTestAnalogTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestAnalogTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString();
		REQUIRE(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 8, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 8, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(10 == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for analog type
  param        : ConitelTestAnalogTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestAnalogTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		std::string result = sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString();
		REQUIRE(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 8, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 8, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(0 == std::stoi(sim_port->GetCurrentState()["AnalogCurrent"]["7"].asString()));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for binary type
  param        : ConitelTestBinaryTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestBinaryTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {11, 12, 13, 14};
		std::string binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(5 == odc::to_decimal(binary));

		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 9, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			REQUIRE(i == static_cast<int>(odc::to_decimal(binary)));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 9, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(10 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for binary type
  param        : ConitelTestBinaryTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestBinaryTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {11, 12, 13, 14};
		std::string binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(5 == odc::to_decimal(binary));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 9, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			REQUIRE(i == static_cast<int>(odc::to_decimal(binary)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 9, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		REQUIRE(0 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for BCD type
  param        : ConitelBCDTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestBCDTapChangerRaise")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {10, 11, 12, 13, 14};
		REQUIRE(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 10, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 10, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(10 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_ON_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for BCD type
  param        : ConitelTestBCDTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("ConitelTestBCDTapChangerLower")
{
	//Load the library
	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	//scope for port, ios lifetime
	{
		auto IOS = odc::asio_service::Get();
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", GetTestConfigJSON()), delete_sim);
		sim_port->Build();
		sim_port->Enable();

		const std::vector<std::size_t> indexes = {10, 11, 12, 13, 14};
		REQUIRE(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 10, sim_port, CommandStatus::SUCCESS);
			REQUIRE(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 10, sim_port, CommandStatus::OUT_OF_RANGE);
		REQUIRE(0 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(LATCH_OFF_CODES[RandomNumber() % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

