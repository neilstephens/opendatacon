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
 * LuaUICommander.cpp
 *
 *  Created on: 26/6/2023
 *      Author: Neil Stephens
 */
#include "LuaUICommander.h"
#include <opendatacon/util.h>

LuaUICommander::LuaUICommander(CommandHandler&& CmdHandler, MessageHandler&& MsgHandler, const std::string& LoggerName):
	CmdHandler(CmdHandler),
	MsgHandler(MsgHandler),
	LoggerName(LoggerName)
{}

LuaUICommander::~LuaUICommander()
{
	std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
	Scripts.clear();
}

bool LuaUICommander::Execute(const std::string& lua_code, const std::string& ID, std::stringstream& script_args)
{
	try
	{
		std::unique_lock<std::shared_mutex> lck(ScriptsMtx);

		//first clear complete scripts
		std::vector<std::string> rem;
		for(auto&[id,script] : Scripts)
			if(script.Completed())
				rem.push_back(id);
		for(auto& id : rem)
			Scripts.erase(id);

		auto [itr,was_inserted] = Scripts.try_emplace(ID,lua_code,CmdHandler,MsgHandler,LoggerName,ID,script_args);
		//TODO: log warning on clash
		return was_inserted;
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get(LoggerName))
			log->error("Failed loading Lua command script. Error: '{}'",e.what());
		return false;
	}
}

void LuaUICommander::Cancel(const std::string& ID)
{
	std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
	Scripts.erase(ID);
}

bool LuaUICommander::Completed(const std::string& ID)
{
	std::shared_lock<std::shared_mutex> lck(ScriptsMtx);
	auto script_it = Scripts.find(ID);
	if(script_it != Scripts.end())
		return script_it->second.Completed();
	return true;
}
