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

inline void SimPortPointData::SetPoint(const odc::EventType& type, std::size_t index, std::shared_ptr<Point> point)
{
	if (type == odc::EventType::Binary)
	{
		m_binary_points[index] = std::make_shared<BinaryPoint>(point->start_value, point->update_interval);
	}
	else
		m_analog_points[index] = std::make_shared<AnalogPoint>(point->start_value, point->update_interval, point->std_dev);
}

void SimPortPointData::SetForcedState(const odc::EventType& type, std::size_t index, bool state)
{
	if (type == odc::EventType::Binary)
		m_binary_points[index]->forced_state = state;
	else
		m_analog_points[index]->forced_state = state;
}

bool SimPortPointData::GetForcedState(const odc::EventType& type, std::size_t index)
{
	if (type == odc::EventType::Binary)
		return m_binary_points[index]->forced_state;
	return m_analog_points[index]->forced_state;
}

void SimPortPointData::SetUpdateInterval(const odc::EventType& type, std::size_t index, std::size_t value)
{
	if (type == odc::EventType::Binary)
		m_binary_points[index]->update_interval = value;
	m_analog_points[index]->update_interval = value;
}

std::size_t SimPortPointData::GetUpdateInterval(const odc::EventType& type, std::size_t index)
{
	if (type == odc::EventType::Binary)
		return m_binary_points[index]->update_interval;
	return m_analog_points[index]->update_interval;
}

inline double SimPortPointData::GetStartValue(const odc::EventType& type, std::size_t index)
{
	if (type == odc::EventType::Binary)
		return m_binary_points[index]->start_value;
	return m_analog_points[index]->start_value;
}

inline double SimPortPointData::GetStdDev(std::size_t index)
{
	return m_analog_points[index]->std_dev;
}

inline std::vector<std::size_t> SimPortPointData::GetIndexes(const odc::EventType& type)
{
	std::vector<std::size_t> indexes;
	if (type == odc::EventType::Binary)
		for (auto it = m_binary_points.begin(); it != m_binary_points.end(); ++it)
			indexes.emplace_back(it->first);
	else
		for (auto it = m_analog_points.begin(); it != m_analog_points.end(); ++it)
			indexes.emplace_back(it->first);
	return indexes;
}

inline std::unordered_map<std::size_t, double> SimPortPointData::GetValues(const odc::EventType& type)
{
	std::unordered_map<std::size_t, double> values;
	if (type == odc::EventType::Binary)
		for (auto it = m_binary_points.begin(); it != m_binary_points.end(); ++it)
			values[it->first] = it->second->value;
	else
		for (auto it = m_analog_points.begin(); it != m_analog_points.end(); ++it)
			values[it->first] = it->second->value;
	return values;
}

inline bool SimPortPointData::IsIndex(const odc::EventType& type, std::size_t index)
{
	if (type == odc::EventType::Binary)
		return m_binary_points.find(index) == m_binary_points.end();
	else
		return m_analog_points.find(index) == m_analog_points.end();
}
