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
 * <PORT_TYPE_TEMPLATE>Port.h
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#ifndef <PORT_TYPE_TEMPLATE>Port_H_
#define <PORT_TYPE_TEMPLATE>Port_H_
#include "<PORT_TYPE_TEMPLATE>PortConf.h"
#include <opendatacon/DataPort.h>

using namespace odc;

class <PORT_TYPE_TEMPLATE>Port: public DataPort
{
public:
	<PORT_TYPE_TEMPLATE>Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);

	void Enable() override;
	void Disable() override;
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void ProcessElements(const Json::Value& JSONRoot) override;

private:
};

#endif /* <PORT_TYPE_TEMPLATE>Port_H_ */
