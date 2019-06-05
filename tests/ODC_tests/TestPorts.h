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
 * TestPorts.h
 *
 *  Created on: 05/06/2019
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef TESTPORTS_H_
#define TESTPORTS_H_

#include "../opendatacon/NullPort.h"

namespace odc
{

class PublicPublishPort: public NullPort
{
public:
	PublicPublishPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		NullPort(aName, aConfFilename, aConfOverrides)
	{}
	void PublicPublishEvent(std::shared_ptr<EventInfo> event, SharedStatusCallback_t pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([] (CommandStatus status){}))
	{
		PublishEvent(event,pStatusCallback);
	}
};

class PayloadCheckPort: public NullPort
{
public:
	PayloadCheckPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		NullPort(aName, aConfFilename, aConfOverrides)
	{}
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override
	{
		auto correct_payload = std::to_string(event->GetIndex())+"_"+
		                       event->GetSourcePort()+"_"+
		                       ToString(event->GetQuality())+"_"+
		                       std::to_string(event->GetTimestamp());
		if(event->GetPayloadString() == correct_payload)
		{
			(*pStatusCallback)(CommandStatus::SUCCESS);
			return;
		}

		(*pStatusCallback)(CommandStatus::FORMAT_ERROR);
	}
};

}

#endif /* TESTPORTS_H_ */
