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
 * EventDB.cpp
 *
 *  Created on: 24/04/2021
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/EventDB.h>
#include <atomic>

namespace odc
{

EventDB::EventDB(const std::vector<std::shared_ptr<const EventInfo>>& init_events)
{
	for(const auto& event : init_events)
		PointEvents[{event->GetEventType(),event->GetIndex()}] = event;
}

bool EventDB::Set(const std::shared_ptr<const EventInfo> event)
{
	const auto old_it = PointEvents.find({event->GetEventType(),event->GetIndex()});

	if(old_it == PointEvents.end())
		return false;

	const auto old_evt_addr = &(old_it->second);
	std::atomic_store(old_evt_addr,event);
	return true;
}

std::shared_ptr<const EventInfo> EventDB::Swap(const std::shared_ptr<const EventInfo> event)
{
	const auto old_it = PointEvents.find({event->GetEventType(),event->GetIndex()});

	if(old_it == PointEvents.end())
		return nullptr;

	const auto old_evt_addr = &(old_it->second);
	return std::atomic_exchange(old_evt_addr,event);
}

std::shared_ptr<const EventInfo> EventDB::Get(const std::shared_ptr<const EventInfo>& event) const
{
	return Get(event->GetEventType(),event->GetIndex());
}

std::shared_ptr<const EventInfo> EventDB::Get(const EventType event_type, const size_t index) const
{
	const auto old_it = PointEvents.find({event_type,index});

	if(old_it == PointEvents.end())
		return nullptr;

	const auto old_evt_addr = &(old_it->second);
	return std::atomic_load(old_evt_addr);
}

} //namespace odc
