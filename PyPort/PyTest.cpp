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

#include "../HTTP/clientrequest.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <thread>
#include <utility>

#define COMPILE_TESTS

#if defined(COMPILE_TESTS)

// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.
#include "PyPort.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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

	// Python Module/Class/Method name definitions
	"ModuleName" : "PyPortSim",
	"ClassName": "SimPortClass",
	"EventsAreQueued": false,
	"OnlyQueueEventsWithTags": false,
	"QueueFormatString": "{{\"Tag\" : \"{6}\", \"Idx\" : {1}, \"Val\" : \"{4}\", \"Quality\" : \"{3}\", \"TS\" : \"{2}\"}}", // Valid fmt.print string
	"GlobalUseSystemPython": false,

	// The point definitions are only proccessed by the Python code. Any events sent to PyPort by ODC will be passed on.
	"Binaries" :
	[
		{"Index" : 0, "CBNumber" : 1, "SimType" : "CBStateBit0", "State": 0, "Tag": "Test0" },	// Half of a dual bit binary Open 10, Closed 01, Fault 00 or 11 (Is this correct?)
		{"Index" : 1, "CBNumber" : 1, "SimType" : "CBStateBit1", "State": 1, "Tag": "Test1" },	// Half of a dual bit binary. State is starting state.
		{"Index" : 2, "Tag": "Test2" },
		{"Index" : 3, "Tag": "Test3" },
		{"Index" : 4, "Tag": "Test4" }
	],

	"BinaryControls" :
	[
		{"Index": 0, "CBNumber" : 1, "CBCommand":"Trip", "Tag": "Test3" },		// Trip pulse
		{"Index": 1, "CBNumber" : 1, "CBCommand":"Close", "Tag": "Test4" }		// Close pulse
	]
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

// Changed so that the log_level only changes the console logging level. Want the log file to always be debug
void SetupLoggers(spdlog::level::level_enum log_level)
{
	// So create the log sink first - can be more than one and add to a vector.
	#if defined(NONVSTESTING)
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	#else
	auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	#endif
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("pyporttest.log", true);

	std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

	file_sink->set_level(spdlog::level::level_enum::debug);
	console_sink->set_level(log_level);

	auto pLibLogger = std::make_shared<spdlog::logger>("PyPort", begin(sinks), end(sinks));
	pLibLogger->set_level(spdlog::level::level_enum::debug);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", begin(sinks), end(sinks));
	pODCLogger->set_level(spdlog::level::level_enum::debug);
	odc::spdlog_register_logger(pODCLogger);

}
void WriteStartLoggingMessage(const std::string& TestName)
{
	std::string msg = "Logging for '" + TestName + "' started..";

	if (auto pylogger = odc::spdlog_get("PyPort"))
	{
		pylogger->info("------------------");
		pylogger->info("PyPort Logger Message "+msg);
	}
	else
		std::cout << "Error PyPort Logger not operational";

	if (auto odclogger = odc::spdlog_get("opendatacon"))
	{
		odclogger->info("opendatacon Logger Message "+msg);
	}
	else
		std::cout << "Error opendatacon Logger not operational";
}
void TestSetup(const std::string& TestName, bool writeconffiles = true)
{
	#ifndef NONVSTESTING
	SetupLoggers(spdlog::level::level_enum::trace);
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
void CommandLineLoggingSetup(spdlog::level::level_enum log_level)
{
	SetupLoggers(log_level);
	WriteStartLoggingMessage("All Tests");
}
void CommandLineLoggingCleanup()
{
	odc::spdlog_flush_all();
	odc::spdlog_drop_all(); // Un-register loggers, and if no other shared_ptr references exist, they will be destroyed.
}

void RunIOSForXSeconds(std::shared_ptr<odc::asio_service> IOS, unsigned int seconds)
{
	// We dont have to consider the timer going out of scope in this use case.
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(seconds));
	timer->async_wait([&IOS](asio::error_code ) // [=] all autos by copy, [&] all autos by ref
		{
			// If there was no more work, the asio::io_context will exit from the IOS.run() below.
			// However something is keeping it running, so use the stop command to force the issue.
			IOS->stop();
		});

	IOS->run(); // Will block until all Work is done, or IOS.Stop() is called. In our case will wait for the TCP write to be done,
	// and also any async timer to time out and run its work function (or lambda) - does not need to really do anything!
	// If the IOS runs out of work, it must be reset before being run again.
}
std::thread *StartIOSThread(const std::shared_ptr<odc::asio_service>& IOS)
{
	return new std::thread([IOS] { IOS->run(); });
}
void StopIOSThread(const std::shared_ptr<odc::asio_service>& IOS, std::thread *runthread)
{
	IOS->stop();       // This does not block. The next line will! If we have multiple threads, have to join all of them.
	runthread->join(); // Wait for it to exit
	delete runthread;
}
void WaitIOS(const std::shared_ptr<odc::asio_service>& IOS, int seconds)
{
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(seconds));
	timer->wait();
}
bool WaitIOSResult(const std::shared_ptr<odc::asio_service>& IOS, int MaxWaitSeconds, std::atomic<CommandStatus>& res, CommandStatus InitialValue)
{
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::milliseconds(0)); // Set below.
	size_t cnt = MaxWaitSeconds * 20;                      // 50 msec * 20 = 1 second
	while (cnt-- > 0)
	{
		timer->expires_at(timer->expires_at() + std::chrono::milliseconds(50));
		timer->wait();
		if (res != InitialValue)
			return true; // Value changed
	}
	return false; // Timed out
}
bool WaitIOSFnResult(const std::shared_ptr<odc::asio_service>& IOS, int MaxWaitSeconds, const std::function<bool()>&Result)
{
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::milliseconds(0)); // Set below.
	size_t cnt = MaxWaitSeconds * 20;                      // 50 msec * 20 = 1 second
	while (cnt-- > 0)
	{
		timer->expires_at(timer->expires_at() + std::chrono::milliseconds(50));
		timer->wait();
		if (Result())
			return true; // Value changed
	}
	return false; // Timed out
}

