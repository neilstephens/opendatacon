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

#define COMPILE_TESTS

#if defined(COMPILE_TESTS)

// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "CBOutstationPort.h"
#include "CBMasterPort.h"
#include "CBUtility.h"


#if defined(NONVSTESTING)
#include <catch.hpp>
#else
#include "spdlog/sinks/msvc_sink.h"
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer - but also has some problems not destructing objects correctly.
#endif

#define SUITE(name) "CBTests - " name

// To remove GCC warnings
namespace RTUConnectedTests
{
extern const char *CBmasterconffile;
}

extern const char *conffilename1;
extern const char *conffilename2;
extern const char *conffile1;
extern const char *conffile2;

const char *conffilename1 = "CBConfig.conf";
const char *conffilename2 = "CBConfig2.conf";

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

	//-------Point conf--------#
	// Conitel/baker switch
	"IsBakerDevice" : false,

	// If a binary event time stamp is outside 30 minutes of current time, replace the timestamp
	"OverrideOldTimeStamps" : false,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for CB Master responses to a command and number of times to retry a command.
	"CBCommandTimeoutmsec" : 3000,
	"CBCommandRetries" : 1,

	// Master only PollGroups - ignored by outstation
	"PollGroups" : [{"ID" : 1, "PollRate" : 10000, "Group" : 3, "PollType" : "Scan"},
					{"ID" : 2, "PollRate" : 20000, "Group" : 3, "PollType" : "SOEScan"}],

	// The payload location can be 1B, 2A, 2B
	// Where there is a 24 bit result (ACC24) the next payload location will automatically be used. Do not put something else in there!
	// The point table will build a group list with all the data it has to collect for a given group number.
	// We can only use range for Binary and Control. For analog each one has to be defined singularly
	// SOE point definitions are optional. If missing - not an SOE point.

	// Digital IN
	// DIG - 12 bits to a Payload, Channel(bit) 1 to 12. On a range, the Channel is the first Channel in the range.
	// MCA,MCB,MCC - 6 bits and for one payload

	// Analog IN
	// ANA - 1 Channel to a payload,

	// Counter IN
	// ACC12 - 1 to a payload,
	// ACC24 - takes two payloads.

	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 1, "Type" : "DIG", "SOE" : true, "SOEIndex" : 0},
					{"Index" : 12, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA", "SOE" : true, "SOEIndex" : 20},
					{"Index" : 13, "Group" : 3, "PayloadLocation": "2A", "Channel" : 2, "Type" : "MCB", "SOE" : false},
					{"Index" : 14, "Group" : 3, "PayloadLocation": "2A", "Channel" : 3, "Type" : "MCC" }],

	"Analogs" : [	{"Index" : 0, "Group" : 3, "PayloadLocation": "3A","Channel" : 1, "Type":"ANA"},
					{"Index" : 1, "Group" : 3, "PayloadLocation": "3B","Channel" : 1, "Type":"ANA"},
					{"Index" : 2, "Group" : 3, "PayloadLocation": "4A","Channel" : 1, "Type":"ANA"},
					{"Index" : 3, "Group" : 3, "PayloadLocation": "4B","Channel" : 1, "Type":"ANA6"},
					{"Index" : 4, "Group" : 3, "PayloadLocation": "4B","Channel" : 2, "Type":"ANA6"}],

	// None of the counter commands are used  - ACC(12) and ACC24 are not used.
	"Counters" : [	{"Index" : 5, "Group" : 3, "PayloadLocation": "5A","Channel" : 1, "Type":"ACC12"},
					{"Index" : 6, "Group" : 3, "PayloadLocation": "5B","Channel" : 1, "Type":"ACC12"},
					{"Index" : 7, "Group" : 3, "PayloadLocation": "6A","Channel" : 1, "Type":"ACC24"}],

	// CONTROL up to 12 bits per group address, Channel 1 to 12. Python simulator used dual points one for trip one for close.
	"BinaryControls" : [{"Index": 1,  "Group" : 4, "Channel" : 1, "Type" : "CONTROL"},
                        {"Range" : {"Start" : 10, "Stop" : 21}, "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],

	"AnalogControls" : [{"Index": 1,  "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],

	// Special definition, so we know where to find the Remote Status Data in the scanned group.
	"RemoteStatus" : [{"Group":3, "Channel" : 1, "PayloadLocation": "7A"}]

})001";

// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile2 = R"002(
{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 10,
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"IsBakerDevice" : true,

	// The magic Analog point we use to pass through the CB time set command.
	"StandAloneOutstation" : true,

	// Maximum time to wait for CB Master responses to a command and number of times to retry a command.
	"CBCommandTimeoutmsec" : 4000,
	"CBCommandRetries" : 1,

	"Binaries" : [{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 0, "PointType" : "DIG"}],

	// CONTROL up to 12 bits per group address, Channel 1 to 12. Simulator used dual points one for trip one for close.
	"BinaryControls" : [{"Index": 20,  "Group" : 5, "Channel" : 1, "Type" : "CONTROL"}]

})002";

#ifdef _MSC_VER
#pragma endregion

#pragma region TEST_HELPERS
#endif

// Write out the conf file information about into a file so that it can be read back in by the code.
void WriteConfFilesToCurrentWorkingDirectory()
{
	std::ofstream ofs(conffilename1);
	if (!ofs)
		WARN("Could not open conffile1 for writing");

	ofs << conffile1;
	ofs.close();

	std::ofstream ofs2(conffilename2);
	if (!ofs2)
		WARN("Could not open conffile2 for writing");

	ofs2 << conffile2;
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

	auto pLibLogger = std::make_shared<spdlog::logger>("CBPort", begin(sinks),end(sinks));
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

	if (auto cblogger = odc::spdlog_get("CBPort"))
		cblogger->info(msg);
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
	SetupLoggers();
	#endif
	WriteStartLoggingMessage(TestName);

	if (writeconffiles)
		WriteConfFilesToCurrentWorkingDirectory();
}
void TestTearDown(void)
{
	if (auto cblogger = odc::spdlog_get("CBPort"))
		cblogger->info("Test Finished");
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
// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
std::string BuildBinaryStringFromASCIIHexString(const std::string &as)
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

#define TEST_CBMAPort(overridejson)\
	auto CBMAPort = std::make_unique<CBMasterPort>("TestMaster", conffilename1, overridejson); \
	CBMAPort->SetIOS(&IOS);      \
	CBMAPort->Build();

#define TEST_CBMAPort2(overridejson)\
	auto CBMAPort2 = std::make_unique<CBMasterPort>("TestMaster", conffilename2, overridejson); \
	CBMAPort2->SetIOS(&IOS);      \
	CBMAPort2->Build();


#define TEST_CBOSPort(overridejson)      \
	auto CBOSPort = std::make_unique<CBOutstationPort>("TestOutStation", conffilename1, overridejson);   \
	CBOSPort->SetIOS(&IOS);      \
	CBOSPort->Build();

#define TEST_CBOSPort2(overridejson)     \
	auto CBOSPort2 = std::make_unique<CBOutstationPort>("TestOutStation2", conffilename2, overridejson); \
	CBOSPort2->SetIOS(&IOS);     \
	CBOSPort2->Build();

#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif

namespace SimpleUnitTestsCB
{
TEST_CASE("Util - HexStringTest")
{
	SIMPLE_TEST_SETUP();
	std::string ts = "c406400f0b00"  "0000fffe9000";
	std::string w1 = { ToChar(0xc4),0x06,0x40,0x0f,0x0b,0x00 };
	std::string w2 = { 0x00,0x00,ToChar(0xff),ToChar(0xfe),ToChar(0x90),0x00 };

	std::string res = BuildBinaryStringFromASCIIHexString(ts);
	REQUIRE(res == (w1 + w2));
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Util - CBBCHTest")
{
	SIMPLE_TEST_SETUP();
	CBBlockData res = CBBlockData(0x09200028);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x000c0020);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x0009111e);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x00291106);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x22290030);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x22280126);
	REQUIRE(res.BCHPasses());

	res = CBBlockData(0x08080029);
	REQUIRE(res.BCHPasses());
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Util - ParsePayloadString")
{
	SIMPLE_TEST_SETUP();
	PayloadLocationType payloadlocation;

	bool res = CBPointConf::ParsePayloadString("16B", payloadlocation);
	REQUIRE(payloadlocation.Packet == 16);
	REQUIRE(payloadlocation.Position == PayloadABType::PositionB);

	res = CBPointConf::ParsePayloadString("1B", payloadlocation);
	REQUIRE(payloadlocation.Packet == 1);
	REQUIRE(payloadlocation.Position == PayloadABType::PositionB);

	res = CBPointConf::ParsePayloadString("16A", payloadlocation);
	REQUIRE(payloadlocation.Packet == 16);
	REQUIRE(payloadlocation.Position == PayloadABType::PositionA);

	if (auto cblogger = odc::spdlog_get("CBPort")) cblogger->info("Ignore Next Three Errors");

	res = CBPointConf::ParsePayloadString("1A", payloadlocation);
	REQUIRE(payloadlocation.Packet == 1);
	REQUIRE(payloadlocation.Position == PayloadABType::PositionA);

	res = CBPointConf::ParsePayloadString("0A", payloadlocation);
	REQUIRE(res == false);
	REQUIRE(payloadlocation.Position == PayloadABType::Error);

	res = CBPointConf::ParsePayloadString("123", payloadlocation);
	REQUIRE(res == false);
	REQUIRE(payloadlocation.Position == PayloadABType::Error);
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Util - MakeChannelID")
{
	SIMPLE_TEST_SETUP();
	std::string IP = "192.168.1.23";
	std::string Port = "34567";
	bool isaserver = true;

	std::string res = CBConnection::MakeChannelID(IP, Port, isaserver);
	REQUIRE(res == "192.168.1.23:34567:1");

	isaserver = false;
	IP = "127.0.0.1";
	Port = "10000";
	res = CBConnection::MakeChannelID(IP, Port, isaserver);
	REQUIRE(res == "127.0.0.1:10000:0");
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Util - CBIndexTest")
{
	SIMPLE_TEST_SETUP();
	uint8_t group = 1;
	uint8_t channel = 1;
	PayloadLocationType payloadlocation(1, PayloadABType::PositionA);

	// Top 4 bits group (0-15), Next 4 bits channel (1-12), next 4 bits payload packet number (0-15), next 4 bits 0(A) or 1(B)
	uint16_t CBIndex = CBPointTableAccess::GetCBPointMapIndex(group, channel, payloadlocation);
	REQUIRE(CBIndex == 0x1100);

	group = 15;
	channel = 12;
	payloadlocation= PayloadLocationType(16, PayloadABType::PositionB);
	CBIndex = CBPointTableAccess::GetCBPointMapIndex(group, channel, payloadlocation);
	REQUIRE(CBIndex == 0xFCF1);
	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("Util - CBPort::BuildUpdateTimeMessage")
{
	SIMPLE_TEST_SETUP();
	uint8_t address = 1;
	CBTime cbtime = static_cast<CBTime>(0x0000016338b6d4fb); // A value around June 2018

	CBMessage_t CompleteCBMessage;

	CBPort::BuildUpdateTimeMessage(address, cbtime, CompleteCBMessage);

	REQUIRE(CompleteCBMessage.size() == 2);

	uint16_t B0Data = CompleteCBMessage[0].GetB();
	uint16_t A1Data = CompleteCBMessage[1].GetA();
	uint16_t B1Data = CompleteCBMessage[1].GetB();

	REQUIRE(B0Data == 0x0020);
	REQUIRE(A1Data == 0x0f04);
	REQUIRE(B1Data == 0x00fb);
	REQUIRE(CompleteCBMessage[0].IsEndOfMessageBlock() == false);
	REQUIRE(CompleteCBMessage[1].IsEndOfMessageBlock() == true);
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Util - SOEEventFormat")
{
	SIMPLE_TEST_SETUP();
	SOEEventFormat S;

	CBTime cbtime = static_cast<CBTime>(0x0000016338b6d4fb); // A value around June 2018

	S.Group = 5;     // 3 bits b101
	S.Number = 0x41; // 7 bits b1000001 - 0x41
	S.ValueBit = true;
	S.QualityBit = false;
	S.TimeFormatBit = true; // Long format

	S.Hour = 0x11;         // 5 bits	b10001 - 0x11
	S.Minute = 0x21;       // 6 bits b10 0001 - 0x21
	S.Second = 0x21;       // 6 bits b10 0001 - 0x21
	S.Millisecond = 0x201; // 10 bits b10 0000 0001 - 0x201
	S.LastEventFlag = false;

	uint64_t res = S.GetFormattedData();

	REQUIRE(res == 0xB06C618601000000);

	// Now test our BitArray handling.
	std::array<bool, MaxSOEBits> BitArray;
	for (int i = 0; i < 64; i++)
		BitArray[i] = ((res >> (63 - i)) & 0x01) == 0x01;

	uint8_t newstartbit = 0;
	SOEEventFormat SOE(BitArray, 0, newstartbit,0); // Build a new SOE record from the BitArray

	REQUIRE(newstartbit == 41);
	REQUIRE(SOE.GetResultBitLength() == 41); // Should be long...
	REQUIRE(SOE.GetFormattedData() == 0xB06C618601000000);
	REQUIRE(SOE.Group == 5);
	REQUIRE(SOE.Number == 0x41);
	REQUIRE(SOE.ValueBit == true);
	REQUIRE(SOE.TimeFormatBit == true);
	REQUIRE(SOE.Hour == 0x11);
	REQUIRE(SOE.Minute == 0x21);
	REQUIRE(SOE.Second == 0x21);
	REQUIRE(SOE.Millisecond == 0x201);
	REQUIRE(SOE.LastEventFlag == false);


	S.TimeFormatBit = false; // Short format
	S.LastEventFlag = true;

	res = S.GetFormattedData();

	REQUIRE(res == 0xb064300c00000000);

	// Now test our BitArray handling.
	std::array<bool, MaxSOEBits> BitArray2;
	for (int i = 0;  i < 64; i++)
		BitArray2[i] = ((res >> (63 - i)) & 0x01) == 0x01;

	newstartbit = 0;
	SOEEventFormat SOE2(BitArray2, 0, newstartbit,0); // Build a new SOE record from the BitArray

	REQUIRE(newstartbit == 30);
	REQUIRE(SOE2.GetResultBitLength() == 41); // Always long
	REQUIRE(SOE2.GetFormattedData() == 0xb068008601800000);
	REQUIRE(SOE2.Group == 5);
	REQUIRE(SOE2.Number == 0x41);
	REQUIRE(SOE2.ValueBit == true);
	REQUIRE(SOE2.TimeFormatBit == true);
	REQUIRE(SOE2.Second == 0x21);
	REQUIRE(SOE2.Millisecond == 0x201);
	REQUIRE(SOE2.LastEventFlag == true);

	STANDARD_TEST_TEARDOWN();
}

#ifdef _MSC_VER
#pragma region Block Tests
#endif

TEST_CASE("CBBlock - ClassConstructor1")
{
	SIMPLE_TEST_SETUP();
	CBBlockArray msg = { 0x09,0x20,0x00,0x28}; // From a packet capture - this is the address packet

	CBBlockData b(msg);

	REQUIRE(b.GetStationAddress() == 9);
	REQUIRE(b.GetGroup() == 2);
	REQUIRE(b.GetFunctionCode() == 0);
	REQUIRE(b.GetB() == 0);
	REQUIRE(b.IsEndOfMessageBlock() == false);
	REQUIRE(b.IsAddressBlock());
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());
	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("CBBlock - ClassConstructor2")
{
	SIMPLE_TEST_SETUP();
	CBBlockData b2("22290030");
	REQUIRE(b2.GetData() == 0x22290030);
	REQUIRE(b2.GetA() == 0x222);
	REQUIRE(b2.GetB() == 0x200);
	REQUIRE(b2.IsEndOfMessageBlock() == false);
	REQUIRE(b2.IsDataBlock());
	REQUIRE(b2.BCHPasses());
	REQUIRE(b2.CheckBBitIsZero());

	CBBlockData b3("08080029");
	REQUIRE(b3.GetData() == 0x08080029);
	REQUIRE(b3.GetA() == 0x080);
	REQUIRE(b3.GetB() == 0x000);
	REQUIRE(b3.IsEndOfMessageBlock() == true);
	REQUIRE(b3.IsDataBlock());
	REQUIRE(b3.BCHPasses());
	REQUIRE(b3.CheckBBitIsZero());
	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("CBBlock - ClassConstructor3")
{
	SIMPLE_TEST_SETUP();
	uint8_t d[] = { 0x09, 0x20, 0x00,0x28 };
	CBBlockData b(d[0], d[1], d[2], d[3]);

	REQUIRE(b.GetStationAddress() == 9);
	REQUIRE(b.GetGroup() == 2);
	REQUIRE(b.GetFunctionCode() == 0);
	REQUIRE(b.GetB() == 0);
	REQUIRE(b.IsEndOfMessageBlock() == false);
	REQUIRE(b.IsAddressBlock());
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());

	CBBlockData c(numeric_cast<char>(d[0]), numeric_cast<char>(d[1]), numeric_cast<char>(d[2]), numeric_cast<char>(d[3]));

	REQUIRE(c.GetStationAddress() == 9);
	REQUIRE(c.GetGroup() == 2);
	REQUIRE(c.GetFunctionCode() == 0);
	REQUIRE(c.GetB() == 0);
	REQUIRE(c.IsEndOfMessageBlock() == false);
	REQUIRE(c.IsAddressBlock());
	REQUIRE(c.BCHPasses());
	REQUIRE(c.CheckBBitIsZero());
	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("CBBlock - ClassConstructor4")
{
	SIMPLE_TEST_SETUP();
	uint16_t AData = 0xF23;
	uint16_t BData = 0x102;
	bool lastblock = false;

	CBBlockData b(AData, BData, lastblock);

	REQUIRE(b.GetA() == AData);
	REQUIRE(b.GetB() == BData);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsAddressBlock() == false);
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());

	AData = 0x023;

	b.SetA(AData);
	REQUIRE(b.GetA() == AData);
	REQUIRE(b.GetB() == BData);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsAddressBlock() == false);
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());

	BData = 0xF01;

	b.SetB(BData);
	REQUIRE(b.GetB() == BData);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsAddressBlock() == false);
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());
	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("CBBlock - ClassConstructor5")
{
	SIMPLE_TEST_SETUP();
	// Need to test the construction of an address block. What is B set to??
	uint8_t StationAddress = 4;
	uint8_t Group = 2;
	uint8_t FunctionCode = 5;
	uint16_t BData = 0x040;
	bool lastblock = false;

	CBBlockData b(StationAddress, Group, FunctionCode, BData, lastblock);
	REQUIRE(b.GetStationAddress() == StationAddress);
	REQUIRE(b.GetGroup() == Group);
	REQUIRE(b.GetFunctionCode() == FunctionCode);
	REQUIRE(b.GetB() == BData);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsAddressBlock());
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());

	b.DoBakerConitelSwap(); // Swap Group and Station

	REQUIRE(b.GetStationAddress() == Group);
	REQUIRE(b.GetGroup() == StationAddress);
	REQUIRE(b.GetFunctionCode() == FunctionCode);
	REQUIRE(b.GetB() == BData);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsAddressBlock());
	REQUIRE(b.BCHPasses());
	REQUIRE(b.CheckBBitIsZero());
	STANDARD_TEST_TEARDOWN();
}

