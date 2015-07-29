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
//  IUI.h
//  opendatacon
//
//  Created by Alan Murray on 29/08/2014.
//
//

#ifndef opendatacon_IUI_h
#define opendatacon_IUI_h

#include "IUIResponder.h"
#include <memory>

class IUI
{
public:
	virtual ~IUI(){};
	virtual void AddResponder(const std::string name, const IUIResponder& pResponder) = 0;
	virtual int start() = 0;
	virtual void stop() = 0;
};


#endif
