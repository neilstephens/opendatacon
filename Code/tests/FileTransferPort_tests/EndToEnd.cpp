/*	opendatacon
 *
 *	Copyright (c) 2014:
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
/**
 */
#include "Helpers.h"
#include "../PortLoader.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>
#include <map>

#define SUITE(name) "FileTransferPortEndToEndTestSuite - " name

const unsigned int test_timeout = 60000;

TEST_CASE(SUITE("DirectBack2Back"))
{
	TestSetup();
	ClearFiles();
	MakeFiles();

	auto portlib = LoadModule(GetLibFileName("FileTransferPort"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		newptr newPort = GetPortCreator(portlib, "FileTransfer");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, "FileTransfer");
		REQUIRE(deletePort);

		//make a TX port
		auto TXconf = GetConfigJSON(true);
		std::shared_ptr<DataPort> TX(newPort("TX", "", TXconf), deletePort);
		REQUIRE(TX);

		//make an RX port
		auto RXconf = GetConfigJSON(false);
		std::shared_ptr<DataPort> RX(newPort("RX", "", RXconf), deletePort);
		REQUIRE(RX);

		//connect them directly
		RX->Subscribe(TX.get(),"TX");
		TX->Subscribe(RX.get(),"RX");

		//get them to build themselves using their configs
		RX->Build();
		TX->Build();

		//turn them on
		RX->Enable();
		TX->Enable();

		size_t count = 0;
		auto stats = RX->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 6 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = RX->GetStatistics();
		}
		auto stat_string = stats.toStyledString();
		CAPTURE(stat_string);

		//logging time, and files are locked under windows, so make sure writing is finished
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		CHECK(stats["FilesTransferred"].asUInt() == 6);
		std::map<std::string,std::string> file_pairs;
		file_pairs["FileTxTest1.bin"] = "RX/dateformat_FileTxTest1.bin";
		file_pairs["FileTxTest2.bin"] = "RX/dateformat_FileTxTest2.bin";
		file_pairs["sub/FileTxTest3.bin"] = "RX/dateformat_FileTxTest3.bin";
		file_pairs["sub/FileTxTest4.bin"] = "RX/dateformat_FileTxTest4.bin";
		file_pairs["sub/sub/FileTxTest5.bin"] = "RX/dateformat_FileTxTest5.bin";
		file_pairs["sub/sub/FileTxTest6.bin"] = "RX/dateformat_FileTxTest6.bin";
		for(auto [tx_filename, rx_filename] : file_pairs)
		{
			CAPTURE(tx_filename,rx_filename);
			std::ifstream tx_fin(tx_filename,std::ios::binary), rx_fin(rx_filename,std::ios::binary);
			CHECK_FALSE(tx_fin.fail());
			CHECK_FALSE(rx_fin.fail());
			char txch,rxch;
			while(tx_fin.get(txch) && !rx_fin.fail())
			{
				CHECK(rx_fin.get(rxch));
				CHECK(rxch == txch);
			}
		}

		//turn them off
		RX->Disable();
		TX->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
	ClearFiles();
}

TEST_CASE(SUITE("CorruptConnector"))
{
	TestSetup();
	ClearFiles();
	MakeFiles();

	auto portlib = LoadModule(GetLibFileName("FileTransferPort"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		newptr newPort = GetPortCreator(portlib, "FileTransfer");
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, "FileTransfer");
		REQUIRE(deletePort);

		//make a TX port

		auto TXconf = GetConfigJSON(true);
		//Turn on guards against corruption
		TXconf["UseConfirms"] = true;
		TXconf["ConfirmControlIndex"] = 10;
		TXconf["UseCRCs"]= true;
		TXconf["TransferTimeoutms"] = 350;
		std::shared_ptr<DataPort> TX(newPort("TX", "", TXconf), deletePort);
		REQUIRE(TX);

		//make an RX port

		auto RXconf = GetConfigJSON(false);
		//Turn on guards against corruption
		RXconf["UseConfirms"] = true;
		RXconf["ConfirmControlIndex"] = 10;
		RXconf["UseCRCs"]= true;
		RXconf["TransferTimeoutms"] = 300;
		std::shared_ptr<DataPort> RX(newPort("RX", "", RXconf), deletePort);
		REQUIRE(RX);

		//connect them using our nasty connector replacement
		CorruptConnector nasty(TX,RX);

		//get them to build themselves using their configs
		RX->Build();
		TX->Build();

		//turn them on
		RX->Enable();
		TX->Enable();

		size_t count = 0;
		auto stats = RX->GetStatistics();
		while(stats["FilesTransferred"].asUInt() < 6 && count < test_timeout)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count += 10;
			stats = RX->GetStatistics();
		}
		auto stat_string = stats.toStyledString();
		CAPTURE(stat_string);

		//logging time, and files are locked under windows, so make sure writing is finished
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		CHECK(stats["FilesTransferred"].asUInt() == 6);
		std::map<std::string,std::string> file_pairs;
		file_pairs["FileTxTest1.bin"] = "RX/dateformat_FileTxTest1.bin";
		file_pairs["FileTxTest2.bin"] = "RX/dateformat_FileTxTest2.bin";
		file_pairs["sub/FileTxTest3.bin"] = "RX/dateformat_FileTxTest3.bin";
		file_pairs["sub/FileTxTest4.bin"] = "RX/dateformat_FileTxTest4.bin";
		file_pairs["sub/sub/FileTxTest5.bin"] = "RX/dateformat_FileTxTest5.bin";
		file_pairs["sub/sub/FileTxTest6.bin"] = "RX/dateformat_FileTxTest6.bin";
		for(auto [tx_filename, rx_filename] : file_pairs)
		{
			CAPTURE(tx_filename,rx_filename);
			std::ifstream tx_fin(tx_filename,std::ios::binary), rx_fin(rx_filename,std::ios::binary);
			CHECK_FALSE(tx_fin.fail());
			CHECK_FALSE(rx_fin.fail());
			char txch,rxch;
			while(tx_fin.get(txch) && !rx_fin.fail())
			{
				CHECK(rx_fin.get(rxch));
				CHECK(rxch == txch);
			}
		}

		//turn them off
		RX->Disable();
		TX->Disable();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
	ClearFiles();
}


