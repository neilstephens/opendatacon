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
//  MD3OutstationPortCollection.h
//  opendatacon
//
//  Created by Scott Ellis on 12/05/2020.
//
//

#ifndef __opendatacon__MD3OutStationPortCollection__
#define __opendatacon__MD3OutStationPortCollection__

#include <opendatacon/DataPort.h>
#include <opendatacon/ResponderMap.h>
#include "MD3OutstationPort.h"

using namespace odc;

class MD3OutstationPortCollection: public ResponderMap< std::weak_ptr<MD3OutstationPort> >
{
public:
	MD3OutstationPortCollection()
	{
		this->AddCommand("FailControlResponse", [this](const ParamCollection &params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No MD3OutstationPort matched");

				// Active - True or False
				if(params.count("0") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter - Pass True to activate or False to deactivate");
				}
				auto active = params.at("0");
				return target->UIFailControl(active) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			},"Specify that any control response gets migrated one bit left and returns if the operation was succesful. Syntax: 'FailControlResponse <MD3OutstationPort|Regex> <True/False>");

		this->AddCommand("RandomBitFlips", [this](const ParamCollection &params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if (!target)
					return IUIResponder::GenerateResult("No MD3OutstationPort matched");

				//param 0: Probability 0 to 1
				if (params.count("0") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter - Pass in the Probability of a bit flip range 0 to 1");
				}
				auto probability = params.at("0");
				return target->UIRandomReponseBitFlips(probability) ? IUIResponder::GenerateResult("Success") : IUIResponder::GenerateResult("Bad Parameter");

			},"Sets the probability of a bit flip in the response packet and returns if the operation was succesful. Syntax: 'RandomBitFlips <MD3OutstationPort|Regex> <Probability (float)>");

	}

	void Add(std::shared_ptr<MD3OutstationPort> p, const std::string& Name)
	{
		std::lock_guard<std::mutex> lck(mtx);
		this->insert(std::pair<std::string,std::shared_ptr<MD3OutstationPort>>(Name,p));
	}
	virtual ~MD3OutstationPortCollection(){}
private:
	std::mutex mtx;
};


#endif /* defined(__opendatacon__MD3OutStationPortCollection__) */
