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

#include <spdlog/common.h>

extern spdlog::level::level_enum log_level;

// ---------------------------------------------------------------------------
//  Compile-time alignment checks between C API types and IOTypes.h
//  If any of these fail, the internal representation has changed and the
//  C API must be updated to match.
// ---------------------------------------------------------------------------

// --- EventType ---
static_assert(static_cast<uint8_t>(odc::EventType::Binary) == C_EventType_Binary, "");
static_assert(static_cast<uint8_t>(odc::EventType::DoubleBitBinary) == C_EventType_DoubleBitBinary, "");
static_assert(static_cast<uint8_t>(odc::EventType::Analog) == C_EventType_Analog, "");
static_assert(static_cast<uint8_t>(odc::EventType::Counter) == C_EventType_Counter, "");
static_assert(static_cast<uint8_t>(odc::EventType::FrozenCounter) == C_EventType_FrozenCounter, "");
static_assert(static_cast<uint8_t>(odc::EventType::BinaryOutputStatus) == C_EventType_BinaryOutputStatus, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputStatus) == C_EventType_AnalogOutputStatus, "");
static_assert(static_cast<uint8_t>(odc::EventType::BinaryCommandEvent) == C_EventType_BinaryCommandEvent, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogCommandEvent) == C_EventType_AnalogCommandEvent, "");
static_assert(static_cast<uint8_t>(odc::EventType::OctetString) == C_EventType_OctetString, "");
static_assert(static_cast<uint8_t>(odc::EventType::TimeAndInterval) == C_EventType_TimeAndInterval, "");
static_assert(static_cast<uint8_t>(odc::EventType::SecurityStat) == C_EventType_SecurityStat, "");
static_assert(static_cast<uint8_t>(odc::EventType::ControlRelayOutputBlock) == C_EventType_ControlRelayOutputBlock, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputInt16) == C_EventType_AnalogOutputInt16, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputInt32) == C_EventType_AnalogOutputInt32, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputFloat32) == C_EventType_AnalogOutputFloat32, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputDouble64) == C_EventType_AnalogOutputDouble64, "");
static_assert(static_cast<uint8_t>(odc::EventType::TimeSync) == C_EventType_TimeSync, "");
static_assert(static_cast<uint8_t>(odc::EventType::BinaryQuality) == C_EventType_BinaryQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::DoubleBitBinaryQuality) == C_EventType_DoubleBitBinaryQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogQuality) == C_EventType_AnalogQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::CounterQuality) == C_EventType_CounterQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::BinaryOutputStatusQuality) == C_EventType_BinaryOutputStatusQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::FrozenCounterQuality) == C_EventType_FrozenCounterQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::AnalogOutputStatusQuality) == C_EventType_AnalogOutputStatusQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::OctetStringQuality) == C_EventType_OctetStringQuality, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileAuth) == C_EventType_FileAuth, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileCommand) == C_EventType_FileCommand, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileCommandStatus) == C_EventType_FileCommandStatus, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileTransport) == C_EventType_FileTransport, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileTransportStatus) == C_EventType_FileTransportStatus, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileDescriptor) == C_EventType_FileDescriptor, "");
static_assert(static_cast<uint8_t>(odc::EventType::FileSpecString) == C_EventType_FileSpecString, "");
static_assert(static_cast<uint8_t>(odc::EventType::ConnectState) == C_EventType_ConnectState, "");

// --- CommandStatus ---
static_assert(static_cast<uint8_t>(odc::CommandStatus::SUCCESS) == C_CommandStatus_SUCCESS, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::TIMEOUT) == C_CommandStatus_TIMEOUT, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::NO_SELECT) == C_CommandStatus_NO_SELECT, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::FORMAT_ERROR) == C_CommandStatus_FORMAT_ERROR, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::NOT_SUPPORTED) == C_CommandStatus_NOT_SUPPORTED, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::ALREADY_ACTIVE) == C_CommandStatus_ALREADY_ACTIVE, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::HARDWARE_ERROR) == C_CommandStatus_HARDWARE_ERROR, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::LOCAL) == C_CommandStatus_LOCAL, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::TOO_MANY_OPS) == C_CommandStatus_TOO_MANY_OPS, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::NOT_AUTHORIZED) == C_CommandStatus_NOT_AUTHORIZED, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::AUTOMATION_INHIBIT) == C_CommandStatus_AUTOMATION_INHIBIT, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::PROCESSING_LIMITED) == C_CommandStatus_PROCESSING_LIMITED, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::OUT_OF_RANGE) == C_CommandStatus_OUT_OF_RANGE, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::DOWNSTREAM_LOCAL) == C_CommandStatus_DOWNSTREAM_LOCAL, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::ALREADY_COMPLETE) == C_CommandStatus_ALREADY_COMPLETE, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::BLOCKED) == C_CommandStatus_BLOCKED, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::CANCELLED) == C_CommandStatus_CANCELLED, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::BLOCKED_OTHER_MASTER) == C_CommandStatus_BLOCKED_OTHER_MASTER, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::DOWNSTREAM_FAIL) == C_CommandStatus_DOWNSTREAM_FAIL, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::NON_PARTICIPATING) == C_CommandStatus_NON_PARTICIPATING, "");
static_assert(static_cast<uint8_t>(odc::CommandStatus::UNDEFINED) == C_CommandStatus_UNDEFINED, "");

