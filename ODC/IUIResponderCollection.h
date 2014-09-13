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
//  ResponderCollection.h
//  opendatacon
//
//  Created by Alan Murray on 13/09/2014.
//  
//

#ifndef __opendatacon__IUIResponderCollection__
#define __opendatacon__IUIResponderCollection__

#include <iostream>
#include <unordered_map>
#include "IUIResponder.h"

class IUIResponderCollection : public IUIResponder
{
public:
    
    virtual Json::Value GetResponse(const ParamCollection& params) const = 0;
    virtual IUIResponder* GetUIResponder(const std::string& arName) const = 0;
    /*
    {
        Json::Value event;
        Json::Value vec;
        
        //for(auto responder : *this)
        //{
         //   vec.append(Json::Value(responder.first));
       // }
        
        event["Responders"] = vec;
        
        return event;
    };*/
};

#endif /* defined(__opendatacon__IUIResponderCollection__) */
