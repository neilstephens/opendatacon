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
#pragma once
#include "tinycon.h"
#include <vector>
#include <map>
#include <sstream>
#include <functional>

static std::function<void(void)> sig_quit_p;

class Console:
	public tinyConsole
{
public:
	Console(std::string s);
	virtual ~Console(void);

	int trigger (std::string s) override;
	int hotkeys(char c) override;

	void AddCmd(std::string cmd, std::function<void (std::stringstream&)> callback, std::string desc = "No description available\n");
	static void sig_quit(int signum);
	void AddHelp(std::string help);

private:
	std::map<std::string,std::function<void (std::stringstream&)> > mCmds;
	std::map<std::string,std::string> mDescriptions;
	std::string help_intro;
};