#ifdef _MSC_VER
#pragma endregion Block Tests
#endif
}

namespace StationTests
{
void SendBinaryEvent(std::unique_ptr<CBOutstationPort> &CBOSPort, int ODCIndex, bool val, QualityFlags qual = QualityFlags::ONLINE, msSinceEpoch_t time = msSinceEpoch())
{
	auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing", qual, time);
	event->SetPayload<EventType::Binary>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	CBOSPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}

TEST_CASE("Station - ScanRequest F0")
{
	// So we send a Scan F0 packet to the Outstation, it responds with the data in the point table.
	// Then we update the data in the point table, scan again and check the data we get back.
	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	uint8_t station = 9;
	uint8_t group = 3;
	CBBlockData commandblock(station, group, FUNC_SCAN_DATA, 0, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);


	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

	// Send the commasnd in as if came from TCP channel
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Check the command is formatted correctly
	std::string DesiredResult = BuildBinaryStringFromASCIIHexString("0937ffaa" // Echoed block plus data 1B
		                                                          "a8080020" // Data 2A and 2B
		                                                          "00080006"
		                                                          "00080006"
		                                                          "00080006"
		                                                          "00080006"
		                                                          "55580013");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);


	// Call the Event functions to set the CB table data to what we are expecting to get back.
	// Write to the analog registers that we are going to request the values for.
	for (int ODCIndex = 0; ODCIndex < 3; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(std::move(1024 + ODCIndex));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);
		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}
	for (int ODCIndex = 3; ODCIndex < 5; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(std::move(1 + ODCIndex));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);
		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}
	for (int ODCIndex = 5; ODCIndex < 8; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex);
		event->SetPayload<EventType::Counter>(std::move(numeric_cast<unsigned int>(1024 + ODCIndex)));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}

	for (int ODCIndex = 0; ODCIndex < 12; ODCIndex++)
	{
		SendBinaryEvent(CBOSPort, ODCIndex, ((ODCIndex % 2) == 0));
	}

	// MCA,MCB,MCC Set to starting values
	SendBinaryEvent(CBOSPort, 12, true);
	SendBinaryEvent(CBOSPort, 13, false);
	SendBinaryEvent(CBOSPort, 14, true);

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Should now get different data!
	DesiredResult = BuildBinaryStringFromASCIIHexString("09355516" // Echoed block plus data 1B
		                                              "8808000c" // Data 2A and 2B
		                                              "400a00b6"
		                                              "402882b8"
		                                              "405a032c"
		                                              "40780030"
		                                              "55580013");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	// Test the MC bit types.
	// MCA - The change bit is set when the input changes from open to closed (1-->0). The status bit is 0 when the contact is CLOSED.
	// MCB - The change bit is set when the input changes from closed to open (0-->1). The status bit is 0 when the contact is OPEN.
	// MCC - The change bit is set when the input has gone through more than one change of state. The status bit is 0 when the contact is OPEN.

	// {"Index" : 12, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA"},
	// {"Index" : 13, "Group" : 3, "PayloadLocation": "2A", "Channel" : 2, "Type" : "MCB"},
	// {"Index" : 14, "Group" : 3, "PayloadLocation": "2A", "Channel" : 3, "Type" : "MCC"}],

	// Now make changes to trigger the change flags being set
	SendBinaryEvent(CBOSPort, 12, false);
	SendBinaryEvent(CBOSPort, 13, true);
	SendBinaryEvent(CBOSPort, 14, false); // Cause more than one change.
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	SendBinaryEvent(CBOSPort, 14, true);

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// We are setting Channels 1,2,3. So should be the bits Change1, Status1, Change2, Status2, Change3, Status3.
	// The 2A block was 0x880 Ch 1 = 1, Ch 2 = 0, Ch 3 = 1
	// It becomes 0x780 Ch1 = 0 S1 =1, Ch2=1,S2 =1,Ch3=1,S3 =1
	// Finally 0x800	ch1=1,ch2=0,ch3=0
	DesiredResult = BuildBinaryStringFromASCIIHexString("09355516" // Echoed block plus data 1B
		                                              "78080000" // Data 2A and 2B
		                                              "400a00b6"
		                                              "402882b8"
		                                              "405a032c"
		                                              "40780030"
		                                              "55580013");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	// Now do the changes that do not trigger the change bits being set.
	SendBinaryEvent(CBOSPort, 12, true);
	SendBinaryEvent(CBOSPort, 13, false);
	SendBinaryEvent(CBOSPort, 14, false); // Only 1 change, need 2 to trigger

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Should now get different data!
	DesiredResult = BuildBinaryStringFromASCIIHexString("09355516" // Echoed block plus data 1B
		                                              "80080022" // Data 2A and 2B
		                                              "400a00b6"
		                                              "402882b8"
		                                              "405a032c"
		                                              "40780030"
		                                              "55580013");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	CBOSPort->Disable();

	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("Station - SOERequest F10")
{
	// So we send a SOE Request F10 packet to the Outstation, it responds with the data in the SOE Queue/List.
	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	// Request SOE Data
	uint8_t station = 9;
	uint8_t group = 3;
	CBBlockData commandblock(station, group, FUNC_SEND_NEW_SOE, 0, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);


	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

	// Send the command in as if came from TCP channel
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Check that we got nothing back ? No Events yet?
	std::string DesiredResult = BuildBinaryStringFromASCIIHexString("a930002d"); // Echoed block plus

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);


	// Call the Event functions to put some SOE data into the queue ODC Binaries 0 to 12 will capture SOE data.
	// For testing we need to fix the time so that we dont get changing data on every pass.
	msSinceEpoch_t time = 0x0000016734934659; // 21/11/2018 3:42pm  msSinceEpoch();

	for (int ODCIndex = 0; ODCIndex < 12; ODCIndex++)
	{
		SendBinaryEvent(CBOSPort, ODCIndex, ((ODCIndex % 2) == 0), QualityFlags::ONLINE, time++);
	}
	SendBinaryEvent(CBOSPort,0, true, QualityFlags::ONLINE, time++);
	SendBinaryEvent(CBOSPort, 0, false, QualityFlags::ONLINE, time++);
	SendBinaryEvent(CBOSPort, 0, true, QualityFlags::ONLINE, time++);
	SendBinaryEvent(CBOSPort, 12, true, QualityFlags::ONLINE, time++);


	// Send the SOE Scan command again.
	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Now we should get back the SOE queued events.
	DesiredResult = BuildBinaryStringFromASCIIHexString("a933010c92a8c93293080016004e023c000898100008021cc048000013080032004e023c000898100008021cc048000013080032004e023c000898100008660d");
	//										"a933010c92a8c932930803244a8e0226192d9836006a580cc048c908d30803364b8e0238192f9800006a603cc048c99c130803004c8e02281939981c006a6e31");
	// OLD VALUE???							"a933010c192c9536006c981ec04a5434b308c9324b08032a192e0232006e9828c04a5c16f308c9204c08033a193e023e00689838c04a64263308c9824dc8033f");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);


	// Now send the SOE resend command and make sure we get the same result.
	commandblock = CBBlockData(station, group, FUNC_REPEAT_SOE, 0, true);

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);


	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	// Check if the lastmessagebit is set, if so request the remaining events - we happen to know it it is, so ask for them.
	commandblock = CBBlockData(station, group, FUNC_SEND_NEW_SOE, 0, true);

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("a933010c92a8c9a653080020004e023c0008981010086733");
	//												"a933010c92a8c9a6530803124d8e0236193b982a106a7723");
	//	OLD VALUE									"a933010c193c953a006a980ec04a6c047308c9904ee8833f");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	CBOSPort->Disable();

	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Station - BinaryEvent")
{
	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	// Set up a callback for the result - assume sync operation at the moment
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// TEST EVENTS WITH DIRECT CALL
	// Test on a valid binary point
	const int ODCIndex = 1;

	EventTypePayload<EventType::Binary>::type val = true;

	std::shared_ptr<odc::EventInfo> event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
	event->SetPayload<EventType::Binary>(std::move(val));

	CBOSPort->Event(event, "TestHarness", pStatusCallback);

	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set. 1 is defined

	res = CommandStatus::NOT_AUTHORIZED;

	// Test on an undefined binary point. 40 NOT defined in the config text at the top of this file.
	auto event2 = std::make_shared<EventInfo>(EventType::Binary, ODCIndex + 200);
	event2->SetPayload<EventType::Binary>(std::move(val));

	CBOSPort->Event(event2, "TestHarness", pStatusCallback);
	REQUIRE((res == CommandStatus::UNDEFINED)); // The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...
	// Wait for some period to do something?? Check that the port is open and we can connect to it?

	STANDARD_TEST_TEARDOWN();
}

