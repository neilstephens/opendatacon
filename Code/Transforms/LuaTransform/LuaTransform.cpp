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
 * LuaPort.cpp
 *
 *  Created on: 17/06/2023
 *      Author: Neil Stephens
 */

#include "LuaTransform.h"
#include <Lua/CLua.h>
#include <Lua/Wrappers.h>
#include <opendatacon/util.h>

LuaTransform::LuaTransform(const std::string& Name, const Json::Value& params): Transform(Name,params)
{
	if(!params.isObject() || !params.isMember("LuaFile") || !params["LuaFile"].isString())
		throw std::runtime_error("LuaTransform requires 'Parameters' object with 'LuaFile' member string: "+params.toStyledString());
	//top level table "odc"
	lua_newtable(LuaState);
	{
		//Export our JSON params
		PushJSON(LuaState,params);
		lua_setfield(LuaState,-2,"Parameters");
	}
	lua_setglobal(LuaState,"odc");

	ExportWrappersToLua(LuaState,pLuaSyncStrand,handler_tracker,Name,"LuaTransform");
	luaL_openlibs(LuaState);

	//load the lua code
	auto ret = luaL_dofile(LuaState, params["LuaFile"].asCString());
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		throw std::runtime_error(Name+": Failed to load LuaFile '"+params["LuaFile"].asString()+"'. Error: "+err);
	}

	lua_getglobal(LuaState, "Event");
	if(!lua_isfunction(LuaState, -1))
		throw std::runtime_error(Name+": Lua code doesn't have 'Event' function.");
}

LuaTransform::~LuaTransform()
{
	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	lua_close(LuaState);
}

//only called on Lua sync strand
bool LuaTransform::Event_(std::shared_ptr<EventInfo> event)
{
	//Get ready to call the lua function
	lua_getglobal(LuaState, "Event");

	//first arg
	PushEventInfo(LuaState,event);

	//now call lua Event()
	const int argc = 1; const int retc = 1;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		if(auto log = odc::spdlog_get("LuaTransform"))
			log->error("{}: Lua Event() call error: {}",Name,err);
		lua_pop(LuaState,1);
		return false;
	}
	if(!lua_istable(LuaState,-1))
	{
		lua_pop(LuaState,1);
		return false;
	}
	if(auto transformed = EventInfoFromLua(LuaState,Name,"LuaTransform",-1))
	{
		std::destroy_at(event.get());
		new(event.get()) EventInfo(*transformed.get());
		lua_pop(LuaState,1);
		return true;
	}
	lua_pop(LuaState,1);
	return false;
}
