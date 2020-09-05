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
 * TCPSocketManagerTests.cpp
 *
 *  Created on: 04/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "TestODCHelpers.h"
#include <opendatacon/TCPSocketManager.h>
#include <opendatacon/util.h>
#include <catch.hpp>
#include <atomic>

using namespace odc;

const unsigned int test_timeout = 10000;

#define SUITE(name) "TCPSocketManager - " name

template<typename T>
inline void require_equal(const T& thing1, const T& thing2)
{
	unsigned int count = 0;
	while(thing1 != thing2 && count < test_timeout)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		odc::asio_service::Get()->poll_one();
		count++;
	}
	REQUIRE(thing1 == thing2);
}

TEST_CASE(SUITE("SimpleStrings"))
{
	TestSetup();

	std::string send1 = "Hello World!";
	std::string send2 = "looooooooooooooooooooooooooooooooooooooooooonnnnnnnnnnnnnnnnggggggggggggggggggggeeeeeeeeeeeeeeeeeeeeeeeerrrrrrrrr string";
	std::string recv1 = "";
	std::string recv2 = "";

	auto ReadHandler = [&](bool sock1, odc::buf_t& buf)
				 {
					 auto& recv = sock1 ? recv1 : recv2;

					 while(buf.size())
					 {
						 char c = buf.sgetc();
						 recv.push_back(c);
						 buf.consume(1);
					 }
				 };
	auto StateHandler = [](bool sock1, bool state)
				  {
					  odc::spdlog_get("opendatacon")->debug("Sock{} state: {}",sock1 ? "1" : "2",state ? "OPEN" : "CLOSED");
				  };

	auto ReadHandler1 = std::bind(ReadHandler,true,std::placeholders::_1);
	auto ReadHandler2 = std::bind(ReadHandler,false,std::placeholders::_1);
	auto StateHandler1 = std::bind(StateHandler,true,std::placeholders::_1);
	auto StateHandler2 = std::bind(StateHandler,false,std::placeholders::_1);

	odc::spdlog_get("opendatacon")->debug("Creating Sock1");
	auto pSockMan1 = std::make_unique<TCPSocketManager<std::string>>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler1,StateHandler1,10,true,0,
		[](const std::string& msg){ odc::spdlog_get("opendatacon")->debug("Sock1: {}",msg);});

	odc::spdlog_get("opendatacon")->debug("Creating Sock2");
	auto pSockMan2 = std::make_unique<TCPSocketManager<std::string>>(odc::asio_service::Get(),
		false,"127.0.0.1","22222",ReadHandler2,StateHandler2,10,true,0,
		[](const std::string& msg){ odc::spdlog_get("opendatacon")->debug("Sock2: {}",msg);});

	pSockMan1->Open();
	pSockMan2->Open();
	pSockMan1->Write(std::string(send1));
	pSockMan2->Write(std::string(send2));

	require_equal(send1,recv2);
	require_equal(send2,recv1);

	pSockMan1->Close();
	pSockMan2->Close();
	pSockMan1.reset();
	pSockMan2.reset();

	TestTearDown();
}

void interrupt(std::unique_ptr<TCPSocketManager<std::string>>& sock, std::atomic_bool& open, std::unique_ptr<asio::steady_timer>& timer, const std::atomic_bool& stop)
{
	thread_local std::mt19937 RandNumGenerator = std::mt19937(std::random_device()());
	open = !open;
	if(open)
		sock->Open();
	else
		sock->Close();
	timer->expires_from_now(std::chrono::milliseconds(std::uniform_int_distribution<uint16_t>(100,300)(RandNumGenerator)));
	timer->async_wait([&](const asio::error_code& err)
		{
			if(err || stop)
				odc::spdlog_get("opendatacon")->debug("Interrupt timer stop: {}",err.message());
			else
				interrupt(sock,open,timer,stop);
		});
}

