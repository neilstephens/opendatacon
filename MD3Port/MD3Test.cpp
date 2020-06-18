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
* MD3Test.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifdef _MSC_VER
// Disable excessing data on stack warning for the test file only.
#pragma warning(disable: 6262)
#endif

#define COMPILE_TESTS

#ifdef COMPILE_TESTS


// #include <trompeloeil.hpp> Not used at the moment - requires __cplusplus to be defined so the cppcheck works properly.
#include "MD3MasterPort.h"
#include "MD3OutstationPort.h"
#include "MD3Utility.h"
#include "ProducerConsumerQueue.h"
#include "StrandProtectedQueue.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <array>
#include <cassert>
#include <fstream>
#include <opendatacon/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <utility>

#ifdef NONVSTESTING
#include <catch.hpp>
#else
#include "spdlog/sinks/msvc_sink.h"
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer
#endif

#define SUITE(name) "MD3Tests - " name

// To remove GCC warnings
namespace RTUConnectedTests
{
extern const char *md3masterconffile;
}

extern const char *conffilename1;
extern const char *conffilename2;
extern const char *conffile1;
extern const char *conffile2;
extern std::vector<spdlog::sink_ptr> LogSinks;


const char *conffilename1 = "MD3Config.conf";
const char *conffilename2 = "MD3Config2.conf";

#ifdef _MSC_VER
#pragma region conffiles
#endif
// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile1 = R"001(
{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 124,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	// If a binary event time stamp is outside 30 minutes of current time, replace the timestamp
	"OverrideOldTimeStamps" : false,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 3000,
	"MD3CommandRetries" : 1,

	// Master only PollGroups
	// Cannot mix analog and binary points in a poll group. Group 1 is Binary, Group 2 is Analog in this example
	// You will get errors if the wrong type of points are assigned to the wrong poll group
	// The force unconditional parameter will force the code to NOT use delta commands during polling.
	// The TimeSetCommand is used to send time set commands to the OutStation
	// The "TimeTaggedDigital" flag if true, asks for COS records in addition to "Normal" digital values in the scan (using Fn11)
	// When we scan digitals, we do a scan for each module. The logic of scanning multiple modules gets a little tricky.
	// The digital scans (what is scanned) is worked out from the Binary point definition. The first module address,
	// the total number of modules which are part of the Fn11 and 12 commands

	"PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary", "TimeTaggedDigital" : true },
					{"PollRate" : 20000, "ID" : 2, "PointType" : "Analog", "ForceUnconditional" : false },
					{"PollRate" : 60000, "ID" : 3, "PointType" : "TimeSetCommand"},
					{"PollRate" : 60000, "ID" : 4, "PointType" : "Binary",  "TimeTaggedDigital" : false },
					{"PollRate" : 180000, "ID" : 5, "PointType" : "SystemFlagScan"},
					{"PollRate" : 60000, "ID" : 6, "PointType" : "NewTimeSetCommand"},
					{"PollRate" : 60000, "ID" : 7, "PointType" : "Counter"},
					{"PollRate" : 60000, "ID" : 8, "PointType" : "Counter"}],

	// We expect the binary modules to be consecutive - MD3 Addressing - (or consecutive groups for scanning), the scanning is then simpler
	"Binaries" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "PollGroup" : 1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PollGroup":1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 32, "Stop" : 47}, "Module" : 63, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0, "PollGroup" : 2}],

	"BinaryControls" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Index": 81,  "Module" : 34, "Offset" : 0, "PointType" : "DIMOUTPUT"},
						{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PointType" : "POMOUTPUT"}],

	"Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Module" : 61, "Offset" : 0, "PollGroup" : 7},
					{"Range" : {"Start" : 8, "Stop" : 15}, "Module" : 62, "Offset" : 0, "PollGroup" : 8}],

	"AnalogControls" : [{"Range" : {"Start" : 1, "Stop" : 8}, "Module" : 39, "Offset" : 0}]
})001";


// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile2 = R"002(
{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 125,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 4000,
	"MD3CommandRetries" : 1,

	"Binaries" : [{"Index": 90,  "Module" : 33, "Offset" : 0},
				{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 32, "Stop" : 47}, "Module" : 63, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0}],

	"BinaryControls" : [{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PointType" : "POMOUTPUT"}],

	"Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Module" : 61, "Offset" : 0},{"Range" : {"Start" : 8, "Stop" : 15}, "Module" : 62, "Offset" : 0}]
})002";
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
	if (!ofs) FAIL("Could not open conffile1 for writing");

	ofs << conffile1;
	ofs.close();

	std::ofstream ofs2(conffilename2);
	if (!ofs2) FAIL("Could not open conffile2 for writing");

	ofs2 << conffile2;
	ofs2.close();
}
void SetupLoggers(spdlog::level::level_enum loglevel)
{
	// So create the log sink first - can be more than one and add to a vector.
	#ifdef NONVSTESTING
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	#else
	auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	#endif
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("md3porttest.log", true);

	std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

	auto pLibLogger = std::make_shared<spdlog::logger>("MD3Port", begin(sinks), end(sinks));
	pLibLogger->set_level(loglevel);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", begin(sinks), end(sinks));
	pODCLogger->set_level(spdlog::level::trace);
	odc::spdlog_register_logger(pODCLogger);
}
void WriteStartLoggingMessage(const std::string& TestName)
{
	std::string msg = "Logging for '" + TestName + "' started..";
	if (auto md3logger = odc::spdlog_get("MD3Port"))
	{
		md3logger->info("------------------");
		md3logger->info(msg);
	}
	else
		std::cout << "Error MD3Port Logger not operational";

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
	#endif

	if (writeconffiles)
		WriteConfFilesToCurrentWorkingDirectory();
}

void TestTearDown()
{
	spdlog::drop_all(); // Close off everything
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
void RunIOSForXSeconds(odc::asio_service &IOS, unsigned int seconds)
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
std::thread *StartIOSThread(odc::asio_service &IOS)
{
	return new std::thread([&] { IOS.run(); });
}
void StopIOSThread(odc::asio_service &IOS, std::thread *runthread)
{
	IOS.stop();        // This does not block. The next line will! If we have multiple threads, have to join all of them.
	runthread->join(); // Wait for it to exit
	delete runthread;
}
void Wait(odc::asio_service &IOS, int seconds)
{
	auto timer = IOS.make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(seconds));
	timer->wait();
}


// Don't like using macros, but we use the same test set up almost every time.
#define STANDARD_TEST_SETUP()\
	TestSetup();\
	auto IOS = odc::asio_service::Get()

#define START_IOS(threadcount) \
	LOGINFO("Starting ASIO Threads"); \
	auto work = IOS->make_work(); /* To keep run - running!*/\
	const int ThreadCount = threadcount; \
	std::thread *pThread[threadcount]; \
	for (int i = 0; i < (threadcount); i++) pThread[i] = StartIOSThread(*IOS)

#define STOP_IOS() \
	LOGINFO("Shutting Down ASIO Threads");    \
	work.reset();     \
	for (int i = 0; i < ThreadCount; i++) StopIOSThread(*IOS, pThread[i])

#define TEST_MD3MAPort(overridejson)\
	auto MD3MAPort = std::make_shared<MD3MasterPort>("TestMaster", conffilename1, overridejson); \
	MD3MAPort->Build()

#define TEST_MD3MAPort2(overridejson)\
	auto MD3MAPort2 = std::make_shared<MD3MasterPort>("TestMaster2", conffilename2, overridejson); \
	MD3MAPort2->Build()

#define TEST_MD3OSPort(overridejson)      \
	auto MD3OSPort = std::make_shared<MD3OutstationPort>("TestOutStation", conffilename1, overridejson);   \
	MD3OSPort->Build()

#define TEST_MD3OSPort2(overridejson)     \
	auto MD3OSPort2 = std::make_shared<MD3OutstationPort>("TestOutStation2", conffilename2, overridejson); \
	MD3OSPort2->Build()

#ifdef _MSC_VER
#pragma endregion TEST_HELPERS
#endif

namespace SimpleUnitTests
{

TEST_CASE("Utility - HexStringTest")
{
	std::string ts = "c406400f0b00"  "0000fffe9000";
	std::string w1 = { ToChar(0xc4),0x06,0x40,0x0f,0x0b,0x00 };
	std::string w2 = { 0x00,0x00,ToChar(0xff),ToChar(0xfe),ToChar(0x90),0x00 };

	std::string res = BuildHexStringFromASCIIHexString(ts);
	REQUIRE(res == (w1 + w2));
}

TEST_CASE("Utility - MD3CRCTest")
{
	// The first 4 are all formatted first and only packets
	uint8_t res = MD3CRC(0x7C05200F);
	REQUIRE(MD3CRCCompare(res, 0x52));

	res = MD3CRC(0x910d400f);
	REQUIRE(MD3CRCCompare(res, 0x76));

	res = MD3CRC(0xaa0d160f);
	REQUIRE(MD3CRCCompare(res, 0x62));

	res = MD3CRC(0x8d0d200f);
	REQUIRE(MD3CRCCompare(res, 0x77));

	// Formatted first but not last packet
	res = MD3CRC(0x9c06200f);
	REQUIRE(MD3CRCCompare(res, 0x1a));

	// Non formatted packet, not last packet
	res = MD3CRC(0x0000FFfe);
	REQUIRE(MD3CRCCompare(res, 0x90));

	// Non formatted packet, also last packet
	res = MD3CRC(0x00000000);
	REQUIRE(MD3CRCCompare(res, 0xff));
}

TEST_CASE("Utility - Strand Queue")
{
	auto pIOS = odc::asio_service::Get();

	auto work = pIOS->make_work(); // Just to keep things from stopping..

	std::thread t1([&]() {pIOS->run(); });
	std::thread t2([&]() {pIOS->run(); });

	StrandProtectedQueue<int> foo(*pIOS, 10);
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

	work.reset();

	t1.join(); // Wait for thread to end
	t2.join();
}
#ifdef _MSC_VER
#pragma region Block Tests
#endif

TEST_CASE("MD3Block - ClassConstructor1")
{
	uint8_t stationaddress = 124;
	bool mastertostation = true;
	MD3_FUNCTION_CODE functioncode = ANALOG_UNCONDITIONAL;
	uint8_t moduleaddress = 0x20;
	uint8_t channels = 16;
	bool lastblock = true;
	bool APL = false;
	bool RSF = false;
	bool HRP = false;
	bool DCP = false;

	MD3BlockArray msg = { 0x7C,0x05,0x20,0x0F,0x52, 0x00 }; // From a packet capture

	MD3BlockData db(msg);
	MD3BlockFormatted b(db);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	MD3BlockData b2("d41f00103c00");
	REQUIRE(b2.GetData() == 0xd41f0010);
	REQUIRE(b2.CheckSumPasses());
}
TEST_CASE("MD3Block - ClassConstructor2")
{
	uint8_t stationaddress = 0x33;
	bool mastertostation = false;
	MD3_FUNCTION_CODE functioncode = ANALOG_UNCONDITIONAL;
	uint8_t moduleaddress = 0x30;
	uint8_t channels = 16;
	bool lastblock = true;
	bool APL = false;
	bool RSF = false;
	bool HRP = false;
	bool DCP = false;

	MD3BlockFormatted b(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	mastertostation = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	lastblock = false;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	channels = 1;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	moduleaddress = 0xFF;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	stationaddress = 0x7F;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	stationaddress = 0x01;
	moduleaddress = 0x01;
	APL = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	RSF = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	HRP = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	DCP = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HRP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());
}
TEST_CASE("MD3Block - ClassConstructor3")
{
	uint16_t firstword = 12;
	uint16_t secondword = 32000;
	bool lastblock = true;

	MD3BlockData b(firstword, secondword, lastblock);

	REQUIRE(b.GetFirstWord() == firstword);
	REQUIRE(b.GetSecondWord() == secondword);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());

