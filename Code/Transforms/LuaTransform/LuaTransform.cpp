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
#include "Log.h"
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
	lua_gc(LuaState,LUA_GCCOLLECT);

	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	lua_close(LuaState);
}

//only called on Lua sync strand
void LuaTransform::Enable_()
{
	lua_getglobal(LuaState, "Enable");
	if(!lua_isfunction(LuaState, -1))
		return; //Enable is optional

	const int argc = 0; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		Log.Error("{}: Lua Enable() call error: {}",Name,err);
		lua_pop(LuaState,1);
	}
}

//only called on Lua sync strand
void LuaTransform::Disable_()
{
	lua_getglobal(LuaState, "Disable");
	if(lua_isfunction(LuaState, -1)) //Disable is optional
	{
		const int argc = 0; const int retc = 0;
		auto ret = lua_pcall(LuaState,argc,retc,0);
		if(ret != LUA_OK)
		{
			std::string err = lua_tostring(LuaState, -1);
			Log.Error("{}: Lua Disable() call error: {}",Name,err);
			lua_pop(LuaState,1);
		}
	}
	//Force garbage collection now. Lingering shared_ptr finalizers can block shutdown etc
	lua_gc(LuaState,LUA_GCCOLLECT);
}

//only called on Lua sync strand
void LuaTransform::Event_(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow)
{
	//Get ready to call the lua function
	lua_getglobal(LuaState, "Event");

	//first arg
	PushEventInfo(LuaState,event);

	//second arg - closure with callback and Name as upvalues
	auto p = lua_newuserdatauv(LuaState,sizeof(std::shared_ptr<void>),0);
	new(p) std::shared_ptr<void>(pAllow);
	luaL_getmetatable(LuaState, "std_shared_ptr_void");
	lua_setmetatable(LuaState, -2);
	lua_pushstring(LuaState,Name.c_str());
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			auto cb_void = static_cast<std::shared_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
			auto pAllow = std::static_pointer_cast<EvtHandler_ptr::element_type>(*cb_void);
			std::string Name = lua_tostring(L,lua_upvalueindex(2));

			auto event = EventInfoFromLua(L,Name,"LuaTransform",1);
			(*pAllow)(event);
			return 0; //number of lua ret vals pushed onto the stack
		}, 2);

	//now call lua Event()
	const int argc = 2; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		Log.Error("{}: Lua Event() call error: {}",Name,err);
		lua_pop(LuaState,1);
	}
}
