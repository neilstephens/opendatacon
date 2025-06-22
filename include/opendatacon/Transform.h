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
using EvtHandler_ptr = std::shared_ptr<std::function<void (std::shared_ptr<EventInfo> event)>>;
class Transform
{
public:
	Transform(const std::string& Name, const Json::Value& params):
		Name(Name),
		params(params)
	{}
	virtual ~Transform(){}

	virtual void Enable() {}
	virtual void Disable() {}
	virtual void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) = 0;

	const std::string Name;

protected:
	Json::Value params;
};

}

#endif /* TRANSFORM_H_ */
