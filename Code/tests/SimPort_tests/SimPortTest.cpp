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
#include "../ThreadPool.h"
#include <catch.hpp>
#include <opendatacon/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <array>
#include <random>
#include <sstream>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <set>
#include <algorithm>
#include <sqlite3.h>

#define SUITE(name) "SimTests - " name

//TODO: Add TimeSync event handling tests

const std::vector<ControlCode> TRIP_CODES = {ControlCode::CLOSE_PULSE_ON, ControlCode::TRIP_PULSE_ON};
const std::vector<std::vector<ControlCode>> CODES = {
	{ControlCode::LATCH_OFF, ControlCode::TRIP_PULSE_ON, ControlCode::PULSE_OFF},
	{ControlCode::LATCH_ON, ControlCode::CLOSE_PULSE_ON, ControlCode::PULSE_ON}
};
const std::vector<std::size_t> ANALOG_INDEXES = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 110, 120, 1110, 1293, 119201, 118281, 1782718, 19281919};
const std::vector<std::size_t> BINARY_INDEXES = {0, 1, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};


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
		//"HttpPort" : 9000,
		//"Version" : "Dummy Version 2-3-2020",

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
            {"Index": 15},
			{"Index": 20},
            {"Index": 21}
		],

		"Analogs" :
		[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 17, "StartVal" : 9, "StdDev" : 0}, //this is the tap position feedback
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
		// this is the testing for 'activation model' controls. AKA Pulse
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
		// this is the testing for 'complimentary latch model' controls. AKA Trip/Close Latch On/Off
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
			},
			{
				"Index" : 11,
				"FeedbackPosition":	{
                                       "Type": "BCD",
                                       "Indexes" : [10,11,12,13,14],
                                       "OnAction":"LOWER",
                                       "OffAction":"RAISE",
                                       "RaiseLimit":10,
                                       "LowerLimit":0}
            },
            {
                "Index" : 12
            },
			{
				"Index" : 20,
				"FeedbackPosition": {"Type": "Analog", "Index" : 17, "OnAction":"RAISE", "RaiseLimit": 40, "Step": 2}
			},
			{
				"Index" : 21,
				"FeedbackPosition": {"Type": "Analog", "Index" : 17, "OnAction":"LOWER", "LowerLimit": 0, "Step": 2}
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
  return       : void
*/
extern spdlog::level::level_enum log_level;
inline void TestSetup()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("SimPort", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pODCLogger->set_level(log_level);
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
	odc::spdlog_flush_all();
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
		IOS->run_one_for(std::chrono::milliseconds(10));
	}
	CHECK(cb_status == status);
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
		binary += sim_port->GetCurrentState()["BinaryPayload"][std::to_string(index)].asString();
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
		bcd_str += sim_port->GetCurrentState()["BinaryPayload"][std::to_string(index)].asString();
	return odc::bcd_encoded_to_decimal(bcd_str);
}

/*
  function     : RandomNumber
  description  : this function generate a random number between start and end both inclusive
  param        : s, start of the set limit
  param        : e, end of the set limit
  return       : random number
*/
inline int RandomNumber(int s, int e)
{
	std::random_device rd;
	std::uniform_int_distribution<> dt(s, e);
	return dt(rd);
}

/*
  function     : SQLiteTestDBPath
  description  : this function returns a fresh path (in a temp dir) for a SQLite test DB
  param        : name, a name unique to the calling test/point
  return       : std::string, full path to the DB file
*/
inline std::string SQLiteTestDBPath(const std::string& name)
{
	auto dir = std::filesystem::temp_directory_path() / "odc_simport_tests";
	std::filesystem::create_directories(dir);
	return (dir / (name + ".db")).string();
}

