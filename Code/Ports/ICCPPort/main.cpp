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
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ICCP (IEC 60870-6 TASE.2) port library - plugin entry points
 */

#include "ICCPClientPort.h"
#include "ICCPServerPort.h"

extern "C" ICCPClientPort* new_ICCPClientPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new ICCPClientPort(Name,File,Overrides);
}

extern "C" void delete_ICCPClientPort(ICCPClientPort* aICCPClientPort_ptr)
{
	delete aICCPClientPort_ptr;
}

extern "C" ICCPServerPort* new_ICCPServerPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new ICCPServerPort(Name,File,Overrides);
}

extern "C" void delete_ICCPServerPort(ICCPServerPort* aICCPServerPort_ptr)
{
	delete aICCPServerPort_ptr;
}
