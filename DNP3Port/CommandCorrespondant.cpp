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
 * CommandCorrespondant.cpp
 *
 *  Created on: 07/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "CommandCorrespondant.h"
#include "CommandCallbackPromise.h"

std::unordered_map<CommandCallbackPromise*, std::shared_ptr<CommandCallbackPromise> > CommandCorrespondant::Callbacks;

CommandCallbackPromise* CommandCorrespondant::GetCallback(std::promise<opendnp3::CommandStatus> aPromise, std::function<void()> aCompletionHook)
{
	auto pCallback = std::shared_ptr<CommandCallbackPromise>(new CommandCallbackPromise(std::move(aPromise), std::move(aCompletionHook)));
	auto key = pCallback.get();
	Callbacks[key] = pCallback;
	return key;
}
void CommandCorrespondant::ReleaseCallback(CommandCallbackPromise* pFinshedCallback)
{
	Callbacks.erase(pFinshedCallback);
}