class protected_string
{
public:
	protected_string(): val("") {};
	protected_string(const std::string& _val): val(_val) {};
	std::string  getandset(const std::string& newval)
	{
		std::unique_lock<std::shared_timed_mutex> lck(m);
		std::string retval = val;
		val = newval;
		return retval;
	}
	void set(std::string newval)
	{
		std::unique_lock<std::shared_timed_mutex> lck(m);
		val = std::move(newval);
	}
	std::string get(void)
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		return val;
	}
private:
	std::shared_timed_mutex m;
	std::string val;
};

// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP(threadcount)\
	TestSetup(Catch::getResultCapture().getCurrentTestName());\
	const int ThreadCount = threadcount; \
	std::shared_ptr<odc::asio_service> IOS = odc::asio_service::Get()

// Used for tests that dont need IOS
#define SIMPLE_TEST_SETUP()\
	TestSetup(Catch::getResultCapture().getCurrentTestName())

#define STANDARD_TEST_TEARDOWN()\
	TestTearDown()

#define START_IOS() \
	LOGINFO("Starting ASIO Threads"); \
	auto work = IOS->make_work(); /* To keep run - running!*/\
	std::thread *pThread[ThreadCount]; \
	for (int i = 0; i < ThreadCount; i++) pThread[i] = StartIOSThread(IOS)

#define STOP_IOS() \
	LOGINFO("Shutting Down ASIO Threads");    \
	work.reset();     \
	for (int i = 0; i < ThreadCount; i++) StopIOSThread(IOS, pThread[i]);

#define TEST_PythonPort(overridejson)\
	auto PythonPort = std::make_shared<PyPort>("TestMaster", conffilename1, overridejson); \
	PythonPort->Build()
#define TEST_PythonPort2(overridejson)\
	auto PythonPort2 = std::make_shared<PyPort>("TestMaster2", conffilename1, overridejson); \
	PythonPort2->Build()
