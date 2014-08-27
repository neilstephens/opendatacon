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
 *  Created on: 13/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "DNP3MasterPort.h"
#include "DNP3OutstationPort.h"

extern "C" __declspec(dllexport) DNP3MasterPort* new_DNP3MasterPort(std::string Name, std::string File, std::string Overrides)
{
	return new DNP3MasterPort(Name,File,Overrides);
}
extern "C" __declspec(dllexport) DNP3OutstationPort* new_DNP3OutstationPort(std::string Name, std::string File, std::string Overrides)
{
	return new DNP3OutstationPort(Name,File,Overrides);
}


