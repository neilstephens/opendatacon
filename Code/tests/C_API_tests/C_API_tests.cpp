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

#include <catch.hpp>

#include <opendatacon/Platform.h>
#include <opendatacon/odc_c_api.h>
#include <opendatacon/IOHandler.h>
#include <opendatacon/LogHelpers.h>
#include <opendatacon/util.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "CAPI/C_Port.h"
#include "CAPI/C_Transform.h"
#include "CAPI/C_UI.h"

#include "../ThreadPool.h"

extern spdlog::level::level_enum log_level;

// ---------------------------------------------------------------------------
//  Test helpers
// ---------------------------------------------------------------------------

static void TestSetup()
{
	odc::spdlog_drop_all();
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	// Logger for the "opendatacon" framework
	auto pLibLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);
	// Per-type logger for mock C ports (matches aType used in tests)
	auto pMockLogger = std::make_shared<spdlog::logger>("MockCPort", console_sink);
	pMockLogger->set_level(log_level);
	odc::spdlog_register_logger(pMockLogger);
}

static void TestTearDown()
{
	odc::spdlog_flush_all();
}

// A minimal DataPort subclass for subscribing to events
struct TestSubPort: public odc::DataPort
{
	using odc::DataPort::DataPort;
	using odc::IOHandler::Subscribe;
	using odc::IOHandler::UnSubscribe;
	int received = 0;
	void Enable() override {}
	void Disable() override {}
	void Build() override {}
	void Event(std::shared_ptr<const odc::EventInfo>, const std::string&,
		odc::SharedStatusCallback_t cb) override
	{ received++; if(cb) (*cb)(odc::CommandStatus::SUCCESS);}
	void ProcessElements(const Json::Value&) override {}
};

// ---------------------------------------------------------------------------
//  Direct C API function tests (no C++ wrapper)
// ---------------------------------------------------------------------------

TEST_CASE("C_API - mock_c_port direct calls")
{
	// odc_c_api_version
	auto ver = odc_c_api_version();
	REQUIRE(ver != nullptr);
	REQUIRE(std::string(ver) == "1.0");

	// Create port
	void* inst = odc_port_create("TestType", "TestPort");
	REQUIRE(inst != nullptr);

	// Build
	odc_port_build(inst);

	// Enable / Disable
	odc_port_enable(inst);
	odc_port_disable(inst);

	// Send event (null callback = no invoke needed)
	C_EventInfo evt;
	memset(&evt, 0, sizeof(evt));
	evt.event_type = C_EventType_Binary;
	evt.index = 42;
	evt.timestamp = 1000;
	evt.payload.binary_val = 1;
	odc_port_event(inst, &evt, "sender", nullptr);

	// Optional: stats JSON
	const char* stats = odc_port_stats_json(inst);
	REQUIRE(stats != nullptr);
	odc_port_free_string(stats);

	odc_port_destroy(inst);
}

TEST_CASE("C_API - mock_c_transform direct calls")
{
	void* inst = odc_transform_create("TestTransform", "{}");
	REQUIRE(inst != nullptr);
	odc_transform_enable(inst);
	odc_transform_disable(inst);
	odc_transform_destroy(inst);
}

TEST_CASE("C_API - mock_c_ui direct calls")
{
	void* inst = odc_plugin_create("TestUI", "conf.json", "{}");
	REQUIRE(inst != nullptr);
	odc_plugin_build(inst);
	odc_plugin_enable(inst);
	odc_plugin_disable(inst);
	odc_plugin_destroy(inst);
}

// ---------------------------------------------------------------------------
//  C_Port wrapper lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("C_API - C_Port wrapper lifecycle")
{
	TestSetup();

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "LifecycleTestPort", "conf.json", overrides, self);
	REQUIRE(port != nullptr);

	port->Build();
	port->Enable();
	port->Disable();

	REQUIRE(port->GetCInst() != nullptr);
	REQUIRE(port->GetType() == "MockC");

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  C_Port event passthrough (tests odc_InvokeStatusCallback path)
// ---------------------------------------------------------------------------

TEST_CASE("C_API - C_Port event passthrough")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "EventPort", "conf.json", overrides, self);
	port->Build();
	port->Enable();

	auto event = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 1, "test",
		odc::QualityFlags::ONLINE, 12345);

	auto status_result = std::make_shared<odc::CommandStatus>(odc::CommandStatus::UNDEFINED);
	auto status_cb = std::make_shared<std::function<void(odc::CommandStatus)>>(
		[status_result](odc::CommandStatus s) { *status_result = s; });

	port->Event(event, "sender", status_cb);

	for(int i = 0; i < 100; i++)
	{
		if(*status_result != odc::CommandStatus::UNDEFINED) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	REQUIRE(*status_result == odc::CommandStatus::SUCCESS);
	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_PublishEvent with C_StatusCallbackFunc_t
// ---------------------------------------------------------------------------

TEST_CASE("C_API - odc_PublishEvent callback")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);

	// Publisher C_Port
	auto pub = std::make_shared<odc::C_Port>("MockC", "Publisher", "conf.json", overrides, self);
	pub->Build();
	pub->Enable();

	// Subscriber
	auto sub = std::make_shared<TestSubPort>("Subscriber", "conf.json", overrides);
	sub->Build();
	sub->Enable();
	pub->Subscribe(sub.get(), "Subscriber");

	// Build C event to publish via odc_PublishEvent
	C_EventInfo cevt;
	memset(&cevt, 0, sizeof(cevt));
	cevt.event_type = C_EventType_Binary;
	cevt.index = 99;
	cevt.timestamp = 5000;
	cevt.payload.binary_val = 1;

	// Track C callback
	bool cb_fired = false;

	auto callback = [](uint8_t status, void* handle)
			    {
				    (void)status;
				    auto* fired = static_cast<bool*>(handle);
				    *fired = true;
			    };

	// Publish using C API helper
	odc_PublishEvent(pub->GetCInst(), &cevt, callback, &cb_fired);

	// Wait for processing
	for(int i = 0; i < 100; i++)
	{
		if(cb_fired) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(cb_fired);
	REQUIRE(sub->received == 1);

	pub->Disable();
	sub->Disable();
	TestTearDown();
}

