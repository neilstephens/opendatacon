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
#include <opendatacon/asio.h>
#include <memory>

class IUI
{
public:
	virtual ~IUI(){}
	virtual void AddCommand(const std::string& name, std::function<void (std::stringstream&)> callback, const std::string& desc = "No description available\n") = 0;
	virtual void AddResponder(const std::string& name, const IUIResponder& pResponder) = 0;
	virtual void Build() = 0;
	virtual void Enable() = 0;
	virtual void Disable() = 0;
protected:
	const std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
};


#endif
