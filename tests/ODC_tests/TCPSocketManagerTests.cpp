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

const unsigned int test_timeout = 20000;

#define SUITE(name) "TCPSocketManager - " name

template<typename T>
void require_equal(const T& thing1, const T& thing2)
{
	//make a watchdog timer
	std::atomic_bool stop = false;
	auto stop_timer = odc::asio_service::Get()->make_steady_timer();
	stop_timer->expires_from_now(std::chrono::milliseconds(test_timeout));
	stop_timer->async_wait([&stop](const asio::error_code& err){stop = true;});

	while(thing1 != thing2 && !stop)
		odc::asio_service::Get()->poll_one();

	if(!stop)
		stop_timer->cancel();
	else
		odc::spdlog_get("opendatacon")->critical("Test timeout");

	while(!stop)
		odc::asio_service::Get()->poll_one();

	REQUIRE(thing1 == thing2);
}

TEST_CASE(SUITE("SimpleStrings"))
{
	TestSetup();

	std::string send1 = "Hello World!";
	std::string send2 = "looooooooooooooooooooooooooooooooooooooooooonnnnnnnnnnnnnnnnggggggggggggggggggggeeeeeeeeeeeeeeeeeeeeeeeerrrrrrrrr string";
	std::string recv1 = "";
	std::string recv2 = "";
	std::atomic_bool state1 = false;
	std::atomic_bool state2 = false;

	bool close_on_read = false;
	std::unique_ptr<TCPSocketManager<std::string>>* sock_ptr_ptr;
	auto CloseOnRead = [&](){if(close_on_read) {(*sock_ptr_ptr)->Close();close_on_read = false;}};

	auto ReadHandler = [&](bool sock1, odc::buf_t& buf)
				 {
					 CloseOnRead();
					 auto& recv = sock1 ? recv1 : recv2;

					 while(buf.size())
					 {
						 char c = buf.sgetc();
						 recv.push_back(c);
						 buf.consume(1);
					 }
				 };
	auto StateHandler = [&](bool sock1, bool state)
				  {
					  auto& sockstate = sock1 ? state1 : state2;
					  sockstate = state;
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

	require_equal(recv1,send2);
	require_equal(recv2,send1);

	//close then write, to test buffering
	pSockMan1->Close();
	pSockMan2->Close();
	//wait for close
	while(state1 || state2)
		odc::asio_service::Get()->poll_one();
	//echo back - the recv1 strings are ok to use now, because sockets should be closed
	pSockMan1->Write(std::string(recv1));
	pSockMan2->Write(std::string(recv2));
	pSockMan1->Write(std::string("X"));
	pSockMan2->Write(std::string("X"));
	pSockMan1->Open();
	pSockMan2->Open();

	require_equal(recv1,send2+send1+"X");
	require_equal(recv2,send1+send2+"X");

	//send a big packet then close the sender as soon as it starts coming through
	sock_ptr_ptr = &pSockMan1;
	recv2.clear();
	close_on_read = true;
	for(auto i=0; i<50000; i++)
		send1.append(std::to_string(i));
	pSockMan1->Write(std::string(send1));

	//wait for close
	require_equal(state1,std::atomic_bool(false));
	pSockMan1->Open();
	//wait for open
	require_equal(state1,std::atomic_bool(true));

	require_equal(recv2,send1);

	pSockMan1->Close();
	pSockMan2->Close();
	//wait for close
	require_equal(state1,std::atomic_bool(false));
	require_equal(state2,std::atomic_bool(false));

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
	timer->expires_from_now(std::chrono::milliseconds(std::uniform_int_distribution<uint16_t>(70,200)(RandNumGenerator)));
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
	std::atomic<uint64_t> interrupt_count = 0;
	std::atomic_bool state1;
	std::atomic_bool state2;
	int recv_offset1 = 0;
	int recv_offset2 = 0;

	auto ReadHandler =
		[&](bool sock1, odc::buf_t& buf)
		{
			auto& count = sock1 ? recv_count1 : recv_count2;
			auto& recv_offset = sock1 ? recv_offset1 : recv_offset2;
			while(buf.size())
			{
				count++;
				auto ch = buf.sgetc();
				if((count+recv_offset)%256 != ch)
				{
					recv_offset = ch - count%256;
					odc::spdlog_get("opendatacon")->critical("Possible out of order or lost data. Recieved {}, count {} ({}), offset {}",(uint8_t)ch,count,count%256,recv_offset);
				}
				buf.consume(1);
			}
		};
	auto StateHandler =
		[&](bool sock1, bool state)
		{
			auto& sock_state = sock1 ? state1 : state2;
			sock_state = state;
			if(!state1 && !state2)
				interrupt_count++;
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
	stop_timer->expires_from_now(std::chrono::milliseconds(5000));
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
		std::this_thread::sleep_for(std::chrono::microseconds(300));

		std::string data1 = "";
		std::string data2 = "";
		for(int i=0; i<100; i++)
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

	odc::spdlog_get("opendatacon")->info("send_count1:{}, recv_count2:{}, send_count2:{}, recv_count1:{}",send_count1,recv_count2,send_count2,recv_count1);
	odc::spdlog_get("opendatacon")->info("Interruption count: {}", interrupt_count);

	for(auto& thread : threads)
		thread.join();

	TestTearDown();
}