TEST_CASE(SUITE("ManyStrings"))
{
	TestSetup();

	odc::spdlog_get("opendatacon")->info("Start {}",SUITE("ManyStrings"));

	std::atomic<uint64_t> recv_count1 = 0;
	std::atomic<uint64_t> recv_count2 = 0;
	std::atomic<uint64_t> send_count1 = 0;
	std::atomic<uint64_t> send_count2 = 0;

	auto ReadHandler =
		[&](bool sock1, odc::buf_t& buf)
		{
			auto& count = sock1 ? recv_count1 : recv_count2;
			while(buf.size())
			{
				count++;
				auto ch = buf.sgetc();
				if((count)%256 != ch)
					odc::spdlog_get("opendatacon")->critical("Possible out of order or lost data: {} != {}",(count)%256, ch);
				buf.consume(1);
			}
		};
	auto StateHandler =
		[&](bool sock1, bool state)
		{
			odc::spdlog_get("opendatacon")->debug("Sock{} state: {}",sock1 ? "1" : "2",state ? "OPEN" : "CLOSED");
		};

	auto ReadHandler1 = std::bind(ReadHandler,true,std::placeholders::_1);
	auto ReadHandler2 = std::bind(ReadHandler,false,std::placeholders::_1);
	auto StateHandler1 = std::bind(StateHandler,true,std::placeholders::_1);
	auto StateHandler2 = std::bind(StateHandler,false,std::placeholders::_1);

	odc::spdlog_get("opendatacon")->debug("Creating Sock1");
	auto pSockMan1 = std::make_unique<TCPSocketManager<std::string>>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler1,StateHandler1,1000000,true,10,
		[](const std::string& msg){ odc::spdlog_get("opendatacon")->debug("Sock1: {}",msg);});

	odc::spdlog_get("opendatacon")->debug("Creating Sock2");
	auto pSockMan2 = std::make_unique<TCPSocketManager<std::string>>(odc::asio_service::Get(),
		false,"127.0.0.1","22222",ReadHandler2,StateHandler2,1000000,true,10,
		[](const std::string& msg){ odc::spdlog_get("opendatacon")->debug("Sock2: {}",msg);});

	auto work = odc::asio_service::Get()->make_work();
	std::vector<std::thread> threads;
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		threads.emplace_back([](){odc::asio_service::Get()->run();});

	//make a timer to stop the run
	std::atomic_bool stop = false;
	auto stop_timer = odc::asio_service::Get()->make_steady_timer();
	stop_timer->expires_from_now(std::chrono::milliseconds(1000));
	stop_timer->async_wait([&stop](const asio::error_code& err){stop = true;});

	//make two 'chains' of timers that fire off at random times to open and close the two socket managers
	std::atomic_bool s1_open = false;
	std::atomic_bool s2_open = false;
	auto interrupt_timer1 = odc::asio_service::Get()->make_steady_timer();
	auto interrupt_timer2 = odc::asio_service::Get()->make_steady_timer();
	//kick off the interruptions
	interrupt(pSockMan1, s1_open, interrupt_timer1,stop);
	interrupt(pSockMan2, s2_open, interrupt_timer2,stop);

	//write a bunch of data while the connections are going up and down
	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::microseconds(50));

		std::string data1 = "";
		std::string data2 = "";
		for(int i=0; i<4; i++)
		{
			data1.push_back(char(++send_count1%256));
			data2.push_back(char(++send_count2%256));
		}
		pSockMan1->Write(std::string(std::move(data1)));
		pSockMan2->Write(std::string(std::move(data2)));
	}

	interrupt_timer1->cancel();
	interrupt_timer2->cancel();

	//final open to make sure all the writes get a chance
	pSockMan1->Open();
	pSockMan2->Open();

	require_equal(send_count1, recv_count2);
	require_equal(send_count2, recv_count1);

	pSockMan1->Close();
	pSockMan2->Close();
	pSockMan1.reset();
	pSockMan2.reset();
	work.reset();

	odc::spdlog_get("opendatacon")->debug("send_count1 {} recv_count2 {} send_count2 {} recv_count1 {}",send_count1,recv_count2,send_count2,recv_count1);

	for(auto& thread : threads)
		thread.join();

	TestTearDown();
}