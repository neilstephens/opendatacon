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
* CBTest.cpp
*
*  Created on: 10/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

// Disable excessing data on stack warning for the test file only.
#ifdef _MSC_VER
#pragma warning(disable: 6262)
#endif

#include <array>
#include <fstream>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstdint>

#define COMPILE_TESTS

#if defined(COMPILE_TESTS)

// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "PyPort.h"


#if defined(NONVSTESTING)
#include <catch.hpp>
#else
#include "spdlog/sinks/msvc_sink.h"
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer - but also has some problems not destructing objects correctly.
#endif

// To remove GCC warnings
namespace RTUConnectedTests
{
extern const char *Pyconffile;
}

extern const char *conffilename1;
extern const char *conffile1;

const char *conffilename1 = "PyConfig.conf";


#ifdef _MSC_VER
#pragma region conffiles
#endif
// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile1 = R"001(
{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 9,
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	// Python Module/Class/Method name definitions
	"ModuleName" : "PyPortSim",
	"ClassName": "SimPortClass",
	"FuncEnable" : "Enable",
	"FuncDisable" : "Disable",
	"FuncEventHandler" : "EventHandler",

	//-------Point conf--------Pass this through to the Python code to deal with

	"PollGroups" : [{"ID" : 1, "PollRate" : 10000, "Group" : 3, "PollType" : "Scan"},
					{"ID" : 2, "PollRate" : 20000, "Group" : 3, "PollType" : "SOEScan"},
					{"ID" : 3, "PollRate" : 120000, "PollType" : "TimeSetCommand"}],

	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 1, "Type" : "DIG", "SOE" : {"Group": 5, "Index" : 0} },
					{"Index" : 12, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA"},
					{"Index" : 13, "Group" : 3, "PayloadLocation": "2A", "Channel" : 2, "Type" : "MCB"},
					{"Index" : 14, "Group" : 3, "PayloadLocation": "2A", "Channel" : 3, "Type" : "MCC" }],

	"Analogs" : [	{"Index" : 0, "Group" : 3, "PayloadLocation": "3A","Channel" : 1, "Type":"ANA"},
					{"Index" : 1, "Group" : 3, "PayloadLocation": "3B","Channel" : 1, "Type":"ANA"},
					{"Index" : 2, "Group" : 3, "PayloadLocation": "4A","Channel" : 1, "Type":"ANA"},
					{"Index" : 3, "Group" : 3, "PayloadLocation": "4B","Channel" : 1, "Type":"ANA6"},
					{"Index" : 4, "Group" : 3, "PayloadLocation": "4B","Channel" : 2, "Type":"ANA6"}],

	"BinaryControls" : [{"Index": 1,  "Group" : 4, "Channel" : 1, "Type" : "CONTROL"},
                        {"Range" : {"Start" : 10, "Stop" : 21}, "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],

	"AnalogControls" : [{"Index": 1,  "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}]

})001";



#ifdef _MSC_VER
#pragma endregion

#pragma region TEST_HELPERS
#endif

// Write out the conf file information about into a file so that it can be read back in by the code.
void WriteConfFilesToCurrentWorkingDirectory()
{
	std::ofstream ofs(conffilename1);
	if (!ofs)
	{
		INFO("Could not open conffile1 for writing");
	}
	ofs << conffile1;
	ofs.close();
}

void SetupLoggers()
{
	// So create the log sink first - can be more than one and add to a vector.
	#if defined(NONVSTESTING)
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	#else
	auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	#endif
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("testslog.txt", true);

	std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

	auto pLibLogger = std::make_shared<spdlog::logger>("PyPort", begin(sinks),end(sinks));
	pLibLogger->set_level(spdlog::level::trace);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", begin(sinks), end(sinks));
	pODCLogger->set_level(spdlog::level::trace);
	odc::spdlog_register_logger(pODCLogger);

}
void WriteStartLoggingMessage(std::string TestName)
{
	std::string msg = "Logging for '"+TestName+"' started..";

	if (auto cblogger = odc::spdlog_get("PyPort"))
	{
		cblogger->info("------------------");
		cblogger->info(msg);
	}
	else
		std::cout << "Error PyPort Logger not operational";

/*	if (auto odclogger = odc::spdlog_get("opendatacon"))
            odclogger->info(msg);
      else
            std::cout << "Error opendatacon Logger not operational";
            */
}
void TestSetup(std::string TestName, bool writeconffiles = true)
{
	#ifndef NONVSTESTING
	SetupLoggers();
	#endif
	WriteStartLoggingMessage(TestName);

	if (writeconffiles)
		WriteConfFilesToCurrentWorkingDirectory();
}
void TestTearDown(void)
{
	INFO("Test Finished");
	#ifndef NONVSTESTING

	spdlog::drop_all(); // Un-register loggers, and if no other shared_ptr references exist, they will be destroyed.
	#endif
}
// Used for command line test setup
void CommandLineLoggingSetup()
{
	SetupLoggers();
	WriteStartLoggingMessage("All Tests");
}
void CommandLineLoggingCleanup()
{
	spdlog::drop_all(); // Un-register loggers, and if no other shared_ptr references exist, they will be destroyed.
}

void RunIOSForXSeconds(asio::io_service &IOS, unsigned int seconds)
{
	// We don\92t have to consider the timer going out of scope in this use case.
	Timer_t timer(IOS);
	timer.expires_from_now(std::chrono::seconds(seconds));
	timer.async_wait([&IOS](asio::error_code ) // [=] all autos by copy, [&] all autos by ref
		{
			// If there was no more work, the asio::io_service will exit from the IOS.run() below.
			// However something is keeping it running, so use the stop command to force the issue.
			IOS.stop();
		});

	IOS.run(); // Will block until all Work is done, or IOS.Stop() is called. In our case will wait for the TCP write to be done,
	// and also any async timer to time out and run its work function (or lambda) - does not need to really do anything!
	// If the IOS runs out of work, it must be reset before being run again.
}
std::thread *StartIOSThread(asio::io_service &IOS)
{
	return new std::thread([&] { IOS.run(); });
}
void StopIOSThread(asio::io_service &IOS, std::thread *runthread)
{
	IOS.stop();        // This does not block. The next line will! If we have multiple threads, have to join all of them.
	runthread->join(); // Wait for it to exit
	delete runthread;
}
void WaitIOS(asio::io_service &IOS, int seconds)
{
	Timer_t timer(IOS);
	timer.expires_from_now(std::chrono::seconds(seconds));
	timer.wait();
}


// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP()\
	TestSetup(Catch::getResultCapture().getCurrentTestName());\
	asio::io_service IOS(4); // Max 4 threads

// Used for tests that dont need IOS
#define SIMPLE_TEST_SETUP()\
	TestSetup(Catch::getResultCapture().getCurrentTestName());

#define STANDARD_TEST_TEARDOWN()\
	TestTearDown();\

#define START_IOS(threadcount) \
	LOGINFO("Starting ASIO Threads"); \
	auto work = std::make_shared<asio::io_service::work>(IOS); /* To keep run - running!*/\
	const int ThreadCount = threadcount; \
	std::thread *pThread[threadcount]; \
	for (int i = 0; i < threadcount; i++) pThread[i] = StartIOSThread(IOS);

#define STOP_IOS() \
	LOGINFO("Shutting Down ASIO Threads");    \
	work.reset();     \
	for (int i = 0; i < ThreadCount; i++) StopIOSThread(IOS, pThread[i]);

#define TEST_PythonPort(overridejson)\
	auto PythonPort = std::make_unique<PyPort>("TestMaster", conffilename1, overridejson); \
	PythonPort->SetIOS(&IOS);      \
	PythonPort->Build();

#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif

namespace SimpleUnitTestsPy
{
TEST_CASE("Py.CheckTimeConversion")
{
	SIMPLE_TEST_SETUP();

	REQUIRE(true);
	STANDARD_TEST_TEARDOWN();
}
}

namespace EventTests
{
void SendBinaryEvent(std::unique_ptr<PyPort>& PythonPort, int ODCIndex, bool val, QualityFlags qual = QualityFlags::ONLINE, msSinceEpoch_t time = msSinceEpoch())
{
	auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing", qual, time);
	event->SetPayload<EventType::Binary>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	PythonPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}
void SendAnalogEvent(std::unique_ptr<PyPort> & PythonPort, int ODCIndex, double val, QualityFlags qual = QualityFlags::ONLINE, msSinceEpoch_t time = msSinceEpoch())
{
	auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
	event->SetPayload<EventType::Analog>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	PythonPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}

TEST_CASE("Py.SendBinaryAndAnalogEvents")
{
	// So we send a Scan F0 packet to the Outstation, it responds with the data in the point table.
	// Then we update the data in the point table, scan again and check the data we get back.
	STANDARD_TEST_SETUP();
	TEST_PythonPort(Json::nullValue);

	START_IOS(2);

	PythonPort->Enable();

//	SendBinaryEvent(PythonPort, 1, true);

//	SendAnalogEvent(PythonPort, 1, 1000.1);

	WaitIOS(IOS, 1);

	PythonPort->Disable();

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
}

#endif
