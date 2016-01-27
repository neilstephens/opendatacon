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
 *	ChannelStateSubscriber.h
 *
 *  Created on: 2016-01-27
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef CHANNELSTATESUBSCRIBER_H
#define CHANNELSTATESUBSCRIBER_H

#include <map>
#include <functional>
#include "DNP3Port.h"
#include <asiodnp3/IChannel.h>

class ChannelStateSubscriber
{
public:
	static void Subscribe(DNP3Port* pPort, asiodnp3::IChannel* pChan);
	static void Unsubscribe(DNP3Port* pPort, asiodnp3::IChannel* pChan = nullptr);
	static void StateListener(asiodnp3::IChannel* pChan, opendnp3::ChannelState state);

private:
	ChannelStateSubscriber(){}
	static std::multimap<asiodnp3::IChannel*, DNP3Port*> SubscriberMap;
	static std::mutex MapMutex;
};

#endif // CHANNELSTATESUBSCRIBER_H
