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
 * CommandCorrespondant.h
 *
 *  Created on: 07/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef COMMANDCORRESPONDANT_H_
#define COMMANDCORRESPONDANT_H_

#include <future>
#include <unordered_map>
#include <opendnp3/gen/CommandStatus.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

class CommandCallbackPromise;

class CommandCorrespondant
{
public:
	static CommandCallbackPromise* GetCallback(std::promise<CommandStatus> aPromise, std::function<void()> aCompletionHook = nullptr);
	static void ReleaseCallback(CommandCallbackPromise* pFinshedCallback);

private:
	CommandCorrespondant(){}
	static std::unordered_map<CommandCallbackPromise*, std::shared_ptr<CommandCallbackPromise> > Callbacks;
};

#endif /* COMMANDCORRESPONDANT_H_ */
