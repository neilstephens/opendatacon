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

#include "ConsoleUI.h"
#include <opendatacon/util.h>
#include <fstream>
#include <iomanip>
#include <exception>

using namespace odc;

ConsoleUI::ConsoleUI():
	tinyConsole("odc> "),
	context(""),
	mute_scripts(false),
	ScriptRunner([this](const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss) -> Json::Value
		{
			return ScriptCommandHandler(responder_name,cmd,args_iss);
		},
		[this](const std::string& ID, const std::string& msg)
		{
			if(!mute_scripts)
				std::cout<<"[Lua] "<<ID<<": "<<msg<<std::endl;
		},
		"ConsoleUI")
{
	AddHelp("If commands in context to a collection (Eg. DataPorts etc.) require parameters, "
		  "the first argument is taken as a regex to match which items in the collection the "
		  "command will run for. The remainer of the arguments should be parameter Name/Value pairs."
		  "Eg. Collection Dosomething \".*OnlyAFewMatch.*\" arg_to_dosomething ...");
	AddCommand("help",[this](std::stringstream& LineStream) -> Json::Value
		{
			std::string arg;
			std::cout<<std::endl;
			if(LineStream>>arg) //asking for help on a specific command
			{
			      if(!mDescriptions.count(arg) && !Responders.count(arg))
					std::cout<<"No such command or context: '"<<arg<<"'"<<std::endl;
			      else if(mDescriptions.count(arg))
					std::cout<<std::setw(25)<<std::left<<arg+":"<<mDescriptions[arg]<<std::endl<<std::endl;
			      else if(Responders.count(arg))
			      {
			            std::cout<<std::setw(25)<<std::left<<arg+":"<<
			            "Access contextual subcommands:"<<std::endl<<std::endl;
			/* list sub commands */
			            auto commands = Responders[arg]->GetCommandList();
			            for (const auto& command : commands)
			            {
			                  auto cmd = command.asString();
			                  auto desc = Responders[arg]->GetCommandDescription(cmd);
			                  std::cout<<std::setw(25)<<std::left<<"\t"+cmd+":"<<desc<<std::endl<<std::endl;
					}
				}
			}
			else
			{
			      std::cout<<help_intro<<std::endl<<std::endl;
			//print root commands with descriptions
			      for(const auto& desc: mDescriptions)
			      {
			            std::cout<<std::setw(25)<<std::left<<desc.first+":"<<desc.second<<std::endl<<std::endl;
				}
			//if there is no context, print Responders
			      if (this->context.empty())
			      {
			//check if command matches a Responder - if so, arg is our partial sub command
			            for(const auto& name_n_responder : Responders)
			            {
			                  std::cout<<std::setw(25)<<std::left<<name_n_responder.first+":"<<
			                  "Access contextual subcommands."<<std::endl<<std::endl;
					}
				}
			      else //we have context - list sub commands
			      {
			            /* list commands available to current responder */
			            auto commands = Responders[this->context]->GetCommandList();
			            for (const auto& command : commands)
			            {
			                  auto cmd = command.asString();
			                  auto desc = Responders[this->context]->GetCommandDescription(cmd);
			                  std::cout<<std::setw(25)<<std::left<<cmd+":"<<desc<<std::endl<<std::endl;
					}
			            std::cout<<std::setw(25)<<std::left<<"exit:"<<
			            "Leave '"+context+"' context."<<std::endl<<std::endl;
				}
			}
			std::cout<<std::endl;
			return IUIResponder::GenerateResult("Success");
		},"Get help on commands. Optional argument of specific command.");

	AddCommand("run_cmd_script",[this](std::stringstream& LineStream) -> Json::Value
		{
			std::string filename,ID;
			if(LineStream>>filename && LineStream>>ID)
			{
			      std::ifstream fin(filename);
			      if(fin.is_open())
			      {
			            std::string lua_code((std::istreambuf_iterator<char>(fin)),std::istreambuf_iterator<char>());
			            auto started = ScriptRunner.Execute(lua_code,ID,LineStream);
			            auto msg = "Execution start: "+std::string(started ? "SUCCESS" : "FAILURE");
			            return IUIResponder::GenerateResult(msg);
				}
			      return IUIResponder::GenerateResult("Failed to open file.");
			}
			std::cout<<"Usage: 'run_cmd_script <lua_filename> <ID> [script args]'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Load a Lua script to automate sending UI commands. Usage: 'run_cmd_script <lua_filename> <ID> [script args]'. ID is a arbitrary user-specified ID that can be used with 'stat_cmd_script'");

	AddCommand("base64_cmd_script",[this](std::stringstream& LineStream) -> Json::Value
		{
			std::string Base64,ID;
			if(LineStream>>Base64 && LineStream>>ID)
			{
			      std::string lua_code = b64decode(Base64);
			      if(auto log = odc::spdlog_get("ConsoleUI"))
					log->trace("Decoded base64 as:\n{}",lua_code);
			      auto started = ScriptRunner.Execute(lua_code,ID,LineStream);
			      auto msg = "Execution start: "+std::string(started ? "SUCCESS" : "FAILURE");
			      return IUIResponder::GenerateResult(msg);
			}
			std::cout<<"Usage: 'base64_cmd_script <base64_encoded_script> <ID> [script args]'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Similar to 'run_cmd_script', but instead of loading the lua code from file, decodes it directly from base64 entered at the console");

	AddCommand("stat_cmd_script",[this](std::stringstream& LineStream) -> Json::Value
		{
			std::string ID;
			if(LineStream>>ID)
			{
			      auto msg = "Script "+ID+(ScriptRunner.Completed(ID) ? " completed" : " running");
			      return IUIResponder::GenerateResult(msg);
			}
			std::cout<<"Usage: 'stat_cmd_script <ID>'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Check the execution status of a command script started by 'run_cmd_script'. Usage: 'stat_cmd_script <ID>'");

	AddCommand("cancel_cmd_script",[this](std::stringstream& LineStream) -> Json::Value
		{
			std::string ID;
			if(LineStream>>ID)
			{
			      ScriptRunner.Cancel(ID);
			      return IUIResponder::GenerateResult("Success");
			}
			std::cout<<"Usage: 'cancel_cmd_script <ID>'"<<std::endl;
			return IUIResponder::GenerateResult("Bad parameter");
		},"Cancel the execution of a command script started by 'run_cmd_script' the next time it returns or yeilds. Usage: 'cancel_cmd_script <ID>'");

	AddCommand("toggle_script_mute",[this](std::stringstream& LineStream) -> Json::Value
		{
			mute_scripts = !mute_scripts;
			return mute_scripts ? IUIResponder::GenerateResult("Muted") : IUIResponder::GenerateResult("Unmuted");
		},"Toggle whether messages from command scripts will be printed to the console.");
}

ConsoleUI::~ConsoleUI(void)
{
	Disable();
}

void ConsoleUI::AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc)
{
	mCmds[name] = callback;
	mDescriptions[name] = desc;

	int width = 0;
	for(size_t i=0; i < mDescriptions[name].size(); i++)
	{
		if(++width > 80)
		{
			while(mDescriptions[name][i] != ' ')
				i--;
			mDescriptions[name].insert(i,"\n                        ");
			i+=26;
			width = 0;
		}
	}
}
void ConsoleUI::AddHelp(std::string help)
{
	help_intro = std::move(help);
	int width = 0;
	for(size_t i=0; i < help_intro.size(); i++)
	{
		if(++width > 105)
		{
			while(help_intro[i] != ' ')
				i--;
			help_intro.insert(i,"\n");
			width = 0;
		}
	}
}

int ConsoleUI::trigger (const std::string& s)
{
	std::stringstream LineStream(s);
	std::string cmd;
	LineStream>>cmd;

	if(this->context.empty() && Responders.count(cmd))
	{
		/* responder */
		std::string rcmd;
		LineStream>>rcmd;

		if (rcmd.length() == 0)
		{
			/* change context */
			this->context = cmd;
			this->_prompt = "odc " + cmd + "> ";
		}
		else
		{
			std::cout<<ExecuteCommand(Responders[cmd],rcmd,LineStream).toStyledString()<<std::endl;
		}
	}
	else if(!this->context.empty() && cmd == "exit")
	{
		/* change context */
		this->context = "";
		this->_prompt = "odc> ";
	}
	else if(!this->context.empty())
	{
		if(mCmds.count(cmd))
		{
			/* regular command */
			std::cout<<mCmds[cmd](LineStream).toStyledString()<<std::endl;
		}
		else
		{
			//contextual command
			std::cout<<ExecuteCommand(Responders[this->context],cmd,LineStream).toStyledString()<<std::endl;
		}
	}
	else if(mCmds.count(cmd))
	{
		/* regular command */
		std::cout<<mCmds[cmd](LineStream).toStyledString()<<std::endl;
	}
	else
	{
		if(cmd != "")
			std::cout <<"Unknown command: "<< cmd << std::endl;
	}

	return 0;
}

int ConsoleUI::hotkeys(char c)
{
	if (c == TAB) //auto complete/list
	{
		std::stringstream stream(std::string(buffer.begin(), buffer.end()));

		std::string cmd, sub_cmd;
		stream >> cmd;
		stream >> sub_cmd;

		std::string history_cmd;
		std::string s;
		while (stream >> s)
			history_cmd += s + " ";

		std::vector<std::string> matches;
		AddRootCommands(cmd, matches);
		AddCommands(cmd, sub_cmd, matches);
		PrintMatches(cmd, sub_cmd, history_cmd, matches);

		return 1;
	}
	return 0;
}

Json::Value ConsoleUI::ScriptCommandHandler(const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss)
{
	auto root_cmd_it = mCmds.find(cmd);
	if(responder_name == "" && root_cmd_it != mCmds.end())
		return mCmds.at(cmd)(args_iss);

	auto it = Responders.find(responder_name);
	if(it != Responders.end())
		return ExecuteCommand(it->second, cmd, args_iss);

	return IUIResponder::GenerateResult("Bad command");
}

Json::Value ConsoleUI::ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args)
{
	ParamCollection params;

	//Define first arg as Target regex
	std::string T_regex_str;
	extract_delimited_string("\"'`",args,T_regex_str);

	//turn any regex it into a list of targets
	Json::Value target_list;
	if(!T_regex_str.empty())
	{
		params["Target"] = T_regex_str;
		if(command != "List") //Use List to handle the regex for the other commands
			target_list = pResponder->ExecuteCommand("List", params)["Items"];
	}

	int arg_num = 0;
	std::string Val;
	while(extract_delimited_string("\"'`",args,Val))
		params[std::to_string(arg_num++)] = Val;

	Json::Value results;
	if(target_list.size() > 0) //there was a list resolved
	{
		for(auto& target : target_list)
		{
			params["Target"] = target.asString();
			results[target.asString()] = pResponder->ExecuteCommand(command, params);
		}
	}
	else //There was no list - execute anyway
		results = pResponder->ExecuteCommand(command, params);

	return results;
}

void ConsoleUI::AddRootCommands(const std::string& cmd, std::vector<std::string>& matches)
{
	std::string lower_cmd = to_lower(cmd);
	for (auto name : mDescriptions)
	{
		std::string lower_name = to_lower(name.first);
		if (lower_name.substr(0, lower_cmd.size()) == lower_cmd)
			matches.push_back(name.first);
	}
}

void ConsoleUI::AddCommands(const std::string& cmd, const std::string& sub_cmd, std::vector<std::string>& matches)
{
	std::string lower_cmd = to_lower(cmd);
	std::string lower_sub_cmd = to_lower(sub_cmd);

	if (context.empty())
	{
		if (Responders.count(cmd))
		{
			for (auto command : Responders[cmd]->GetCommandList())
			{
				std::string lower_name = to_lower(command.asString());
				if (lower_name.substr(0, lower_sub_cmd.size()) == lower_sub_cmd)
					matches.push_back(cmd + " " + command.asString());
			}
		}
		else
		{
			for (auto name : Responders)
			{
				std::string lower_name = to_lower(name.first);
				if (lower_name.substr(0, lower_cmd.size()) == lower_cmd)
					matches.push_back(name.first);
			}
		}
	}
	else
	{
		for (auto command : Responders[context]->GetCommandList())
		{
			std::string lower_name = to_lower(command.asString());
			if (lower_name.substr(0, lower_cmd.size()) == lower_cmd)
				matches.push_back(command.asString());
		}
	}
}

void ConsoleUI::PrintMatches(const std::string& cmd, const std::string& sub_cmd,
	const std::string& history_cmd, const std::vector<std::string>& matches)
{
	std::string prompt;
	if (!matches.empty())
	{
		if (matches.size() == 1)
		{
			/*
			 * This line is to clear the current line so we can make the new prompt
			 * It is easy to make the prompt than adding or not.
			 */
			for(auto i = _prompt.size()+buffer.size(); i>0; i--)
				std::cout<<"\b \b";
			std::cout << _prompt << std::flush;
			prompt = matches[0] + " ";
			if (matches[0].find(" ") == std::string::npos && !sub_cmd.empty())
				prompt += sub_cmd + " ";
			if (!history_cmd.empty()) prompt += history_cmd + " ";
			std::cout << prompt << std::flush;
		}
		else
		{
			std::cout << std::endl << std::flush;
			for (const std::string& c : matches)
			{
				if (!history_cmd.empty())
					std::cout << c << " " << history_cmd << std::endl << std::flush;
				else
					std::cout << c << std::endl << std::flush;
			}
			std::size_t common_chars_count = 0;
			while (common_chars_count < matches[0].size())
			{
				std::size_t diff = 0;
				for (std::size_t i = 1; i < matches.size(); ++i)
					diff += std::tolower(matches[i][common_chars_count]) - std::tolower(matches[0][common_chars_count]);
				if (diff)
					break;
				++common_chars_count;
			}
			prompt = matches[0].substr(0, common_chars_count);
			if (!history_cmd.empty())
				prompt += " " + history_cmd;
			std::cout << _prompt << prompt << std::flush;
		}
		buffer.assign(prompt.begin(), prompt.end());
		line_pos = buffer.size();
	}
}

void ConsoleUI::Build()
{}

void ConsoleUI::Enable()
{
	this->_quit = false;
	if (!uithread)
	{
		uithread = std::make_unique<asio::thread>([this]()
			{
				this->run();
			});
	}
}

void ConsoleUI::Disable()
{
	this->quit();
	if (uithread)
	{
		uithread->join();
		uithread.reset();
	}
}


