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
//  WebUI.h
//  opendatacon
//
//  Created by Alan Murray on 06/09/2014.
//
//

#ifndef __opendatacon__WebUI__
#define __opendatacon__WebUI__
#include "tinycon.h"
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
	void AddCommand(const std::string& name, std::function<void (std::stringstream&)> callback, const std::string& desc = "No description available\n") override final;
	void AddResponder(const std::string& name, const IUIResponder& pResponder) override;
	void Build() override;
	void Enable() override;
	void Disable() override final;

private:
	/* */
	std::string context;
	std::unique_ptr<asio::thread> uithread;

	/* tinyConsole functions */
	std::map<std::string,std::function<void (std::stringstream&)> > mCmds;
	std::map<std::string,std::string> mDescriptions;
	std::string help_intro;

	/* UI response handlers */
	std::unordered_map<std::string, const IUIResponder*> Responders;

	/* Internal functions */
	void ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args);

	void ToLower(std::string& str);
};

#endif /* defined(__opendatacon__WebUI__) */
