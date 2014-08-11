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
 * 	-fix logging:
 * 		-use log level properly
 * 		-cmd to change log level on the fly (part of config cmds: see below)
 * 	-add config change commands
 * 		-implement BuildOrRebuild properly for changed configs
 * 		-cmd to apply config item from command line
 * 		-cmd to load config from file
 * 		-save config to file
 * 	-add maintenance commands:
 * 		-enable/disable/restart ports/connectors/connections
 * 	-remove the need for DNP3Manager and two threadpools?
 * 		-DataConcentrator class can do it all
 * 	-implement plugin architecture - the following should be plugins:
 * 		-DataPort implementations
 * 		-Transform implementations
 * 	-add a network interface to the console
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

int main(int argc, char* argv[])
{
	try
	{
		//default config file
		std::string ConfFileName = "opendatacon.conf";
		//override if there's one provided
		if (argc>1)
			ConfFileName = argv[1];

		DataConcentrator TheDataConcentrator(ConfFileName);
		TheDataConcentrator.BuildOrRebuild();
		TheDataConcentrator.Run();

	}
	catch(std::exception& e)
	{
		std::cout<<"Caught exception: "<<e.what()<<std::endl;
		return 1;
	}

	return 0;
}