/*
  function     : CreateSQLitePlaybackDB
  description  : this function creates (or overwrites) a SQLite DB with an "events" table
                 (timestamp INTEGER, value REAL, idx INTEGER) used to drive SimPort's
                 SQLite3 playback feature
  param        : path, file path for the DB
  param        : rows, (timestamp_ms, value, idx) tuples to insert
  return       : void
*/
inline void CreateSQLitePlaybackDB(const std::string& path, const std::vector<std::tuple<int64_t, double, int>>& rows)
{
	std::filesystem::remove(path);

	sqlite3* db = nullptr;
	REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "CREATE TABLE events(timestamp INTEGER, value REAL, idx INTEGER)", nullptr, nullptr, nullptr) == SQLITE_OK);

	sqlite3_stmt* stmt = nullptr;
	REQUIRE(sqlite3_prepare_v2(db, "INSERT INTO events VALUES (?,?,?)", -1, &stmt, nullptr) == SQLITE_OK);
	for (const auto& [ts, val, idx] : rows)
	{
		sqlite3_bind_int64(stmt, 1, ts);
		sqlite3_bind_double(stmt, 2, val);
		sqlite3_bind_int(stmt, 3, idx);
		REQUIRE(sqlite3_step(stmt) == SQLITE_DONE);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

/*
  function     : MakeSQLitePoint
  description  : this function builds the JSON for a single SQLite3-backed point
  param        : index, point index
  param        : file, SQLite DB file path
  param        : query, SQL query (2 columns: timestamp, value)
  param        : timestamp_handling, one of the SimPort TimestampHandling modes
  param        : start_val, the point's StartVal (so playback can be distinguished from it)
  return       : Json::Value, the point config
*/
inline Json::Value MakeSQLitePoint(std::size_t index, const std::string& file, const std::string& query,
	const std::string& timestamp_handling, double start_val = 0.0)
{
	Json::Value point(Json::objectValue);
	point["Index"] = static_cast<Json::UInt64>(index);
	point["StartVal"] = start_val;
	point["SQLite3"]["File"] = file;
	point["SQLite3"]["Query"] = query;
	point["SQLite3"]["TimestampHandling"] = timestamp_handling;
	return point;
}

/*
  function     : PollPointValues
  description  : this function samples a point's current value at regular intervals over a
                 duration, and returns the set of distinct values observed
  param        : sim_port, the port to sample
  param        : payload_key, "AnalogPayload" or "BinaryPayload"
  param        : index, point index
  param        : duration, how long to sample for
  param        : interval, delay between samples
  return       : std::set<double>, the distinct values observed
*/
inline std::set<double> PollPointValues(const std::shared_ptr<DataPort>& sim_port, const std::string& payload_key,
	std::size_t index, std::chrono::milliseconds duration, std::chrono::milliseconds interval = std::chrono::milliseconds(10))
{
	std::set<double> seen;
	auto end = std::chrono::steady_clock::now() + duration;
	do
	{
		seen.insert(std::stod(sim_port->GetCurrentState()[payload_key][std::to_string(index)].asString()));
		std::this_thread::sleep_for(interval);
	}
	while(std::chrono::steady_clock::now() < end);
	return seen;
}

/*
  function     : ContainsAll
  description  : this function checks every value in 'expected' was observed in 'seen' -
                 used instead of exact equality since polling can also catch transient
                 startup values, or miss very short-lived intermediate states
  param        : seen, the observed set
  param        : expected, the values that must all be present
  return       : bool
*/
inline bool ContainsAll(const std::set<double>& seen, const std::set<double>& expected)
{
	for(auto v : expected)
		if(seen.find(v) == seen.end())
			return false;
	return true;
}

/*
  function     : PollPointTransitions
  description  : this function samples a point's current value at regular intervals, and
                 returns the de-duplicated sequence of values seen (consecutive repeats
                 collapsed). Useful for proving wraparound occurred (a value re-appearing
                 after others were seen), where the last row before a wrap can be too
                 short-lived for plain snapshot sampling to reliably catch
  param        : sim_port, the port to sample
  param        : payload_key, "AnalogPayload" or "BinaryPayload"
  param        : index, point index
  param        : duration, how long to sample for
  param        : interval, delay between samples
  return       : std::vector<double>, the de-duplicated sequence of values observed
*/
inline std::vector<double> PollPointTransitions(const std::shared_ptr<DataPort>& sim_port, const std::string& payload_key,
	std::size_t index, std::chrono::milliseconds duration, std::chrono::milliseconds interval = std::chrono::milliseconds(10))
{
	std::vector<double> transitions;
	auto end = std::chrono::steady_clock::now() + duration;
	do
	{
		double v = std::stod(sim_port->GetCurrentState()[payload_key][std::to_string(index)].asString());
		if(transitions.empty() || transitions.back() != v)
			transitions.push_back(v);
		std::this_thread::sleep_for(interval);
	}
	while(std::chrono::steady_clock::now() < end);
	return transitions;
}

/*
  function     : TEST_CASE
  description  : tests loading and creation of the sim port
  param        : TestConfigLoad, name of the test case
  return       : NA
*/
TEST_CASE("TestConfigLoad")
{
	TestSetup();

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
		const bool result = sim_port->GetCurrentState()["AnalogPayload"].isMember("0");
		CHECK(result == true);
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
	TestSetup();
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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", "0", "12345.6789");
		Json::Value value = resp->ExecuteCommand("ForcePoint", params);
		CHECK(value["RESULT"].asString() == "Success");
		CHECK(sim_port->GetCurrentState()["AnalogPayload"]["0"] == "12345.678900");
		sim_port->Disable();
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
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", "0", "");
		Json::Value value = resp->ExecuteCommand("ReleasePoint", params);
		CHECK(value["RESULT"].asString() == "Success");
		sim_port->Disable();
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
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Analog", ".*", "12345.6789");
		Json::Value value = resp->ExecuteCommand("ForcePoint", params);
		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : ANALOG_INDEXES)
			CHECK(sim_port->GetCurrentState()["AnalogPayload"][std::to_string(index)] == "12345.678900");
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests sending an event to all binary points
  param        : TestBinaryEventToAll
  return       : NA
*/
TEST_CASE("TestBinaryEventToAll")
{
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Binary", ".*", "1");
		Json::Value value = resp->ExecuteCommand("ForcePoint", params);
		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : BINARY_INDEXES)
			CHECK(sim_port->GetCurrentState()["BinaryPayload"][std::to_string(index)] == "1");
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests binary event quality
  param        : TestBinaryEventQuality
  return       : NA
*/
TEST_CASE("TestBinaryEventQuality")
{
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Binary", ".*", "1");
		Json::Value value = resp->ExecuteCommand("SendEvent", params);
		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : BINARY_INDEXES)
			CHECK(sim_port->GetCurrentState()["BinaryQuality"][std::to_string(index)] == "|ONLINE|");
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests analog event quality
  param        : TestAnalogEventQuality
  return       : NA
*/
TEST_CASE("TestAnalogEventQuality")
{
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());
		const ParamCollection params = BuildParams("Binary", ".*", "1");
		Json::Value value = resp->ExecuteCommand("SendEvent", params);
		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : ANALOG_INDEXES)
			CHECK(sim_port->GetCurrentState()["AnalogQuality"][std::to_string(index)] == "|ONLINE|");
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests binary event timestamp
  param        : TestBinaryEventTimestamp
  return       : NA
