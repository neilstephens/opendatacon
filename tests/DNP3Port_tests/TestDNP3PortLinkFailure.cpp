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

#include "TestDNP3Helpers.h"
#include "../PortLoader.h"
#include "../ManInTheMiddle.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "DNP3PortLinkFailureTestSuite - " name

using port_pair_t = std::pair<std::shared_ptr<DataPort>,std::shared_ptr<DataPort>>;

const unsigned int link_ka_period = 100;
const unsigned int test_timeout = 20000;

inline port_pair_t PortPair(void* portlib, size_t os_addr, size_t ms_addr = 0, MITMConfig direction = MITMConfig::CLIENT_SERVER, unsigned int ms_port = 20000, unsigned int os_port = 20000)
{
	//fetch the function pointers from the lib for new and del for ports
	newptr newOutstation = GetPortCreator(portlib, "DNP3Outstation");
	REQUIRE(newOutstation);
	delptr delOutstation = GetPortDestroyer(portlib, "DNP3Outstation");
	REQUIRE(delOutstation);
	newptr newMaster = GetPortCreator(portlib, "DNP3Master");
	REQUIRE(newMaster);
	delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
	REQUIRE(delMaster);

	//config overrides
	Json::Value conf;
	conf["MasterAddr"] = ms_addr;
	conf["OutstationAddr"] = os_addr;
	conf["ServerType"] = "PERSISTENT";
	conf["LinkKeepAlivems"] = link_ka_period;
	conf["LinkTimeoutms"] = link_ka_period >> 1;
	//man in the middle config: first half opposite of MS, second half opposite of OS
	conf["TCPClientServer"] = (direction == MITMConfig::SERVER_CLIENT
	                           || direction == MITMConfig::CLIENT_CLIENT)
	                          ? "DEFAULT" : "CLIENT";
	conf["Port"] = os_port;

	//make an outstation port
	auto OPUT = std::shared_ptr<DataPort>(newOutstation("Outstation"+std::to_string(os_addr), "", conf), delOutstation);
	REQUIRE(OPUT);

	conf["TCPClientServer"] = (direction == MITMConfig::SERVER_CLIENT
	                           || direction == MITMConfig::SERVER_SERVER)
	                          ? "DEFAULT" : "SERVER";
	conf["Port"] = ms_port;

	//make a master port
	auto MPUT = std::shared_ptr<DataPort>(newMaster("Master"+std::to_string(ms_addr), "", conf), delMaster);
	REQUIRE(MPUT);

	//get them to build themselves using their configs
	OPUT->Build();
	MPUT->Build();

	return port_pair_t(OPUT,MPUT);
}

