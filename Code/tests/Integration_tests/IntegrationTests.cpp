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

void PrepConfFiles(bool init);

class TestHook
{
public:
	static ConsoleUI* GetDataconConsole(std::shared_ptr<DataConcentrator> DC)
	{
		auto smart = std::static_pointer_cast<ConsoleUI>(DC->Interfaces.at("ConsoleUI-1"));
		return smart.get();
	}
	static std::unordered_map<std::string, spdlog::sink_ptr>* GetLogSinks(std::shared_ptr<DataConcentrator> DC)
	{
		return &DC->LogSinks;
	}
};

TEST_CASE(SUITE("ReloadConfig"))
{
	PrepConfFiles(true);
	std::cout<<"Constructing TheDataConcentrator"<<std::endl;
	auto TheDataConcentrator = std::make_shared<DataConcentrator>("opendatacon.conf");
	REQUIRE(TheDataConcentrator);
	std::cout<<"Building TheDataConcentrator"<<std::endl;
	TheDataConcentrator->Build();
	std::cout<<"Running TheDataConcentrator"<<std::endl;
	auto run_thread = std::thread([=](){TheDataConcentrator->Run();});

	std::cout<<"Adding 'TestHarness' logger"<<std::endl;
	AddLogger("TestHarness",*TestHook::GetLogSinks(TheDataConcentrator));
	auto log = odc::spdlog_get("TestHarness");
	REQUIRE(log);

	std::cout<<"Hooking into the console"<<std::endl;
	auto pConsole = TestHook::GetDataconConsole(TheDataConcentrator);
	REQUIRE(pConsole);

	std::string cmd = "set_loglevel console "+level_str+"\n";
	std::cout<<cmd<<std::flush;
	pConsole->trigger(cmd);

	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: no change");
	REQUIRE(TheDataConcentrator->ReloadConfig());
	//TODO: check the stream of events coming out of JSON port

	log->critical("ReloadConfig: bogus config");
	REQUIRE_FALSE(TheDataConcentrator->ReloadConfig("bogus"));
	//TODO: check the stream of events coming out of JSON port

	log->critical("ReloadConfig: dangling connector");
	REQUIRE_FALSE(TheDataConcentrator->ReloadConfig("opendatacon_dangling.conf"));
	//TODO: check the stream of events coming out of JSON port

	log->critical("ReloadConfig: changed port");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_changed_port.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: changed connector");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_changed_conn.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: removed port");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_removed_port.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: removed connector");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_removed_conn.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: removed plugin");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_removed_plugin.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: change everything");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon_change_everything.conf",1));
	//TODO: check the stream of events coming out of JSON port
	//let some event flow for a while
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log->critical("ReloadConfig: reset");
	REQUIRE(TheDataConcentrator->ReloadConfig("opendatacon.conf",1));
	//TODO: check the stream of events coming out of JSON port
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	log.reset();

	cmd = "shutdown\n";
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

	REQUIRE(TheDataConcentrator->isShutDown());
	if(auto log = odc::spdlog_get("opendatacon"))
		log->critical("Shutdown cleanly");
	//time for async log write
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	run_thread.join();
	std::cout<<"Run thread joined"<<std::endl;

	TheDataConcentrator.reset();
	if(auto log = odc::spdlog_get("opendatacon"))
		log->critical("Destroyed");
	//time for async log write
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	odc::spdlog_drop_all();
	odc::spdlog_shutdown();
	std::cout<<"spdlog has been shutdown"<<std::endl;

	PrepConfFiles(false);
	std::cout<<"Temp files deleted"<<std::endl;
}
