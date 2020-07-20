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

inline void SimPortPointData::SetAnalogPoint(std::size_t index, std::shared_ptr<AnalogPoint> point)
{
	m_analog_points[index] = point;
}

inline double SimPortPointData::GetAnalogStartValue(std::size_t index) const
{
	return m_analog_points[index]->start_value;
}

inline double SimPortPointData::GetAnalogStdDev(std::size_t index) const
{
	return m_analog_points[index]->std_dev;
}

inline void SimPortPointData::SetAnalogPoint(std::size_t index, double value)
{
	m_analog_points[index] = value;
}

inline std::map<std::size_t, double> SimPortPointData::GetAnalogValues() const
{
	std::map<std::size_t, double> values;
	for (auto it = m_analog_points.begin(); it != m_analog_points.end(); ++it)
		values[it->first] = it->second.start_value;
	return std::move(values);
}
