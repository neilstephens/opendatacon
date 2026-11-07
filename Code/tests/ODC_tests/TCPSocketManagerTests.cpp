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
#include "../ThreadPool.h"
#include <opendatacon/TCPSocketManager.h>
#include <opendatacon/util.h>
#include <catch.hpp>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <thread>

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
		if(!odc::asio_service::Get()->poll_one()) std::this_thread::yield();

	if(!stop)
		stop_timer->cancel();
	else
		Log.Critical("Test timeout");

	while(!stop)
		if(!odc::asio_service::Get()->poll_one()) std::this_thread::yield();

	if(thing1 != thing2)
		CHECK(thing1 == thing2);
}

TEST_CASE(SUITE("ListenClose"))
{
	TestSetup();

	auto ReadHandler = [&](odc::buf_t&){};
	auto StateHandler = [&](bool){};

	Log.Debug("Creating Sock");
	auto pSockMan = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler,StateHandler,10,true,0,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.GetLog()->log(spdlog::level::from_str(level),"Sock1: {}",msg);});

	auto pThreadPool = std::make_unique<ThreadPool>(2);

	pSockMan->Open();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	pSockMan->Close();
	pSockMan.reset();

	pThreadPool.reset();

	TestTearDown();
}

TEST_CASE(SUITE("OpenClose"))
{
	TestSetup();

	std::atomic_bool state1 = false;
	std::atomic_bool state2 = false;


	auto ReadHandler = [&](bool, odc::buf_t&){};
	auto StateHandler = [&](bool sock1, bool state)
				  {
					  auto& sockstate = sock1 ? state1 : state2;
					  sockstate = state;
					  Log.Debug("Sock{} state: {}",sock1 ? "1" : "2",state ? "OPEN" : "CLOSED");
				  };

	auto ReadHandler1 = std::bind(ReadHandler,true,std::placeholders::_1);
	auto ReadHandler2 = std::bind(ReadHandler,false,std::placeholders::_1);
	auto StateHandler1 = std::bind(StateHandler,true,std::placeholders::_1);
	auto StateHandler2 = std::bind(StateHandler,false,std::placeholders::_1);

	Log.Debug("Creating Sock1");
	auto pSockMan1 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler1,StateHandler1,10,true,0,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.GetLog()->log(spdlog::level::from_str(level),"Sock1: {}",msg);});

	Log.Debug("Creating Sock2");
	auto pSockMan2 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		false,"127.0.0.1","22222",ReadHandler2,StateHandler2,10,true,0,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.GetLog()->log(spdlog::level::from_str(level),"Sock2: {}",msg);});

	pSockMan1->Open();
	pSockMan2->Open();
	//wait for open
	require_equal(state1,std::atomic_bool(true));
	require_equal(state2,std::atomic_bool(true));

	pSockMan1->Close();
	pSockMan2->Close();
	//wait for close
	require_equal(state1,std::atomic_bool(false));
	require_equal(state2,std::atomic_bool(false));

	pSockMan1.reset();
	pSockMan2.reset();

	//make sure work is all done
	odc::asio_service::Get()->run();

	TestTearDown();
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
	std::unique_ptr<TCPSocketManager>* sock_ptr_ptr;
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
					  Log.Debug("Sock{} state: {}",sock1 ? "1" : "2",state ? "OPEN" : "CLOSED");
				  };

	auto ReadHandler1 = std::bind(ReadHandler,true,std::placeholders::_1);
	auto ReadHandler2 = std::bind(ReadHandler,false,std::placeholders::_1);
	auto StateHandler1 = std::bind(StateHandler,true,std::placeholders::_1);
	auto StateHandler2 = std::bind(StateHandler,false,std::placeholders::_1);

	Log.Debug("Creating Sock1");
	auto pSockMan1 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler1,StateHandler1,10,true,0,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.GetLog()->log(spdlog::level::from_str(level),"Sock1: {}",msg);});

	Log.Debug("Creating Sock2");
	auto pSockMan2 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		false,"127.0.0.1","22222",ReadHandler2,StateHandler2,10,true,0,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.GetLog()->log(spdlog::level::from_str(level),"Sock2: {}",msg);});

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
	require_equal(state1,std::atomic_bool(false));
	require_equal(state2,std::atomic_bool(false));
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
	require_equal(state2,std::atomic_bool(false));
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

	//make sure work is all done
	odc::asio_service::Get()->run();

	TestTearDown();
}

