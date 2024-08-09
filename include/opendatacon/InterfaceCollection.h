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
//  InterfaceCollection.h
//  opendatacon
//
//  Created by Alan Murray on 29/11/2014.
//
//

#ifndef __opendatacon__InterfaceCollection__
#define __opendatacon__InterfaceCollection__

#include "ResponderMap.h"
#include "IUI.h"


class InterfaceCollection: public ResponderMap< std::shared_ptr<IUI> >
{
public:
	InterfaceCollection()
	{
		this->AddCommand("Enable", [this](const ParamCollection &params) -> const Json::Value
			{
				if (auto target = GetTarget(params))
				{
					target->Enable();
					return IUIResponder::GenerateResult("Success");
				}
				return IUIResponder::GenerateResult("Bad parameter");
			});
		this->AddCommand("Disable", [this](const ParamCollection &params) -> const Json::Value
			{
				if (auto target = GetTarget(params))
				{
					target->Disable();
					return IUIResponder::GenerateResult("Success");
				}
				return IUIResponder::GenerateResult("Bad parameter");
			});
	}
	virtual ~InterfaceCollection(){}
};



#endif /* defined(__opendatacon__InterfaceCollection__) */
