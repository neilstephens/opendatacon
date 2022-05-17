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
 * BinaryControl.cpp
 *
 *  Created on: 2021-02-08
 *  A year of hope that pandemic will end soon
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#include "BinaryControl.h"

void BinaryControl::CreateBinaryControl(std::size_t index,
	const std::shared_ptr<odc::EventInfo>& on,
	const std::shared_ptr<odc::EventInfo>& off,
	FeedbackMode mode,
	uint32_t delay,
	std::size_t update_interval)
{
	{ // for the scope of lock
		std::unique_lock<std::shared_timed_mutex> lck(feedback_mutex);
		m_binary_feedbacks[index].emplace_back(std::make_shared<BinaryFeedback>(on, off, mode, delay, update_interval));
	}
	if (!IsIndex(index))
	{
		odc::EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type payload;
		payload.functionCode = odc::ControlCode::NUL;
		std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, on->GetSourcePort(), odc::QualityFlags::COMM_LOST);
		control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
		control_event->SetTimestamp(0);
		{ // for the scope of the lock
			std::unique_lock<std::shared_timed_mutex> lck(current_mutex);
			m_latest_control_events[index] = control_event;
		}
	}
}

std::vector<std::shared_ptr<BinaryFeedback>> BinaryControl::BinaryFeedbacks(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(feedback_mutex);
	std::vector<std::shared_ptr<BinaryFeedback>> feedback;
	if (m_binary_feedbacks.find(index) != m_binary_feedbacks.end())
		feedback = m_binary_feedbacks[index];
	return feedback;
}

void BinaryControl::CreateBinaryControl(std::size_t index,
	const std::string& port_source,
	odc::FeedbackType type,
	const std::vector<std::size_t>& indexes,
	const std::vector<odc::PositionAction>& action,
	std::size_t lower_limit, std::size_t raise_limit,
	double tap_step)
{
	odc::EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type payload;
	payload.functionCode = odc::ControlCode::NUL;
	std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, port_source, odc::QualityFlags::COMM_LOST);
	control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
	control_event->SetTimestamp(0);
	{ // scope of the lock
		std::unique_lock<std::shared_timed_mutex> lck(position_mutex);
		m_position_feedbacks[index] = std::make_shared<PositionFeedback>(type, action, indexes, lower_limit, raise_limit, tap_step);
	}
	{ // scope of the lock
		std::unique_lock<std::shared_timed_mutex> lck(current_mutex);
		m_latest_control_events[index] = control_event;
	}
}

std::shared_ptr<PositionFeedback> BinaryControl::GetPositionFeedback(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(position_mutex);
	return m_position_feedbacks[index];
}

void BinaryControl::CreateBinaryControl(std::size_t index)
{
	odc::EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type payload;
	payload.functionCode = odc::ControlCode::NUL;
	std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, "NULL", odc::QualityFlags::COMM_LOST);
	control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
	control_event->SetTimestamp(0);
	{ // scope of the lock
		std::unique_lock<std::shared_timed_mutex> lck(current_mutex);
		m_latest_control_events[index] = control_event;
	}
}

bool BinaryControl::IsIndex(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(current_mutex);
	return m_latest_control_events.find(index) != m_latest_control_events.end();
}

void BinaryControl::SetLatestControlEvent(const std::shared_ptr<odc::EventInfo>& event, std::size_t index)
{
	std::unique_lock<std::shared_timed_mutex> lck(current_mutex);
	m_latest_control_events[index] = event;
}

std::shared_ptr<odc::EventInfo> BinaryControl::GetCurrentBinaryEvent(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(current_mutex);
	return m_latest_control_events[index];
}

