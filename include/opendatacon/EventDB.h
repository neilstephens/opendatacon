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
 * EventDB.h
 *
 *  Created on: 24/04/2021
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef EVENTDB_H
#define EVENTDB_H

#include "IOTypes.h"
#include <unordered_map>
#include <memory>
#include <limits>

namespace odc
{

struct EventPointHash
{
	size_t operator()(const std::pair<EventType,size_t>& point) const
	{
		return (point.second << (sizeof(EventType)<<3)) + static_cast<size_t>(point.first);
	}
};

class EventDB
{
public:
	EventDB() = delete;
	EventDB(const std::vector<std::shared_ptr<const EventInfo>>& init_events);
	bool Set(const std::shared_ptr<const EventInfo> event);
	std::shared_ptr<const EventInfo> Swap(const std::shared_ptr<const EventInfo> event);
	std::shared_ptr<const EventInfo> Get(const std::shared_ptr<const EventInfo>& event) const;
	std::shared_ptr<const EventInfo> Get(const EventType event_type, const size_t index) const;

private:
	std::unordered_map<std::pair<EventType,size_t>,std::shared_ptr<const EventInfo>,EventPointHash> PointEvents;
};

} //namespace odc

#endif // EVENTDB_H
