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
#include <limits>
#include <memory>
#include <opendatacon/asio.h>
#include <thread>

#define SUITE(name) "DNP3EventHandlingTestSuite - " name

constexpr size_t num_indexes = 1024; //needs a multiple of 4 for the Analog Output Types
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
			point["Index"] = Json::UInt(idx);
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
	conf["IntegrityScanRatems"] = Json::UInt(0);
	conf["EventClass1ScanRatems"] = Json::UInt(0);
	conf["EventClass2ScanRatems"] = Json::UInt(0);
	conf["EventClass3ScanRatems"] = Json::UInt(0);


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

template<odc::EventType ET, typename ExpectedT>
bool CheckAnalogOutPayload(const std::shared_ptr<const EventInfo>& event, const ExpectedT& expected_payload)
{
	auto payload = event->GetPayload<ET>();
	if((double)expected_payload.first > std::numeric_limits<decltype(payload.first)>::max()
	   || (double)expected_payload.first < std::numeric_limits<decltype(payload.first)>::lowest())
	{
		if(payload.first == std::numeric_limits<decltype(payload.first)>::max()
		   || payload.first == std::numeric_limits<decltype(payload.first)>::lowest())
			return true;
	}
	auto delta = std::abs(expected_payload.first*0.0001);
	auto min = expected_payload.first - delta;
	auto max = expected_payload.first + delta;
	if(payload.first >= min && payload.first <= max)
		return true;
	UNSCOPED_INFO("Payload is " << payload.first << ", expected " << min << " to " << max);
	return false;
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
		while(ms_count < test_timeout_ms)
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
								payload_as_expected = CheckAnalogOutPayload<odc::EventType::AnalogOutputInt16>(event, expected_payload);
								break;
							case odc::EventType::AnalogOutputInt32:
								payload_as_expected = CheckAnalogOutPayload<odc::EventType::AnalogOutputInt32>(event, expected_payload);
								break;
							case odc::EventType::AnalogOutputFloat32:
								payload_as_expected = CheckAnalogOutPayload<odc::EventType::AnalogOutputFloat32>(event, expected_payload);
								break;
							case odc::EventType::AnalogOutputDouble64:
								payload_as_expected = CheckAnalogOutPayload<odc::EventType::AnalogOutputDouble64>(event, expected_payload);
								break;
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
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			ms_count += 50;
		}
		if(event)
		{
			CHECK(payload_as_expected);
		}
		CHECK(event);
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

template<odc::EventType ET>
void CheckAnalogOutRange(const std::shared_ptr<DataPort>& MPUT, const std::shared_ptr<DataPort>& OPUT, const double min, const double max)
{
	typename EventTypePayload<ET>::type value = {min, CommandStatus::SUCCESS};
	size_t num_values = num_indexes/4;
	auto step_size = (max-min)/(num_values-1);
	//send a value to each index
	std::vector<typename EventTypePayload<ET>::type> values;
	for(size_t idx = 0; idx < num_indexes; ++idx)
	{
		SendEvent<ET>(MPUT, idx, value);
		values.push_back(value);
		if(idx%4 == 3 && idx < num_indexes-1)
			value.first += step_size;
	}
	CheckPointDB<ET>(OPUT, values);
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

		CheckAnalogOutRange<odc::EventType::AnalogOutputDouble64>(MPUT, OPUT, -4e38, 4e38);
		CheckAnalogOutRange<odc::EventType::AnalogOutputFloat32>(MPUT, OPUT, -2500000000, 2500000000);
		CheckAnalogOutRange<odc::EventType::AnalogOutputInt32>(MPUT, OPUT, -40000, 40000);
		CheckAnalogOutRange<odc::EventType::AnalogOutputInt16>(MPUT, OPUT, std::numeric_limits<int16_t>::lowest(), std::numeric_limits<int16_t>::max());

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