// --- ControlCode ---
static_assert(static_cast<uint8_t>(odc::ControlCode::NUL) == C_ControlCode_NUL, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::PULSE_ON) == C_ControlCode_PULSE_ON, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::PULSE_OFF) == C_ControlCode_PULSE_OFF, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::LATCH_ON) == C_ControlCode_LATCH_ON, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::LATCH_OFF) == C_ControlCode_LATCH_OFF, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::CLOSE_PULSE_ON) == C_ControlCode_CLOSE_PULSE_ON, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::TRIP_PULSE_ON) == C_ControlCode_TRIP_PULSE_ON, "");
static_assert(static_cast<uint8_t>(odc::ControlCode::UNDEFINED) == C_ControlCode_UNDEFINED, "");

// --- ConnectState ---
static_assert(static_cast<int>(odc::ConnectState::PORT_UP) == C_ConnectState_PORT_UP, "");
static_assert(static_cast<int>(odc::ConnectState::CONNECTED) == C_ConnectState_CONNECTED, "");
static_assert(static_cast<int>(odc::ConnectState::DISCONNECTED) == C_ConnectState_DISCONNECTED, "");
static_assert(static_cast<int>(odc::ConnectState::PORT_DOWN) == C_ConnectState_PORT_DOWN, "");
static_assert(static_cast<int>(odc::ConnectState::UNDEFINED) == C_ConnectState_UNDEFINED, "");

// --- QualityFlags ---
static_assert(static_cast<uint16_t>(odc::QualityFlags::NONE) == C_QualityFlags_NONE, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::ONLINE) == C_QualityFlags_ONLINE, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::RESTART) == C_QualityFlags_RESTART, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::COMM_LOST) == C_QualityFlags_COMM_LOST, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::REMOTE_FORCED) == C_QualityFlags_REMOTE_FORCED, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::LOCAL_FORCED) == C_QualityFlags_LOCAL_FORCED, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::OVERRANGE) == C_QualityFlags_OVERRANGE, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::REFERENCE_ERR) == C_QualityFlags_REFERENCE_ERR, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::ROLLOVER) == C_QualityFlags_ROLLOVER, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::DISCONTINUITY) == C_QualityFlags_DISCONTINUITY, "");
static_assert(static_cast<uint16_t>(odc::QualityFlags::CHATTER_FILTER) == C_QualityFlags_CHATTER_FILTER, "");

// --- C_ControlRelayOutputBlock layout matches ControlRelayOutputBlock ---
static_assert(sizeof(C_ControlRelayOutputBlock) == sizeof(odc::ControlRelayOutputBlock), "");
static_assert(alignof(C_ControlRelayOutputBlock) == alignof(odc::ControlRelayOutputBlock), "");
static_assert(offsetof(C_ControlRelayOutputBlock, function_code) == offsetof(odc::ControlRelayOutputBlock, functionCode), "");
static_assert(offsetof(C_ControlRelayOutputBlock, count) == offsetof(odc::ControlRelayOutputBlock, count), "");
static_assert(offsetof(C_ControlRelayOutputBlock, on_time_ms) == offsetof(odc::ControlRelayOutputBlock, onTimeMS), "");
static_assert(offsetof(C_ControlRelayOutputBlock, off_time_ms) == offsetof(odc::ControlRelayOutputBlock, offTimeMS), "");
static_assert(offsetof(C_ControlRelayOutputBlock, status) == offsetof(odc::ControlRelayOutputBlock, status), "");

// --- C_Payload alignment is sufficient for largest scalar ---
static_assert(alignof(C_Payload) >= alignof(double), "");

// --- Log level #defines match spdlog level enum values ---
static_assert(SPDLOG_LEVEL_TRACE    == C_LOG_LEVEL_TRACE, "");
static_assert(SPDLOG_LEVEL_DEBUG    == C_LOG_LEVEL_DEBUG, "");
static_assert(SPDLOG_LEVEL_INFO     == C_LOG_LEVEL_INFO, "");
static_assert(SPDLOG_LEVEL_WARN     == C_LOG_LEVEL_WARN, "");
static_assert(SPDLOG_LEVEL_ERROR    == C_LOG_LEVEL_ERROR, "");
static_assert(SPDLOG_LEVEL_CRITICAL == C_LOG_LEVEL_CRITICAL, "");
static_assert(SPDLOG_LEVEL_OFF      == C_LOG_LEVEL_OFF, "");

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
	// Per-type logger for mock C ports (matches Type+"Port")
	auto pMockLogger = std::make_shared<spdlog::logger>("MockCPort", console_sink);
	pMockLogger->set_level(log_level);
	odc::spdlog_register_logger(pMockLogger);
	// Per-type loggers for mock C transforms (Type, no suffix)
	auto pTxLogger = std::make_shared<spdlog::logger>("TestTxType", console_sink);
	pTxLogger->set_level(log_level);
	odc::spdlog_register_logger(pTxLogger);
	// Per-type logger for mock C UI plugins (Type, no suffix)
	auto pUILogger = std::make_shared<spdlog::logger>("TestUIType", console_sink);
	pUILogger->set_level(log_level);
	odc::spdlog_register_logger(pUILogger);
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
	g_host_api.publish_event(pub->GetCInst(), &cevt, callback, &cb_fired);

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
	g_host_api.publish_event(pub->GetCInst(), &cevt, nullptr, nullptr);

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

	g_host_api.publish_connect_state(port->GetCInst(), C_ConnectState_CONNECTED);
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
	const char* json = g_host_api.get_config_json(port->GetCInst());
	REQUIRE(json != nullptr);
	REQUIRE(std::string(json).find("TestKey") != std::string::npos);
	REQUIRE(std::string(json).find("TestValue") != std::string::npos);

	port->Disable();
	TestTearDown();
}

