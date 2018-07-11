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

#include <array>
#include <fstream>
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer
#include <trompeloeil.hpp>

#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/windebug_sink.h>

#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"
#include "MD3Engine.h"
#include "StrandProtectedQueue.h"
#include "ProducerConsumerQueue.h"

#define SUITE(name) "MD3Tests - " name

const char *conffilename1 = "MD3Config.conf";
const char *conffilename2 = "MD3Config2.conf";

// Serial connection string...
// std::string  JsonSerialOverride = "{ ""SerialDevice"" : "" / dev / ttyUSB0"", ""BaudRate"" : 115200, ""Parity"" : ""NONE"", ""DataBits"" : 8, ""StopBits" : 1, "MasterAddr" : 0, "OutstationAddr" : 1, "ServerType" : "PERSISTENT"}";

// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile1 = R"001(
{
	"IP" : "127.0.0.1",
	"Port" : 1000,
	"OutstationAddr" : 124,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 3000,
	"MD3CommandRetries" : 1,

	// The magic points we use to pass through MD3 commands. 0 is the default value. If set to 0, they are inactive.
	// If we set StandAloneOutStation above, and use the pass through's here, we can maintain MD3 type packets through ODC.
	// It is not however then possible to do a MD3 to DNP3 connection that will work.
	"TimeSetPoint" : {"Index" : 0},
	"SystemSignOnPoint" : {"Index" : 0},
	"FreezeResetCountersPoint"  : {"Index" : 0},
	"POMControlPoint" : {"Index" : 0},
	"DOMControlPoint" : {"Index" : 0},

	// Master only PollGroups
	// Cannot mix analog and binary points in a poll group. Group 1 is Binary, Group 2 is Analog in this example
	// You will get errors if the wrong type of points are assigned to the wrong poll group
	// We will scan the Analog and Counters to build a vector of poll group MD3 addresses
	// Same for Binaries.
	// The TimeSetCommand is used to send time set commands to the OutStation

	"PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary"},
					{"PollRate" : 20000, "ID" : 2, "PointType" : "Analog", "ForceUnconditional" : false },
					{"PollRate" : 60000, "ID" : 3, "PointType" : "TimeSetCommand"},
					{"PollRate" : 60000, "ID" : 4, "PointType" : "Binary"}],

	// We expect the binary modules to be consecutive - MD3 Addressing - (or consecutive groups for scanning), the scanning is then simpler
	"Binaries" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "PollGroup" : 1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PollGroup":1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 32, "Stop" : 47}, "Module" : 63, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0, "PollGroup" : 2}],

	"BinaryControls" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PointType" : "POMOUTPUT"}],

	"Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Module" : 61, "Offset" : 0},
					{"Range" : {"Start" : 8, "Stop" : 15}, "Module" : 62, "Offset" : 0}],

	"AnalogControls" : [{"Range" : {"Start" : 1, "Stop" : 8}, "Module" : 39, "Offset" : 0}]
})001";


// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile2 = R"002(
{
	"IP" : "127.0.0.1",
	"Port" : 1000,
	"OutstationAddr" : 125,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	// The magic Analog point we use to pass through the MD3 time set command.
	"TimeSetPoint" : {"Index" : 0},	// Not defined if 0
	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 4000,
	"MD3CommandRetries" : 1,

	"Binaries" : [{"Index": 90,  "Module" : 33, "Offset" : 0},
				{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 32, "Stop" : 47}, "Module" : 63, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0}],

	"BinaryControls" : [{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PointType" : "DOMOUTPUT"}],

	"Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Module" : 61, "Offset" : 0},{"Range" : {"Start" : 8, "Stop" : 15}, "Module" : 62, "Offset" : 0}]
})002";

#pragma region TEST_HELPERS

std::vector<spdlog::sink_ptr> LogSinks;


// Write out the conf file information about into a file so that it can be read back in by the code.
void WriteConfFilesToCurrentWorkingDirectory()
{
	std::ofstream ofs(conffilename1);
	if (!ofs) REQUIRE("Could not open conffile1 for writing");

	ofs << conffile1;
	ofs.close();

	std::ofstream ofs2(conffilename2);
	if (!ofs2) REQUIRE("Could not open conffile2 for writing");

	ofs2 << conffile2;
	ofs.close();
}

