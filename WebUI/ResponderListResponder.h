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
//  ResponderListResponder.h
//  opendatacon
//
//  Created by Alan Murray on 30/08/2014.
//  
//

#ifndef __opendatacon__ResponderListResponder__
#define __opendatacon__ResponderListResponder__

#include "IJsonResponder.h"

class ResponderListResponder : public IJsonResponder
{
public:
    ResponderListResponder(const std::unordered_map<std::string, std::weak_ptr<const IJsonResponder>>& pJsonResponders) :
        JsonResponders(pJsonResponders)
    {
        
    }
    
    virtual Json::Value GetResponse(const ParamCollection& params) const final
    {
        Json::Value event;
        Json::Value vec(Json::arrayValue);
        
        for(auto responder : JsonResponders)
        {
            vec.append(Json::Value(responder.first));
        }
        
        event["Responders"] = vec;
        
        return event;
    };
private:
    const std::unordered_map<std::string, std::weak_ptr<const IJsonResponder>>& JsonResponders;
};

#endif /* defined(__opendatacon__ResponderListResponder__) */
