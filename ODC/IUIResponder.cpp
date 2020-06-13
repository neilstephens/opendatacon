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
//  IUIResponder.cpp
//  opendatacon
//
//  Created by Alan Murray on 13/09/2014.
//
//

#include <opendatacon/IUIResponder.h>

const Json::Value IUIResponder::GenerateResult(const std::string& message)
{
	Json::Value result;
	result["RESULT"] = message;
	return result;
}

Json::Value IUIResponder::GetCommandList() const
{
	Json::Value result;
	for (const auto& command : commands)
	{
		if (!command.second.hidden) result.append(command.first);
	}
	return result;
}

std::string IUIResponder::GetCommandDescription(const std::string& acmd) const
{
	std::string result;
	if(commands.count(acmd) != 0 && !commands.at(acmd).hidden)
		result = commands.at(acmd).description;
	else
		result = "No such command: '"+acmd+"'";
	return result;
}

Json::Value IUIResponder::ExecuteCommand(const std::string& arCommandName, const ParamCollection& params) const
{
	if(commands.count(arCommandName) != 0)
	{
		auto command = commands.at(arCommandName);
		return command.function(params);
	}
	return IUIResponder::GenerateResult("Bad command");
}

void IUIResponder::AddCommand(const std::string& arCommandName, const UIFunction& arCommand, const std::string& desc, const bool hide)
{

	commands.insert(std::pair<std::string, UICommand>(arCommandName, UICommand(arCommand, desc, hide)));
}