void interrupt(const std::string& description, std::unique_ptr<TCPSocketManager>& sock, std::atomic_bool& open, std::unique_ptr<odc::steady_timer, deleter>& timer, const std::atomic_bool& stop)
{
	thread_local std::mt19937 RandNumGenerator = std::mt19937(std::random_device()());
	open = !open;
	if(open)
	{
		Log.Info("{}: Interrupt timer Open()",description);
		sock->Open();
	}
	else
	{
		Log.Info("{}: Interrupt timer Close()",description);
		sock->Close();
	}
	timer->expires_from_now(std::chrono::milliseconds(std::uniform_int_distribution<uint16_t>(70,200)(RandNumGenerator)));
	timer->async_wait([&](const asio::error_code& err)
		{
			if(err || stop)
				Log.Debug("Interrupt timer info: {}",err.message());
			else
				interrupt(description,sock,open,timer,stop);
		});
}

TEST_CASE(SUITE("ManyStrings"))
{
	TestSetup();

	Log.Info("Start {}",SUITE("ManyStrings"));

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
				uint8_t ch = buf.sgetc();
				if((count+recv_offset)%256 != ch)
				{
					recv_offset = ch - count%256;
					Log.Critical("{}: Possible out of order or lost data. Received {}, count {} ({}), offset {}",sock1 ? "Sock1" : "Sock2",(uint8_t)ch,count,count%256,recv_offset);
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
			Log.Debug("Sock{}: state {}",sock1 ? "1" : "2",state ? "OPEN" : "CLOSED");
		};

	auto ReadHandler1 = std::bind(ReadHandler,true,std::placeholders::_1);
	auto ReadHandler2 = std::bind(ReadHandler,false,std::placeholders::_1);
	auto StateHandler1 = std::bind(StateHandler,true,std::placeholders::_1);
	auto StateHandler2 = std::bind(StateHandler,false,std::placeholders::_1);

	Log.Debug("Creating Sock1");
	auto pSockMan1 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		true,"127.0.0.1","22222",ReadHandler1,StateHandler1,1000000,true,10,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.Debug("[{}] Sock1: {}",level,msg);});
	//use debug for logs - we force lots of errors

	Log.Debug("Creating Sock2");
	auto pSockMan2 = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),
		false,"127.0.0.1","22222",ReadHandler2,StateHandler2,1000000,true,10,0,0,0,
		[](const std::string& level, const std::string& msg){ Log.Debug("[{}] Sock2: {}",level,msg);});
	//use debug for logs - we force lots of errors

	auto pThreadPool = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());

	//make a timer to stop the run
	std::atomic_bool stop = false;
	auto stop_timer = odc::asio_service::Get()->make_steady_timer();
	stop_timer->expires_from_now(std::chrono::milliseconds(5000));
	stop_timer->async_wait([&stop](const asio::error_code&){stop = true;});

	//make two 'chains' of timers that fire off at random times to open and close the two socket managers
	std::atomic_bool s1_open = false;
	std::atomic_bool s2_open = false;
	auto interrupt_timer1 = odc::asio_service::Get()->make_steady_timer();
	auto interrupt_timer2 = odc::asio_service::Get()->make_steady_timer();
	//kick off the interruptions
	const std::string s1 = "Sock1"; const std::string s2 = "Sock2";
	interrupt(s1,pSockMan1, s1_open, interrupt_timer1,stop);
	interrupt(s2,pSockMan2, s2_open, interrupt_timer2,stop);

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

	interrupt_timer1.reset();
	interrupt_timer2.reset();
	stop_timer.reset();

	//just in case some interrupts were queued
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	//final open to make sure all the writes get a chance
	pSockMan1->Open();
	pSockMan2->Open();

	require_equal(send_count1, recv_count2);
	require_equal(send_count2, recv_count1);

	pSockMan1->Close();
	pSockMan2->Close();
	pSockMan1.reset();
	pSockMan2.reset();
	pThreadPool.reset();

	Log.Info("send_count1:{}, recv_count2:{}, send_count2:{}, recv_count1:{}",send_count1,recv_count2,send_count2,recv_count1);
	Log.Info("Interruption count: {}", interrupt_count);
	TestTearDown();
}

