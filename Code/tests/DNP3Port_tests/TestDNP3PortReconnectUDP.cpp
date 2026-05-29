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
 * TestDNP3PortReconnectUDP.cpp
 *
 *  Created on: 28/05/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "TestDNP3Helpers.h"
#include "../PortLoader.h"
#include "../ThreadPool.h"
#include <catch.hpp>
#include <opendatacon/asio.h>
#include <thread>
#include <random>
#include <atomic>
#include <vector>
#include <memory>

#define SUITE(name) "DNP3PortReconnectUDP - " name

// ============================================================================
// EvilRemote — sits on the remote endpoint, receives DNP3 packets, sends
// empty datagram replies (to trigger UDPSocketChannel connect()), then
// closes the socket to generate ICMP port unreachable on the DNP3 side.
//
// Duty cycle (10ms period):
//   Phase LISTEN  — random 1–10ms: socket open, receives & replies
//   Phase CLOSED  — remainder:     socket closed → ICMP on DNP3 sends
// ============================================================================
class EvilRemote
{
public:
	EvilRemote(uint16_t port);
	~EvilRemote();

private:
	void StartListenPhase();
	void StartClosePhase();
	void StartReceive();
	void ReceiveHandler(std::error_code ec, size_t num);

	std::shared_ptr<odc::asio_service> ios;
	std::unique_ptr<asio::io_service::strand> strand;
	std::unique_ptr<asio::ip::udp::socket> sock;
	std::unique_ptr<asio::steady_timer> phase_timer;
	asio::ip::udp::endpoint local_ep;
	asio::ip::udp::endpoint peer_ep;
	std::vector<char> buf;
	std::mt19937 rng;
	std::uniform_int_distribution<int> duty_dist{1, 10};
	static constexpr int CYCLE_MS = 10;
	std::atomic<bool> shutting_down = false;
	int current_listen_ms = 0;
};

EvilRemote::EvilRemote(uint16_t port):
	ios(odc::asio_service::Get()),
	strand(ios->make_strand()),
	sock(ios->make_udp_socket()),
	phase_timer(ios->make_steady_timer()),
	local_ep(asio::ip::address_v4::loopback(), port),
	buf(65536),
	rng(std::random_device{}())
{
	StartListenPhase();
}

EvilRemote::~EvilRemote()
{
	shutting_down = true;
	asio::error_code ec;
	phase_timer->cancel(ec);
	sock->cancel(ec);
	sock->close(ec);
}

void EvilRemote::StartListenPhase()
{
	if(shutting_down)
		return;

	current_listen_ms = duty_dist(rng);

	asio::error_code ec;
	sock->open(asio::ip::udp::v4(), ec);
	if(!ec)
		sock->bind(local_ep, ec);

	if(ec)
	{
		auto log = spdlog::get("DNP3Port");
		if(log)
			log->error("[EvilRemote] Failed to bind: {}", ec.message());
	}

	StartReceive();

	phase_timer->expires_after(std::chrono::milliseconds(current_listen_ms));
	phase_timer->async_wait(strand->wrap([this](std::error_code ec)
		{
			if(ec || shutting_down)
				return;
			StartClosePhase();
		}));
}

void EvilRemote::StartClosePhase()
{
	if(shutting_down)
		return;

	asio::error_code ec;
	sock->close(ec);

	int close_ms = CYCLE_MS - current_listen_ms;
	if(close_ms < 1)
		close_ms = 1;

	phase_timer->expires_after(std::chrono::milliseconds(close_ms));
	phase_timer->async_wait(strand->wrap([this](std::error_code ec)
		{
			if(ec || shutting_down)
				return;
			StartListenPhase();
		}));
}

void EvilRemote::StartReceive()
{
	if(shutting_down)
		return;

	sock->async_receive_from(asio::buffer(buf), peer_ep,
		strand->wrap([this](std::error_code ec, size_t num)
			{
				ReceiveHandler(ec, num);
			}));
}

