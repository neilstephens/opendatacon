/*	opendatacon
 *
 *	Copyright (c) 2016:
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
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include "SNMPOutstationPort.h"
#include "SNMPMasterPort.h"

extern "C" SNMPMasterPort* new_SNMPMasterPort(std::string Name, std::string File, const Json::Value Overrides)
{
	return new SNMPMasterPort(Name,File,Overrides);
}

extern "C" SNMPOutstationPort* new_SNMPOutstationPort(std::string Name, std::string File, const Json::Value Overrides)
{
	return new SNMPOutstationPort(Name,File,Overrides);
}

extern "C" void delete_SNMPMasterPort(SNMPMasterPort* aSNMPMasterPort_ptr)
{
	delete aSNMPMasterPort_ptr;
	return;
}

extern "C" void delete_SNMPOutstationPort(SNMPOutstationPort* aSNMPOutstationPort_ptr)
{
	delete aSNMPOutstationPort_ptr;
	return;
}
