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
 * GoModbusEndToEndTest.cpp
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

// Go's runtime needs to own both POSIX signals and Windows SEH/VEH.
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_NO_WINDOWS_SEH

#include "TestHelpers.h"
#include <catch.hpp>

#define SUITE(name) "GoModbusEndToEndTests - " name

// ---------------------------------------------------------------------------
// End-to-End: GoModbusClient + GoModbusServer running together
//
// Architecture recap
// ------------------
//  GoModbusServer: listens for TCP connections; its data store is updated by
//    inbound ODC events (from the test); Modbus reads return stored values;
//    Modbus writes trigger outbound ODC events to subscribers.
//
//  GoModbusClient: connects to GoModbusServer; polls configured registers
//    and publishes ODC events; translates inbound ODC control events into
//    Modbus write commands.
//
// Timing note
// -----------
//  GoModbusClientPort applies an initial reconnect delay of 1 second before
//  it first attempts to connect.  All E2E tests that require the client to
//  be connected use a 5-second WaitForMatch() window, which is well within
//  typical CI budgets while still allowing for slow machines.
//
// Type names
// ----------
//  "GoModbusServer" — server port type (same libGoModbusPort.so)
//  "GoModbusClient" — client port type (polling + controls)
//
// Object lifetime order
// ---------------------
//  To mirror the DataConcentrator shutdown sequence (ios_working.reset() +
//  pIOS->run() drains ALL pending io_context work, THEN DataPorts.clear()
//  destroys ports), each test declares ports and capture objects BEFORE the
//  ThreadPool so that ThreadPool is destroyed FIRST.  ~ThreadPool() releases
//  the work guard and calls pIOS->run() — draining all lambdas while every
//  port and subscriber is still alive — then joins its threads.  After that,
//  ports and captures are destroyed safely.
// ---------------------------------------------------------------------------

// Helper: merge a point-arrays Json::Value into a base client config.
static Json::Value ClientConfigWithPoints(int port, int pollRateMs,
	const Json::Value& points)
{
	Json::Value conf = MakeClientConfig(port, pollRateMs);
	for(const auto& k : points.getMemberNames())
		conf[k] = points[k];
	return conf;
}

// ---------------------------------------------------------------------------
// Poll direction: ODC event → server store → client poll → ODC event
// ---------------------------------------------------------------------------

// Inject Binary(true) into the server's coil store; the client polls the coil
// and must publish Binary(index=0, value=true) to its subscriber.
TEST_CASE(SUITE("ClientPolls_CoilFromServer"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
	REQUIRE(portlib != nullptr);
	{
		const int port = 1502;

		// Server: coil 0 ↔ ODC Binary index 0.
		Json::Value srvPts;
		Json::Value sBin;
		sBin["Index"]            = 0;
		sBin["Modbus"]["Type"]    = "Coil";
		sBin["Modbus"]["Address"] = 0;
		srvPts["Binaries"].append(sBin);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "E2E_SrvCoil",
			"", MakeServerConfig(port, srvPts), portlib);

		// Client: same coil mapping, fast poll rate.
		Json::Value cliPts;
		Json::Value cBin;
		cBin["Index"]            = 0;
		cBin["Modbus"]["Type"]    = "Coil";
		cBin["Modbus"]["Address"] = 0;
		cliPts["Binaries"].append(cBin);

		auto capture = std::make_shared<EventCapturePort>("E2E_CaptureCoil");
		auto cli = std::make_shared<odc::C_Port>("GoModbusClient", "E2E_CliCoil",
			"", ClientConfigWithPoints(port, 200, cliPts), portlib);

		// Pool declared last → destroyed first: ~ThreadPool() drains pIOS while
		// srv, capture, cli are still alive (mirrors DataConcentrator shutdown).
		ThreadPool pool(1);

		REQUIRE(srv != nullptr);
		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Pre-populate the store before the client connects.
		auto binEvent = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 0);
		binEvent->SetPayload<odc::EventType::Binary>(true);
		REQUIRE(SendEvent(*srv, binEvent) == odc::CommandStatus::SUCCESS);

		REQUIRE(cli != nullptr);
		SubscribeAndEstablishDemand(*cli, capture.get());
		cli->Build();
		cli->Enable();

		// ConnectState(CONNECTED) arrives at ~1 s; WaitForMatch skips it and
		// waits specifically for the polled Binary(index=0, value=true).
		REQUIRE(capture->WaitForMatch(
			[](const odc::EventInfo& e)
			{
				return e.GetEventType() == odc::EventType::Binary
				       && e.GetIndex() == 0
				       && e.GetPayload<odc::EventType::Binary>();
			},
			std::chrono::milliseconds(5000)));

		cli->Disable();
		srv->Disable();
		// Destruction order: pool (drains pIOS) → cli → capture → srv.
	}
	TestTearDown();
}