void EvilRemote::ReceiveHandler(std::error_code ec, size_t num)
{
	if(shutting_down || ec)
		return;

	// Send an empty datagram back to trigger the DNP3 side's lazy-connect
	auto reply = std::make_shared<std::vector<char>>();
	sock->async_send_to(asio::buffer(reply->data(), reply->size()),
		peer_ep,
		strand->wrap([reply](std::error_code, size_t) {}));

	// Start the next receive
	StartReceive();
}

// ============================================================================
// Config helper
// ============================================================================
inline Json::Value MakeStressConf(uint16_t listen_port, uint16_t remote_port)
{
	Json::Value conf;
	conf["MasterAddr"] = 0;
	conf["OutstationAddr"] = 1;
	conf["ServerType"] = "PERSISTENT";
	conf["IPTransport"] = "UDP";
	conf["IP"] = "127.0.0.1";
	conf["UDPListenPort"] = Json::UInt(listen_port);
	conf["Port"] = Json::UInt(remote_port);

	// Aggressively short timeouts for maximum reconnect rate
	conf["LinkKeepAlivems"] = 50;
	conf["LinkTimeoutms"] = 25;
	conf["IPConnectRetryPeriodMinms"] = 10;
	conf["IPConnectRetryPeriodMaxms"] = 100;

	// Lower app-layer timeouts from 5s default
	conf["MasterResponseTimeoutms"] = 100;
	conf["TaskRetryPeriodms"] = 100;
	conf["SolConfirmTimeoutms"] = 100;
	conf["UnsolConfirmTimeoutms"] = 100;

	// Unsol for chattiness
	conf["EnableUnsol"] = true;
	conf["UnsolClass1"] = true;
	conf["UnsolClass2"] = true;
	conf["UnsolClass3"] = true;
	conf["DisableUnsolOnStartup"] = true;

	return conf;
}

