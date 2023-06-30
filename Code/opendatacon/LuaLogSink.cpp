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
 * LuaLogSink.cpp
 *
 *  Created on: 30/6/2023
 *      Author: Neil Stephens
 */
#include "LuaLogSink.h"
#include <opendatacon/Platform.h>
#include <opendatacon/util.h>
#include <string>

#define LUA_TFUNCTION 6
typedef int (*lua_KFunction) (lua_State *L, int status, intptr_t ctx);

LuaLogSink::LuaLogSink(const std::string& Name, const std::string& LuaFile):
	Name(Name)
{
	//We don't link the lua library. It's a non-core component,
	//so to keep things modular, we dynamically load it only if there's a LuaLogSink created

	std::string libfilename(GetLibFileName("Lua"));
	LuaLib = LoadModule(libfilename);
	if(!LuaLib)
		throw std::runtime_error("Failed to load Lua library.");

	auto luaL_newstate = reinterpret_cast<lua_State* (*)(void)>(LoadSymbol(LuaLib, "luaL_newstate"));
	auto lua_createtable = reinterpret_cast<void (*)(lua_State *L, int narray, int nrec)>(LoadSymbol(LuaLib, "lua_createtable"));
	auto lua_setglobal = reinterpret_cast<void (*)(lua_State *L, const char *name)>(LoadSymbol(LuaLib, "lua_setglobal"));
	auto ExportMetaTables = reinterpret_cast<void (*)(lua_State* const L)>(LoadSymbol(LuaLib, "ExportMetaTables"));
	auto ExportLogWrappers = reinterpret_cast<void (*)(lua_State* const L, const std::string& Name, const std::string& LogName)>(LoadSymbol(LuaLib, "ExportLogWrappers"));
	auto ExportUtilWrappers = reinterpret_cast<void (*)(lua_State* const L, std::shared_ptr<asio::io_service::strand> pSyncStrand, std::shared_ptr<void> handler_tracker, const std::string& Name, const std::string& LogName)>(LoadSymbol(LuaLib, "ExportUtilWrappers"));
	auto luaL_openlibs = reinterpret_cast<void (*)(lua_State*)>(LoadSymbol(LuaLib, "luaL_openlibs"));
	auto luaL_loadfilex = reinterpret_cast<int (*)(lua_State *L, const char *filename, const char *mode)>(LoadSymbol(LuaLib, "luaL_loadfilex"));
	auto lua_pcallk = reinterpret_cast<int (*)(lua_State *L, int nargs, int nresults, int errfunc, intptr_t ctx, lua_KFunction k)>(LoadSymbol(LuaLib, "lua_pcallk"));
	auto lua_tolstring = reinterpret_cast<const char* (*)(lua_State *L, int idx, size_t *len)>(LoadSymbol(LuaLib, "lua_tolstring"));

	L = luaL_newstate();
	lua_createtable(L,0,0);
	{
		//TODO: export Config
	}
	lua_setglobal(L,"odc");
	ExportMetaTables(L);
	ExportLogWrappers(L,Name,"opendatacon");
	ExportUtilWrappers(L,pStrand,handler_tracker,Name,"opendatacon");
	luaL_openlibs(L);

	//load the lua code
	auto ret = luaL_loadfilex(L, LuaFile.c_str(),0) || lua_pcallk(L, 0, -1, 0, 0, nullptr);
	if(ret != 0)
	{
		std::string err = lua_tolstring(L, -1, nullptr);
		throw std::runtime_error("Failed to load LuaFile '"+LuaFile+"'. Error: "+err);
	}

	auto lua_getglobal = reinterpret_cast<int (*)(lua_State *L, const char *name)>(LoadSymbol(LuaLib, "lua_getglobal"));
	auto ret_type = lua_getglobal(L,"LogSink");
	if(ret_type != LUA_TFUNCTION)
		throw std::runtime_error("Lua code missing LogSink function");
}

void LuaLogSink::sink_it_(const spdlog::details::log_msg &msg)
{
	//Don't try to log our own error - or that could get messy!
	if(strncmp(msg.payload.data(),(Name+":").c_str(),Name.length()+1) == 0)
		return;

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()).count();
	auto logger = std::string(msg.logger_name.data(),msg.logger_name.size());
	auto message = std::string(msg.payload.data(),msg.payload.size());

	auto lua_getglobal = reinterpret_cast<int (*)(lua_State *L, const char *name)>(LoadSymbol(LuaLib, "lua_getglobal"));
	auto lua_pushinteger = reinterpret_cast<void (*)(lua_State *L, long long n)>(LoadSymbol(LuaLib, "lua_pushinteger"));
	auto lua_pushstring = reinterpret_cast<const char* (*)(lua_State *L, const char *s)>(LoadSymbol(LuaLib, "lua_pushstring"));
	auto lua_pcallk = reinterpret_cast<int (*)(lua_State *L, int nargs, int nresults, int errfunc, intptr_t ctx, lua_KFunction k)>(LoadSymbol(LuaLib, "lua_pcallk"));
	auto lua_tolstring = reinterpret_cast<const char* (*)(lua_State *L, int idx, size_t *len)>(LoadSymbol(LuaLib, "lua_tolstring"));

	pStrand->post([=,t{std::move(time)},lg{std::move(logger)},l{msg.level},m{std::move(message)},h{handler_tracker}]()
		{
			lua_getglobal(L,"LogSink");
			lua_pushinteger(L, t);
			lua_pushstring(L, lg.c_str());
			lua_pushinteger(L, l);
			lua_pushstring(L, m.c_str());

			const int num_args = 4;
			auto ret = lua_pcallk(L, num_args, 0, 0, 0, nullptr);
			if(ret != 0)
			{
			      std::string err = lua_tolstring(L, -1, nullptr);
			      if(auto log = odc::spdlog_get("opendatacon"))
					log->error("{}: error calling Lua code's LogSink() function. Error: '{}'",Name,err);
			}
		});
}

void LuaLogSink::flush_(){};

LuaLogSink::~LuaLogSink()
{
	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	auto lua_close = reinterpret_cast<void (*)(lua_State*)>(LoadSymbol(LuaLib, "lua_close"));
	lua_close(L);
}
