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
 * Plugin.cpp
 *
 *  Created on: 9/7/2023
 *      Author: Neil Stephens
 */


//TODO: define func for exe to call
//	execute some lua that 'requires' the test lua module, and calls echo

#include <Lua/DynamicSymbols.h>
#include <Lua/Wrappers.h>
#include <Lua/CLua.h>

#include <opendatacon/Platform.h>
#include <opendatacon/util.h>
#include <whereami++.h>

#include <iostream>
#include <filesystem>

extern "C" bool test_func()
{
	auto LuaModuleFileName = std::filesystem::path(GetLibFileName("TestLuaModule"));
	auto ext = LuaModuleFileName.extension().string();
	auto name = LuaModuleFileName.stem().string();
	auto path = std::filesystem::relative(whereami::getExecutablePath().dirname()).generic_string();
	auto paths = path+"/?"+ext+";"+path+"/lib/?"+ext+";";

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Lua module name: {}", name);

	Lua::DynamicSymbols syms;

	auto LuaCode =
	"    package.cpath = \""+paths+"\" .. package.cpath;       \n"
	"    local mo = require(\""+name+"\");                     \n"
	"    mo.echo(\"Message from Lua module\");                 \n"
	"    odc.log.info(\"Message from Lua\");                   \n";

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Lua code:\n{}", LuaCode);

	auto L = luaL_newstate();
	//top level table "odc"
	lua_newtable(L);
	lua_setglobal(L,"odc");

	ExportLogWrappers(L,"LuaTestPlugin","opendatacon");
	luaL_openlibs(L);

	//load the lua code
	auto ret = luaL_dostring(L, LuaCode.c_str());
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(L, -1);
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("Failed to load code. Error: {}", err);
	}
	lua_close(L);
	return (ret == LUA_OK);
}
