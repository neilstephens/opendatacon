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
 * nopCommandCallback.h
 *
 *  Created on: 18/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef COMMANDCALLBACKPROMISE_H_
#define COMMANDCALLBACKPROMISE_H_

#include <iostream>
#include <future>
#include <opendnp3/master/ICommandCallback.h>
#include <opendnp3/master/CommandResponse.h>
#include "CommandCorrespondant.h"

class CommandCallbackPromise: public opendnp3::ICommandCallback
{
public:
	CommandCallbackPromise(std::promise<opendnp3::CommandStatus> aPromise, std::function<void()> aCompletionHook = nullptr):
		mPromise(std::move(aPromise)),
		mCompletionHook(std::move(aCompletionHook))
	{};
	virtual ~CommandCallbackPromise(){};

	void OnComplete(const opendnp3::CommandResponse& response)
	{
		switch(response.GetResult())
		{
			case opendnp3::CommandResult::RESPONSE_OK:
				mPromise.set_value(response.GetStatus());
				break;
			case opendnp3::CommandResult::TIMEOUT:
				mPromise.set_value(opendnp3::CommandStatus::TIMEOUT);
				break;
			case opendnp3::CommandResult::BAD_RESPONSE:
			case opendnp3::CommandResult::NO_COMMS:
			case opendnp3::CommandResult::QUEUE_FULL:
			default:
				mPromise.set_value(opendnp3::CommandStatus::UNDEFINED);
				break;
		}
		if(mCompletionHook != nullptr)
			mCompletionHook();
		CommandCorrespondant::ReleaseCallback(this);
	};

private:
	std::promise<opendnp3::CommandStatus> mPromise;
	std::function<void()> mCompletionHook;

};

#endif /* COMMANDCALLBACKPROMISE_H_ */
