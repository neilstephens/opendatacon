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
	//TODO: check if destruction of ports is always sychronous
	//...and block if there's outstanding handlers etc.
	lua_close(LuaState);
}

void LuaPort::Enable()
{
	//TODO:
}
void LuaPort::Disable()
{
	//TODO:
}

//Build is called while there's only one active thread, so we don't need to sync access to LuaState here
void LuaPort::Build()
{
	//populate the Lua state with wrapped types and helper functions
	ExportWrappersToLua(LuaState,Name);
	ExportLuaPublishEvent();
	luaL_openlibs(LuaState);

	auto OSF = static_cast< std::underlying_type_t<odc::DataToStringMethod> >(DataToStringMethod::Hex);
	if (JSONConf.isMember("OctetStringFormat"))
	{
		auto fmt = JSONConf["OctetStringFormat"].asString();
		if(fmt == "Hex")
			OSF = static_cast< std::underlying_type_t<odc::DataToStringMethod> >(DataToStringMethod::Hex);
		else if(fmt == "Raw")
			OSF = static_cast< std::underlying_type_t<odc::DataToStringMethod> >(DataToStringMethod::Raw);
		else
			throw std::invalid_argument("OctetStringFormat should be Raw or Hex, not " + fmt);
	}
	lua_pushinteger(LuaState,OSF);
	lua_setglobal(LuaState,"OctetStringFormat");

	//load the lua code
	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());
	auto ret = luaL_dofile(LuaState, pConf->LuaFile.c_str());
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		throw std::runtime_error(Name+": Failed to load LuaFile '"+pConf->LuaFile+"'. Error: "+err);
	}

	for(const auto& func_name : {"Enable","Disable","Build","Event"})
	{
		lua_getglobal(LuaState, func_name);
		if(!lua_isfunction(LuaState, -1))
			throw std::runtime_error(Name+": Lua code doesn't have '"+func_name+"' function.");
	}
}

void LuaPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	//TODO: synch access to LuaState below

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
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		if(auto log = odc::spdlog_get("LuaPort"))
			log->error("{}: Lua Event() call error: {}",Name,err);
		lua_pop(LuaState,1);
	}
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

//PushEventInfo must only be called from the the LuaState sync strand
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

	//Payload
	lua_pushstring(LuaState, "Payload");
	PushPayload(LuaState,event->GetEventType(),event);
	lua_settable(LuaState, -3);
}
//PopEventInfo must only be called from the the LuaState sync strand
//	(or from lua code - which will be running on the strand anyway)
std::shared_ptr<EventInfo> LuaPort::PopEventInfo() const
{
	if(!lua_istable(LuaState,1))
	{
		if(auto log = odc::spdlog_get("LuaPort"))
			log->error("{}: EventInfo table argument not found.",Name);
		return nullptr;
	}

	//EventType
	lua_getfield(LuaState, 1, "EventType");
	if(!lua_isinteger(LuaState,-1))
		return nullptr;
	auto et = static_cast<EventType>(lua_tointeger(LuaState,-1));
	auto event = std::make_shared<EventInfo>(et);

	//Index
	lua_getfield(LuaState, 1, "Index");
	if(lua_isinteger(LuaState,-1))
		event->SetIndex(lua_tointeger(LuaState,-1));

	//SourcePort
	lua_getfield(LuaState, 1, "SourcePort");
	if(lua_isstring(LuaState,-1))
		event->SetSource(lua_tostring(LuaState,-1));
	else
		event->SetSource(Name);

	//QualityFlags
	lua_getfield(LuaState, 1, "QualityFlags");
	if(lua_isinteger(LuaState,-1))
		event->SetQuality(static_cast<QualityFlags>(lua_tointeger(LuaState,-1)));

	//Timestamp
	lua_getfield(LuaState, 1, "Timestamp");
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
			if(auto log = odc::spdlog_get("LuaPort"))
				log->error("{}: Lua EventInfo Timestamp error: {}",Name,e.what());
		}
	}

	//TODO: Payload
	event->SetPayload();

	return event;
}

//This is only called from Build(), so no sync required.
void LuaPort::ExportLuaPublishEvent()
{
	//push closure with one upvalue (to capture 'this')
	lua_pushlightuserdata(LuaState, this);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			//retrieve 'this'
			auto self = static_cast<const LuaPort*>(lua_topointer(L, lua_upvalueindex(1)));

			//first arg is EventInfo
			auto event = self->PopEventInfo();
			if(!event)
			{
			      lua_pushboolean(L, false);
			      return 1;
			}

			//optional second arg is command status callback
			if(lua_isfunction(L,2))
			{
			      lua_pushvalue(L,2);                             //push a copy, because luaL_ref pops
			      auto LuaCBref = luaL_ref(L, LUA_REGISTRYINDEX); //pop
			      auto cb = std::make_shared<std::function<void (CommandStatus status)>>([L,LuaCBref,self](CommandStatus status)
					{
						auto lua_status = static_cast< std::underlying_type_t<odc::CommandStatus> >(status);
						//get callback from the registry back on stack
						lua_rawgeti(L, LUA_REGISTRYINDEX, LuaCBref);
						//push the status argument
						lua_pushinteger(L,lua_status);

						//now call
						const int argc = 1; const int retc = 0;
						auto ret = lua_pcall(L,argc,retc,0);
						if(ret != LUA_OK)
						{
						      std::string err = lua_tostring(L, -1);
						      if(auto log = odc::spdlog_get("LuaPort"))
								log->error("{}: Lua PublishEvent() callback error: {}",self->Name,err);
						      lua_pop(L,1);
						}
					});
			      self->PublishEvent(event, cb);
			}
			else
				self->PublishEvent(event);

			lua_pushboolean(L, true);
			return 1; //number of lua ret vals pushed onto the stack
		}, 1);
	lua_setglobal(LuaState,"PublishEvent");
}
