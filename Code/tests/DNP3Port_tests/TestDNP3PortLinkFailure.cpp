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
const unsigned int test_timeout = 30000;

inline port_pair_t PortPair(module_ptr portlib, size_t os_addr, size_t ms_addr = 0, MITMConfig direction = MITMConfig::CLIENT_SERVER, unsigned int ms_port = 20000, unsigned int os_port = 20000, bool comms = false)
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
	conf["MasterAddr"] = Json::UInt(ms_addr);
	conf["OutstationAddr"] = Json::UInt(os_addr);
	conf["ServerType"] = "PERSISTENT";
	conf["LinkKeepAlivems"] = Json::UInt(link_ka_period);
	conf["LinkTimeoutms"] = Json::UInt(link_ka_period >> 1);
	conf["IPConnectRetryPeriodMinms"] = 100;
	conf["IPConnectRetryPeriodMaxms"] = 100;
	//man in the middle config: first half opposite of MS, second half opposite of OS
	conf["TCPClientServer"] = (direction == MITMConfig::SERVER_CLIENT
	                           || direction == MITMConfig::CLIENT_CLIENT)
	                          ? "DEFAULT" : "CLIENT";
	conf["Port"] = Json::UInt(os_port);

	conf["Binaries"][0]["Range"]["Start"] = 0;
	conf["Binaries"][0]["Range"]["Stop"] = comms ? 9 : 10;
	conf["Analogs"][0]["Range"]["Start"] = 0;
	conf["Analogs"][0]["Range"]["Stop"] = 9;
	conf["BinaryControls"][0]["Range"]["Start"] = 0;
	conf["BinaryControls"][0]["Range"]["Stop"] = 4;
	if(comms)
	{
		conf["CommsPoint"]["Index"] = 10;
		conf["CommsPoint"]["FailValue"] = false;
		conf["FlagsToClearOnLinkStatus"] = "";
	}
	else
		conf["SetQualityOnLinkStatus"] = false;

	conf["EnableUnsol"] = true;
	conf["UnsolClass1"] = true;
	conf["UnsolClass2"] = true;
	conf["UnsolClass3"] = true;
	conf["DoUnsolOnStartup"] = true;

	//make an outstation port
	auto OPUT = std::shared_ptr<DataPort>(newOutstation("Outstation"+std::to_string(os_addr), "", conf), delOutstation);
	REQUIRE(OPUT);

	conf["TCPClientServer"] = (direction == MITMConfig::SERVER_CLIENT
	                           || direction == MITMConfig::SERVER_SERVER)
	                          ? "DEFAULT" : "SERVER";
	conf["Port"] = Json::UInt(ms_port);

	//make a master port
	auto MPUT = std::shared_ptr<DataPort>(newMaster("Master"+std::to_string(ms_addr), "", conf), delMaster);
	REQUIRE(MPUT);

	//get them to build themselves using their configs
	OPUT->Build();
	MPUT->Build();

	return port_pair_t(OPUT,MPUT);
}

