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
/*
 * IntegrationTests.cpp
 *
 *  Created on: 29/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "IntegrationTests.h"
#include "../Code/Plugins/ConsoleUI/ConsoleUI.h"
#include "../../../opendatacon/DataConcentrator.h"
#include <catch.hpp>

#define SUITE(name) "IntegrationTestSuite - " name

void PrepReloadConfFiles(bool init);
void PrepTransferFiles(bool init);

class TestHook
{
public:
	static ConsoleUI* GetDataconConsole(std::shared_ptr<DataConcentrator> DC)
	{
		auto smart = std::static_pointer_cast<ConsoleUI>(DC->Interfaces.at("ConsoleUI-1"));
		return smart.get();
	}
	static DataPort* GetDataconPort(std::shared_ptr<DataConcentrator> DC, const std::string& Name)
	{
		auto smart = std::static_pointer_cast<DataPort>(DC->DataPorts.at(Name));
		return smart.get();
	}
	static std::unordered_map<std::string, spdlog::sink_ptr>* GetLogSinks(std::shared_ptr<DataConcentrator> DC)
	{
		return &DC->LogSinks;
	}
};

using DataconHandles = std::tuple<std::shared_ptr<DataConcentrator>,std::shared_ptr<std::thread>,std::shared_ptr<spdlog::logger>,ConsoleUI*>;

DataconHandles StartupDatacon(std::string config_filename)
{
	INFO("Constructing TheDataConcentrator");
	auto TheDataConcentrator = std::make_shared<DataConcentrator>(config_filename);
	REQUIRE(TheDataConcentrator);
	INFO("Building TheDataConcentrator");
	TheDataConcentrator->Build();
	INFO("Running TheDataConcentrator");
	auto run_thread = std::make_shared<std::thread>([=](){TheDataConcentrator->Run();});

	INFO("Adding 'TestHarness' logger");
	AddLogger("TestHarness",*TestHook::GetLogSinks(TheDataConcentrator));
	auto log = odc::spdlog_get("TestHarness");
	REQUIRE(log);

	INFO("Hooking into the console");
	auto pConsole = TestHook::GetDataconConsole(TheDataConcentrator);
	REQUIRE(pConsole);

	std::string cmd = "set_loglevel console "+level_str+"\n";
	std::cout<<cmd<<std::flush;
	pConsole->trigger(cmd);

	return {TheDataConcentrator,run_thread,log,pConsole};
}

void ShutdownDatacon(DataconHandles& handles)
{
	auto& [TheDataConcentrator,run_thread,log,pConsole] = handles;
	log.reset();

	std::string cmd = "shutdown\n";
	std::cout<<cmd<<std::flush;
	pConsole->trigger(cmd);

	//Shutting down - give some time for clean shutdown
	unsigned int i=0; std::atomic_bool wakeup_called = false;
	while(!TheDataConcentrator->isShutDown() && i++ < 150)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if(i == 140)
			odc::asio_service::Get()->post([&wakeup_called]
				{
					if(auto log = odc::spdlog_get("opendatacon"))
						log->critical("10s waiting on shutdown. Posted this message as asio wake-up call.");
					wakeup_called = true;
				});
	}
	//time for async log write
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	CHECK(TheDataConcentrator->isShutDown());

	std::cout<<"Cleanup time"<<std::endl;
	if(TheDataConcentrator->isShutDown())
	{
		run_thread->join();
		std::cout<<"Run thread joined"<<std::endl;
	}
	else
	{
		run_thread->detach();
		std::cout<<"Run thread detached"<<std::endl;
	}

	TheDataConcentrator.reset();
	std::cout<<"TheDataConcentrator has been destroyed"<<std::endl;

	odc::spdlog_drop_all();
	odc::spdlog_shutdown();
	std::cout<<"spdlog has been shutdown"<<std::endl;
}

TEST_CASE(SUITE("FileTransfer"))
{
	PrepTransferFiles(true);
	auto handles = StartupDatacon("transfer.conf");
	auto& [TheDataConcentrator,run_thread,log,pConsole] = handles;
	auto RxPort = TestHook::GetDataconPort(TheDataConcentrator,"FileTransferRX");

	size_t count = 0;
	auto stats = RxPort->GetStatistics();
	while(stats["FilesTransferred"].asUInt() < 4 && count < 20000)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		count += 10;
		stats = RxPort->GetStatistics();
	}

	auto stat_string = stats.toStyledString();
	CAPTURE(stat_string);
	CHECK(stats["FilesTransferred"].asUInt() == 4);

	const std::array<std::string,4> transfer_files =
	{
		"transfer.conf",
		"transfer1.dat",
		"transfer2.dat",
		"transfer3.dat"
	};
	for(auto& fn : transfer_files)
	{
		CAPTURE(fn);
		std::ifstream tx_fin(fn), rx_fin("RX/"+fn);
		CHECK_FALSE(tx_fin.fail());
		CHECK_FALSE(rx_fin.fail());
		char txch = 0, rxch = 0;
		size_t byte_count = 0;
		while(tx_fin.get(txch) && !rx_fin.fail() && rxch == txch)
		{
			CAPTURE(byte_count);
			CHECK(rx_fin.get(rxch));
			CHECK(rxch == txch);
			byte_count++;
		}
	}

	ShutdownDatacon(handles);

	PrepTransferFiles(false);
	std::cout<<"Temp files deleted"<<std::endl;
}

TEST_CASE(SUITE("ReloadConfig"))
{
	PrepReloadConfFiles(true);
	auto handles = StartupDatacon("opendatacon.conf");
	auto& [TheDataConcentrator,run_thread,log,pConsole] = handles;

	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: no change");
	CHECK(TheDataConcentrator->ReloadConfig());
	//TODO: check the stream of events coming out of JSON port

	INFO("ReloadConfig: bogus config");
	CHECK_FALSE(TheDataConcentrator->ReloadConfig("bogus"));
	//TODO: check the stream of events coming out of JSON port

	INFO("ReloadConfig: dangling connector");
	CHECK_FALSE(TheDataConcentrator->ReloadConfig("opendatacon_dangling.conf"));
	//TODO: check the stream of events coming out of JSON port

	INFO("ReloadConfig: changed port");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_changed_port.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: reset");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: changed connector");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_changed_conn.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: reset");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: removed port");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_removed_port.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: reset");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: removed connector");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_removed_conn.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: reset");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: removed plugin");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_removed_plugin.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: reset");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	INFO("ReloadConfig: change everything");
	CHECK(TheDataConcentrator->ReloadConfig("opendatacon_change_everything.conf",2));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	ShutdownDatacon(handles);

	PrepReloadConfFiles(false);
	std::cout<<"Temp files deleted"<<std::endl;
}
