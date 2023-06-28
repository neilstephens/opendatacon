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
//
//  ConsoleUI.h
//  opendatacon
//
//  Created by Alan Murray on 01/01/2016.
//
//

#ifndef __opendatacon__ConsoleUI__
#define __opendatacon__ConsoleUI__
#include "tinycon.h"
#include <LuaUICommander.h>
#include <opendatacon/IUI.h>
#include <opendatacon/asio.h>
#include <vector>
#include <map>
#include <sstream>
#include <functional>

class ConsoleUI: public IUI, tinyConsole
{
public:
	ConsoleUI();
	~ConsoleUI(void) override;

	void AddHelp(std::string help);

	/* tinyConsole functions */
	int trigger (const std::string& s) override;
	int hotkeys(char c) override;

	/* Implement IUI interface */
	void AddCommand(const std::string& name, std::function<Json::Value (std::stringstream&)> callback, const std::string& desc = "No description available\n") override final;
	void Build() override;
	void Enable() override;
	void Disable() override final;

private:
	/* */
	std::string context;
	std::unique_ptr<asio::thread> uithread;

	/* tinyConsole functions */
	std::map<std::string,std::function<Json::Value (std::stringstream&)> > mCmds;
	std::map<std::string,std::string> mDescriptions;
	std::string help_intro;

	LuaUICommander ScriptRunner;
	Json::Value ScriptCommandHandler(const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss);

	/* Internal functions */
	Json::Value ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args, bool quiet = false);

	/* Internal functions for auto completion and console prompt handling */
	void AddRootCommands(const std::string& cmd, std::vector<std::string>& matches);
	void AddCommands(const std::string& cmd, const std::string& sub_cmd, std::vector<std::string>& mathces);
	void PrintMatches(const std::string& cmd,
		const std::string& sub_cmd,
		const std::string& history_cmd,
		const std::vector<std::string>& matches);
};

#endif /* defined(__opendatacon__ConsoleUI__) */
