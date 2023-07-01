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
#include <opendatacon/IUI.h>
#include <opendatacon/IUIResponder.h>
#include <string>
#include <shared_mutex>
#include <unordered_map>

class LuaUICommander: public IUI, public IUIResponder
{
public:
	LuaUICommander(const std::string& Name, const std::string& File, const Json::Value& Overrides);
	~LuaUICommander();

	/* Implement IUI interface */
	void AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc = "No description available\n") override;
	void Build() override {}
	void Enable() override {}
	void Disable() override {}
	std::pair<std::string,const IUIResponder*> GetUIResponder() override { return {Name,this}; }

private:
	//IUI functionality
	const std::string Name;
	const std::string LoggerName = "LuaIUCommander";
	std::map<std::string,CmdFunc_t> mCmds;
	std::map<std::string,std::string> mDescriptions;

	//Lua functionality
	std::unordered_map<std::string,LuaInst> Scripts;
	std::shared_mutex ScriptsMtx;
	std::atomic_bool mute_scripts;

	Json::Value ScriptCommandHandler(const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss);
	bool Execute(const std::string& lua_code, const std::string& ID, std::stringstream& script_args);
	void Cancel(const std::string& ID);
	bool Completed(const std::string& ID);

	CommandHandler CmdHandler = [this](const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss) -> Json::Value
					    {
						    return ScriptCommandHandler(responder_name,cmd,args_iss);
					    };
	MessageHandler MsgHandler = [this](const std::string& ID, const std::string& msg)
					    {
						    if(!mute_scripts)
							    std::cout<<"[Lua] "<<ID<<": "<<msg<<std::endl;
					    };
};

#endif // LUAUICOMMANDER_H