// ============================================================================
// Reconnect monitoring — polls numClose, fails if stalled for ~1s
// ============================================================================
inline void MonitorReconnect(std::shared_ptr<DataPort> port, unsigned int duration_ms)
{
	const auto deadline = std::chrono::steady_clock::now()
	                      + std::chrono::milliseconds(duration_ms);
	const size_t MAX_STALLED = 10; // 10 polls × 100ms = 1s stall threshold

	size_t last_close = 0;
	size_t stalled = 0;

	while(std::chrono::steady_clock::now() < deadline)
	{
		auto stats = port->GetStatistics();
		size_t cur_close = stats["channel"]["numClose"].asUInt();

		if(cur_close == last_close)
		{
			stalled++;
			if(stalled > MAX_STALLED)
			{
				UNSCOPED_INFO("Stalled at " << cur_close << " channel closes");
				FAIL("Reconnect state machine stalled");
			}
		}
		else
		{
			stalled = 0;
			last_close = cur_close;
		}

		auto status = port->GetStatus()["Result"].asString();
		REQUIRE(status != "Port disabled");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	UNSCOPED_INFO("Total channel closes: " << last_close);
	REQUIRE(last_close > 10);
}

// ============================================================================
// Test cases
// ============================================================================
/*
TEST_CASE(SUITE("Master reconnect stress"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		ThreadPool thread_pool(1);
		// EvilRemote after ThreadPool so it's destroyed first (cancels async ops)
		EvilRemote evil(20100);

		newptr newMaster = GetPortCreator(portlib, "DNP3Master");
		REQUIRE(newMaster);
		delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
		REQUIRE(delMaster);

		auto conf = MakeStressConf(20101, 20100);
		auto MPUT = std::shared_ptr<DataPort>(newMaster("MasterUT", "", conf), delMaster);
		REQUIRE(MPUT);
		MPUT->Build();

		MPUT->Enable();

		// Let the system reach a steady reconnect cycle (~500ms)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		MonitorReconnect(MPUT, 30000);

		MPUT->Disable();
	}
	UnLoadModule(portlib);
	TestTearDown();
}

TEST_CASE(SUITE("Outstation reconnect stress"))
{
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		ThreadPool thread_pool(1);

		// Outstation: listen=20103, remote=20102 (EvilRemote)
		EvilRemote evil(20102);

		newptr newOS = GetPortCreator(portlib, "DNP3Outstation");
		REQUIRE(newOS);
		delptr delOS = GetPortDestroyer(portlib, "DNP3Outstation");
		REQUIRE(delOS);

		auto conf = MakeStressConf(20103, 20102);
		auto OSUT = std::shared_ptr<DataPort>(newOS("OutstationUT", "", conf), delOS);
		REQUIRE(OSUT);
		OSUT->Build();

		OSUT->Enable();

		// Let the system reach a steady reconnect cycle (~500ms)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		MonitorReconnect(OSUT, 30000);

		OSUT->Disable();
	}
	UnLoadModule(portlib);
	TestTearDown();
}
*/
TEST_CASE(SUITE("Multi reconnect stress"))
{
	// 3 masters + 5 outstations sharing a single UDP channel and EvilRemote.
	// All use the same UDPListenPort/Port/IP so ChannelHandler gives them all
	// the same ChannelID → one io_handler with 8 stacked sessions.
	TestSetup();

	auto portlib = LoadModule(GetLibFileName("DNP3Port"));
	REQUIRE(portlib);
	{
		newptr newMaster = GetPortCreator(portlib, "DNP3Master");
		REQUIRE(newMaster);
		delptr delMaster = GetPortDestroyer(portlib, "DNP3Master");
		REQUIRE(delMaster);
		newptr newOS = GetPortCreator(portlib, "DNP3Outstation");
		REQUIRE(newOS);
		delptr delOS = GetPortDestroyer(portlib, "DNP3Outstation");
		REQUIRE(delOS);

		// Each port needs a unique DNP3 (source,dest) address pair
		const uint16_t ev_port = 20104;
		const uint16_t all_listen = 20105;
		auto make_master = [&](uint16_t ms_addr, uint16_t os_addr, int idx)
					 {
						 auto conf = MakeStressConf(all_listen, ev_port);
						 conf["MasterAddr"] = Json::UInt(ms_addr);
						 conf["OutstationAddr"] = Json::UInt(os_addr);
						 auto p = std::shared_ptr<DataPort>(newMaster("MultiM"+std::to_string(idx), "", conf), delMaster);
						 p->Build();
						 return p;
					 };
		auto make_os = [&](uint16_t ms_addr, uint16_t os_addr, int idx)
				   {
					   auto conf = MakeStressConf(all_listen, ev_port);
					   conf["MasterAddr"] = Json::UInt(ms_addr);
					   conf["OutstationAddr"] = Json::UInt(os_addr);
					   auto p = std::shared_ptr<DataPort>(newOS("MultiOS"+std::to_string(idx), "", conf), delOS);
					   p->Build();
					   return p;
				   };

		std::vector<std::shared_ptr<DataPort>> ports;
		ports.push_back(make_master(10, 20, 0));
		ports.push_back(make_master(11, 21, 1));
		ports.push_back(make_master(12, 22, 2));
		ports.push_back(make_os(23, 13, 0));
		ports.push_back(make_os(24, 14, 1));
		ports.push_back(make_os(25, 15, 2));
		ports.push_back(make_os(26, 16, 3));
		ports.push_back(make_os(27, 17, 4));

		ThreadPool thread_pool(2);
		EvilRemote evil(ev_port);

		for(auto& p : ports)
			p->Enable();

		// Let the system reach a steady reconnect cycle (~500ms)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// All ports share the same channel, so channel stats are identical
		MonitorReconnect(ports[0], 10000);

		// Verify no ports got stuck disabled
		for(auto& p : ports)
		{
			auto status = p->GetStatus()["Result"].asString();
			REQUIRE(status != "Port disabled");
		}

		for(auto& p : ports)
			p->Disable();
	}
	UnLoadModule(portlib);
	TestTearDown();
}
