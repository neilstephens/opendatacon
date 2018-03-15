#include "catchvs.hpp"
#include "Trompeloeil.hpp"
#include <opendnp3/LogLevels.h>
#include <asiodnp3/ConsoleLogger.h>
#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"

#define SUITE(name) "MD3Tests - " name

const char *conffilename = "MD3Config.conf";

// Serial connection string...
// std::string  JsonSerialOverride = "{ ""SerialDevice"" : "" / dev / ttyUSB0"", ""BaudRate"" : 115200, ""Parity"" : ""NONE"", ""DataBits"" : 8, ""StopBits" : 1, "MasterAddr" : 0, "OutstationAddr" : 1, "ServerType" : "PERSISTENT"}";

// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile = R"001(
{
	// MD3 Test Configuration - Only need to know what to pass, don't actually need to know what it is?
	// What about analog/digital? Need to know that for MD3 Module 0 has special meaning - not sure if Module == Index??

	"IP" : "127.0.0.1",
	"Port" : 20000,
	"MasterAddr" : 0,
	"OutstationAddr" : 1,
	"ServerType" : "PERSISTENT",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	"Binaries" : [{"Index": 1}, { "Index": 5 }, { "Index": 6 }, { "Index": 7 }, { "Index": 8 }, { "Index": 10 }, { "Index": 11 }, { "Index": 12 }, { "Index": 13 }, { "Index": 14 }, { "Index": 15 }],
	"Analogs" : [{"Range" : {"Start" : 1, "Stop" : 5}}],
	"BinaryControls" : [{"Range" : {"Start" : 1, "Stop" : 4}}]

})001";

namespace UnitTests
{
	// Write out the conf file information about into a file so that it can be read back in by the code.
	void WriteConfFile()
	{
		std::ofstream ofs(conffilename);
		if (!ofs) REQUIRE("Could not open conffile for writing");

		ofs << conffile;
		ofs.close();
	}

	// Have to fish out of the Packet capture some real DataBlock information to feed into this test.
	TEST_CASE(SUITE("MD3CRCTest"))
	{
		const uint32_t data = 0x0F0F0F0F;
		uint32_t res = MD3CRC(data);
		REQUIRE(res == 0x23);
	}

	TEST_CASE(SUITE("ConstructorTest"))
	{
		WriteConfFile();

		// Would normally have an IOs and IOMgr instance up and running for the Port to work with - how to manage that?

		WARN("Trying to construct an OutStationPort");
		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		//port.second->AddLogSubscriber(AdvancedLoggers.at("Console Log").get());	// Should we mock out a logger to make sure we get the messages we expect?
		//port.second->SetIOS(&IOS);
		//port.second->SetLogLevel(LOG_LEVEL);

		// TEST EVENTS WITH DIRECT CALL

		// This event will is data coming into the OutStation from the OpenDataCon connector. When the Master attached in the MD3Port TCP port requests it, we will send it the data.
		// For now all it does is store it.

		// Test on a valid binary point
		const odc::BinaryOutputStatus b((bool)true);
		const int index = 1;
		auto res = MD3Port->Event(b, index, "TestHarness");
		REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set. 1 is defined

		// Test on an undefined binary point.
		const int index2 = 2;
		auto res2 = MD3Port->Event(b, index2, "TestHarness");
		REQUIRE((res2.get() == odc::CommandStatus::UNDEFINED));	// The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...

		IOManager IOMgr(1);

		// send log messages to the console
		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance());
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		// Wait for some period to do something?? Check that the port is open and we can connect to it?

		IOMgr.Shutdown();
	}
}