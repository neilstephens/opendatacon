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
	mute_scripts(true),
	maxQ(500)
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
			return IUIResponder::GenerateResult("Bad parameter");

		},"Load a Lua script to automate sending UI commands. Usage: 'ExecuteFile <lua_filename> <ID> [script args]'. "
		  "ID is a arbitrary user-specified ID that can be used with 'StatExecution'");

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
			return IUIResponder::GenerateResult("Bad parameter");
		},"Similar to 'ExecuteFile', but instead of loading the lua code from file, decodes it directly from base64 entered at the console. "
		  "Usage: 'Base64Execute <base64_encoded_script> <ID> [script args]'");

	IUIResponder::AddCommand("StatExecution", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string ID;
			if(LineStream>>ID)
			{
			      return Status(ID);
			}
			return IUIResponder::GenerateResult("Bad parameter");
		},"Check the execution status of a command script started by 'ExecuteFile/Base64Execute'. Usage: 'StatExecution <ID>'");

	IUIResponder::AddCommand("CancelExecution", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			std::string ID;
			if(LineStream>>ID)
			{
			      Cancel(ID);
			      return IUIResponder::GenerateResult("Success");
			}
			return IUIResponder::GenerateResult("Bad parameter");
		},"Cancel the execution of a command script started by 'ExecuteFile' the next time it returns or yeilds. Usage: 'CancelExecution <ID>'");

	IUIResponder::AddCommand("ClearAll", [this](const ParamCollection &params) -> const Json::Value
		{
			ClearAll();
			return IUIResponder::GenerateResult("Success");
		},"Cancel execution of all scripts and clear all status info.");

	IUIResponder::AddCommand("ToggleMuteExecution", [this](const ParamCollection &) -> const Json::Value
		{
			mute_scripts = !mute_scripts;
			return mute_scripts ? IUIResponder::GenerateResult("Muted") : IUIResponder::GenerateResult("Unmuted");
		},"Toggle whether messages from command scripts will be printed to the console.");

	IUIResponder::AddCommand("MessageQueueSize", [this](const ParamCollection &params) -> const Json::Value
		{
			auto LineStream = ParamsToSStream(params);
			size_t sz;
			if(LineStream>>sz)
			{
			      maxQ = sz;
			      return IUIResponder::GenerateResult("Success");
			}
			return IUIResponder::GenerateResult("Bad parameter");
		},"Set the maximum number of messages that will be retained for each script (default 500). Usage: 'MessageQueueSize <integer>'");
}

LuaUICommander::~LuaUICommander()
{
	decltype(Scripts) destroy_outside_lock_scope;
	{
		std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
		destroy_outside_lock_scope = Scripts;
		Scripts.clear();
	}
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

void LuaUICommander::ScriptMessageHandler(const std::string& ID, const std::string& msg)
{
	if(!mute_scripts)
		std::cout<<"[Lua] "<<ID<<": "<<msg<<std::endl;

	std::unique_lock<std::shared_mutex> lck(MessagesMtx);
	Messages[ID].push_back(msg);
	while(Messages.at(ID).size() > maxQ)
		Messages.at(ID).pop_front();
}

bool LuaUICommander::Execute(const std::string& lua_code, const std::string& ID, std::stringstream& script_args)
{
	decltype(Scripts) destroy_outside_lock_scope;
	try
	{
		std::unique_lock<std::shared_mutex> lckS(ScriptsMtx);

		//first clear complete scripts
		for(auto&[id,pScript] : Scripts)
			if(pScript->Completed())
				destroy_outside_lock_scope[id] = pScript;
		for(auto&[id,pScript] : destroy_outside_lock_scope)
			Scripts.erase(id);

		std::unique_lock<std::shared_mutex> lckM(MessagesMtx);
		auto& pScript = Scripts[ID];
		if(pScript)
		{
			if(auto log = odc::spdlog_get(LoggerName))
				log->warn("There is already a running script with ID '{}'",ID);
			return false;
		}
		pScript = std::make_shared<LuaInst>(lua_code,CmdHandler,MsgHandler,LoggerName,ID,script_args);
		Messages.erase(ID);
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get(LoggerName))
			log->error("Failed loading Lua command script. Error: '{}'",e.what());
		return false;
	}
	return true;
}

void LuaUICommander::Cancel(const std::string& ID)
{
	decltype(Scripts)::node_type destroy_outside_lock_scope;
	{
		std::unique_lock<std::shared_mutex> lck(ScriptsMtx);
		destroy_outside_lock_scope = Scripts.extract(ID);
	}
}

void LuaUICommander::ClearAll()
{
	decltype(Scripts) destroy_outside_lock_scope;
	{
		std::unique_lock<std::shared_mutex> lckS(ScriptsMtx);
		std::unique_lock<std::shared_mutex> lckM(MessagesMtx);
		destroy_outside_lock_scope = Scripts;
		Scripts.clear();
		Messages.clear();
	}
}

bool LuaUICommander::Completed(const std::string& ID)
{
	std::shared_lock<std::shared_mutex> lck(ScriptsMtx);
	auto script_it = Scripts.find(ID);
	if(script_it != Scripts.end())
		return script_it->second->Completed();
	return true;
}

Json::Value LuaUICommander::Status(const std::string& ID)
{
	auto ret = Json::Value(Json::objectValue);
	ret["Execution status"] = Completed(ID) ? "Not running" : "Running";

	std::shared_lock<std::shared_mutex> lck(MessagesMtx);
	auto messages_it = Messages.find(ID);
	if(messages_it != Messages.end())
	{
		ret["Messages"] = Json::Value(Json::arrayValue);
		Json::ArrayIndex n=0;
		for(const auto& msg : messages_it->second)
			ret["Messages"][n++]=msg;
	}
	return ret;
}