	firstword = 3200;
	secondword = 16000;
	lastblock = false;

	b = MD3BlockData(firstword, secondword, lastblock);

	REQUIRE(b.GetFirstWord() == firstword);
	REQUIRE(b.GetSecondWord() == secondword);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());
}
TEST_CASE("MD3Block - ClassConstructor4")
{
	uint32_t data = 128364324;
	bool lastblock = true;

	MD3BlockData b(data, lastblock);

	REQUIRE(b.GetData() == data);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());

	data = 8364324;
	lastblock = false;

	b = MD3BlockData(data, lastblock);

	REQUIRE(b.GetData() == data);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());
}
TEST_CASE("MD3Block - ClassConstructor5")
{
	uint32_t data = 0x32F1F203;
	bool lastblock = true;
	uint8_t bc[] = {0x32, 0xF1, 0xf2, 0x03};

	MD3BlockData b(bc[0],bc[1],bc[2],bc[3], lastblock);

	REQUIRE(b.GetData() == data);
	REQUIRE(b.GetByte(0) == 0x32);
	REQUIRE(b.GetByte(1) == 0xF1);
	REQUIRE(b.GetByte(2) == 0xF2);
	REQUIRE(b.GetByte(3) == 0x03);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());
}
TEST_CASE("MD3Block - BlockBuilding")
{
	MD3BlockData db;

	// From a packet capture - 0x7C,0x05,0x20,0x0F,0x52, 0x00
	// Test the block building code.
	db.AddByteToBlock(0x7c);
	db.AddByteToBlock(0x05);
	db.AddByteToBlock(0x20);
	db.AddByteToBlock(0x0f);
	db.AddByteToBlock(0x52);
	db.AddByteToBlock(0x00);

	REQUIRE(db.IsValidBlock());

	uint8_t stationaddress = 124;
	bool mastertostation = true;
	MD3_FUNCTION_CODE functioncode = ANALOG_UNCONDITIONAL;
	uint8_t moduleaddress = 0x20;
	uint8_t channels = 16;
	bool lastblock = true;
	bool APL = false;
	bool RSF = false;
	bool HRP = false;
	bool DCP = false;

	MD3BlockFormatted b(db);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHRP() == HRP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());


	// Now add some more bytes, check that they block is invalid until we should have a valid block again...
	db.AddByteToBlock(0x7c);
	REQUIRE(!db.IsValidBlock());

	db.AddByteToBlock(0x05);
	REQUIRE(!db.IsValidBlock());

	db.AddByteToBlock(0x20);
	REQUIRE(!db.IsValidBlock());

	db.AddByteToBlock(0x0f);
	REQUIRE(!db.IsValidBlock());

	db.AddByteToBlock(0x52);
	REQUIRE(!db.IsValidBlock());

	db.AddByteToBlock(0x00);

	REQUIRE(db.IsValidBlock());
}
TEST_CASE("MD3Block - Fn9")
{
	MD3BlockFn9 b9(0x20, true, 3, 100, false, true);

	REQUIRE(b9.GetStationAddress() == 0x20);
	REQUIRE(b9.IsMasterToStationMessage() == true);
	REQUIRE(b9.GetFunctionCode() == 9);
	REQUIRE(b9.GetMoreEventsFlag() == false);
	REQUIRE(b9.GetSequenceNumber() == 3);
	REQUIRE(b9.GetEventCount() == 100);
	REQUIRE(b9.IsEndOfMessageBlock() == true);
	REQUIRE(b9.CheckSumPasses());

	b9.SetEventCountandMoreEventsFlag(130, true);

	REQUIRE(b9.GetStationAddress() == 0x20);
	REQUIRE(b9.IsMasterToStationMessage() == true);
	REQUIRE(b9.GetFunctionCode() == 9);
	REQUIRE(b9.GetMoreEventsFlag() == true);
	REQUIRE(b9.GetSequenceNumber() == 3);
	REQUIRE(b9.GetEventCount() == 130);
	REQUIRE(b9.IsEndOfMessageBlock() == true);
	REQUIRE(b9.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn11")
{
	MD3BlockFn11MtoS b11(0x32, 15, 3, 14);
	REQUIRE(b11.GetStationAddress() == 0x32);
	REQUIRE(b11.IsMasterToStationMessage() == true);
	REQUIRE(b11.GetFunctionCode() == 11);
	REQUIRE(b11.GetDigitalSequenceNumber() == 3);
	REQUIRE(b11.GetTaggedEventCount() == 15);
	REQUIRE(b11.GetModuleCount() == 14);
	REQUIRE(b11.IsEndOfMessageBlock() == true);
	REQUIRE(b11.CheckSumPasses());

	MD3BlockFn11StoM b11a(0x32, 14, 4, 13);
	REQUIRE(b11a.GetStationAddress() == 0x32);
	REQUIRE(b11a.IsMasterToStationMessage() == false);
	REQUIRE(b11a.GetFunctionCode() == 11);
	REQUIRE(b11a.GetDigitalSequenceNumber() == 4);
	REQUIRE(b11a.CheckSumPasses());
	REQUIRE(b11a.GetTaggedEventCount() == 14);
	b11a.SetTaggedEventCount(7);
	REQUIRE(b11a.GetTaggedEventCount() == 7);
	REQUIRE(b11a.GetModuleCount() == 13);
	b11a.SetModuleCount(5);
	REQUIRE(b11a.GetModuleCount() == 5);
	REQUIRE(b11a.IsEndOfMessageBlock() == false);
	REQUIRE(b11a.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn12")
{
	MD3BlockFn12MtoS b12(0x32, 70, 7, 15);
	REQUIRE(b12.GetStationAddress() == 0x32);
	REQUIRE(b12.IsMasterToStationMessage() == true);
	REQUIRE(b12.GetFunctionCode() == 12);
	REQUIRE(b12.GetDigitalSequenceNumber() == 7);
	REQUIRE(b12.GetStartingModuleAddress() == 70);
	REQUIRE(b12.GetModuleCount() == 15);
	REQUIRE(b12.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn14")
{
	// New style no change response constructor - contains a digital sequence number (6)
	MD3BlockFn14StoM b14(0x25, 6);
	REQUIRE(b14.IsMasterToStationMessage() == false);
	REQUIRE(b14.GetStationAddress() == 0x25);
	REQUIRE(b14.GetDigitalSequenceNumber() == 6);
	REQUIRE(b14.GetFunctionCode() == 14);
	REQUIRE(b14.GetData() == 0xa50e0600);
	REQUIRE(b14.IsEndOfMessageBlock() == true);
	REQUIRE(b14.CheckSumPasses());

	// Old style no change response constructor - contains a module address (0x70) and # of modules 0x0E
	MD3BlockFn14StoM b14a(0x25, 0x70, static_cast<uint8_t>(0x0E));
	REQUIRE(b14a.IsMasterToStationMessage() == false);
	REQUIRE(b14a.GetStationAddress() == 0x25);
	REQUIRE(b14a.GetModuleAddress() == 0x70);
	REQUIRE(b14a.GetFunctionCode() == 14);
	REQUIRE(b14a.GetModuleCount() == 0x0E);
	REQUIRE(b14a.GetData() == 0xa50e700e);
	REQUIRE(b14a.IsEndOfMessageBlock() == true);
	REQUIRE(b14a.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn17")
{
	MD3BlockFn17MtoS b17(0x63, 0xF0, 15);
	REQUIRE(b17.GetStationAddress() == 0x63);
	REQUIRE(b17.GetModuleAddress() == 0xF0);
	REQUIRE(b17.GetOutputSelection() == 15);
	REQUIRE(!b17.IsEndOfMessageBlock());
	REQUIRE(b17.IsFormattedBlock());
	REQUIRE(b17.CheckSumPasses());
	REQUIRE(b17.IsMasterToStationMessage() == true);

	MD3BlockData SecondBlock = b17.GenerateSecondBlock();
	REQUIRE(b17.VerifyAgainstSecondBlock(SecondBlock));
	REQUIRE(SecondBlock.IsEndOfMessageBlock());
	REQUIRE(!SecondBlock.IsFormattedBlock());
	REQUIRE(SecondBlock.CheckSumPasses());

	// So have verified against packet captures below.
	// From packet capture 2711a500 0300 585a8000 d500
	// Try and generate a packet that matches...bit 0 on.
	MD3BlockFn17MtoS b17a(0x27, 0xA5, 0); // Bit0
	REQUIRE(b17a.GetData() == 0x2711a500);
	MD3BlockData SecondBlocka = b17a.GenerateSecondBlock();
	REQUIRE(SecondBlocka.GetData() == 0x585a8000);

	// Second packet
	// 01119607 2c00 7e690100 f900
	MD3BlockFn17MtoS b17b(0x01, 0x96, 7); // Bit0
	REQUIRE(b17b.GetData() == 0x01119607);
	MD3BlockData SecondBlockb = b17b.GenerateSecondBlock();
	REQUIRE(SecondBlockb.GetData() == 0x7e690100);
}
TEST_CASE("MD3Block - Fn19")
{
	MD3BlockFn19MtoS b19(0x63, 0xF0);
	REQUIRE(b19.GetStationAddress() == 0x63);
	REQUIRE(b19.GetModuleAddress() == 0xF0);
	REQUIRE(!b19.IsEndOfMessageBlock());
	REQUIRE(b19.IsFormattedBlock());
	REQUIRE(b19.CheckSumPasses());
	REQUIRE(b19.IsMasterToStationMessage() == true);

	MD3BlockData Block2 = b19.GenerateSecondBlock(0x55);
	REQUIRE(b19.VerifyAgainstSecondBlock(Block2));
	REQUIRE(b19.GetOutputFromSecondBlock(Block2) == 0x55);
	REQUIRE(Block2.IsEndOfMessageBlock());
	REQUIRE(!Block2.IsFormattedBlock());
	REQUIRE(Block2.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn16")
{
	MD3BlockFn16MtoS b16(0x63, true);
	REQUIRE(b16.GetStationAddress() == 0x63);
	REQUIRE(b16.IsEndOfMessageBlock());
	REQUIRE(b16.IsFormattedBlock());
	REQUIRE(b16.CheckSumPasses());
	REQUIRE(b16.IsValid());
	REQUIRE(b16.GetNoCounterReset() == true);
	REQUIRE(b16.IsMasterToStationMessage() == true);

	MD3BlockFn16MtoS b16b(0x73, false);
	REQUIRE(b16b.GetStationAddress() == 0x73);
	REQUIRE(b16b.IsEndOfMessageBlock());
	REQUIRE(b16b.IsFormattedBlock());
	REQUIRE(b16b.CheckSumPasses());
	REQUIRE(b16b.IsValid());
	REQUIRE(b16b.GetNoCounterReset() == false);
}

TEST_CASE("MD3Block - Fn20")
{
	// Master from packet capture -> 1e140c951e0061f36a95960000200fdff600

	// Decode the captured packet to get the values and then rebuild it again!
	MD3BlockData FirstPacket("1e140c951e00");
	MD3BlockFn20MtoS commandblocktest(FirstPacket);

	uint8_t StationAddress = commandblocktest.GetStationAddress();     // 0x7C;
	uint8_t ModuleAddress = commandblocktest.GetModuleAddress();       //10;
	uint8_t ControlSelection = commandblocktest.GetControlSelection(); //9;
	uint8_t ChannelSelection = commandblocktest.GetChannelSelection(); //5;

	MD3BlockFn20MtoS commandblock(StationAddress, ModuleAddress,  ChannelSelection, ControlSelection);

	std::string DesiredResult = BuildHexStringFromASCIIHexString("1e140c951e00");
	std::string ActualResult = commandblock.ToBinaryString();
	REQUIRE(DesiredResult == ActualResult);

	MD3BlockData datablock1 = commandblock.GenerateSecondBlock(false); // Not last block
	DesiredResult = BuildHexStringFromASCIIHexString("61f36a959600");
	ActualResult = datablock1.ToBinaryString();
	REQUIRE(DesiredResult == ActualResult);

	MD3BlockData datablock2 = commandblock.GenerateThirdBlock(0x20); // Control value is 0x20 (32)
	DesiredResult = BuildHexStringFromASCIIHexString("00200fdff600");
	ActualResult = datablock2.ToBinaryString();
	REQUIRE(DesiredResult == ActualResult);
}
TEST_CASE("MD3Block - Fn23")
{
	MD3BlockFn23MtoS b23(0x63, 0x37, 15);
	REQUIRE(b23.GetStationAddress() == 0x63);
	REQUIRE(b23.GetModuleAddress() == 0x37);
	REQUIRE(b23.GetChannel() == 15);
	REQUIRE(!b23.IsEndOfMessageBlock());
	REQUIRE(b23.IsFormattedBlock());
	REQUIRE(b23.CheckSumPasses());
	REQUIRE(b23.IsMasterToStationMessage() == true);

	MD3BlockData SecondBlock23 = b23.GenerateSecondBlock(0x55);
	REQUIRE(b23.VerifyAgainstSecondBlock(SecondBlock23));
	REQUIRE(b23.GetOutputFromSecondBlock(SecondBlock23) == 0x55);
	REQUIRE(SecondBlock23.IsEndOfMessageBlock());
	REQUIRE(!SecondBlock23.IsFormattedBlock());
	REQUIRE(SecondBlock23.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn40")
{
	MD3BlockFn40MtoS b40(0x3F);
	REQUIRE(b40.IsValid());
	REQUIRE(b40.GetData() == 0x3f28c0d7);
	REQUIRE(b40.GetStationAddress() == 0x3F);
	REQUIRE(b40.IsEndOfMessageBlock());
	REQUIRE(b40.IsFormattedBlock());
	REQUIRE(b40.CheckSumPasses());
	REQUIRE(b40.IsMasterToStationMessage() == true);

	MD3BlockFn40StoM b40s(0x3F);
	REQUIRE(b40s.IsValid());
	REQUIRE(b40s.GetData() == 0xBf28c0d7);
	REQUIRE(b40s.GetStationAddress() == 0x3F);
	REQUIRE(b40s.IsEndOfMessageBlock());
	REQUIRE(b40s.IsFormattedBlock());
	REQUIRE(b40s.CheckSumPasses());
	REQUIRE(b40s.IsMasterToStationMessage() == false);
}
TEST_CASE("MD3Block - Fn43")
{
	// Actual captured test data 5e2b007a1300 5ad6ba8ce700
	TestSetup(false);
	MD3BlockFn43MtoS b43(0x38, 999);
	REQUIRE(b43.GetMilliseconds() == 999);
	REQUIRE(b43.GetStationAddress() == 0x38);
	REQUIRE(b43.IsMasterToStationMessage() == true);
	REQUIRE(b43.GetFunctionCode() == 43);
	REQUIRE(b43.IsEndOfMessageBlock() == false);
	REQUIRE(b43.IsFormattedBlock() == true);
	REQUIRE(b43.CheckSumPasses());

	// Test against capture timedate command.
	// 402b01632f00 5ad6b5b4d900
	MD3BlockFn43MtoS b43_t1(MD3BlockData("402b01632f00"));
	REQUIRE(b43_t1.CheckSumPasses());
	MD3BlockData b43_b2("5ad6b5b4d900");
	REQUIRE(b43_b2.CheckSumPasses());

	MD3Time timebase = static_cast<uint64_t>(b43_b2.GetData()) * 1000 + b43_t1.GetMilliseconds(); //MD3Time msec since Epoch.
	LOGDEBUG("TimeDate Packet Local : " + to_LOCALtimestringfromMD3time(timebase));

	TestTearDown();
}
TEST_CASE("MD3Block - Fn44")
{
	// Fn44 Example 262c02521d00 5ad6b75fbc00 02580000c200
	// The second block looks like the Fn43 second block which is seconds since epoch.
	// The first block could be essentially the same as Fn43
	// The 3rd block we have a clue is some kind of UTC offset. 0x258 is 600 - 600 minutes, or +10 hours - Our offset from UTC
	// Arrival Time : Apr 18, 2018 13 : 11 : 27.532344000 AUS Eastern Standard Time
	// Epoch Time : 1524021087.532344000 seconds
	TestSetup(false);

	MD3Time timebase = static_cast<uint64_t>(0x5ad6b75f) * 1000 + (0x252 & 0x03FF); //MD3Time msec since Epoch.
	LOGDEBUG("TimeDate Packet Local : " + to_LOCALtimestringfromMD3time(timebase));

	MD3BlockFn44MtoS b44(0x38, 999);
	REQUIRE(b44.GetMilliseconds() == 999);
	REQUIRE(b44.GetStationAddress() == 0x38);
	REQUIRE(b44.IsMasterToStationMessage() == true);
	REQUIRE(b44.GetFunctionCode() == 44);
	REQUIRE(b44.IsEndOfMessageBlock() == false);
	REQUIRE(b44.IsFormattedBlock() == true);
	REQUIRE(b44.CheckSumPasses());

	// Test against capture timedate command.
	// 402b01632f00 5ad6b5b4d900
	MD3BlockFn44MtoS b44_t1(MD3BlockData("262c02521d00"));
	REQUIRE(b44_t1.CheckSumPasses());
	MD3BlockData b44_b2("5ad6b75fbc00");
	REQUIRE(b44_b2.CheckSumPasses());
	MD3BlockData b44_b3("02580000c200");
	REQUIRE(b44_b3.CheckSumPasses());

	int UTCOffset = b44_b3.GetFirstWord();
	timebase = static_cast<uint64_t>(b44_b2.GetData()) * 1000 + b44_t1.GetMilliseconds(); //MD3Time msec since Epoch.
	LOGDEBUG("TimeDate Packet Local : " + to_LOCALtimestringfromMD3time(timebase)+ " UTC Offset Minutes "+std::to_string(UTCOffset));
	TestTearDown();
}
TEST_CASE("MD3Block - Fn15 OK Packet")
{
	MD3BlockFn43MtoS b43(0x38, 999);

	MD3BlockFn15StoM b15(b43);
	REQUIRE(b15.GetStationAddress() == 0x38);
	REQUIRE(b15.IsMasterToStationMessage() == false);
	REQUIRE(b15.GetFunctionCode() == 15);
	REQUIRE(b15.IsEndOfMessageBlock() == true);
	REQUIRE(b15.IsFormattedBlock() == true);
	REQUIRE(b15.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn30 Fail Packet")
{
	MD3BlockFn43MtoS b43(0x38, 999);
	MD3BlockFn30StoM b30(b43);
	REQUIRE(b30.GetStationAddress() == 0x38);
	REQUIRE(b30.IsMasterToStationMessage() == false);
	REQUIRE(b30.GetFunctionCode() == 30);
	REQUIRE(b30.IsEndOfMessageBlock() == true);
	REQUIRE(b30.IsFormattedBlock() == true);
	REQUIRE(b30.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn52")
{
	// New style no change response constructor - contains a digital sequence number (6)
	MD3BlockFn52MtoS b52mtos(0x25);
	REQUIRE(b52mtos.IsMasterToStationMessage() == true);
	REQUIRE(b52mtos.GetStationAddress() == 0x25);
	REQUIRE(b52mtos.GetFunctionCode() == 52);
	REQUIRE(b52mtos.IsEndOfMessageBlock() == true);
	REQUIRE(b52mtos.CheckSumPasses());

	MD3BlockFn52StoM b52stom(b52mtos);

	REQUIRE((b52stom.GetData() & 0x00008000) != 0x00008000);
	REQUIRE((b52stom.GetData() & 0x00004000) != 0x00004000);
	b52stom.SetSystemFlags(true, true);
	REQUIRE((b52stom.GetData() & 0x00008000) == 0x00008000);
	REQUIRE((b52stom.GetData() & 0x00004000) == 0x00004000);

	REQUIRE(b52stom.IsMasterToStationMessage() == false);
	REQUIRE(b52stom.GetStationAddress() == 0x25);
	REQUIRE(b52stom.GetFunctionCode() == 52);
	REQUIRE(b52stom.GetSystemPoweredUpFlag() == true);
	REQUIRE(b52stom.GetSystemTimeIncorrectFlag() == true);
	REQUIRE(b52stom.IsEndOfMessageBlock() == true);
	REQUIRE(b52stom.CheckSumPasses());
}
#ifdef _MSC_VER
#pragma endregion
#endif
}

namespace StationTests
{
#ifdef _MSC_VER
#pragma region Station Tests
#endif

TEST_CASE("Station - BinaryEvent")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Set up a callback for the result - assume sync operation at the moment
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=,&res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// TEST EVENTS WITH DIRECT CALL
	// Test on a valid binary point
	const int ODCIndex = 1;

	bool val = true;
	auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
	event->SetPayload<EventType::Binary>(bool(val));

	MD3OSPort->Event(event, "TestHarness", pStatusCallback);

	REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set. 1 is defined

	res = CommandStatus::NOT_AUTHORIZED;

	// Test on an undefined binary point. 40 NOT defined in the config text at the top of this file.
	auto event2 = std::make_shared<EventInfo>(EventType::Binary, ODCIndex+200);
	event2->SetPayload<EventType::Binary>(std::move(val));

	MD3OSPort->Event(event2, "TestHarness", pStatusCallback);
	REQUIRE((res == CommandStatus::UNDEFINED)); // The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...
	// Wait for some period to do something?? Check that the port is open and we can connect to it?

	TestTearDown();
}
TEST_CASE("Station - AnalogUnconditionalF5")
{
	// Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
	// Checks the TCP send output for correct data and format.
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Request Analog Unconditional, Station 0x7C, Module 0x20, 16 Channels
	MD3BlockFormatted commandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Call the Event functions to set the MD3 table data to what we are expecting to get back.
	// Write to the analog registers that we are going to request the values for.
	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(4096 + ODCIndex + ODCIndex * 0x100);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc05205f1500" // Echoed block
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

	TestTearDown();
}
TEST_CASE("Station - CounterScanFn30")
{
	// Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
	// Checks the TCP send output for correct data and format.

	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);
	MD3OSPort->Enable();

	// Do the same test as analog unconditional, we should give the same response from the Counter Scan.
	MD3BlockFormatted commandblock(0x7C, true, COUNTER_SCAN, 0x20, 16, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Call the Event functions to set the MD3 table data to what we are expecting to get back.
	// Write to the analog registers that we are going to request the values for.
	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(4096 + ODCIndex + ODCIndex * 0x100);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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

	// Station 0x7c, Module 61 and 62 - 8 channels each.
	MD3BlockFormatted commandblock2(0x7C, true, COUNTER_SCAN, 61, 16, true);
	output << commandblock2.ToBinaryString();

	// Set the counter values to match what the analogs were set to.
	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex);
		event->SetPayload<EventType::Counter>(numeric_cast<uint16_t>(4096 + ODCIndex + ODCIndex * 0x100));

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	Response = "Not Set";

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Call the Event functions to set the MD3 table data to what we are expecting to get back.
	// Write to the analog registers that we are going to request the values for.
	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(4096 + ODCIndex + ODCIndex * 0x100);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// Request Analog Delta Scan, Station 0x7C, Module 0x20, 16 Channels
	MD3BlockFormatted commandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
		event->SetPayload<EventType::Analog>(4096 + ODCIndex + ODCIndex * 0x100 + ((ODCIndex % 2) == 0 ? 50 : -50));

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc06205f3100"
		                                                              "32ce32ce8b00"
		                                                              "320000009200"
		                                                              "00000000bf00"
		                                                              "00000000ff00");

	// Now a delta scan
	REQUIRE(Response == DesiredResult2);
	//------------------------------

	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Write to the analog registers that we are going to request the values for.

	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex);
		event->SetPayload<EventType::Binary>((ODCIndex%2)==0);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// Request Digital Unconditional (Fn 7), Station 0x7C, Module 33, 3 Modules(Channels)
	MD3BlockFormatted commandblock(0x7C, true, DIGITAL_UNCONDITIONAL_OBS, 33, 3, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Request Digital Unconditional (Fn 7), Station 0x7C, Module 34, 2 Modules( fills the Channels field)
	MD3BlockFormatted commandblock(0x7C, true, DIGITAL_DELTA_SCAN, 34, 2, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
		event->SetPayload<EventType::Binary>((ODCIndex % 2) == 0);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0822701d00"   // Return function 8, Channels == 0, so 1 block to follow.
		                                                              "7c22aaaaf900"); // Values set above

	REQUIRE(Response == DesiredResult2);

	// Now repeat the command with no changes, should get the no change response.

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e22727800"); // Digital No Change response

	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalHRERFn9")
{
	// Tests time tagged change response Fn 9
	STANDARD_TEST_SETUP();
	Json::Value portoverride;
	portoverride["NewDigitalCommands"] = static_cast<Json::UInt>(0);
	TEST_MD3OSPort(portoverride);
	MD3OSPort->Enable();

	// Request HRER List (Fn 9), Station 0x7C,  sequence # 0, max 10 events, mev = 1
	MD3BlockFn9 commandblock(0x7C, true, 0, 10,true, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// List should be empty...so get an emtpy HRER response

	REQUIRE(Response[0] == ToChar(0xfc));  // 0x7C plus 0x80 for direction
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
		event->SetPayload<EventType::Binary>((ODCIndex % 2) == 0);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	Response = "Not Set";

	// The command remains the same each time, but is consumed in the InjectCommand
	commandblock = MD3BlockFn9(0x7C, true, 2, 10, true, true);
	output << commandblock.ToBinaryString();
	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Fn9 Test - Will have a set of blocks containing 10 change records. Need to decode to test as the times will vary by run.
	// Need to write the master station decode - code for this in order to be able to check it. The message is going to change each time

	REQUIRE(Response[2] == 0x28); // Seq 3, MEV == 1
	REQUIRE(Response[3] == 10);

	REQUIRE(Response.size() == 72); // DecodeFnResponse(Response)

	// Now repeat the command to get the last 6 results

	// The command remains the same each time, but is consumed in the InjectCommand
	commandblock = MD3BlockFn9(0x7C, true, 3, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Again need a decode function
	REQUIRE(Response[2] == 0x30); // Seq 3, MEV == 0
	REQUIRE(Response[3] == 6);    // Only 6 changes left

	REQUIRE(Response.size() == 48);

	// Send the command again, but we should get an empty response. Should only be the one block.
	commandblock = MD3BlockFn9(0x7C, true, 4, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Will get all data changing this time around
	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0940006d00"); // No events, seq # = 4

	REQUIRE(Response == DesiredResult2);

	// Send the command again, we should get the previous response - tests the recovery from lost packet code.
	commandblock = MD3BlockFn9(0x7C, true, 4, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response == DesiredResult2);

	//-----------------------------------------
	// Need to test the code path where the delta between two records is > 31.999 seconds.
	// Cheat and write directly to the HRER queue
	MD3Time changedtime = MD3NowUTC();

	MD3BinaryPoint pt1(1, 34, 1, 0, TIMETAGGEDINPUT, 1, true, changedtime);
	MD3BinaryPoint pt2(2, 34, 2, 0, TIMETAGGEDINPUT, 0, true, static_cast<MD3Time>(changedtime + 32000));
	MD3OSPort->GetPointTable()->AddToDigitalEvents(pt1);
	MD3OSPort->GetPointTable()->AddToDigitalEvents(pt2);

	commandblock = MD3BlockFn9(0x7C, true,5, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response[2] == 0x58); // Seq 5, MEV == 1	 The long period between events will require another request from the master
	REQUIRE(Response[3] == 1);

	REQUIRE(Response.size() == 18); // DecodeFnResponse(Response)

	commandblock = MD3BlockFn9(0x7C, true, 6, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response[2] == 0x60); // Seq 6, MEV == 0	 The long delay will require another request from the master
	REQUIRE(Response[3] == 1);

	REQUIRE(Response.size() == 18); // DecodeFnResponse(Response)

	//---------------------
	// Test rejection of set time command - which is when MaximumEvents is set to 0
	commandblock = MD3BlockFn9(0x7C, true, 5, 0, true, true);
	output << commandblock.ToBinaryString();

	uint64_t currenttime = MD3NowUTC();
	MD3BlockData datablock(static_cast<uint32_t>(currenttime / 1000), true );
	output << datablock.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e58505100"); // Should get a command rejected response
	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalCOSScanFn10")
{
	// Tests change response Fn 10
	STANDARD_TEST_SETUP();
	Json::Value portoverride;
	portoverride["NewDigitalCommands"] = static_cast<Json::UInt>(0);
	TEST_MD3OSPort(portoverride);

	MD3OSPort->Enable();

	// Request Digital Change Only Fn 10, Station 0x7C, Module 0 scan from the first module, Modules 2 max number to return
	MD3BlockFn10 commandblock(0x7C, true, 0, 2, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });


	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0A00522000" "7c2180008200" "7c22ffffdc00");

	REQUIRE(Response == DesiredResult1);

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	auto event = std::make_shared<EventInfo>(EventType::Binary, 80);
	event->SetPayload<EventType::Binary>(std::move(false));

	MD3OSPort->Event(event, "TestHarness", pStatusCallback); // 0x21, bit 1

	// Send the command but start from module 0x22, we did not get all the blocks last time. Test the wrap around
	commandblock = MD3BlockFn10(0x7C, true, 0x25, 5, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0a25452400"
		                                                              "7c25ffff9300"
		                                                              "7c26ff00b000"
		                                                              "7c3fffffbc00"
		                                                              "7c2100008c00"
		                                                              "7c23ffffc000");

	REQUIRE(Response == DesiredResult2);

	// Send the command with 0 start module, should return a no change block.
	commandblock = MD3BlockFn10(0x7C, true, 0, 2, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e00404c00"); // Digital No Change response

	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalCOSFn11")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	auto changedtime = static_cast<MD3Time>(0x0000016338b6d4fb); // A value around June 2018

	// Request Digital COS (Fn 11), Station 0x7C, 15 tagged events, sequence #0 - used on start up to send all data, 15 modules returned

	MD3BlockData commandblock = MD3BlockFn11MtoS(0x7C, 15, 1, 15);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Will get all data changing this time around
	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0b01462800" "210080008100" "2200ffff8300" "2300ffffa200" "2500ffff8900" "2600ff00b600" "3f00ffffca00");

	REQUIRE(Response == DesiredResult1);

	//---------------------
	// No data changes so should get a no change Fn14 block
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 2, 15); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0e02406800"); // Digital No Change response for Fn 11 - different for 7,8,10

	REQUIRE(Response == DesiredResult2);

	//---------------------
	// No sequence number change, so should get the same data back as above.
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 2, 15); // Sequence number must increase - but for this test not
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response == DesiredResult2);

	//---------------------
	// Now change data in one block only
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 3, 15); // Sequence number must increase
	output << commandblock.ToBinaryString();

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	for (int ODCIndex = 0; ODCIndex < 4; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, "TestHarness", QualityFlags::ONLINE, static_cast<MD3Time>(changedtime));
		event->SetPayload<EventType::Binary>((ODCIndex % 2) == 0);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// The second block is time, and will change each run.
	// The other blocks will have the msec part of the field change.

	REQUIRE(Response.size() == 0x2a);
	std::string BlockString = Response.substr(0, 6);
	MD3BlockFn11StoM RBlock1 = MD3BlockFn11StoM(MD3BlockData(BlockString));

	REQUIRE(RBlock1.GetStationAddress() == 0x7C);
	REQUIRE(RBlock1.GetModuleCount() == 1);
	REQUIRE(RBlock1.GetTaggedEventCount() == 4);

	// Now the module that changed
	BlockString = Response.substr(6, 6);
	MD3BlockData RBlock = MD3BlockData(BlockString);
	REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
	REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset - always 0 for ModuleBlock
	REQUIRE(RBlock.GetSecondWord() == 0xafff); // 16 bits of data

	// Now a time block
	BlockString = Response.substr(12, 6);
	RBlock = MD3BlockData(BlockString);

	MD3Time timebase = static_cast<uint64_t>(RBlock.GetData()) * 1000; //MD3Time msec since Epoch.
	LOGDEBUG("Fn11 TimeDate Packet Local : " + to_LOCALtimestringfromMD3time(timebase));
	REQUIRE(timebase == 0x0000016338b6d400ULL);

	// Then 4 COS blocks.
	BlockString = Response.substr(18, 6);
	RBlock = MD3BlockData(BlockString);
	REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
	REQUIRE(RBlock.GetByte(1) == 0xfb);        //  msec offset
	REQUIRE(RBlock.GetSecondWord() == 0xffff); // 16 bits of data

	BlockString = Response.substr(24, 6);
	RBlock = MD3BlockData(BlockString);
	REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
	REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
	REQUIRE(RBlock.GetSecondWord() == 0xbfff); // 16 bits of data

	BlockString = Response.substr(30, 6);
	RBlock = MD3BlockData(BlockString);
	REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
	REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
	REQUIRE(RBlock.GetSecondWord() == 0xbfff); // 16 bits of data

	BlockString = Response.substr(36, 6);
	RBlock = MD3BlockData(BlockString);
	REQUIRE(RBlock.GetByte(0) == 0x22);        // Module address
	REQUIRE(RBlock.GetByte(1) == 0x00);        //  msec offset
	REQUIRE(RBlock.GetSecondWord() == 0xafff); // 16 bits of data

	//-----------------------------------------
	// Need to test the code path where the delta between two records is > 255 milliseconds. Also when it is more than 0xFFFF
	// Cheat and write directly to the DCOS queue

	MD3BinaryPoint pt1(1, 34, 1, 0, TIMETAGGEDINPUT, 1, true,  changedtime);
	MD3BinaryPoint pt2(2, 34, 2, 0, TIMETAGGEDINPUT, 0, true, static_cast<MD3Time>(changedtime + 256));
	MD3BinaryPoint pt3(3, 34, 3, 0, TIMETAGGEDINPUT, 1, true, static_cast<MD3Time>(changedtime + 0x20000)); // Time gap too big, will require another Master request
	MD3OSPort->GetPointTable()->AddToDigitalEvents(pt1);
	MD3OSPort->GetPointTable()->AddToDigitalEvents(pt2);
	MD3OSPort->GetPointTable()->AddToDigitalEvents(pt3);

	commandblock = MD3BlockFn11MtoS(0x7C, 15, 4, 0); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult4 = BuildHexStringFromASCIIHexString("fc0b24603f00" "5aefcc809300" "22fbafff9a00" "00012200a900" "afff0000e600");

	REQUIRE(Response == DesiredResult4); // The RSF, HRP and DCP flag value will now be valid in all tests - need to check the comparison data!

	//-----------------------------------------
	// Get the single event left in the queue
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 5, 0); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult5 = BuildHexStringFromASCIIHexString("fc0b15402d00" "5aefcd03a500" "00012243ad00" "afff0000e600");

	REQUIRE(Response == DesiredResult5);

	TestTearDown();
}
TEST_CASE("Station - DigitalUnconditionalFn12")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Request DigitalUnconditional (Fn 12), Station 0x7C,  sequence #1, up to 15 modules returned

	MD3BlockData commandblock = MD3BlockFn12MtoS(0x7C, 0x21, 1, 3);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
		event->SetPayload<EventType::Binary>((ODCIndex % 2) == 0);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	//--------------------------------
	// Send the same command and sequence number, should get the same data as before - even though we have changed it
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response == DesiredResult1);

	//--------------------------------
	commandblock = MD3BlockFn12MtoS(0x7C, 0x21, 2, 3); // Have to change the sequence number
	output << commandblock.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0b02733a00" "210080008100" "2200aaaaa600" "2300ffffe200");

	REQUIRE(Response == DesiredResult2);

	TestTearDown();
}
TEST_CASE("Station - FreezeResetFn16")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	//  Station 0x7C
	MD3BlockFn16MtoS commandblock(0x7C, true);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f01034600");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	MD3BlockFn16MtoS commandblock2(0, false); // Reset all counters on all stations
	output << commandblock2.ToBinaryString();
	Response = "Not Set";

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response =="Not Set"); // As address zero, no response expected

	TestTearDown();
}
TEST_CASE("Station - POMControlFn17")
{
	// Test that we can generate a packet set that matches a captured packet
	MD3BlockFn17MtoS testblock(0x27, 0xa5, 0);
	REQUIRE(testblock.ToString() == "2711a5000300");
	MD3BlockData sb = testblock.GenerateSecondBlock();
	REQUIRE(sb.ToString() == "585a8000d500");

	MD3BlockFn17MtoS testblock2(0x26, 0xa4, 4);
	REQUIRE(testblock2.ToString() == "2611a4040700");
	MD3BlockData sb2 = testblock2.GenerateSecondBlock();
	REQUIRE(sb2.ToString() == "595b0800c000");

	// One of the few multi-block commands
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	//  Station 0x7C
	MD3BlockFn17MtoS commandblock(0x7C, 37, 15);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock = commandblock.GenerateSecondBlock();
	output << datablock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f250f7900");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(static_cast<uint32_t>(1000), true); // Nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e255f6700");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn17MtoS commandblock2(0x7C, 36, 1); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock();
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24514100");

	REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

	TestTearDown();
}
TEST_CASE("Station - DOMControlFn19")
{
	// Test that we can generate a packet set that matches a captured packet
	MD3BlockFn19MtoS testblock(0x27, 0xa5);
	REQUIRE(testblock.ToString() == "2713a55a2000");
	MD3BlockData sb = testblock.GenerateSecondBlock(0x12);
	REQUIRE(sb.ToString() == "00121258c300");

	// From a Fn19 packet capture - but direction is wrong...so is checksuM!
	//MD3BlockData b("91130d013100");
	//REQUIRE(b.CheckSumPasses());
	// The second packet passed however!!
	MD3BlockData b2("1000600cf100");
	REQUIRE(b2.CheckSumPasses());

	// This test was written for where the outstation is simply sinking the timedate change command
	// Will have to change if passed to ODC and events handled here
	// One of the few multiblock commands
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	//  Station 0x7C
	MD3BlockFn19MtoS commandblock(0x7C, 37);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock = commandblock.GenerateSecondBlock(0x34);
	output << datablock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f25da4400");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(static_cast<uint32_t>(1000), true); // Non nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e255a4b00");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn19MtoS commandblock2(0x7C, 36); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock(0x73);
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e245b4200");

	REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

	TestTearDown();
}

TEST_CASE("Station - InputPointControlFn20")
{
	// One of the few multi-block commands - the request can be 2 or 3 blocks
	// The 3 block version is an analog set point control, which is difficult to deal with...
	// This is an example from where 32 is written out to an RTU to cause an auto reclose - in a local control routine
	// Master -> 1e140c951e0061f36a95960000200fdff600
	// RTU Response -> 9e0f0c054a00

	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();
	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	//  Station 0x7C
	uint8_t StationAddress = 0x7C;
	uint8_t ModuleAddress = 34;
	DIMControlSelectionType ControlSelection = DIMControlSelectionType::TRIP;
	uint8_t ChannelSelection = 0;
	MD3BlockFn20MtoS commandblock(StationAddress, ModuleAddress, ChannelSelection, ControlSelection);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock1 = commandblock.GenerateSecondBlock(true);
	output << datablock1.ToBinaryString();

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f22104200");

	REQUIRE(Response == DesiredResult); // OK Command

	//***** Now Test AnalogSetPoint Control on DIM
	ModuleAddress = 39;
	ControlSelection = DIMControlSelectionType::SETPOINT;
	ChannelSelection = 5;
	MD3BlockFn20MtoS commandblock2(StationAddress, ModuleAddress, ChannelSelection, ControlSelection);
	output << commandblock2.ToBinaryString();

	datablock1 = commandblock2.GenerateSecondBlock(false); // Not last block
	output << datablock1.ToBinaryString();

	MD3BlockData datablock2 = commandblock2.GenerateThirdBlock(0x20); // Control value is 32
	output << datablock2.ToBinaryString();

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0f27554600");

	REQUIRE(Response == DesiredResult2); // OK Command

	TestTearDown();
}
TEST_CASE("Station - AOMControlFn23")
{
	// One of the few multi-block commands
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	//  Station 0x7C
	MD3BlockFn23MtoS commandblock(0x7C, 39, 1);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock = commandblock.GenerateSecondBlock(0x55);
	output << datablock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f27016900");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(static_cast<uint32_t>(1000), true); // Non nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e27517700");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn23MtoS commandblock2(0x7C, 36, 1); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock(0x55);
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24514100");

	REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

	TestTearDown();
}
TEST_CASE("Station - SystemsSignOnFn40")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// System SignOn Command, Station 0 - the slave responds with the address set correctly (i.e. if originally 0, change to match the station address - where it is asked to identify itself.
	MD3BlockFn40MtoS commandblock(0);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

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
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	uint64_t currenttime = MD3NowUTC();

	// TimeChange command (Fn 43), Station 0x7C
	MD3BlockFn43MtoS commandblock(0x7C, currenttime % 1000);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock(static_cast<uint32_t>(currenttime / 1000),true);
	output << datablock.ToBinaryString();

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command

	// Now do again with a bodgy time.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(static_cast<uint32_t>(1000), true); // Nonsensical time
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(30)); // Control/Scan Rejected Command

	// Now do again with a 10 hour offset - just use the same msec as first command. Does not matter for this test.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock3(static_cast<uint32_t>(currenttime / 1000 + 10*60*60), true); // Current time + 10 hours (in seconds)
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command
	REQUIRE(MD3OSPort->GetSOEOffsetMinutes() == 10*60);

	// Now do again with a -9 hour offset - just use the same msec as first command. Does not matter for this test.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock4(static_cast<uint32_t>(currenttime / 1000 - 9 * 60 * 60), true); // Current time - 9 hours (in seconds)
	output << datablock4.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command
	REQUIRE(MD3OSPort->GetSOEOffsetMinutes() == -9 * 60);

	TestTearDown();
}
TEST_CASE("Station - ChangeTimeDateFn44")
{
	// This test was written for where the outstation is simply sinking the timedate change command
	// Will have to change if passed to ODC and events handled here
	// One of the few multiblock commands
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();
	uint64_t currenttime = MD3NowUTC();

	// TimeChange command (Fn 44), Station 0x7C
	MD3BlockFn44MtoS commandblock(0x7C, currenttime % 1000);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock(static_cast<uint32_t>(currenttime / 1000));
	output << datablock.ToBinaryString();

	int UTCOffsetMinutes = -600;
	MD3BlockData datablockofs(static_cast<uint32_t>(UTCOffsetMinutes<<16), true);
	output << datablockofs.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command


	// Now do again with a bodgy time.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(static_cast<uint32_t>(1000)); // Nonsensical time
	output << datablock2.ToBinaryString();
	output << datablockofs.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(30)); // Control/Scan Rejected Command

	// Now do again with a 10 hour offset - just use the same msec as first command. Does not matter for this test.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock3(static_cast<uint32_t>(currenttime / 1000 - 9 * 60 * 60)); // Current time + 10 hours (in seconds)
	output << datablock3.ToBinaryString();
	output << datablockofs.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command
	REQUIRE(MD3OSPort->GetSOEOffsetMinutes() == -9 * 60);

	// Now do again with a -9 hour offset - just use the same msec as first command. Does not matter for this test.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock4(static_cast<uint32_t>(currenttime / 1000 - 9 * 60 * 60)); // Current time - 9 hours (in seconds)
	output << datablock4.ToBinaryString();
	output << datablockofs.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command
	REQUIRE(MD3OSPort->GetSOEOffsetMinutes() == -9 * 60);

	TestTearDown();
}
TEST_CASE("Station - Multi-drop TCP Test")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination.
	// The Stations will be 0x7C, 0x7D
	STANDARD_TEST_SETUP();

	START_IOS(1);

	// The two MD3Ports will share a connection, only the first to enable will open it. Two different conf files.
	TEST_MD3OSPort(Json::nullValue);
	TEST_MD3OSPort2(Json::nullValue);

	MD3OSPort->Enable(); // This should open a listening port on 1000.
	MD3OSPort2->Enable();

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
	auto pSockMan = std::make_shared<TCPSocketManager<std::string>>
		                (IOS, false, "127.0.0.1", "10000",
		                ResponseCallback,
		                SocketStateHandler,
		                std::numeric_limits<size_t>::max(),
		                true,
		                250);
	pSockMan->Open();

	Wait(*IOS, 3);
	REQUIRE(socketisopen); // Should be set in a callback.

	// Send the Command - results in an async write
	//  Station 0x7C
	MD3BlockFn16MtoS commandblock(0x7C, true);
	pSockMan->Write(commandblock.ToBinaryString());

	Wait(*IOS, 1);

	//  Station 0x7D
	MD3BlockFn16MtoS commandblock2(0x7D, true);
	pSockMan->Write(commandblock2.ToBinaryString());

	Wait(*IOS, 3); // Just pause to make sure any queued work is done (events)

	MD3OSPort->Disable();
	MD3OSPort2->Disable();
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
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();
	uint64_t currenttime = MD3NowUTC();
	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);

	// Do a "Normal" analog scan command, and make sure that the RSF bit in the response is set - for each packet that carries that bit???
	MD3BlockFormatted analogcommandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
	output << analogcommandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	MD3BlockFormatted respana = MD3BlockFormatted(MD3BlockData(Response.substr(0,6)));

	REQUIRE(respana.CheckSumPasses() == true);
	REQUIRE(respana.GetRSF() == true);
	REQUIRE(respana.GetDCP() == true);  // Changed digitals available
	REQUIRE(respana.GetHRP() == false); // Time tagged digitals available

	Response = "Not Set";

	// FlagScan command (Fn 52), Station 0x7C
	MD3BlockFn52MtoS commandblock(0x7C);

	// Send the Command
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	MD3BlockFn52StoM resp = MD3BlockFn52StoM(MD3BlockData(Response));

	REQUIRE(resp.CheckSumPasses() == true);
	REQUIRE(resp.GetSystemPoweredUpFlag() == true);
	REQUIRE(resp.GetSystemTimeIncorrectFlag() == true);

	Response = "Not Set";

	// Now read again, the PUF should be false.
	// Send the Command (again)
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	MD3BlockFn52StoM resp2 = MD3BlockFn52StoM(MD3BlockData(Response));

	REQUIRE(resp2.CheckSumPasses() == true);
	REQUIRE(resp2.GetSystemPoweredUpFlag() == false);
	REQUIRE(resp2.GetSystemTimeIncorrectFlag() == true);

	Response = "Not Set";

	// Now send a time command, so the STI flag is cleared.
	MD3BlockFn43MtoS timecommandblock(0x7C, currenttime % 1000);
	output << timecommandblock.ToBinaryString();
	MD3BlockData datablock(static_cast<uint32_t>(currenttime / 1000), true);
	output << datablock.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response[0] == ToChar(0xFC));
	REQUIRE(Response[1] == ToChar(0x0F)); // OK Command

	Response = "Not Set";

	// Now scan again and check the STI flag has been cleared.
	// Send the Command (again)
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	MD3BlockFn52StoM resp3 = MD3BlockFn52StoM(MD3BlockData(Response));

	REQUIRE(resp3.CheckSumPasses() == true);
	REQUIRE(resp3.GetSystemPoweredUpFlag() == false);
	REQUIRE(resp3.GetSystemTimeIncorrectFlag() == false);

	TestTearDown();
}
#ifdef _MSC_VER
#pragma endregion
#endif
}

