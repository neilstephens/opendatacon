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
 *	ChannelStateSubscriber.cpp
 *
 *  Created on: 2016-01-27
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "ChannelStateSubscriber.h"
#include <utility>

std::multimap<std::string, std::weak_ptr<ChannelHandler>> ChannelStateSubscriber::SubscriberMap;
std::shared_mutex ChannelStateSubscriber::MapMutex;

void ChannelStateSubscriber::Subscribe(std::weak_ptr<ChannelHandler> wChanH)
{
	if(auto pChanH = wChanH.lock())
	{
		std::unique_lock<std::shared_mutex> lock(MapMutex);
		SubscriberMap.emplace(pChanH->GetChannelID(),wChanH);
	}
}

void ChannelStateSubscriber::StateListener(const std::string& ChanID, opendnp3::ChannelState state)
{
	std::shared_lock<std::shared_mutex> lock(MapMutex);
	auto bounds = SubscriberMap.equal_range(ChanID);
	for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; ++aMatch_it)
	{
		if(auto pChanH = aMatch_it->second.lock())
			pChanH->StateListener(state);
	}
}

void ChannelStateSubscriber::Unsubscribe(const std::string& ChanID)
{
	std::unique_lock<std::shared_mutex> lock(MapMutex);
	auto bounds = SubscriberMap.equal_range(ChanID);
	for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; /*advance in loop*/)
	{
		if(aMatch_it->second.expired())
			aMatch_it = SubscriberMap.erase(aMatch_it);
		else
			++aMatch_it;
	}
}