TEST_CASE("Station - CONTROL Commands")
{
	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	uint8_t station = 9;
	uint8_t group = 3;
	uint16_t BData = 1;
	CBBlockData commandblock = CBBlockData(station, group, FUNC_CLOSE, BData, true); // Trip is OPEN or OFF

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	std::string DesiredResult = BuildBinaryStringFromASCIIHexString("493000a3");

	REQUIRE(Response == DesiredResult); // OK PendingCommand

	// Access the pending command in the OutStation, and check it is set to what we think it should be.
	PendingCommandType pc = CBOSPort->GetPendingCommand(group);
	REQUIRE(pc.Command == PendingCommandType::CommandType::Close);
	REQUIRE(pc.Data == BData);

	// Now send the excecute command.
	commandblock = CBBlockData(station, group, FUNC_EXECUTE_COMMAND, 0, true);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	Response = "Not Set";

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("19300033");

	REQUIRE(Response == DesiredResult);

	uint8_t res;
	bool hasbeenset;
	size_t ODCIndex = 21;
	CBOSPort->GetPointTable()->GetBinaryControlValueUsingODCIndex(ODCIndex, res, hasbeenset);
	REQUIRE(res == 1);
	REQUIRE(hasbeenset == true);

	commandblock = CBBlockData(station, group, FUNC_TRIP, BData, true); // Trip is OPEN or OFF

	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	Response = "Not Set";

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("2930009d");

	REQUIRE(Response == DesiredResult); // OK PendingCommand

	PendingCommandType pc2 = CBOSPort->GetPendingCommand(group);
	REQUIRE(pc2.Command == PendingCommandType::CommandType::Trip);
	REQUIRE(pc2.Data == BData);

	// Now send the excecute command.
	commandblock = CBBlockData(station, group, FUNC_EXECUTE_COMMAND, 0, true);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	Response = "Not Set";

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("19300033");

	REQUIRE(Response == DesiredResult); // OK PendingCommand

	CBOSPort->GetPointTable()->GetBinaryControlValueUsingODCIndex(ODCIndex, res, hasbeenset);
	REQUIRE(res == 0);
	REQUIRE(hasbeenset == true);


	// Do an Analog Set Point
	BData = 0xAAA;
	commandblock = CBBlockData(station, group, FUNC_SETPOINT_A, BData, true); // Analog SetA

	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	Response = "Not Set";

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("3935552d");

	REQUIRE(Response == DesiredResult); // OK PendingCommand

	PendingCommandType pc3 = CBOSPort->GetPendingCommand(group);
	REQUIRE(pc3.Command == PendingCommandType::CommandType::SetA);
	REQUIRE(pc3.Data == BData);

	// Now send the excecute command.
	commandblock = CBBlockData(station, group, FUNC_EXECUTE_COMMAND, 0, true);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	Response = "Not Set";

	// Send the PendingCommand
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	DesiredResult = BuildBinaryStringFromASCIIHexString("19300033");

	REQUIRE(Response == DesiredResult); // OK PendingCommand

	uint16_t res16 = 0;
	ODCIndex = 1;
	CBOSPort->GetPointTable()->GetAnalogControlValueUsingODCIndex(ODCIndex, res16, hasbeenset);
	REQUIRE(res16 == BData);
	REQUIRE(hasbeenset == true);

	STANDARD_TEST_TEARDOWN();
}
}

