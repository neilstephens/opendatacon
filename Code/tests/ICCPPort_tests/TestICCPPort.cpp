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
 * TestICCPPort.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The Catch main of the ICCPPort unit tests. The tests are compiled into this executable together with the port's objects (see CMakeLists.txt),
 *  not into the ICCPPort library.
 */

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif

int main( int argc, char* argv[] )
{
	#if defined(_MSC_VER) && defined(_DEBUG)
	//Debug CRT/STL assertions (e.g. mismatched iterators) go to stderr and the debugger instead of a dialog,
	//so that a failure is visible and can't hang a headless run. Harmless (compiled out) in Release.
	for(int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
	{
		_CrtSetReportMode(type, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
		_CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
	}
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	#endif
	//ctest passes the log level first (see _odc_test_log_level_args); it is not a Catch argument, or it would be taken as a test name
	int new_argc = argc;
	char** new_argv = argv;
	if(argc > 1)
	{
		const std::string arg = argv[1];
		if(arg == "trace" || arg == "debug" || arg == "info" || arg == "warning" || arg == "error" || arg == "critical" || arg == "off")
		{
			new_argc = argc - 1;
			new_argv = argv + 1;
		}
	}
	return Catch::Session().run(new_argc, new_argv);
}
