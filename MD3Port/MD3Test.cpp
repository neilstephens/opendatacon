#include <array>
#include "catchvs.hpp"
#include "Trompeloeil.hpp"
#include <opendnp3/LogLevels.h>
#include <asiodnp3/ConsoleLogger.h>
#include <asiopal/UTCTimeSource.h>
#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"
#include "MD3Engine.h"

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
	"OutstationAddr" : 124,
	"ServerType" : "PERSISTENT",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	"Binaries" : [{"Index": 100,  "Module" : 33, "Offset" : 0, "TimeTagged" : false}, {"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "TimeTagged" : true}, {"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "TimeTagged" : true}],
	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0}],
	"BinaryControls" : [{"Range" : {"Start" : 1, "Stop" : 4}, "Module" : 35, "Offset" : 0}]

})001";

namespace UnitTests
{
	// Write out the conf file information about into a file so that it can be read back in by the code.
	void WriteConfFileToCurrentWorkingDirectory()
	{
		std::ofstream ofs(conffilename);
		if (!ofs) REQUIRE("Could not open conffile for writing");

		ofs << conffile;
		ofs.close();
	}
	std::string Response;

	TEST_CASE(SUITE("MD3CRCTest"))
	{
		/* Sample full packet, 5 blocks
		c4 : 06 : 40  :0f : 0b : 00 :
		00 : 00 : ff : fe :	90 : 00 :
		fe : fe : 00 : 00 : 9c : 00 :
		00 : 00 : 00 : 00 : bf : 00 :
		00 : 00 : 00 : 00 : ff : 00
		*/

		// The first 4 are all formatted first and only packets
		uint32_t res = MD3CRC(0x7C05200F);
		REQUIRE(MD3CRCCompare(res, 0x52));

		res = MD3CRC(0x910d400f );
		REQUIRE(MD3CRCCompare(res,0x76));

		res = MD3CRC(0xaa0d160f);
		REQUIRE(MD3CRCCompare(res,0x62));

		res = MD3CRC(0x8d0d200f);
		REQUIRE(MD3CRCCompare(res,0x77));

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

	TEST_CASE(SUITE("MD3BlockClassConstructor1Test"))
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

		MD3BlockArray msg = { 0x7C,0x05,0x20,0x0F,0x52, 0x00 };	// From a packet capture

		MD3FormattedBlock b = MD3DataBlock::MD3DataBlock(msg);

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
	TEST_CASE(SUITE("MD3BlockClassConstructor2Test"))
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

		MD3FormattedBlock b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
		b = MD3FormattedBlock::MD3FormattedBlock(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

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
	TEST_CASE(SUITE("MD3BlockClassConstructor3Test"))
	{
		uint16_t firstword = 12;
		uint16_t secondword = 32000;
		bool lastblock = true;

		MD3DataBlock b = MD3DataBlock::MD3DataBlock(firstword, secondword, lastblock);

		REQUIRE(b.GetFirstWord() == firstword);
		REQUIRE(b.GetSecondWord() == secondword);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());

		firstword = 3200;
		secondword = 16000;
		lastblock = false;

		b = MD3DataBlock::MD3DataBlock(firstword, secondword, lastblock);

		REQUIRE(b.GetFirstWord() == firstword);
		REQUIRE(b.GetSecondWord() == secondword);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("MD3BlockClassConstructor4Test"))
	{
		uint32_t data = 128364324;
		bool lastblock = true;

		MD3DataBlock b = MD3DataBlock::MD3DataBlock(data, lastblock);

		REQUIRE(b.GetData() == data);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());

		data = 8364324;
		lastblock = false;

		b = MD3DataBlock::MD3DataBlock(data, lastblock);

		REQUIRE(b.GetData() == data);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("MD3BlockClassConstructor5Test"))
	{
		uint32_t data = 0x32F1F203;
		bool lastblock = true;

		MD3DataBlock b = MD3DataBlock::MD3DataBlock(0x32,0xF1,0xf2,0x03, lastblock);

		REQUIRE(b.GetData() == data);
		REQUIRE(b.GetByte(0) == 0x32);
		REQUIRE(b.GetByte(1) == 0xF1);
		REQUIRE(b.GetByte(2) == 0xF2);
		REQUIRE(b.GetByte(3) == 0x03);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("MD3BlockSpecialClassTest"))
	{
		MD3BlockFn9 b9(0x20, true, 3, 100, false, true);

		REQUIRE(b9.GetStationAddress() == 0x20);
		REQUIRE(b9.IsMasterToStationMessage() == true);
		REQUIRE(b9.GetMoreEventsFlag() == false);
		REQUIRE(b9.GetSequenceNumber() == 3);
		REQUIRE(b9.GetEventCount() == 100);
		REQUIRE(b9.IsEndOfMessageBlock() == true);

		b9.SetEventCountandMoreEventsFlag(130, true);

		REQUIRE(b9.GetStationAddress() == 0x20);
		REQUIRE(b9.IsMasterToStationMessage() == true);
		REQUIRE(b9.GetMoreEventsFlag() == true);
		REQUIRE(b9.GetSequenceNumber() == 3);
		REQUIRE(b9.GetEventCount() == 130);
		REQUIRE(b9.IsEndOfMessageBlock() == true);

		MD3BlockFn11MtoS b11(0x32, 15, 3,14, true);
		REQUIRE(b11.GetStationAddress() == 0x32);
		REQUIRE(b11.IsMasterToStationMessage() == true);
		REQUIRE(b11.GetDigitalSequenceNumber() == 3);
		REQUIRE(b11.GetTaggedEventCount() == 15);
		REQUIRE(b11.GetModuleCount() == 14);
		REQUIRE(b11.IsEndOfMessageBlock() == true);

		MD3BlockFn11StoM b11a(0x32, 14, 4,13, true);
		REQUIRE(b11a.GetStationAddress() == 0x32);
		REQUIRE(b11a.IsMasterToStationMessage() == false);
		REQUIRE(b11a.GetDigitalSequenceNumber() == 4);
		REQUIRE(b11a.GetTaggedEventCount() == 14);
		REQUIRE(b11a.GetModuleCount() == 13);
		REQUIRE(b11a.IsEndOfMessageBlock() == true);

 		MD3BlockFn12MtoS b12(0x32, 70, 7, 15, true);
		REQUIRE(b12.GetStationAddress() == 0x32);
		REQUIRE(b12.IsMasterToStationMessage() == true);
		REQUIRE(b12.GetDigitalSequenceNumber() == 7);
		REQUIRE(b12.GetStartingModuleAddress() == 70);
		REQUIRE(b12.GetModuleCount() == 15);

		// New style no change response constructor - contains a digital sequence number (6)
		MD3BlockFn14StoM b14(0x25, 6);
		REQUIRE(b14.IsMasterToStationMessage() == false);
		REQUIRE(b14.GetStationAddress() == 0x25);
		REQUIRE(b14.GetDigitalSequenceNumber() == 6);
		REQUIRE(b14.GetData() == 0xa50e0600);
		REQUIRE(b14.IsEndOfMessageBlock() == true);

		// Old style no change response constructor - contains a module address (0x70) and # of modules 0x0E
		MD3BlockFn14StoM b14a(0x25, 0x70, (uint8_t)0x0E);
		REQUIRE(b14a.IsMasterToStationMessage() == false);
		REQUIRE(b14a.GetStationAddress() == 0x25);
		REQUIRE(b14a.GetModuleAddress() == 0x70);
		REQUIRE(b14a.GetModuleCount() == 0x0E);
		REQUIRE(b14a.GetData() == 0xa50e700e);
		REQUIRE(b14a.IsEndOfMessageBlock() == true);
	}

