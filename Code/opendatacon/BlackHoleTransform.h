/*	opendatacon
 *
 *	Copyright (c) 2020:
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
 * BlackHoleTransform.h
 *
 *  Created on: 4/8/2020
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef EVENTSINKTRANSFORM_H_
#define EVENTSINKTRANSFORM_H_

#include "Log.h"
#include <cstdint>
#include <opendatacon/Transform.h>

using namespace odc;

class BlackHoleTransform: public Transform
{
public:
	BlackHoleTransform(const std::string& Name, const Json::Value& params): Transform(Name,params)
	{}

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		// Will result in the callback being called with response undefined. Which is technically correct, but we would probably like to "fool" the
		// port we are sinking into thinking that everything is ok. Will require changes in DataConnector.cpp
		return (*pAllow)(nullptr);
	}
};

#endif /* EVENTSINKTRANSFORM_H_ */