namespace MasterTests
{

#ifdef _MSC_VER
#pragma region Master Tests
#endif
TEST_CASE("Master - Analog")
{
	// Tests the decoding of return data in the format of Fn 5
	// We send the response to an analog unconditional command.
	// Need the Master to send the correct command first, so that what we send back is expected.
	// We need to create an OutStation port, and subscribe it to the Master Events, so that we can then see if the events are triggered.
	// We can determine this by checking the values stored in the point table.

	STANDARD_TEST_SETUP();

	TEST_MD3MAPort(Json::nullValue);

	Json::Value portoverride;
	portoverride["Port"] = static_cast<Json::UInt64>(10001);
	TEST_MD3OSPort(portoverride);

	START_IOS(1);

	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink"); // The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = std::move(MD3Message); });

	INFO("Analog Unconditional Fn5")
	{
		// Now send a request analog unconditional command - asio does not need to run to see this processed, in this test set up
		// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);

		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c05200f5200");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above.
		MD3BlockFormatted commandblock(0x7C, false, ANALOG_UNCONDITIONAL, 0x20, 16, false);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		std::string Payload = BuildHexStringFromASCIIHexString("100011018400" // Channel 0 and 1
			                                                 "12021303b700" // Channel 2 and 3 etc
			                                                 "14041505b900"
			                                                 "160617078a00"
			                                                 "18081909a500"
			                                                 "1A0A1B0B9600"
			                                                 "1C0C1D0D9800"
			                                                 "1E0E1F0Feb00");
		Payload = "stuff" + Payload + "more stuff"; // To test TCP Framing...
		output << Payload;

		// Send the Analog Unconditional command in as if came from TCP channel. This should stop a resend of the command due to timeout...
		MD3MAPort->InjectSimulatedTCPMessage(write_buffer);

		Wait(*IOS, 1);

		// To check the result, see if the points in the master point list have been changed to the correct values.
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res,hasbeenset);
		REQUIRE(res == 0x1000);
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1101);
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x1707);
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808);

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them?
		Wait(*IOS, 1);

		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x1000);

		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808);
	}

	INFO("Analog Delta Fn6")
	{
		// We need to have done an Unconditional to correctly test a delta so do following the previous test.
		// Same address and channels as above
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below. This should stop a resend of the command due to timeout...
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c06200f7600");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above.
		MD3BlockFormatted commandblock(0x7C, false, ANALOG_DELTA_SCAN, 0x20, 16, false);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Create the delta values for the 16 channels
		MD3BlockData b1(static_cast<uint8_t>(-1), 1, static_cast<uint8_t>(-127), 128, false);
		output << b1.ToBinaryString();

		MD3BlockData b2(2, static_cast<uint8_t>(-3), 20, static_cast<uint8_t>(-125), false);
		output << b2.ToBinaryString();

		MD3BlockData b3(0x0000000, false);
		output << b3.ToBinaryString();

		MD3BlockData b4(0x00000000, true); // Mark as last block.
		output << b4.ToBinaryString();

		// Send the command in as if came from TCP channel
		MD3MAPort->InjectSimulatedTCPMessage(write_buffer);
		Wait(*IOS, 1);

		// To check the result, see if the points in the master point list have been changed to the correct values.
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them?
		Wait(*IOS, 1);

		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged
	}

	INFO("Analog Delta Fn6 - No Change Response")
	{
		// We need to have done an Unconditional to correctly test a delta so do following the previous test.
		// Same address and channels as above
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c06200f7600");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above. This should stop a resend of the command due to timeout...
		MD3BlockFormatted commandblock(0x7C, false, ANALOG_NO_CHANGE_REPLY, 0x20, 16, true); // Channels etc remain the same
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Send the command in as if came from TCP channel
		MD3MAPort->InjectSimulatedTCPMessage(write_buffer);
		Wait(*IOS, 1);

		// To check the result, see if the points in the master point list have not changed?
		// Should their time stamp be updated?
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged

		// Check that the OutStation values have not been updated over ODC.
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged
	}

	INFO("Analog Unconditional Fn5 Timeout")
	{
		// Now send a request analog unconditional command
		// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c05200f5200");
		REQUIRE(Response == DesiredResponse);

		// Instead of injecting the expected response, we don't send anything, which should result in a timeout.
		// That timeout should then result in the analog value being set to 0x8000 to indicate it is invalid??

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them
		Wait(*IOS, 10);

		// To check the result, the quality of the points will be set to comms_lost - and this will result in the values being set to 0x8000 which is MD3 for something has failed.
		uint16_t res = 0;
		bool hasbeenset;
		MD3OSPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x8000);

		MD3MAPort->GetPointTable()->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x8000);
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}
TEST_CASE("Master - ODC Comms Up Send Data/Comms Down (TCP) Quality Setting")
{
	// When ODC signals that it has regained communication up the line (or is a new connection), we will resend all data through ODC.
	STANDARD_TEST_SETUP();
	TEST_MD3MAPort(Json::nullValue);
	START_IOS(1);

	MD3MAPort->Enable();

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

	MD3MAPort->Event(event,"TestHarness", pStatusCallback);

	Wait(*IOS, 2);

	REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set. 1 is defined

	// If we loose communication down the line (TCP) we then set all data to COMMS-LOST quality through ODC so the ports
	// connected know that data is now not valid. As the MD3 slave maintains its own copy of data, to respond to polls, this is important.
	//TODO: Comms up/Down test not complete

	STOP_IOS();
	TestTearDown();
}
TEST_CASE("Master - DOM and POM Tests")
{
	STANDARD_TEST_SETUP();

	Json::Value MAportoverride;
	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001);
	OSportoverride["StandAloneOutstation"] = false;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink");
	MD3OSPort->Subscribe(MD3MAPort.get(), "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = std::move(MD3Message); });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = std::move(MD3Message); });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("DOM ODC->Master Command Test")
	{
		// So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct MD3 command,
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
		MD3MAPort->Event(event, "TestHarness", pStatusCallback);

		Wait(*IOS, 2);

		// Need to check that the hooked tcp output function got the data we expected.
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1325da2700" "fffffe03ed00")); // DOM Command

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM commandblock(resp); // Changes a few things in the block...
		MAoutput << commandblock.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 2);

		REQUIRE(res == CommandStatus::SUCCESS);
	}

	INFO("POM ODC->Master Command Test")
	{
		// So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct MD3 command,
		// and then also when we send the correct response we get an ODC::success message.

		CommandStatus res = CommandStatus::NOT_AUTHORIZED;
		auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
			{
				res = command_stat;
			});

		uint16_t ODCIndex = 116;

		EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
		val.functionCode = ControlCode::PULSE_ON;

		auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
		event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

		// Send an ODC DigitalOutput command to the Master.
		MD3MAPort->Event(event, "TestHarness", pStatusCallback);

		Wait(*IOS, 2);

		// Need to check that the hooked tcp output function got the data we expected.
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126003b00" "03d98000cc00")); // POM Command

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM commandblock(resp); // Changes a few things in the block...
		MAoutput << commandblock.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 2);

		REQUIRE(res == CommandStatus::SUCCESS);
	}

	INFO("DOM OutStation->ODC->Master Command Test")
	{
		// We want to send a DOM Command to the OutStation, have it convert that to (up to) 16   Events.
		// The Master will then ask for a response to those events (all 16!!), which we have to give it, as simulated TCP.
		// Each should be responded to with an OK packet, and its callback executed.

		IOS->post([&]()
			{
				MD3BlockFn19MtoS commandblock(0x7C, 33);                           // DOM Module is 33 - only 1 point defined, so should only have one DOM command generated.
				MD3BlockData datablock = commandblock.GenerateSecondBlock(0x8000); // Bit 0 ON? Top byte ON, bottom byte OFF

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(*IOS, 1);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1321de0300" "80008003c300")); // Should be 1 DOM command.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		OSResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 4);

		REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f21de6000")); // OK Command
	}


	INFO("POM OutStation->ODC->Master Command Test")
	{
		// We want to send a POM Command to the OutStation, it is a single bit and event, so easy compared to DOM
		// It should responded with an OK packet, and its callback executed.

		OSResponse = "Not Set";
		MAResponse = "Not Set";

		IOS->post([&]()
			{
				MD3BlockFn17MtoS commandblock(0x7C, 38, 0); // POM Module is 38, 116 to 123 Idx
				MD3BlockData datablock = commandblock.GenerateSecondBlock();

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(*IOS, 1);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126003b00" "03d98000cc00")); // Should be 1 POM command.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		// The response should then flow through ODC, back to the OutStation who should then send the OK out on TCP.
		Wait(*IOS, 5);

		REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f26006000")); // OK Command
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}

TEST_CASE("Master - TimeDate Poll Tests")
{
	STANDARD_TEST_SETUP();

	Json::Value MAportoverride;
	MAportoverride["TimeSetPoint"]["Index"] = 60000; // Set up the magic port to match
	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001);
	OSportoverride["StandAloneOutstation"] = false;
	OSportoverride["TimeSetPoint"]["Index"] = 60000;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink");
	MD3OSPort->Subscribe(MD3MAPort.get(), "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = std::move(MD3Message); });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = std::move(MD3Message); });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);


	INFO("Time Set Poll Command")
	{
		// The config file has the timeset poll as group 2.
		MD3MAPort->DoPoll(3);

		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		// The value for the time will always be different...
		REQUIRE(MAResponse[0] == 0x7c);
		REQUIRE(MAResponse[1] == 0x2b);
		REQUIRE(MAResponse.size() == 12);

		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM commandblock(resp); // Changes a few things in the block...

		MAoutput << commandblock.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 2);

		// Check there is no resend of the command - we must have got an OK packet.
		REQUIRE(MAResponse == "Not Set");
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}
TEST_CASE("Master - Digital Fn11 Command Test")
{
	STANDARD_TEST_SETUP();

	Json::Value MAportoverride;

	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001);
	OSportoverride["StandAloneOutstation"] = false;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink");
	MD3OSPort->Subscribe(MD3MAPort.get(), "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	MD3MAPort->EnablePolling(false); // Don't want the timer triggering this. We will call manually.

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = std::move(MD3Message); });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = std::move(MD3Message); });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("Test actual returned data for DCOS 11")
	{
		// We have two modules in this poll group, 34 and 35, ODC points 0 to 31.
		// Will send 3 time tagged events, and data for both modules
		// 34 set to 8000, 35 to FF00
		// Timedate is msec, but need seconds
		// Then COS records, and we insert one time block to test the decoding which is only 16 bits and offsets everything...
		// So COS records are 22058000, 23100100, time extend, 2200fe00, time extend/padding
		auto changedtime = static_cast<MD3Time>(0x0000016338b6d4fb);
		MD3BlockData b[] = { MD3BlockFn11StoM(0x7C, 4, 1, 2),MD3BlockData(0x22008000),MD3BlockData(0x2300ff00), MD3BlockData(static_cast<uint32_t>(changedtime / 1000)),
			               MD3BlockData(0x22058000), MD3BlockData(0x23100100), MD3BlockData(0x00202200),MD3BlockData(0xfe000000,true) };


		for (auto bl : b)
			MAoutput << bl.ToBinaryString();

		MAResponse = "Not Set";

		// Send the command
		auto cmdblock = MD3BlockFn11MtoS(0x7C, 15, 1, 2); // Up to 15 events, sequence #1, Two modules.
		MD3MAPort->QueueMD3Command(cmdblock, nullptr);    // No callback, does not originate from ODC
		Wait(*IOS, 2);

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer); // Sends MAoutput

		Wait(*IOS, 3);

		// Check the module values made it into the point table
		bool ModuleFailed = false;
		uint16_t wordres = MD3OSPort->GetPointTable()->CollectModuleBitsIntoWord(0x22, ModuleFailed);
		REQUIRE(wordres == 0xfe00); //0x8000 Would be this value if no timetagged data was present

		wordres = MD3OSPort->GetPointTable()->CollectModuleBitsIntoWord(0x23, ModuleFailed);
		REQUIRE(wordres == 0x0100); //0xff00 Would be this value if no timetagged data was present

		// Get the list of time tagged events, and check...
		std::vector<MD3BinaryPoint> PointList = MD3OSPort->GetPointTable()->DumpTimeTaggedPointList();
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

	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001);
	OSportoverride["StandAloneOutstation"] = false;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink");
	MD3OSPort->Subscribe(MD3MAPort.get(), "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	MD3MAPort->EnablePolling(false); // Don't want the timer triggering this. We will call manually.

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = std::move(MD3Message); });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = std::move(MD3Message); });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("New Digital Poll Command")
	{
		// Poll group 1, we want to send out the poll command from the master,
		// inject a response and then look at the Masters point table to check that the data got to where it should.
		// We could also check the linked OutStation to make sure its point table was updated as well.

		MD3MAPort->DoPoll(1); // Will send an unconditional the first time on startup, or if forced. From the logging on the RTU the response is packet 11 either way

		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		// Request DigitalUnconditional (Fn 12), Station 0x7C,  sequence #1, up to 2 modules returned - that is what the RTU we are testing has
		MD3BlockData commandblock = MD3BlockFn12MtoS(0x7C, 0x22, 1, 2); // Check out methods give the same result

		REQUIRE(MAResponse[0] == commandblock.GetByte(0));
		REQUIRE(MAResponse[1] == commandblock.GetByte(1));
		REQUIRE(MAResponse[2] == static_cast<char>(commandblock.GetByte(2)));
		REQUIRE((MAResponse[3] & 0x0f) == (commandblock.GetByte(3) & 0x0f)); // Not comparing the sequence count, only the module count

		// We now inject the expected response to the command above, a DCOS block
		// Want to test this response from an actual RTU. We asked for 15 modules, so it tried to give us that. Should only ask for what we want.

		//MD3BlockData b[] = {"fc0b016f3f00","100000009a00","00001101a100","00001210bd00","00001310af00","000014108a00","000015109800","00001610ae00","00001710bc00","00001810bf00","00001910ad00","00001a109b00","00001b108900","00001c10ac00","00001d10be00","00001e10c800" };

		// We have two modules in this poll group, 34 and 35, ODC points 0 to 31.
		// Will send 3 time tagged events, and data for both modules
		// 34 set to 8000, 35 to FF00
		// Timedate is msec, but need seconds
		// Then COS records, and we insert one time block to test the decoding which is only 16 bits and offsets everything...
		// So COS records are 22058000, 23100100, time extend, 2200fe00, time extend/padding
		auto changedtime = static_cast<MD3Time>(0x0000016338b6d4fb);
		MD3BlockData b[] = {MD3BlockFn11StoM(0x7C, 4, 1, 2),MD3BlockData(0x22008000),MD3BlockData(0x2300ff00), MD3BlockData(static_cast<uint32_t>(changedtime/1000)),
			              MD3BlockData(0x22058000), MD3BlockData(0x23100100), MD3BlockData(0x00202200),MD3BlockData(0xfe000000,true)};

		for (auto bl :b)
			MAoutput << bl.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 4);

		// Check there is no resend of the command - the response must have been ok.
		REQUIRE(MAResponse == "Not Set");

		// Get the list of time tagged events, and check...
		std::vector<MD3BinaryPoint> PointList = MD3OSPort->GetPointTable()->DumpTimeTaggedPointList();
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

	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = static_cast<Json::UInt64>(10001);
	OSportoverride["StandAloneOutstation"] = false;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort.get(), "TestLink");
	MD3OSPort->Subscribe(MD3MAPort.get(), "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	MD3MAPort->EnablePolling(false); // Don't want the timer triggering this. We will call manually.

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = std::move(MD3Message); });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = std::move(MD3Message); });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("Flag Scan Poll Command")
	{
		// Poll group 5, we want to send out the poll command from the master, then check the response.

		MD3MAPort->DoPoll(5); // Expect the RTU to be in startup mode with flags set

		Wait(*IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("7c3400006100");

		REQUIRE(MAResponse == DesiredResult1);

		MAResponse = "Not Set";


		MD3BlockFn52StoM commandblock = MD3BlockFn52StoM(MD3BlockFormatted(0x7C, true, SYSTEM_FLAG_SCAN, 0x22, 1, true)); // Check out methods give the same result
		bool SPU = true;                                                                                                  // System power on flag - should trigger unconditional scans
		bool STI = true;                                                                                                  // System time invalid - should trigger a time command
		commandblock.SetSystemFlags(SPU, STI);

		MAoutput << commandblock.ToBinaryString();
		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(*IOS, 4);

		// There are a lot of things to check  here depending on what functionality is enabled.
		// A time response, all digital scans, all analog scans...

		// We should get a time set command back first (little dependent on scheduling?
		REQUIRE(MAResponse[0] == 0x7c);
		REQUIRE(MAResponse[1] == 0x2b);
		REQUIRE(MAResponse.size() == 12);

		// Catching the rest is difficult!
	}
	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}