	TEST_CASE(SUITE("AnalogUnconditionalTest1"))
	{
		// Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
		// Checks the TCP send output for correct data and format.

		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Write to the analog registers that we are going to request the values for.
		for (int i = 0; i < 16; i++)
		{
			const odc::Analog a(4096+i+i*0x100);
			auto res = MD3Port->Event(a, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// Request Analog Unconditional, Station 0x7C, Module 0x20, 16 Channels
		std::array<uint8_t, 6> MD3data{ 0x7C, 0x05, 0x20, 0x0F, 0x52, 0x00 };
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		for (const auto& b : MD3data)
			output << b;

		// Call the Event functions to set the MD3 table data to what we are expecting to get back.

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the Analog Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult = { (char)0xfc,0x05,0x20,0x0f,0x0d,0x00,	// Echoed block
								0x10,0x00,0x11,0x01,(char)0x84,0x00,			// Channel 0 and 1
								0x12,0x02,0x13,0x03,(char)0xb7,0x00,		// Channel 2 and 3 etc
								0x14,0x04,0x15,0x05,(char)0xb9,0x00,
								0x16,0x06,0x17,0x07,(char)0x8a,0x00,
								0x18,0x08,0x19,0x09,(char)0xa5,0x00,
								0x1A,0x0A,0x1B,0x0B,(char)0x96,0x00,
								0x1C,0x0C,0x1D,0x0D,(char)0x98,0x00,
								0x1E,0x0E,0x1F,0x0F,(char)0xeb,0x00 };

		// No need to delay to process result, all done in the ReadCompletionHandler at call time.
		REQUIRE(Response == DesiredResult);

		IOMgr.Shutdown();
	}
	TEST_CASE(SUITE("AnalogDeltaScanTest1"))
	{
		// Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
		// Checks the TCP send output for correct data and format.

		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Call the Event functions to set the MD3 table data to what we are expecting to get back.
		// Write to the analog registers that we are going to request the values for.
		for (int i = 0; i < 16; i++)
		{
			const odc::Analog a(4096 + i + i * 0x100);
			auto res = MD3Port->Event(a, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// Request Analog Delta Scan, Station 0x7C, Module 0x20, 16 Channels
		MD3FormattedBlock commandblock(0x7C, true, ANALOG_DELTA_SCAN, 0x20, 16, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult1 = { (char)0xfc,0x05,0x20,0x0f,0x0d,0x00,	// Echoed block
			0x10,0x00,0x11,0x01,(char)0x84,0x00,			// Channel 0 and 1
			0x12,0x02,0x13,0x03,(char)0xb7,0x00,		// Channel 2 and 3 etc
			0x14,0x04,0x15,0x05,(char)0xb9,0x00,
			0x16,0x06,0x17,0x07,(char)0x8a,0x00,
			0x18,0x08,0x19,0x09,(char)0xa5,0x00,
			0x1A,0x0A,0x1B,0x0B,(char)0x96,0x00,
			0x1C,0x0C,0x1D,0x0D,(char)0x98,0x00,
			0x1E,0x0E,0x1F,0x0F,(char)0xeb,0x00 };

		// We should get an identical response to an analog unconditonal here
		REQUIRE(Response == DesiredResult1);
		//------------------------------

		output << commandblock.ToBinaryString();

		// Make changes to 5 channels
		for (int i = 0; i < 5; i++)
		{
			const odc::Analog a(4096 + i + i * 0x100 + ((i % 2)==0?50:-50));	// +/- 50 either side of original value
			auto res = MD3Port->Event(a, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// Send the command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult2 = { (char)0xfc,0x06,0x20,0x0f,0x29,0x00,
			0x32,(char)0xce,0x32,(char)0xce,(char)0x8b,0x00,
			0x32,0x00,0x00,0x00,(char)0x92,0x00,
			0x00,0x00,0x00,0x00,(char)0xbf,0x00,
			0x00,0x00,0x00,0x00,(char)0xff,0x00 };

		// Now a delta scan
		REQUIRE(Response == DesiredResult2);
		//------------------------------

		output << commandblock.ToBinaryString();

		// Send the command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult3 = { (char)0xfc,0x0d,0x20,0x0f,0x40,0x00 };

		// Now no changes so should get analog no change response.
		REQUIRE(Response == DesiredResult3);

		IOMgr.Shutdown();
	}

	TEST_CASE(SUITE("DigitalUnconditionalTest1"))
	{
		// Tests triggering events to set the Outstation data points, then sends an Analog Unconditional command in as if from TCP.
		// Checks the TCP send output for correct data and format.

		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Write to the analog registers that we are going to request the values for.
		for (int i = 0; i < 16; i++)
		{
			const odc::Binary b((i%2) == 0);
			auto res = MD3Port->Event(b, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// Request Digital Unconditional (Fn 7), Station 0x7C, Module 33, 3 Modules(Channels)
		MD3FormattedBlock commandblock(0x7C, true, DIGITAL_UNCONDITIONAL_OBS, 33, 3, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Call the Event functions to set the MD3 table data to what we are expecting to get back.

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult = { (char)0xfc,0x07,0x21,0x02,0x3e,0x00,
											0x7c,0x00,0x00,0x21,(char)0x86,0x00,				// Error block - missing values?
											0x7c,0x22,(char)0xaa,(char)0xaa,(char)0xb9,0x00,	// We set these values
											0x7c,0x23,(char)0xff,(char)0xff,(char)0xc0,0x00};	// Default on values.

		// No need to delay to process result, all done in the ReadCompletionHandler at call time.
		REQUIRE(Response == DesiredResult);

		IOMgr.Shutdown();
	}
	TEST_CASE(SUITE("DigitalChangeOnlyFn8Test1"))
	{
		// Tests time tagged change response Fn 8
		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Request Digital Unconditional (Fn 7), Station 0x7C, Module 34, 2 Modules( fills the Channels field)
		MD3FormattedBlock commandblock(0x7C, true, DIGITAL_DELTA_SCAN, 34, 2, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult1 = { (char)0xfc,0x07,0x22,0x01,0x25,0x00,
			0x7c,0x22,(char)0xff,(char)0xff,(char)0x9c,0x00,		// All on
			0x7c,0x23,(char)0xff,(char)0xff,(char)0xc0,0x00 };		// All on

		REQUIRE(Response == DesiredResult1);

		// Write to the first module, but not the second. Should get only the first module results sent.
		for (int i = 0; i < 16; i++)
		{
			const odc::Binary b((i % 2) == 0);
			auto res = MD3Port->Event(b, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// The command remains the same each time, but is consumed in the readcompletionhandler
		output << commandblock.ToBinaryString();
		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult2 = { (char)0xfc,0x08,0x22,0x00,0x3c,0x00,				// Return function 8, Channels == 0, so 1 block to follow.
											0x7c,0x22,(char)0xaa,(char)0xaa,(char)0xf9,0x00 };	// Default on values.

		REQUIRE(Response == DesiredResult2);

		// Now repeat the command with no changes, should get the no change response.

		// The command remains the same each time, but is consumed in the readcompletionhandler
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult3 = { (char)0xfc,0x0e,0x22,0x02,0x59,0x00 };	// Digital No Change response

		REQUIRE(Response == DesiredResult3);

		IOMgr.Shutdown();
	}
	TEST_CASE(SUITE("DigitalHRERFn9Test1"))
	{
		// Tests time tagged change response Fn 9

		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Request HRER List (Fn 9), Station 0x7C,  sequence # 0, max 10 events, mev = 1
		MD3BlockFn9 commandblock(0x7C, true, 0, 10,true, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		// List should be empty...
		const std::string DesiredResult1 = { (char)0xfc,0x09,0x00,0x00,0x6a,0x00 };		// Empty HRER response?

		REQUIRE(Response[0] == DesiredResult1[0]);
		REQUIRE(Response[1] == DesiredResult1[1]);
		REQUIRE((Response[2] & 0xF0) == 0x10);	// Top 4 bits are the sequence number - will be 1
		REQUIRE((Response[2] & 0x08) == 0);	// Bit 3 is the MEV flag
		REQUIRE(Response[3] == 0);

		// Write to the first module all 16 bits
		for (int i = 0; i < 16; i++)
		{
			const odc::Binary b((i % 2) == 0);
			auto res = MD3Port->Event(b, i, "TestHarness");

			std::this_thread::sleep_for(std::chrono::milliseconds(500));

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// The command remains the same each time, but is consumed in the readcompletionhandler
		commandblock = MD3BlockFn9(0x7C, true, 2, 10, true, true);
		output << commandblock.ToBinaryString();
		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		// Will have a set of blocks containing 10 change records. Need to decode to test as the times will vary by run.
		// Need to write the master station decode - code for this in order to be able to check it. The message is going to change each time

		REQUIRE(Response[2] == 0x28);	// Seq 3, MEV == 1
		REQUIRE(Response[3] == 10);

		REQUIRE(Response.size() == 72);	// DecodeFnResponse(Response)


		// Now repeat the command to get the last 6 results

		// The command remains the same each time, but is consumed in the readcompletionhandler
		commandblock = MD3BlockFn9(0x7C, true, 3, 10, true, true);
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		// Again need a decode function
		REQUIRE(Response[2] == 0x30);	// Seq 3, MEV == 0
		REQUIRE(Response[3] == 6);		// Only 6 changes left

		REQUIRE(Response.size() == 48);

		// Send the command again, but we should get an empty response. Should only be the one block.
		commandblock = MD3BlockFn9(0x7C, true, 4, 10, true, true);
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		// Will get all data changing this time around
		const std::string DesiredResult2 = { (char)0xfc,0x09,0x40,0x00,0x6d,0x00 };		// No events, seq # = 4

		REQUIRE(Response == DesiredResult2);

		// Send the command again, we should get the previous response - tests the recovery from lost packet code.
		commandblock = MD3BlockFn9(0x7C, true, 4, 10, true, true);
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		REQUIRE(Response == DesiredResult2);

		//-----------------------------------------
		// Need to test the code path where the delta between two records is > 31.999 seconds.
		// Cheat and write directly to the HRER queue
		uint64_t changedtime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;

		MD3Point pt1(1, 33, 1, true, 1, 1, changedtime);
		MD3Point pt2(2, 33, 2, true, 0, 1, changedtime+32000);
		MD3Port->AddToDigitalEvents(pt1);
		MD3Port->AddToDigitalEvents(pt2);

		commandblock = MD3BlockFn9(0x7C, true,5, 10, true, true);
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		REQUIRE(Response[2] == 0x58);	// Seq 5, MEV == 1	 The long delay will require another request from the master
		REQUIRE(Response[3] == 1);

		REQUIRE(Response.size() == 18);	// DecodeFnResponse(Response)

		commandblock = MD3BlockFn9(0x7C, true, 6, 10, true, true);
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		REQUIRE(Response[2] == 0x60);	// Seq 6, MEV == 0	 The long delay will require another request from the master
		REQUIRE(Response[3] == 1);

		REQUIRE(Response.size() == 18);	// DecodeFnResponse(Response)

		IOMgr.Shutdown();
	}

	TEST_CASE(SUITE("DigitalCOSScanFn10Test1"))
	{
		// Tests change response Fn 10
		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Request Digital Change Only Fn 10, Station 0x7C, Module 0 scan from the first module, Modules 2 max number to return
		MD3BlockFn10 commandblock(0x7C, true, 0, 2, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });


		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult1 = { (char)0xfc,0x0A,0x00,0x02,0x38,0x00,
											0x7c,0x21,(char)0x80,(char)0x00,(char)0x82,0x00,		// One On
											0x7c,0x22,(char)0xff,(char)0xff,(char)0xdc,0x00 };		// All on

		REQUIRE(Response == DesiredResult1);

		const odc::Binary b(false);
		auto res = MD3Port->Event(b, 100, "TestHarness");	// 0x21, bit 1

		// Send the command but start from module 0x22, we did not get all the blocks last time. Test the wrap around
		commandblock = MD3BlockFn10(0x7C, true, 0x22, 2, true);
		output << commandblock.ToBinaryString();
		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult2 = { (char)0xfc,0xa,0x22,0x02,0x32,0x00,				// Return function 10, ModuleCount =2 so 2 blocks to follow.
												0x7c,0x23,(char)0xff,(char)0xff,(char)0x80,0x00,
												0x7c,0x21,0x00,0x00,(char)0xcc,0x00 };

		REQUIRE(Response == DesiredResult2);

		// Send the command with 0 startmodule, should return a no change block.
		commandblock = MD3BlockFn10(0x7C, true, 0, 2, true);
		output << commandblock.ToBinaryString();

		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult3 = { (char)0xfc,0x0e,0x0,0x00,0x65,0x00 };	// Digital No Change response

		REQUIRE(Response == DesiredResult3);

		IOMgr.Shutdown();
	}

	TEST_CASE(SUITE("DigitalCOSFn11Test1"))
	{
		// pcap DigitalCOS
		// 0xe6, 0x0b, 0x03, 0x02, 0x11, 0x00,
		// 0x10, 0x00, 0xca, 0x00, 0x91, 0x00,
		// 0x11, 0x00, 0x17, 0xfe, 0xd5, 0x00

		// pcap DigitalCOS Station 96
		// 0xe0,0x0b, 0x0e, 0x02, 0x1d, 0x00
		// 0x06, 0x00, 0x44, 0xd2, 0xa7, 0x00
		// 0x07, 0x00, 0x00, 0x00, 0xf5, 0x00

		// pcap Set System Date and Time message
		// 0x7d, 0x2b, 0x02, 0x2b, 0x1d, 0x00
		// 0x59, 0x31, 0x75, 0x81, 0xf9, 0x00

		// Tests triggering events to set the Outstation data points, Send only changed data. Need to send all first, then make a change and then send again
		// Checks the TCP send output for correct data and format.

		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Request Digital COS (Fn 11), Station 0x7C

		MD3DataBlock commandblock = MD3BlockFn11MtoS(0x7C, 15, 1, 15, true);
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		output << commandblock.ToBinaryString();

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		// Send the Digital Uncoditional command in as if came from TCP channel
		MD3Port->ReadCompletionHandler(write_buffer);

		// Will get all data changing this time around
		const std::string DesiredResult1 = { (char)0xfc,0x0b,0x10,0x12,0x02,0x00 };		// More to come

		REQUIRE(Response == DesiredResult1);

		//---------------------
		// No data changes so should get a no change Fn14 block
		commandblock = MD3BlockFn11MtoS(0x7C, 15, 2, 15, true);	// Sequence number must increase
		output << commandblock.ToBinaryString();

		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult2 = { (char)0xfc,0x0e,0x22,0x01,0x74,0x00 };	// Digital No Change response

		REQUIRE(Response == DesiredResult2);

		//---------------------
		// No sequence number shange, so should get the same data back as above.
		commandblock = MD3BlockFn11MtoS(0x7C, 15, 2, 15, true);	// Sequence number must increase - but for this test not
		output << commandblock.ToBinaryString();

		MD3Port->ReadCompletionHandler(write_buffer);

		REQUIRE(Response == DesiredResult2);


		//---------------------
		// Now change data in one block only
		commandblock = MD3BlockFn11MtoS(0x7C, 15, 3, 15, true);	// Sequence number must increase
		output << commandblock.ToBinaryString();

		// Write to the first module, but not the second. Should get only the first module results sent.
		for (int i = 0; i < 16; i++)
		{
			const odc::Binary b((i % 2) == 0);
			auto res = MD3Port->Event(b, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set.
		}

		// The command remains the same each time, but is consumed in the readcompletionhandler
		output << commandblock.ToBinaryString();
		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult3 = { (char)0xfc,0x0e,0x22,0x01,0x74,0x00 };

		REQUIRE(Response == DesiredResult3);

		IOMgr.Shutdown();
	}
	TEST_CASE(SUITE("BinaryEventTest"))
	{
		WriteConfFileToCurrentWorkingDirectory();

		WARN("Trying to construct an OutStationPort");

		// The 1 is for concurrency hint - usually the number of cores.
		IOManager IOMgr(1);
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// TEST EVENTS WITH DIRECT CALL
		// Test on a valid binary point
		const odc::Binary b((bool)true);
		const int index = 1;
		auto res = MD3Port->Event(b, index, "TestHarness");

		REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set. 1 is defined

		// Test on an undefined binary point. 40 NOT defined in the config text at the top of this file.
		const int index2 = 200;
		auto res2 = MD3Port->Event(b, index2, "TestHarness");
		REQUIRE((res2.get() == odc::CommandStatus::UNDEFINED));	// The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...
		// Wait for some period to do something?? Check that the port is open and we can connect to it?

		IOMgr.Shutdown();
	}
}