namespace
{
struct Recorder
{
	std::mutex mtx;
	std::string received;
	std::vector<bool> states;
	std::unique_ptr<TCPSocketManager> pSockMan;

	void Make(bool server, uint16_t port, bool drop = false, uint64_t writedelay_ms = 0)
	{
		pSockMan = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),server,"127.0.0.1",std::to_string(port),
			[this](odc::buf_t& buf)
			{
				std::string s(buf.size(),'\0');
				asio::buffer_copy(asio::buffer(s),buf.data());
				buf.consume(buf.size());
				std::lock_guard<std::mutex> lock(mtx);
				received += s;
			},
			[this](bool state)
			{
				std::lock_guard<std::mutex> lock(mtx);
				states.push_back(state);
			},
			std::numeric_limits<size_t>::max(),true,20,0,0,writedelay_ms,
			[server](const std::string& level, const std::string& msg){ Log.Debug("[{}] {}: {}",level,server ? "Server" : "Client",msg);},
			true,599,10,3,drop);
	}
	std::string Data()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return received;
	}
	size_t Count(bool state)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return std::count(states.begin(),states.end(),state);
	}
};

template <typename P>
void WaitFor(P pred, const std::string& what)
{
	const auto end = std::chrono::steady_clock::now()+std::chrono::milliseconds(test_timeout);
	while(!pred())
	{
		if(std::chrono::steady_clock::now() > end)
			FAIL("timed out waiting for "+what);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

template <typename P>
bool StaysFalse(P pred, int ms = 300)
{
	const auto end = std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
	while(std::chrono::steady_clock::now() < end)
	{
		if(pred())
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}
}

//A TCP listener that accepts connections and never closes them (or reads from them)
class SilentPeer
{
public:
	explicit SilentPeer(uint16_t port):
		acceptor(io,asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(),port))
	{
		Accept();
		thread = std::thread([this]{ io.run(); });
	}
	~SilentPeer()
	{
		asio::post(io,[this]{ std::error_code ec; acceptor.close(ec); sockets.clear(); });
		guard.reset();
		thread.join();
	}
	size_t Accepted() const { return accepted.load(); }
private:
	void Accept()
	{
		auto s = std::make_shared<asio::ip::tcp::socket>(io);
		acceptor.async_accept(*s,[this,s](const std::error_code& ec)
			{
				if(ec)
					return;
				sockets.push_back(s);
				++accepted;
				Accept();
			});
	}
	asio::io_context io;
	asio::executor_work_guard<asio::io_context::executor_type> guard = asio::make_work_guard(io);
	asio::ip::tcp::acceptor acceptor;
	std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets;
	std::atomic<size_t> accepted{0};
	std::thread thread;
};

TEST_CASE(SUITE("Writes are discarded between connections"))
{
	TestSetup();
	{
		ThreadPool pool(2);
		Recorder server, client;
		server.Make(true,22240);
		client.Make(false,22240,true);
		client.pSockMan->Write(std::string("before open,"));
		client.pSockMan->Open();
		client.pSockMan->Write(std::string("not connected,"));
		server.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 1 && server.Count(true) == 1;},"the connection");
		client.pSockMan->Write(std::string("first"));
		WaitFor([&]{return server.Data() == "first";},"the first write");

		server.pSockMan->Close();
		WaitFor([&]{return client.Count(false) == 1;},"the client seeing the close");
		client.pSockMan->Write(std::string("while down,"));
		server.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 2 && server.Count(true) == 2;},"the second connection");
		client.pSockMan->Write(std::string("second"));
		WaitFor([&]{return server.Data().size() >= 11;},"the second write");
		CHECK(server.Data() == "firstsecond");

		client.pSockMan->Close();
		server.pSockMan->Close();
		WaitFor([&]{return client.Count(false) == 2 && server.Count(false) == 2;},"the close");
	}
	TestTearDown();
}