TEST_CASE("C_API - odc_PublishEvent no callback")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto pub = std::make_shared<odc::C_Port>("MockC", "PubNoCB", "conf.json", overrides, self);
	pub->Build();
	pub->Enable();

	C_EventInfo cevt;
	memset(&cevt, 0, sizeof(cevt));
	cevt.event_type = C_EventType_Analog;
	cevt.index = 7;
	cevt.payload.analog_val = 3.14;

	// Null callback — should not crash
	odc_PublishEvent(pub->GetCInst(), &cevt, nullptr, nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	pub->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_PublishConnectState
// ---------------------------------------------------------------------------

TEST_CASE("C_API - odc_PublishConnectState")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "CStatePort", "conf.json", overrides, self);
	port->Build();
	port->Enable();

	odc_PublishConnectState(port->GetCInst(), C_ConnectState_CONNECTED);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_GetConfigJSON
// ---------------------------------------------------------------------------

TEST_CASE("C_API - odc_GetConfigJSON")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "ConfPort", "conf.json", overrides, self);
	port->Build();

	// Provide config via ProcessElements
	Json::Value config;
	config["TestKey"] = "TestValue";
	config["Number"] = 42;
	port->ProcessElements(config);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Retrieve via C helper — returns JSON string
	const char* json = odc_GetConfigJSON(port->GetCInst());
	REQUIRE(json != nullptr);
	REQUIRE(std::string(json).find("TestKey") != std::string::npos);
	REQUIRE(std::string(json).find("TestValue") != std::string::npos);

	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_Log
// ---------------------------------------------------------------------------

TEST_CASE("C_API - odc_Log")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "LogPort", "conf.json", overrides, self);
	port->Build();
	port->Enable();

	// Log at various levels — verify no crash
	odc_Log(port->GetCInst(), 0, "trace message");
	odc_Log(port->GetCInst(), 2, "info message");
	odc_Log(port->GetCInst(), 4, "error message");
	odc_Log(port->GetCInst(), 6, nullptr); // null message — no crash

	// Null inst — no crash
	odc_Log(nullptr, 2, "no instance");

	// Wait for log posts to drain
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_msTimerCallback + odc_cancelTimer
// ---------------------------------------------------------------------------

TEST_CASE("C_API - odc_msTimerCallback fires")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "TimerPort", "conf.json", overrides, self);
	port->Build();
	port->Enable();

	bool fired = false;

	auto callback = [](uint8_t status, void* handle)
			    {
				    (void)status;
				    auto* data = static_cast<bool*>(handle);
				    *data = true;
			    };

	void* handle = odc_msTimerCallback(port->GetCInst(), 10, callback, &fired);
	REQUIRE(handle != nullptr);

	// Wait for timer to fire
	for(int i = 0; i < 200; i++)
	{
		if(fired) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(fired);

	// Handle is stale after firing — calling cancel is safe (no-op)
	odc_cancelTimer(handle);

	port->Disable();
	TestTearDown();
}

TEST_CASE("C_API - odc_cancelTimer prevents callback")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "CancelPort", "conf.json", overrides, self);
	port->Build();
	port->Enable();

	bool fired = false;

	auto callback = [](uint8_t status, void* handle)
			    {
				    (void)status;
				    auto* data = static_cast<bool*>(handle);
				    *data = true;
			    };

	void* handle = odc_msTimerCallback(port->GetCInst(), 10000, callback, &fired);
	REQUIRE(handle != nullptr);

	// Cancel immediately
	odc_cancelTimer(handle);

	// Wait a bit to ensure callback does NOT fire
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	REQUIRE(!fired);

	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  C_Transform wrapper
// ---------------------------------------------------------------------------

TEST_CASE("C_API - C_Transform wrapper")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto params = Json::Value(Json::objectValue);
	auto tx = std::make_shared<odc::C_Transform>("TestTransform", params, self);
	REQUIRE(tx != nullptr);

	tx->Enable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	tx->Disable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	TestTearDown();
}

// ---------------------------------------------------------------------------
//  C_UI wrapper
// ---------------------------------------------------------------------------

TEST_CASE("C_API - C_UI wrapper")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto ui = std::make_shared<odc::C_UI>("TestUI", "conf.json", overrides, self);
	REQUIRE(ui != nullptr);

	ui->Build();
	ui->Enable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	ui->Disable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	TestTearDown();
}
