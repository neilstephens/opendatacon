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

double SimPortData::GetDefaultStdDev() const
{
	return m_default_std_dev_factor;
}

void SimPortData::SetPoint(odc::EventType type, std::size_t index, const std::string& name,
	double std_dev, std::size_t update_interal, double value)
{
	m_ppoint_data->SetPoint(type, index, name, std_dev, update_interal, value);
}

void SimPortData::SetForcedState(odc::EventType type, std::size_t index, bool state)
{
	m_ppoint_data->SetForcedState(type, index, state);
}

bool SimPortData::GetForcedState(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->GetForcedState(type, index);
}

void SimPortData::SetUpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	m_ppoint_data->SetUpdateInterval(type, index, value);
}

std::size_t SimPortData::GetUpdateInterval(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->GetUpdateInterval(type, index);
}

void SimPortData::SetPayload(odc::EventType type, std::size_t index, double payload)
{
	m_ppoint_data->SetPayload(type, index, payload);
}

double SimPortData::GetPayload(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->GetPayload(type, index);
}

double SimPortData::GetStartValue(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->GetStartValue(type, index);
}

double SimPortData::GetStdDev(std::size_t index) const
{
	return m_ppoint_data->GetStdDev(index);
}

std::vector<std::size_t> SimPortData::GetIndexes(odc::EventType type) const
{
	return m_ppoint_data->GetIndexes(type);
}

std::unordered_map<std::size_t, double> SimPortData::GetValues(odc::EventType type) const
{
	return m_ppoint_data->GetValues(type);
}

bool SimPortData::IsIndex(odc::EventType type, std::size_t index) const
{
	return m_ppoint_data->IsIndex(type, index);
}