#define TEST_PythonPort3(overridejson)\
	auto PythonPort3 = std::make_shared<PyPort>("TestMaster3", conffilename1, overridejson); \
	PythonPort3->Build()
#define TEST_PythonPort4(overridejson)\
	auto PythonPort4 = std::make_shared<PyPort>("TestMaster4", conffilename1, overridejson); \
	PythonPort4->Build()
#define TEST_PythonPort5(overridejson)\
	auto PythonPort5 = std::make_shared<PyPort>("TestMaster5", conffilename1, overridejson); \
	PythonPort5->Build()

#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif

namespace EventTests
{
void CheckEventStringConversions(const std::shared_ptr<EventInfo>& inevent)
{
	// Get string representation as used in PythonWrapper
	std::string EventTypeStr = odc::ToString(inevent->GetEventType());
	std::string QualityStr = ToString(inevent->GetQuality());
	std::string PayloadStr = inevent->GetPayloadString();
	size_t ODCIndex = inevent->GetIndex();

	// Create a new event from those strings
	std::shared_ptr<EventInfo> pubevent = PyPort::CreateEventFromStrParams(EventTypeStr, ODCIndex, QualityStr, PayloadStr, "Testing");

	// Check that we got back data matching the original event.
	REQUIRE(odc::ToString(pubevent->GetEventType()) == EventTypeStr);
	REQUIRE(pubevent->GetIndex() == ODCIndex);
	REQUIRE(ToString(pubevent->GetQuality()) == QualityStr);
	REQUIRE(pubevent->GetPayloadString() == PayloadStr);
}

TEST_CASE("Py.TestEventStringConversions")
{
	STANDARD_TEST_SETUP(4);

	uint32_t ODCIndex = 1001;

	INFO("ConnectState")
	{
		ConnectState state = ConnectState::CONNECTED;
		auto odcevent = std::make_shared<EventInfo>(EventType::ConnectState, 0, "Testing");
		odcevent->SetPayload<EventType::ConnectState>(std::move(state));

		CheckEventStringConversions(odcevent);
	}
	INFO("Binary")
	{
		bool val = true;
		auto odcevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing", QualityFlags::RESTART);
		odcevent->SetPayload<EventType::Binary>(std::move(val));

		CheckEventStringConversions(odcevent);
	}
	INFO("Analog")
	{
		double fval = 100.1;
		auto odcevent = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, "Testing", QualityFlags::ONLINE);
		odcevent->SetPayload<EventType::Analog>(std::move(fval));

		CheckEventStringConversions(odcevent);
	}
	INFO("ControlRelayOutputBlock")
	{
		EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
		val.functionCode = ControlCode::LATCH_ON; // ControlCode::LATCH_OFF;
		auto odcevent = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness", QualityFlags::RESTART | QualityFlags::ONLINE);
		odcevent->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

		CheckEventStringConversions(odcevent);
	}
	//TODO: The rest of the Payload types that we need to handle
	STANDARD_TEST_TEARDOWN();
}

uint32_t GetProcessedEventsFromJSON(const std::string& jsonstr)
{
	Json::Value root;
	Json::CharReaderBuilder jsonReader;
	std::string errs;
	std::stringstream jsonstream(jsonstr);

	if (!Json::parseFromStream(jsonReader, jsonstream, &root, &errs))
		return -1;
	return root["processedevents"].asUInt();
}