// Inject Analog(42.0) into the server's holding-register store; the client
// polls the register and must publish Analog(index=0, value=42.0).
TEST_CASE(SUITE("ClientPolls_AnalogFromServer"))
{
	TestSetup();
	{
		auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
		REQUIRE(portlib != nullptr);

		const int port = 1502;

		// Server: HR 0 ↔ ODC Analog index 0, Int16.
		Json::Value srvPts;
		Json::Value sAna;
		sAna["Index"]            = 0;
		sAna["Modbus"]["Type"]    = "HoldingRegister";
		sAna["Modbus"]["Address"] = 0;
		sAna["DataType"]          = "Int16";
		srvPts["Analogs"].append(sAna);

		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "E2E_SrvAnalog",
			"", MakeServerConfig(port, srvPts), portlib);

		// Client: same HR mapping.
		Json::Value cAna;
		cAna["Index"]            = 0;
		cAna["Modbus"]["Type"]    = "HoldingRegister";
		cAna["Modbus"]["Address"] = 0;
		cAna["DataType"]          = "Int16";

		auto capture = std::make_shared<EventCapturePort>("E2E_CaptureAnalog");
		auto cli = std::make_shared<odc::C_Port>("GoModbusClient", "E2E_CliAnalog",
			"", ClientConfigWithPoints(port, 200, [&]{ Json::Value p; p["Analogs"].append(cAna); return p; }()), portlib);

		ThreadPool pool(1);

		REQUIRE(srv != nullptr);
		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		auto anaEvent = std::make_shared<odc::EventInfo>(odc::EventType::Analog, 0);
		anaEvent->SetPayload<odc::EventType::Analog>(42.0);
		REQUIRE(SendEvent(*srv, anaEvent) == odc::CommandStatus::SUCCESS);

		REQUIRE(cli != nullptr);
		SubscribeAndEstablishDemand(*cli, capture.get());
		cli->Build();
		cli->Enable();

		REQUIRE(capture->WaitForMatch(
			[](const odc::EventInfo& e)
			{
				return e.GetEventType() == odc::EventType::Analog
				       && e.GetIndex() == 0
				       && e.GetPayload<odc::EventType::Analog>() == 42.0;
			},
			std::chrono::milliseconds(5000)));

		cli->Disable();
		srv->Disable();
	}
	TestTearDown();
}

// ---------------------------------------------------------------------------
// Control direction: ODC control → client write → server publish
// ---------------------------------------------------------------------------

// The client receives CROB LATCH_ON at index 3 and writes coil 100 to the
// server.  The server's BinaryControls mapping publishes Binary(index=3,
// value=true) to its subscriber.
TEST_CASE(SUITE("ClientControl_WritesCoil_ServerPublishes"))
{
	TestSetup();
	{
		auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
		REQUIRE(portlib != nullptr);

		const int port = 1502;

		// Server: coil 100 writes → ODC Binary index 3.
		Json::Value srvPts;
		Json::Value sCtl;
		sCtl["Index"]            = 3;
		sCtl["Modbus"]["Type"]    = "Coil";
		sCtl["Modbus"]["Address"] = 100;
		srvPts["BinaryControls"].append(sCtl);

		auto srvCapture = std::make_shared<EventCapturePort>("E2E_SrvCoilCapture");
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "E2E_SrvCtl",
			"", MakeServerConfig(port, srvPts), portlib);

		// Client: coil 100 BinaryControl at index 3 (slow poll — only controls matter here).
		Json::Value cCtl;
		cCtl["Index"]            = 3;
		cCtl["Modbus"]["Type"]    = "Coil";
		cCtl["Modbus"]["Address"] = 100;

		auto cli = std::make_shared<odc::C_Port>("GoModbusClient", "E2E_CliCtl",
			"", ClientConfigWithPoints(port, 5000, [&]{ Json::Value p; p["BinaryControls"].append(cCtl); return p; }()), portlib);

		ThreadPool pool(1);

		REQUIRE(srv != nullptr);
		SubscribeAndEstablishDemand(*srv, srvCapture.get());
		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		REQUIRE(cli != nullptr);
		cli->Build();
		cli->Enable();

		// Wait for the client to connect before sending a control event.
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));

		auto crob = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, 3);
		odc::ControlRelayOutputBlock blk;
		blk.functionCode = odc::ControlCode::LATCH_ON;
		crob->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(blk));

		// Callback status may be SUCCESS or HARDWARE_ERROR depending on timing;
		// what matters is that it is not UNDEFINED (i.e. it fired).
		REQUIRE(SendEvent(*cli, crob, std::chrono::milliseconds(3000))
			!= odc::CommandStatus::UNDEFINED);

		// Server must publish Binary(index=3, value=true) to its subscriber.
		REQUIRE(srvCapture->WaitFor(1, std::chrono::milliseconds(3000)));
		auto e = srvCapture->FirstOfType(odc::EventType::Binary);
		REQUIRE(e != nullptr);
		REQUIRE(e->GetIndex() == 3);
		REQUIRE(e->GetPayload<odc::EventType::Binary>() == true);

		cli->Disable();
		srv->Disable();
	}
	TestTearDown();
}

