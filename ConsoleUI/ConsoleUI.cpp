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
#include <opendatacon/Version.h>
#include <opendatacon/util.h>
#include <fstream>
#include <iomanip>
#include <exception>

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
			ExecuteCommand(Responders[cmd],rcmd,LineStream);
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
			printf("%c[2K\r", 27); std::cout << _prompt << std::flush;
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


