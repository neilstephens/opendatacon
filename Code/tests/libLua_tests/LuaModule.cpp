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
 * LuaModule.cpp
 *
 *  Created on: 9/7/2023
 *      Author: Neil Stephens
 */

extern "C"
{
#include <lualib.h>
#include <lauxlib.h>
#include <lua.h>
}

#include <iostream>

static int echo(lua_State *L)
{
	std::string ech = lua_tostring(L,1);
	std::cout << ech << std::endl;
	return 0;
}

static const luaL_Reg funcs[] =
{
	{ "echo", echo},
	{ NULL, NULL }
};


extern "C" int luaopen_libTestLuaModule(lua_State* L)
{
	luaL_newlib(L, funcs);
	return 1;
}

extern "C" int luaopen_TestLuaModule(lua_State* L)
{
	return luaopen_libTestLuaModule(L);
}

extern "C" int luaopen_TestLuaModuled(lua_State* L)
{
	return luaopen_libTestLuaModule(L);
}
