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
 * LuaInst.cpp
 *
 *  Created on: 27/6/2023
 *      Author: Neil Stephens
 */
#include "LuaInst.h"
#include <Lua/Wrappers.h>
#include <sstream>
#include <string>
#include <chrono>

LuaInst::LuaInst(const std::string& lua_code,
	const CommandHandler& CmdHandler,
	const MessageHandler& MsgHandler,
	const std::string& LoggerName,
	const std::string& ID,
	std::stringstream& script_args):
	CmdHandler(CmdHandler),
	MsgHandler(MsgHandler),
	LoggerName(LoggerName),
	ID(ID)
{
	//top level table "odc"
	lua_newtable(L);
	{
		//push a UICommand closure capturing 'this'
		lua_pushlightuserdata(L,this);
		lua_pushstring(L,ID.c_str());
		lua_pushstring(L,LoggerName.c_str());
		lua_pushcclosure(L,[](lua_State* const L) -> int
			{
				//retrieve 'this'
				auto self = static_cast<LuaInst*>(lua_touserdata(L, lua_upvalueindex(1)));
				if(!(lua_isstring(L,1) && lua_isstring(L,2)))
				{
				      std::string name = lua_tostring(L, lua_upvalueindex(2));
				      std::string logname = lua_tostring(L, lua_upvalueindex(3));
				      if(auto log = odc::spdlog_get(logname))
						log->error("{}: UICommand() requires 'responder name' and 'command' string args.",name);
				      lua_pushnil(L);
				      return 1;
				}
				auto responder_name = lua_tostring(L,1);
				auto cmd = lua_tostring(L,2);
				size_t idx = 3;
				std::stringstream args;
				while(lua_isstring(L,idx))
					args<<lua_tostring(L,idx++)<<"\n";
				auto json_result = self->CmdHandler(responder_name,cmd,args);
				PushJSON(L,json_result);
				return 1;
			},3);
		lua_setfield(L,-2,"UICommand");

		//push a UIMessage closure capturing 'this'
		lua_pushlightuserdata(L,this);
		lua_pushstring(L,ID.c_str());
		lua_pushstring(L,LoggerName.c_str());
		lua_pushcclosure(L,[](lua_State* const L) -> int
			{
				//retrieve 'this'
				auto self = static_cast<LuaInst*>(lua_touserdata(L, lua_upvalueindex(1)));
				if(!lua_isstring(L,1))
				{
				      std::string name = lua_tostring(L, lua_upvalueindex(2));
				      std::string logname = lua_tostring(L, lua_upvalueindex(3));
				      if(auto log = odc::spdlog_get(logname))
						log->error("{}: UIMessage() requires string argument.",name);
				      return 0;
				}
				auto msg = lua_tostring(L,1);
				self->MsgHandler(self->ID,msg);
				return 0;
			},3);
		lua_setfield(L,-2,"UIMessage");
	}
	lua_setglobal(L,"odc");

	ExportWrappersToLua(L,pStrand,handler_tracker,ID,LoggerName);
	luaL_openlibs(L);

	//load the lua code
	auto ret = luaL_dostring(L, lua_code.c_str());
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(L, -1);
		throw std::runtime_error(ID+": Failed to load code: "+err);
	}

	lua_getglobal(L, "SendCommands");
	if(!lua_isfunction(L, -1))
		throw std::runtime_error(ID+": Lua code doesn't have 'SendCommands' function.");

	std::string remaining = !script_args.eof() ? script_args.str().substr(script_args.tellg()) : "";
	pStrand->post([this,remaining,h{handler_tracker}](){Runner(remaining);});
}
LuaInst::~LuaInst()
{
	cancelled = true;
	pSleepTimer->cancel();
	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	lua_close(L);
}

bool LuaInst::Completed()
{
	return completed;
}

//only call Runner() on strand
void LuaInst::Runner(const std::string& args)
{
	if(!cancelled)
	{
		lua_getglobal(L, "SendCommands");
		int argc = 0;
		std::stringstream args_ss(args); std::string arg;
		while(args_ss>>arg)
		{
			lua_pushstring(L,arg.c_str());
			argc++;
		}
		const int retc = 1;
		auto ret = lua_pcall(L,argc,retc,0);
		if(ret != LUA_OK)
		{
			std::string err = ID+": Lua SendCommands() call error: "+lua_tostring(L, -1);
			MsgHandler(ID,err);
			if(auto log = odc::spdlog_get(LoggerName))
				log->error(err);
			lua_pop(L,1);
			completed = true;
			return;
		}
		//should have a sleep time in ms on the stack (return value)
		if(!lua_isinteger(L,-1))
		{
			completed = true;
			return;
		}
		auto sleep_time_ms = lua_tointeger(L,-1);
		if(sleep_time_ms < 0)
		{
			completed = true;
			return;
		}
		pSleepTimer->expires_from_now(std::chrono::milliseconds(sleep_time_ms));
		pSleepTimer->async_wait(pStrand->wrap([this,h{handler_tracker}](asio::error_code err)
			{
				if(err)
					cancelled = true;
				Runner();
			}));
		return;
	}
	lua_getglobal(L, "Cancel");
	if(lua_isfunction(L, -1)) //Providing a 'Cancel()' Fn is optional
	{
		constexpr int argc = 0;
		constexpr int retc = 0;
		auto ret = lua_pcall(L,argc,retc,0);
		if(ret != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			if(auto log = odc::spdlog_get(LoggerName))
				log->error("{}: Lua Cancel() call error: {}",ID,err);
			lua_pop(L,1);
		}
	}
	completed = true;
}