*/
TEST_CASE("TestBinaryEventTimestamp")
{
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());

		char buf[64] = {0};
		const std::time_t before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&before));
		const std::string before_time(buf);

		const ParamCollection params = BuildParams("Binary", ".*", "1");
		Json::Value value = resp->ExecuteCommand("SendEvent", params);

		const std::time_t after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&after));
		const std::string after_time(buf);

		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : BINARY_INDEXES)
		{
			const std::string dt = sim_port->GetCurrentState()["BinaryTimestamp"][std::to_string(index)].asString();
			auto evt_time = dt.substr(0, before_time.size());
			CAPTURE(before_time,evt_time,after_time);
			CHECK((evt_time == before_time || evt_time == after_time));
		}
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests analog event timestamp
  param        : TestAnalogEventTimestamp
  return       : NA
*/
TEST_CASE("TestAnalogEventTimestamp")
{
	TestSetup();

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

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		//give some time for PortUp() to finish
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		const IUIResponder* resp = std::get<1>(sim_port->GetUIResponder());

		char buf[64] = {0};
		const std::time_t before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&before));
		const std::string before_time(buf);

		const ParamCollection params = BuildParams("Analog", ".*", "123.4567");
		Json::Value value = resp->ExecuteCommand("SendEvent", params);

		const std::time_t after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&after));
		const std::string after_time(buf);

		CHECK(value["RESULT"].asString() == "Success");
		for (std::size_t index : ANALOG_INDEXES)
		{
			const std::string dt = sim_port->GetCurrentState()["AnalogTimestamp"][std::to_string(index)].asString();
			auto evt_time = dt.substr(0, before_time.size());
			CAPTURE(before_time,evt_time,after_time);
			CHECK((evt_time == before_time || evt_time == after_time));
		}
		sim_port->Disable();
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
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["BinaryPayload"]["0"].asString();
		CHECK(result == "0");
		SendEvent(ControlCode::LATCH_ON, 0, sim_port, CommandStatus::SUCCESS);
		result = sim_port->GetCurrentState()["BinaryPayload"]["0"].asString();
		CHECK(result == "1");
		sim_port->Disable();
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
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["BinaryPayload"]["0"].asString();
		CHECK(result == "0");
		SendEvent(ControlCode::TRIP_PULSE_ON, 0, sim_port, CommandStatus::SUCCESS);
		result = sim_port->GetCurrentState()["BinaryPayload"]["0"].asString();
		CHECK(result == "0");
		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*-------------------------------------------------------------------------------
 *
 *                            Pulse tests
 *
 *-------------------------------------------------------------------------------*/

