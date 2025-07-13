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
#include "Log.h"
#include <opendatacon/util.h>
#include <fstream>
#include <iomanip>
#include <exception>

using namespace odc;

std::string column_wrap(std::string input, const size_t target_width = 80, const size_t col_offset = 26)
{
	size_t width = 0;
	bool passed_space = false;
	for(size_t i=0; i < input.size(); i++)
	{
		passed_space = passed_space | (input[i] == ' ');
		if(++width > target_width)
		{
			while(input[i] != ' ' && passed_space)
				i--;
			if(input[i] == ' ')
			{
				input.insert(i,"\n"+std::string(col_offset-2,' '));
				i+=col_offset;
				width = 0;
				passed_space = false;
			}
		}
	}
	return input;
}

ConsoleUI::ConsoleUI():
	tinyConsole("odc> "),
	context("")
{
	AddHelp("If commands in context to a collection (Eg. DataPorts etc.) require parameters, "
		"the first argument is taken as a regex to match which items in the collection the "
		"command will run for. The remainer of the arguments will be passed through to each element."
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
					std::cout<<std::setw(25)<<std::left<<arg+":"<<column_wrap(mDescriptions[arg])<<std::endl<<std::endl;
				else if(Responders.count(arg))
				{
					std::cout<<std::setw(25)<<std::left<<arg+":"<<
					      "Access contextual subcommands:"<<std::endl<<std::endl;
					auto commands = Responders[arg]->GetCommandList();
					for (const auto& command : commands) //list sub commands
					{
						auto cmd = command.asString();
						auto desc = Responders[arg]->GetCommandDescription(cmd);
						std::cout<<std::setw(25)<<std::left<<"  "+cmd+":"<<column_wrap(desc)<<std::endl<<std::endl;
					}
				}
			}
			else
			{
				std::cout<<help_intro<<std::endl<<std::endl;
				std::cout<<"============ Root Commands ============"<<std::endl<<std::endl;
				for(const auto& desc: mDescriptions)
				{
					std::cout<<std::setw(25)<<std::left<<desc.first+":"<<column_wrap(desc.second)<<std::endl<<std::endl;
				}
				if (this->context.empty()) //if there is no context, print Responders
				{
					std::cout<<"============ Command Contexts ============"<<std::endl<<std::endl;
					for(const auto& name_n_responder : Responders)
					{
						std::cout<<std::setw(25)<<std::left<<name_n_responder.first+":"<<
						      "Access contextual subcommands."<<std::endl<<std::endl;
					}
				}
				else //we have context - list sub commands
				{
					std::cout<<"============ Context Specific Commands ============"<<std::endl<<std::endl;
					auto commands = Responders[this->context]->GetCommandList();
					for (const auto& command : commands)
					{
						auto cmd = command.asString();
						auto desc = Responders[this->context]->GetCommandDescription(cmd);
						std::cout<<std::setw(25)<<std::left<<cmd+":"<<column_wrap(desc)<<std::endl<<std::endl;
					}
					std::cout<<std::setw(25)<<std::left<<"exit:"<<
					      "Leave '"+context+"' context."<<std::endl<<std::endl;
				}
			}
			std::cout<<std::endl;
			return IUIResponder::GenerateResult("Success");
		},"Get help on commands. Optional argument of specific command.");
}

ConsoleUI::~ConsoleUI(void)
{
	Disable();
}

void ConsoleUI::AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc)
{
	mCmds[name] = callback;
	mDescriptions[name] = desc;
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


