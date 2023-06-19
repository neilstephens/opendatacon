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

#include "LuaPort.h"
#include "LuaPortConf.h"
#include "CLua.h"
#include "Wrappers.h"
#include <opendatacon/util.h>

LuaPort::LuaPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<LuaPortConf>();
	ProcessFile();
}

LuaPort::~LuaPort()
{
	lua_close(LuaState);
}

void LuaPort::Enable()
{}
void LuaPort::Disable()
{}
void LuaPort::Build()
{
	//populate the Lua state with wrapped types and helper functions
	ExportWrappersToLua(LuaState,Name);
	ExportLuaPublishEvent();
	luaL_openlibs(LuaState);

	//load the lua code
	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());
	auto ret = luaL_dofile(LuaState, pConf->LuaFile.c_str());
	if(ret != LUA_OK)
		throw std::runtime_error(Name+": Failed to load LuaFile '"+pConf->LuaFile+"'. Error: "+lua_tostring(LuaState, -1));

	for(const auto& func_name : {"Enable","Disable","Build","Event"})
	{
		lua_getglobal(LuaState, func_name);
		if(!lua_isfunction(LuaState, -1))
			throw std::runtime_error(Name+": Lua code doesn't have '"+func_name+"' function.");
	}
}

void LuaPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(auto log = spdlog::get("LuaPort"))
		log->trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);

	//Get ready to call the lua function
	lua_getglobal(LuaState, "Event");

	//first arg
	PushEventInfo(event);

	//put sendername on stack as second arg
	lua_pushstring(LuaState, SenderName.c_str());

	//put callback on stack as a closure as last/third arg
	auto cb = new SharedStatusCallback_t(pStatusCallback);
	lua_pushlightuserdata(LuaState,cb);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			auto cb = static_cast<SharedStatusCallback_t*>(lua_touserdata(L, lua_upvalueindex(1)));
			auto cmd_stat = static_cast<CommandStatus>(lua_tointeger(L, -1));
			(**cb)(cmd_stat);
			delete cb;
			return 0; //number of lua ret vals pushed onto the stack
		}, 1);

	//now call lua Event()
	const int argc = 3; const int retc = 0;
	lua_pcall(LuaState,argc,retc,0);
}

void LuaPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto MemberNames = JSONRoot.getMemberNames();
	for(auto mn : MemberNames)
		JSONConf[mn] = JSONRoot[mn];

	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("LuaFile"))
		pConf->LuaFile = JSONRoot["LuaFile"].asString();
}

void LuaPort::PushEventInfo(std::shared_ptr<const EventInfo> event)
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

	//TODO: payload
}

void LuaPort::PopEventInfo(const std::shared_ptr<EventInfo>& event)
{
	//TODO:
}

void LuaPort::ExportLuaPublishEvent()
{
	//TODO:
}
