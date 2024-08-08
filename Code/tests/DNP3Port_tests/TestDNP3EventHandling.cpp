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
#include "../ThreadPool.h"
#include "opendatacon/IOTypes.h"
#include "json/json.h"
#include <catch.hpp>
#include <cmath>
#include <cstddef>
#include <memory>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "DNP3EventHandlingTestSuite - " name

constexpr size_t num_indexes = 10;
constexpr size_t test_timeout_ms = 5000;

constexpr const char* AnaOutTypes[] = {"AnalogOutputInt16", "AnalogOutputInt32", "AnalogOutputFloat32", "AnalogOutputDouble64"};

std::pair<std::shared_ptr<DataPort>,std::shared_ptr<DataPort>> MakePorts(const module_ptr portlib)
{
	//make a config
	Json::Value conf = Json::objectValue;
	conf["ServerType"] = "PERSISTENT";
	//add some points
	for(auto& PointType : {"Analogs", "Binaries", "OctetStrings", "BinaryControls", "AnalogControls", "BinaryOutputStatuses", "AnalogOutputStatuses"})
	{
		conf[PointType] = Json::arrayValue;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			Json::Value point = Json::objectValue;
			point["Index"] = idx;
			conf[PointType].append(point);
		}
	}
	//add a mix of Analog Output Types
	for(Json::ArrayIndex idx = 0; idx < num_indexes; ++idx)
		conf["AnalogControls"][idx]["Type"] = AnaOutTypes[idx%4];

	//enable unsol
	conf["EnableUnsol"] = true;
	conf["UnsolClass1"] = true;
	conf["UnsolClass2"] = true;
	conf["UnsolClass3"] = true;
	conf["DoUnsolOnStartup"] = true;
	conf["IntegrityScanRatems"] = 0;
	conf["EventClass1ScanRatems"] = 0;
	conf["EventClass2ScanRatems"] = 0;
	conf["EventClass3ScanRatems"] = 0;


	//make an outstation port
	newptr newOutstation = GetPortCreator(portlib, "DNP3Outstation");
	REQUIRE(newOutstation);
	delptr delOutstation = GetPortDestroyer(portlib, "DNP3Outstation");
	REQUIRE(delOutstation);
	auto OPUT = std::shared_ptr<DataPort>(newOutstation("OutstationUnderTest", "", conf), delOutstation);
	REQUIRE(OPUT);

	//make a master port
	newptr newMaster = GetPortCreator(portlib, "DNP3Master");
	REQUIRE(newMaster);
	delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
	REQUIRE(delMaster);
	auto MPUT = std::shared_ptr<DataPort>(newMaster("MasterUnderTest", "", conf), delMaster);
	REQUIRE(MPUT);

	//get them to build themselves using their configs
	OPUT->Build();
	MPUT->Build();

	return std::make_pair(OPUT, MPUT);
}

bool WaitForLinkUp(const std::shared_ptr<DataPort>& port, const size_t timeout_ms = 20000)
{
	unsigned int count = 0;
	while((port->GetStatus()["Result"].asString() == "Port enabled - link down") && count < timeout_ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		count++;
	}
	return (port->GetStatus()["Result"].asString() == "Port enabled - link up (unreset)");
}

template<odc::EventType ET, typename PayloadT>
void SendEvent(const std::shared_ptr<DataPort>& port, const size_t idx, PayloadT payload)
{
	auto event = std::make_shared<odc::EventInfo>(ET, idx);
	event->SetPayload<ET>(std::move(payload));
	port->Event(event, "Test", std::make_shared<std::function<void (CommandStatus status)>>([] (CommandStatus status){}));
}

