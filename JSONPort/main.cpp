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

#include "JSONPort.h"

extern "C" JSONPort* new_JSONClientPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new JSONPort(Name,File,Overrides,false);
}

extern "C" void delete_JSONClientPort(JSONPort* aJSONClientPort_ptr)
{
	delete aJSONClientPort_ptr;
	return;
}

extern "C" JSONPort* new_JSONServerPort(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	return new JSONPort(Name,File,Overrides,true);
}

extern "C" void delete_JSONServerPort(JSONPort* aJSONServerPort_ptr)
{
	delete aJSONServerPort_ptr;
	return;
}

