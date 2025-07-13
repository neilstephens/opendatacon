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
#include <Lua/Wrappers.h>
#include <opendatacon/LogHelpers.h>
#include <opendatacon/Platform.h>
#include <opendatacon/util.h>
#include <string>

static odc::LogHelpers Log{"opendatacon"};

LuaLogSink::LuaLogSink(const std::string& Name, const std::string& LuaFile, const Json::Value& Config):
	Name(Name)
{

	lua_newtable(L);
	{
		lua_pushstring(L,Name.c_str());
		lua_setfield(L,-2,"Name");
		PushJSON(L,Config);
		lua_setfield(L,-2,"Config");
	}
	lua_setglobal(L,"odc");
	ExportMetaTables(L);
	ExportLogWrappers(L,Name,"opendatacon");
	ExportUtilWrappers(L,pStrand,handler_tracker,Name,"opendatacon");
	luaL_openlibs(L);

	//load the lua code
	auto ret = luaL_dofile(L, LuaFile.c_str());
	if(ret != 0)
	{
		std::string err = lua_tostring(L, -1);
		throw std::runtime_error("Failed to load LuaFile '"+LuaFile+"'. Error: "+err);
	}

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

	pStrand->post([this,t{std::move(time)},lg{std::move(logger)},l{msg.level},m{std::move(message)},h{handler_tracker}]()
		{
			lua_getglobal(L,"LogSink");
			lua_pushinteger(L, t);
			lua_pushstring(L, lg.c_str());
			lua_pushinteger(L, l);
			lua_pushstring(L, m.c_str());

			const int num_args = 4; const int num_rets = 0;
			auto ret = lua_pcall(L, num_args, num_rets, 0);
			if(ret != 0)
			{
				std::string err = lua_tostring(L, -1);
				Log.Error("{}: error calling Lua code's LogSink() function. Error: '{}'",Name,err);
			}
		});
}

void LuaLogSink::flush_()
{
	lua_gc(L,LUA_GCCOLLECT);
}

LuaLogSink::~LuaLogSink()
{
	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	lua_close(L);
}
