/*	opendatacon
*
*	Copyright (c) 2018:
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
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"

extern "C" MD3MasterPort* new_MD3MasterPort(std::string Name, std::string File, const Json::Value Overrides)
{
	return nullptr;
//	return new MD3MasterPort(Name,File,Overrides);
}

extern "C" MD3OutstationPort* new_MD3OutstationPort(std::string Name, std::string File, const Json::Value Overrides)
{
	return new MD3OutstationPort(Name,File,Overrides);
}

extern "C" void delete_MD3MasterPort(MD3MasterPort* aMD3MasterPort_ptr)
{
	// delete aMD3MasterPort_ptr;
	return;
}

extern "C" void delete_MD3OutstationPort(MD3OutstationPort* aMD3OutstationPort_ptr)
{
	delete aMD3OutstationPort_ptr;
	return;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	// Get duplicate symbols if the code below is compiled under VS. Have to work out a better way to manage this.

#else
#define CATCH_CONFIG_RUNNER
#include <catchvs.hpp>		// This version has the hooks to display the tests in the VS Test Explorer
extern "C" int run_tests( int argc, char* argv[] )
{
	return Catch::Session().run( argc, argv );
}
#endif
