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
//  IUI.h
//  opendatacon
//
//  Created by Alan Murray on 29/08/2014.
//
//

#ifndef opendatacon_IUI_h
#define opendatacon_IUI_h

#include "IUIResponder.h"
#include <opendatacon/LogHelpers.h>
#include <opendatacon/asio.h>
#include <opendatacon/util.h>
#include <memory>

using CmdFunc_t = std::function<Json::Value (std::stringstream& args)>;

class IUI: public odc::LogHelpers
{
public:
	virtual ~IUI(){}
	virtual void AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc = "No description available\n") = 0;
	virtual void Build() = 0;
	virtual void Enable() = 0;
	virtual void Disable() = 0;

	//non-re-entrant functions only to be used when the  IUI is Disabled
	//UI should not access Responers when disabled
	inline void SetResponders(const std::unordered_map<std::string, const IUIResponder*>& aResponders)
	{
		Responders = aResponders;
	}

	virtual std::pair<std::string,const IUIResponder*> GetUIResponder()
	{
		IUIResponder* pR(nullptr);
		std::string s = "";
		std::pair<std::string,const IUIResponder*> pair(s,pR);
		return pair;
	}

protected:

	inline Json::Value ExecuteCommand(const IUIResponder* pResponder, const std::string& command, std::stringstream& args, ParamCollection* params = nullptr)
	{
		ParamCollection temp_params;
		if(!params)
			params = &temp_params;

		//Define first arg as Target regex - if it's a collection
		std::string T_regex_str;
		for (auto command : pResponder->GetCommandList())
		{
			if(command == "List")
			{
				odc::extract_delimited_string("\"'`",args,T_regex_str);
				break;
			}
		}

		//turn any regex it into a list of targets
		Json::Value target_list;
		if(!T_regex_str.empty())
		{
			(*params)["Target"] = T_regex_str;
			if(command != "List") //Use List to handle the regex for the other commands
				target_list = pResponder->ExecuteCommand("List", *params)["Items"];
		}

		int arg_num = 0;
		std::string Val;
		while(odc::extract_delimited_string("\"'`",args,Val))
			(*params)[std::to_string(arg_num++)] = Val;

		Json::Value results;
		if(target_list.size() > 0) //there was a list resolved
		{
			for(auto& target : target_list)
			{
				(*params)["Target"] = target.asString();
				results[target.asString()] = pResponder->ExecuteCommand(command, *params);
			}
		}
		else //There was no list - execute anyway
			results = pResponder->ExecuteCommand(command, *params);

		return results;
	}

	const std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unordered_map<std::string, const IUIResponder*> Responders;
};


#endif
