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
#pragma warning(disable: 6262)

#include <array>
#include <fstream>
#include <cassert>

#define COMPILE_TESTS

#if defined(COMPILE_TESTS)

// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.

#include <spdlog/sinks/ansicolor_sink.h>

#if defined(WIN32)
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/windebug_sink.h>
#endif

#include "CBOutstationPort.h"
#include "CBMasterPort.h"
#include "CBUtility.h"
//#include "StrandProtectedQueue.h"
//#include "ProducerConsumerQueue.h"


#if defined(NONVSTESTING)
#include <catch.hpp>
#else
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer
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
extern std::vector<spdlog::sink_ptr> LogSinks;


const char *conffilename1 = "CBConfig.conf";
const char *conffilename2 = "CBConfig2.conf";

#pragma region conffiles
// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile1 = R"001(
{
	"IP" : "127.0.0.1",
	"Port" : 1000,
	"OutstationAddr" : 9,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"IsBakerDevice" : false,

	// If a binary event time stamp is outside 30 minutes of current time, replace the timestamp
	"OverrideOldTimeStamps" : false,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for CB Master responses to a command and number of times to retry a command.
	"CBCommandTimeoutmsec" : 3000,
	"CBCommandRetries" : 1,

	// Master only PollGroups

	"PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary", "TimeTaggedDigital" : true }],

	// The payload location can be 1B, 2A, 2B
	// Where there is a 24 bit result (MCA,MCB,MCC,ACC24) the next payload location will automatically be used. Do not put something else in there!
	// The point table will build a group list with all the data it has to collect for a given group number.
	// The problem is that a point could actually be in two (or more) groups...
	// We can only use range for Binary and Control. For analog each one has to be defined singularly

	// Digital IN
	// DIG - 12 bits to a Payload, Channel(bit) 1 to 12. On a range, the Channel is the first Channel in the range.
	// MCA,MCB,MCC - 12 bits and takes two payloads (as soon as you have 1 bit, you have two payloads.

	// Analog IN
	// ANA - 1 Channel to a payload,

	// Counter IN
	// ACC12 - 1 to a payload,
	// ACC24 - takes two payloads.

	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 1, "Type" : "DIG"},
					{"Range" : {"Start" : 12, "Stop" : 17}, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA"}],

	"Analogs" : [	{"Index" : 0, "Group" : 3, "PayloadLocation": "3A","Channel" : 1, "Type":"ANA"},
					{"Index" : 1, "Group" : 3, "PayloadLocation": "3B","Channel" : 1, "Type":"ANA"},
					{"Index" : 2, "Group" : 3, "PayloadLocation": "4A","Channel" : 1, "Type":"ANA"},
					{"Index" : 3, "Group" : 3, "PayloadLocation": "4B","Channel" : 1, "Type":"ANA"}],

	"Counters" : [	{"Index" : 4, "Group" : 3, "PayloadLocation": "5A","Channel" : 1, "Type":"ACC12"},
					{"Index" : 5, "Group" : 3, "PayloadLocation": "5B","Channel" : 1, "Type":"ACC12"},
					{"Index" : 6, "Group" : 3, "PayloadLocation": "6A","Channel" : 1, "Type":"ACC24"}],

	// Special definition, so we know where to find the Remote Status Data in the scanned group.

	"RemoteStatus" : [{"Group":3, "PayloadLocation": "7A"}]

})001";
/*	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Group" : 32, "Channel" : 0, "PayloadLocation" : 2}],

      "BinaryControls" : [{"Index": 80,  "Group" : 33, "Channel" : 0, "PointType" : "MCB"},
                                    {"Range" : {"Start" : 100, "Stop" : 115}, "Group" : 37, "Channel" : 0, "PointType" : "MCB"},
                                    {"Range" : {"Start" : 116, "Stop" : 123}, "Group" : 38, "Channel" : 0, "PointType" : "MCC"}],

      "Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Group" : 61, "Channel" : 0},
                              {"Range" : {"Start" : 8, "Stop" : 15}, "Group" : 62, "Channel" : 0}],

      "AnalogControls" : [{"Range" : {"Start" : 1, "Stop" : 8}, "Group" : 39, "Channel" : 0}]*/

// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile2 = R"002(
{
	"IP" : "127.0.0.1",
	"Port" : 1000,
	"OutstationAddr" : 10,
	"ServerType" : "PERSISTENT",
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

	"Binaries" : [{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1A", "Channel" : 0, "PointType" : "MCA"}]

})002";
/*"Binaries" : [{"Index": 90,  "Group" : 33, "Channel" : 0},
                        {"Range" : {"Start" : 0, "Stop" : 15}, "Group" : 34, "Channel" : 0, "PointType" : "MCA"},
                        {"Range" : {"Start" : 16, "Stop" : 31}, "Group" : 35, "Channel" : 0, "PointType" : "MCA"},
                        {"Range" : {"Start" : 32, "Stop" : 47}, "Group" : 63, "Channel" : 0, "PointType" : "MCA"}],

      "Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Group" : 32, "Channel" : 0}],

      "BinaryControls" : [{"Range" : {"Start" : 16, "Stop" : 31}, "Group" : 35, "Channel" : 0, "PointType" : "MCB"}],

      "Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Group" : 61, "Channel" : 0},{"Range" : {"Start" : 8, "Stop" : 15}, "Group" : 62, "Channel" : 0}]*/
#pragma endregion

#pragma region TEST_HELPERS

std::vector<spdlog::sink_ptr> LogSinks;


// Write out the conf file information about into a file so that it can be read back in by the code.
void WriteConfFilesToCurrentWorkingDirectory()
{
	std::ofstream ofs(conffilename1);
	if (!ofs) WARN("Could not open conffile1 for writing");

	ofs << conffile1;
	ofs.close();

	std::ofstream ofs2(conffilename2);
	if (!ofs2) WARN("Could not open conffile2 for writing");

	ofs2 << conffile2;
	ofs.close();
}


void SetupLoggers()
{
	// So create the log sink first - can be more than one and add to a vector.
	#ifdef WIN32
	auto console = std::make_shared<spdlog::sinks::msvc_sink_mt>(); // Windows Debug Sync - puts info into the test output window. Great for debugging...
	// OR wincolor_stdout_sink_mt>();
	#else
	auto console = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
	#endif
	console->set_level(spdlog::level::debug);
	LogSinks.push_back(console);

	// Then create the logger (async - as used in ODC) then connect to all sinks.
	auto pLibLogger = std::make_shared<spdlog::async_logger>("CBPort", begin(LogSinks), end(LogSinks),
		4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
	pLibLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::async_logger>("opendatacon", begin(LogSinks), end(LogSinks),
		4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
	pODCLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(pODCLogger);

	logger = spdlog::get("CBPort"); // For our code - wont catch the config file errors???

	std::string msg = "Logging for this test started..";

	if (logger)
		logger->info(msg);
	else
		std::cout << "Error CBPort Logger not operational";

	if (auto odclogger = spdlog::get("opendatacon"))
		odclogger->info(msg);
	else
		std::cout << "Error opendatacon Logger not operational";
}
void TestSetup(bool writeconffiles = true)
{
	#ifndef NONVSTESTING
	SetupLoggers();
	#endif

	if (writeconffiles)
		WriteConfFilesToCurrentWorkingDirectory();
}
void TestTearDown(void)
{
	spdlog::drop_all(); // Close off everything
}

// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
std::string BuildHexStringFromASCIIHexString(const std::string &as)
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
	timer.async_wait([&IOS](asio::error_code err_code) // [=] all autos by copy, [&] all autos by ref
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
}
void Wait(asio::io_service &IOS, int seconds)
{
	Timer_t timer(IOS);
	timer.expires_from_now(std::chrono::seconds(seconds));
	timer.wait();
}


// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP()\
	TestSetup();\
	asio::io_service IOS(4); // Max 4 threads

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
	auto CBMAPort = new  CBMasterPort("TestMaster", conffilename1, overridejson); \
	CBMAPort->SetIOS(&IOS);      \
	CBMAPort->Build();

#define TEST_CBOSPort(overridejson)      \
	auto CBOSPort = new  CBOutstationPort("TestOutStation", conffilename1, overridejson);   \
	CBOSPort->SetIOS(&IOS);      \
	CBOSPort->Build();

#define TEST_CBOSPort2(overridejson)     \
	auto CBOSPort2 = new  CBOutstationPort("TestOutStation2", conffilename2, overridejson); \
	CBOSPort2->SetIOS(&IOS);     \
	CBOSPort2->Build();

#pragma endregion TEST_HELPERS

namespace SimpleUnitTestsCB
{
TEST_CASE("Util - HexStringTest")
{
	std::string ts = "c406400f0b00"  "0000fffe9000";
	std::string w1 = { ToChar(0xc4),0x06,0x40,0x0f,0x0b,0x00 };
	std::string w2 = { 0x00,0x00,ToChar(0xff),ToChar(0xfe),ToChar(0x90),0x00 };

	std::string res = BuildHexStringFromASCIIHexString(ts);
	REQUIRE(res == (w1 + w2));
}

TEST_CASE("Util - CBBCHTest")
{
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
}
TEST_CASE("Util - ParsePayloadString")
{
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

	res = CBPointConf::ParsePayloadString("1A", payloadlocation);
	REQUIRE(payloadlocation.Packet == 1);
	REQUIRE(payloadlocation.Position == PayloadABType::PositionA);

	res = CBPointConf::ParsePayloadString("0A", payloadlocation);
	REQUIRE(res == false);
	REQUIRE(payloadlocation.Position == PayloadABType::Error);

	res = CBPointConf::ParsePayloadString("123", payloadlocation);
	REQUIRE(res == false);
	REQUIRE(payloadlocation.Position == PayloadABType::Error);
}
TEST_CASE("Util - CBIndexTest")
{
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
}


/*
TEST_CASE("Utility - Strand Queue")
{
      asio::io_service IOS(2);

      asio::io_service::work work(IOS); // Just to keep things from stopping..

      std::thread t1([&]() {IOS.run(); });
      std::thread t2([&]() {IOS.run(); });

      StrandProtectedQueue<int> foo(IOS, 10);
      foo.sync_push(21);
      foo.sync_push(31);
      foo.sync_push(41);

      int res;
      bool success = foo.sync_front(res);
      REQUIRE(success);
      REQUIRE(res == 21);
      foo.sync_pop();

      success = foo.sync_front(res);
      REQUIRE(success);
      REQUIRE(res == 31);
      foo.sync_pop();

      foo.sync_push(2 * res);
      success = foo.sync_front(res);
      foo.sync_pop();
      REQUIRE(success);
      REQUIRE(res == 41);

      success = foo.sync_front(res);
      foo.sync_pop();
      REQUIRE(success);
      REQUIRE(res == 31 * 2);

      success = foo.sync_front(res);
      REQUIRE(!success);

      IOS.stop(); // Or work.reset(), if work was a pointer.!

      t1.join(); // Wait for thread to end
      t2.join();
}
*/
#pragma region Block Tests

TEST_CASE("CBBlock - ClassConstructor1")
{
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
}

TEST_CASE("CBBlock - ClassConstructor2")
{
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
}

TEST_CASE("CBBlock - ClassConstructor3")
{
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
}

TEST_CASE("CBBlock - ClassConstructor4")
{
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
}

TEST_CASE("CBBlock - ClassConstructor5")
{
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
}

#pragma endregion Block Tests
}

namespace StationTests
{
#pragma region Station Tests

// The first 4 are all formatted first and only packets - this is a scan response
/*09200028	- Address
000c0020	- Data
0009111e
00291106
22290030
22280126
08080029*/

TEST_CASE("Station - ScanRequest F0")
{
	// So we send a Scan F0 packet to the Outstation, it responds with the data in the point table.
	// Then we update the data in the point table, scan again and check the data we get back.

	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	// Request Analog Unconditional, Station 9 Module 0x20, 16 Channels
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

	// Send the Analog Unconditional command in as if came from TCP channel
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	std::string DesiredResult = BuildHexStringFromASCIIHexString(     "0937ffaa" // Echoed block plus data 1B
		                                                            "fc080016" // Data 2A and 2B
		                                                            "00080006"
		                                                            "00080006"
		                                                            "00080006"
		                                                            "00080007");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);


	// Call the Event functions to set the CB table data to what we are expecting to get back.
	// Write to the analog registers that we are going to request the values for.
	for (int ODCIndex = 0; ODCIndex < 4; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(std::move(1024 + ODCIndex));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}
	for (int ODCIndex = 4; ODCIndex < 7; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex);
		event->SetPayload<EventType::Counter>(std::move(1024 + ODCIndex));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}
	for (int ODCIndex = 0; ODCIndex < 12; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
		event->SetPayload<EventType::Binary>(std::move((ODCIndex % 2) == 0));

		CBOSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	Response = "Not Set";
	output << commandblock.ToBinaryString();
	CBOSPort->InjectSimulatedTCPMessage(write_buffer);

	// Should now get different data!
	DesiredResult = BuildHexStringFromASCIIHexString("09355516" // Echoed block plus data 1B
		                                           "fc080016" // Data 2A and 2B
		                                           "400a00b6"
		                                           "402a0186"
		                                           "404a029c"
		                                           "4068003d");

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response == DesiredResult);

	TestTearDown();
}
TEST_CASE("Station - BinaryEvent")
{
	STANDARD_TEST_SETUP();
	TEST_CBOSPort(Json::nullValue);

	CBOSPort->Enable();

	// Set up a callback for the result - assume sync operation at the moment
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=,&res](CommandStatus command_stat)
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
	auto event2 = std::make_shared<EventInfo>(EventType::Binary, ODCIndex+200);
	event2->SetPayload<EventType::Binary>(std::move(val));

	CBOSPort->Event(event2, "TestHarness", pStatusCallback);
	REQUIRE((res == CommandStatus::UNDEFINED)); // The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...
	// Wait for some period to do something?? Check that the port is open and we can connect to it?

	TestTearDown();
}


/*
TEST_CASE("Station - CounterScanFn30")
{
      // Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
      // Checks the TCP send output for correct data and format.

      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);
      CBOSPort->Enable();

      // Do the same test as analog unconditional, we should give the same response from the Counter Scan.
      CBBlockData commandblock(9, true, COUNTER_SCAN, 0x20, 16, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // Call the Event functions to set the CB table data to what we are expecting to get back.
      // Write to the analog registers that we are going to request the values for.
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
            event->SetPayload<EventType::Analog>(std::move(4096 + ODCIndex + ODCIndex * 0x100));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
      }

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Analog Unconditional command in as if came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc1f205f3a00" // Echoed block
                                                                         "100011018400" // Channel 0 and 1
                                                                         "12021303b700" // Channel 2 and 3 etc
                                                                         "14041505b900"
                                                                         "160617078a00"
                                                                         "18081909a500"
                                                                         "1A0A1B0B9600"
                                                                         "1C0C1D0D9800"
                                                                         "1E0E1F0Feb00");

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response == DesiredResult);

      // Station 9, Module 61 and 62 - 8 channels each.
      CBBlockData commandblock2(9, true, COUNTER_SCAN, 61, 16, true);
      output << commandblock2.ToBinaryString();

      // Set the counter values to match what the analogs were set to.
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex);
            event->SetPayload<EventType::Counter>(std::move(numeric_cast<uint16_t>(4096 + ODCIndex + ODCIndex * 0x100)));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      Response = "Not Set";

      // Send the Analog Unconditional command in as if came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1f3d5f0a00" // Echoed block
                                                                          "100011018400" // Channel 0 and 1
                                                                          "12021303b700" // Channel 2 and 3 etc
                                                                          "14041505b900"
                                                                          "160617078a00"
                                                                          "18081909a500"
                                                                          "1A0A1B0B9600"
                                                                          "1C0C1D0D9800"
                                                                          "1E0E1F0Feb00");

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response == DesiredResult2);

      TestTearDown();
}
TEST_CASE("Station - AnalogDeltaScanFn6")
{
      // Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
      // Checks the TCP send output for correct data and format.
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // Call the Event functions to set the CB table data to what we are expecting to get back.
      // Write to the analog registers that we are going to request the values for.
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
            event->SetPayload<EventType::Analog>(std::move(4096 + ODCIndex + ODCIndex * 0x100));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      // Request Analog Delta Scan, Station 9, Module 0x20, 16 Channels
      CBBlockData commandblock(9, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the command in as if came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc05205f1500" // Echoed block
                                                                          "100011018400" // Channel 0 and 1
                                                                          "12021303b700" // Channel 2 and 3 etc
                                                                          "14041505b900"
                                                                          "160617078a00"
                                                                          "18081909a500"
                                                                          "1A0A1B0B9600"
                                                                          "1C0C1D0D9800"
                                                                          "1E0E1F0Feb00");

      // We should get an identical response to an analog unconditional here
      REQUIRE(Response == DesiredResult1);
      //------------------------------

      // Make changes to 5 channels
      for (int ODCIndex = 0; ODCIndex < 5; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
            event->SetPayload<EventType::Analog>(std::move(4096 + ODCIndex + ODCIndex * 0x100 + ((ODCIndex % 2) == 0 ? 50 : -50)));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc06205f3100"
                                                                          "32ce32ce8b00"
                                                                          "320000009200"
                                                                          "00000000bf00"
                                                                          "00000000ff00");

      // Now a delta scan
      REQUIRE(Response == DesiredResult2);
      //------------------------------

      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0d205f5800");

      // Now no changes so should get analog no change response.
      REQUIRE(Response == DesiredResult3);

      TestTearDown();
}
TEST_CASE("Station - DigitalUnconditionalFn7")
{
      // Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
      // Checks the TCP send output for correct data and format.
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // Write to the analog registers that we are going to request the values for.

      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
            event->SetPayload<EventType::Binary>(std::move((ODCIndex%2)==0));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      // Request Digital Unconditional (Fn 7), Station 9, Module 33, 3 Modules(Channels)
      CBBlockData commandblock(9, true, DIGITAL_UNCONDITIONAL_OBS, 33, 3, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // Address 21, only 1 bit, set by default - check bit order
      // Address 22, set to alternating on/off above
      // Address 23, all on by default
      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0721721f00" "7c2180008200" "7c22aaaab900" "7c23ffffc000");

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response == DesiredResult);

      TestTearDown();
}
TEST_CASE("Station - DigitalChangeOnlyFn8")
{
      // Tests time tagged change response Fn 8
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      // Request Digital Unconditional (Fn 7), Station 9, Module 34, 2 Modules( fills the Channels field)
      CBBlockData commandblock(9, true, DIGITAL_DELTA_SCAN, 34, 2, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Inject command as if it came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0722513d00" "7c22ffff9c00" "7c23ffffc000"); // All on

      REQUIRE(Response == DesiredResult1);

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // Write to the first module, but not the second. Should get only the first module results sent.
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
            event->SetPayload<EventType::Binary>(std::move((ODCIndex % 2) == 0));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      // The command remains the same each time, but is consumed in the InjectCommand
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0822701d00"   // Return function 8, Channels == 0, so 1 block to follow.
                                                                          "7c22aaaaf900"); // Values set above

      REQUIRE(Response == DesiredResult2);

      // Now repeat the command with no changes, should get the no change response.

      // The command remains the same each time, but is consumed in the InjectCommand
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e22727800"); // Digital No Change response

      REQUIRE(Response == DesiredResult3);

      TestTearDown();
}
TEST_CASE("Station - DigitalHRERFn9")
{
      // Tests time tagged change response Fn 9
      STANDARD_TEST_SETUP();
      Json::Value portoverride;
      portoverride["IsBakerDevice"] = static_cast<Json::UInt>(0);
      TEST_CBOSPort(portoverride);
      CBOSPort->Enable();

      // Request HRER List (Fn 9), Station 9,  sequence # 0, max 10 events, mev = 1
      CBBlockData commandblock(9, true, 0, 10,true, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Inject command as if it came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // List should be empty...so get an emtpy HRER response

      REQUIRE(Response[0] == ToChar(0xfc));  // 9 plus 0x80 for direction
      REQUIRE(Response[1] == 0x09);          // Fn 9
      REQUIRE((Response[2] & 0xF0) == 0x10); // Top 4 bits are the sequence number - will be 1
      REQUIRE((Response[2] & 0x08) == 0);    // Bit 3 is the MEV flag
      REQUIRE(Response[3] == 0);

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // Write to the first module all 16 bits, there should now be 16 "events" in the Old format event queue
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
            event->SetPayload<EventType::Binary>(std::move((ODCIndex % 2) == 0));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      Response = "Not Set";

      // The command remains the same each time, but is consumed in the InjectCommand
      commandblock = CBBlockData(9, true, 2, 10, true, true);
      output << commandblock.ToBinaryString();
      // Inject command as if it came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // Fn9 Test - Will have a set of blocks containing 10 change records. Need to decode to test as the times will vary by run.
      // Need to write the master station decode - code for this in order to be able to check it. The message is going to change each time

      REQUIRE(Response[2] == 0x28); // Seq 3, MEV == 1
      REQUIRE(Response[3] == 10);

      REQUIRE(Response.size() == 72); // DecodeFnResponse(Response)

      // Now repeat the command to get the last 6 results

      // The command remains the same each time, but is consumed in the InjectCommand
      commandblock = CBBlockData(9, true, 3, 10, true, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // Again need a decode function
      REQUIRE(Response[2] == 0x30); // Seq 3, MEV == 0
      REQUIRE(Response[3] == 6);    // Only 6 changes left

      REQUIRE(Response.size() == 48);

      // Send the command again, but we should get an empty response. Should only be the one block.
      commandblock = CBBlockData(9, true, 4, 10, true, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // Will get all data changing this time around
      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0940006d00"); // No events, seq # = 4

      REQUIRE(Response == DesiredResult2);

      // Send the command again, we should get the previous response - tests the recovery from lost packet code.
      commandblock = CBBlockData(9, true, 4, 10, true, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response == DesiredResult2);

      //-----------------------------------------
      // Need to test the code path where the delta between two records is > 31.999 seconds.
      // Cheat and write directly to the HRER queue
      CBTime changedtime = CBNow();

      CBBinaryPoint pt1(1, 34, 1, 0, MCA, 1, true, changedtime);
      CBBinaryPoint pt2(2, 34, 2, 0, MCA, 0, true, static_cast<CBTime>(changedtime + 32000));
      CBOSPort->GetPointTable()->AddToDigitalEvents(pt1);
      CBOSPort->GetPointTable()->AddToDigitalEvents(pt2);

      commandblock = CBBlockData(9, true,5, 10, true, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response[2] == 0x58); // Seq 5, MEV == 1	 The long period between events will require another request from the master
      REQUIRE(Response[3] == 1);

      REQUIRE(Response.size() == 18); // DecodeFnResponse(Response)

      commandblock = CBBlockData(9, true, 6, 10, true, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response[2] == 0x60); // Seq 6, MEV == 0	 The long delay will require another request from the master
      REQUIRE(Response[3] == 1);

      REQUIRE(Response.size() == 18); // DecodeFnResponse(Response)

      //---------------------
      // Test rejection of set time command - which is when MaximumEvents is set to 0
      commandblock = CBBlockData(9, true, 5, 0, true, true);
      output << commandblock.ToBinaryString();

      uint64_t currenttime = CBNow();
      CBBlockData datablock(static_cast<uint32_t>(currenttime / 1000), true );
      output << datablock.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e58505100"); // Should get a command rejected response
      REQUIRE(Response == DesiredResult3);

      TestTearDown();
}
TEST_CASE("Station - DigitalCOSScanFn10")
{
      // Tests change response Fn 10
      STANDARD_TEST_SETUP();
      Json::Value portoverride;
      portoverride["IsBakerDevice"] = static_cast<Json::UInt>(0);
      TEST_CBOSPort(portoverride);

      CBOSPort->Enable();

      // Request Digital Change Only Fn 10, Station 9, Module 0 scan from the first module, Modules 2 max number to return
      CBBlockData commandblock(9, true, 0, 2, true);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });


      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0A00522000" "7c2180008200" "7c22ffffdc00");

      REQUIRE(Response == DesiredResult1);

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      auto event = std::make_shared<EventInfo>(EventType::Binary, 80);
      event->SetPayload<EventType::Binary>(std::move(false));

      CBOSPort->Event(event, "TestHarness", pStatusCallback); // 0x21, bit 1

      // Send the command but start from module 0x22, we did not get all the blocks last time. Test the wrap around
      commandblock = CBBlockData(9, true, 0x25, 5, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0a25452400"
                                                                          "7c25ffff9300"
                                                                          "7c26ff00b000"
                                                                          "7c3fffffbc00"
                                                                          "7c2100008c00"
                                                                          "7c23ffffc000");

      REQUIRE(Response == DesiredResult2);

      // Send the command with 0 start module, should return a no change block.
      commandblock = CBBlockData(9, true, 0, 2, true);
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e00404c00"); // Digital No Change response

      REQUIRE(Response == DesiredResult3);

      TestTearDown();
}
TEST_CASE("Station - DigitalCOSFn11")
{
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      CBTime changedtime = static_cast<CBTime>(0x0000016338b6d4fb); // A value around June 2018

      // Request Digital COS (Fn 11), Station 9, 15 tagged events, sequence #0 - used on start up to send all data, 15 modules returned

      CBBlockData commandblock = CBBlockData(9, 15, 1, 15);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Inject command as if it came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // Will get all data changing this time around
      const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0b01462800" "210080008100" "2200ffff8300" "2300ffffa200" "2500ffff8900" "2600ff00b600" "3f00ffffca00");

      REQUIRE(Response == DesiredResult1);

      //---------------------
      // No data changes so should get a no change Fn14 block
      commandblock = CBBlockData(9, 15, 2, 15); // Sequence number must increase
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0e02406800"); // Digital No Change response for Fn 11 - different for 7,8,10

      REQUIRE(Response == DesiredResult2);

      //---------------------
      // No sequence number change, so should get the same data back as above.
      commandblock = CBBlockData(9, 15, 2, 15); // Sequence number must increase - but for this test not
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response == DesiredResult2);

      //---------------------
      // Now change data in one block only
      commandblock = CBBlockData(9, 15, 3, 15); // Sequence number must increase
      output << commandblock.ToBinaryString();

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      for (int ODCIndex = 0; ODCIndex < 4; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "TestHarness", QualityFlags::ONLINE, static_cast<CBTime>(changedtime));
            event->SetPayload<EventType::Binary>(std::move((ODCIndex % 2) == 0));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      // The command remains the same each time, but is consumed in the InjectCommand
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // The second block is time, and will change each run.
      // The other blocks will have the msec part of the field change.

      REQUIRE(Response.size() == 0x2a);
      std::string BlockString = Response.substr(0, 6);
      CBBlockFn11StoM RBlock1 = CBBlockFn11StoM(CBBlockData(BlockString));

      REQUIRE(RBlock1.GetStationAddress() == 9);
      REQUIRE(RBlock1.GetModuleCount() == 1);
      REQUIRE(RBlock1.GetTaggedEventCount() == 4);

      // Now the module that changed
      BlockString = Response.substr(6, 6);
      CBBlockData RBlock = CBBlockData(BlockString);
      REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
      REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset - always 0 for ModuleBlock
      REQUIRE(RBlock.GetSecondWord() == 0xafff); // 16 bits of data

      // Now a time block
      BlockString = Response.substr(12, 6);
      RBlock = CBBlockData(BlockString);

      CBTime timebase = static_cast<uint64_t>(RBlock.GetData()) * 1000; //CBTime msec since Epoch.
      LOGDEBUG("Fn11 TimeDate Packet Local : " + to_timestringfromCBtime(timebase));
      REQUIRE(timebase == 0x0000016338b6d400);

      // Then 4 COS blocks.
      BlockString = Response.substr(18, 6);
      RBlock = CBBlockData(BlockString);
      REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
      REQUIRE(RBlock.GetByte(1) == 0xfb);        //  msec offset
      REQUIRE(RBlock.GetSecondWord() == 0xffff); // 16 bits of data

      BlockString = Response.substr(24, 6);
      RBlock = CBBlockData(BlockString);
      REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
      REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
      REQUIRE(RBlock.GetSecondWord() == 0xbfff); // 16 bits of data

      BlockString = Response.substr(30, 6);
      RBlock = CBBlockData(BlockString);
      REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
      REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
      REQUIRE(RBlock.GetSecondWord() == 0xbfff); // 16 bits of data

      BlockString = Response.substr(36, 6);
      RBlock = CBBlockData(BlockString);
      REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
      REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
      REQUIRE(RBlock.GetSecondWord() == 0xafff); // 16 bits of data

      //-----------------------------------------
      // Need to test the code path where the delta between two records is > 255 milliseconds. Also when it is more than 0xFFFF
      // Cheat and write directly to the DCOS queue

      CBBinaryPoint pt1(1, 34, 1, 0, MCA, 1, true,  changedtime);
      CBBinaryPoint pt2(2, 34, 2, 0, MCA, 0, true, static_cast<CBTime>(changedtime + 256));
      CBBinaryPoint pt3(3, 34, 3, 0, MCA, 1, true, static_cast<CBTime>(changedtime + 0x20000)); // Time gap too big, will require another Master request
      CBOSPort->GetPointTable()->AddToDigitalEvents(pt1);
      CBOSPort->GetPointTable()->AddToDigitalEvents(pt2);
      CBOSPort->GetPointTable()->AddToDigitalEvents(pt3);

      commandblock = CBBlockData(9, 15, 4, 0); // Sequence number must increase
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult4 = BuildHexStringFromASCIIHexString("fc0b24603f00" "5aefcc809300" "22fbafff9a00" "00012200a900" "afff0000e600");

      REQUIRE(Response == DesiredResult4); // The RSF, HRP and DCP flag value will now be valid in all tests - need to check the comparison data!

      //-----------------------------------------
      // Get the single event left in the queue
      commandblock = CBBlockData(9, 15, 5, 0); // Sequence number must increase
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult5 = BuildHexStringFromASCIIHexString("fc0b15402d00" "5aefcd03a500" "00012243ad00" "afff0000e600");

      REQUIRE(Response == DesiredResult5);

      TestTearDown();
}
TEST_CASE("Station - DigitalUnconditionalFn12")
{
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      // Request DigitalUnconditional (Fn 12), Station 9,  sequence #1, up to 15 modules returned

      CBBlockData commandblock = CBBlockData(9, 0x21, 1, 3);
      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Inject command as if it came from TCP channel
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0b01533500" "210080008100" "2200ffff8300" "2300ffffe200");

      REQUIRE(Response == DesiredResult1);

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      //--------------------------------
      // Change the data at 0x22 to 0xaaaa
      //
      // Write to the first module, but not the second. Should get only the first module results sent.
      for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
      {
            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
            event->SetPayload<EventType::Binary>(std::move((ODCIndex % 2) == 0));

            CBOSPort->Event(event, "TestHarness", pStatusCallback);

            REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
      }

      //--------------------------------
      // Send the same command and sequence number, should get the same data as before - even though we have changed it
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response == DesiredResult1);

      //--------------------------------
      commandblock = CBBlockData(9, 0x21, 2, 3); // Have to change the sequence number
      output << commandblock.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0b02733a00" "210080008100" "2200aaaaa600" "2300ffffe200");

      REQUIRE(Response == DesiredResult2);

      TestTearDown();
}
TEST_CASE("Station - FreezeResetFn16")
{
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      //  Station 9
      CBBlockData commandblock(9, true);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f01034600");

      REQUIRE(Response == DesiredResult); // OK Command

      //---------------------------
      CBBlockData commandblock2(0, false); // Reset all counters on all stations
      output << commandblock2.ToBinaryString();
      Response = "Not Set";

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response =="Not Set"); // As address zero, no response expected

      TestTearDown();
}
TEST_CASE("Station - POMControlFn17")
{
      // Test that we can generate a packet set that matches a captured packet
      CBBlockData testblock(0x27, 0xa5, 0);
      REQUIRE(testblock.ToString() == "2711a5000300");
      CBBlockData sb = testblock.GenerateSecondBlock();
      REQUIRE(sb.ToString() == "585a8000d500");

      CBBlockData testblock2(0x26, 0xa4, 4);
      REQUIRE(testblock2.ToString() == "2611a4040700");
      CBBlockData sb2 = testblock2.GenerateSecondBlock();
      REQUIRE(sb2.ToString() == "595b0800c000");

      // One of the few multi-block commands
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      //  Station 9
      CBBlockData commandblock(9, 37, 1);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CBBlockData datablock = commandblock.GenerateSecondBlock();
      output << datablock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f25014d00");

      REQUIRE(Response == DesiredResult); // OK Command

      //---------------------------
      // Now do again with a bodgy second block.
      output << commandblock.ToBinaryString();
      CBBlockData datablock2(static_cast<uint32_t>(1000), true); // Nonsensical block
      output << datablock2.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e25515300");

      REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

      //---------------------------
      CBBlockData commandblock2(9, 36, 1); // Invalid control point
      output << commandblock2.ToBinaryString();

      CBBlockData datablock3 = commandblock.GenerateSecondBlock();
      output << datablock3.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24514100");

      REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

      TestTearDown();
}
TEST_CASE("Station - DOMControlFn19")
{
      // Test that we can generate a packet set that matches a captured packet
      CBBlockData testblock(0x27, 0xa5);
      REQUIRE(testblock.ToString() == "2713a55a2000");
      CBBlockData sb = testblock.GenerateSecondBlock(0x12);
      REQUIRE(sb.ToString() == "00121258c300");

      // From a Fn19 packet capture - but direction is wrong...so is checksuM!
      //CBBlockData b("91130d013100");
      //REQUIRE(b.BCHPasses());
      // The second packet passed however!!
      CBBlockData b2("1000600cf100");
      REQUIRE(b2.BCHPasses());

      // This test was written for where the outstation is simply sinking the timedate change command
      // Will have to change if passed to ODC and events handled here
      // One of the few multiblock commands
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      //  Station 9
      CBBlockData commandblock(9, 37);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CBBlockData datablock = commandblock.GenerateSecondBlock(0x34);
      output << datablock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f25da4400");

      REQUIRE(Response == DesiredResult); // OK Command

      //---------------------------
      // Now do again with a bodgy second block.
      output << commandblock.ToBinaryString();
      CBBlockData datablock2(static_cast<uint32_t>(1000), true); // Non nonsensical block
      output << datablock2.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e255a4b00");

      REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

      //---------------------------
      CBBlockData commandblock2(9, 36); // Invalid control point
      output << commandblock2.ToBinaryString();

      CBBlockData datablock3 = commandblock.GenerateSecondBlock(0x73);
      output << datablock3.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e245b4200");

      REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

      TestTearDown();
}
TEST_CASE("Station - AOMControlFn23")
{
      // One of the few multi-block commands
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      //  Station 9
      CBBlockData commandblock(9, 39, 1);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CBBlockData datablock = commandblock.GenerateSecondBlock(0x55);
      output << datablock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f27016900");

      REQUIRE(Response == DesiredResult); // OK Command

      //---------------------------
      // Now do again with a bodgy second block.
      output << commandblock.ToBinaryString();
      CBBlockData datablock2(static_cast<uint32_t>(1000), true); // Non nonsensical block
      output << datablock2.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e27517700");

      REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

      //---------------------------
      CBBlockData commandblock2(9, 36, 1); // Invalid control point
      output << commandblock2.ToBinaryString();

      CBBlockData datablock3 = commandblock.GenerateSecondBlock(0x55);
      output << datablock3.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24514100");

      REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

      TestTearDown();
}
TEST_CASE("Station - SystemsSignOnFn40")
{
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();

      // System SignOn Command, Station 0 - the slave responds with the address set correctly (i.e. if originally 0, change to match the station address - where it is asked to identify itself.
      CBBlockData commandblock(0);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc2883d77100");

      REQUIRE(Response == DesiredResult);

      TestTearDown();
}
TEST_CASE("Station - ChangeTimeDateFn43")
{
      // This test was written for where the outstation is simply sinking the timedate change command
      // Will have to change if passed to ODC and events handled here
      // One of the few multiblock commands
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();
      uint64_t currenttime = CBNow();

      // TimeChange command (Fn 43), Station 9
      CBBlockData commandblock(9, currenttime % 1000);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CBBlockData datablock(static_cast<uint32_t>(currenttime / 1000),true);
      output << datablock.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response[0] == ToChar(0xFC));
      REQUIRE(Response[1] == ToChar(0x0F)); // OK Command

      // Now do again with a bodgy time.
      output << commandblock.ToBinaryString();
      CBBlockData datablock2(static_cast<uint32_t>(1000), true); // Nonsensical time
      output << datablock2.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response[0] == ToChar(0xFC));
      REQUIRE(Response[1] == ToChar(30)); // Control/Scan Rejected Command

      TestTearDown();
}
TEST_CASE("Station - ChangeTimeDateFn44")
{
      // This test was written for where the outstation is simply sinking the timedate change command
      // Will have to change if passed to ODC and events handled here
      // One of the few multiblock commands
      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();
      uint64_t currenttime = CBNow();

      // TimeChange command (Fn 44), Station 9
      CBBlockData commandblock(9, currenttime % 1000);

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);
      output << commandblock.ToBinaryString();

      CBBlockData datablock(static_cast<uint32_t>(currenttime / 1000));
      output << datablock.ToBinaryString();

      int UTCOffsetMinutes = -600;
      CBBlockData datablock2(static_cast<uint32_t>(UTCOffsetMinutes<<16), true);
      output << datablock2.ToBinaryString();

      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      // Send the Command
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response[0] == ToChar(0xFC));
      REQUIRE(Response[1] == ToChar(0x0F)); // OK Command

      // Now do again with a bodgy time.
      output << commandblock.ToBinaryString();
      datablock2 = CBBlockData(static_cast<uint32_t>(1000), true); // Nonsensical time
      output << datablock2.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // No need to delay to process result, all done in the InjectCommand at call time.
      REQUIRE(Response[0] == ToChar(0xFC));
      REQUIRE(Response[1] == ToChar(30)); // Control/Scan Rejected Command

      TestTearDown();
}
TEST_CASE("Station - Multi-drop TCP Test")
{
      // Here we test the ability to support multiple Stations on the one Port/IP Combination.
      // The Stations will be 9, 0x7D
      STANDARD_TEST_SETUP();

      START_IOS(1);

      // The two CBPorts will share a connection, only the first to enable will open it.
      TEST_CBOSPort(Json::nullValue);
      TEST_CBOSPort2(Json::nullValue);

      CBOSPort->Enable(); // This should open a listening port on 1000.
      CBOSPort2->Enable();

      ProducerConsumerQueue<std::string> Response;
      auto ResponseCallback = [&](buf_t& readbuf)
                              {
                                    size_t bufsize = readbuf.size();
                                    std::string S(bufsize, 0);

                                    for (size_t i = 0; i < bufsize; i++)
                                    {
                                          S[i] = static_cast<char>(readbuf.sgetc());
                                          readbuf.consume(1);
                                    }
                                    Response.Push(S); // Store so we can check
                              };

      bool socketisopen = false;
      auto SocketStateHandler = [&socketisopen](bool state)
                                {
                                      socketisopen = state;
                                };

      // An outstation is a server by default (Master connects to it...)
      // Open a client socket on 127.0.0.1, 1000 and see if we get what we expect...
      std::shared_ptr<TCPSocketManager<std::string>> pSockMan;
      pSockMan.reset(new TCPSocketManager<std::string>
                  (&IOS, false, "127.0.0.1", "1000",
                  ResponseCallback,
                  SocketStateHandler,
                  std::numeric_limits<size_t>::max(),
                  true,
                  250));
      pSockMan->Open();

      Wait(IOS, 3);
      REQUIRE(socketisopen); // Should be set in a callback.

      // Send the Command - results in an async write
      //  Station 9
      CBBlockData commandblock(9, true);
      pSockMan->Write(commandblock.ToBinaryString());

      //  Station 0x7D
      CBBlockData commandblock2(0x7D, true);
      pSockMan->Write(commandblock2.ToBinaryString());

      Wait(IOS, 3); // Just pause to make sure any queued work is done (events)

      CBOSPort->Disable();
      CBOSPort2->Disable();
      pSockMan->Close();

      STOP_IOS();

      // Need to handle multiple responses...
      // Deal with the last response first...
      REQUIRE(Response.Size() == 2);

      std::string s;
      Response.Pop(s);
      REQUIRE(s == BuildHexStringFromASCIIHexString("fc0f01034600")); // OK Command

      Response.Pop(s);
      REQUIRE(s == BuildHexStringFromASCIIHexString("fd0f01027c00")); // OK Command

      REQUIRE(Response.IsEmpty());

      TestTearDown();
}
TEST_CASE("Station - System Flag Scan Test")
{
      // Station - System Flag Scan Poll Test
      // There are only two flags that matter - we don't know if there are any contract depended ones...
      // System powered Up and System Time Incorrect.
      // The first remains set until a flag scan, the second until a time set command.
      // If we receive a system powered up status from a connected Master - through ODC, then we should set the SPU flag.
      // For this test, makes sure both are set on start up, then do what is necessary to clear them and then check that this has happened.

      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);

      CBOSPort->Enable();
      uint64_t currenttime = CBNow();
      // Hook the output function with a lambda
      std::string Response = "Not Set";
      CBOSPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

      asio::streambuf write_buffer;
      std::ostream output(&write_buffer);

      // Do a "Normal" analog scan command, and make sure that the RSF bit in the response is set - for each packet that carries that bit???
      CBBlockData analogcommandblock(9, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
      output << analogcommandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      CBBlockData respana = CBBlockData(CBBlockData(Response.substr(0,6)));

      REQUIRE(respana.BCHPasses() == true);
      REQUIRE(respana.GetRSF() == true);
      REQUIRE(respana.GetDCP() == true);  // Changed digitals available
      REQUIRE(respana.GetHRP() == false); // Time tagged digitals available

      Response = "Not Set";

      // FlagScan command (Fn 52), Station 9
      CBBlockData commandblock(9);

      // Send the Command
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      // No need to delay to process result, all done in the InjectCommand at call time.
      CBBlockFn52StoM resp = CBBlockFn52StoM(CBBlockData(Response));

      REQUIRE(resp.BCHPasses() == true);
      REQUIRE(resp.GetSystemPoweredUpFlag() == true);
      REQUIRE(resp.GetSystemTimeIncorrectFlag() == true);

      Response = "Not Set";

      // Now read again, the PUF should be false.
      // Send the Command (again)
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      CBBlockFn52StoM resp2 = CBBlockFn52StoM(CBBlockData(Response));

      REQUIRE(resp2.BCHPasses() == true);
      REQUIRE(resp2.GetSystemPoweredUpFlag() == false);
      REQUIRE(resp2.GetSystemTimeIncorrectFlag() == true);

      Response = "Not Set";

      // Now send a time command, so the STI flag is cleared.
      CBBlockData timecommandblock(9, currenttime % 1000);
      output << timecommandblock.ToBinaryString();
      CBBlockData datablock(static_cast<uint32_t>(currenttime / 1000), true);
      output << datablock.ToBinaryString();

      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      REQUIRE(Response[0] == ToChar(0xFC));
      REQUIRE(Response[1] == ToChar(0x0F)); // OK Command

      Response = "Not Set";

      // Now scan again and check the STI flag has been cleared.
      // Send the Command (again)
      output << commandblock.ToBinaryString();
      CBOSPort->InjectSimulatedTCPMessage(write_buffer);

      CBBlockFn52StoM resp3 = CBBlockFn52StoM(CBBlockData(Response));

      REQUIRE(resp3.BCHPasses() == true);
      REQUIRE(resp3.GetSystemPoweredUpFlag() == false);
      REQUIRE(resp3.GetSystemTimeIncorrectFlag() == false);

      TestTearDown();
}
*/
#pragma endregion
}

