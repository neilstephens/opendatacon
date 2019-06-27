//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include <asio.hpp>
#include "server.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>


std::thread* StartIOSThread(asio::io_context& IOS)
{
	return new std::thread([&] { IOS.run(); });
}
void StopIOSThread(asio::io_context& IOS, std::thread* runthread)
{
	IOS.stop();        // This does not block. The next line will! If we have multiple threads, have to join all of them.
	runthread->join(); // Wait for it to exit
	delete runthread;
}
void SSetupLoggers(spdlog::level::level_enum log_level)
{
	// So create the log sink first - can be more than one and add to a vector.

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("testslog.txt", true);

	std::vector<spdlog::sink_ptr> sinks = { file_sink,console_sink };

	std::shared_ptr<spdlog::logger> pLibLogger = std::make_shared<spdlog::logger>("PyPort", begin(sinks), end(sinks));
	pLibLogger->set_level(log_level);

	spdlog::register_logger(pLibLogger);
}
int servermain(int argc, char* argv[])
{
	try
	{
		SSetupLoggers(spdlog::level::level_enum::debug);

		LOGDEBUG("Server about to Start");

		const int threadcount = 4;
		auto pIOS = std::make_shared<asio::io_context>(threadcount); // Max 4 threads

		// Initialise the server.
		http::server s(pIOS, "localhost","8000"); // As long as there is an index.html - you will get a reply...

		auto roothandler = std::make_shared<http::HandlerCallbackType>([](const std::string& absoluteuri, http::reply& rep)
			{
				rep.status = http::reply::ok;
				rep.content.append("You have reached the PyPort http interface.<br>To talk to a port the url must contain the PyPort name, which is case senstive.<br>Anything beyond this will be passed to the Python code.");
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
			});
		s.register_handler("GET /", roothandler);
		auto porthandler = std::make_shared<http::HandlerCallbackType>([](const std::string& absoluteuri, http::reply& rep)
			{
				rep.status = http::reply::ok;
				rep.content.append("You have reached the PyPort Instance");
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
			});
		s.register_handler("GET /PyPort", porthandler);

		auto work = std::make_shared<asio::io_context::work>(*pIOS); // To keep running - have to check server exit so this is the only thing keeping things awake.

		std::thread* pThread[threadcount];
		for (int i = 0; i < threadcount; i++)
			pThread[i] = StartIOSThread(*pIOS);

		std::cout << "Hit enter to exit";
		std::cin.get();

		work.reset();
		for (int i = 0; i < threadcount; i++)
			StopIOSThread(*pIOS, pThread[i]);

	}
	catch (std::exception& e)
	{
		std::cerr << "exception: " << e.what() << "\n";
	}
	spdlog::drop_all();
	return 0;
}
