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

#include <catch.hpp>
#include <iostream>

std::string level_str = "info";

int main( int argc, char* argv[] )
{
	int new_argc = argc;
	char** new_argv = argv;
	if (argc > 1)
	{
		level_str = argv[1];
		new_argc = argc - 1;
		new_argv = argv + 1;
	}

	return Catch::Session().run(new_argc, new_argv);
}
