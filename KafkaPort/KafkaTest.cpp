/*	opendatacon
*
*	Copyright (c) 2019:
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
* KafkaTest.cpp
*
*  Created on: 16/04/2019
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
#include <memory>

#define COMPILE_TESTS

#if defined(COMPILE_TESTS)

// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "KafkaPort.h"


#if defined(NONVSTESTING)
#include <catch.hpp>
#else
#include "spdlog/sinks/msvc_sink.h"
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer - but also has some problems not destructing objects correctly.
#endif

#define SUITE(name) "KafkaTests - " name


extern const char *conffilename1;
extern const char *conffile1;


const char *conffilename1 = "KafkaConfig.conf";

#ifdef _MSC_VER
#pragma region conffiles
#endif
// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile1 = R"001(
{
	"IP" : "127.0.0.1",		// Kafka IP Address for initial connection - Kafka then supplies all the IP's of the Kafka cluster. We dont have to worry about this.
	"Port" : 9092,
	"Topic" : "Test",	// Kafka Topic we will publish messages to. NOTE: we do not specify the partiton (look at the Kafka docs!)
	"SocketTimeout" : 10000,	// msec

	//-------Point conf--------#
	// Index is the ODC Index. All other fields in the point conf are Kafka fields.
	// The key will look like: "HS01234|ANA|56" The first part is the EQType, which can be roughly translated as RTU. The second is ANA (analog) or BIN (binary).
	// The last value is the point number which will be the ODC index. i.e. Set the ODC index to match the point number.
	// The value part of the Kafka pair will be like {"Value": 0.123, "Quality": "ONLINE|RESTART"}

	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "EQType" : "HS01234" },
					{"Index" : 12, "EQType" : "HS01234"}],

	"Analogs" : [	{"Index" : 0, "EQType" : "HS01234"},
					{"Index" : 1, "EQType" : "HS01234"}]

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

void SetupLoggers(spdlog::level::level_enum log_level)
{
	// So create the log sink first - can be more than one and add to a vector.
	#if defined(NONVSTESTING)
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	#else
	auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	#endif
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("testslog.txt", true);

	std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

	auto pLibLogger = std::make_shared<spdlog::logger>("KafkaPort", begin(sinks), end(sinks));
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", begin(sinks), end(sinks));
	pODCLogger->set_level(log_level);
	odc::spdlog_register_logger(pODCLogger);

}
void WriteStartLoggingMessage(std::string TestName)
{
	std::string msg = "Logging for '"+TestName+"' started..";

	if (auto cblogger = odc::spdlog_get("KafkaPort"))
	{
		cblogger->info("------------------");
		cblogger->info(msg);
	}
	else
		std::cout << "Error CBPort Logger not operational";

/*	if (auto odclogger = odc::spdlog_get("opendatacon"))
            odclogger->info(msg);
      else
            std::cout << "Error opendatacon Logger not operational";
            */
}
void TestSetup(std::string TestName, bool writeconffiles = true)
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
	spdlog::drop_all(); // Un-register loggers, and if no other shared_ptr references exist, they will be destroyed.
}

void RunIOSForXSeconds(std::shared_ptr<odc::asio_service> IOS, unsigned int seconds)
{
	// We dont have to consider the timer going out of scope in this use case.
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(seconds));
	timer->async_wait([&IOS](asio::error_code) // [=] all autos by copy, [&] all autos by ref
		{
			// If there was no more work, the asio::io_context will exit from the IOS.run() below.
			// However something is keeping it running, so use the stop command to force the issue.
			IOS->stop();
		});

	IOS->run(); // Will block until all Work is done, or IOS.Stop() is called. In our case will wait for the TCP write to be done,
	// and also any async timer to time out and run its work function (or lambda) - does not need to really do anything!
	// If the IOS runs out of work, it must be reset before being run again.
}
std::thread* StartIOSThread(std::shared_ptr<odc::asio_service> IOS)
{
	return new std::thread([&] { IOS->run(); });
}
void StopIOSThread(std::shared_ptr<odc::asio_service>IOS, std::thread* runthread)
{
	IOS->stop();       // This does not block. The next line will! If we have multiple threads, have to join all of them.
	runthread->join(); // Wait for it to exit
	delete runthread;
}
void WaitIOS(std::shared_ptr<odc::asio_service> IOS, int seconds)
{
	auto timer = IOS->make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(seconds));
	timer->wait();
}


// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP()\
	TestSetup(Catch::getResultCapture().getCurrentTestName());\
	auto IOS = std::make_shared<odc::asio_service>(4); // Max 4 threads

// Used for tests that dont need IOS
#define SIMPLE_TEST_SETUP()\
	TestSetup(Catch::getResultCapture().getCurrentTestName());

#define STANDARD_TEST_TEARDOWN()\
	TestTearDown();\

