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
 * SimPortPointData.h
 *
 *  Created on: 2020-07-15
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#ifndef SIMPORTPOINTDATA_H
#define SIMPORTPOINTDATA_H

#include <map>
#include <memory>
#include <unordered_map>

struct AnalogPoint
{
	AnalogPoint(double start_val,
		std::size_t refresh_rate,
		double standard_dev):
		start_value(start_val),
		value(start_val),
		forced_state(false),
		update_interval(refresh_rate),
		std_dev(standard_dev) {}

	double start_value;
	double value;
	bool forced_state;
	// This time interval is in milliseconds.
	std::size_t update_interval;
	// Standard deviation
	double std_dev;
};

struct BinaryPoint
{
	BinaryPoint() {}

	bool start_val;
	bool value;
	bool forced_state;
	// This time interval is in milliseconds.
	std::size_t update_interval;
};

class SimPortPointData
{
public:
	SimPortPointData();

	void SetAnalogPoint(std::size_t index, std::shared_ptr<AnalogPoint> point);
	double GetAnalogStartValue(std::size_t index) const;
	double GetAnalogStdDev(std::size_t index) const;

	void SetAnalogValue(std::size_t index, double value);

	std::map<std::size_t , double> GetAnalogValues() const;

private:
	std::unordered_map<std::size_t, std::shared_ptr<AnalogPoint>> m_analog_points;
	std::unordered_map<std::size_t, std::shared_ptr<BinaryPoint>> m_binary_points;
};

#endif // SIMPORTPOINTDATA_H
