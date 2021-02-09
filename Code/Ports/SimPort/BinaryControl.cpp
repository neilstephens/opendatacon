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
	std::size_t update_interval)
{
	std::unique_lock<std::shared_timed_mutex> lck(feedback_mutex);
	m_binary_feedbacks[index].emplace_back(std::make_shared<BinaryFeedback>(on, off, mode, update_interval));
	if (m_current_binary_events.find(index) == m_current_binary_events.end())
	{
		odc::EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type payload;
		payload.functionCode = odc::ControlCode::NUL;
		std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, on->GetSourcePort(), odc::QualityFlags::COMM_LOST);
		control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
		control_event->SetTimestamp(0);
		m_current_binary_events[index] = control_event;
	}
}

std::vector<std::shared_ptr<BinaryFeedback>> BinaryControl::BinaryFeedbacks(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(feedback_mutex);
	return m_binary_feedbacks[index];
}

void BinaryControl::CreateBinaryControl(std::size_t index,
	const std::string& port_source,
	odc::FeedbackType type,
	const std::vector<std::size_t>& indexes,
	const std::vector<odc::PositionAction>& action,
	std::size_t lower_limit, std::size_t raise_limit)
{
	std::unique_lock<std::shared_timed_mutex> lck(position_mutex);
	odc::EventTypePayload<odc::EventType::ControlRelayOutputBlock>::type payload;
	payload.functionCode = odc::ControlCode::NUL;
	std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, port_source, odc::QualityFlags::COMM_LOST);
	control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
	control_event->SetTimestamp(0);
	m_binary_positions[index] = std::make_shared<BinaryPosition>(type, action, indexes, lower_limit, raise_limit);
	m_current_binary_events[index] = control_event;
}

std::shared_ptr<BinaryPosition> BinaryControl::GetBinaryPosition(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(position_mutex);
	return m_binary_positions[index];
}

void BinaryControl::CreateBinaryControl(std::size_t index)
{
	m_current_binary_events[index] = nullptr;
}

bool BinaryControl::IsIndex(std::size_t index)
{
	return m_binary_feedbacks.find(index) != m_binary_feedbacks.end() ||
	       m_binary_positions.find(index) != m_binary_positions.end();
}

void BinaryControl::SetCurrentBinaryEvent(const std::shared_ptr<odc::EventInfo>& event, std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(current_mutex);
	m_current_binary_events[index] = event;
}

std::shared_ptr<odc::EventInfo> BinaryControl::GetCurrentBinaryEvent(std::size_t index)
{
	std::shared_lock<std::shared_timed_mutex> lck(current_mutex);
	return m_current_binary_events[index];
}

