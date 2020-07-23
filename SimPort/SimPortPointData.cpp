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

void SimPortPointData::SetPoint(odc::EventType type, std::size_t index, const std::string& name,
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
		evt->SetPayload<odc::EventType::Binary>(static_cast<bool>(val));
		p.event = evt;
		m_points[odc::EventType::Binary][index] = std::make_shared<Point>(p);
	}
}

void SimPortPointData::SetForcedState(odc::EventType type, std::size_t index, bool state)
{
	m_points[type][index]->forced_state = state;
}

bool SimPortPointData::GetForcedState(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->forced_state;
}

void SimPortPointData::SetUpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	m_points[type][index]->update_interval = value;
}

std::size_t SimPortPointData::GetUpdateInterval(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->update_interval;
}

void SimPortPointData::SetPayload(odc::EventType type, std::size_t index, double payload)
{
	if (type == odc::EventType::Analog)
		m_points[type][index]->event->SetPayload<odc::EventType::Analog>(std::move(payload));
	if (type == odc::EventType::Binary)
		m_points[type][index]->event->SetPayload<odc::EventType::Binary>(std::move(payload));
}

double SimPortPointData::GetPayload(odc::EventType type, std::size_t index)
{
	double payload = 0.0f;
	if (type == odc::EventType::Analog)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Analog>();
	if (type == odc::EventType::Binary)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Binary>();
	return payload;
}

double SimPortPointData::GetStartValue(odc::EventType type, std::size_t index)
{
	return m_points[type][index]->start_value;
}

double SimPortPointData::GetStdDev(std::size_t index)
{
	return m_points[odc::EventType::Analog][index]->std_dev;
}

std::vector<std::size_t> SimPortPointData::GetIndexes(odc::EventType type)
{
	std::vector<std::size_t> indexes;
	auto points = m_points[type];
	for (auto it = points.begin(); it != points.end(); ++it)
	{
		indexes.emplace_back(it->first);
	}
	return indexes;
}

std::unordered_map<std::size_t, double> SimPortPointData::GetValues(odc::EventType type)
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
