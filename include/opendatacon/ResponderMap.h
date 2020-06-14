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
//  ResponderMap.h
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#ifndef opendatacon_ResponderMap_h
#define opendatacon_ResponderMap_h
#include "IUIResponder.h"
#include <memory>
#include <regex>
#include <opendatacon/util.h>

template <class T>
class ResponderMap: public std::unordered_map<std::string, T >, public IUIResponder
{
public:
	ResponderMap()
	{
		this->AddCommand("List", [this](const ParamCollection &params)
			{
				Json::Value result;

				result["Commands"] = GetCommandList();

				Json::Value vec;
				if(params.count("Target") == 0)
				{
				      for(auto& responder: *this)
				      {
				            vec.append(Json::Value(responder.first));
					}
				}
				else
				{
				      auto list = GetTargetNames(params);
				      for(auto& target: list)
				      {
				            vec.append(Json::Value(target));
					}
				}
				result["Items"]  = vec;

				return result;
			}, "Returns a list of commands and items for this collection. Optional argument: regex for which items to match", false);
	}
	~ResponderMap() override {}

	std::vector<T> GetTargets(const ParamCollection& params)
	{
		std::vector<T> targets;

		if (params.count("Target"))
		{

			std::string mregex = params.at("Target");
			std::regex reg;

			try
			{
				reg = std::regex(mregex);
				for(auto& pName_n_pVal : *this)
				{
					if(std::regex_match(pName_n_pVal.first, reg))
					{
						targets.push_back(pName_n_pVal.second);
					}
				}
			}
			catch(std::exception& e)
			{
				std::string msg("Regex exception: '" + std::string(e.what()) + "'");
				if(auto log = odc::spdlog_get("opendatacon"))
					log->error(msg);
				else
					std::cout << msg << std::endl;
			}
		}
		return targets;
	}

	std::vector<std::string> GetTargetNames(const ParamCollection& params)
	{
		std::vector<std::string> targets;

		if (params.count("Target"))
		{
			std::string mregex = params.at("Target");
			std::regex reg;

			try
			{
				reg = std::regex(mregex);
				for(auto& pName_n_pVal : *this)
				{
					if(std::regex_match(pName_n_pVal.first, reg))
					{
						targets.push_back(pName_n_pVal.first);
					}
				}
			}
			catch(std::exception& e)
			{
				std::string msg("Regex exception: '" + std::string(e.what()) + "'");
				if(auto log = odc::spdlog_get("opendatacon"))
					log->error(msg);
				else
					std::cout << msg << std::endl;
			}
		}
		return targets;
	}

	T GetTarget(const ParamCollection& params)
	{
		if (params.count("Target") && this->count(params.at("Target")))
		{
			return this->at(params.at("Target"));
		}
		return T();
	}

};

#endif
