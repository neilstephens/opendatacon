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
#include <exception>
#include <fstream>
#include <iomanip>
#include <memory>
#include <opendatacon/Version.h>
#include <opendatacon/util.h>
#include <utility>

using namespace odc;

ConsoleUI::ConsoleUI():
	tinyConsole("odc> "),
	context("")
{
	AddHelp("If commands in context to a collection (Eg. DataPorts etc.) require parameters, "
		  "the first argument is taken as a regex to match which items in the collection the "
		  "command will run for. The remainer of the arguments should be parameter Name/Value pairs."
		  "Eg. Loggers ignore \".*\" filter \"[Aa]n{2}oying\\smessage\"");
	AddCommand("help",[this](std::stringstream& LineStream)
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
		},"Get help on commands. Optional argument of specific command.");
}

ConsoleUI::~ConsoleUI(void)
{
	Disable();
}

void ConsoleUI::AddCommand(const std::string& name, std::function<void (std::stringstream&)> callback, const std::string& desc)
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
	std::string cmd, lower_cmd;
	LineStream>>cmd;
	lower_cmd = cmd;
	ToLower(lower_cmd);

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
			ExecuteCommand(Responders[cmd],rcmd,LineStream);
		}
	}
	else if(!this->context.empty() && lower_cmd == "exit")
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
			mCmds[cmd](LineStream);
		}
		else
		{
			//contextual command
			ExecuteCommand(Responders[this->context],cmd,LineStream);
		}
	}
	else if(mCmds.count(cmd))
	{
		/* regular command */

		mCmds[cmd](LineStream);
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
		//store what's been entered so far
		std::string cmd, lower_cmd;
		cmd.assign(buffer.begin(), buffer.end());

		lower_cmd = cmd;
		ToLower(lower_cmd);

		//find root commands that start with the partial
		std::vector<std::string> matching_cmds;
		for(const auto& name_n_description : mDescriptions)
		{
			std::string name = name_n_description.first;
			ToLower(name);
			if (name.substr(0, lower_cmd.size()) == lower_cmd)
				matching_cmds.push_back(name_n_description.first);
		}

		//find contextual commands that start with the partial
		if (this->context.empty())
		{
			//check if command matches a Responder - if so, arg is our partial sub command
			if (Responders.count(cmd))
			{
				/* list commands avaialble to responder */
				auto commands = Responders[cmd]->GetCommandList();
				for (const auto& command : commands)
				{
					std::string cmd = command.asString();
					ToLower(cmd);
					if (cmd.substr(0, lower_cmd.size()) == lower_cmd)
						matching_cmds.push_back(cmd + " " + command.asString());
				}
			}
			//if not, cmd is a partial Responder
			else
			{
				/* list all matching responders */
				for(const auto& name_n_responder : Responders)
				{
					std::string name = name_n_responder.first;
					ToLower(name);
					if (name.substr(0, lower_cmd.size()) == lower_cmd)
						matching_cmds.emplace_back(name_n_responder.first);
				}
			}
		}
		else //we have context - cmd is a partial sub command
		{
			/* list commands available to current responder */
			auto commands = Responders[this->context]->GetCommandList();
			for (const auto& command : commands)
			{
				std::string cmd = command.asString();
				ToLower(cmd);
				if (cmd.substr(0, lower_cmd.size()) == lower_cmd)
					matching_cmds.push_back(command.asString());
			}
		}

		//any matches?
		if(matching_cmds.size())
		{
			//we want to see how many chars all the matches have in common
			auto common_length = cmd.size() - 1; //starting from what we already know matched

			if(matching_cmds.size() == 1)
			{
				common_length = matching_cmds.back().size();
			}
			else
			{
				bool common = true;
				//iterate over each character while it's common to all
				while(common)
				{
					common_length++;
					char ch = matching_cmds[0][common_length];
					for(auto& matching_cmd : matching_cmds)
					{
						if(matching_cmd[common_length] != ch)
						{
							common = false;
							break;
						}
					}
				}
			}

			//auto-complete common chars
			if(common_length > cmd.size())
			{
				buffer.assign(matching_cmds.back().begin(),matching_cmds.back().begin()+common_length);
				std::string remainder;
				remainder.assign(matching_cmds.back().begin() + cmd.size(), matching_cmds.back().begin() + common_length);
				std::cout<<remainder<< std::flush;
				line_pos = (int)common_length;
			}
			//otherwise we're at the branching point - list possible commands
			else if(matching_cmds.size() > 1)
			{
				std::cout<<std::endl;
				for(const auto& cmd : matching_cmds)
					std::cout<<cmd<<std::endl;
				std::cout<<_prompt<<cmd<<std::flush;
			}

			//if there's just one match, and we just auto-completed it, print a trailing space.
			if(matching_cmds.size() == 1 && cmd.size() <= matching_cmds.back().size())
			{
				std::cout<<' '<< std::flush;
				buffer.push_back(' ');
				line_pos++;
			}
		}

		return 1;
	}
	return 0;
}


void ConsoleUI::AddResponder(const std::string& name, const IUIResponder& pResponder)
{
	Responders[ name ] = &pResponder;
}

void ConsoleUI::ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args)
{
	ParamCollection params;

	//Define first arg as Target regex
	std::string T_regex_str;
	extract_delimited_string("\"'`",args,T_regex_str);

	//turn any regex it into a list of targets
	Json::Value target_list;
	if(!T_regex_str.empty() && command != "List") //List is a special case - handles it's own regex
	{
		params["Target"] = T_regex_str;
		target_list = pResponder->ExecuteCommand("List", params)["Items"];
	}

	int arg_num = 0;
	std::string Val;
	while(extract_delimited_string("\"'`",args,Val))
		params[std::to_string(arg_num++)] = Val;

	if(target_list.size() > 0) //there was a list resolved
	{
		for(auto& target : target_list)
		{
			params["Target"] = target.asString();
			auto result = pResponder->ExecuteCommand(command, params);
			std::cout<<target.asString()+":\n"<<result.toStyledString()<<std::endl;
		}
	}
	else //There was no list - execute anyway
	{
		auto result = pResponder->ExecuteCommand(command, params);
		std::cout<<result.toStyledString()<<std::endl;
	}
}

void ConsoleUI::ToLower(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
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


