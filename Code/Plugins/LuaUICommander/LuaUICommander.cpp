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

inline std::stringstream ParamsToSStream(const ParamCollection &params)
{
	std::stringstream script_args;
	for(size_t i=0; params.count(std::to_string(i)); i++)
	{
		script_args << params.at(std::to_string(i)) << std::endl;
	}
	return script_args;
}

LuaUICommander::LuaUICommander(const std::string& Name, const std::string& File, const Json::Value& Overrides):
	Name(Name),
	mute_scripts(true)
{
	IUIResponder::AddCommand("ExecuteFile", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string filename,ID;
			if(LineStream>>filename && LineStream>>ID)
			{
			      std::ifstream fin(filename);
			      if(fin.is_open())
			      {
			            std::string lua_code((std::istreambuf_iterator<char>(fin)),std::istreambuf_iterator<char>());
			            auto started = Execute(lua_code,ID,LineStream);
			            auto msg = "Execution start: "+std::string(started ? "SUCCESS" : "FAILURE");
			            return IUIResponder::GenerateResult(msg);
				}
			      return IUIResponder::GenerateResult("Failed to open file.");
			}
			return IUIResponder::GenerateResult("Usage: 'ExecuteFile <lua_filename> <ID> [script args]'");

		},"Load a Lua script to automate sending UI commands. Usage: 'ExecuteFile <lua_filename> <ID> [script args]'. ID is a arbitrary user-specified ID that can be used with 'StatExecution'");

	IUIResponder::AddCommand("Base64Execute", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string Base64,ID;
			if(LineStream>>Base64 && LineStream>>ID)
			{
			      std::string lua_code = odc::b64decode(Base64);
			      if(auto log = odc::spdlog_get("ConsoleUI"))
					log->trace("Decoded base64 as:\n{}",lua_code);
			      auto started = Execute(lua_code,ID,LineStream);
			      auto msg = "Execution start: "+std::string(started ? "SUCCESS" : "FAILURE");
			      return IUIResponder::GenerateResult(msg);
			}
			std::cout<<"Usage: 'Base64Execute <base64_encoded_script> <ID> [script args]'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Similar to 'ExecuteFile', but instead of loading the lua code from file, decodes it directly from base64 entered at the console");

	IUIResponder::AddCommand("StatExecution", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string ID;
			if(LineStream>>ID)
			{
			      auto msg = "Script "+ID+(Completed(ID) ? " completed" : " running");
			      return IUIResponder::GenerateResult(msg);
			}
			std::cout<<"Usage: 'StatExecution <ID>'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Check the execution status of a command script started by 'ExecuteFile'. Usage: 'StatExecution <ID>'");

	IUIResponder::AddCommand("CancelExecution", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string ID;
			if(LineStream>>ID)
			{
			      Cancel(ID);
			      return IUIResponder::GenerateResult("Success");
			}
			std::cout<<"Usage: 'CancelExecution <ID>'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Cancel the execution of a command script started by 'ExecuteFile' the next time it returns or yeilds. Usage: 'CancelExecution <ID>'");

	IUIResponder::AddCommand("ToggleMuteExecution", [this](const ParamCollection &) -> const Json::Value
		{
			mute_scripts = !mute_scripts;
			return mute_scripts ? IUIResponder::GenerateResult("Muted") : IUIResponder::GenerateResult("Unmuted");
		},"Toggle whether messages from command scripts will be printed to the console.");
}

LuaUICommander::~LuaUICommander()
{
	std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
	Scripts.clear();
}

void LuaUICommander::AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc)
{
	mCmds[name] = callback;
	mDescriptions[name] = desc;
}

Json::Value LuaUICommander::ScriptCommandHandler(const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss)
{
	auto root_cmd_it = mCmds.find(cmd);
	if(responder_name == "" && root_cmd_it != mCmds.end())
		return mCmds.at(cmd)(args_iss);

	auto it = Responders.find(responder_name);
	if(it != Responders.end())
		return IUI::ExecuteCommand(it->second, cmd, args_iss);

	return IUIResponder::GenerateResult("Bad command");
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
		if(!was_inserted)
		{
			if(auto log = odc::spdlog_get(LoggerName))
				log->warn("There is already a running script with ID '{}'",ID);
		}
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
