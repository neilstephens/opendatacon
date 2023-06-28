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
 * LuaUICommander.h
 *
 *  Created on: 26/6/2023
 *      Author: Neil Stephens
 */
#ifndef LUAUICOMMANDER_H
#define LUAUICOMMANDER_H

#include "LuaInst.h"
#include <string>
#include <shared_mutex>
#include <unordered_map>

class LuaUICommander
{
public:
	LuaUICommander(CommandHandler&& CmdHandler, const std::string& LoggerName);
	~LuaUICommander();
	bool Execute(const std::string& lua_code, const std::string& ID, std::stringstream& script_args);
	void Cancel(const std::string& ID);
	bool Completed(const std::string& ID);

private:
	std::unordered_map<std::string,LuaInst> Scripts;
	std::shared_mutex ScriptsMtx;
	CommandHandler CmdHandler;
	const std::string LoggerName;
};

#endif // LUAUICOMMANDER_H
