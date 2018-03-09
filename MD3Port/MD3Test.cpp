#include "catchvs.hpp"
#include "Trompeloeil.hpp"
#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"

#define SUITE(name) "MD3Tests - " name

const char *conffilename = "MD3Config.conf";

// We actually have the conf file here to match the tests it is use din below. We write out (overwrite) on each test.
const char *conffile = R"001({
	// MD3 Test Configuration - Only need to know what to pass, don't actually need to know what it is?
	// What about analog/digital? Need to know that for MD3 Module 0 has special meaning - not sure if Module == Index??

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
	TEST_CASE(SUITE("ConstructorTest"))
	{
		WriteConfFile();

		// std::string JsonIpOverride = "{ "IP" : "0.0.0.0", "Port" : 30000, "MasterAddr" : 0, "OutstationAddr" : 1}";
		// std::string  JsonSerialOverride = "{ ""SerialDevice"" : "" / dev / ttyUSB0"", ""BaudRate"" : 115200, ""Parity"" : ""NONE"", ""DataBits"" : 8, ""StopBits" : 1, "MasterAddr" : 0, "OutstationAddr" : 1, "ServerType" : "PERSISTENT"}";

		WARN("Trying to construct an OutStationPort");
		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		REQUIRE((MD3Port != nullptr));
	}
}