/*
  function     : TEST_CASE
  description  : tests tap changer raise for analog types
  param        : PulseTestAnalogTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestAnalogTapChangerRaise")
{
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["AnalogPayload"]["7"].asString();
		CHECK(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 2, sim_port, CommandStatus::SUCCESS);
			CHECK(i == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 2, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(10 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for analog type
  param        : PulseTestAnalogTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestAnalogTapChangerLower")
{
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["AnalogPayload"]["7"].asString();
		CHECK(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 3, sim_port, CommandStatus::SUCCESS);
			CHECK(i == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 3, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(0 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for analog type
  param        : PulseTestAnalogTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestAnalogTapChangerRandom")
{
	TestSetup();

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

		int tap_position = std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString());
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) --tap_position;
			else ++tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(ControlCode::UNDEFINED, 2 + index, sim_port, status);
			CHECK(tap_position == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}


/*
  function     : TEST_CASE
  description  : tests tap changer raise for binary type
  param        : PulseTestBinaryTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBinaryTapChangerRaise")
{
	TestSetup();

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
		CHECK(5 == odc::to_decimal(binary));

		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 4, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(i == static_cast<int>(odc::to_decimal(binary)));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 4, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		CHECK(10 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for binary type
  param        : PulseTestBinaryTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBinaryTapChangerLower")
{
	TestSetup();

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
		CHECK(5 == odc::to_decimal(binary));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 5, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(i == static_cast<int>(odc::to_decimal(binary)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 5, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		CHECK(0 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for binary type
  param        : PulseTestBinaryTapChangerRandom, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBinaryTapChangerRandom")
{
	TestSetup();

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
		int tap_position = odc::to_decimal(binary);
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) --tap_position;
			else ++tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(ControlCode::UNDEFINED, 4 + index, sim_port, status);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(tap_position == static_cast<int>(odc::to_decimal(binary)));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for BCD type
  param        : PulseTestBCDTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBCDTapChangerRaise")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(ControlCode::UNDEFINED, 6, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 6, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(10 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for BCD type
  param        : PulseTestBCDTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBCDTapChangerLower")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(ControlCode::UNDEFINED, 7, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(ControlCode::UNDEFINED, 7, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(0 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(ControlCode::UNDEFINED, 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for BCD type
  param        : PulseTestBCDTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("PulseTestBCDTapChangerRandom")
{
	TestSetup();

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
		int tap_position = GetBCDEncodedString(indexes, sim_port);
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) --tap_position;
			else ++tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(ControlCode::UNDEFINED, 6 + index, sim_port, status);
			CHECK(tap_position == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*-------------------------------------------------------------------------------
 *
 *                            OnOff tests
 *
 *-------------------------------------------------------------------------------*/

