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
 * PortManager.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <thread>
#include <opendatacon/asio.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <opendatacon/asio_syslog_spdlog_sink.h>

#include <opendatacon/util.h>
#include <opendatacon/Version.h>
#include "PortManager.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <conio.h>
#endif

PortManager::PortManager(std::string FileName):
	pIOS(std::make_shared<odc::asio_service>(std::thread::hardware_concurrency()+1)),
	ios_working(pIOS->make_work()),
	shutting_down(false),
	shut_down(false),
	pTCPostream(nullptr)
{
	SetupLoggers();

	//TODO: Parse the config file and create the ports.
	// ProcessFile(Filename);
	const std::string aServerEndPoint("127.0.0.1");
	const std::string aServerPort("5001");
	const std::string aClientEndPoint("127.0.0.1");
	const std::string aClientPort("10000");
	uint16_t retry_time_ms(500);

	auto p = std::make_shared<PortProxyConnection>( pIOS, aServerEndPoint,aServerPort,aClientEndPoint,aClientPort,retry_time_ms);
	PortCollectionMap.push_back(p);

	if(PortCollectionMap.empty() )
		throw std::runtime_error("No port objects to manage");

}

PortManager::~PortManager()
{
	//In case of exception - ie. if we're destructed while still running
	//dump (detach) any threads
	//the threads would be already joined and cleared on normal shutdown
	for (auto& thread : threads)
		thread.detach();
	threads.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
	{
		//if there's a tcp sink, we need to destroy it
		//	because ostream will be destroyed
		//same for syslog, because asio::io_service will be destroyed
		for(const char* logger : {"tcp","syslog"})
		{
			if(LogSinksMap.count(logger))
			{
				//This doesn't look thread safe
				//	but we're on the main thread at this point
				//	the only other threads should be spdlog threads
				//	so if we flush first this should be safe...
				//BUT flush doesn't wait for the async Qs :-(
				//	it only flushes the sinks
				//	only thing to do is give some time for the Qs to empty
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				log->flush();
				auto tcp_sink_pos = std::find(log->sinks().begin(),log->sinks().end(),LogSinksMap[logger]);
				if(tcp_sink_pos != log->sinks().end())
				{
					log->sinks().erase(tcp_sink_pos);
				}
			}
		}
	}
}

void PortManager::SetLogLevel(std::stringstream& ss)
{
	std::string sinkname;
	std::string level_str;
	if(ss>>sinkname && ss>>level_str)
	{
		for(auto& sink : LogSinksMap)
		{
			if(sink.first == sinkname)
			{
				auto new_level = spdlog::level::from_str(level_str);
				if(new_level == spdlog::level::off && level_str != "off")
				{
					std::cout << "Invalid log level. Options are:" << std::endl;
					for(uint8_t i = 0; i < 7; i++)
						std::cout << spdlog::level::level_string_views[i].data() << std::endl;
					return;
				}
				else
				{
					sink.second->set_level(new_level);
					return;
				}
			}
		}
	}
	std::cout << "Usage: set loglevel <sinkname> <level>" << std::endl;
	std::cout << "Sinks:" << std::endl;
	for(auto& sink : LogSinksMap)
		std::cout << sink.first << std::endl;
	std::cout << std::endl << "Levels:" << std::endl;
	for(uint8_t i = 0; i < 7; i++)
		std::cout << spdlog::level::level_string_views[i].data() << std::endl;
}

void PortManager::SetupLoggers()
{
	//these return level::off if no match
	auto log_level = spdlog::level::debug;
	auto console_level = spdlog::level::debug;

	try
	{
		auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("./PortProxy.log", 5000*1024, 2);
		auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

		file->set_level(log_level);
		console->set_level(console_level);

		LogSinksMap["file"] = file;
		LogSinksVec.push_back(file);
		LogSinksMap["console"] = console;
		LogSinksVec.push_back(console);

		odc::spdlog_init_thread_pool(4096,3);
		auto pMainLogger = std::make_shared<spdlog::async_logger>("opendatacon", begin(LogSinksVec), end(LogSinksVec),
			odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
		pMainLogger->set_level(spdlog::level::trace);
		odc::spdlog_register_logger(pMainLogger);
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		throw std::runtime_error("Main logger initialization failed: " + std::string(ex.what()));
	}

	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

	log->critical("This is opendatacon version '{}'", ODC_VERSION_STRING);
	log->critical("Log level set to {}", spdlog::level::level_string_views[log_level]);
	log->critical("Console level set to {}", spdlog::level::level_string_views[console_level]);
	log->info("Loading configuration... ");
}
void PortManager::Build()
{
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Initialising Ports...");

}
void PortManager::Run()
{
	if (auto log = odc::spdlog_get("opendatacon"))
		log->info("Starting worker threads...");
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		threads.emplace_back([&]() {pIOS->run(); });

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Opening Server Ports...");

	for (auto& port : PortCollectionMap)
	{
		port->Open(); // Opens up server ports
	}

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Up and running -- Hit <s> to exit...");

	// Setup a console reader to allow keyboard exit.
	ReCheckKeyboard();

	pIOS->run();
	for(auto& thread : threads)
		thread.join();
	threads.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Destoying Ports...");
	PortCollectionMap.clear();

	shut_down = true;
}
void PortManager::ReCheckKeyboard()
{
	std::shared_ptr<asio::steady_timer> timer = pIOS->make_steady_timer();
	timer->expires_from_now(std::chrono::seconds(1));
	timer->async_wait([&, timer](asio::error_code e) // [=] all autos by copy, [&] all autos by ref
		{
			#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
			// Just check for keypresses 's' to stop!
			if (kbhit())
			{
			      if (getch() == 's')
			      {
			            Shutdown();
			            return;
				}
			}
			if (!e)
				ReCheckKeyboard();
			#endif
		});
}

void PortManager::Shutdown()
{
	//Shutdown gets called from various places (signal handling, user interface(s))
	//ensure we only act once
	std::call_once(shutdown_flag, [this]()
		{
			shutting_down = true;
			if(auto log = odc::spdlog_get("opendatacon"))
			{
			      log->critical("Shutting Down...");
			      log->info("Disabling Ports...");
			}

			for (auto& port : PortCollectionMap)
			{
			      port->Close(); // Close server ports
			}

			if(auto log = odc::spdlog_get("opendatacon"))
			{
			      log->info("Finishing asynchronous tasks...");
			      log->flush(); //for the benefit of tcp logger shutdown
			}

			//shutdown tcp logger so it doesn't keep the io_service going
			TCPbuf.DeInit();
			if(LogSinksMap.count("tcp"))
				LogSinksMap["tcp"]->set_level(spdlog::level::off);

			ios_working.reset();
		});
}

bool PortManager::isShuttingDown()
{
	return shutting_down;
}
bool PortManager::isShutDown()
{
	return shut_down;
}