TEST_CASE("Master - POM Multi-drop Test Using TCP")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination. The Stations will be 0x7C, 0x7D
	// Create two masters, and then see if they can share the TCP connection successfully.
	STANDARD_TEST_SETUP();
	// Outstations are as for the conf files
	TEST_MD3OSPort(Json::nullValue);
	TEST_MD3OSPort2(Json::nullValue);

	// The masters need to be TCP Clients - should be only change necessary.
	Json::Value MAportoverride;
	MAportoverride["TCPClientServer"] = "CLIENT";
	TEST_MD3MAPort(MAportoverride);

	Json::Value MAportoverride2;
	MAportoverride2["TCPClientServer"] = "CLIENT";
	TEST_MD3MAPort2(MAportoverride2);

	START_IOS(1);

	MD3OSPort->Enable();
	MD3OSPort2->Enable();
	MD3MAPort->Enable();
	MD3MAPort2->Enable();

	// Allow everything to get setup.
	Wait(*IOS, 3);

	// So to do this test, we are going to send an Event into the Master which will require it to send a POM command to the outstation.
	// We should then have an Event triggered on the outstation caused by the POM. We need to capture this to check that it was the correct POM Event.

	// Send a POM command by injecting an ODC event to the Master
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on POM command result : " + std::to_string(static_cast<int>(command_stat)));
			res = command_stat;
		});

	bool point_on = true;
	uint16_t ODCIndex = 116;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort->Event(event, "TestHarness", pStatusCallback);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 3);

	REQUIRE(res == CommandStatus::SUCCESS);

	// Now do the other Master/Outstation combination.
	CommandStatus res2 = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback2 = std::make_shared<std::function<void(CommandStatus)>>([=, &res2](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on POM command result : " + std::to_string(static_cast<int>(command_stat)));
			res2 = command_stat;
		});

	ODCIndex = 16;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val2;
	val2.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event2 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event2->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val2));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort2->Event(event2, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 3);

	REQUIRE(res2 == CommandStatus::SUCCESS);

	MD3OSPort->Disable();
	MD3OSPort2->Disable();

	MD3MAPort->Disable();
	MD3MAPort2->Disable();

	STOP_IOS();
	TestTearDown();
}

