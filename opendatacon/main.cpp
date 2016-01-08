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
 *    -fix logging:
 *          -cmd to change log level on the fly (part of config cmds: see below)
 *    -add config change commands
 *          -implement BuildOrRebuild properly for changed configs
 *          -cmd to apply config item from command line
 *          -cmd to load config from file
 *          -save config to file
 *    -add maintenance commands:
 *          -enable/disable/restart ports/connectors/connections
 *    -remove the need for two threadpools?
 *          -Mod to DNP3Manager to use existing io_service?
 *    -add a network interface to the console
 *	-network logging
 *	-daemon mode
 *	-more dataports to implement:
 *		-EventGenPort (random events ala old test_slaves util)
 *		-C37.118
 *		-NMEA 2k / CANv2
 *		-NMEA 0183
 *		-XMLoHTML (inc. Gridlab-D)
 *		-JSONoHTML
 */

#include "DataConcentrator.h"
#include <opendatacon/Platform.h>
#include <errno.h>
#include <csignal>

#include "DaemonInterface.h"
#include "ODCArgs.h"

int main(int argc, char* argv[])
{
	// Wrap everything in a try block.  Do this every time,
	// because exceptions will be thrown for problems.
	try
	{
		static std::unique_ptr<DataConcentrator> TheDataConcentrator(nullptr);

		ODCArgs Args(argc, argv);

		std::cout << Args.toString();


		if (Args.PathArg.isSet())
		{
			// Try to change working directory
			std::string PathName = Args.PathArg.getValue();
			if (CHDIR(PathName.c_str()))
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

		if (Args.DaemonRemoveArg.isSet())
		{
			daemon_remove();
		}
		if (Args.DaemonInstallArg.isSet())
		{
			daemon_install(Args);
		}
		if (Args.DaemonArg.isSet())
		{
			daemonp(Args);
		}

		std::cout << "This is opendatacon version " << ODC_VERSION_STRING << std::endl;
		std::cout << "Loading configuration... ";
		TheDataConcentrator.reset(new DataConcentrator(Args.ConfigFileArg.getValue()));
		std::cout << "done" << std::endl << "Initialising objects... " << std::endl;
		TheDataConcentrator->BuildOrRebuild();
		std::cout << "done" << std::endl << "Starting up opendatacon..." << std::endl;
        
        auto shutdown_func = [](int signum)
        {
            TheDataConcentrator->Shutdown();
        };
        
        for (auto SIG: SIG_SHUTDOWN)
        {
            ::signal(SIG,shutdown_func);
        }
        for (auto SIG: SIG_IGNORE)
        {
            ::signal(SIG,SIG_IGN);
        }
        
		TheDataConcentrator->Run();
        
		std::cout << "opendatacon version " << ODC_VERSION_STRING << " shutdown cleanly." << std::endl;
	}
	catch (TCLAP::ArgException &e) // catch any exceptions
	{
		std::cerr << "Command line error: " << e.error() << " for arg " << e.argId() << std::endl;
		return 1;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
