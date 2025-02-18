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
 * Wrappers.h
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */
#ifndef WRAPPERS_H
#define WRAPPERS_H

#include "../../IOTypeWrappers.h"
#include "../../LogWrappers.h"
#include "../../UtilWrappers.h"
#include <opendatacon/asio.h>
#include <string>

extern "C" void ExportMetaTables(lua_State* const L);

inline void ExportWrappersToLua(lua_State* const L,
	std::shared_ptr<asio::io_service::strand> pSyncStrand,
	std::shared_ptr<void> handler_tracker,
	const std::string& Name,
	const std::string& LogName)
{
	//UserData Metatables
	ExportMetaTables(L);
	//IOTypes
	ExportEventTypes(L);
	ExportQualityFlags(L);
	ExportCommandStatus(L);
	ExportControlCodes(L);
	ExportConnectStates(L);
	ExportPayloadFactory(L);
	ExportToStringFunctions(L);
	//Logging
	ExportLogWrappers(L,Name,LogName);
	//Util
	ExportUtilWrappers(L,pSyncStrand,handler_tracker,Name,LogName);
}

#endif // WRAPPERS_H
