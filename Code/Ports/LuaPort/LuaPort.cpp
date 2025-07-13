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
#include "Log.h"
#include <Lua/CLua.h>
#include <Lua/Wrappers.h>
#include <opendatacon/util.h>
#include <opendatacon/MergeJsonConf.h>

LuaPort::LuaPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<LuaPortConf>();
	ProcessFile();
}

LuaPort::~LuaPort()
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
void LuaPort::Enable_()
{
	CallLuaGlobalVoidVoidFunc("Enable");
}
//only called on Lua sync strand
void LuaPort::Disable_()
{
	CallLuaGlobalVoidVoidFunc("Disable");
	//Force garbage collection now. Lingering shared_ptr finalizers can block shutdown etc
	lua_gc(LuaState,LUA_GCCOLLECT);
}

//Build is called while there's only one active thread, so we don't need to sync access to LuaState here
void LuaPort::Build()
{
	//top level table "odc"
	lua_newtable(LuaState);
	{
		//Export our name
		lua_pushstring(LuaState,Name.c_str());
		lua_setfield(LuaState,-2,"Name");

		//Export our JSON config
		PushJSON(LuaState,JSONConf);
		lua_setfield(LuaState,-2,"Config");
	}
	lua_setglobal(LuaState,"odc");

	ExportWrappersToLua(LuaState,pLuaSyncStrand,handler_tracker,Name,"LuaPort");
	ExportLuaPublishEvent();
	ExportLuaInDemand();
	luaL_openlibs(LuaState);

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

	CallLuaGlobalVoidVoidFunc("Build");
}

//only called on Lua sync strand
void LuaPort::Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(Log.ShouldLog(spdlog::level::trace))
		Log.Trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);

	//Get ready to call the lua function
	lua_getglobal(LuaState, "Event");

	//first arg
	PushEventInfo(LuaState,event);

	//put sendername on stack as second arg
	lua_pushstring(LuaState, SenderName.c_str());

	//put callback closure on stack as last/third arg
	//shared_ptr userdata up-value
	auto p = lua_newuserdatauv(LuaState,sizeof(std::shared_ptr<void>),0);
	new(p) std::shared_ptr<void>(pStatusCallback);
	luaL_getmetatable(LuaState, "std_shared_ptr_void");
	lua_setmetatable(LuaState, -2);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			auto cb_void = static_cast<std::shared_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
			auto cb = std::static_pointer_cast<SharedStatusCallback_t::element_type>(*cb_void);
			auto cmd_stat = static_cast<CommandStatus>(lua_tointeger(L, -1));
			(*cb)(cmd_stat);
			return 0; //number of lua ret vals pushed onto the stack
		}, 1);

	//now call lua Event()
	const int argc = 3; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		Log.Error("{}: Lua Event() call error: {}",Name,err);
		lua_pop(LuaState,1);
	}
}

void LuaPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	MergeJsonConf(JSONConf,JSONRoot);

	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("LuaFile"))
		pConf->LuaFile = JSONRoot["LuaFile"].asString();
}

//This is only called from Build(), so no sync required.
void LuaPort::ExportLuaPublishEvent()
{
	lua_getglobal(LuaState,"odc");
	//push closure with one upvalue (to capture 'this')
	lua_pushlightuserdata(LuaState, this);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			//retrieve 'this'
			auto self = static_cast<const LuaPort*>(lua_topointer(L, lua_upvalueindex(1)));

			//first arg is EventInfo
			auto event = EventInfoFromLua(L,self->Name,"LuaPort",1);
			if(!event)
			{
				lua_pushboolean(L, false);
				return 1;
			}
			if(event->GetSourcePort()=="")
				event->SetSource(self->Name);

			//optional second arg is command status callback
			if(lua_isfunction(L,2))
			{
				lua_pushvalue(L,2);                             //push a copy, because luaL_ref pops
				auto LuaCBref = luaL_ref(L, LUA_REGISTRYINDEX); //pop
				auto cb = std::make_shared<std::function<void (CommandStatus status)>>(self->pLuaSyncStrand->wrap([L,LuaCBref,self,h{self->handler_tracker}](CommandStatus status)
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
							Log.Error("{}: Lua PublishEvent() callback error: {}",self->Name,err);
							lua_pop(L,1);
						}
						//release the reference to the callback
						luaL_unref(L, LUA_REGISTRYINDEX, LuaCBref);
					}));
				self->PublishEvent(event, cb);
			}
			else
				self->PublishEvent(event);

			lua_pushboolean(L, true);
			return 1; //number of lua ret vals pushed onto the stack
		}, 1);
	lua_setfield(LuaState,-2,"PublishEvent");
}

//This is only called from Build(), so no sync required.
void LuaPort::ExportLuaInDemand()
{
	lua_getglobal(LuaState,"odc");
	//push closure with one upvalue (to capture 'this')
	lua_pushlightuserdata(LuaState, this);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			//retrieve 'this'
			auto self = static_cast<const LuaPort*>(lua_topointer(L, lua_upvalueindex(1)));
			auto in_demand = self->InDemand();
			lua_pushboolean(L, in_demand);
			return 1;
		}, 1);
	lua_setfield(LuaState,-2,"InDemand");
}

//Must only be called from the the LuaState sync strand
void LuaPort::CallLuaGlobalVoidVoidFunc(const std::string& FnName)
{
	lua_getglobal(LuaState, FnName.c_str());

	//no args, so just call it
	const int argc = 0; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		Log.Error("{}: Lua {}() call error: {}",Name,FnName,err);
		lua_pop(LuaState,1);
	}
}