#define START_IOS(threadcount) \
	LOGINFO("Starting ASIO Threads"); \
	auto work = IOS->make_work(); /* To keep run - running!*/\
	const int ThreadCount = threadcount; \
	std::thread *pThread[threadcount]; \
	for (int i = 0; i < threadcount; i++) pThread[i] = StartIOSThread(IOS);

#define STOP_IOS() \
	LOGINFO("Shutting Down ASIO Threads");    \
	work.reset();     \
	for (int i = 0; i < ThreadCount; i++) StopIOSThread(IOS, pThread[i]);

#define TEST_KafkaPort(overridejson)\
	auto KPort = std::make_shared<KafkaPort>("TestPort", conffilename1, overridejson); \
	KPort->SetIOS(IOS);      \
	KPort->Build();


#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif

namespace SimpleUnitTests
{
TEST_CASE("Util - ConfigFileLoadTest")
{
	//This is a test to load the autogenerated config file from the Mosaic database output.
	STANDARD_TEST_SETUP();
	TEST_KafkaPort(Json::nullValue);

	std::string key;

	bool ares = KPort->GetPointTable()->GetAnalogKafkaKeyUsingODCIndex(1, key);
	REQUIRE(ares);
	REQUIRE(key == "HS01234|ANA|1");

	bool bres = KPort->GetPointTable()->GetBinaryKafkaKeyUsingODCIndex(12, key);
	REQUIRE(bres);
	REQUIRE(key == "HS01234|BIN|12");

	REQUIRE(KPort->GetTopic() == "Test");
	REQUIRE(KPort->GetAddrAccess()->IP == "127.0.0.1");
	REQUIRE(KPort->GetAddrAccess()->Port == "9092");

	odc::msSinceEpoch_t timestamp = 0x0000016bfd90e5a8; // odc::msSinceEpoch();
	std::string result = KPort->to_ISO8601_TimeString(timestamp);
	REQUIRE(result == "2019-07-17T01:34:20.072Z");

	QualityFlags quality(QualityFlags::ONLINE | QualityFlags::RESTART);
	std::string json = KPort->CreateKafkaPayload("HS01234|BIN|1", 0.0123, quality, timestamp);
	REQUIRE(json == "{\"PITag\" : \"HS01234|BIN|1\", \"Index\" : 0, \"Value\" : 0.0123, \"Quality\" : \"|ONLINE|RESTART|\", \"TimeStamp\" : \"2019-07-17T01:34:20.072Z\"}");

	STANDARD_TEST_TEARDOWN();
}
}

namespace EventTests
{
void SendBinaryEvent(std::shared_ptr<KafkaPort>& KPort, int ODCIndex, bool val, QualityFlags qual = QualityFlags::ONLINE)
{
	auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing", qual);
	event->SetPayload<EventType::Binary>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	KPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}

TEST_CASE("ODC - BinaryEvent")
{
	STANDARD_TEST_SETUP();
	TEST_KafkaPort(Json::nullValue);

	KPort->Enable();

	// TEST EVENTS WITH DIRECT CALL
	// Test on a valid binary point
	const int ODCIndex = 1;

	QualityFlags qual = QualityFlags::ONLINE;

	// Do three events so they end up in the same kafka message - otherwise single messages..
	SendBinaryEvent(KPort, ODCIndex, true, qual);
	SendBinaryEvent(KPort, ODCIndex + 1, false, qual);
	SendBinaryEvent(KPort, ODCIndex + 2, true, qual);
	START_IOS(2);

	WaitIOS(IOS, 1);
	for (int i = 0; i < 100; i++)
	{
		SendBinaryEvent(KPort, ODCIndex, true, qual);
		SendBinaryEvent(KPort, ODCIndex + 1, false, qual);
		SendBinaryEvent(KPort, ODCIndex + 2, true, qual);
		SendBinaryEvent(KPort, ODCIndex, false, qual);
		WaitIOS(IOS, 5);
	}


	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}

void SendAnalogEvent(std::shared_ptr<KafkaPort>& KPort, int ODCIndex, double val, QualityFlags qual = QualityFlags::ONLINE)
{
	auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, "Testing", qual);
	event->SetPayload<EventType::Analog>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	KPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}

TEST_CASE("ODC - AnalogEvent")
{
	STANDARD_TEST_SETUP();
	TEST_KafkaPort(Json::nullValue);
	START_IOS(2);

	KPort->Enable();

	// TEST EVENTS WITH DIRECT CALL
	// Test on a valid binary point
	const int ODCIndex = 1;
	QualityFlags qual = QualityFlags::ONLINE | QualityFlags::RESTART;

	SendAnalogEvent(KPort, ODCIndex, 12.345, qual);

	WaitIOS(IOS, 3);

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
}
#endif
