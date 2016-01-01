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
#include <regex>

ConsoleUI::ConsoleUI(): tinyConsole("odc> ")
{
    mDescriptions["help"] = "Get help on commands. Optional argument of specific command.";
    mDescriptions["exit"] = "Shutdown all components and exit the application.";

    std::function<void (std::stringstream&)> bound_func;
    
    //Version
    bound_func = [] (std::stringstream& ss){std::cout<<"Release " << ODC_VERSION_STRING <<std::endl;};
    this->AddCmd("version",bound_func,"Print version information");

}

ConsoleUI::~ConsoleUI(void)
{}

void ConsoleUI::AddCmd(std::string cmd, std::function<void (std::stringstream&)> callback, std::string desc)
{
    mCmds[cmd] = callback;
    mDescriptions[cmd] = desc;
    
    int width = 0;
    for(size_t i=0; i < mDescriptions[cmd].size(); i++)
    {
        if(++width > 80)
        {
            while(mDescriptions[cmd][i] != ' ')
                i--;
            mDescriptions[cmd].insert(i,"\n                        ");
            i+=26;
            width = 0;
        }
    }
}
void ConsoleUI::AddHelp(std::string help)
{
    help_intro = help;
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

int ConsoleUI::trigger (std::string s)
{
    std::stringstream LineStream(s);
    std::string cmd,arg;
    LineStream>>cmd;
    
    if(cmd == "exit")
    {
        std::cout << "Exiting..." << std::endl;
        quit();
    }
    else if(cmd == "help")
    {
        std::cout<<std::endl;
        if(LineStream>>arg)
        {
            if(!mDescriptions.count(arg))
                std::cout<<"Command '"<<arg<<"' not found\n";
            else
                std::cout<<std::setw(25)<<std::left<<arg+":"<<mDescriptions[arg]<<std::endl<<std::endl;
        }
        else
        {
            std::cout<<help_intro<<std::endl<<std::endl;
            for(auto desc : mDescriptions)
            {
                std::cout<<std::setw(25)<<std::left<<desc.first+":"<<desc.second<<std::endl<<std::endl;
            }
        }
        std::cout<<std::endl;
    }
    else if(!mCmds.count(cmd))
    {
        if(cmd != "")
            std::cout <<"Unknown command:"<< cmd << std::endl;
    }
    else
        mCmds[cmd](LineStream);
    
    return 0;
}

int ConsoleUI::hotkeys(char c)
{
    if (c == 3 /*CTRL-C*/) //TODO: check this value is cross-platform and macro it.
    {
        trigger("exit");
        return 1;
    }
    if (c == TAB) //auto complete/list
    {
        //store what's been entered so far
        std::string partial_cmd;
        partial_cmd.assign(buffer.begin(), buffer.end());
        
        //find commands that start with the partial
        std::vector<std::string> matching_cmds;
        for(auto cmd : mDescriptions)
        {
            if(strncmp(cmd.first.c_str(),partial_cmd.c_str(),partial_cmd.size())==0)
                matching_cmds.push_back(cmd.first.c_str());
        }
        
        //any matches?
        if(matching_cmds.size())
        {
            //we want to see how many chars all the matches have in common
            auto common_length = partial_cmd.size()-1; //starting from what we already know matched
            
            if(matching_cmds.size()==1)
                common_length=matching_cmds.back().size();
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
            if(common_length > partial_cmd.size())
            {
                buffer.assign(matching_cmds.back().begin(),matching_cmds.back().begin()+common_length);
                std::string remainder;
                remainder.assign(matching_cmds.back().begin()+partial_cmd.size(),matching_cmds.back().begin()+common_length);
                std::cout<<remainder;
                line_pos = (int)common_length;
            }
            //otherwise we're at the branching point - list possible commands
            else if(matching_cmds.size() > 1)
            {
                std::cout<<std::endl;
                for(auto cmd : matching_cmds)
                    std::cout<<cmd<<std::endl;
                std::cout<<_prompt<<partial_cmd;
            }
            
            //if there's just one match, we just auto-completed it. Now print a trailing space.
            if(matching_cmds.size() == 1)
            {
                std::cout<<' ';
                buffer.push_back(' ');
                line_pos++;
            }
            
        }
        
        return 1;
    }
    return 0;
}


void ConsoleUI::AddResponder(const std::string name, const IUIResponder& pResponder)
{
	Responders[name] = &pResponder;

    auto commands = pResponder.GetCommandList();

    for (auto command : commands)
    {
        auto bound_func = std::bind(&ConsoleUI::ExecuteCommand,this,pResponder,command.asString(),std::placeholders::_1);
        this->AddCmd(name + "_" + command.asString(),bound_func,"List ports matching a regex (by name)");
    }
}

void ConsoleUI::ExecuteCommand(const IUIResponder& pResponder, const std::string& command, std::stringstream& args)
{
    std::string arg = "";
    std::string mregex;
    std::regex reg;
    
    if(!extract_delimited_string(args,mregex))
    {
        std::cout<<"Syntax error: Delimited regex expected, found \"..."<<mregex<<"\""<<std::endl;
        return;
    }
    try
    {
        reg = std::regex(mregex);
        ParamCollection params;
        auto result = pResponder.ExecuteCommand(command, params);
        std::cout<<result.toStyledString()<<std::endl;

    }
    catch(std::exception& e)
    {
        std::cout<<e.what()<<std::endl;
        return;
    }
}

void ConsoleUI::Enable()
{
    uithread = std::unique_ptr<asio::thread>(new asio::thread([this](){
        this->run();
    }));
}

void ConsoleUI::Disable()
{
    this->quit();
    //uithread->join();
}


