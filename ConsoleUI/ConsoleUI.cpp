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

ConsoleUI::ConsoleUI(): tinyConsole("odc> "), context("")
{
    //mDescriptions["help"] = "Get help on commands. Optional argument of specific command.";
    AddCommand("help",[this](std::stringstream& LineStream){
        std::string arg;
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
        
    },"Get help on commands. Optional argument of specific command.");

    std::function<void (std::stringstream&)> bound_func;
}

ConsoleUI::~ConsoleUI(void)
{}

void ConsoleUI::AddCommand(const std::string name, std::function<void (std::stringstream&)> callback, const std::string desc)
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
        ExecuteCommand(Responders[this->context],cmd,LineStream);
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
        std::string partial_cmd;
        partial_cmd.assign(buffer.begin(), buffer.end());
        
        std::stringstream LineStream(partial_cmd);
        std::string cmd,arg;
        LineStream>>cmd;
        
        //find commands that start with the partial
        std::vector<std::string> matching_cmds;
        if (this->context.empty())
        {
            LineStream>>arg;

            if (Responders.count(cmd))
            {
                /* list commands avaialble to responder */
                auto commands = Responders[cmd]->GetCommandList();
                for (auto command : commands)
                {
                    if(strncmp(command.asString().c_str(),arg.c_str(),arg.size())==0)
                        matching_cmds.push_back(cmd + " " + command.asString());
                }
            }
            else
            {
                /* list all matching responders */
                for(auto name_n_responder: Responders)
                {
                    if(strncmp(name_n_responder.first.c_str(),cmd.c_str(),cmd.size())==0)
                        matching_cmds.push_back(name_n_responder.first.c_str());
                }
            }
        }
        else
        {
            /* list commands available to current responder */
            auto commands = Responders[this->context]->GetCommandList();
            for (auto command : commands)
            {
                if(strncmp(command.asString().c_str(),partial_cmd.c_str(),partial_cmd.size())==0)
                    matching_cmds.push_back(command.asString());
            }
        }
        
        for(auto name_n_description : mDescriptions)
        {
            if(strncmp(name_n_description.first.c_str(),partial_cmd.c_str(),partial_cmd.size())==0)
                matching_cmds.push_back(name_n_description.first.c_str());
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
	Responders[ name ] = &pResponder;
}

void ConsoleUI::ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args)
{
    ParamCollection params;

    std::string pName;
    std::string pVal;
    while(args)
    {
        extract_delimited_string("\"'`",args,pName);
        extract_delimited_string("\"'`",args,pVal);
        params[pName] = pVal;
    }
    auto result = pResponder->ExecuteCommand(command, params);
    std::cout<<result.toStyledString()<<std::endl;
}

void ConsoleUI::Enable()
{
    this->_quit = false;
    if (!uithread) {
        uithread = std::unique_ptr<asio::thread>(new asio::thread([this](){
            this->run();
        }));
    }
}

void ConsoleUI::Disable()
{
    this->quit();
    if (uithread) {
        //uithread->join();
        uithread.reset();
    }
}


