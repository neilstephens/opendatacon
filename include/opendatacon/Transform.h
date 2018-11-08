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
/*
 * Transform.h
 *
 *  Created on: 30/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef TRANSFORM_H_
#define TRANSFORM_H_

#include <opendatacon/IOHandler.h>
#include <opendatacon/IOTypes.h>
#include <json/json.h>
#include <string>

namespace odc
{

class Transform
{
public:
	Transform(const Json::Value& params): params(params){}
	virtual ~Transform(){}

	virtual bool Event(std::shared_ptr<EventInfo> event) = 0;

	Json::Value params;
};

}

#endif /* TRANSFORM_H_ */
