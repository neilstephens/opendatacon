/*	opendatacon
 *
 *	Copyright (c) 2020:
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
 * SimPortPointData.cpp
 *
 *  Created on: 16/07/2020
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#include "SimPortPointData.h"

SimPortPointData::SimPortPointData() {}

void SimPortPointData::CreateEvent(odc::EventType type, std::size_t index, const std::string& name,
	double s_dev, std::size_t u_interval, double val)
{
	if (type == odc::EventType::Analog)
	{
		Point p;
		p.std_dev = s_dev;
		p.update_interval = u_interval;
		p.start_value = val;

		odc::QualityFlags flag = odc::QualityFlags::ONLINE;
		if (!s_dev && !u_interval && !val)
			flag = odc::QualityFlags::COMM_LOST;
		auto evt = std::make_shared<odc::EventInfo>(odc::EventType::Analog, index, name, flag);
		evt->SetPayload<odc::EventType::Analog>(std::move(val));
		p.event = evt;
		m_points[odc::EventType::Analog][index] = std::make_shared<Point>(p);
	}

	if (type == odc::EventType::Binary)
	{
		Point p;
		p.std_dev = s_dev;
		p.update_interval = u_interval;

		auto evt = std::make_shared<odc::EventInfo>(odc::EventType::Binary, index, name, odc::QualityFlags::ONLINE);
		bool v = static_cast<bool>(val);
		evt->SetPayload<odc::EventType::Binary>(std::move(v));
		p.event = evt;
		m_points[odc::EventType::Binary][index] = std::make_shared<Point>(p);
	}
}

std::shared_ptr<odc::EventInfo> SimPortPointData::Event(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->event;
}

void SimPortPointData::Event(std::shared_ptr<odc::EventInfo> event)
{
	m_points[event->GetEventType()][event->GetIndex()]->event = event;
}

void SimPortPointData::ForcedState(odc::EventType type, std::size_t index, bool state)
{
	m_points[type][index]->forced_state = state;
}

bool SimPortPointData::ForcedState(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->forced_state;
}

void SimPortPointData::UpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	m_points[type][index]->update_interval = value;
}

std::size_t SimPortPointData::UpdateInterval(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->update_interval;
}

void SimPortPointData::Payload(odc::EventType type, std::size_t index, double payload)
{
	auto points = m_points[type];
	if (points.find(index) == points.end())
	{
		if(auto log = odc::spdlog_get("SimPort"))
		{
			log->error("{} Index == {} not found on the points container, why they are asking for set?", ToString(type), index);
			return;
		}
	}
	if (type == odc::EventType::Analog)
		m_points[type][index]->event->SetPayload<odc::EventType::Analog>(std::move(payload));
	if (type == odc::EventType::Binary)
		m_points[type][index]->event->SetPayload<odc::EventType::Binary>(std::move(static_cast<bool>(payload)));
}

double SimPortPointData::Payload(odc::EventType type, std::size_t index)
{
	double payload = 0.0f;
	if (type == odc::EventType::Analog)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Analog>();
	if (type == odc::EventType::Binary)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Binary>();
	return payload;
}

double SimPortPointData::StartValue(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->start_value;
}

double SimPortPointData::StdDev(std::size_t index)
{
	return m_points[odc::EventType::Analog][index]->std_dev;
}

std::vector<std::size_t> SimPortPointData::Indexes(odc::EventType type)
{
	std::vector<std::size_t> indexes;
	auto points = m_points[type];
	for (auto it = points.begin(); it != points.end(); ++it)
	{
		indexes.emplace_back(it->first);
	}
	return indexes;
}

std::unordered_map<std::size_t, double> SimPortPointData::Values(odc::EventType type)
{
	std::unordered_map<std::size_t, double> values;
	auto points = m_points[type];
	for (auto it = points.begin(); it != points.end(); ++it)
	{
		if (type == odc::EventType::Analog)
			values[it->first] = it->second->event->GetPayload<odc::EventType::Analog>();
		if (type == odc::EventType::Binary)
			values[it->first] = it->second->event->GetPayload<odc::EventType::Binary>();
	}
	return values;
}

bool SimPortPointData::IsIndex(odc::EventType type, std::size_t index)
{
	return m_points[type].find(index) == m_points[type].end();
}