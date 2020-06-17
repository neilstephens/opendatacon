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

#include <iostream>
#include <opendatacon/Platform.h>

std::string dirnameOf(const std::string& fname)
{
	size_t pos = fname.find_last_of("\\/");
	return (std::string::npos == pos)
	       ? ""
	       : fname.substr(0, pos);
}

int main( int argc, char* argv[] )
{
	InitLibaryLoading();
	std::string libname = "PyPort";
	std::string libfilename = GetLibFileName(libname);
	auto pluginlib = LoadModule(libfilename);

	if (pluginlib == nullptr)
	{
		std::cout << libname << " Info: dynamic library load failed '" << libfilename << "' :" << LastSystemError() << std::endl;
		return 1;
	}

	auto run_tests = reinterpret_cast<int (*)(int,char**)>(LoadSymbol(pluginlib, "run_tests"));

	if(run_tests == nullptr)
	{
		std::cout << "Info: failed to load run_tests symbol from '" << libfilename << "' "<< std::endl;
		return 1;
	}
	return run_tests( argc, argv );
}
