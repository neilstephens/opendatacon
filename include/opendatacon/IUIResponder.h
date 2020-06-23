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
//  IUIResponder.h
//  opendatacon
//
//  Created by Alan Murray on 13/09/2014.
//
//

#ifndef __opendatacon__IUIResponder__
#define __opendatacon__IUIResponder__

#include <iostream>
#include <functional>
#include <unordered_map>
#include <json/json.h>
#include <opendatacon/ParamCollection.h>

typedef std::function<Json::Value(const ParamCollection& params)> UIFunction;

class UICommand
{
public:
	UICommand(const UIFunction& func, const std::string& desc, const bool hide): function(func), description(desc), hidden(hide) {}
	UIFunction function;
	std::string description;
	bool hidden; // if true, command is not listed during a call to GetCommandList
};

class IUIResponder
{
public:
	virtual ~IUIResponder(){}
	static const Json::Value GenerateResult(const std::string& message);

	virtual Json::Value GetCommandList() const;
	virtual std::string GetCommandDescription(const std::string& acmd) const;
	virtual Json::Value ExecuteCommand(const std::string& arCommandName, const ParamCollection& params) const;
	void AddCommand(const std::string& arCommandName, const UIFunction& arCommand, const std::string& desc = "", const bool hide = false);

private:
	std::unordered_map<std::string, UICommand> commands;
};

#endif /* defined(__opendatacon__IUIResponder__) */