inline void require_link_up(std::shared_ptr<DataPort> pPort)
{
	unsigned int count = 0;
	while(pPort->GetStatus()["Result"].asString() == "Port enabled - link down" && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(pPort->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");

	//wait to actually recieve something
	count = 0;
	while(pPort->GetStatistics()["transport"]["numTransportRx"].asUInt() == 0 && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	REQUIRE(pPort->GetStatistics()["transport"]["numTransportRx"].asUInt() > 0);
}

inline void require_link_down(std::shared_ptr<DataPort> pPort)
{
	unsigned int count = 0;
	std::string new_status;
	while((new_status = pPort->GetStatus()["Result"].asString()) == "Port enabled - link up (unreset)" && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}

	REQUIRE(new_status == "Port enabled - link down");
}

inline void require_connection_increase(std::shared_ptr<ManInTheMiddle> pMITM, bool dir, unsigned int start_conns)
{
	unsigned int count = 0;
	while(pMITM->ConnectionCount(dir) == start_conns && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	REQUIRE(pMITM->ConnectionCount(dir) > start_conns);
}

TEST_CASE(SUITE("Single Drop"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		const unsigned int os_port = 20000;
		const unsigned int ms_port = 20001;
		const MITMConfig conn_dirs[4] = {MITMConfig::CLIENT_SERVER,MITMConfig::SERVER_CLIENT,MITMConfig::CLIENT_CLIENT,MITMConfig::SERVER_SERVER};

		for(auto conn_dir : conn_dirs)
		{
			//Make back-to-back TCP sockets so we can drop data and count re-connects
			auto pMITM = std::make_shared<ManInTheMiddle>(conn_dir,ms_port,os_port);

			//create an outstation/master pair, and enable them
			port_pair_t port_pair = PortPair(portlib,1,0,conn_dir,ms_port,os_port);
			port_pair.first->Enable();
			port_pair.second->Enable();

			//wait for them to connect through the man-in-the-middle
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);

			//take a baseline for the number of connections
			auto start_open1 = pMITM->ConnectionCount(true);
			auto start_open2 = pMITM->ConnectionCount(false);

			pMITM->Drop();
			//data is being dropped now, so the links should go down,
			//and reconnects should happen because it's single-drop
			require_link_down(port_pair.first);
			require_link_down(port_pair.second);

			pMITM->Allow();
			//wait another couple of keepalive periods just in case
			std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period*2));
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);

			//make sure there were reconnects
			require_connection_increase(pMITM,true,start_open1);
			require_connection_increase(pMITM,false,start_open2);
		}

		work.reset();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Multi Drop"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		const unsigned int port1 = 20000;
		const unsigned int port2 = 20001;

		//Make back-to-back TCP sockets so we can drop data and count re-connects
		auto pMITM = std::make_shared<ManInTheMiddle>(MITMConfig::SERVER_CLIENT,port1,port2);

		//create outstation/master pairs, and enable them
		std::vector<port_pair_t> port_pairs;
		for(size_t os_addr : {1,2,3})
			port_pairs.push_back(PortPair(portlib,os_addr,0,MITMConfig::SERVER_CLIENT,port1,port2));
		for(auto port_pair : port_pairs)
		{
			port_pair.first->Enable();
			port_pair.second->Enable();
		}

		odc::spdlog_get("DNP3Port")->debug("Initial links up test.");
		for(auto port_pair : port_pairs)
		{
			//wait for them to connect through the man-in-the-middle
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);
		}

		//take a baseline for the number of connections
		auto start_open1 = pMITM->ConnectionCount(true);
		auto start_open2 = pMITM->ConnectionCount(false);
		odc::spdlog_get("DNP3Port")->debug("Initial connection count: {},{}",start_open1,start_open2);

		//just disabling one or two ports shouldn't affect the others
		port_pairs[0].first->Disable();
		odc::spdlog_get("DNP3Port")->debug("One down.");
		require_link_down(port_pairs[0].second);
		//wait another keepalive periods to check - just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		for(size_t i : {1,2})
		{
			require_link_up(port_pairs[i].first);
			require_link_up(port_pairs[i].second);
		}
		port_pairs[1].second->Disable();
		odc::spdlog_get("DNP3Port")->debug("Two down.");
		require_link_down(port_pairs[1].first);
		//wait another keepalive periods to check - just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		require_link_up(port_pairs[2].first);
		require_link_up(port_pairs[2].second);

		port_pairs[0].first->Enable();
		port_pairs[1].second->Enable();
		odc::spdlog_get("DNP3Port")->debug("All back.");

		//wait another keepalive periods to check - just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		for(auto port_pair : port_pairs)
		{
			//wait for them to connect through the man-in-the-middle
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);
		}

		//make sure there were no reconnects
		REQUIRE(pMITM->ConnectionCount(true) == start_open1);
		REQUIRE(pMITM->ConnectionCount(false) == start_open2);

		pMITM->Drop();
		odc::spdlog_get("DNP3Port")->debug("Dropping.");
		//data is being dropped now, so all the links should go down,
		//and reconnects should happen because they're all down
		for(auto port_pair : port_pairs)
		{
			require_link_down(port_pair.first);
			require_link_down(port_pair.second);
		}

		pMITM->Allow();
		odc::spdlog_get("DNP3Port")->debug("Allowing.");
		//wait another couple of keepalive periods just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period*2));
		for(auto port_pair : port_pairs)
		{
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);
		}

		//make sure there were reconnects
		require_connection_increase(pMITM,true,start_open1);
		require_connection_increase(pMITM,false,start_open2);

		for(auto port_pair : port_pairs)
		{
			port_pair.first->Disable();
			port_pair.second->Disable();
		}

		//wait another keepalive periods just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		pMITM.reset();

		work.reset();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}