/*
  function     : TEST_CASE
  description  : tests tap changer raise for analog types
  param        : OnOffTestAnalogTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestAnalogTapChangerRaise")
{
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["AnalogPayload"]["7"].asString();
		CHECK(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(CODES[1][RandomNumber(0, 999) % 3], 8, sim_port, CommandStatus::SUCCESS);
			CHECK(i == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 8, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(10 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for analog type
  param        : OnOffTestAnalogTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestAnalogTapChangerLower")
{
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["AnalogPayload"]["7"].asString();
		CHECK(result == "5.000000");
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(CODES[0][RandomNumber(0, 999) % 3], 8, sim_port, CommandStatus::SUCCESS);
			CHECK(i == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 8, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(0 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

TEST_CASE("OnOffTestAnalogTapChangerStepRaiseLower")
{
	TestSetup();

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

		std::string result = sim_port->GetCurrentState()["AnalogPayload"]["17"].asString();
		CHECK(result == "9.000000");
		/*
		  As we know the index 7 tap changer's default position is 9, step is 2
		  Raise -> 11
		*/
		SendEvent(CODES[1][0], 20, sim_port, CommandStatus::SUCCESS);
		CHECK(11 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["17"].asString()));

		SendEvent(CODES[1][0], 20, sim_port, CommandStatus::SUCCESS);
		CHECK(13 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["17"].asString()));

		SendEvent(CODES[1][0], 21, sim_port, CommandStatus::SUCCESS);
		CHECK(11 == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["17"].asString()));
	}
	UnLoadModule(port_lib);
	TestTearDown();
}
/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for analog type
  param        : OnOffTestAnalogTapChangerRandom, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestAnalogTapChangerRandom")
{
	TestSetup();

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

		int tap_position = std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString());
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) ++tap_position;
			else --tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(CODES[index][RandomNumber(0, 999) % 3], 8, sim_port, status);
			CHECK(tap_position == std::stoi(sim_port->GetCurrentState()["AnalogPayload"]["7"].asString()));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for binary type
  param        : OnOffTestBinaryTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBinaryTapChangerRaise")
{
	TestSetup();

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
		CHECK(5 == odc::to_decimal(binary));

		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(CODES[1][RandomNumber(0, 999) % 3], 9, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(i == static_cast<int>(odc::to_decimal(binary)));
		}

		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the upper limit mark
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 9, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		CHECK(10 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for binary type
  param        : OnOffTestBinaryTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBinaryTapChangerLower")
{
	TestSetup();

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
		CHECK(5 == odc::to_decimal(binary));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(CODES[0][RandomNumber(0, 999) % 3], 9, sim_port, CommandStatus::SUCCESS);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(i == static_cast<int>(odc::to_decimal(binary)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 9, sim_port, CommandStatus::OUT_OF_RANGE);
		binary = GetBinaryEncodedString(indexes, sim_port);
		CHECK(0 == odc::to_decimal(binary));

		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for binary type
  param        : OnOffTestBinaryTapChangerRandom, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBinaryTapChangerRandom")
{
	TestSetup();

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
		int tap_position = odc::to_decimal(binary);
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) ++tap_position;
			else --tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(CODES[index][RandomNumber(0, 999) % 3], 9, sim_port, status);
			binary = GetBinaryEncodedString(indexes, sim_port);
			CHECK(tap_position == static_cast<int>(odc::to_decimal(binary)));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for BCD type
  param        : OnOffBCDTapChangerRaise, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBCDTapChangerRaise")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(CODES[1][RandomNumber(0, 999) % 3], 10, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 10, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(10 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[1][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for BCD type
  param        : OnOffTestBCDTapChangerLower, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBCDTapChangerLower")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(CODES[0][RandomNumber(0, 999) % 3], 10, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 10, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(0 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(CODES[0][RandomNumber(0, 999) % 3], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for BCD type
  param        : OnOffTestBCDTapChangerRandom, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBCDTapChangerRandom")
{
	TestSetup();

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
		int tap_position = GetBCDEncodedString(indexes, sim_port);
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 1000) % 2;
			if (index) ++tap_position;
			else --tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(CODES[index][RandomNumber(0, 999) % 3], 10, sim_port, status);
			CHECK(tap_position == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer raise for BCD type for trip
  param        : OnOffBCDTapChangerRaiseWithTrip, name of the test case
  return       : NA
*/
TEST_CASE("OnOffBCDTapChangerRaiseWithTrip")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Raise -> 6, Raise -> 7, Raise -> 8, Raise -> 9, Raise -> 10
		  Raise -> 10 (because 10 is the max limit)
		*/
		for (int i = 6; i <= 10; ++i)
		{
			SendEvent(TRIP_CODES[1], 11, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(TRIP_CODES[1], 11, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(10 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(TRIP_CODES[1], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer lower for BCD type
  param        : OnOffTestBCDTapChangerLowerWithClose, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBCDTapChangerLowerWithClose")
{
	TestSetup();

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
		CHECK(5 == GetBCDEncodedString(indexes, sim_port));
		/*
		  As we know the index 7 tap changer's default position is 5
		  Lower -> 4, Lower -> 3, Lower -> 2, Lower -> 1, Lower -> 0
		  Lower -> 0 (because 0 is the min limit)
		*/
		for (int i = 4; i >= 0; --i)
		{
			SendEvent(TRIP_CODES[0], 11, sim_port, CommandStatus::SUCCESS);
			CHECK(i == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		/*
		  test the corner cases now.
		  we will test to raise the tap changer beyond the lower limit mark
		 */
		SendEvent(TRIP_CODES[0], 11, sim_port, CommandStatus::OUT_OF_RANGE);
		CHECK(0 == GetBCDEncodedString(indexes, sim_port));
		/*
		  test the corner cases now.
		  send the event with an index which doesnt exist
		 */
		SendEvent(TRIP_CODES[0], 9189, sim_port, CommandStatus::NOT_SUPPORTED);
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests tap changer random raise / lower for BCD type
  param        : OnOffTestBCDTapChangerRandomWithClose, name of the test case
  return       : NA
*/
TEST_CASE("OnOffTestBCDTapChangerRandomWithClose")
{
	TestSetup();

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
		int tap_position = GetBCDEncodedString(indexes, sim_port);
		CHECK(tap_position == 5);
		for (int i = 0; i < 20; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			const int index = RandomNumber(0, 100) % 2;
			if (index) ++tap_position;
			else --tap_position;
			if (tap_position < 0)
			{
				tap_position = 0;
				status = CommandStatus::OUT_OF_RANGE;
			}
			if (tap_position > 10)
			{
				tap_position = 10;
				status = CommandStatus::OUT_OF_RANGE;
			}
			SendEvent(TRIP_CODES[index], 11, sim_port, status);
			CHECK(tap_position == static_cast<int>(GetBCDEncodedString(indexes, sim_port)));
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests empty binary controls
  param        : EmptyBinaryContol, name of the test case
  return       : NA
*/
TEST_CASE("EmptyBinaryContol")
{
	TestSetup();

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

		for (int i = 0; i < 2; ++i)
		{
			CommandStatus status = CommandStatus::SUCCESS;
			SendEvent(TRIP_CODES[i], 12, sim_port, status);
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests invalid index binary control hanlding
  param        : EmptyBinaryContol, name of the test case
  return       : NA
*/
TEST_CASE("InvalidIndexForBinaryControl")
{
	TestSetup();

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

		for (int i = 0; i < 2; ++i)
		{
			SendEvent(odc::ControlCode::LATCH_ON, 99, sim_port, odc::CommandStatus::NOT_SUPPORTED);
		}
		sim_port->Disable();
	}
	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests that a SQLite3-backed point with zero rows doesn't crash or hang,
                 and simply never updates from its StartVal
  param        : SQLiteDB_EmptyTable, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_EmptyTable")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto db_path = SQLiteTestDBPath("empty");
		CreateSQLitePlaybackDB(db_path, {});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500001, db_path, "select timestamp,value from events order by timestamp asc", "RELATIVE_FIRST", 77.0));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		CHECK(sim_port->Enabled());
		CHECK(std::stod(sim_port->GetCurrentState()["AnalogPayload"]["500001"].asString()) == 77.0);

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests RELATIVE_FIRST playback wraps around and replays the DB's rows
  param        : SQLiteDB_RelativeFirstWraparound, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_RelativeFirstWraparound")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto db_path = SQLiteTestDBPath("relative_first_wrap");
		CreateSQLitePlaybackDB(db_path, {
			{0,111.0,0}, {150,222.0,0}, {300,333.0,0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500002, db_path, "select timestamp,value from events order by timestamp asc", "RELATIVE_FIRST"));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//row1 re-appearing after later rows proves at least one wraparound cycle completed.
		//(the last row before a wrap is re-anchored to "now" with ~zero dwell time, so it's
		//not reliably caught by plain snapshot sampling - hence checking transitions/repeats
		//of the earlier, reliably-observable rows instead of every single value)
		auto transitions = PollPointTransitions(sim_port, "AnalogPayload", 500002, std::chrono::milliseconds(900));
		CHECK(sim_port->Enabled());
		CHECK(std::count(transitions.begin(), transitions.end(), 111.0) >= 2);
		CHECK(std::count(transitions.begin(), transitions.end(), 222.0) >= 1);

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests RELATIVE_TOD playback maps a recorded time-of-day pattern onto today
                 and wraps around, without FASTFORWARD
  param        : SQLiteDB_RelativeTOD, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_RelativeTOD")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		//rows at 00:00:01, 00:00:02, 00:00:03 UTC on an arbitrary past date - only the
		//time-of-day component matters, the date gets re-mapped onto today
		auto db_path = SQLiteTestDBPath("relative_tod");
		CreateSQLitePlaybackDB(db_path, {
			{1577836801000,1.0,0}, {1577836802000,2.0,0}, {1577836803000,3.0,0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500003, db_path, "select timestamp,value from events order by timestamp asc", "RELATIVE_TOD"));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//without FASTFORWARD, all 3 (already past for today) rows fire back-to-back
		//essentially instantly, then wrap and repeat - poll densely to catch all 3
		auto seen = PollPointValues(sim_port, "AnalogPayload", 500003, std::chrono::milliseconds(300), std::chrono::milliseconds(1));
		CHECK(sim_port->Enabled());
		CHECK(ContainsAll(seen, {1.0, 2.0, 3.0}));

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : regression test - RELATIVE_TOD_FASTFORWARD where every row is already in the
                 past for today must roll forward exactly one day and land on a future
                 occurrence, rather than recursing/hanging trying to find a non-past row
  param        : SQLiteDB_RelativeTODFastForwardAllPast, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_RelativeTODFastForwardAllPast")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		//rows at 00:00:01, 00:00:02, 00:00:03 UTC - these will always be "in the past"
		//relative to "now" (except in the first few seconds after UTC midnight)
		auto db_path = SQLiteTestDBPath("relative_tod_ff_all_past");
		CreateSQLitePlaybackDB(db_path, {
			{1577836801000,1.0,0}, {1577836802000,2.0,0}, {1577836803000,3.0,0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500004, db_path, "select timestamp,value from events order by timestamp asc", "RELATIVE_TOD_FASTFORWARD", 77.0));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//the found event is ~24h in the future, so no crash/hang is the main thing being
		//verified here - the value should stay at its StartVal for the life of this test
		auto seen = PollPointValues(sim_port, "AnalogPayload", 500004, std::chrono::milliseconds(300));
		CHECK(sim_port->Enabled());
		CHECK(seen == std::set<double>{77.0});

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests ABSOLUTE playback fires rows at their literal timestamps, in order,
                 and then stops (no wraparound) once the table is exhausted
  param        : SQLiteDB_Absolute, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_Absolute")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto now = msSinceEpoch();
		auto db_path = SQLiteTestDBPath("absolute");
		CreateSQLitePlaybackDB(db_path, {
			{static_cast<int64_t>(now)+50, 1.0, 0},
			{static_cast<int64_t>(now)+150, 2.0, 0},
			{static_cast<int64_t>(now)+250, 3.0, 0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500005, db_path, "select timestamp,value from events order by timestamp asc", "ABSOLUTE"));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//wait for all 3 rows to fire, then confirm it settles on the last one and stays there
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		CHECK(std::stod(sim_port->GetCurrentState()["AnalogPayload"]["500005"].asString()) == 3.0);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		CHECK(std::stod(sim_port->GetCurrentState()["AnalogPayload"]["500005"].asString()) == 3.0);
		CHECK(sim_port->Enabled());

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests ABSOLUTE_FASTFORWARD skips rows whose timestamps have already passed
                 and lands directly on the next future row
  param        : SQLiteDB_AbsoluteFastForward, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_AbsoluteFastForward")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto now = msSinceEpoch();
		auto db_path = SQLiteTestDBPath("absolute_ff");
		CreateSQLitePlaybackDB(db_path, {
			{static_cast<int64_t>(now)-2000, 1.0, 0},
			{static_cast<int64_t>(now)-1000, 2.0, 0},
			{static_cast<int64_t>(now)+150, 3.0, 0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500006, db_path, "select timestamp,value from events order by timestamp asc", "ABSOLUTE_FASTFORWARD", 0.0));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//the two past rows (1.0, 2.0) must never be observed - only the StartVal (0.0)
		//until the future row (3.0) fires
		auto seen = PollPointValues(sim_port, "AnalogPayload", 500006, std::chrono::milliseconds(400));
		CHECK(sim_port->Enabled());
		CHECK(ContainsAll(seen, {3.0}));
		CHECK_FALSE(ContainsAll(seen, {1.0}));
		CHECK_FALSE(ContainsAll(seen, {2.0}));

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests SQLite3 playback of a Binary point (int column mapped to bool)
  param        : SQLiteDB_BinaryType, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_BinaryType")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto db_path = SQLiteTestDBPath("binary_first_wrap");
		CreateSQLitePlaybackDB(db_path, {
			{0,1,0}, {150,0,0}, {300,1,0}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Binaries"].append(MakeSQLitePoint(500007, db_path, "select timestamp,value from events order by timestamp asc", "RELATIVE_FIRST"));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		auto seen = PollPointValues(sim_port, "BinaryPayload", 500007, std::chrono::milliseconds(900));
		CHECK(sim_port->Enabled());
		CHECK(ContainsAll(seen, {0.0, 1.0}));

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

/*
  function     : TEST_CASE
  description  : tests two points sharing one SQLite3 DB/query, filtered by the bound
                 :INDEX parameter, each only ever seeing its own rows
  param        : SQLiteDB_IndexBinding, name of the test case
  return       : NA
*/
TEST_CASE("SQLiteDB_IndexBinding")
{
	TestSetup();

	auto port_lib = LoadModule(GetLibFileName("SimPort"));
	REQUIRE(port_lib);

	{
		newptr new_sim = GetPortCreator(port_lib, "Sim");
		REQUIRE(new_sim);
		delptr delete_sim = GetPortDestroyer(port_lib, "Sim");
		REQUIRE(delete_sim);

		auto db_path = SQLiteTestDBPath("index_binding");
		CreateSQLitePlaybackDB(db_path, {
			{0,10.0,500008}, {150,11.0,500008}, {300,12.0,500008},
			{0,100.0,500009}, {150,101.0,500009}, {300,102.0,500009}
		});

		Json::Value conf = GetTestConfigJSON();
		conf["Analogs"].append(MakeSQLitePoint(500008, db_path, "select timestamp,value from events where idx=:INDEX order by timestamp asc", "RELATIVE_FIRST"));
		conf["Analogs"].append(MakeSQLitePoint(500009, db_path, "select timestamp,value from events where idx=:INDEX order by timestamp asc", "RELATIVE_FIRST"));

		auto sim_port = std::shared_ptr<DataPort>(new_sim("OutstationUnderTest", "", conf), delete_sim);
		sim_port->Build();

		ThreadPool thread_pool(1);

		sim_port->Enable();
		while(!sim_port->Enabled())
			;

		//row1 of each point (10.0/100.0) re-appearing more than once proves each point wraps
		//independently, without cross-contaminating the other point's bound query results
		auto transitions_1 = PollPointTransitions(sim_port, "AnalogPayload", 500008, std::chrono::milliseconds(900));
		auto transitions_2 = PollPointTransitions(sim_port, "AnalogPayload", 500009, std::chrono::milliseconds(900));
		std::set<double> seen_1(transitions_1.begin(), transitions_1.end());
		std::set<double> seen_2(transitions_2.begin(), transitions_2.end());

		CHECK(sim_port->Enabled());
		CHECK(std::count(transitions_1.begin(), transitions_1.end(), 10.0) >= 2);
		CHECK(std::count(transitions_1.begin(), transitions_1.end(), 11.0) >= 1);
		CHECK(std::count(transitions_2.begin(), transitions_2.end(), 100.0) >= 2);
		CHECK(std::count(transitions_2.begin(), transitions_2.end(), 101.0) >= 1);
		//confirm no cross-contamination between the two bound queries
		CHECK_FALSE(ContainsAll(seen_1, {100.0}));
		CHECK_FALSE(ContainsAll(seen_1, {101.0}));
		CHECK_FALSE(ContainsAll(seen_2, {10.0}));
		CHECK_FALSE(ContainsAll(seen_2, {11.0}));

		sim_port->Disable();
	}

	UnLoadModule(port_lib);
	TestTearDown();
}