inline void require_quality(const QualityFlags& test_qual, const bool test_set, std::shared_ptr<DataPort> pPort)
{
	unsigned int count = 0;
	bool all_have_qual = false;
	std::string json_str; //use for debugging failure
	while(!all_have_qual && count < test_timeout)
	{
		all_have_qual = true;
		auto current_state = pPort->GetCurrentState();
		json_str = current_state.toStyledString();
		auto time = current_state.getMemberNames()[0];
		for(auto t : {"Binaries","Analogs"})
		{
			for(Json::ArrayIndex i=0; i<10; i++)
			{
				auto qual = QualityFlagsFromString(current_state[time][t][i]["Quality"].asString());
				all_have_qual = test_set ? (qual & test_qual) != QualityFlags::NONE
				                : (qual & test_qual) == QualityFlags::NONE;

				if(!all_have_qual)
					break;
			}
			if(!all_have_qual)
				break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	if(!all_have_qual)
		std::cout<<"Test "<<ToString(test_qual)<<(test_set ? "" : " not")<<" set, failed"<<std::endl<<json_str<<std::endl;
	REQUIRE(all_have_qual);
}

inline void require_comms_point(const bool val, std::shared_ptr<DataPort> pPort)
{
	unsigned int count = 0;
	bool pass = false;
	std::string json_str; //use for debugging failure
	while(!pass && count < test_timeout)
	{
		auto current_state = pPort->GetCurrentState();
		json_str = current_state.toStyledString();
		auto time = current_state.getMemberNames()[0];
		for(Json::ArrayIndex i = 0; i <= 10; i++)
		{
			if(current_state[time]["Binaries"][i]["Index"].asUInt() == 10)
			{
				pass = (current_state[time]["Binaries"][i]["Value"].asBool() == val
				        && QualityFlagsFromString(current_state[time]["Binaries"][i]["Quality"].asString()) == QualityFlags::ONLINE);
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	if(!pass)
		std::cout<<json_str<<std::endl;
	REQUIRE(pass);
}

template<EventType type>
inline void send_events(std::shared_ptr<DataPort> pPort, size_t start, size_t end, const QualityFlags qual = QualityFlags::ONLINE)
{
	for(auto index = start; index <= end; index++)
	{
		auto event = std::make_shared<EventInfo>(type,index,"Test",qual);
		event->SetPayload<type>(static_cast<typename EventTypePayload<type>::type>(index%2));
		pPort->Event(event,"Test",std::make_shared<std::function<void (CommandStatus status)>>([] (CommandStatus){}));
	}
}

inline void check_event_values(std::shared_ptr<DataPort> pPort)
{
	unsigned int count = 0;
	bool pass = false;
	std::string json_str; //use for debugging failure
	while(!pass && count < test_timeout)
	{
		auto current_state = pPort->GetCurrentState();
		json_str = current_state.toStyledString();
		auto time = current_state.getMemberNames()[0];
		for(Json::ArrayIndex i = 0; i < 10; i++)
		{
			pass = (current_state[time]["Binaries"][i]["Value"].asBool() == static_cast<bool>(current_state[time]["Binaries"][i]["Index"].asUInt()%2)
			        && current_state[time]["Analogs"][i]["Value"].asUInt() == current_state[time]["Analogs"][i]["Index"].asUInt()%2);
			if(!pass)
				break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	if(!pass)
		std::cout<<json_str<<std::endl;
	REQUIRE(pass);
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

inline unsigned int require_connection_increase(std::shared_ptr<ManInTheMiddle> pMITM, bool dir, unsigned int start_conns)
{
	unsigned int count = 0;
	while(pMITM->ConnectionCount(dir) == start_conns && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	REQUIRE(pMITM->ConnectionCount(dir) > start_conns);
	return pMITM->ConnectionCount(dir);
}

TEST_CASE(SUITE("Quality and CommsPoint"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		INFO("Asio liftime")
		auto ios = odc::asio_service::Get();
		auto work = ios->make_work();
		std::thread t([ios](){ios->run();});

		//This is to test the datacon/gateway situation where
		//we want the link status of a master to be presented on the
		//corresponding outstation as bad quality points and a comms point

		// | Downstream connection  |           | Upstream connection |
		// |------------------------|           |---------------------|
		// [OS] <---TCP MITM---> [MS] <--ODC--> [OS] <---TCP---> [MS]

		{
			INFO("Test objects liftime")

			//need a master/outstation pair downstream with a MITM
			auto pMITM = std::make_shared<ManInTheMiddle>(MITMConfig::SERVER_CLIENT,20000,20001,"DNP3Port");
			port_pair_t downstream_pair = PortPair(portlib,3,2,MITMConfig::SERVER_CLIENT,20000,20001,true);

			//then an extra pair upstream without MITM
			//We'll check that the downstream link status flows through to these as quality/commspoint
			port_pair_t upstream_pair = PortPair(portlib,1,0,MITMConfig::SERVER_CLIENT,20002,20002,false);

			//cross subscribe the two pairs for ODC events - usually a connector would be used, but we can go direct
			downstream_pair.second->Subscribe(upstream_pair.first.get(),"UpstreamOS");
			upstream_pair.first->Subscribe(downstream_pair.second.get(),"DownstreamMS");

			downstream_pair.first->Enable();
			upstream_pair.first->Enable();
			downstream_pair.second->Enable();
			upstream_pair.second->Enable();

			require_link_up(downstream_pair.first);
			require_link_up(upstream_pair.first);
			require_link_up(downstream_pair.second);
			require_link_up(upstream_pair.second);

			require_quality(QualityFlags::COMM_LOST,false,upstream_pair.second);
			require_comms_point(true,upstream_pair.second);
			require_quality(QualityFlags::RESTART,true,upstream_pair.second);

			//pump through some values
			send_events<EventType::Binary>(downstream_pair.first,0,9);
			send_events<EventType::Analog>(downstream_pair.first,0,9);

			require_quality(QualityFlags::RESTART,false,upstream_pair.second);
			check_event_values(upstream_pair.second);

			for(size_t i=0; i<10; i++)
			{
				pMITM->Down(); //don't just drop, or the link watchdog will cause flapping
				require_link_down(downstream_pair.first);
				require_link_down(downstream_pair.second);

				require_quality(QualityFlags::COMM_LOST,true,upstream_pair.second);
				require_comms_point(false,upstream_pair.second);
				require_quality(QualityFlags::RESTART,false,upstream_pair.second);
				check_event_values(upstream_pair.second);

				pMITM->Up();
				require_link_up(downstream_pair.first);
				require_link_up(downstream_pair.second);

				require_quality(QualityFlags::COMM_LOST,false,upstream_pair.second);
				require_comms_point(true,upstream_pair.second);
				require_quality(QualityFlags::RESTART,false,upstream_pair.second);
				check_event_values(upstream_pair.second);
			}

			//now just make sure COMM_LOST doesn't affect other flags
			send_events<EventType::Binary>(downstream_pair.first,0,9,QualityFlags::LOCAL_FORCED|QualityFlags::ONLINE);
			send_events<EventType::Analog>(downstream_pair.first,0,9,QualityFlags::LOCAL_FORCED|QualityFlags::ONLINE);
			require_quality(QualityFlags::ONLINE,true,upstream_pair.second);
			require_quality(QualityFlags::LOCAL_FORCED,true,upstream_pair.second);
			require_quality(QualityFlags::COMM_LOST,false,upstream_pair.second);
			pMITM->Down();
			require_quality(QualityFlags::COMM_LOST,true,upstream_pair.second);
			require_quality(QualityFlags::ONLINE,true,upstream_pair.second);
			require_quality(QualityFlags::LOCAL_FORCED,true,upstream_pair.second);

			//now try comm lost while upstream disabled
			pMITM->Up();
			require_link_up(downstream_pair.first);
			require_link_up(downstream_pair.second);
			require_quality(QualityFlags::COMM_LOST,false,upstream_pair.second);
			require_comms_point(true,upstream_pair.second);

			{
				INFO("Comm lost while upstream disabled")
				upstream_pair.first->Disable();

				{
					INFO("Drop downstream link")
					pMITM->Down();
					require_quality(QualityFlags::COMM_LOST,true,downstream_pair.second);
				}
				//wait to give time for anything unexpected... because we don't expect anything
				std::this_thread::sleep_for(std::chrono::milliseconds(200));

				{
					INFO("Check upstream didn't get comm-lost: it's disabled")
					INFO("Check Outstation")
					require_quality(QualityFlags::COMM_LOST,false,upstream_pair.first);
					INFO("Check Master")
					require_quality(QualityFlags::COMM_LOST,false,upstream_pair.second);
				}

				{
					INFO("Check upstream gets comm-lost when it's re-enabled")
					//downstream should re-assert comm-lost when connection detected
					upstream_pair.first->Enable();
					INFO("Check Outstation")
					require_quality(QualityFlags::COMM_LOST,true,upstream_pair.first);
					INFO("Check Master")
					require_quality(QualityFlags::COMM_LOST,true,upstream_pair.second);
				}
			}

			upstream_pair.second->Disable();
			downstream_pair.second->Disable();
			upstream_pair.first->Disable();
			downstream_pair.first->Disable();
		} //lifetime of all test ojects

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
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
			odc::spdlog_get("DNP3Port")->info("MITMConfig: {}.",to_string(conn_dir));
			//Make back-to-back TCP sockets so we can drop data and count re-connects
			auto pMITM = std::make_shared<ManInTheMiddle>(conn_dir,ms_port,os_port,"DNP3Port");

			//create an outstation/master pair, and enable them
			port_pair_t port_pair = PortPair(portlib,1,0,conn_dir,ms_port,os_port);
			port_pair.first->Enable();
			port_pair.second->Enable();

			odc::spdlog_get("DNP3Port")->info("Initial link up check.");
			//wait for them to connect through the man-in-the-middle
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);

			//take a baseline for the number of connections
			auto start_open1 = pMITM->ConnectionCount(true);
			auto start_open2 = pMITM->ConnectionCount(false);
			odc::spdlog_get("DNP3Port")->info("Initial connection count: {},{}",start_open1,start_open2);

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
			auto new_open1 = require_connection_increase(pMITM,true,start_open1);
			auto new_open2 = require_connection_increase(pMITM,false,start_open2);
			odc::spdlog_get("DNP3Port")->info("New connection count: {},{}",new_open1,new_open2);
		}

		work.reset();
		ios->run();
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
		auto pMITM = std::make_shared<ManInTheMiddle>(MITMConfig::SERVER_CLIENT,port1,port2,"DNP3Port");

		//create outstation/master pairs, and enable them
		std::vector<port_pair_t> port_pairs;
		for(size_t os_addr : {1,2,3})
			port_pairs.push_back(PortPair(portlib,os_addr,0,MITMConfig::SERVER_CLIENT,port1,port2));
		for(auto port_pair : port_pairs)
		{
			port_pair.first->Enable();
			port_pair.second->Enable();
		}

		odc::spdlog_get("DNP3Port")->info("Initial links up test.");
		for(auto port_pair : port_pairs)
		{
			//wait for them to connect through the man-in-the-middle
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);
		}

		//take a baseline for the number of connections
		auto start_open1 = pMITM->ConnectionCount(true);
		auto start_open2 = pMITM->ConnectionCount(false);
		odc::spdlog_get("DNP3Port")->info("Initial connection count: {},{}",start_open1,start_open2);

		//just disabling one or two ports shouldn't affect the others
		port_pairs[0].first->Disable();
		odc::spdlog_get("DNP3Port")->info("One down.");
		require_link_down(port_pairs[0].second);
		//wait another keepalive periods to check - just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		for(size_t i : {1,2})
		{
			require_link_up(port_pairs[i].first);
			require_link_up(port_pairs[i].second);
		}
		port_pairs[1].second->Disable();
		odc::spdlog_get("DNP3Port")->info("Two down.");
		require_link_down(port_pairs[1].first);
		//wait another keepalive periods to check - just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		require_link_up(port_pairs[2].first);
		require_link_up(port_pairs[2].second);

		port_pairs[0].first->Enable();
		port_pairs[1].second->Enable();
		odc::spdlog_get("DNP3Port")->info("All back.");

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
		//data is being dropped now, so all the links should go down,
		//and reconnects should happen because they're all down
		for(auto port_pair : port_pairs)
		{
			require_link_down(port_pair.first);
			require_link_down(port_pair.second);
		}

		pMITM->Allow();
		//wait another couple of keepalive periods just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period*2));
		for(auto port_pair : port_pairs)
		{
			require_link_up(port_pair.first);
			require_link_up(port_pair.second);
		}

		//make sure there were reconnects
		auto new_open1 = require_connection_increase(pMITM,true,start_open1);
		auto new_open2 = require_connection_increase(pMITM,false,start_open2);
		odc::spdlog_get("DNP3Port")->info("New connection count: {},{}",new_open1,new_open2);

		for(auto port_pair : port_pairs)
		{
			port_pair.first->Disable();
			port_pair.second->Disable();
		}

		//wait another keepalive periods just in case
		std::this_thread::sleep_for(std::chrono::milliseconds(link_ka_period));
		pMITM.reset();

		work.reset();
		ios->run();
		t.join();
		ios.reset();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}


