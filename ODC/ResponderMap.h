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

template <class T>
class ResponderMap : public std::unordered_map<std::string, std::shared_ptr<T> >, public IUIResponder
{
public:
    ResponderMap()
    {
        this->AddCommand("List", [this](const ParamCollection & params) {
            Json::Value result;
            
            result["Commands"] = GetCommandList();
            
            Json::Value vec;
            for(auto responder : *this)
            {
                vec.append(Json::Value(responder.first));
            }
            
            result["Items"]  = vec;
            
            return result;
        }, "", true);
    }

    T* GetTarget(const ParamCollection & params)
    {
        if (params.count("Target") && this->count(params.at("Target")))
        {
            return this->at(params.at("Target")).get();
        }
        return nullptr;
    }

};

#endif