TEST_CASE("Master - Multi-drop Disable/Enable Single Port Test Using TCP")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination. The Stations will be 0x7C, 0x7D
	// Create two masters, two stations, and then see if they can share the TCP connection successfully.
	STANDARD_TEST_SETUP();
	// Outstations are as for the conf files
	TEST_MD3OSPort(Json::nullValue);
	TEST_MD3OSPort2(Json::nullValue);

	// The masters need to be TCP Clients - should be only change necessary.
	Json::Value MAportoverride;
	MAportoverride["TCPClientServer"] = "CLIENT";
	TEST_MD3MAPort(MAportoverride);

	Json::Value MAportoverride2;
	MAportoverride2["TCPClientServer"] = "CLIENT";
	TEST_MD3MAPort2(MAportoverride2);


	START_IOS(1);

	MD3OSPort->Enable();
	MD3OSPort2->Enable();
	MD3MAPort->Enable();
	MD3MAPort2->Enable();

	// Allow everything to get setup.
	Wait(*IOS, 2);

	// So to do this test, we are going to send an Event into the Master which will require it to send a POM command to the outstation.
	// We should then have an Event triggered on the outstation caused by the POM. We need to capture this to check that it was the correct POM Event.

	// Send a POM command by injecting an ODC event to the Master
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on CONTROL command result : {}", std::to_string(static_cast<int>(command_stat)));
			res = command_stat;
		});

	bool point_on = true;
	uint16_t ODCIndex = 116;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event1 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event1->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort->Event(event1, "TestHarness", pStatusCallback);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 5);

	REQUIRE(res == CommandStatus::SUCCESS);

	// Now do the other Master/Outstation combination.
	CommandStatus res2 = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback2 = std::make_shared<std::function<void(CommandStatus)>>([=, &res2](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on CONTROL command result : {}", std::to_string(static_cast<int>(command_stat)));
			res2 = command_stat;
		});

	ODCIndex = 16;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val2;
	val2.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event2 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event2->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val2));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort2->Event(event2, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 3);

	REQUIRE(res2 == CommandStatus::SUCCESS);

	// Now Disable on port, and check that the other still works.
	MD3OSPort2->Disable();
	res = CommandStatus::NON_PARTICIPATING;
	Wait(*IOS, 2);

	// Do the POM control again
	ODCIndex = 116;
	EventTypePayload<EventType::ControlRelayOutputBlock>::type val3;
	val3.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event3 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event3->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val3));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort->Event(event3, "TestHarness", pStatusCallback);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 5);

	REQUIRE(res == CommandStatus::SUCCESS);

	// Now check that the disabled port does not work!
	res2 = CommandStatus::NON_PARTICIPATING;
	Wait(*IOS, 2);

	ODCIndex = 16;

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val4;
	val4.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event4 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event4->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val4));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort2->Event(event4, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 3);

	REQUIRE(res2 != CommandStatus::SUCCESS);

	//--- Reenable and then test the reenabled port
	MD3OSPort2->Enable();
	res2 = CommandStatus::NON_PARTICIPATING;
	Wait(*IOS, 2);

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val5;
	val5.functionCode = (point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF);

	auto event5 = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, "TestHarness");
	event5->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val5));

	// Send an ODC DigitalOutput command to the Master.
	MD3MAPort2->Event(event5, "TestHarness2", pStatusCallback2);

	// Wait for it to go to the OutStation and Back again
	Wait(*IOS, 3);

	REQUIRE(res2 == CommandStatus::SUCCESS);

	MD3OSPort->Disable();
	MD3OSPort2->Disable();

	MD3MAPort->Disable();
	MD3MAPort2->Disable();

	STOP_IOS();
	TestTearDown();
}
#ifdef _MSC_VER
#pragma endregion
#endif
}