namespace MasterTests
{
void SendBinaryEvent(std::unique_ptr<CBOutstationPort> &CBOSPort, int ODCIndex, bool val, QualityFlags qual = QualityFlags::ONLINE, msSinceEpoch_t time = msSinceEpoch())
{
	auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "Testing", qual, time);
	event->SetPayload<EventType::Binary>(std::move(val));

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	CBOSPort->Event(event, "TestHarness", pStatusCallback);
	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
}

TEST_CASE("Master - Scan Request F0")
{
	// So here we send out an F0 command (so the Master is expecting it back)
	// Then we decode the response from the point table group setup, and fire off the appropriate events.
	STANDARD_TEST_SETUP();

	TEST_CBMAPort(Json::nullValue);

	Json::Value portoverride;
	portoverride["Port"] = static_cast<Json::UInt64>(1001);
	TEST_CBOSPort(portoverride);

	START_IOS(1);

	CBMAPort->Subscribe(CBOSPort.get(), "TestLink"); // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
	// Usually is a cross subscription, where each subscribes to the other.
	CBOSPort->Enable();
	CBMAPort->Enable();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	CBMAPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

	// Now send a request analog unconditional command - asio does not need to run to see this processed, in this test set up
	// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
	CBBlockData sendcommandblock(9, 3, FUNC_SCAN_DATA, 0, true);
	CBMAPort->QueueCBCommand(sendcommandblock, nullptr);

	WaitIOS(IOS, 1);

	// We check the command, but it does not go anywhere, we inject the expected response below.
	const std::string DesiredResponse = BuildBinaryStringFromASCIIHexString("09300025");
	REQUIRE(Response == DesiredResponse);

	// We now inject the expected response to the command above.
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);

	// From the outstation test above!!
	std::string Payload = BuildBinaryStringFromASCIIHexString("09355516" // Echoed block plus data 1B
		                                                    "01480028" // Data 2A and 2B
		                                                    "400a00b6"
		                                                    "402882b8"
		                                                    "405a032c"
		                                                    "40780030"
		                                                    "55580013");
	output << Payload;

	// Send the Analog Unconditional command in as if came from TCP channel. This should stop a resend of the command due to timeout...
	CBMAPort->InjectSimulatedTCPMessage(write_buffer);

	WaitIOS(IOS, 5);

	// To check the result, see if the points in the master point list have been changed to the correct values.
	bool hasbeenset;
	uint16_t res;

	//ANA
	for (size_t ODCIndex = 0; ODCIndex < 3; ODCIndex++)
	{
		CBMAPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
		REQUIRE(res == (1024 + ODCIndex));
	}
	//ANA6
	for (size_t ODCIndex = 3; ODCIndex < 5; ODCIndex++)
	{
		CBMAPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
		REQUIRE(res == (1 + ODCIndex));
	}
	//Counters
	for (size_t ODCIndex = 5; ODCIndex < 8; ODCIndex++)
	{
		CBMAPort->GetPointTable()->GetCounterValueUsingODCIndex(ODCIndex, res, hasbeenset);
		REQUIRE(res == (1024 + ODCIndex));
	}

	for (size_t ODCIndex = 0; ODCIndex < 12; ODCIndex++)
	{
		bool changed;
		uint8_t res;
		CBMAPort->GetPointTable()->GetBinaryValueUsingODCIndexAndResetChangedFlag(ODCIndex, res, changed, hasbeenset);
		REQUIRE(res == ((ODCIndex + 1) % 2));
	}

	// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
	// Need to give ASIO time to process them?
	WaitIOS(IOS, 1);

	for (size_t ODCIndex = 0; ODCIndex < 3; ODCIndex++)
	{
		CBOSPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
		REQUIRE(res == (1024 + ODCIndex));
	}
	for (size_t ODCIndex = 3; ODCIndex < 5; ODCIndex++)
	{
		CBOSPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
		//		REQUIRE(res == (1+ODCIndex));
	}
	for (size_t ODCIndex = 5; ODCIndex < 8; ODCIndex++)
	{
		CBOSPort->GetPointTable()->GetCounterValueUsingODCIndex(ODCIndex, res, hasbeenset);
		REQUIRE(res == (1024 + ODCIndex));
	}
	for (size_t ODCIndex = 0; ODCIndex < 12; ODCIndex++)
	{
		uint8_t res;
		bool changed;
		CBOSPort->GetPointTable()->GetBinaryValueUsingODCIndexAndResetChangedFlag(ODCIndex, res, changed, hasbeenset);
		REQUIRE(res == ((ODCIndex + 1) % 2));
	}

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Master - SOE Request F10")
{
	// So we have cross coupled Master/OutStation as if they were linked through ODC. We then send out a SOEScan command from the master, and feed it
	// actual data generated by our code from the Station test above.
	// The Master will then decode this data, and fire events off to the linked OutStation. We can then look at the OutStation Event Queue to see
	// if everything worked correctly.

	STANDARD_TEST_SETUP();

	Json::Value MAportoverride;

	TEST_CBMAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001); // So the two ports dont actually connect to each other!! Not what we are testing here.
	OSportoverride["StandAloneOutstation"] = false;
	TEST_CBOSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. CBOSPort is registering to get CBPort messages.
	// Usually is a cross subscription, where each subscribes to the other.
	CBMAPort->Subscribe(CBOSPort.get(), "TestLink");
	CBOSPort->Subscribe(CBMAPort.get(), "TestLink");

	CBOSPort->Enable();
	CBMAPort->Enable();

	CBMAPort->EnablePolling(false); // Don't want the timer triggering this. We will call manually.

	// Hook the output functions
	std::string OSResponse = "Not Set";
	CBOSPort->SetSendTCPDataFn([&OSResponse](std::string CBMessage) { OSResponse = CBMessage; });

	std::string MAResponse = "Not Set";
	CBMAPort->SetSendTCPDataFn([&MAResponse](std::string CBMessage) { MAResponse = CBMessage; });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("Test actual returned data for F10 SOE Scan");
	{
		MAResponse = "Not Set";

		// Send an SOE Scan Command from the Master
		uint8_t Group = 3;
		CBMAPort->SendFn10SOEScanCommand(Group, nullptr);

		WaitIOS(IOS, 2);

		// Check that the command was formatted correctly.
		const std::string DesiredResponse = BuildBinaryStringFromASCIIHexString("a930002d");
		REQUIRE(MAResponse == DesiredResponse);

		std::string CommandResponse = BuildBinaryStringFromASCIIHexString("a933010c92a8c93293080016004e023c000898100008021cc048000013080032004e023c000898100008021cc048000013080032004e023c000898100008660d");
		MAoutput << CommandResponse;
		CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer); // Sends MAoutput

		WaitIOS(IOS, 4); // Should be 3

		// We should now have data available...
		// Should match what we created in the Station test - the day part of the times will match, that actual day will not - we add the current day to get a "full" time
		// msSinceEpoch_t time = 0x0000016734934659; // 21/11/2018 3:42pm  msSinceEpoch();
		REQUIRE(CBOSPort->GetPointTable()->TimeTaggedDataAvailable() == true); // Uses a strand queue with wait for result...

		// Get the list of time tagged events, and check...
		std::vector<CBBinaryPoint> PointList = CBOSPort->GetPointTable()->DumpTimeTaggedPointList();
		REQUIRE(PointList.size() == 0x3a);
		REQUIRE(PointList[0].GetIndex() == 0);

		CBTime ChangedTime = GetTimeOfDayOnly(PointList[0].GetChangedTime());
		REQUIRE(ChangedTime == 0x00000164ee106081);

		REQUIRE(PointList[0x30].GetIndex() == 0x1e);
		REQUIRE(PointList[0x30].GetBinary() == 0);

		ChangedTime = GetTimeOfDayOnly(PointList[0x30].GetChangedTime());
		REQUIRE(ChangedTime == 0x00000164ee106081);
	}

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Master - F9 Time Test Using TCP")
{
	// Test the response to a TimeCommand
	STANDARD_TEST_SETUP();
	// Outstations are as for the conf files
	TEST_CBOSPort(Json::nullValue);

	// The masters need to be TCP Clients - should be only change necessary.
	Json::Value MAportoverride;
	MAportoverride["TCPClientServer"] = "CLIENT";
	TEST_CBMAPort(MAportoverride);

	START_IOS(1);

	CBOSPort->Enable();
	CBMAPort->Enable();

	WaitIOS(IOS, 1);

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on CONTROL command result : {} ", static_cast<int>(command_stat));
			res = command_stat;
		});

	// Send an ODC DigitalOutput command to the Master.
	CBMAPort->SendFn9TimeUpdate(pStatusCallback);

	// Wait for it to go to the OutStation and Back again
	WaitIOS(IOS, 4);

	REQUIRE(res == CommandStatus::SUCCESS);

	CBOSPort->Disable();
	CBMAPort->Disable();

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Master - Control Output Multi-drop Test Using TCP")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination. The Stations will be 0x7C, 0x7D
	// Create two masters, two stations, and then see if they can share the TCP connection successfully.
	STANDARD_TEST_SETUP();
	// Outstations are as for the conf files
	TEST_CBOSPort(Json::nullValue);
	TEST_CBOSPort2(Json::nullValue);

	// The masters need to be TCP Clients - should be only change necessary.
	Json::Value MAportoverride;
	MAportoverride["TCPClientServer"] = "CLIENT";
	TEST_CBMAPort(MAportoverride);

	Json::Value MAportoverride2;
	MAportoverride2["TCPClientServer"] = "CLIENT";
	TEST_CBMAPort2(MAportoverride2);


	START_IOS(1);

	CBOSPort->Enable();
	CBOSPort2->Enable();
	CBMAPort->Enable();
	CBMAPort2->Enable();

	// Allow everything to get setup.
	WaitIOS(IOS, 2);

	// So to do this test, we are going to send an Event into the Master which will require it to send a POM command to the outstation.
	// We should then have an Event triggered on the outstation caused by the POM. We need to capture this to check that it was the correct POM Event.

	// Send a POM command by injecting an ODC event to the Master
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on CONTROL command result : " + std::to_string(static_cast<int>(command_stat)));
			res = command_stat;
		});

	bool point_on = true;
	uint16_t ODCIndex = 1;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	// Send an ODC DigitalOutput command to the Master.
	CBMAPort->Event(event, "TestHarness", pStatusCallback);

	// Wait for it to go to the OutStation and Back again
	WaitIOS(IOS, 4);

	REQUIRE(res == CommandStatus::SUCCESS);

	// Now do the other Master/Outstation combination.
	CommandStatus res2 = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback2 = std::make_shared<std::function<void(CommandStatus)>>([=, &res2](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on CONTROL command result : " + std::to_string(static_cast<int>(command_stat)));
			res2 = command_stat;
		});

	ODCIndex = 20;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val2;
	val2.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event2 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event2->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val2));

	// Send an ODC DigitalOutput command to the Master.
	CBMAPort2->Event(event2, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	WaitIOS(IOS, 3);

	REQUIRE(res2 == CommandStatus::SUCCESS);

	////////////////////////////
	// Now do an Analog Control point on the first pair
	ODCIndex = 1;

	EventTypePayload<EventType::AnalogOutputInt16>::type val3;
	val3.first = 0xaaa;

	auto event3 = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ODCIndex, "TestHarness");
	event3->SetPayload<EventType::AnalogOutputInt16>(std::move(val3));
	res2 = CommandStatus::NOT_AUTHORIZED;

	// Send an ODC DigitalOutput command to the Master.
	CBMAPort->Event(event3, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	WaitIOS(IOS, 3);

	REQUIRE(res2 == CommandStatus::SUCCESS);

	CBOSPort->Disable();
	CBOSPort2->Disable();

	CBMAPort->Disable();
	CBMAPort2->Disable();

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
TEST_CASE("Master - Cause a Command Resend on Timeout Using subscribed Master and Outstation")
{
	STANDARD_TEST_SETUP();
	// Outstations are as for the conf files
	TEST_CBOSPort(Json::nullValue);

	// The master needs to be a TCP Client - should be only change necessary.
	Json::Value MAportoverride;
	MAportoverride["TCPClientServer"] = "CLIENT";
	TEST_CBMAPort(MAportoverride);

	START_IOS(1);
	CBMAPort->Subscribe(CBOSPort.get(), "TestLink"); // The subscriber is just another port. CBOSPort is registering to get CBPort messages.

	CBOSPort->Enable();
	CBMAPort->Enable();

	// Allow everything to get setup.
	WaitIOS(IOS, 2);

	std::string Response = "Not Set";
	CBMAPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Master sends a scan command
	CBBlockData sendcommandblock(9, 3, FUNC_SCAN_DATA, 0, true);
	CBMAPort->QueueCBCommand(sendcommandblock, nullptr);

	WaitIOS(IOS, 1);

	// We check the command, but it does not go anywhere, we would normally (in testing) inject the expected response below.
	const std::string DesiredResponse = BuildBinaryStringFromASCIIHexString("09300025");
	REQUIRE(Response == DesiredResponse);

	// Instead of injecting the expected response, we don't send anything, which should result in a timeout.
	// That timeout should then result in the point quality being set to COMMS_LOST??

	// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
	// Need to give ASIO time to process them
	WaitIOS(IOS, 10);

	// To check the result, the quality of the points will be set to comms_lost
	/*
	uint16_t res = 0;
	bool hasbeenset;
	size_t ODCIndex = 0;


	TODO: Finish test for comms lost and quality flag setting also go back and check MD3.

	CBOSPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
	REQUIRE(res == 0x8000);

	CBMAPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
	REQUIRE(res == 0x8000);

	*/
	CBOSPort->Disable();

	CBMAPort->Disable();

	STOP_IOS();
	STANDARD_TEST_TEARDOWN();
}
}


