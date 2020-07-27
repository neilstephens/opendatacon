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

#include <opendatacon/IOTypes.h>
#include <vector>
#include <memory>
#include <unordered_map>

struct Point
{
	Point():
		std_dev(0.0f),
		update_interval(0),
		start_value(0.0f),
		forced_state(false) {}

	std::shared_ptr<odc::EventInfo> event;
	double std_dev;
	// This refresh rate is in milliseconds
	std::size_t update_interval;
	double start_value;
	bool forced_state;
};

class SimPortPointData
{
public:
	SimPortPointData();

	void CreateEvent(odc::EventType type, std::size_t index, const std::string& name,
		double s_dev, std::size_t u_interval, double val);
	void Event(std::shared_ptr<odc::EventInfo> event);
	std::shared_ptr<odc::EventInfo> Event(odc::EventType type, std::size_t index);
	void ForcedState(odc::EventType type, std::size_t index, bool state);
	bool ForcedState(odc::EventType type, std::size_t index);
	void UpdateInterval(odc::EventType type, std::size_t index, std::size_t value);
	std::size_t UpdateInterval(odc::EventType type, std::size_t index);
	void Payload(odc::EventType type, std::size_t index, double payload);
	double Payload(odc::EventType type, std::size_t index);
	double StartValue(odc::EventType type, std::size_t index);
	double StdDev(std::size_t index);

	std::vector<std::size_t> Indexes(odc::EventType type);
	std::unordered_map<std::size_t , double> Values(odc::EventType type);

	bool IsIndex(odc::EventType type, std::size_t index);

private:
	using Points = std::unordered_map<std::size_t, std::shared_ptr<Point>>;
	std::unordered_map<odc::EventType, Points> m_points;
};

#endif // SIMPORTPOINTDATA_H
