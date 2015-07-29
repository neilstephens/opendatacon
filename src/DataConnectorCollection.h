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

class DataConnectorCollection: public ResponderMap<DataConnector>
{
public:
	DataConnectorCollection()
	{
		this->AddCommand("Configuration", [this](const ParamCollection &params) {
		                       if (auto target = GetTarget(params)) return target->GetConfiguration();
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("CurrentState", [this](const ParamCollection &params) {
		                       if (auto target = GetTarget(params)) return target->GetCurrentState();
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("Statistics", [this](const ParamCollection &params) {
		                       if (auto target = GetTarget(params)) return target->GetStatistics();
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("Status", [this](const ParamCollection &params)->const Json::Value {
		                       if (auto target = GetTarget(params))
		                       {
		                             Json::Value result;
		                             result["Result"] = target->enabled;
		                             return result;
					     }
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("Enable", [this](const ParamCollection &params)->const Json::Value {
		                       if (auto target = GetTarget(params))
		                       {
		                             target->Enable();
		                             return IUIResponder::RESULT_SUCCESS;
					     }
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("Disable", [this](const ParamCollection &params)->const Json::Value {
		                       if (auto target = GetTarget(params))
		                       {
		                             target->Disable();
		                             return IUIResponder::RESULT_SUCCESS;
					     }
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
		this->AddCommand("Restart", [this](const ParamCollection &params)->const Json::Value {
		                       if (auto target = GetTarget(params))
		                       {
		                             target->Disable();
		                             target->Enable();
		                             return IUIResponder::RESULT_SUCCESS;
					     }
		                       return IUIResponder::RESULT_BADPARAMETER;
				     });
	}
	virtual ~DataConnectorCollection(){};
};

#endif /* defined(__opendatacon__DataConectorCollection__) */
