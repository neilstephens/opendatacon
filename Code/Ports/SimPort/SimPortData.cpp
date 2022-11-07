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
 * SimPortData.cpp
 *
 *  Created on: 15/07/2020
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#include "SimPortData.h"

SimPortData::SimPortData():
	m_http_addr("0.0.0.0"),
	m_http_port(""),
	m_version("Unknown"),
	m_default_std_dev_factor(0.01f)
{
	m_ppoint_data = std::make_shared<SimPortPointData>();
}

void SimPortData::HttpAddress(const std::string& http_addr)
{
	m_http_addr = http_addr;
}

std::string SimPortData::HttpAddress() const
{
	return m_http_addr;
}

void SimPortData::HttpPort(const std::string& http_port)
{
	m_http_port = http_port;
}

std::string SimPortData::HttpPort() const
{
	return m_http_port;
}

void SimPortData::Version(const std::string& version)
{
	m_version = version;
}

std::string SimPortData::Version() const
{
	return m_version;
}

double SimPortData::DefaultStdDev() const
{
	return m_default_std_dev_factor;
}

void SimPortData::CreateEvent(odc::EventType type, std::size_t index, const std::string& name,
	odc::QualityFlags flag, double std_dev, std::size_t update_interal, double value)
{
	m_ppoint_data->CreateEvent(type, index, name, flag, std_dev, update_interal, value);
}

void SimPortData::Event(std::shared_ptr<odc::EventInfo> event)
{
	m_ppoint_data->Event(event);
}

void SimPortData::SetLatestControlEvent(std::shared_ptr<odc::EventInfo> event, std::size_t index)
{
	m_ppoint_data->SetLatestControlEvent(event, index);
}

std::shared_ptr<odc::EventInfo> SimPortData::Event(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->Event(type, index);
}

void SimPortData::ForcedState(odc::EventType type, std::size_t index, bool state)
{
	m_ppoint_data->ForcedState(type, index, state);
}

bool SimPortData::ForcedState(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->ForcedState(type, index);
}

void SimPortData::UpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	m_ppoint_data->UpdateInterval(type, index, value);
}

std::size_t SimPortData::UpdateInterval(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->UpdateInterval(type, index);
}

void SimPortData::Payload(odc::EventType type, std::size_t index, double payload)
{
	m_ppoint_data->Payload(type, index, payload);
}

double SimPortData::Payload(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->Payload(type, index);
}

double SimPortData::StartValue(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->StartValue(type, index);
}

odc::QualityFlags SimPortData::StartQuality(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->StartQuality(type, index);
}

double SimPortData::StdDev(std::size_t index) const
{
	return m_ppoint_data->StdDev(index);
}

std::vector<std::size_t> SimPortData::Indexes(odc::EventType type) const
{
	return m_ppoint_data->Indexes(type);
}

std::unordered_map<std::size_t, double> SimPortData::Values(odc::EventType type) const
{
	return m_ppoint_data->Values(type);
}

Json::Value SimPortData::CurrentState() const
{
	return m_ppoint_data->CurrentState();
}

std::string SimPortData::CurrentState(odc::EventType type, std::vector<std::size_t>& indexes) const
{
	return m_ppoint_data->CurrentState(type, indexes);
}

void SimPortData::Timer(const std::string& name, ptimer_t ptr)
{
	m_ppoint_data->Timer(name, ptr);
}

ptimer_t SimPortData::Timer(const std::string& name) const
{
	return m_ppoint_data->Timer(name);
}

void SimPortData::CancelTimers()
{
	m_ppoint_data->CancelTimers();
}

bool SimPortData::IsIndex(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->IsIndex(type, index);
}

void SimPortData::CreateBinaryControl(std::size_t index,
	const std::shared_ptr<odc::EventInfo>& on,
	const std::shared_ptr<odc::EventInfo>& off,
	FeedbackMode mode,
	uint32_t delay,
	std::size_t update_interal)
{
	m_ppoint_data->CreateBinaryControl(index, on, off, mode, delay, update_interal);
}

std::vector<std::shared_ptr<BinaryFeedback>> SimPortData::BinaryFeedbacks(std::size_t index) const
{
	return m_ppoint_data->BinaryFeedbacks(index);
}

void SimPortData::CreateBinaryControl(std::size_t index,
	const std::string& port_source,
	odc::FeedbackType type,
	const std::vector<std::size_t>& indexes,
	const std::vector<odc::PositionAction>& action,
	std::size_t lower_limit, std::size_t raise_limit,
	double tap_step)
{
	m_ppoint_data->CreateBinaryControl(index, port_source, type, indexes, action, lower_limit, raise_limit, tap_step);
}

std::shared_ptr<PositionFeedback> SimPortData::GetPositionFeedback(std::size_t index) const
{
	return m_ppoint_data->GetPositionFeedback(index);
}

void SimPortData::CreateBinaryControl(std::size_t index)
{
	m_ppoint_data->CreateBinaryControl(index);
}
