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

// Go's runtime needs to handle posix signals itself
#define CATCH_CONFIG_NO_POSIX_SIGNALS

#include "../PortLoader.h"
#include "../ThreadPool.h"
#include <catch.hpp>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <opendatacon/IOTypes.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>

#include "CAPI/C_Port.h"

#define SUITE(name) "GoModbusPortTests - " name

extern spdlog::level::level_enum log_level;

static void TestSetup()
{
	odc::spdlog_drop_all();
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);
	auto pGoLogger = std::make_shared<spdlog::logger>("GoModbusPort", console_sink);
	pGoLogger->set_level(log_level);
	odc::spdlog_register_logger(pGoLogger);
}

static void TestTearDown()
{
	odc::spdlog_flush_all();
}

static Json::Value BuildMinimalConfig()
{
	Json::Value conf;
	conf["TCP"]["Address"] = "127.0.0.1:1502";
	conf["UnitID"] = 1;
	conf["TimeoutMs"] = 1000;
	conf["PollRateMs"] = 1000;
	return conf;
}

static Json::Value BuildConfigWithPoints()
{
	Json::Value conf = BuildMinimalConfig();

	// Single binary input (coil 0)
	Json::Value bin;
	bin["Modbus"]["Type"] = "Coil";
	bin["Modbus"]["Address"] = 0;
	conf["Binaries"].append(bin);

	// Range of analogs (holding registers 100-103)
	Json::Value ana;
	ana["Modbus"]["Type"] = "HoldingRegister";
	ana["Modbus"]["Range"]["Start"] = 100;
	ana["Modbus"]["Range"]["Stop"] = 103;
	ana["Modbus"]["Endian"] = "ABCD";
	conf["Analogs"].append(ana);

	// Binary control (coil 200)
	Json::Value ctl;
	ctl["Modbus"]["Type"] = "Coil";
	ctl["Modbus"]["Address"] = 200;
	conf["BinaryControls"].append(ctl);

	return conf;
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("ConstructEnableDisableDestroy"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto port = std::make_shared<odc::C_Port>("GoModbus", "LifecycleTest",
			"", BuildMinimalConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		// Brief sleep so the Go goroutine initialises and attempts connection
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
		auto port = std::make_shared<odc::C_Port>("GoModbus", "ConfigTest",
			"", Json::Value(Json::objectValue), portlib);
		REQUIRE(port != nullptr);

		// Provide config before Build
		Json::Value conf;
		conf["TCP"]["Address"] = "127.0.0.1:1502";
		conf["UnitID"] = 42;
		port->ProcessElements(conf);

		port->Build();
		port->Disable();

		// Verify config was stored
		const char* json = port->GetConfigStr().c_str();
		REQUIRE(json != nullptr);
		REQUIRE(std::string(json).find("127.0.0.1") != std::string::npos);
		REQUIRE(std::string(json).find("42") != std::string::npos);
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  Control event dispatch
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("BinaryControlDispatch"))
{
	TestSetup();
	ThreadPool pool(1);

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		auto port = std::make_shared<odc::C_Port>("GoModbus", "ControlTest",
			"", BuildConfigWithPoints(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();

		// Allow Go runtime to initialise
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// Send a binary control event — Go code will attempt to write to coil 200
		// and invoke the status callback with failure (no Modbus server).
		std::atomic_bool executed(false);
		odc::CommandStatus cb_status = odc::CommandStatus::UNDEFINED;
		auto pStatusCallback = std::make_shared<std::function<void (odc::CommandStatus status)>>(
			[&executed, &cb_status](odc::CommandStatus status)
			{
				cb_status = status;
				executed = true;
			});

		auto event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, 0);
		odc::ControlRelayOutputBlock crob;
		crob.functionCode = odc::ControlCode::LATCH_ON;
		event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(crob));
		port->Event(event, "TestHarness", pStatusCallback);

		// Wait for callback to fire
		unsigned int count = 0;
		while(!executed && count < 500)
		{
			odc::asio_service::Get()->poll_one();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count++;
		}
		REQUIRE(executed);
		// Without a Modbus server, the write should fail (but the callback fires)
		// The exact status depends on the Go implementation, but it should not be UNDEFINED
		REQUIRE(cb_status != odc::CommandStatus::UNDEFINED);

		port->Disable();
	}

	TestTearDown();
}

TEST_CASE(SUITE("Build_Multiple_Calls"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto port = std::make_shared<odc::C_Port>("GoModbus", "MultiBuildTest",
			"", BuildMinimalConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		// Call Build again — should be idempotent
		port->Build();
		port->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		port->Disable();
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  Immediate Disable Test
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("Disable_Immediate_Returns"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		// Use a very short poll rate to ensure polling is active
		Json::Value conf = BuildMinimalConfig();
		conf["PollRateMs"] = 10;
		conf["TimeoutMs"] = 5000; // Long timeout so connect attempt blocks

		auto port = std::make_shared<odc::C_Port>("GoModbus", "ImmediateDisableTest",
			"", conf, portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();

		// Wait for polling to start
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Disable should return immediately, not wait for any in-flight poll
		auto start = std::chrono::steady_clock::now();
		port->Disable();
		auto elapsed = std::chrono::steady_clock::now() - start;

		// Disable should complete quickly (well under 1 second)
		REQUIRE(elapsed < std::chrono::seconds(1));
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  Poll Dropping Test
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("Poll_Dropping_When_Overwhelmed"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		// Configure with maxConcurrentPolls=1 and very fast poll rate
		// Poll function will be slow (connection timeout), so polls should be dropped
		Json::Value conf = BuildMinimalConfig();
		conf["PollRateMs"] = 10;        // Very fast polling
		conf["TimeoutMs"] = 5000;       // Long timeout
		conf["MaxConcurrentPolls"] = 1; // Only 1 concurrent poll allowed

		auto port = std::make_shared<odc::C_Port>("GoModbus", "PollDroppingTest",
			"", conf, portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();

		// Let it run for a bit - polls will queue up and be dropped
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		port->Disable();

		// The test passes if we don't crash - the poll dropping is internal
		// We can't easily inspect the dropped count from C++ without adding
		// a stats API, but the fact that it runs without deadlock verifies
		// the non-blocking behavior.
	}

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  Load / Unload / Sleep
//  Verifies that Disable cancels the reconnect timer before the library is
//  dlclose'd.  After destruction the port must leave no goroutines that will
//  fire into unmapped memory.  We sleep past initialReconnectDelayMs (1 s) to
//  give any leaked timer goroutine a chance to execute.  A SIGSEGV here means
//  the Go code did not stop its timer before allowing dlclose to proceed.
// ---------------------------------------------------------------------------

TEST_CASE(SUITE("LoadUnloadSleep"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);

	{
		ThreadPool pool(1);
		auto port = std::make_shared<odc::C_Port>("GoModbus", "LoadUnloadSleepTest",
			"", BuildMinimalConfig(), portlib);
		REQUIRE(port != nullptr);

		port->Build();
		port->Enable();
		// Let Go initialise and arm the reconnect timer (delay = 1000 ms).
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		// Disable must stop the timer before returning.
		port->Disable();
		// ~C_Port fires here: go_port_destroy, then UnLoadModule(portlib) → dlclose.
		// Library segments are unmapped after this scope closes.
	}

	// Sleep past the original reconnect delay.  If doDisable did not call
	// timer.Stop() the timer goroutine will fire into unmapped .text → SIGSEGV.
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));

	// Reaching this line means no goroutine fired after dlclose.
	// (portlib is a dangling handle at this point — do NOT call UnLoadModule.)
	TestTearDown();
}
