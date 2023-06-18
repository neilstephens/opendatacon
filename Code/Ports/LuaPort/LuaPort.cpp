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
#include "CLua.h"
#include "IOTypeWrappers.h"
#include <opendatacon/util.h>

LuaPort::LuaPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<LuaPortConf>();
	ProcessFile();
}

void LuaPort::Enable()
{}
void LuaPort::Disable()
{}
void LuaPort::Build()
{
	ExportIOTypeWrappersToLua(LuaState,Name);
	luaL_openlibs(LuaState);
	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());
	auto ret = luaL_dofile(LuaState, pConf->LuaFile.c_str());
	if(ret != LUA_OK)
		throw std::runtime_error(Name+": Failed to load LuaFile '"+pConf->LuaFile+"'. Error: "+lua_tostring(LuaState, -1));

	for(const auto& func_name : {"Enable","Disable","ProcessConfig","EventHandler"})
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

	(*pStatusCallback)(CommandStatus::SUCCESS);
}

void LuaPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto pConf = static_cast<LuaPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("LuaFile"))
		pConf->LuaFile = JSONRoot["LuaFile"].asString();
}