template<odc::EventType ET, typename PayloadT>
void CheckPointDB(const std::shared_ptr<DataPort>& port, const std::vector<PayloadT>& payloads)
{
	size_t ms_count = 0;
	for(size_t idx = 0; idx < num_indexes; ++idx)
	{
		auto event = port->pEventDB()->Get(ET, idx);
		auto expected_payload = payloads.at(idx);
		bool payload_as_expected = false;
		while(ms_count++ < test_timeout_ms)
		{
			if(ET >= odc::EventType::AnalogOutputInt16 && ET <= odc::EventType::AnalogOutputDouble64)
			{
				(event = port->pEventDB()->Get(odc::EventType::AnalogOutputInt16, idx))
				|| (event = port->pEventDB()->Get(odc::EventType::AnalogOutputInt32, idx))
				|| (event = port->pEventDB()->Get(odc::EventType::AnalogOutputFloat32, idx))
				|| (event = port->pEventDB()->Get(odc::EventType::AnalogOutputDouble64, idx));
			}
			else
				event = port->pEventDB()->Get(ET, idx);
			if(event)
			{
				try
				{
					if constexpr(ET >= odc::EventType::AnalogOutputInt16 && ET <= odc::EventType::AnalogOutputDouble64)
					{
						switch(event->GetEventType())
						{
							case odc::EventType::AnalogOutputInt16:
							{
								auto payload = event->GetPayload<odc::EventType::AnalogOutputInt16>();
								if(payload.first == expected_payload.first)
									payload_as_expected = true;
								break;
							}
							case odc::EventType::AnalogOutputInt32:
							{
								auto payload = event->GetPayload<odc::EventType::AnalogOutputInt32>();
								if(payload.first == expected_payload.first)
									payload_as_expected = true;
								break;
							}
							case odc::EventType::AnalogOutputFloat32:
							{
								auto payload = event->GetPayload<odc::EventType::AnalogOutputFloat32>();
								if(payload.first == expected_payload.first)
									payload_as_expected = true;
								break;
							}
							case odc::EventType::AnalogOutputDouble64:
							{
								auto payload = event->GetPayload<odc::EventType::AnalogOutputDouble64>();
								if(payload.first == expected_payload.first)
									payload_as_expected = true;
								break;
							}
							default:
								break;
						}
						if(payload_as_expected)
							break;
					}
					else
					{
						auto payload = event->GetPayload<ET>();
						if(payload == expected_payload)
						{
							payload_as_expected = true;
							break;
						}
					}
				}
				catch(std::exception&)
				{}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		CHECK(event);
		if(event)
		{
			CHECK(payload_as_expected);
		}
	}
	CHECK(ms_count < test_timeout_ms);
}

TEST_CASE(SUITE("Analogs"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//send a value to each index
		std::vector<double> values;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			values.push_back((double)(rand()%100));
			SendEvent<odc::EventType::Analog>(OPUT, idx, values.back());
		}
		CheckPointDB<odc::EventType::Analog>(MPUT, values);

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Binaries"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//send a value to each index
		std::vector<bool> values;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			values.push_back(rand()>RAND_MAX/2);
			SendEvent<odc::EventType::Binary>(OPUT, idx, values.back());
		}
		CheckPointDB<odc::EventType::Binary>(MPUT, values);

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("OctetStrings"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//send a value to each index
		std::vector<odc::OctetStringBuffer> values;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			values.push_back(std::to_string(idx));
			SendEvent<odc::EventType::OctetString>(OPUT, idx, values.back());
		}
		CheckPointDB<odc::EventType::OctetString>(MPUT, values);

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("BinaryControls"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//send a value to each index
		std::vector<typename EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type> values;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			typename EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type value;
			value.count = rand()%256;
			values.push_back(value);
			SendEvent<odc::EventType::ControlRelayOutputBlock>(MPUT, idx, values.back());
		}
		CheckPointDB<odc::EventType::ControlRelayOutputBlock>(OPUT, values);

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Analog Controls"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//send a value to each index
		std::vector<typename EventTypePayload<odc::EventType::AnalogOutputDouble64>::type> values;
		for(size_t idx = 0; idx < num_indexes; ++idx)
		{
			typename EventTypePayload<odc::EventType::AnalogOutputDouble64>::type value;
			value.first = rand()%100;
			values.push_back(value);
			SendEvent<odc::EventType::AnalogOutputDouble64>(MPUT, idx, values.back());
		}
		CheckPointDB<odc::EventType::AnalogOutputDouble64>(OPUT, values);

		//TODO: test the other Analog Output Types
		//  and test that values are capped if out of range, and warnings are generated
		//  also, should there be an option to block out of range commands, and return bad command status?

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Analog Output Statuses"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//TODO: send some events

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Binary Output Statuses"))
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		//Get some ports up and going
		auto [OPUT, MPUT] = MakePorts(portlib);
		ThreadPool thread_pool(1); //TODO: Use more threads
		OPUT->Enable();
		MPUT->Enable();
		CHECK(WaitForLinkUp(MPUT));

		//TODO: send some events

		//turn things off
		OPUT->Disable();
		MPUT->Disable();
	}
	//Unload the library
	UnLoadModule(portlib);
	TestTearDown();
}