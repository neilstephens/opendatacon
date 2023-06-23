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
 *  LogWrappers.cpp
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */

#include "CLua.h"
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>

void ExportLogWrappers(lua_State* const L, const std::string& Name)
{
	lua_getglobal(L,"odc");

	//Make a table of log functions
	lua_newtable(L);
	for(uint8_t i = 0; i < 7; i++)
	{
		const auto& level = spdlog::level::level_string_views[i].data();

		//push table key
		lua_pushstring(L, level);

		//push table value - closure with two upvalues
		lua_pushstring(L, Name.c_str());
		lua_pushinteger(L,i);
		lua_pushcclosure(L, [](lua_State* const L) -> int
			{
				auto name = lua_tostring(L, lua_upvalueindex(1));
				auto lvl = static_cast<spdlog::level::level_enum>(lua_tointeger(L, lua_upvalueindex(2)));
				auto msg = lua_tostring(L,-1);
				auto log = odc::spdlog_get("LuaPort");
				if(log)
					log->log(lvl,"[Lua] {}: {}",name,msg);
				lua_pushboolean(L, !!log);
				return 1; //number of lua ret vals pushed onto the stack
			}, 2);

		//add key value pair to table
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"log");
}