namespace RTUConnectedTests
{
// Wire shark filter for this OutStation "CB and ip.addr== 172.21.136.80"
/*
const char *CBmasterconffile = R"011(
{
      "IP" : "172.21.136.80",
      "Port" : 5001,
      "OutstationAddr" : 32,
      "TCPClientServer" : "CLIENT",	// I think we are ignoring this!
      "LinkNumRetry": 4,

      //-------Point conf--------#
      // We have two modes for the digital/binary commands. Can be one or the other - not both!
      "IsBakerDevice" : true,
      "StandAloneOutstation" : true,

      // Maximum time to wait for CB Master responses to a command and number of times to retry a command.
      "CBCommandTimeoutmsec" : 3000,
      "CBCommandRetries" : 1,

      "PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary", "TimeTaggedDigital" : true },
                              {"PollRate" : 60000, "ID" : 2, "PointType" : "Analog"},
                              {"PollRate" : 120000, "ID" :4, "PointType" : "TimeSetCommand"},
                              {"PollRate" : 180000, "ID" :5, "PointType" : "SystemFlagScan"}],

      "Binaries" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Group" : 16, "Channel" : 0, "PayloadLocation" : 1, "PointType" : "MCA"},
                        {"Range" : {"Start" : 16, "Stop" : 31}, "Group" : 17, "Channel" : 0,  "PointType" : "MCA"}],

      "Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Group" : 32, "Channel" : 0, "PayloadLocation" : 2}],

      "BinaryControls" : [{"Range" : {"Start" : 100, "Stop" : 115}, "Group" : 192, "Channel" : 0, "PointType" : "MCC"}]

})011";

TEST_CASE("RTU - Binary Scan TO CB311 ON 172.21.136.80:5001 CB 0x20")
{
      // This is not a REAL TEST, just for testing against a unit. Will always pass..

      // So we have an actual RTU connected to the network we are on, given the parameters in the config file above.
      STANDARD_TEST_SETUP();

      std::ofstream ofs("CBmasterconffile.conf");
      if (!ofs) REQUIRE("Could not open CBmasterconffile for writing");

      ofs << CBmasterconffile;
      ofs.close();

      auto CBMAPort = new  CBMasterPort("CBLiveTestMaster", "CBmasterconffile.conf", Json::nullValue);
      CBMAPort->SetIOS(&IOS);
      CBMAPort->Build();

      START_IOS(1);

      CBMAPort->Enable();
      // actual time tagged data a00b22611900 10008fff9000 5b567bee8600 100000009a00 000210b4a600 64240000e100
      Wait(IOS, 2); // Allow the connection to come up.
      //CBMAPort->EnablePolling(false);	// If the connection comes up after this command, it will enable polling!!!


      // Read the current digital state.
//	CBMAPort->DoPoll(1);

      // Delta Scan up to 15 events, 2 modules. Seq # 10
      // Digital Scan Data a00b01610100 00001101e100
      //CBBlockData commandblock = CBBlockData(9, 15, 10, 2); // This resulted in a time out - sequence number related - need to send 0 on start up??
      //CBMAPort->QueueCBCommand(commandblock, nullptr);

      // Read the current analog state.
//	CBMAPort->DoPoll(2);
      Wait(IOS, 2);

      // Do a time set command
      CBMAPort->DoPoll(4);

      Wait(IOS, 2);

      // Send a POM command by injecting an ODC event
      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  LOGDEBUG("Callback on POM command result : " + std::to_string(static_cast<int>(command_stat)));
                  res = command_stat;
            });

      bool point_on = true;
      uint16_t ODCIndex = 100;

      EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
      val.functionCode = point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

      auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
      event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

      // Send an ODC DigitalOutput command to the Master.
//	CBMAPort->Event(event, "TestHarness", pStatusCallback);

      Wait(IOS, 2);


      STOP_IOS();
      STANDARD_TEST_TEARDOWN();
}

TEST_CASE("RTU - GetScanned CB311 ON 172.21.8.111:5001 CB 0x20")
{
      // This is not a REAL TEST, just for testing against a unit. Will always pass..

      // So we are pretending to be a standalone RTU given the parameters in the config file above.
      STANDARD_TEST_SETUP();

      std::ofstream ofs("CBmasterconffile.conf");
      if (!ofs) REQUIRE("Could not open CBmasterconffile for writing");

      ofs << CBmasterconffile;
      ofs.close();

      Json::Value OSportoverride;
      OSportoverride["IP"] = "0.0.0.0"; // Bind to everything?? was 172.21.8.111
      OSportoverride["TCPClientServer"]= "SERVER";

      auto CBOSPort = new  CBOutstationPort("CBLiveTestOutstation", "CBmasterconffile.conf", OSportoverride);
      CBOSPort->SetIOS(&IOS);
      CBOSPort->Build();

      START_IOS(1);
      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
            event->SetPayload<EventType::Analog>(std::move(4096 + ODCIndex + ODCIndex * 0x100));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);
      }

      CBOSPort->Enable();


      Wait(IOS, 2); // We just run for a period and see if we get connected and scanned.

      STOP_IOS();
      STANDARD_TEST_TEARDOWN();
}
*/
}
#endif