// The client receives AnalogOutputInt16(value=99) at index 5 and writes HR 200
// to the server.  The server's AnalogControls mapping publishes
// AnalogOutputInt16(index=5, value=99) to its subscriber.
TEST_CASE(SUITE("ClientControl_WritesRegister_ServerPublishes"))
{
	TestSetup();
	{
		auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
		REQUIRE(portlib != nullptr);

		const int port = 1502;

		// Server: HR 200 writes → ODC AnalogOutputInt16 index 5.
		Json::Value srvPts;
		Json::Value sACtl;
		sACtl["Index"]            = 5;
		sACtl["Modbus"]["Type"]    = "HoldingRegister";
		sACtl["Modbus"]["Address"] = 200;
		sACtl["ControlType"]       = "AnalogOutputInt16";
		srvPts["AnalogControls"].append(sACtl);

		auto srvCapture = std::make_shared<EventCapturePort>("E2E_SrvRegCapture");
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "E2E_SrvACtl",
			"", MakeServerConfig(port, srvPts), portlib);

		// Client: HR 200 AnalogControl at index 5.
		Json::Value cACtl;
		cACtl["Index"]            = 5;
		cACtl["Modbus"]["Type"]    = "HoldingRegister";
		cACtl["Modbus"]["Address"] = 200;
		cACtl["ControlType"]       = "AnalogOutputInt16";

		auto cli = std::make_shared<odc::C_Port>("GoModbusClient", "E2E_CliACtl",
			"", ClientConfigWithPoints(port, 5000, [&]{ Json::Value p; p["AnalogControls"].append(cACtl); return p; }()), portlib);

		ThreadPool pool(1);

		REQUIRE(srv != nullptr);
		SubscribeAndEstablishDemand(*srv, srvCapture.get());
		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		REQUIRE(cli != nullptr);
		cli->Build();
		cli->Enable();

		std::this_thread::sleep_for(std::chrono::milliseconds(1500));

		auto aoEvent = std::make_shared<odc::EventInfo>(odc::EventType::AnalogOutputInt16, 5);
		odc::AO16 ao16{static_cast<int16_t>(99), odc::CommandStatus::SUCCESS};
		aoEvent->SetPayload<odc::EventType::AnalogOutputInt16>(std::move(ao16));

		REQUIRE(SendEvent(*cli, aoEvent, std::chrono::milliseconds(3000))
			!= odc::CommandStatus::UNDEFINED);

		// Server must publish AnalogOutputInt16(index=5, value=99).
		REQUIRE(srvCapture->WaitFor(1, std::chrono::milliseconds(3000)));
		auto e = srvCapture->FirstOfType(odc::EventType::AnalogOutputInt16);
		REQUIRE(e != nullptr);
		REQUIRE(e->GetIndex() == 5);
		REQUIRE(e->GetPayload<odc::EventType::AnalogOutputInt16>().first
			== static_cast<int16_t>(99));

		cli->Disable();
		srv->Disable();
	}
	TestTearDown();
}

// ---------------------------------------------------------------------------
// Two-way: same coil address mapped for both read and write
// ---------------------------------------------------------------------------

