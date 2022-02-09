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
	odc::QualityFlags flag, double s_dev, std::size_t u_interval, double val)
{
	if (type == odc::EventType::Analog)
	{
		Point p;
		p.std_dev = s_dev;
		p.update_interval = u_interval;
		p.start_value = val;

		odc::QualityFlags qflag = flag;
		if (!s_dev && !u_interval && !val)
			qflag = odc::QualityFlags::COMM_LOST;
		auto evt = std::make_shared<odc::EventInfo>(odc::EventType::Analog, index, name, qflag);
		evt->SetPayload<odc::EventType::Analog>(std::move(val));
		p.event = evt;
		{ // lock scope
			std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
			m_points[odc::EventType::Analog][index] = std::make_shared<Point>(p);
		}
	}

	if (type == odc::EventType::Binary)
	{
		Point p;
		p.std_dev = s_dev;
		p.update_interval = u_interval;

		auto evt = std::make_shared<odc::EventInfo>(odc::EventType::Binary, index, name, flag);
		bool v = static_cast<bool>(val);
		evt->SetPayload<odc::EventType::Binary>(std::move(v));
		p.event = evt;
		{ // lock scope
			std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
			m_points[odc::EventType::Binary][index] = std::make_shared<Point>(p);
		}
	}
}

std::shared_ptr<odc::EventInfo> SimPortPointData::Event(odc::EventType type, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	return m_points[type][index]->event;
}

void SimPortPointData::Event(std::shared_ptr<odc::EventInfo> event)
{
	std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
	m_points[event->GetEventType()][event->GetIndex()]->event = event;
}

void SimPortPointData::SetLatestControlEvent(std::shared_ptr<odc::EventInfo> event, std::size_t index)
{
	std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
	m_binary_control.SetLatestControlEvent(event, index);
}

void SimPortPointData::ForcedState(odc::EventType type, std::size_t index, bool state)
{
	std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
	m_points[type][index]->forced_state = state;
}

bool SimPortPointData::ForcedState(odc::EventType type, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	return m_points[type][index]->forced_state;
}

void SimPortPointData::UpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
	m_points[type][index]->update_interval = value;
}

std::size_t SimPortPointData::UpdateInterval(odc::EventType type, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	return m_points[type][index]->update_interval;
}

void SimPortPointData::Payload(odc::EventType type, std::size_t index, double payload)
{
	std::unordered_map<std::size_t, std::shared_ptr<Point>> points;
	{ // lock scope
		std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
		points = m_points[type];
	}
	if (points.find(index) == points.end())
	{
		if(auto log = odc::spdlog_get("SimPort"))
		{
			log->error("{} Index == {} not found on the points container, why they are asking for set?", ToString(type), index);
			return;
		}
	}
	if (type == odc::EventType::Analog)
	{
		std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
		m_points[type][index]->event->SetPayload<odc::EventType::Analog>(std::move(payload));
	}
	if (type == odc::EventType::Binary)
	{
		std::unique_lock<std::shared_timed_mutex> lck(point_mutex);
		m_points[type][index]->event->SetPayload<odc::EventType::Binary>(std::move(static_cast<bool>(payload)));
	}
}

double SimPortPointData::Payload(odc::EventType type, std::size_t index)
{
	double payload = 0.0f;
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	if (type == odc::EventType::Analog)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Analog>();
	if (type == odc::EventType::Binary)
		payload = m_points[type][index]->event->GetPayload<odc::EventType::Binary>();
	return payload;
}

double SimPortPointData::StartValue(odc::EventType type, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	return m_points[type][index]->start_value;
}

double SimPortPointData::StdDev(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	return m_points[odc::EventType::Analog][index]->std_dev;
}

std::vector<std::size_t> SimPortPointData::Indexes(odc::EventType type)
{
	std::unordered_map<std::size_t, std::shared_ptr<Point>> points;
	std::vector<std::size_t> indexes;
	{ // lock scope
		std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
		points = m_points[type];
	}
	for (auto it = points.begin(); it != points.end(); ++it)
		indexes.emplace_back(it->first);
	return indexes;
}

std::unordered_map<std::size_t, double> SimPortPointData::Values(odc::EventType type)
{
	std::unordered_map<std::size_t, std::shared_ptr<Point>> points;
	std::unordered_map<std::size_t, double> values;
	{
		std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
		points = m_points[type];
	}
	for (auto it = points.begin(); it != points.end(); ++it)
	{
		if (type == odc::EventType::Analog)
			values[it->first] = it->second->event->GetPayload<odc::EventType::Analog>();
		if (type == odc::EventType::Binary)
			values[it->first] = it->second->event->GetPayload<odc::EventType::Binary>();
	}
	return values;
}

