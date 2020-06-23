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
 * main.cpp
 *
 *  Created on: 14/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

/*TODO:
 *    -add config change commands
 *          -cmd to apply config item from command line
 *          -cmd to load config from file
 *          -save config to file
 *    -add a network interface to the console
 *	-more dataports to implement:
 *		-C37.118
 *		-NMEA 2k / CANv2
 *		-NMEA 0183
 *		-Gridlab-D
 *		-JSONoHTML
 */

#include "DataConcentrator.h"
#include "DaemonInterface.h"
#include "ODCArgs.h"
#include <csignal>
#include <cstdio>
#include <opendatacon/Platform.h>

int main(int argc, char* argv[])
{
	int ret_val;
	std::string pidfile = "";

	// Wrap everything in a try block.  Do this every time,
	// because exceptions will be thrown for problems.
	try
	{
		// Turn command line arguments into easy to query struct
		ODCArgs Args(argc, argv);

		// If arg "-p <path>" is set, try and change directory to <path>
		if (Args.PathArg.isSet())
		{
			std::string PathName = Args.PathArg.getValue();
			if (ChangeWorkingDir(PathName))
			{
				const size_t strmax = 80;
				char buf[strmax];
				char* str = strerror_rp(errno, buf, strmax);
				std::string msg;
				if (str)
				{
					msg = "Unable to change working directory to '" + PathName + "' : " + str;
				}
				else
				{
					msg = "Unable to change working directory to '" + PathName + "' : UNKNOWN ERROR";
				}
				throw std::runtime_error(msg);
			}
		}

		// If arg "-r" provided, unregister daemon (for platforms that provide support)
		if (Args.DaemonRemoveArg.isSet())
		{
			daemon_remove();
		}
		// If arg "-i" provided, register daemon (for platforms that provide support)
		if (Args.DaemonInstallArg.isSet())
		{
			daemon_install(Args);
		}
		// If arg "-d" provided, daemonise (for platforms that provide support)
		if (Args.DaemonArg.isSet())
		{
			daemonp(Args);
			if(Args.PIDFileArg.isSet())
				pidfile = Args.PIDFileArg.getValue();
		}

		// Construct and build opendatacon object
		//	static shared ptr to use in signal handler
		static auto TheDataConcentrator = std::make_shared<DataConcentrator>(Args.ConfigFileArg.getValue());

		TheDataConcentrator->Build();

		// Configure signal handlers
		auto shutdown_func = [] (int signum)
					   {
						   TheDataConcentrator->Shutdown();
					   };
		auto ignore_func = [] (int signum)
					 {
						 std::cout<<"Signal "<<signum<<" ignored. Not designed to be interrupted or suspended.\n"
						                               "To terminate, send a quit, kill, abort or break signal, or use a UI shutdown command.\n"
						                               "To run in the background, run as a daemon or service."<<std::endl;
					 };

		for (auto SIG : SIG_SHUTDOWN)
		{
			::signal(SIG,shutdown_func);
		}
		for (auto SIG : SIG_IGNORE)
		{
			::signal(SIG,ignore_func);
		}

		// Start opendatacon, returns after a clean shutdown
		auto run_thread = std::thread([=](){TheDataConcentrator->Run();});

		while(!TheDataConcentrator->isShuttingDown())
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		//Shutting down - give some time for clean shutdown
		unsigned int i=0;
		while(!TheDataConcentrator->isShutDown() && i++ < 150)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::string msg("opendatacon version '" ODC_VERSION_STRING);
		if(TheDataConcentrator->isShutDown())
		{
			run_thread.join();
			msg += "' shutdown cleanly.";
			ret_val = 0;
			TheDataConcentrator.reset();
		}
		else
		{
			run_thread.detach();
			msg += "' shutdown timed out.";
			ret_val = 1;
		}

		if(auto log = odc::spdlog_get("opendatacon"))
			log->critical(msg);
		else
			std::cout << msg << std::endl;
	}
	catch (TCLAP::ArgException &e) // catch command line argument exceptions
	{
		std::string msg = "Command line error: " + e.error() +" for arg " + e.argId();
		if(auto log = odc::spdlog_get("opendatacon"))
			log->critical(msg);
		else
			std::cerr << msg << std::endl;
		ret_val = 1;
	}
	catch (std::exception& e) // catch opendatacon runtime exceptions
	{
		std::string msg = std::string("Caught exception: ") + e.what();
		if(auto log = odc::spdlog_get("opendatacon"))
			log->critical(msg);
		else
			std::cerr << msg << std::endl;
		ret_val = 1;
	}

	if(pidfile != "")
	{
		if(std::remove(pidfile.c_str()))
			if(auto log = odc::spdlog_get("opendatacon"))
				log->info("PID file removed");
	}

	if(auto log = odc::spdlog_get("opendatacon"))
		log->flush();

	odc::spdlog_shutdown();
	return ret_val;
}
