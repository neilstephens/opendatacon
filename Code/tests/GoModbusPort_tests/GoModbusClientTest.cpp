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
 * GoModbusClientTest.cpp
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

// Go's runtime needs to own both POSIX signals and Windows SEH/VEH.
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_NO_WINDOWS_SEH

#include "TestHelpers.h"
#include <catch.hpp>

#define SUITE(name) "GoModbusClientTests - " name

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
		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "LifecycleTest",
			"", MakeMinimalClientConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		// Brief sleep so the Go goroutine initialises and attempts connection.
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		port->Disable();
	}

	TestTearDown();
}

TEST_CASE(SUITE("ConfigPassthrough"))
{
	TestSetup();
	ThreadPool pool(1);

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "ConfigTest",
			"", Json::Value(Json::objectValue), portlib);
		REQUIRE(port != nullptr);

		// Provide config before Build.
		Json::Value conf;
		conf["TCP"]["Address"] = "127.0.0.1:1502";
		conf["UnitID"]         = 42;
		port->ProcessElements(conf);

		port->Build();
		port->Disable();

		// Verify config was stored.
		const auto& json = port->GetConfigStr();
		REQUIRE(json.find("127.0.0.1") != std::string::npos);
		REQUIRE(json.find("42") != std::string::npos);
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Control event dispatch
// ---------------------------------------------------------------------------

// Send a BinaryControl (CROB LATCH_ON) to the port.  There is no real Modbus
// server, so the write will fail, but the status callback must still fire with
// a definite (non-UNDEFINED) status within the timeout.
TEST_CASE(SUITE("BinaryControlDispatch"))
{
	TestSetup();
	ThreadPool pool(1);

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "ControlTest",
			"", MakeClientConfigWithPoints(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();

		// Allow Go runtime to initialise.
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		std::atomic_bool executed(false);
		odc::CommandStatus cb_status = odc::CommandStatus::UNDEFINED;
		auto pStatusCallback = std::make_shared<std::function<void(odc::CommandStatus)>>(
			[&executed, &cb_status](odc::CommandStatus status)
			{
				cb_status = status;
				executed  = true;
			});

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, 0);
		odc::ControlRelayOutputBlock crob;
		crob.functionCode = odc::ControlCode::LATCH_ON;
		event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(crob));
		port->Event(event, "TestHarness", pStatusCallback);

		REQUIRE(WaitCallback(executed, std::chrono::milliseconds(5000)));
		REQUIRE(cb_status != odc::CommandStatus::UNDEFINED);

		port->Disable();
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Build / multiple calls
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("BuildMultipleCalls"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "MultiBuildTest",
			"", MakeMinimalClientConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Build(); // second call must be a no-op
		port->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		port->Disable();
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Disable returns immediately
// ---------------------------------------------------------------------------

// With a long connect timeout and a fast poll rate, Disable() must return
// in well under 1 second — it must not block waiting for in-flight polls.
TEST_CASE(SUITE("DisableReturnsImmediately"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		Json::Value conf    = MakeMinimalClientConfig();
		conf["PollRateMs"]  = 10;
		conf["TimeoutMs"]   = 5000;

		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "ImmediateDisableTest",
			"", conf, portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		const auto start   = std::chrono::steady_clock::now();
		port->Disable();
		const auto elapsed = std::chrono::steady_clock::now() - start;

		REQUIRE(elapsed < std::chrono::seconds(1));
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Poll dropping under load
// ---------------------------------------------------------------------------

// With MaxConcurrentPolls=1 and a very fast poll rate, slow connections cause
// polls to be dropped.  The test verifies no deadlock or crash occurs.
TEST_CASE(SUITE("PollDroppingWhenOverloaded"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		Json::Value conf          = MakeMinimalClientConfig();
		conf["PollRateMs"]        = 10;
		conf["TimeoutMs"]         = 5000;
		conf["MaxConcurrentPolls"] = 1;

		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "PollDroppingTest",
			"", conf, portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		port->Disable();
		// Reaching here without deadlock is the assertion.
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Load / Unload / Sleep
// ---------------------------------------------------------------------------

// Disable must cancel the reconnect timer before returning.  After ~C_Port
// fires and dlclose() unmaps the library, we sleep past the original
// reconnect delay (1 s).  A SIGSEGV here means a leaked timer goroutine
// fired into unmapped .text.
TEST_CASE(SUITE("LoadUnloadSleep"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto port = std::make_shared<odc::C_Port>("GoModbusClient", "LoadUnloadSleepTest",
			"", MakeMinimalClientConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		// Let Go arm the reconnect timer (initial delay = 1 000 ms).
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		// Disable must stop the timer before returning.
		port->Disable();
		// ~C_Port fires here: go_port_destroy then UnLoadModule(portlib) → dlclose.
	}

	// Sleep past the original reconnect delay.  A leaked timer goroutine
	// would fire into unmapped memory → SIGSEGV.
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	// (portlib is a dangling handle — do NOT call UnLoadModule.)

	TestTearDown();
}
