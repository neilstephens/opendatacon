/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 *  Created on: 26/03/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#include "PyPort.h"

#ifdef NONVSTESTING
#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#else
#include <catchvs.hpp> // This version has the hooks to display the tests in the VS Test Explorer
#endif

extern "C" PyPort* new_PyPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new PyPort(Name, File, Overrides);
}

extern "C" void delete_PyPort(PyPort* aPyPort_ptr)
{
	delete aPyPort_ptr;
	return;
}

//
// Should be turned on for "normal" builds, and off if you want to use Visual Studio Test Integration.
//

extern "C" int run_tests(int argc, char* argv[])
{
	#ifdef NONVSTESTING
	// Create loggers for tests here
	spdlog::level::level_enum log_level = spdlog::level::off;
	int new_argc = argc;
	char** new_argv = argv;
	if (argc > 1)
	{
		std::string level_str = argv[1];
		log_level = spdlog::level::from_str(level_str);
		if (log_level == spdlog::level::off && level_str != "off")
		{
			std::cout << "PyPort: optional log level as first arg. Choose from:" << std::endl;
			for (uint8_t i = 0; i < 7; i++)
				std::cout << spdlog::level::level_string_views[i].data() << std::endl;
		}
		else
		{
			new_argc = argc - 1;
			new_argv = argv + 1;
		}
	}
	CommandLineLoggingSetup(log_level);

	auto result = Catch::Session().run(new_argc, new_argv);

	// And release here.
	CommandLineLoggingCleanup();
	return result;

	#else
	std::cout << "PyPort: Compiled for Visual Studio Testing only" << std::endl;
	return 1;
	#endif
}
