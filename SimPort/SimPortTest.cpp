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

#ifdef _MSC_VER
// Disable excessing data on stack warning for the test file only.
#pragma warning(disable: 6262)
#endif

#include <array>
#include <fstream>
#include <cassert>
#include <assert.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "SimPort.h"

#ifdef COMPILE_TESTS

#include <spdlog/sinks/stdout_color_sinks.h>

#include "SimPort.h"

#ifdef NONVSTESTING
#include <catch.hpp>
#else
#include "spdlog/sinks/msvc_sink.h"
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer
#endif

#define SUITE(name) "SimTests - " name
namespace SimUnitTests
{

	extern const char* conffilename1;
	extern const char* conffile1;
	extern std::vector<spdlog::sink_ptr> LogSinks;


	const char* conffilename1 = "SimConfig.conf";

#ifdef _MSC_VER
#pragma region conffiles
#endif
	// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
	const char* conffile1 = R"001(
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

#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region TEST_HELPERS
#endif

	std::vector<spdlog::sink_ptr> LogSinks;


	// Write out the conf file information about into a file so that it can be read back in by the code.
	void WriteConfFilesToCurrentWorkingDirectory()
	{
		std::ofstream ofs(conffilename1);
		if (!ofs) throw std::runtime_error(std::string("Could not open conffile1 for writing"));

		ofs << conffile1;
		ofs.close();
	}
	void SetupLoggers(spdlog::level::level_enum loglevel)
	{
		// So create the log sink first - can be more than one and add to a vector.
#ifdef NONVSTESTING
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#else
		auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#endif
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("simporttest.log", true);

		std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

		auto pLibLogger = std::make_shared<spdlog::logger>("SimPort", begin(sinks), end(sinks));
		pLibLogger->set_level(loglevel);
		odc::spdlog_register_logger(pLibLogger);

		// We need an opendatacon logger to catch config file parsing errors
		auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", begin(sinks), end(sinks));
		pODCLogger->set_level(spdlog::level::trace);
		odc::spdlog_register_logger(pODCLogger);
	}
	void WriteStartLoggingMessage(std::string TestName)
	{
		std::string msg = "Logging for '" + TestName + "' started..";
		if (auto simlogger = odc::spdlog_get("SimPort"))
		{
			simlogger->info("------------------");
			simlogger->info(msg);
		}
		else
		{
			std::cout << "Error SimPort Logger not operational";
		}
		/*	if (auto odclogger = odc::spdlog_get("opendatacon"))
						  odclogger->info(msg);
				else
						  std::cout << "Error opendatacon Logger not operational";
						  */
	}
	void TestSetup(bool writeconffiles = true)
	{
#ifndef NONVSTESTING
		SetupLoggers(spdlog::level::level_enum::debug);
		WriteStartLoggingMessage("VS Testing");
#endif

		if (writeconffiles)
			WriteConfFilesToCurrentWorkingDirectory();
	}

	void TestTearDown()
	{
		spdlog::drop_all(); // Close off everything
	}
	// Used for command line test setup
	extern "C" void CommandLineLoggingSetup(spdlog::level::level_enum log_level)
	{
		SetupLoggers(log_level);
		WriteStartLoggingMessage("All Tests");
	}
	extern "C" void CommandLineLoggingCleanup()
	{
		odc::spdlog_flush_all();
		odc::spdlog_drop_all(); // Un-register loggers, and if no other shared_ptr references exist, they will be destroyed.
	}
	// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
	// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
	std::string BuildHexStringFromASCIIHexString(const std::string& as)
	{
		assert(as.size() % 2 == 0); // Must be even length

		// Create, we know how big it will be
		auto res = std::string(as.size() / 2, 0);

		// Take chars in chunks of 2 and convert to a hex equivalent
		for (uint32_t i = 0; i < (as.size() / 2); i++)
		{
			auto hexpair = as.substr(i * 2, 2);
			res[i] = static_cast<char>(std::stol(hexpair, nullptr, 16));
		}
		return res;
	}
	void RunIOSForXSeconds(odc::asio_service& IOS, unsigned int seconds)
	{
		// We don\92t have to consider the timer going out of scope in this use case.
		auto timer = IOS.make_steady_timer();
		timer->expires_from_now(std::chrono::seconds(seconds));
		timer->async_wait([&IOS](asio::error_code err_code) // [=] all autos by copy, [&] all autos by ref
			{
				// If there was no more work, the odc::asio_service will exit from the IOS.run() below.
				// However something is keeping it running, so use the stop command to force the issue.
				IOS.stop();
			});

		IOS.run(); // Will block until all Work is done, or IOS.Stop() is called. In our case will wait for the TCP write to be done,
		// and also any async timer to time out and run its work function (or lambda) - does not need to really do anything!
		// If the IOS runs out of work, it must be reset before being run again.
	}
	std::thread* StartIOSThread(odc::asio_service& IOS)
	{
		return new std::thread([&] { IOS.run(); });
	}
	void StopIOSThread(odc::asio_service& IOS, std::thread* runthread)
	{
		IOS.stop();        // This does not block. The next line will! If we have multiple threads, have to join all of them.
		runthread->join(); // Wait for it to exit
		delete runthread;
	}
	void Wait(odc::asio_service& IOS, int seconds)
	{
		auto timer = IOS.make_steady_timer();
		timer->expires_from_now(std::chrono::seconds(seconds));
		timer->wait();
	}


	// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP()\
	TestSetup();\
	auto IOS = std::make_shared<odc::asio_service>(4); // Max 4 threads

#define START_IOS(threadcount) \
	LOGINFO("Starting ASIO Threads"); \
	auto work = IOS->make_work(); /* To keep run - running!*/\
	const int ThreadCount = threadcount; \
	std::thread *pThread[threadcount]; \
	for (int i = 0; i < threadcount; i++) pThread[i] = StartIOSThread(*IOS);

#define STOP_IOS() \
	LOGINFO("Shutting Down ASIO Threads");    \
	work.reset();     \
	for (int i = 0; i < ThreadCount; i++) StopIOSThread(*IOS, pThread[i]);

#define TEST_SimPort1(overridejson)\
	auto SimPort1 = std::make_unique<SimPort>("SimPort1", conffilename1, overridejson); \
	SimPort1->SetIOS(IOS);      \
	SimPort1->Build();


#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif
	int Dummy(int i)
	{
		return i + 1;
	}

	namespace test
	{
		TEST_CASE("TestConfigLoad")
		{
			STANDARD_TEST_SETUP();
	//		TEST_SimPort1(Json::nullValue);
			auto SimPort1 = std::make_unique<SimPort>("SimPort1", conffilename1, Json::nullValue); 
				SimPort1->SetIOS(IOS);      
				SimPort1->Build();

			SimPort1->Enable();

			// Set up a callback for the result - assume sync operation at the moment
			CommandStatus res = CommandStatus::NOT_AUTHORIZED;
			auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
				{
					res = command_stat;
				});

			// TEST EVENTS WITH DIRECT CALL
			// Test on a valid binary point
			const int ODCIndex = 1;

			EventTypePayload<EventType::Binary>::type val;
			val = true;
			auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
			event->SetPayload<EventType::Binary>(std::move(val));

			SimPort1->Event(event, "TestHarness", pStatusCallback);

			REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set. 1 is defined

			TestTearDown();
		}
	}
}

#endif