namespace MasterTests
{

#pragma region Master Tests

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

	CBMAPort->Subscribe(CBOSPort, "TestLink"); // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
	// Usually is a cross subscription, where each subscribes to the other.
	CBOSPort->Enable();
	CBMAPort->Enable();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	CBMAPort->SetSendTCPDataFn([&Response](std::string CBMessage) { Response = CBMessage; });

	INFO("Analog Unconditional Fn5");
	{
		// Now send a request analog unconditional command - asio does not need to run to see this processed, in this test set up
		// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
		CBBlockData sendcommandblock(9, 3, FUNC_SCAN_DATA, 0, true);
		CBMAPort->QueueCBCommand(sendcommandblock, nullptr);

		Wait(IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("09300025");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above.
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);

		// From the outstation test above!!
		std::string Payload = BuildHexStringFromASCIIHexString(                 "09355516" // Echoed block plus data 1B
			                                                                  "fc080016" // Data 2A and 2B
			                                                                  "400a00b6"
			                                                                  "402a0186"
			                                                                  "404a029c"
			                                                                  "4068003d");
		output << Payload;

		// Send the Analog Unconditional command in as if came from TCP channel. This should stop a resend of the command due to timeout...
		CBMAPort->InjectSimulatedTCPMessage(write_buffer);

		Wait(IOS, 5);

		// To check the result, see if the points in the master point list have been changed to the correct values.
		bool hasbeenset;

		for (int ODCIndex = 0; ODCIndex < 4; ODCIndex++)
		{
			uint16_t res;
			CBMAPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
			REQUIRE(res == (1024 + ODCIndex));
		}
		for (int ODCIndex = 4; ODCIndex < 7; ODCIndex++)
		{
			uint16_t res;
			CBMAPort->GetPointTable()->GetCounterValueUsingODCIndex(ODCIndex, res, hasbeenset);
			REQUIRE(res == (1024 + ODCIndex));
		}
		for (int ODCIndex = 0; ODCIndex < 12; ODCIndex++)
		{
			uint8_t res;
			bool changed;
			CBMAPort->GetPointTable()->GetBinaryValueUsingODCIndexAndResetChangedFlag(ODCIndex, res, changed, hasbeenset);
			REQUIRE(res == ((ODCIndex+1) % 2));
		}

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them?
		Wait(IOS, 1);

		for (int ODCIndex = 0; ODCIndex < 4; ODCIndex++)
		{
			uint16_t res;
			CBOSPort->GetPointTable()->GetAnalogValueUsingODCIndex(ODCIndex, res, hasbeenset);
			REQUIRE(res == (1024 + ODCIndex));
		}
		for (int ODCIndex = 4; ODCIndex < 7; ODCIndex++)
		{
			uint16_t res;
			CBOSPort->GetPointTable()->GetCounterValueUsingODCIndex(ODCIndex, res, hasbeenset);
			REQUIRE(res == (1024 + ODCIndex));
		}
		for (int ODCIndex = 0; ODCIndex < 12; ODCIndex++)
		{
			uint8_t res;
			bool changed;
			CBOSPort->GetPointTable()->GetBinaryValueUsingODCIndexAndResetChangedFlag(ODCIndex, res, changed, hasbeenset);
			REQUIRE(res == ((ODCIndex+1) % 2));
		}
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}
/*
TEST_CASE("Master - ODC Comms Up Send Data/Comms Down (TCP) Quality Setting")
{
      // When ODC signals that it has regained communication up the line (or is a new connection), we will resend all data through ODC.
      STANDARD_TEST_SETUP();
      TEST_CBMAPort(Json::nullValue);
      START_IOS(1);

      CBMAPort->Enable();

      //  We need to register a couple of handlers to be able to receive the event sent below.
      // The DataConnector normally handles this when ODC is running.

      CommandStatus res = CommandStatus::NOT_AUTHORIZED;
      auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
            {
                  res = command_stat;
            });

      // This will result in sending all current data through ODC events.
      auto event = std::make_shared<EventInfo>(EventType::ConnectState);
      event->SetPayload<EventType::ConnectState>(std::move(ConnectState::CONNECTED));

      CBMAPort->Event(event,"TestHarness", pStatusCallback);

      Wait(IOS, 2);

      REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set. 1 is defined

      // If we loose communication down the line (TCP) we then set all data to COMMS-LOST quality through ODC so the ports
      // connected know that data is now not valid. As the CB slave maintains its own copy of data, to respond to polls, this is important.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - DOM and POM Tests")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;
      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = false;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

      CBOSPort->Enable();
      CBMAPort->Enable();

      // Hook the output functions
      std::string OSResponse = "Not Set";
      CBOSPort->SetSendTCPDataFn([&OSResponse](std::string CBMessage) { OSResponse = CBMessage; });

      std::string MAResponse = "Not Set";
      CBMAPort->SetSendTCPDataFn([&MAResponse](std::string CBMessage) { MAResponse = CBMessage; });

      asio::streambuf OSwrite_buffer;
      std::ostream OSoutput(&OSwrite_buffer);

      asio::streambuf MAwrite_buffer;
      std::ostream MAoutput(&MAwrite_buffer);

      INFO("DOM ODC->Master Command Test");
      {
            // So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct CB command,
            // and then also when we send the correct response we get an ODC::success message.

            CommandStatus res = CommandStatus::NOT_AUTHORIZED;
            auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
                  {
                        res = command_stat;
                  });

            bool point_on = true;
            uint16_t ODCIndex = 100;

            EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
            val.functionCode = point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

            auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
            event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

            // Send an ODC DigitalOutput command to the Master.
            CBMAPort->Event(event, "TestHarness", pStatusCallback);

            Wait(IOS, 2);

            // Need to check that the hooked tcp output function got the data we expected.
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1325da2700" "fffffe03ed00")); // DOM Command

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM commandblock(resp); // Changes a few things in the block...
            MAoutput << commandblock.ToBinaryString();

            MAResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 2);

            REQUIRE(res == CommandStatus::SUCCESS);
      }

      INFO("POM ODC->Master Command Test");
      {
            // So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct CB command,
            // and then also when we send the correct response we get an ODC::success message.

            CommandStatus res = CommandStatus::NOT_AUTHORIZED;
            auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
                  {
                        res = command_stat;
                  });

            bool point_on = true;
            uint16_t ODCIndex = 116;

            EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
            val.functionCode = point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

            auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
            event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

            // Send an ODC DigitalOutput command to the Master.
            CBMAPort->Event(event, "TestHarness", pStatusCallback);

            Wait(IOS, 2);

            // Need to check that the hooked tcp output function got the data we expected.
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126003b00" "03d98000cc00")); // POM Command

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM commandblock(resp); // Changes a few things in the block...
            MAoutput << commandblock.ToBinaryString();

            MAResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 2);

            REQUIRE(res == CommandStatus::SUCCESS);
      }

      INFO("DOM OutStation->ODC->Master Command Test");
      {
            // We want to send a DOM Command to the OutStation, have it convert that to (up to) 16   Events.
            // The Master will then ask for a response to those events (all 16!!), which we have to give it, as simulated TCP.
            // Each should be responded to with an OK packet, and its callback executed.

            IOS.post([&]()
                  {
                        CBBlockData commandblock(9, 33);                           // DOM Module is 33 - only 1 point defined, so should only have one DOM command generated.
                        CBBlockData datablock = commandblock.GenerateSecondBlock(MISSINGVALUE); // Bit 0 ON? Top byte ON, bottom byte OFF

                        OSoutput << commandblock.ToBinaryString();
                        OSoutput << datablock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 1);

            // So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1321de0300" "80008003c300")); // Should be 1 DOM command.

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
            MAoutput << rcommandblock.ToBinaryString();

            OSResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 4);

            REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f21de6000")); // OK Command
      }


      INFO("POM OutStation->ODC->Master Command Test");
      {
            // We want to send a POM Command to the OutStation, it is a single bit and event, so easy compared to DOM
            // It should responded with an OK packet, and its callback executed.

            OSResponse = "Not Set";
            MAResponse = "Not Set";

            IOS.post([&]()
                  {
                        CBBlockData commandblock(9, 38, 0); // POM Module is 38, 116 to 123 Idx
                        CBBlockData datablock = commandblock.GenerateSecondBlock();

                        OSoutput << commandblock.ToBinaryString();
                        OSoutput << datablock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 1);

            // So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126003b00" "03d98000cc00")); // Should be 1 POM command.

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
            MAoutput << rcommandblock.ToBinaryString();

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            // The response should then flow through ODC, back to the OutStation who should then send the OK out on TCP.
            Wait(IOS, 5);

            REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f26006000")); // OK Command
      }

      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - DOM and POM Pass Through Tests")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;
      MAportoverride["DOMControlPoint"]["Index"] = 60000; // Set up the magic port to match
      MAportoverride["POMControlPoint"]["Index"] = 60001; // Set up the magic port to match
      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = true;
      OSportoverride["DOMControlPoint"]["Index"] = 60000;
      OSportoverride["POMControlPoint"]["Index"] = 60001;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

      CBOSPort->Enable();
      CBMAPort->Enable();

      // Hook the output functions
      std::string OSResponse = "Not Set";
      CBOSPort->SetSendTCPDataFn([&OSResponse](std::string CBMessage) { OSResponse = CBMessage; });

      std::string MAResponse = "Not Set";
      CBMAPort->SetSendTCPDataFn([&MAResponse](std::string CBMessage)
            {
                  MAResponse = CBMessage;
            });

      asio::streambuf OSwrite_buffer;
      std::ostream OSoutput(&OSwrite_buffer);

      asio::streambuf MAwrite_buffer;
      std::ostream MAoutput(&MAwrite_buffer);

      INFO("DOM OutStation->ODC->Master Pass Through Command Test");
      {
            // We want to send a DOM Command to the OutStation, but pass it through ODC unchanged. Use a "magic" analog port to do this.

            IOS.post([&]()
                  {
                        CBBlockData commandblock(9, 33);                           // DOM Module is 33 - only 1 point defined, so should only have one DOM command generated.
                        CBBlockData datablock = commandblock.GenerateSecondBlock(MISSINGVALUE); // Bit 0 ON?

                        OSoutput << commandblock.ToBinaryString();
                        OSoutput << datablock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 1);

            // So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1321de0300" "80008003c300")); // Should passed through DOM command - match what was sent.

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
            MAoutput << rcommandblock.ToBinaryString();

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 3);

            REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f21de6000")); // OK Command
      }


      INFO("POM OutStation->ODC->Master Pass Through Command Test");
      {
            LOGDEBUG("POM OutStation->ODC->Master Pass Through Command Test");

            // We want to send a POM Command to the OutStation.
            // It should respond with an OK packet, and its callback executed.

            OSResponse = "Not Set";
            MAResponse = "Not Set";

            CBBlockData commandblock(9, 38, 0); // POM Module is 38, 116 to 123 Idx
            CBBlockData datablock = commandblock.GenerateSecondBlock();

            IOS.post([&]()
                  {
                        OSoutput << commandblock.ToBinaryString();
                        OSoutput << datablock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 3);

            // So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
            REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126003b00" "03d98000cc00")); // Should be 1 POM command.

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
            MAoutput << rcommandblock.ToBinaryString();

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            // The response should then flow through ODC, back to the OutStation who should then send the OK out on TCP.
            Wait(IOS, 3);

            REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f26006000")); // OK Command
      }

      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - TimeDate Poll and Pass Through Tests")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;
      MAportoverride["TimeSetPoint"]["Index"] = 60000; // Set up the magic port to match
      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = false;
      OSportoverride["TimeSetPoint"]["Index"] = 60000;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

      CBOSPort->Enable();
      CBMAPort->Enable();

      // Hook the output functions
      std::string OSResponse = "Not Set";
      CBOSPort->SetSendTCPDataFn([&OSResponse](std::string CBMessage) { OSResponse = CBMessage; });

      std::string MAResponse = "Not Set";
      CBMAPort->SetSendTCPDataFn([&MAResponse](std::string CBMessage) { MAResponse = CBMessage; });

      asio::streambuf OSwrite_buffer;
      std::ostream OSoutput(&OSwrite_buffer);

      asio::streambuf MAwrite_buffer;
      std::ostream MAoutput(&MAwrite_buffer);


      INFO("Time Set Poll Command");
      {
            // The config file has the timeset poll as group 2.
            CBMAPort->DoPoll(3);

            Wait(IOS, 1);

            // We check the command, but it does not go anywhere, we inject the expected response below.
            // The value for the time will always be different...
            REQUIRE(MAResponse[0] == 9);
            REQUIRE(MAResponse[1] == 0x2b);
            REQUIRE(MAResponse.size() == 12);

            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM commandblock(resp); // Changes a few things in the block...

            MAoutput << commandblock.ToBinaryString();

            MAResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 1);

            // Check there is no resend of the command - we must have got an OK packet.
            REQUIRE(MAResponse == "Not Set");
      }

      INFO("Time Set TCP to OutStation to Master to TCP");
      {
            // So we post time change command to the outstation, which should then go to the Master, which should then send a timechange command out on TCP.
            // If the Outstation is standalone, it will not wait for the ODC response.
            // "TimeSetPoint" : {"Index" : 100000},
            uint64_t currenttime = CBNow(); // 0x1111222233334444;

            // TimeChange command (Fn 43), Station 9
            CBBlockData commandblock(9, currenttime % 1000);
            CBBlockData datablock(static_cast<uint32_t>(currenttime / 1000), true);

            std::string TimeChangeCommand = commandblock.ToBinaryString() + datablock.ToBinaryString();

            OSoutput << TimeChangeCommand;

            // Send the Command - but this ends up waiting for the ODC call to return, but we need to send in what the call is expecting..
            // Need another thread dealing with the other port?

            // This will execute after InjectSimulatedTCPMessage??
            IOS.post([&]()
                  {
                        // Now send correct response (simulated TCP) to Master,
                        // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
                        CBBlockFn15StoM OKcommandblock(commandblock); // Changes a few things in the block...
                        MAoutput << OKcommandblock.ToBinaryString();

                        Wait(IOS, 1); // Must be sent after we send the command to the outstation
                        LOGDEBUG("Test - Injecting MA message as TCP Input");
                        CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer); // This will also wait for completion?
                  });

            LOGDEBUG("Test - Injecting OS message as TCP Input");
            CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This must be sent first
            Wait(IOS, 4);                                         // Outstation decodes message, sends ODC event, Master gets ODC event, sends out command on TCP.

            // Check the Master port output on TCP - it should be an identical time change command??
            REQUIRE(MAResponse == TimeChangeCommand);

            // Now check we have an OK packet being sent by the OutStation.
            REQUIRE(OSResponse[0] == ToChar(0xFC));
            REQUIRE(OSResponse[1] == ToChar(0x0F)); // OK Command
            REQUIRE(OSResponse.size() == 6);
      }

      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - SystemSignOn and FreezeResetCounter Pass Through Tests")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;
      MAportoverride["FreezeResetCountersPoint"]["Index"] = 60000; // Set up the magic port to match
      MAportoverride["SystemSignOnPoint"]["Index"] = 60001;        // Set up the magic port to match
      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = true;
      OSportoverride["FreezeResetCountersPoint"]["Index"] = 60000;
      OSportoverride["SystemSignOnPoint"]["Index"] = 60001;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

      CBOSPort->Enable();
      CBMAPort->Enable();

      // Hook the output functions
      std::string OSResponse = "Not Set";
      CBOSPort->SetSendTCPDataFn([&OSResponse](std::string CBMessage) { OSResponse = CBMessage; });

      std::string MAResponse = "Not Set";
      CBMAPort->SetSendTCPDataFn([&MAResponse](std::string CBMessage)
            {
                  MAResponse = CBMessage;
            });

      asio::streambuf OSwrite_buffer;
      std::ostream OSoutput(&OSwrite_buffer);

      asio::streambuf MAwrite_buffer;
      std::ostream MAoutput(&MAwrite_buffer);

      // We send a command to the outstation (as if from a master) it goes through ODC, and the Master then spits out a command. We respond to this and
      // the response should find its way back and be spat out by the outstation...
      INFO("System Sign On OutStation->ODC->Master Pass Through Command Test");
      {
            // We want to send a System Sign On Command to the OutStation, but pass it through ODC unchanged. Use a "magic" analog port to do this.
            CBBlockData commandblock(9);

            IOS.post([&]()
                  {
                        // Insert the command into the OutStation
                        OSoutput << commandblock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 2);

            // So the command we started above, will eventually result in a return packet with the only change being the direction
            // But first we have to check and then respond to the masters output.
            REQUIRE(MAResponse == commandblock.ToBinaryString()); // Should passed through System Sign On command - match what was sent.

            // We now inject the expected response to the command above, the same command but S to M
            CBBlockFn40StoM rcommandblock(MAResponse[0] & 0x7F); // Just return the station address
            MAoutput << rcommandblock.ToBinaryString();

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 3);

            REQUIRE(OSResponse == rcommandblock.ToBinaryString()); // Returned command
      }


      INFO("Freeze Reset OutStation->ODC->Master Pass Through Command Test");
      {
            // We want to send a FreezeReset Command to the OutStation.
            // It should respond with an OK packet, and its callback executed.

            OSResponse = "Not Set";
            MAResponse = "Not Set";

            CBBlockData commandblock(9, true); // TRUE == Don't reset counters

            IOS.post([&]()
                  {
                        OSoutput << commandblock.ToBinaryString();

                        CBOSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
                  });

            Wait(IOS, 3);

            // So the command we started above, will result in sending the same command out of the master.. It will eventually result in an OK packet back from the slave.
            REQUIRE(MAResponse == commandblock.ToBinaryString() );

            // Now send the OK packet back in.
            // We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
            CBBlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
            CBBlockFn15StoM rcommandblock(resp); // Changes a few things in the block...turn into OK packet
            MAoutput << rcommandblock.ToBinaryString();

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            // The response should then flow through ODC, back to the OutStation who should then send the OK out on TCP.
            Wait(IOS, 3);

            REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f01034600")); // OK Command
      }


      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - Digital Fn11 Command Test")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;

      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = false;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

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

      INFO("Test actual returned data for DCOS 11");
      {
            // We have two modules in this poll group, 34 and 35, ODC points 0 to 31.
            // Will send 3 time tagged events, and data for both modules
            // 34 set to 8000, 35 to FF00
            // Timedate is msec, but need seconds
            // Then COS records, and we insert one time block to test the decoding which is only 16 bits and offsets everything...
            // So COS records are 22058000, 23100100, time extend, 2200fe00, time extend/padding
            CBTime changedtime = static_cast<CBTime>(0x0000016338b6d4fb);
            CBBlockData b[] = { CBBlockFn11StoM(9, 4, 1, 2),CBBlockData(0x22008000),CBBlockData(0x2300ff00), CBBlockData(static_cast<uint32_t>(changedtime / 1000)),
                                 CBBlockData(0x22058000), CBBlockData(0x23100100), CBBlockData(0x00202200),CBBlockData(0xfe000000,true) };


            for (auto bl : b)
                  MAoutput << bl.ToBinaryString();

            MAResponse = "Not Set";

            // Send the command
            auto cmdblock = CBBlockData(9, 15, 1, 2); // Up to 15 events, sequence #1, Two modules.
            CBMAPort->QueueCBCommand(cmdblock, nullptr);    // No callback, does not originate from ODC
            Wait(IOS, 2);

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer); // Sends MAoutput

            Wait(IOS, 3);

            // Check the module values made it into the point table
            bool ModuleFailed = false;
            uint16_t wordres = CBOSPort->GetPointTable()->CollectModuleBitsIntoWord(0x22, ModuleFailed);
            REQUIRE(wordres == 0xfe00); //MISSINGVALUE Would be this value if no timetagged data was present

            wordres = CBOSPort->GetPointTable()->CollectModuleBitsIntoWord(0x23, ModuleFailed);
            REQUIRE(wordres == 0x0100); //0xff00 Would be this value if no timetagged data was present

            // Get the list of time tagged events, and check...
            std::vector<CBBinaryPoint> PointList = CBOSPort->GetPointTable()->DumpTimeTaggedPointList();
            REQUIRE(PointList.size() == 0x5f);
            REQUIRE(PointList[50].GetIndex() == 0);
            REQUIRE(PointList[50].GetModuleBinarySnapShot() == 0xffff);
            //		  REQUIRE(PointList[50].ChangedTime == 0x00000164ee106081);

            REQUIRE(PointList[80].GetIndex() == 0x1e);
            REQUIRE(PointList[80].GetBinary() == 0);
            REQUIRE(PointList[80].GetModuleBinarySnapShot() == 0xff01);
            //		  REQUIRE(PointList[80].ChangedTime == 0x00000164ee1e751c);
      }
      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - Digital Poll Tests (New Commands Fn11/12)")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;

      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = false;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

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

      INFO("New Digital Poll Command");
      {
            // Poll group 1, we want to send out the poll command from the master,
            // inject a response and then look at the Masters point table to check that the data got to where it should.
            // We could also check the linked OutStation to make sure its point table was updated as well.

            CBMAPort->DoPoll(1); // Will send an unconditional the first time on startup, or if forced. From the logging on the RTU the response is packet 11 either way

            Wait(IOS, 1);

            // We check the command, but it does not go anywhere, we inject the expected response below.
            // Request DigitalUnconditional (Fn 12), Station 9,  sequence #1, up to 2 modules returned - that is what the RTU we are testing has
            CBBlockData commandblock = CBBlockData(9, 0x22, 1, 2); // Check out methods give the same result

            REQUIRE(MAResponse[0] == commandblock.GetByte(0));
            REQUIRE(MAResponse[1] == commandblock.GetByte(1));
            REQUIRE(MAResponse[2] == static_cast<char>(commandblock.GetByte(2)));
            REQUIRE((MAResponse[3] & 0x0f) == (commandblock.GetByte(3) & 0x0f)); // Not comparing the sequence count, only the module count

            // We now inject the expected response to the command above, a DCOS block
            // Want to test this response from an actual RTU. We asked for 15 modules, so it tried to give us that. Should only ask for what we want.

            //CBBlockData b[] = {"fc0b016f3f00","100000009a00","00001101a100","00001210bd00","00001310af00","000014108a00","000015109800","00001610ae00","00001710bc00","00001810bf00","00001910ad00","00001a109b00","00001b108900","00001c10ac00","00001d10be00","00001e10c800" };

            // We have two modules in this poll group, 34 and 35, ODC points 0 to 31.
            // Will send 3 time tagged events, and data for both modules
            // 34 set to 8000, 35 to FF00
            // Timedate is msec, but need seconds
            // Then COS records, and we insert one time block to test the decoding which is only 16 bits and offsets everything...
            // So COS records are 22058000, 23100100, time extend, 2200fe00, time extend/padding
            CBTime changedtime = static_cast<CBTime>(0x0000016338b6d4fb);
            CBBlockData b[] = {CBBlockFn11StoM(9, 4, 1, 2),CBBlockData(0x22008000),CBBlockData(0x2300ff00), CBBlockData(static_cast<uint32_t>(changedtime/1000)),
                                CBBlockData(0x22058000), CBBlockData(0x23100100), CBBlockData(0x00202200),CBBlockData(0xfe000000,true)};

            for (auto bl :b)
                  MAoutput << bl.ToBinaryString();

            MAResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 4);

            // Check there is no resend of the command - the response must have been ok.
            REQUIRE(MAResponse == "Not Set");

            // Get the list of time tagged events, and check...
            std::vector<CBBinaryPoint> PointList = CBOSPort->GetPointTable()->DumpTimeTaggedPointList();
            REQUIRE(PointList.size() == 0x5f);
            REQUIRE(PointList[50].GetIndex() == 0);
            REQUIRE(PointList[50].GetModuleBinarySnapShot() == 0xffff);
            //		  REQUIRE(PointList[50].ChangedTime == 0x00000164ee106081);

            REQUIRE(PointList[80].GetIndex() == 0x1e);
            REQUIRE(PointList[80].GetBinary() == 0);
            REQUIRE(PointList[80].GetModuleBinarySnapShot() == 0xff01);
            //		  REQUIRE(PointList[80].ChangedTime == 0x00000164ee1e751c);
      }

      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - System Flag Scan Poll Test")
{
      STANDARD_TEST_SETUP();

      Json::Value MAportoverride;

      TEST_CBMAPort(MAportoverride);

      Json::Value OSportoverride;
      OSportoverride["Port"] = static_cast<Json::UInt64>(1001);
      OSportoverride["StandAloneOutstation"] = false;
      TEST_CBOSPort(OSportoverride);

      START_IOS(1);

      // The subscriber is just another port. CBOSPort is registering to get CBPort messages.
      // Usually is a cross subscription, where each subscribes to the other.
      CBMAPort->Subscribe(CBOSPort, "TestLink");
      CBOSPort->Subscribe(CBMAPort, "TestLink");

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

      INFO("Flag Scan Poll Command");
      {
            // Poll group 5, we want to send out the poll command from the master, then check the response.

            CBMAPort->DoPoll(5); // Expect the RTU to be in startup mode with flags set

            Wait(IOS, 1);

            // We check the command, but it does not go anywhere, we inject the expected response below.
            const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("7c3400006100");

            REQUIRE(MAResponse == DesiredResult1);

            MAResponse = "Not Set";


            CBBlockFn52StoM commandblock = CBBlockFn52StoM(CBBlockData(9, true, SYSTEM_FLAG_SCAN, 0x22, 1, true)); // Check out methods give the same result
            bool SPU = true;                                                                                                  // System power on flag - should trigger unconditional scans
            bool STI = true;                                                                                                  // System time invalid - should trigger a time command
            commandblock.SetSystemFlags(SPU, STI);

            MAoutput << commandblock.ToBinaryString();
            MAResponse = "Not Set";

            CBMAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

            Wait(IOS, 4);

            // There are a lot of things to check  here depending on what functionality is enabled.
            // A time response, all digital scans, all analog scans...

            // We should get a time set command back first (little dependent on scheduling?
            REQUIRE(MAResponse[0] == 9);
            REQUIRE(MAResponse[1] == 0x2b);
            REQUIRE(MAResponse.size() == 12);

            // Catching the rest is difficult!
      }
      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
TEST_CASE("Master - Binary Scan Multi-drop Test Using TCP")
{
      // Here we test the ability to support multiple Stations on the one Port/IP Combination. The Stations will be 9, 0x7D

      STANDARD_TEST_SETUP();
      TEST_CBOSPort(Json::nullValue);
      TEST_CBOSPort2(Json::nullValue);

      START_IOS(1);

      CBOSPort->Enable();
      CBOSPort2->Enable();

      std::vector<std::string> ResponseVec;
      auto ResponseCallback = [&](buf_t& readbuf)
                              {
                                    size_t bufsize = readbuf.size();
                                    std::string S(bufsize, 0);

                                    for (size_t i = 0; i < bufsize; i++)
                                    {
                                          S[i] = static_cast<char>(readbuf.sgetc());
                                          readbuf.consume(1);
                                    }
                                    ResponseVec.push_back(S); // Store so we can check
                              };

      bool socketisopen = false;
      auto SocketStateHandler = [&socketisopen](bool state)
                                {
                                      socketisopen = state;
                                };

      // An outstation is a server by default (Master connects to it...)
      // Open a client socket on 127.0.0.1, 1000 and see if we get what we expect...
      std::shared_ptr<TCPSocketManager<std::string>> pSockMan;
      pSockMan.reset(new TCPSocketManager<std::string>
                  (&IOS, false, "127.0.0.1", "1000",
                  ResponseCallback,
                  SocketStateHandler,
                  std::numeric_limits<size_t>::max(),
                  true,
                  500));
      pSockMan->Open();

      Wait(IOS, 3);
      REQUIRE(socketisopen); // Should be set in a callback.

      // Send the Command - results in an async write
      //  Station 9
      CBBlockData commandblock(9, true);
      pSockMan->Write(commandblock.ToBinaryString());

      //  Station 0x7D
      CBBlockData commandblock2(0x7D, true);
      pSockMan->Write(commandblock2.ToBinaryString());

      Wait(IOS, 5); // Allow async processes to run.

      // Need to handle multiple responses...
      // Deal with the last response first...
      REQUIRE(ResponseVec.size() == 2);

      REQUIRE(ResponseVec.back() == BuildHexStringFromASCIIHexString("fd0f01027c00")); // OK Command
      ResponseVec.pop_back();

      REQUIRE(ResponseVec.back() == BuildHexStringFromASCIIHexString("fc0f01034600")); // OK Command
      ResponseVec.pop_back();

      REQUIRE(ResponseVec.empty());

      pSockMan->Close();
      CBOSPort->Disable();
      CBOSPort2->Disable();

      STOP_IOS();
      TestTearDown();
}
*/
#pragma endregion
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

      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
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


      work.reset(); // Indicate all work is finished.

      STOP_IOS();
      TestTearDown();
}
*/
}
#endif