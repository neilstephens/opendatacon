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
 * <PORT_TYPE_TEMPLATE>Port.cpp
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#include "<PORT_TYPE_TEMPLATE>Port.h"
#include <opendatacon/util.h>
#include <opendatacon/spdlog.h>

<PORT_TYPE_TEMPLATE>Port::<PORT_TYPE_TEMPLATE>Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<<PORT_TYPE_TEMPLATE>PortConf>(ConfFilename);
	ProcessFile();
}

void <PORT_TYPE_TEMPLATE>Port::Enable()
{
}
void <PORT_TYPE_TEMPLATE>Port::Disable()
{
}
void <PORT_TYPE_TEMPLATE>Port::Build()
{
}
void <PORT_TYPE_TEMPLATE>Port::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(auto log = odc::spdlog_get("<PORT_TYPE_TEMPLATE>Port"))
		log->trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);

	(*pStatusCallback)(CommandStatus::SUCCESS);
}

void <PORT_TYPE_TEMPLATE>Port::ProcessElements(const Json::Value& JSONRoot)
{
	if(JSONRoot.isMember("Parity"))
	{
	}
}