namespace RTUConnectedTests
{
// Wire shark filter for this OutStation "md3 and ip.addr== 172.21.136.80"

const char *md3masterconffile = R"011(
{
	"IP" : "172.21.136.80",
	"Port" : 5001,
	"OutstationAddr" : 32,
	"TCPClientServer" : "CLIENT",	// I think we are ignoring this!
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,
	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 3000,
	"MD3CommandRetries" : 1,

	"PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary", "TimeTaggedDigital" : true },
					{"PollRate" : 60000, "ID" : 2, "PointType" : "Analog"},
					{"PollRate" : 120000, "ID" :4, "PointType" : "TimeSetCommand"},
					{"PollRate" : 180000, "ID" :5, "PointType" : "SystemFlagScan"}],

	"Binaries" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 16, "Offset" : 0, "PollGroup" : 1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 17, "Offset" : 0,  "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0, "PollGroup" : 2}],

	"BinaryControls" : [{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 192, "Offset" : 0, "PointType" : "POMOUTPUT"}]

})011";

TEST_CASE("RTU - Binary Scan TO MD3311 ON 172.21.136.80:5001 MD3 0x20")
{
	// This is not a REAL TEST, just for testing against a unit. Will always pass..

	// So we have an actual RTU connected to the network we are on, given the parameters in the config file above.
	STANDARD_TEST_SETUP();

	std::ofstream ofs("md3masterconffile.conf");
	if (!ofs) REQUIRE("Could not open md3masterconffile for writing");

	ofs << md3masterconffile;
	ofs.close();

	auto MD3MAPort = new  MD3MasterPort("MD3LiveTestMaster", "md3masterconffile.conf", Json::nullValue);
	MD3MAPort->Build();

	START_IOS(1);

	MD3MAPort->Enable();
	// actual time tagged data a00b22611900 10008fff9000 5b567bee8600 100000009a00 000210b4a600 64240000e100
	Wait(*IOS, 2); // Allow the connection to come up.
	//MD3MAPort->EnablePolling(false);	// If the connection comes up after this command, it will enable polling!!!

	// Read the current digital state.
	MD3MAPort->DoPoll(1);

	// Delta Scan up to 15 events, 2 modules. Seq # 10
	// Digital Scan Data a00b01610100 00001101e100
	//MD3BlockData commandblock = MD3BlockFn11MtoS(0x7C, 15, 10, 2); // This resulted in a time out - sequence number related - need to send 0 on start up??
	//MD3MAPort->QueueMD3Command(commandblock, nullptr);

	// Read the current analog state.
	MD3MAPort->DoPoll(2);
	Wait(*IOS, 5);

	// Do a time set command
	MD3MAPort->DoPoll(4);

	Wait(*IOS, 2);

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
	MD3MAPort->Event(event, "TestHarness", pStatusCallback);

	Wait(*IOS, 2);

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}

