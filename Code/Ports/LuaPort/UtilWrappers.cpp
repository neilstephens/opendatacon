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
 *  UtilWrappers.cpp
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */
#include "CLua.h"
#include <opendatacon/util.h>
void ExportUtilWrappers(lua_State* const L)
{
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			lua_pushinteger(L, odc::msSinceEpoch());
			return 1; //number of lua ret vals pushed onto the stack
		});
	lua_setglobal(L, "msSinceEpoch");
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto ms = lua_tointeger(L,-1);
			lua_pushstring(L, odc::since_epoch_to_datetime(ms).c_str());
			return 1; //number of lua ret vals pushed onto the stack
		});
	lua_setglobal(L, "msSinceEpochToDateTime");
}