// The server maps coil 50 both ways: Binaries (ODC→store, client reads) and
// BinaryControls (client write→ODC, server publishes).  The client maps the
// same coil as both a poll point and a control point.
//
// Part 1 — ODC→poll:  Inject Binary(true) into the server; the client polls
//   and publishes Binary(true) to its subscriber.
// Part 2 — control→ODC:  Send LATCH_OFF via the client; the server receives
//   the Modbus write and publishes Binary(false) to its subscriber.
TEST_CASE(SUITE("TwoWayCoil"))
{
	TestSetup();
	{
		auto portlib = LoadModule(GetLibFileName("GoModbusPort"));
		REQUIRE(portlib != nullptr);

		const int port = 1502;

		// Server: coil 50 ↔ ODC Binary index 0 in both directions.
		Json::Value srvPts;

		Json::Value sBin;
		sBin["Index"]            = 0;
		sBin["Modbus"]["Type"]    = "Coil";
		sBin["Modbus"]["Address"] = 50;
		srvPts["Binaries"].append(sBin);

		Json::Value sBCtl;
		sBCtl["Index"]            = 0;
		sBCtl["Modbus"]["Type"]    = "Coil";
		sBCtl["Modbus"]["Address"] = 50;
		srvPts["BinaryControls"].append(sBCtl);

		auto srvCapture = std::make_shared<EventCapturePort>("E2E_2W_SrvCapture");
		auto srv = std::make_shared<odc::C_Port>("GoModbusServer", "E2E_2W_Srv",
			"", MakeServerConfig(port, srvPts), portlib);

		// Client: coil 50 as both poll and control.
		Json::Value cBin;
		cBin["Index"]            = 0;
		cBin["Modbus"]["Type"]    = "Coil";
		cBin["Modbus"]["Address"] = 50;

		Json::Value cBCtl;
		cBCtl["Index"]            = 0;
		cBCtl["Modbus"]["Type"]    = "Coil";
		cBCtl["Modbus"]["Address"] = 50;

		Json::Value cliPts;
		cliPts["Binaries"].append(cBin);
		cliPts["BinaryControls"].append(cBCtl);

		auto cliCapture = std::make_shared<EventCapturePort>("E2E_2W_CliCapture");
		auto cli = std::make_shared<odc::C_Port>("GoModbusClient", "E2E_2W_Cli",
			"", ClientConfigWithPoints(port, 200, cliPts), portlib);

		ThreadPool pool(1);

		REQUIRE(srv != nullptr);
		SubscribeAndEstablishDemand(*srv, srvCapture.get());
		srv->Build();
		srv->Enable();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		REQUIRE(cli != nullptr);
		SubscribeAndEstablishDemand(*cli, cliCapture.get());
		cli->Build();
		cli->Enable();

		// --- Part 1: ODC → server store → client poll ---
		auto setTrue = std::make_shared<odc::EventInfo>(odc::EventType::Binary, 0);
		setTrue->SetPayload<odc::EventType::Binary>(true);
		REQUIRE(SendEvent(*srv, setTrue) == odc::CommandStatus::SUCCESS);

		REQUIRE(cliCapture->WaitForMatch(
			[](const odc::EventInfo& e)
			{
				return e.GetEventType() == odc::EventType::Binary
				       && e.GetIndex() == 0
				       && e.GetPayload<odc::EventType::Binary>();
			},
			std::chrono::milliseconds(5000)));

		// --- Part 2: client control → Modbus write → server publishes ---
		// By the time part 1 succeeded the client is definitely connected.
		auto crob = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, 0);
		odc::ControlRelayOutputBlock blk;
		blk.functionCode = odc::ControlCode::LATCH_OFF;
		crob->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(blk));

		REQUIRE(SendEvent(*cli, crob, std::chrono::milliseconds(3000))
			!= odc::CommandStatus::UNDEFINED);

		// Server must publish Binary(index=0, value=false).
		REQUIRE(srvCapture->WaitFor(1, std::chrono::milliseconds(3000)));
		auto srvEvent = srvCapture->FirstOfType(odc::EventType::Binary);
		REQUIRE(srvEvent != nullptr);
		REQUIRE(srvEvent->GetIndex() == 0);
		REQUIRE(srvEvent->GetPayload<odc::EventType::Binary>() == false);

		cli->Disable();
		srv->Disable();
		// Destruction order: pool (drains pIOS) → cli → cliCapture → srv → srvCapture.
	}
	TestTearDown();
}
