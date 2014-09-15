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

class IUIResponder
{
public:
    
    static const Json::Value GenerateError(const std::string& message);
    
    static const Json::Value ERROR_BADPARAMETER;
    
    virtual Json::Value GetCommandList()
    {
        Json::Value result;
        for (auto command : commands)
        {
            result.append(command.first);
        }
        return result;
    }
    
    Json::Value ExecuteCommand(const std::string& arCommandName, const ParamCollection& params) const
    {
        if(commands.count(arCommandName) != 0)
        {
            auto command = commands.at(arCommandName);
            return command(params);
        }
        return Json::Value();
    }
    
    void AddCommand(const std::string& arCommandName, std::function<Json::Value(const ParamCollection& params)> arCommand)
    {
        commands[arCommandName] = arCommand;
    }
    
private:
    std::unordered_map<std::string, std::function<Json::Value(const ParamCollection& params)>> commands;
};

#endif /* defined(__opendatacon__IUIResponder__) */