TEST_CASE(SUITE("Close sends what was written first, and Open connects again"))
{
	TestSetup();
	{
		ThreadPool pool(2);
		Recorder server, client;
		server.Make(true,22241);
		client.Make(false,22241,true);
		server.pSockMan->Open();
		client.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 1 && server.Count(true) == 1;},"the connection");

		//more than one write's worth, and a write made after Close() while it is still sending
		std::string big;
		for(int i = 0; i < 200000; i++)
			big += std::to_string(i);
		client.pSockMan->Write(std::string(big));
		client.pSockMan->Close();
		client.pSockMan->Write(std::string("END"));
		client.pSockMan->Open();
		WaitFor([&]{return server.Count(false) == 1 && client.Count(false) == 1;},"the end of the connection");
		CHECK(server.Data() == big+"END");

		//Open() after Close(): it connects again once the connection has ended
		WaitFor([&]{return client.Count(true) == 2 && server.Count(true) == 2;},"the next connection");
		client.pSockMan->Write(std::string("next"));
		WaitFor([&]{return server.Data().size() == big.size()+7;},"the write on the next connection");
		CHECK(server.Data() == big+"ENDnext");

		client.pSockMan->Close();
		CHECK(StaysFalse([&]{return client.Count(true) > 2;}));
		server.pSockMan->Close();
	}
	TestTearDown();
}

TEST_CASE(SUITE("Close waits behind writes delayed by throttle_writedelay_ms"))
{
	TestSetup();
	{
		ThreadPool pool(2);
		Recorder server, client;
		server.Make(true,22242);
		client.Make(false,22242,true,200);
		server.pSockMan->Open();
		client.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 1 && server.Count(true) == 1;},"the connection");
		client.pSockMan->Write(std::string("delayed"));
		client.pSockMan->Close();
		WaitFor([&]{return server.Count(false) == 1;},"the end of the connection");
		CHECK(server.Data() == "delayed");
		server.pSockMan->Close();
	}
	TestTearDown();
}

TEST_CASE(SUITE("Close closes the socket if the peer doesn't close its side in time"))
{
	TestSetup();
	{
		ThreadPool pool(2);
		SilentPeer peer(22243);
		Recorder client;
		client.Make(false,22243,true);
		client.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 1;},"the connection");
		//without the abort the old connection's read would keep Open() from connecting again for as long as the peer keeps it
		client.pSockMan->Close(300);
		client.pSockMan->Open();
		WaitFor([&]{return client.Count(true) == 2 && peer.Accepted() == 2;},"the next connection");
		client.pSockMan->Close(100);
		WaitFor([&]{return client.Count(false) == 2;},"the close");
	}
	TestTearDown();
}

TEST_CASE(SUITE("Open then Close at once, again and again, against a peer that never closes: no crash, and nothing waits for the peer"))
{
	TestSetup();
	{
		ThreadPool pool(4);
		SilentPeer peer(22246);
		for(int i = 0; i < 200; i++)
		{
			//(a connect that completes just after Close() closed its socket, or just after Close() and so is unneeded, are the cases)
			auto client = std::make_unique<TCPSocketManager>(odc::asio_service::Get(),false,"127.0.0.1","22246",[](odc::buf_t& buf){ buf.consume(buf.size()); },
				[](bool){},std::numeric_limits<size_t>::max(),true,0,0,0,0,[](const std::string&, const std::string&){},true,599,10,3,true);
			client->Open();
			if(i % 2)
				std::this_thread::sleep_for(std::chrono::microseconds(100*(i%10)));
			client->Close(100);
			const auto start = std::chrono::steady_clock::now();
			client.reset();
			CHECK(std::chrono::steady_clock::now()-start < std::chrono::seconds(2));
		}
	}
	TestTearDown();
}

TEST_CASE(SUITE("Post and Wrap"))
{
	TestSetup();
	{
		ThreadPool pool(2);
		Recorder client;
		client.Make(false,22244);
		std::atomic_bool posted = false, wrapped = false;
		auto timer = odc::asio_service::Get()->make_steady_timer(std::chrono::milliseconds(100));
		timer->async_wait(client.pSockMan->Wrap([&wrapped](const std::error_code&){ wrapped = true; }));
		client.pSockMan->Post([&posted](){ posted = true; });
		client.pSockMan.reset(); //waits for them
		CHECK(posted);
		CHECK(wrapped);
	}
	TestTearDown();
}
