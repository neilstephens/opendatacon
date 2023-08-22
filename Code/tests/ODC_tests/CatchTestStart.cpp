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
/**
 */

#define CATCH_CONFIG_RUNNER

#include "../../../opendatacon/DataConnector.cpp"
#include <opendatacon/util.h>
#include <catch.hpp>

spdlog::level::level_enum log_level = spdlog::level::off;

int main( int argc, char* argv[] )
{
	int new_argc = argc;
	char** new_argv = argv;
	if (argc > 1)
	{
		std::string level_str = argv[1];
		log_level = spdlog::level::from_str(level_str);
		if (log_level == spdlog::level::off && level_str != "off")
		{
			std::cout << "libODC_tests: optional log level as first arg. Choose from:" << std::endl;
			for (uint8_t i = 0; i < 7; i++)
				std::cout << spdlog::level::level_string_views[i].data() << std::endl;
		}
		else
		{
			new_argc = argc - 1;
			new_argv = argv + 1;
		}
	}

	return Catch::Session().run(new_argc, new_argv);
}
