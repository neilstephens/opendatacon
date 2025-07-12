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

#include "ChannelHandler.h"
#include <opendnp3/channel/IChannel.h>
#include <opendnp3/channel/IChannelListener.h>
#include <map>
#include <shared_mutex>
#include <memory>

class ChannelStateSubscriber
{
	friend class ChannelListener;
private:
	static std::multimap<std::string, std::weak_ptr<ChannelHandler>> SubscriberMap;
	static std::shared_mutex MapMutex;

	ChannelStateSubscriber() = delete;
	static void Subscribe(std::weak_ptr<ChannelHandler> wChanH);
	static void StateListener(const std::string& ChanID, opendnp3::ChannelState state);

public:
	static void Unsubscribe(const std::string& ChanID);
};

class ChannelListener: public opendnp3::IChannelListener
{
public:
	ChannelListener(const std::string& aChanID, std::weak_ptr<ChannelHandler> wChanH):
		ChanID(aChanID)
	{
		ChannelStateSubscriber::Subscribe(wChanH);
	}
	//Receive callbacks for state transitions on the channels from opendnp3
	void OnStateChange(opendnp3::ChannelState state) override
	{
		ChannelStateSubscriber::StateListener(ChanID,state);
	}
private:
	const std::string ChanID;
};

#endif // CHANNELSTATESUBSCRIBER_H
