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

EventDB::EventDB(const std::vector<std::shared_ptr<const EventInfo>>& init_events):
	PointEvents(init_events.begin(),init_events.end())
{}

bool EventDB::Set(std::shared_ptr<const EventInfo> event)
{
	auto old_it = PointEvents.find(event);

	if(old_it == PointEvents.end())
		return false;

	//cast away the constness of the iterator element
	//this is OK, because we know we're not changing its hash
	auto old_evt_addr = &const_cast<std::shared_ptr<const EventInfo>&>(*old_it);

	std::atomic_store(old_evt_addr,event);
	return true;
}

std::shared_ptr<const EventInfo> EventDB::Swap(std::shared_ptr<const EventInfo> event)
{
	auto old_it = PointEvents.find(event);

	if(old_it == PointEvents.end())
		return nullptr;

	//cast away the constness of the iterator element
	//this is OK, because we know we're not changing its hash
	auto old_evt_addr = &const_cast<std::shared_ptr<const EventInfo>&>(*old_it);

	return std::atomic_exchange(old_evt_addr,event);
}

std::shared_ptr<const EventInfo> EventDB::Get(const std::shared_ptr<const EventInfo>& event) const
{
	auto old_it = PointEvents.find(event);

	if(old_it == PointEvents.end())
		return nullptr;

	auto old_evt = std::atomic_load(&(*old_it));

	return old_evt;
}

std::shared_ptr<const EventInfo> EventDB::Get(const EventType event_type, const size_t index) const
{
	auto lookup_evt = std::make_shared<const EventInfo>(event_type, index, "", QualityFlags::NONE, 0);
	return Get(lookup_evt);
}

} //namespace odc
