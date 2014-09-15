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
//  DataConectorCollection.h
//  opendatacon
//
//  Created by Alan Murray on 13/09/2014.
//  
//

#ifndef __opendatacon__DataConectorCollection__
#define __opendatacon__DataConectorCollection__

#include <iostream>

#include "DataConnector.h"
#include <opendatacon/ResponderMap.h>

class DataConnectorCollection : public ResponderMap<DataConnector>
{
public:
    DataConnectorCollection()
    {
        this->AddCommand("GetConfiguration", [this](const ParamCollection & params) {
            if (auto target = GetTarget(params)) return target->GetConfiguration();
            return IUIResponder::ERROR_BADPARAMETER;
        });
        this->AddCommand("GetCurrentState", [this](const ParamCollection & params) {
            if (auto target = GetTarget(params)) return target->GetCurrentState();
            return IUIResponder::ERROR_BADPARAMETER;
        });
        this->AddCommand("GetStatistics", [this](const ParamCollection & params) {
            if (auto target = GetTarget(params)) return target->GetStatistics();
            return IUIResponder::ERROR_BADPARAMETER;
        });
    }
};

#endif /* defined(__opendatacon__DataConectorCollection__) */