Json::Value SimPortPointData::CurrentState()
{
	Json::Value state;
	std::vector<odc::EventType> type = {odc::EventType::Binary, odc::EventType::Analog};
	for (auto t : type)
	{
		for (auto it : m_points[t])
		{
			if (t == odc::EventType::Binary)
				state[odc::ToString(t)+"Payload"][std::to_string(it.first)] = std::to_string(it.second->event->GetPayload<odc::EventType::Binary>());
			else
				state[odc::ToString(t)+"Payload"][std::to_string(it.first)] = std::to_string(it.second->event->GetPayload<odc::EventType::Analog>());
			state[odc::ToString(t)+"Quality"][std::to_string(it.first)] = odc::ToString(it.second->event->GetQuality());
			state[odc::ToString(t)+"Timestamp"][std::to_string(it.first)] = odc::since_epoch_to_datetime(it.second->event->GetTimestamp());
		}
	}
	return state;
}

std::string SimPortPointData::CurrentState(odc::EventType type, std::vector<std::size_t>& indexes)
{
	Json::Value state;
	for (std::size_t index : indexes)
	{
		std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
		if (type == odc::EventType::Binary)
		{
			const bool val = m_points[type][index]->event->GetPayload<odc::EventType::Binary>();
			state[std::to_string(index)][odc::ToString(type)+"Payload"] = std::to_string(val);
		}
		if (type == odc::EventType::Analog)
		{
			const double val = m_points[type][index]->event->GetPayload<odc::EventType::Analog>();
			state[std::to_string(index)][odc::ToString(type)+"Payload"] = std::to_string(val);
		}
		if (type == odc::EventType::ControlRelayOutputBlock)
		{
			auto val = m_binary_control.GetCurrentBinaryEvent(index)->GetPayload<odc::EventType::ControlRelayOutputBlock>();
			state[std::to_string(index)][odc::ToString(type)+"Payload"] = std::string(val);
		}
		if (type == odc::EventType::Binary || type == odc::EventType::Analog)
		{
			state[std::to_string(index)][odc::ToString(type)+"Quality"] = odc::ToString(m_points[type][index]->event->GetQuality());
			state[std::to_string(index)][odc::ToString(type)+"Timestamp"] = odc::since_epoch_to_datetime(m_points[type][index]->event->GetTimestamp());
		}
		if (type == odc::EventType::ControlRelayOutputBlock)
		{
			state[std::to_string(index)][odc::ToString(type)+"Quality"] = odc::ToString(m_binary_control.GetCurrentBinaryEvent(index)->GetQuality());
			state[std::to_string(index)][odc::ToString(type)+"Timestamp"] = odc::since_epoch_to_datetime(m_binary_control.GetCurrentBinaryEvent(index)->GetTimestamp());
		}
	}
	Json::StreamWriterBuilder wbuilder;
	wbuilder["indentation"] = "";
	const std::string result = Json::writeString(wbuilder, state);
	return result;
}

void SimPortPointData::Timer(const std::string& name, ptimer_t ptr)
{
	std::unique_lock<std::shared_timed_mutex> lck(timer_mutex);
	m_timers[name] = ptr;
}

ptimer_t SimPortPointData::Timer(const std::string& name)
{
	std::shared_lock<std::shared_timed_mutex> lck(timer_mutex);
	return m_timers.at(name);
}

void SimPortPointData::CancelTimers()
{
	std::unique_lock<std::shared_timed_mutex> lck(timer_mutex);
	for (const auto& timer : m_timers)
		timer.second->cancel();
	m_timers.clear();
}

bool SimPortPointData::IsIndex(odc::EventType type, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(point_mutex);
	if (type == odc::EventType::Analog || type == odc::EventType::Binary)
		return m_points[type].find(index) != m_points[type].end();
	else if (type == odc::EventType::ControlRelayOutputBlock)
		return m_binary_control.IsIndex(index);
	else
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->error("IsIndex() called for unsupported event type '{}'", ToString(type));
		return false;
	}
}

void SimPortPointData::CreateBinaryControl(std::size_t index,
	const std::shared_ptr<odc::EventInfo>& on,
	const std::shared_ptr<odc::EventInfo>& off,
	FeedbackMode mode,
	std::size_t update_interval)
{
	m_binary_control.CreateBinaryControl(index, on, off, mode, update_interval);
}

std::vector<std::shared_ptr<BinaryFeedback>> SimPortPointData::BinaryFeedbacks(std::size_t index)
{
	return m_binary_control.BinaryFeedbacks(index);
}

void SimPortPointData::CreateBinaryControl(std::size_t index,
	const std::string& port_source,
	odc::FeedbackType type,
	const std::vector<std::size_t>& indexes,
	const std::vector<odc::PositionAction>& action,
	std::size_t lower_limit, std::size_t raise_limit,
	double tap_step)
{
	m_binary_control.CreateBinaryControl(index, port_source, type, indexes, action, lower_limit, raise_limit, tap_step);
}

std::shared_ptr<PositionFeedback> SimPortPointData::GetPositionFeedback(std::size_t index)
{
	return m_binary_control.GetPositionFeedback(index);
}

void SimPortPointData::CreateBinaryControl(std::size_t index)
{
	m_binary_control.CreateBinaryControl(index);
}