TEST_CASE("Py.TestsUsingPython")
{
	// So do all the tests that involve using the Python Interpreter in one test, as we dont seem to be able to close it down correctly
	// and start it again. Not great, but not a massive problem under normal use cases.

	// The ODC startup process is Build, Start IOS, then Enable posts. So we are doing that right.
	// However in build we are telling our Python script we are operational - have to assume not enabled!
	STANDARD_TEST_SETUP(4); // Threads

	TEST_PythonPort(Json::nullValue);
	TEST_PythonPort2(Json::nullValue);
	TEST_PythonPort3(Json::nullValue);
	TEST_PythonPort4(Json::nullValue);

	START_IOS();

	PythonPort->Enable();
	PythonPort2->Enable();
	PythonPort3->Enable();
	PythonPort4->Enable();


	// The RasPi build is really slow to get ports up and enabled. If the events below are sent before they are enabled - test fail.
	REQUIRE_NOTHROW(
		if (!WaitIOSFnResult(IOS, 10, [&]()
			{
				return (PythonPort->Enabled() && PythonPort2->Enabled() && PythonPort3->Enabled() && PythonPort4->Enabled());
			}))
		{
			throw std::runtime_error("Waiting for Ports to Enable timed out");
		}
		);
	LOGINFO("Ports Enabled");

	INFO("SendBinaryAndAnalogEvents")
	{
		std::atomic<CommandStatus> res{ CommandStatus::UNDEFINED };
		auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([&](CommandStatus command_stat)
			{
				res = command_stat;
			});

		int ODCIndex = 1;
		bool val = true;
		auto boolevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing");
		boolevent->SetPayload<EventType::Binary>(std::move(val));

		LOGINFO("Sending Binary Events");
		PythonPort->Event(boolevent, "TestHarness", pStatusCallback);
		PythonPort2->Event(boolevent, "TestHarness2", nullptr);
		PythonPort3->Event(boolevent, "TestHarness3", nullptr);
		PythonPort4->Event(boolevent, "TestHarness4", nullptr);

		REQUIRE_NOTHROW(
			if (!WaitIOSResult(IOS, 12, res, CommandStatus::UNDEFINED))
				throw std::runtime_error("Command Status Update timed out");
			);
		REQUIRE(ToString(res) == ToString(CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.

		res = CommandStatus::UNDEFINED;
		double fval = 100.1;
		ODCIndex = 1001;
		auto event2 = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event2->SetPayload<EventType::Analog>(std::move(fval));

		PythonPort->Event(event2, "TestHarness", pStatusCallback);

		REQUIRE_NOTHROW(
			if (!WaitIOSResult(IOS, 10, res, CommandStatus::UNDEFINED))
				throw("Command Status Update timed out");
			);
		REQUIRE(ToString(res) == ToString(CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.

		std::string url("http://testserver/thisport/cb?test=harold");
		protected_string sres("");

		std::atomic<size_t> done_count(0);
		auto pResponseCallback = std::make_shared<std::function<void(std::string url)>>([&](std::string response)
			{
				sres.set(std::move(response));
				done_count++;
			});

		// Direct inject web request to python code - we get response back from python module PyPortSim.py
		PythonPort->RestHandler(url, "", pResponseCallback);

		REQUIRE_NOTHROW
		(
			if (!WaitIOSFnResult(IOS, 5, [&]()
				{
					return bool(done_count);
				}))
			{
				throw("Waiting for RestHandler response timed out");
			}
		);

		LOGDEBUG("Response {}", sres.get());
		REQUIRE(sres.get() == "{\"test\": \"POST\"}"); // The Get will Wait for the result to be set.

		// Spew a whole bunch of commands into the Python interface - which will be ASIO dispatch or post commands, to ensure single strand access.
		PythonPort->SetTimer(120, 1200);
		PythonPort->SetTimer(121, 1000);
		PythonPort->SetTimer(122, 800);

		done_count = 0;
		for (int i = 0; i < 1000; i++)
		{
			url = fmt::format("RestHandler sent url {:d}", i);
			PythonPort2->SetTimer(i + 100, 1001 - i);
			PythonPort->RestHandler(url, "", pResponseCallback);
		}

		// Wait - we should see the timer callback triggered and no crashes!
		REQUIRE_NOTHROW
		(
			if (!WaitIOSFnResult(IOS, 7, [&]()
				{
					if(done_count == 1000)
						return true;
					return false;
				}))
			{
				throw("Waiting for 1000 RestHandler responses timed out");
			}
		);
		LOGDEBUG("Last Response {}", sres.get());
	}

	INFO("WebServerTest")
	{
		std::string hroot = "http://localhost:10000";
		std::string h1 = "http://localhost:10000/TestMaster";
		std::string h2 = "http://localhost:10000/TestMaster2";

		// Do a http request to the root port and make sure we are getting the answer we expect.
		std::string expectedresponse("Content-Length: 185\r\nContent-Type: text/html\r\n\n"
			                       "You have reached the PyPort http interface.<br>To talk to a port the url must contain the PyPort name, "
			                       "which is case senstive.<br>Anything beyond this will be passed to the Python code.");

		LOGERROR("If the Tests Hang here, the client making a HTTP request is waiting for an answer from the HTTP server - and is not getting it..");
		std::string callresp;
		bool res = DoHttpRequst("localhost", "10000", "/", callresp);

		LOGDEBUG("GET http://localhost:10000 - We got back {}", callresp);

		REQUIRE(res);
		REQUIRE(expectedresponse == callresp);

		callresp = "";

		res = DoHttpRequst("localhost", "10000", "/TestMaster", callresp);

		LOGDEBUG("GET http://localhost:10000/TestMaster We got back {}", callresp);

		REQUIRE(res);
		REQUIRE(callresp.find("\"processedevents\": 2") != std::string::npos);
		REQUIRE(callresp.find("\"test\": \"GET\"") != std::string::npos);

		res = DoHttpRequst("localhost", "10000", "/TestMaster2", callresp);

		LOGDEBUG("GET http://localhost:10000/TestMaster2 We got back {}", callresp);

		REQUIRE(res);
		REQUIRE(callresp.find("\"processedevents\": 1") != std::string::npos);
		REQUIRE(callresp.find("\"test\": \"GET\"") != std::string::npos);
		LOGERROR("HTTP tests are finished..");
	}

	LOGDEBUG("Starting teardown ports 1-4");

	PythonPort->Disable();
	PythonPort2->Disable();
	PythonPort3->Disable();
	PythonPort4->Disable();

	REQUIRE_NOTHROW
	(
		if (!WaitIOSFnResult(IOS, 10, [&]()
			{
				return (!PythonPort->Enabled() && !PythonPort2->Enabled() && !PythonPort3->Enabled() && !PythonPort4->Enabled());
			}))
		{
			throw("Waiting for Ports to be disabled timed out");
		}
	);
	LOGDEBUG("Ports1-4 Disabled");

	INFO("QueuedEvents")
	{
		LOGERROR("Queued Events Tests..");
		Json::Value portoverride;
		portoverride["EventsAreQueued"] = static_cast<Json::UInt>(1);
		TEST_PythonPort5(portoverride);

		PythonPort5->Enable();
		// The RasPi build is really slow to get ports up and enabled. If the events below are sent before they are enabled - test fail.
		REQUIRE_NOTHROW(
			if (!WaitIOSFnResult(IOS, 11, [&]()
				{
					return (PythonPort5->Enabled());
				}))
			{
				throw std::runtime_error("Waiting for Ports to Enable timed out");
			}
			);

		LOGINFO("Port5 Enabled");

		std::atomic<size_t> block_counts[4];
		odc::SharedStatusCallback_t block_callbacks[4];
		for(size_t i=0; i<4; i++)
		{
			block_counts[i] = 0;
			block_callbacks[i] = std::make_shared<std::function<void (CommandStatus status)>>([&block_counts,i] (CommandStatus status)
				{
					block_counts[i]++;
				});
		}

		IOS->post([&]()
			{
				LOGINFO("Sending Binary Events 1");
				for (int ODCIndex = 1; ODCIndex <= 5000; ODCIndex++)
				{
				      bool val = (ODCIndex % 2 == 0);
				      auto boolevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing1");
				      boolevent->SetPayload<EventType::Binary>(std::move(val));
				      PythonPort5->Event(boolevent, "TestHarness", block_callbacks[0]);
				}
				LOGINFO("Sending Binary Events 1 Done");
			});
		IOS->post([&]()
			{
				LOGINFO("Sending Binary Events 2");
				for (int ODCIndex = 5001; ODCIndex <= 9000; ODCIndex++)
				{
				      bool val = (ODCIndex % 2 == 0);
				      auto boolevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing2");
				      boolevent->SetPayload<EventType::Binary>(std::move(val));
				      PythonPort5->Event(boolevent, "TestHarness", block_callbacks[1]);
				}
				LOGINFO("Sending Binary Events 2 Done");
			});
		IOS->post([&]()
			{
				LOGINFO("Sending Binary Events 3");
				for (int ODCIndex = 9001; ODCIndex <= 12000; ODCIndex++)
				{
				      bool val = (ODCIndex % 2 == 0);
				      auto boolevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing2");
				      boolevent->SetPayload<EventType::Binary>(std::move(val));
				      PythonPort5->Event(boolevent, "TestHarness", block_callbacks[2]);
				}
				LOGINFO("Sending Binary Events 3 Done");
			});
		IOS->post([&]()
			{
				LOGINFO("Sending Binary Events 4");
				for (int ODCIndex = 12001; ODCIndex <= 15000; ODCIndex++)
				{
				      bool val = (ODCIndex % 2 == 0);
				      auto boolevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing2");
				      boolevent->SetPayload<EventType::Binary>(std::move(val));
				      PythonPort5->Event(boolevent, "TestHarness", block_callbacks[3]);
				}
				LOGINFO("Sending Binary Events 4 Done");
			});


		LOGERROR("Waiting for all events to be queued");
		REQUIRE_NOTHROW
		(
			if (!WaitIOSFnResult(IOS, 60, [&]() // RPI very much slower than everything else... 5 works for all other platforms...
				{
					if(block_counts[0]+block_counts[1]+block_counts[2]+block_counts[3] < 15000)
						return false;
					return true;
				}))
			{
				LOGERROR("Waiting for queuing events timed out"); // the throw does not get reported - causes a seg fault in travis
				throw("Waiting for queuing events timed out");
			}
		);
		LOGERROR("All events queued");

		REQUIRE_NOTHROW
		(
			if (!WaitIOSFnResult(IOS, 6, [&]()
				{
					if(PythonPort5->GetEventQueueSize() > 1)
						return false;
					return true;
				}))
			{
				throw("Waiting for queued events timed out");
			}
		);

		// Check that the PyPortSim module has processed the number of events that we have sent?
		// Query through the Restful interface
		std::string callresp = "";

		bool resp = DoHttpRequst("localhost", "10000", "/TestMaster5", callresp);

		REQUIRE(resp);

		LOGDEBUG("GET http://localhost:10000/TestMaster5 We got back {}", callresp);

		std::string matchstr("json\r\n\n");
		size_t pos = callresp.find(matchstr);
		REQUIRE(pos != std::string::npos);

		uint32_t ProcessedEvents = GetProcessedEventsFromJSON(callresp.substr(pos + matchstr.length()));

		LOGDEBUG("The PyPortSim Code Processed {} Events", ProcessedEvents);

		size_t QueueSize = PythonPort5->GetEventQueueSize();

		REQUIRE(ProcessedEvents == 14999);
		REQUIRE(QueueSize == 1);

		LOGDEBUG("Tests Complete, starting teardown");

		PythonPort5->Disable();
		REQUIRE_NOTHROW
		(
			if (!WaitIOSFnResult(IOS, 11, [&]()
				{
					return (!PythonPort->Enabled());
				}))
			{
				throw("Waiting for Ports to be disabled timed out");
			}
		);
		LOGDEBUG("Port5 Disabled");
	}

	STOP_IOS(); // Wait in here for all threads to stop.
	LOGDEBUG("IOS Stopped");

	STANDARD_TEST_TEARDOWN();
	LOGDEBUG("Test Teardown complete");
}

}

#endif
