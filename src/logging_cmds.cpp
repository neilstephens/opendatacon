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
 * logging_cmds.cpp
 *
 *  Created on: 14/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "logging_cmds.h"

void cmd_ignore_message(std::stringstream& args, LogToStdioAdv& AdvLog)
{
	std::string mregex;
	if(!extract_delimited_string(args,mregex))
	{
		std::cout<<"Syntax error: Delimited regex expected, found \"..."<<mregex<<"\""<<std::endl;
		return;
	}
	if(mregex == "")
	{
		std::cout<<"Please supply a regex filter"<<std::endl;
		return;
	}
	std::cout<<"adding regex "<<mregex<<std::endl;
	AdvLog.AddIngoreAlways(mregex);
}
void cmd_unignore_message(std::stringstream& args, LogToStdioAdv& AdvLog)
{
	std::string arg = "";
	std::string mregex;
	if(!extract_delimited_string(args,mregex))
	{
		std::cout<<"Syntax error: Delimited regex expected, found \"..."<<mregex<<"\""<<std::endl;
		return;
	}
	std::cout<<"removing regex "<<mregex<<std::endl;
	AdvLog.RemoveIgnore(mregex);
}
void cmd_show_ignored(std::stringstream& args, LogToStdioAdv& AdvLog)
{
	AdvLog.ShowIgnored();
}