TEST_CASE("RTU - GetScanned MD3311 ON 172.21.8.111:5001 MD3 0x20")
{
	// This is not a REAL TEST, just for testing against a unit. Will always pass..

	// So we are pretending to be a standalone RTU given the parameters in the config file above.
	STANDARD_TEST_SETUP();

	std::ofstream ofs("md3masterconffile.conf");
	if (!ofs) REQUIRE("Could not open md3masterconffile for writing");

	ofs << md3masterconffile;
	ofs.close();

	Json::Value OSportoverride;
	OSportoverride["IP"] = "0.0.0.0"; // Bind to everything?? was 172.21.8.111
	OSportoverride["TCPClientServer"]= "SERVER";

	auto MD3OSPort = std::make_shared<MD3OutstationPort>("MD3LiveTestOutstation", "md3masterconffile.conf", OSportoverride);
	MD3OSPort->Build();

	START_IOS(1);
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	for (int ODCIndex = 0; ODCIndex < 16; ODCIndex++)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex);
		event->SetPayload<EventType::Analog>(4096 + ODCIndex + ODCIndex * 0x100);

		MD3OSPort->Event(event, "TestHarness", pStatusCallback);
	}

	MD3OSPort->Enable();


	Wait(*IOS, 2); // We just run for a period and see if we get connected and scanned.


	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}
}

#endif
