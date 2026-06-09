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
 * GoModbusServerTest.cpp
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

// Go's runtime needs to own both POSIX signals and Windows SEH/VEH.
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_NO_WINDOWS_SEH

#include "TestHelpers.h"
#include <catch.hpp>

#define SUITE(name) "GoModbusServerTests - " name

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("ConstructEnableDisableDestroy"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvLifecycle",
			"", MakeServerConfig(port), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		srv->Disable();
	}

	TestTearDown();
}

TEST_CASE(SUITE("BuildIdempotent"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvBuildTwice",
			"", MakeServerConfig(port), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		srv->Build(); // second Build must be a no-op
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		srv->Disable();
	}

	TestTearDown();
}

TEST_CASE(SUITE("ConfigPassthrough"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvConfigTest",
			"", Json::Value(Json::objectValue), portlib);
		REQUIRE(srv != nullptr);

		Json::Value conf;
		conf["TCP"]["Listen"] = "127.0.0.1:19502";
		conf["UnitID"]        = 7;
		srv->ProcessElements(conf);
		srv->Build();
		srv->Disable();

		const auto& json = srv->GetConfigStr();
		REQUIRE(json.find("19502") != std::string::npos);
		REQUIRE(json.find("7")     != std::string::npos);
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Inbound ODC event handling
// ---------------------------------------------------------------------------

// ODC Binary event for a mapped coil → store update → callback SUCCESS.
TEST_CASE(SUITE("InboundBinaryEvent"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;

		Json::Value pts;
		Json::Value bin;
		bin["Index"]         = 0;
		bin["Modbus"]["Type"]    = "Coil";
		bin["Modbus"]["Address"] = 0;
		pts["Binaries"].append(bin);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvBinaryIn",
			"", MakeServerConfig(port, pts), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 0);
		event->SetPayload<odc::EventType::Binary>(true);

		REQUIRE(SendEvent(*srv, event) == odc::CommandStatus::SUCCESS);
		srv->Disable();
	}

	TestTearDown();
}

// ODC Analog event for a mapped holding register → store update → callback SUCCESS.
TEST_CASE(SUITE("InboundAnalogEvent"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;

		Json::Value pts;
		Json::Value ana;
		ana["Index"]         = 0;
		ana["Modbus"]["Type"]    = "HoldingRegister";
		ana["Modbus"]["Address"] = 0;
		ana["DataType"]          = "Int16";
		pts["Analogs"].append(ana);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvAnalogIn",
			"", MakeServerConfig(port, pts), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Analog, 0);
		event->SetPayload<odc::EventType::Analog>(42.0);

		REQUIRE(SendEvent(*srv, event) == odc::CommandStatus::SUCCESS);
		srv->Disable();
	}

	TestTearDown();
}

// ODC event at an unmapped index → NOT_SUPPORTED.
TEST_CASE(SUITE("UnmappedEventNotSupported"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;

		// Only index 0 is configured; send at index 99.
		Json::Value pts;
		Json::Value bin;
		bin["Index"]         = 0;
		bin["Modbus"]["Type"]    = "Coil";
		bin["Modbus"]["Address"] = 0;
		pts["Binaries"].append(bin);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvUnmapped",
			"", MakeServerConfig(port, pts), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 99);
		event->SetPayload<odc::EventType::Binary>(true);

		REQUIRE(SendEvent(*srv, event) == odc::CommandStatus::NOT_SUPPORTED);
		srv->Disable();
	}

	TestTearDown();
}

// Event received before Enable() → HARDWARE_ERROR.
TEST_CASE(SUITE("EventWhileDisabled"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		const int port = 1502;

		Json::Value pts;
		Json::Value bin;
		bin["Index"]         = 0;
		bin["Modbus"]["Type"]    = "Coil";
		bin["Modbus"]["Address"] = 0;
		pts["Binaries"].append(bin);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "SrvDisabledEvent",
			"", MakeServerConfig(port, pts), portlib);
		REQUIRE(srv != nullptr);

		srv->Build();
		// Port is not enabled — event must be rejected immediately.

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 0);
		event->SetPayload<odc::EventType::Binary>(true);

		REQUIRE(SendEvent(*srv, event) == odc::CommandStatus::HARDWARE_ERROR);
	}

	TestTearDown();
}
