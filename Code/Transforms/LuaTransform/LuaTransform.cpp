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
		throw std::runtime_error("LuaTransform requires 'Parameters' object with 'LuaFile' member string: {}"+params.toStyledString());
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
	PushEventInfo(event);

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
		return false;
	if(auto transformed = EventInfoFromLua(-1))
	{
		event.swap(transformed);
		return true;
	}
	return false;
}

//PushEventInfo must only be called from the the LuaState sync strand
void LuaTransform::PushEventInfo(std::shared_ptr<const EventInfo> event)
{
	//make a lua table for event info on stack
	lua_newtable(LuaState);

	//EventType
	auto lua_et = static_cast< std::underlying_type_t<odc::EventType> >(event->GetEventType());
	lua_pushstring(LuaState, "EventType");
	lua_pushinteger(LuaState, lua_et);
	lua_settable(LuaState, -3);

	//Index
	lua_pushstring(LuaState, "Index");
	lua_pushinteger(LuaState, event->GetIndex());
	lua_settable(LuaState, -3);

	//SourcePort
	lua_pushstring(LuaState, "SourcePort");
	lua_pushstring(LuaState, event->GetSourcePort().c_str());
	lua_settable(LuaState, -3);

	//Quality
	auto lua_q = static_cast< std::underlying_type_t<odc::QualityFlags> >(event->GetQuality());
	lua_pushstring(LuaState, "QualityFlags");
	lua_pushinteger(LuaState, lua_q);
	lua_settable(LuaState, -3);

	//Timestamp
	lua_pushstring(LuaState, "Timestamp");
	lua_pushinteger(LuaState, event->GetTimestamp());
	lua_settable(LuaState, -3);

	//Payload
	lua_pushstring(LuaState, "Payload");
	PushPayload(LuaState,event->GetEventType(),event);
	lua_settable(LuaState, -3);
}
//EventInfoFromLua must only be called from the the LuaState sync strand
//	(or from lua code - which will be running on the strand anyway)
std::shared_ptr<EventInfo> LuaTransform::EventInfoFromLua(int idx) const
{
	if(idx < 0)
		idx = lua_gettop(LuaState) + (idx+1);

	if(!lua_istable(LuaState,idx))
	{
		if(auto log = odc::spdlog_get("LuaTransform"))
			log->error("{}: EventInfo table argument not found.",Name);
		return nullptr;
	}

	//EventType
	lua_getfield(LuaState, idx, "EventType");
	if(!lua_isinteger(LuaState,-1))
		return nullptr;
	auto et = static_cast<EventType>(lua_tointeger(LuaState,-1));
	auto event = std::make_shared<EventInfo>(et);

	//Index
	lua_getfield(LuaState, idx, "Index");
	if(lua_isinteger(LuaState,-1))
		event->SetIndex(lua_tointeger(LuaState,-1));

	//SourcePort
	lua_getfield(LuaState, idx, "SourcePort");
	if(lua_isstring(LuaState,-1))
		event->SetSource(lua_tostring(LuaState,-1));

	//QualityFlags
	lua_getfield(LuaState, idx, "QualityFlags");
	if(lua_isinteger(LuaState,-1))
		event->SetQuality(static_cast<QualityFlags>(lua_tointeger(LuaState,-1)));

	//Timestamp
	lua_getfield(LuaState, idx, "Timestamp");
	if(lua_isinteger(LuaState,-1))
		event->SetTimestamp(lua_tointeger(LuaState,-1));
	else if(lua_isstring(LuaState,-1))
	{
		try
		{
			event->SetTimestamp(odc::datetime_to_since_epoch(lua_tostring(LuaState,-1)));
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("LuaTransform"))
				log->error("{}: Lua EventInfo Timestamp error: {}",Name,e.what());
		}
	}

	//Payload
	lua_getfield(LuaState, idx, "Payload");
	try
	{
		PopPayload(LuaState, event);
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get("LuaTransform"))
			log->error("{}: Lua EventInfo Payload error: {}",Name,e.what());
		event->SetPayload();
	}

	return event;
}
