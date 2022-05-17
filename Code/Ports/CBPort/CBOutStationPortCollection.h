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
//  CBOutstationPortCollection.h
//  opendatacon
//
//  Created by Scott Ellis 2020-05-11
//
//

#ifndef __opendatacon__CBOutStationPortCollection__
#define __opendatacon__CBOutStationPortCollection__

#include <opendatacon/DataPort.h>
#include <opendatacon/ResponderMap.h>
#include "CBOutstationPort.h"

using namespace odc;

class CBOutstationPortCollection: public ResponderMap< std::weak_ptr<CBOutstationPort> >
{
public:
	CBOutstationPortCollection()
	{
		this->AddCommand("FailControlResponse", [this](const ParamCollection &params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No CBOutstationPort matched");

				// Active - True or False
				if(params.count("0") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter - Pass True to activate or False to deactivate");
				}
				auto active = params.at("0");
				return target->UIFailControl(active) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			},"Specify that any control response gets migrated one bit left and returns if the operation was succesful. Syntax: 'FailControlResponse <CBOutstationPort|Regex> <True/False>");

		this->AddCommand("RTUStartupFlag", [this](const ParamCollection& params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if (!target)
					return IUIResponder::GenerateResult("No CBOutstationPort matched");

				// Active - True or False
				if (params.count("0") == 0)
				{
					return IUIResponder::GenerateResult("Bad parameter - Pass True to set or False to clear");
				}
				auto active = params.at("0");
				return target->UISetRTUReStartFlag(active) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			}, "Set or Clear the RTU power on flag. Syntax: 'RTUStartupFlag <CBOutstationPort|Regex> <True/False>");

		this->AddCommand("IsolateControls", [this](const ParamCollection& params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if (!target)
					return IUIResponder::GenerateResult("No CBOutstationPort matched");

				// Active - True or False
				if (params.count("0") == 0)
				{
					return IUIResponder::GenerateResult("Bad parameter - Pass True to set or False to clear");
				}
				auto active = params.at("0");
				return target->UISetRTUControlIsolateFlag(active) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			}, "Set or Clear the Controls Isolate flag. Syntax: 'IsolateControls <CBOutstationPort|Regex> <True/False>");

		this->AddCommand("RandomBitFlips", [this](const ParamCollection &params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if (!target)
					return IUIResponder::GenerateResult("No CBOutstationPort matched");

				//param 0: Probability 0 to 1
				if (params.count("0") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter - Pass in the Probability of a bit flip range 0 to 1");
				}
				auto probability = params.at("0");
				return target->UIRandomReponseBitFlips(probability) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			},"Sets the probability of a bit flip in the response packet and returns if the operation was successful. Syntax: 'RandomBitFlips <CBOutstationPort|Regex> <Probability (float)>");

		this->AddCommand("RandomResponseDrops", [this](const ParamCollection& params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if (!target)
					return IUIResponder::GenerateResult("No CBOutstationPort matched");

				//param 0: Probability 0 to 1
				if (params.count("0") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter - Pass in the Probability of a dropped packet range 0 to 1");
				}
				auto probability = params.at("0");
				return target->UIRandomReponseDrops(probability) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			}, "Sets the probability of a dropped response packet and returns if the operation was successful. Syntax: 'RandomResponseDrops <CBOutstationPort|Regex> <Probability (float)>");
	}
	virtual ~CBOutstationPortCollection(){}
};


#endif /* defined(__opendatacon__CBOutStationPortCollection__) */
