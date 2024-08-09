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
 * NullPort.h
 *
 *  Created on: 04/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef NULLPORT_H_
#define NULLPORT_H_

#include <opendatacon/DataPort.h>

using namespace odc;

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;

/* The equivalent of /dev/null as a DataPort */
class NullPort: public DataPort
{
private:
	std::unique_ptr<Timer_t> pTimer;

public:
	NullPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		DataPort(aName, aConfFilename, aConfOverrides),
		pTimer(nullptr)
	{}
	void Enable() override
	{
		pTimer = pIOS->make_steady_timer(std::chrono::milliseconds(100));
		pTimer->async_wait(
			[this](asio::error_code err_code)
			{
				if (err_code != asio::error::operation_aborted)
				{
					PublishEvent(ConnectState::PORT_UP);
					PublishEvent(ConnectState::CONNECTED);
				}
			});
		enabled = true;
		return;
	}
	void Disable() override
	{
		enabled = false;
		if(pTimer)
			pTimer->cancel();
		pTimer.reset();
		PublishEvent(ConnectState::PORT_DOWN);
		PublishEvent(ConnectState::DISCONNECTED);
	}
	void Build() override {}
	void ProcessElements(const Json::Value& JSONRoot) override {}

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(enabled ? CommandStatus::SUCCESS : CommandStatus::BLOCKED); }
};

#endif /* NULLPORT_H_ */