// ---------------------------------------------------------------------------
//  odc_Log
// ---------------------------------------------------------------------------

TEST_CASE("C_API - g_host_api log and should_log")
{
	TestSetup();
	ThreadPool pool;

	auto self = GetCurrentModule();
	REQUIRE(self != nullptr);

	auto overrides = Json::Value(Json::objectValue);
	auto port = std::make_shared<odc::C_Port>("MockC", "LogPort", "conf.json", overrides, self);
	port->Build();
	port->Enable();
	auto cinst = port->GetCInst();

	// Transform and UI instances
	auto tx = std::make_shared<odc::C_Transform>("TestTxType", "LogTx", Json::objectValue, self);
	tx->Enable();
	auto ux = std::make_shared<odc::C_UI>("TestUIType", "LogUI", "conf.json", overrides, self);
	ux->Build();
	ux->Enable();

	// log at various levels — verify no crash (port, transform, UI)
	g_host_api.log(cinst, C_LOG_LEVEL_TRACE, "trace message");
	g_host_api.log(cinst, C_LOG_LEVEL_INFO, "info message");
	g_host_api.log(cinst, C_LOG_LEVEL_ERROR, "error message");
	g_host_api.log(cinst, C_LOG_LEVEL_OFF, nullptr); // null message — no crash
	g_host_api.log(tx->GetCInst(), C_LOG_LEVEL_ERROR, "tx error");
	g_host_api.log(ux->GetCInst(), C_LOG_LEVEL_ERROR, "ui error");

	// Null inst — no crash
	g_host_api.log(nullptr, C_LOG_LEVEL_INFO, "no instance");

	// should_log on all three types
	if(log_level <= spdlog::level::err)
	{
		REQUIRE(g_host_api.should_log(cinst, C_LOG_LEVEL_ERROR));
		REQUIRE(g_host_api.should_log(cinst, C_LOG_LEVEL_CRITICAL));
		REQUIRE(g_host_api.should_log(tx->GetCInst(), C_LOG_LEVEL_ERROR));
		REQUIRE(g_host_api.should_log(ux->GetCInst(), C_LOG_LEVEL_ERROR));
	}
	REQUIRE(!g_host_api.should_log(cinst, C_LOG_LEVEL_TRACE));
	REQUIRE(!g_host_api.should_log(nullptr, C_LOG_LEVEL_INFO));

	// Log via vtable at each level
	g_host_api.log(cinst, C_LOG_LEVEL_TRACE,    "trace via vtable");
	g_host_api.log(cinst, C_LOG_LEVEL_DEBUG,    "debug via vtable");
	g_host_api.log(cinst, C_LOG_LEVEL_INFO,     "info via vtable");
	g_host_api.log(cinst, C_LOG_LEVEL_WARN,     "warn via vtable");
	g_host_api.log(cinst, C_LOG_LEVEL_ERROR,    "error via vtable");
	g_host_api.log(cinst, C_LOG_LEVEL_CRITICAL, "critical via vtable");

	// Wait for log posts to drain
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	port->Disable();
	tx->Disable();
	ux->Disable();
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

	void* handle = g_host_api.ms_timer_callback(port->GetCInst(), 10, callback, &fired);
	REQUIRE(handle != nullptr);

	// Wait for timer to fire
	for(int i = 0; i < 200; i++)
	{
		if(fired) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(fired);

	// Handle is stale after firing — calling cancel is safe (no-op)
	g_host_api.cancel_timer(handle);

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

	void* handle = g_host_api.ms_timer_callback(port->GetCInst(), 10000, callback, &fired);
	REQUIRE(handle != nullptr);

	// Cancel immediately
	g_host_api.cancel_timer(handle);

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
	auto tx = std::make_shared<odc::C_Transform>("TestTxType", "TestTransform", params, self);
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
	auto ui = std::make_shared<odc::C_UI>("TestUIType", "TestUI", "conf.json", overrides, self);
	REQUIRE(ui != nullptr);

	ui->Build();
	ui->Enable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	ui->Disable();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	TestTearDown();
}