void TestSetup()
{
	// So create the log sink first - can be more than one and add to a vector.
	#ifdef WIN32
	auto console = std::make_shared<spdlog::sinks::msvc_sink_mt>(); // Windows Debug Sync - see how this goes.
	// OR wincolor_stdout_sink_mt>();
	#else
	auto console = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
	#endif
	console->set_level(spdlog::level::debug);
	LogSinks.push_back(console);

	// Then create the logger (async - as used in ODC) then connect to all sinks.
	auto pLibLogger = std::make_shared<spdlog::async_logger>("MD3Port", begin(LogSinks), end(LogSinks),
		4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
	pLibLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::async_logger>("opendatacon", begin(LogSinks), end(LogSinks),
		4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
	pODCLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(pODCLogger);

	std::string msg = "Logging for this test started..";

	if (auto log = spdlog::get("MD3Port"))
		log->info(msg);
	else
		std::cout << "Error MD3Port Logger not operational";

	if (auto log = spdlog::get("opendatacon"))
		log->info(msg);
	else
		std::cout << "Error opendatacon Logger not operational";

	WriteConfFilesToCurrentWorkingDirectory();
}
void TestTearDown()
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
		res[i] = (uint8_t)std::stol(hexpair, nullptr, 16);
	}
	return res;
}
void RunIOSForXSeconds(asio::io_service &IOS, unsigned int seconds)
{
	// We don’t have to consider the timer going out of scope in this use case.
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

#define TEST_MD3MAPort(overridejson)\
	auto MD3MAPort = new  MD3MasterPort("TestMaster", conffilename1, overridejson); \
	MD3MAPort->SetIOS(&IOS);      \
	MD3MAPort->BuildOrRebuild();

#define TEST_MD3OSPort(overridejson)      \
	auto MD3OSPort = new  MD3OutstationPort("TestOutStation", conffilename1, overridejson);   \
	MD3OSPort->SetIOS(&IOS);      \
	MD3OSPort->BuildOrRebuild();

#define TEST_MD3OSPort2(overridejson)     \
	auto MD3OSPort2 = new  MD3OutstationPort("TestOutStation2", conffilename2, overridejson); \
	MD3OSPort2->SetIOS(&IOS);     \
	MD3OSPort2->BuildOrRebuild();

#pragma endregion TEST_HELPERS

namespace SimpleUnitTests
{

TEST_CASE("Utility - HexStringTest")
{
	std::string ts = "c406400f0b00"  "0000fffe9000";
	std::string w1 = { (char)0xc4,0x06,0x40,0x0f,0x0b,0x00 };
	std::string w2 = { 0x00,0x00,(char)0xff,(char)0xfe,(char)0x90,0x00 };

	std::string res = BuildHexStringFromASCIIHexString(ts);
	REQUIRE(res == (w1 + w2));
}

TEST_CASE("Utility - MD3CRCTest")
{
	// The first 4 are all formatted first and only packets
	uint32_t res = MD3CRC(0x7C05200F);
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
	asio::io_service IOS(2);

	asio::io_service::work work(IOS); // Just to keep things from stopping..

	std::thread t1([&]() {IOS.run(); });
	std::thread t2([&]() {IOS.run(); });

	StrandProtectedQueue<int> foo(IOS,10);
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
#pragma region Block Tests

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
	bool HCP = false;
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
	REQUIRE(b.GetHCP() == HCP);
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
	bool HCP = false;
	bool DCP = false;

	MD3BlockFormatted b(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	mastertostation = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	lastblock = false;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	channels = 1;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	moduleaddress = 0xFF;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	stationaddress = 0x7F;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	stationaddress = 0x01;
	moduleaddress = 0x01;
	APL = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	RSF = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	HCP = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
	REQUIRE(b.GetDCP() == DCP);
	REQUIRE(b.CheckSumPasses());

	DCP = true;
	b = MD3BlockFormatted(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

	REQUIRE(b.GetStationAddress() == stationaddress);
	REQUIRE(b.IsMasterToStationMessage() == mastertostation);
	REQUIRE(b.GetModuleAddress() == moduleaddress);
	REQUIRE(b.GetFunctionCode() == functioncode);
	REQUIRE(b.GetChannels() == channels);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(b.IsFormattedBlock());
	REQUIRE(b.GetAPL() == APL);
	REQUIRE(b.GetRSF() == RSF);
	REQUIRE(b.GetHCP() == HCP);
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

	MD3BlockData b(0x32, 0xF1, 0xf2, 0x03, lastblock);

	REQUIRE(b.GetData() == data);
	REQUIRE(b.GetByte(0) == 0x32);
	REQUIRE(b.GetByte(1) == 0xF1);
	REQUIRE(b.GetByte(2) == 0xF2);
	REQUIRE(b.GetByte(3) == 0x03);
	REQUIRE(b.IsEndOfMessageBlock() == lastblock);
	REQUIRE(!b.IsFormattedBlock());
	REQUIRE(b.CheckSumPasses());
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
	MD3BlockFn14StoM b14a(0x25, 0x70, (uint8_t)0x0E);
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

	MD3BlockFn16MtoS b16b(0x73, false);
	REQUIRE(b16b.GetStationAddress() == 0x73);
	REQUIRE(b16b.IsEndOfMessageBlock());
	REQUIRE(b16b.IsFormattedBlock());
	REQUIRE(b16b.CheckSumPasses());
	REQUIRE(b16b.IsValid());
	REQUIRE(b16b.GetNoCounterReset() == false);
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

	MD3BlockData SecondBlock23 = b23.GenerateSecondBlock(0x55);
	REQUIRE(b23.VerifyAgainstSecondBlock(SecondBlock23));
	REQUIRE(b23.GetOutputFromSecondBlock(SecondBlock23) == 0x55);
	REQUIRE(SecondBlock23.IsEndOfMessageBlock());
	REQUIRE(!SecondBlock23.IsFormattedBlock());
	REQUIRE(SecondBlock23.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn40")
{
	MD3BlockFn40 b40(0x3F);
	REQUIRE(b40.IsValid());
	REQUIRE(b40.GetData() == 0x3f28c0d7);
	REQUIRE(b40.GetStationAddress() == 0x3F);
	REQUIRE(b40.IsEndOfMessageBlock());
	REQUIRE(b40.IsFormattedBlock());
	REQUIRE(b40.CheckSumPasses());
}
TEST_CASE("MD3Block - Fn43 Fn15 Fn30")
{
	MD3BlockFn43MtoS b43(0x38, 999);
	REQUIRE(b43.GetMilliseconds() == 999);
	REQUIRE(b43.GetStationAddress() == 0x38);
	REQUIRE(b43.IsMasterToStationMessage() == true);
	REQUIRE(b43.GetFunctionCode() == 43);
	REQUIRE(b43.IsEndOfMessageBlock() == false);
	REQUIRE(b43.IsFormattedBlock() == true);
	REQUIRE(b43.CheckSumPasses());

	MD3BlockFn15StoM b15(b43);
	REQUIRE(b15.GetStationAddress() == 0x38);
	REQUIRE(b15.IsMasterToStationMessage() == false);
	REQUIRE(b15.GetFunctionCode() == 15);
	REQUIRE(b15.IsEndOfMessageBlock() == true);
	REQUIRE(b15.IsFormattedBlock() == true);
	REQUIRE(b15.CheckSumPasses());

	MD3BlockFn30StoM b30(b43);
	REQUIRE(b30.GetStationAddress() == 0x38);
	REQUIRE(b30.IsMasterToStationMessage() == false);
	REQUIRE(b30.GetFunctionCode() == 30);
	REQUIRE(b30.IsEndOfMessageBlock() == true);
	REQUIRE(b30.IsFormattedBlock() == true);
	REQUIRE(b30.CheckSumPasses());
}

#pragma endregion
}

namespace StationTests
{
#pragma region Station Tests

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
	const odc::Binary b((bool)true);
	const int index = 1;
	MD3OSPort->Event(b, index, "TestHarness", pStatusCallback);

	REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set. 1 is defined

	res = CommandStatus::NOT_AUTHORIZED;
	// Test on an undefined binary point. 40 NOT defined in the config text at the top of this file.
	const int index2 = 200;
	MD3OSPort->Event(b, index2, "TestHarness", pStatusCallback);
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
	for (int i = 0; i < 16; i++)
	{
		const odc::Analog a(4096 + i + i * 0x100);
		MD3OSPort->Event(a, i, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc05200f0d00" // Echoed block
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

	// Do the same test as analog unconditional, we should give teh same response from the Counter Scan.
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
	for (int i = 0; i < 16; i++)
	{
		const odc::Analog a(4096 + i + i * 0x100);
		MD3OSPort->Event(a, i, "TestHarness", pStatusCallback);

		REQUIRE((res == CommandStatus::SUCCESS)); // The Get will Wait for the result to be set.
	}

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc1f200f2200" // Echoed block
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
	for (int i = 0; i < 16; i++)
	{
		const odc::Counter c(4096 + i + i * 0x100);
		MD3OSPort->Event(c, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	Response = "Not Set";

	// Send the Analog Unconditional command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1f3d0f1200" // Echoed block
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
	for (int i = 0; i < 16; i++)
	{
		const odc::Analog a(4096 + i + i * 0x100);
		MD3OSPort->Event(a, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// Request Analog Delta Scan, Station 0x7C, Module 0x20, 16 Channels
	MD3BlockFormatted commandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the command in as if came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult1 = { (char)0xfc,0x05,0x20,0x0f,0x0d,0x00, // Echoed block
		                               0x10,0x00,0x11,0x01,(char)0x84,0x00, // Channel 0 and 1
		                               0x12,0x02,0x13,0x03,(char)0xb7,0x00, // Channel 2 and 3 etc
		                               0x14,0x04,0x15,0x05,(char)0xb9,0x00,
		                               0x16,0x06,0x17,0x07,(char)0x8a,0x00,
		                               0x18,0x08,0x19,0x09,(char)0xa5,0x00,
		                               0x1A,0x0A,0x1B,0x0B,(char)0x96,0x00,
		                               0x1C,0x0C,0x1D,0x0D,(char)0x98,0x00,
		                               0x1E,0x0E,0x1F,0x0F,(char)0xeb,0x00 };

	// We should get an identical response to an analog unconditional here
	REQUIRE(Response == DesiredResult1);
	//------------------------------

	// Make changes to 5 channels
	for (int i = 0; i < 5; i++)
	{
		const odc::Analog a(4096 + i + i * 0x100 + ((i % 2)==0 ? 50 : -50)); // +/- 50 either side of original value
		MD3OSPort->Event(a, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = { (char)0xfc,0x06,0x20,0x0f,0x29,0x00,
		                               0x32,(char)0xce,0x32,(char)0xce,(char)0x8b,0x00,
		                               0x32,0x00,0x00,0x00,(char)0x92,0x00,
		                               0x00,0x00,0x00,0x00,(char)0xbf,0x00,
		                               0x00,0x00,0x00,0x00,(char)0xff,0x00 };

	// Now a delta scan
	REQUIRE(Response == DesiredResult2);
	//------------------------------

	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = { (char)0xfc,0x0d,0x20,0x0f,0x40,0x00 };

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
	for (int i = 0; i < 16; i++)
	{
		const odc::Binary b((i%2) == 0);
		MD3OSPort->Event(b, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// Request Digital Unconditional (Fn 7), Station 0x7C, Module 33, 3 Modules(Channels)
	MD3BlockFormatted commandblock(0x7C, true, DIGITAL_UNCONDITIONAL_OBS, 33, 3, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Address 21, only 1 bit, set by default - check bit order
	// Address 22, set to alternating on/off above
	// Address 23, all on by default
	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0721023e00" "7c2180008200" "7c22aaaab900" "7c23ffffc000");

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
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0722012500" "7c22ffff9c00" "7c23ffffc000"); // All on

	REQUIRE(Response == DesiredResult1);

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Write to the first module, but not the second. Should get only the first module results sent.
	for (int i = 0; i < 16; i++)
	{
		const odc::Binary b((i % 2) == 0);
		MD3OSPort->Event(b, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0822003c00"   // Return function 8, Channels == 0, so 1 block to follow.
		                                                              "7c22aaaaf900"); // Values set above

	REQUIRE(Response == DesiredResult2);

	// Now repeat the command with no changes, should get the no change response.

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e22025900"); // Digital No Change response

	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalHRERFn9")
{
	// Tests time tagged change response Fn 9
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);
	MD3OSPort->Enable();

	// Request HRER List (Fn 9), Station 0x7C,  sequence # 0, max 10 events, mev = 1
	MD3BlockFn9 commandblock(0x7C, true, 0, 10,true, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// List should be empty...
	const std::string DesiredResult1 = { (char)0xfc,0x09,0x00,0x00,0x6a,0x00 }; // Empty HRER response?

	REQUIRE(Response[0] == DesiredResult1[0]);
	REQUIRE(Response[1] == DesiredResult1[1]);
	REQUIRE((Response[2] & 0xF0) == 0x10); // Top 4 bits are the sequence number - will be 1
	REQUIRE((Response[2] & 0x08) == 0);    // Bit 3 is the MEV flag
	REQUIRE(Response[3] == 0);

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});

	// Write to the first module all 16 bits
	for (int i = 0; i < 16; i++)
	{
		const odc::Binary b((i % 2) == 0);
		MD3OSPort->Event(b, i, "TestHarness", pStatusCallback);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// The command remains the same each time, but is consumed in the InjectCommand
	commandblock = MD3BlockFn9(0x7C, true, 2, 10, true, true);
	output << commandblock.ToBinaryString();
	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	//TODO: Fn9 Test - Will have a set of blocks containing 10 change records. Need to decode to test as the times will vary by run.
	//TODO: Need to write the master station decode - code for this in order to be able to check it. The message is going to change each time

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
	MD3Time changedtime = MD3Now();

	MD3BinaryPoint pt1(1, 34, 1, 1, true, changedtime);
	MD3BinaryPoint pt2(2, 34, 2, 0, true, (MD3Time)(changedtime+32000));
	MD3OSPort->AddToDigitalEvents(pt1);
	MD3OSPort->AddToDigitalEvents(pt2);

	commandblock = MD3BlockFn9(0x7C, true,5, 10, true, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	REQUIRE(Response[2] == 0x58); // Seq 5, MEV == 1	 The long delay will require another request from the master
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

	uint64_t currenttime = MD3Now();
	MD3BlockData datablock((uint32_t)(currenttime / 1000), true );
	output << datablock.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e58004900"); // Should get a command rejected response
	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalCOSScanFn10")
{
	// Tests change response Fn 10
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Request Digital Change Only Fn 10, Station 0x7C, Module 0 scan from the first module, Modules 2 max number to return
	MD3BlockFn10 commandblock(0x7C, true, 0, 2, true);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });


	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0A00023800" "7c2180008200" "7c22ffffdc00");

	REQUIRE(Response == DesiredResult1);

	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			res = command_stat;
		});
	const odc::Binary b(false);
	MD3OSPort->Event(b, 80, "TestHarness", pStatusCallback); // 0x21, bit 1

	// Send the command but start from module 0x22, we did not get all the blocks last time. Test the wrap around
	commandblock = MD3BlockFn10(0x7C, true, 0x25, 5, true);
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0a25050d00"
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

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0e00006500"); // Digital No Change response

	REQUIRE(Response == DesiredResult3);

	TestTearDown();
}
TEST_CASE("Station - DigitalCOSFn11")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();

	// Request Digital COS (Fn 11), Station 0x7C, 15 tagged events, sequence #0 - used on start up to send all data, 15 modules returned

	MD3BlockData commandblock = MD3BlockFn11MtoS(0x7C, 15, 1, 15);
	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// Will get all data changing this time around
	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0b01060100" "210080008100" "2200ffff8300" "2300ffffa200" "2500ffff8900" "2600ff00b600" "3f00ffffca00");

	REQUIRE(Response == DesiredResult1);

	//---------------------
	// No data changes so should get a no change Fn14 block
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 2, 15); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0e02004100"); // Digital No Change response for Fn 11 - different for 7,8,10

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

	// Write to the first module 0-16 index, but not the second. Should get only the first module results sent.
	//TODO: Fn11 Test - Pass the digital change time in the Event as part of the binary object
	for (int i = 0; i < 4; i++)
	{
		const odc::Binary b((i % 2) == 0);
		MD3OSPort->Event(b, i, "TestHarness", pStatusCallback);

		REQUIRE(res == CommandStatus::SUCCESS); // The Get will Wait for the result to be set.
	}

	// The command remains the same each time, but is consumed in the InjectCommand
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// The second block is time, adn will change each run.
	// The other blocks will have the msec part of the field change.
	//TODO: Fn11 Test - Will have to cast to MD3Blocks and check the parts that we can check...
	//	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc0b03013f00" "5aebf9259c00" "800a801aa900" "80010000fc00");

	//	REQUIRE(Response.size() == 0x30);

	//-----------------------------------------
	// Need to test the code path where the delta between two records is > 255 milliseconds. Also when it is more than 0xFFFF
	// Cheat and write directly to the DCOS queue
	MD3Time changedtime = (MD3Time)0x0000016338b6d4fb; //asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;

	MD3BinaryPoint pt1(1, 34, 1, 1, true,  changedtime);
	MD3BinaryPoint pt2(2, 34, 2, 0, true, (MD3Time)(changedtime + 256));
	MD3BinaryPoint pt3(3, 34, 3, 1, true, (MD3Time)(changedtime + 0x20000)); // Time gap too big, will require another Master request
	MD3OSPort->AddToDigitalEvents(pt1);
	MD3OSPort->AddToDigitalEvents(pt2);
	MD3OSPort->AddToDigitalEvents(pt3);

	commandblock = MD3BlockFn11MtoS(0x7C, 15, 4, 0); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult4 = BuildHexStringFromASCIIHexString("fc0b04000100" "5aefcc809300" "22fbafff9a00" "00012200a900"     "afff0000e600");

	REQUIRE(Response == DesiredResult4);

	//-----------------------------------------
	// Get the single event left in the queue
	commandblock = MD3BlockFn11MtoS(0x7C, 15, 5, 0); // Sequence number must increase
	output << commandblock.ToBinaryString();
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult5 = BuildHexStringFromASCIIHexString("fc0b05001300" "5aefcd03a500" "00012243ad00" "afff0000e600");

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
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Inject command as if it came from TCP channel
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult1 = BuildHexStringFromASCIIHexString("fc0b01032d00" "210080008100" "2200ffff8300" "2300ffffe200");

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
	for (int i = 0; i < 16; i++)
	{
		const odc::Binary b((i % 2) == 0);
		MD3OSPort->Event(b, i, "TestHarness", pStatusCallback);

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

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc0b02031b00" "210080008100" "2200aaaaa600" "2300ffffe200");

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
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

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
	MD3BlockFn17MtoS commandblock(0x7C, 37, 1);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock = commandblock.GenerateSecondBlock();
	output << datablock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f25014d00");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(1000, true); // Nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e25014b00");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn17MtoS commandblock2(0x7C, 36, 1); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock();
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24015900");

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
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f25da4400");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(1000, true); // Non nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e250a5300");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn19MtoS commandblock2(0x7C, 36); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock(0x73);
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e240b5a00");

	REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

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
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult = BuildHexStringFromASCIIHexString("fc0f27016900");

	REQUIRE(Response == DesiredResult); // OK Command

	//---------------------------
	// Now do again with a bodgy second block.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(1000, true); // Non nonsensical block
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult2 = BuildHexStringFromASCIIHexString("fc1e27016f00");

	REQUIRE(Response == DesiredResult2); // Control/Scan Rejected Command

	//---------------------------
	MD3BlockFn23MtoS commandblock2(0x7C, 36, 1); // Invalid control point
	output << commandblock2.ToBinaryString();

	MD3BlockData datablock3 = commandblock.GenerateSecondBlock(0x55);
	output << datablock3.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	const std::string DesiredResult3 = BuildHexStringFromASCIIHexString("fc1e24015900");

	REQUIRE(Response == DesiredResult3); // Control/Scan Rejected Command

	TestTearDown();
}
TEST_CASE("Station - SystemsSignOnFn40")
{
	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);

	MD3OSPort->Enable();
	uint64_t currenttime = MD3Now();

	// System SignOn Command, Station 0 - the slave only responds to a zero address - where it is asked to indetify itself.
	MD3BlockFn40 commandblock(0);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

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
	uint64_t currenttime = MD3Now();

	// TimeChange command (Fn 43), Station 0x7C
	MD3BlockFn43MtoS commandblock(0x7C, currenttime % 1000);

	asio::streambuf write_buffer;
	std::ostream output(&write_buffer);
	output << commandblock.ToBinaryString();

	MD3BlockData datablock((uint32_t)(currenttime / 1000),true);
	output << datablock.ToBinaryString();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	// Send the Command
	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == (char)0xFC);
	REQUIRE(Response[1] == (char)0x0F); // OK Command

	// Now do again with a bodgy time.
	output << commandblock.ToBinaryString();
	MD3BlockData datablock2(1000, true); // Nonsensical time
	output << datablock2.ToBinaryString();

	MD3OSPort->InjectSimulatedTCPMessage(write_buffer);

	// No need to delay to process result, all done in the InjectCommand at call time.
	REQUIRE(Response[0] == (char)0xFC);
	REQUIRE(Response[1] == (char)30); // Control/Scan Rejected Command

	TestTearDown();
}

TEST_CASE("Station - Multi-drop TCP Test")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination.
	// The Stations will be 0x7C, 0x7D
	STANDARD_TEST_SETUP();

	START_IOS(1);

	// The two MD3Ports will share a connection, only the first to enable will open it.
	TEST_MD3OSPort(Json::nullValue);
	TEST_MD3OSPort2(Json::nullValue);

	MD3OSPort->Enable(); // This should open a listening port on 1000.
	MD3OSPort2->Enable();

	ProducerConsumerQueue<std::string> Response;
	auto ResponseCallback = [&](buf_t& readbuf)
					{
						int bufsize = readbuf.size();
						std::string S(bufsize, 0);

						for (int i = 0; i < bufsize; i++)
						{
							S[i] = readbuf.sgetc();
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
			true, 250));
	pSockMan->Open();

	Wait(IOS, 3);
	REQUIRE(socketisopen); // Should be set in a callback.

	// Send the Command - results in an async write
	//  Station 0x7C
	MD3BlockFn16MtoS commandblock(0x7C, true);
	pSockMan->Write(commandblock.ToBinaryString());

	//  Station 0x7D
	MD3BlockFn16MtoS commandblock2(0x7D, true);
	pSockMan->Write(commandblock2.ToBinaryString());

	Wait(IOS, 3); // Just pause to make sure any queued work is done (events)

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
#pragma endregion
}

namespace MasterTests
{

#pragma region Master Tests

TEST_CASE("Master - Analog")
{
	// Tests the decoding of return data in the format of Fn 5
	// We send the response to an analog unconditional command.
	// Need the Master to send the correct command first, so that what we send back is expected.
	// We need to create an OutStation port, and subscribe it to the Master Events, so that we can then see if the events are triggered.
	// We can determine this by checking the values stored in the point table.
	//TODO: SJE It would probably be ideal to have a TestOutStationPort and TestMasterPort that allow us to hook all events with lambdas from the test code. It would only have an ODC and Point table interface.

	STANDARD_TEST_SETUP();

	TEST_MD3MAPort(Json::nullValue);

	Json::Value portoverride;
	portoverride["Port"] = (Json::UInt64)1001;
	TEST_MD3OSPort(portoverride);

	START_IOS(1);

	MD3MAPort->Subscribe(MD3OSPort, "TestLink"); // The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output function with a lambda
	std::string Response = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&Response](std::string MD3Message) { Response = MD3Message; });

	INFO("Analog Unconditional Fn5");
	{
		// Now send a request analog unconditional command - asio does not need to run to see this processed, in this test set up
		// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);

		Wait(IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c05200f5200");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above.
		MD3BlockFormatted commandblock(0x7C, false, ANALOG_UNCONDITIONAL, 0x20, 16, false);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		const std::string Payload = BuildHexStringFromASCIIHexString("100011018400" // Channel 0 and 1
			                                                       "12021303b700" // Channel 2 and 3 etc
			                                                       "14041505b900"
			                                                       "160617078a00"
			                                                       "18081909a500"
			                                                       "1A0A1B0B9600"
			                                                       "1C0C1D0D9800"
			                                                       "1E0E1F0Feb00");
		output << Payload;

		// Send the Analog Unconditional command in as if came from TCP channel. This should stop a resend of the command due to timeout...
		MD3MAPort->InjectSimulatedTCPMessage(write_buffer);

		Wait(IOS, 1);

		// To check the result, see if the points in the master point list have been changed to the correct values.
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 0, res,hasbeenset);
		REQUIRE(res == 0x1000);
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1101);
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x1707);
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808);

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them?
		Wait(IOS, 1);

		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x1000);

		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808);
	}

	INFO("Analog Delta Fn6");
	{
		// We need to have done an Unconditional to correctly test a delta so do following the previous test.
		// Same address and channels as above
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below. This should stop a resend of the command due to timeout...
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c06200f7600");
		REQUIRE(Response == DesiredResponse);

		// We now inject the expected response to the command above.
		MD3BlockFormatted commandblock(0x7C, false, ANALOG_DELTA_SCAN, 0x20, 16, false);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Create the delta values for the 16 channels
		MD3BlockData b1(-1, 1, -127, 128, false);
		output << b1.ToBinaryString();

		MD3BlockData b2(2, -3, 20, -125, false);
		output << b2.ToBinaryString();

		MD3BlockData b3(0, 0, 0, 0, false);
		output << b3.ToBinaryString();

		MD3BlockData b4(0, 0, 0, 0, true); // Mark as last block.
		output << b4.ToBinaryString();

		// Send the command in as if came from TCP channel
		MD3MAPort->InjectSimulatedTCPMessage(write_buffer);
		Wait(IOS, 1);

		// To check the result, see if the points in the master point list have been changed to the correct values.
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them?
		Wait(IOS, 1);

		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged
	}

	INFO("Analog Delta Fn6 - No Change Response");
	{
		// We need to have done an Unconditional to correctly test a delta so do following the previous test.
		// Same address and channels as above
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(IOS, 1);

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
		Wait(IOS, 1);

		// To check the result, see if the points in the master point list have not changed?
		// Should their time stamp be updated?
		uint16_t res = 0;
		bool hasbeenset;
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged

		// Check that the OutStation values have not been updated over ODC.
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x0FFF); // -1
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 1, res, hasbeenset);
		REQUIRE(res == 0x1102); // +1
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 7, res, hasbeenset);
		REQUIRE(res == 0x168A); // 0x1707 - 125

		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 8, res, hasbeenset);
		REQUIRE(res == 0x1808); // Unchanged
	}

	INFO("Analog Unconditional Fn5 Timeout");
	{
		// Now send a request analog unconditional command
		// The analog unconditional command would normally be created by a poll event, or us receiving an ODC read analog event, which might trigger us to check for an updated value.
		MD3BlockFormatted sendcommandblock(0x7C, true, ANALOG_UNCONDITIONAL, 0x20, 16, true);
		Response = "Not Set";
		MD3MAPort->QueueMD3Command(sendcommandblock, nullptr);
		Wait(IOS, 1);

		// We check the command, but it does not go anywhere, we inject the expected response below.
		const std::string DesiredResponse = BuildHexStringFromASCIIHexString("7c05200f5200");
		REQUIRE(Response == DesiredResponse);

		// Instead of injecting the expected response, we don't send anything, which should result in a timeout.
		// That timeout should then result in the analog value being set to 0x8000 to indicate it is invalid??

		// Also need to check that the MasterPort fired off events to ODC. We do this by checking values in the OutStation point table.
		// Need to give ASIO time to process them
		Wait(IOS, 10);

		// To check the result, the quality of the points will be set to comms_lost - and this will result in the values being set to 0x8000 which is MD3 for something has failed.
		uint16_t res = 0;
		bool hasbeenset;
		MD3OSPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
		REQUIRE(res == 0x8000);

		MD3MAPort->GetAnalogValueUsingMD3Index(0x20, 0, res, hasbeenset);
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
	MD3MAPort->ConnectionEvent(odc::CONNECTED,"TestHarness", pStatusCallback);

	Wait(IOS, 2);

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
	//MAportoverride["DOMControlPoint"]["Index"] = 60000; // Set up the magic port to match
	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = (Json::UInt64)1001;
	OSportoverride["StandAloneOutstation"] = false;
	//OSportoverride["DOMControlPoint"]["Index"] = 60000;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort, "TestLink");
	MD3OSPort->Subscribe(MD3MAPort, "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = MD3Message; });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = MD3Message; });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("DOM ODC->Master Command Test");
	{
		// So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct MD3 command,
		// and then also when we send the correct response we get an ODC::success message.

		CommandStatus res = CommandStatus::NOT_AUTHORIZED;
		auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
			{
				res = command_stat;
			});

		ControlRelayOutputBlock command(ControlCode::LATCH_ON ); // Turn on...
		uint16_t index = 100;

		// Send an ODC DigitalOutput command to the Master.
		MD3MAPort->Event(command,index, "TestHarness", pStatusCallback);

		Wait(IOS, 2);

		// Need to check that the hooked tcp output function got the data we expected.
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1325da2700" "fffffe03ed00")); // DOM Command

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM commandblock(resp); // Changes a few things in the block...
		MAoutput << commandblock.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(IOS, 2);

		REQUIRE(res == CommandStatus::SUCCESS);
	}

	INFO("POM ODC->Master Command Test");
	{
		// So we want to send an ODC ControlRelayOutputBlock command to the Master through ODC, and check that it sends out the correct MD3 command,
		// and then also when we send the correct response we get an ODC::success message.

		CommandStatus res = CommandStatus::NOT_AUTHORIZED;
		auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
			{
				res = command_stat;
			});

		ControlRelayOutputBlock command(ControlCode::LATCH_ON); // Turn on...
		uint16_t index = 116;

		// Send an ODC DigitalOutput command to the Master.
		MD3MAPort->Event(command, index, "TestHarness", pStatusCallback);

		Wait(IOS, 2);

		// Need to check that the hooked tcp output function got the data we expected.
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126080e00" "03d90080cb00")); // POM Command

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM commandblock(resp); // Changes a few things in the block...
		MAoutput << commandblock.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

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
				MD3BlockFn19MtoS commandblock(0x7C, 33);                         // DOM Module is 33 - only 1 point defined, so should only have one DOM command generated.
				MD3BlockData datablock = commandblock.GenerateSecondBlock(0x80); // Bit 0 ON?

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(IOS, 1);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1321de0300" "80008003c300")); // Should be 1 DOM command.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		OSResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

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
				MD3BlockFn17MtoS commandblock(0x7C, 38, 0); // POM Module is 38, 116 to 123 Idx
				MD3BlockData datablock = commandblock.GenerateSecondBlock();

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(IOS, 1);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126080e00" "03d90080cb00")); // Should be 1 POM command.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

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
	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = (Json::UInt64)1001;
	OSportoverride["StandAloneOutstation"] = true;
	OSportoverride["DOMControlPoint"]["Index"] = 60000;
	OSportoverride["POMControlPoint"]["Index"] = 60001;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort, "TestLink");
	MD3OSPort->Subscribe(MD3MAPort, "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = MD3Message; });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = MD3Message; });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);

	INFO("DOM OutStation->ODC->Master Pass Through Command Test");
	{
		// We want to send a DOM Command to the OutStation, but pass it through ODC unchanged. Use a "magic" analog port to do this.

		IOS.post([&]()
			{
				MD3BlockFn19MtoS commandblock(0x7C, 33);                         // DOM Module is 33 - only 1 point defined, so should only have one DOM command generated.
				MD3BlockData datablock = commandblock.GenerateSecondBlock(0x80); // Bit 0 ON?

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(IOS, 2);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1321de0300" "80008003c300")); // Should passed through DOM command - match what was sent.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(IOS, 3);

		REQUIRE(OSResponse == BuildHexStringFromASCIIHexString("fc0f21de6000")); // OK Command
	}


	INFO("POM OutStation->ODC->Master Pass Through Command Test");
	{
		// We want to send a POM Command to the OutStation.
		// It should respond with an OK packet, and its callback executed.

		OSResponse = "Not Set";
		MAResponse = "Not Set";

		IOS.post([&]()
			{
				MD3BlockFn17MtoS commandblock(0x7C, 38, 0); // POM Module is 38, 116 to 123 Idx
				MD3BlockData datablock = commandblock.GenerateSecondBlock();

				OSoutput << commandblock.ToBinaryString();
				OSoutput << datablock.ToBinaryString();

				MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This one waits, but we need the code below executed..
			});

		Wait(IOS, 3);

		// So the command we started above, will eventually result in an OK packet. But have to do the Master simulated TCP first...
		REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c1126080e00" "03d90080cb00")); // Should be 1 POM command.

		// Now send the OK packet back in.
		// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
		MD3BlockData resp(MAResponse[0], MAResponse[1], MAResponse[2], MAResponse[3], true);
		MD3BlockFn15StoM rcommandblock(resp); // Changes a few things in the block...
		MAoutput << rcommandblock.ToBinaryString();

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

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
	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = (Json::UInt64)1001;
	OSportoverride["StandAloneOutstation"] = false;
	OSportoverride["TimeSetPoint"]["Index"] = 60000;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort, "TestLink");
	MD3OSPort->Subscribe(MD3MAPort, "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = MD3Message; });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = MD3Message; });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);


	INFO("Time Set Poll Command");
	{
		// The config file has the timeset poll as group 2.
		MD3MAPort->DoPoll(3);

		Wait(IOS, 1);

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

		Wait(IOS, 1);

		// Check there is no resend of the command - we must have got an OK packet.
		REQUIRE(MAResponse == "Not Set");
	}

	INFO("Time Set TCP to OutStation to Master to TCP");
	{
		// So we post time change command to the outstation, which should then go to the Master, which should then send a timechange command out on TCP.
		// If the Outstation is standalone, it will not wait for the ODC response.
		// "TimeSetPoint" : {"Index" : 100000},
		uint64_t currenttime = MD3Now(); // 0x1111222233334444;

		// TimeChange command (Fn 43), Station 0x7C
		MD3BlockFn43MtoS commandblock(0x7C, currenttime % 1000);
		MD3BlockData datablock((uint32_t)(currenttime / 1000), true);

		std::string TimeChangeCommand = commandblock.ToBinaryString() + datablock.ToBinaryString();

		OSoutput << TimeChangeCommand;

		// Send the Command - but this ends up waiting for the ODC call to return, but we need to send in what the call is expecting..
		// Need another thread dealing with the other port?

		// This will execute after InjectSimulatedTCPMessage??
		IOS.post([&]()
			{
				// Now send correct response (simulated TCP) to Master,
				// We now inject the expected response to the command above, a control OK message, using the received data of the first block as the basis
				MD3BlockFn15StoM OKcommandblock(commandblock); // Changes a few things in the block...
				MAoutput << OKcommandblock.ToBinaryString();

				Wait(IOS, 1); // Must be sent after we send the command to the outstation
				LOGDEBUG("Test - Injecting MA message as TCP Input");
				MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer); // This will also wait for completion?
			});

		LOGDEBUG("Test - Injecting OS message as TCP Input");
		MD3OSPort->InjectSimulatedTCPMessage(OSwrite_buffer); // This must be sent first
		Wait(IOS, 4);                                         // Outstation decodes message, sends ODC event, Master gets ODC event, sends out command on TCP.

		// Check the Master port output on TCP - it should be an identical time change command??
		REQUIRE(MAResponse == TimeChangeCommand);

		// Now check we have an OK packet being sent by the OutStation.
		REQUIRE(OSResponse[0] == (char)0xFC);
		REQUIRE(OSResponse[1] == (char)0x0F); // OK Command
		REQUIRE(OSResponse.size() == 6);
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}

TEST_CASE("Master - Digital Poll Tests (Old and New)")
{
	STANDARD_TEST_SETUP();

	Json::Value MAportoverride;

	TEST_MD3MAPort(MAportoverride);

	Json::Value OSportoverride;
	OSportoverride["Port"] = (Json::UInt64)1001;
	OSportoverride["StandAloneOutstation"] = false;
	TEST_MD3OSPort(OSportoverride);

	START_IOS(1);

	// The subscriber is just another port. MD3OSPort is registering to get MD3Port messages.
	// Usually is a cross subscription, where each subscribes to the other.
	MD3MAPort->Subscribe(MD3OSPort, "TestLink");
	MD3OSPort->Subscribe(MD3MAPort, "TestLink");

	MD3OSPort->Enable();
	MD3MAPort->Enable();

	MD3MAPort->EnablePolling(false); // Dont want the timer triggering this. We will call manually.

	// Hook the output functions
	std::string OSResponse = "Not Set";
	MD3OSPort->SetSendTCPDataFn([&OSResponse](std::string MD3Message) { OSResponse = MD3Message; });

	std::string MAResponse = "Not Set";
	MD3MAPort->SetSendTCPDataFn([&MAResponse](std::string MD3Message) { MAResponse = MD3Message; });

	asio::streambuf OSwrite_buffer;
	std::ostream OSoutput(&OSwrite_buffer);

	asio::streambuf MAwrite_buffer;
	std::ostream MAoutput(&MAwrite_buffer);


	INFO("New Digital Poll Command");
	{
		// Poll group 1, we want to send out the poll command from the master,
		// inject a response and then look at the Masters point table to check that the data got to where it should.
		// We could also check the linked OutStation to make sure its point table was updated as well.

		MD3MAPort->DoPoll(1); // Will send an unconditional the first time? From the logging on the RTU the response is packet 11 either way

		Wait(IOS, 1);

		// TODO: Check the result but allow the sequence number to change!

		// We check the command, but it does not go anywhere, we inject the expected response below.
		//REQUIRE(MAResponse == BuildHexStringFromASCIIHexString("7c0c22124f00"));

		// Request DigitalUnconditional (Fn 12), Station 0x7C,  sequence #1, up to 2 modules returned - that is what the RTU we are testing has
		MD3BlockData commandblock = MD3BlockFn12MtoS(0x7C, 0x22, 1, 2); // Check out methods give the same result
		//REQUIRE(MAResponse == commandblock.ToBinaryString());

		// We now inject the expected response to the command above, a DCOS block
		// Want to test this response from an actual RTU. We asked for 15 modules, so it tried to give us that. Should only ask for what we want.

		MD3BlockData b[] = {"fc0b016f3f00","100000009a00","00001101a100","00001210bd00","00001310af00","000014108a00","000015109800","00001610ae00","00001710bc00","00001810bf00","00001910ad00","00001a109b00","00001b108900","00001c10ac00","00001d10be00","00001e10c800" };

		// MD3BlockData b[] = {("fc0b01043700","210080008100","2200ffff8300","2300ffffa200","3f00ffffca00")};

		for (auto bl :b)
			MAoutput << bl.ToBinaryString();

		MAResponse = "Not Set";

		MD3MAPort->InjectSimulatedTCPMessage(MAwrite_buffer);

		Wait(IOS, 4);

		// Check there is no resend of the command - the response must have been ok.
		REQUIRE(MAResponse == "Not Set");
	}

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}

TEST_CASE("Master - Binary Scan Multi-drop Test Using TCP")
{
	// Here we test the ability to support multiple Stations on the one Port/IP Combination. The Stations will be 0x7C, 0x7D

	STANDARD_TEST_SETUP();
	TEST_MD3OSPort(Json::nullValue);
	TEST_MD3OSPort2(Json::nullValue);

	START_IOS(1);

	MD3OSPort->Enable();
	MD3OSPort2->Enable();

	std::vector<std::string> ResponseVec;
	auto ResponseCallback = [&](buf_t& readbuf)
					{
						int bufsize = readbuf.size();
						std::string S(bufsize, 0);

						for (int i = 0; i < bufsize; i++)
						{
							S[i] = readbuf.sgetc();
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
			true, 500));
	pSockMan->Open();

	Wait(IOS, 3);
	REQUIRE(socketisopen); // Should be set in a callback.

	// Send the Command - results in an async write
	//  Station 0x7C
	MD3BlockFn16MtoS commandblock(0x7C, true);
	pSockMan->Write(commandblock.ToBinaryString());

	//  Station 0x7D
	MD3BlockFn16MtoS commandblock2(0x7D, true);
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
	MD3OSPort->Disable();
	MD3OSPort2->Disable();

	STOP_IOS();
	TestTearDown();
}
#pragma endregion
}

namespace RTUConnectedTests
{
const char *md3masterconffile = R"011(
{
	"IP" : "172.21.136.80",
	"Port" : 5001,
	"OutstationAddr" : 32,
	"TCPClientServer" : "CLIENT",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 3000,
	"MD3CommandRetries" : 1,

	"PollGroups" : [{"PollRate" : 50000, "ID" : 1, "PointType" : "Binary"},
					{"PollRate" : 10000, "ID" : 2, "PointType" : "Analog"},
					{"PollRate" : 120000, "ID" :4, "PointType" : "TimeSetCommand"}],

	"Binaries" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 16, "Offset" : 0, "PollGroup" : 1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 17, "Offset" : 0,  "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0, "PollGroup" : 2}],

	"BinaryControls" : [{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 192, "Offset" : 0, "PointType" : "POMOUTPUT"}]

})011";

TEST_CASE("RTU - Binary Scan TO MD3311 ON 172.21.136.80:5001 MD3 0x20")
{
	// So we have an actual RTU connected to the network we are on, given the parameters in the config file below.
	STANDARD_TEST_SETUP();

	std::ofstream ofs("md3masterconffile.conf");
	if (!ofs) REQUIRE("Could not open md3masterconffile for writing");

	ofs << md3masterconffile;
	ofs.close();

	auto MD3MAPort = new  MD3MasterPort("MD3LiveTestMaster", "md3masterconffile.conf", Json::nullValue);
	MD3MAPort->SetIOS(&IOS);
	MD3MAPort->BuildOrRebuild();

	START_IOS(1);

	MD3MAPort->Enable();
	MD3MAPort->EnablePolling(false);

	// Got an AnalogUnconditional Response - test this.
	// a005207f2600 00000000bf00 00000000bf00 00000000bf00 00000000bf00 80008000ae00 80008000ae00 80008000ae00 80008000ee00
	// 4 VALID, VALUE ZERO, 4 NOT CONFIGURED VALUE 0x8000 THE ERROR VALUE
	// Read the current digital state. Response a00b0171300 010008000d400
	//	MD3MAPort->DoPoll(1);

	// Delta Scan up to 15 events, 2 modules. Seq # 10
	MD3BlockData commandblock = MD3BlockFn11MtoS(0x7C, 15, 10, 2); // This resulted in a time out - sequence number related - need to send 0 on start up??
	MD3MAPort->QueueMD3Command(commandblock, nullptr);

	// Read the current analog state.
//		MD3MAPort->DoPoll(2);

	Wait(IOS, 5);

	// Send a POM command by injecting an ODC event
	CommandStatus res = CommandStatus::NOT_AUTHORIZED;
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus)>>([=, &res](CommandStatus command_stat)
		{
			LOGDEBUG("Callback on POM command result : " + std::to_string((int)command_stat));
			res = command_stat;
		});

	ControlRelayOutputBlock command(ControlCode::PULSE_ON); // Turn on...
	uint16_t index = 100;

	// Send an ODC DigitalOutput command to the Master.
//		MD3MAPort->Event(command, index, "TestHarness", pStatusCallback);

	Wait(IOS, 5);

	work.reset(); // Indicate all work is finished.

	STOP_IOS();
	TestTearDown();
}

}

