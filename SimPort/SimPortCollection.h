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
//  SimPortCollection.h
//  opendatacon
//
//  Created by Neil Stephens on 30/10/2019.
//
//

#ifndef __opendatacon__SimPortCollection__
#define __opendatacon__SimPortCollection__

#include <opendatacon/DataPort.h>
#include <opendatacon/ResponderMap.h>
#include "SimPort.h"

using namespace odc;

class SimPortCollection: public ResponderMap< std::weak_ptr<SimPort> >
{
public:
	SimPortCollection()
	{
		this->AddCommand("ForcePoint", [this](const ParamCollection &params) -> const Json::Value
			{
				//param 0: Point Type
				//param 1: Point Index
				//param 2: Value
				//param 3: Optional Quality
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No SimPort matched");
				//check mandatory params are there
				if(params.count("0") == 0 || params.count("1") == 0 || params.count("2") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter");
				}
				auto type = params.at("0");
				auto index = params.at("1");
				auto value = params.at("2");
				std::string quality = "";
				if(params.count("3") != 0)
					quality = params.at("3");
				if(target->Force(type,index,value,quality))
					return IUIResponder::GenerateResult("Success");
				else
					return IUIResponder::GenerateResult("Bad parameter");
			},"Overrides the value of a point and returns if the operation was succesful. Syntax: 'ForcePoint <SimPort|Regex> <PointType> <Index> <Value> [<Quality>]");
		this->AddCommand("ReleasePoint", [this](const ParamCollection &params) -> const Json::Value
			{
				//param 0: Point Type
				//param 1: Point Index
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No SimPort matched");
				//check mandatory params are there
				if(params.count("0") == 0 || params.count("1") == 0)
				{
				      return IUIResponder::GenerateResult("Bad parameter");
				}
				auto type = params.at("0");
				auto index = params.at("1");
				if(target->Release(type,index))
					return IUIResponder::GenerateResult("Success");
				else
					return IUIResponder::GenerateResult("Bad parameter");
			},"Removes override from a point and returns if the operation was succesful. Syntax: 'ReleasePoint <SimPort|Regex> <PointType> <Index>");

	}
	void Add(std::shared_ptr<SimPort> p, const std::string& Name)
	{
		std::lock_guard<std::mutex> lck(mtx);
		this->insert(std::pair<std::string,std::shared_ptr<SimPort>>(Name,p));
	}
	virtual ~SimPortCollection(){}
private:
	std::mutex mtx;
};


#endif /* defined(__opendatacon__SimPortCollection__) */
