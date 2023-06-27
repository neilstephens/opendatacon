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

LuaUICommander::LuaUICommander(CommandHandler&& CmdHandler, const std::string& LoggerName):
	CmdHandler(CmdHandler),
	LoggerName(LoggerName)
{}

LuaUICommander::~LuaUICommander()
{
	std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
	Scripts.clear();
}

bool LuaUICommander::Execute(const std::string& lua_code, const std::string& ID)
{
	try
	{
		std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
		auto [itr,was_inserted] = Scripts.try_emplace(ID,lua_code,CmdHandler,ID,LoggerName);
		return was_inserted;
	}
	catch(const std::exception& e)
	{
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
