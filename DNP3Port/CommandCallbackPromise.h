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
#include <opendnp3/master/ITaskCallback.h>

class CommandCallbackPromise
{
public:
	CommandCallbackPromise(std::promise<CommandStatus> aPromise, std::function<void()> aCompletionHook = nullptr):
		mPromise(std::move(aPromise)),
		mCompletionHook(std::move(aCompletionHook))
	{}
	virtual ~CommandCallbackPromise(){}

	void OnComplete(const opendnp3::ICommandTaskResult& response)
	{
		switch(response.summary)
		{
			case opendnp3::TaskCompletion::SUCCESS:
				mPromise.set_value(CommandStatus::SUCCESS);
				break;
			case opendnp3::TaskCompletion::FAILURE_RESPONSE_TIMEOUT:
				mPromise.set_value(CommandStatus::TIMEOUT);
				break;
			case opendnp3::TaskCompletion::FAILURE_BAD_RESPONSE:
			case opendnp3::TaskCompletion::FAILURE_NO_COMMS:
			default:
				mPromise.set_value(CommandStatus::UNDEFINED);
				break;
		}
		if(mCompletionHook != nullptr)
			mCompletionHook();
	}

private:
	std::promise<CommandStatus> mPromise;
	std::function<void()> mCompletionHook;

};

#endif /* COMMANDCALLBACKPROMISE_H_ */
