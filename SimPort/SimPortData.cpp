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

inline void SimPortData::HttpAddress(const std::string& http_addr)
{
	m_http_addr = http_addr;
}

inline std::string SimPortData::HttpAddress() const
{
	return m_http_addr;
}

inline void SimPortData::HttpPort(const std::string& http_port)
{
	m_http_port = http_port;
}

inline std::string SimPortData::HttpPort() const
{
	return m_http_port;
}

inline void SimPortData::Version(const std::string& version)
{
	m_version = version;
}

inline std::string SimPortData::Version() const
{
	return m_version;
}

inline double SimPortData::GetDefaultStdDev() const
{
	return m_default_std_dev_factor;
}

inline void SimPortData::SetPoint(const odc::EventType& type, std::size_t index, const std::shared_ptr<Point>& point)
{
	m_ppoint_data->SetPoint(type, index, point);
}

inline void SimPortData::SetForcedState(const odc::EventType& type, std::size_t index, bool state)
{
	m_ppoint_data->SetForcedState(type, index, state);
}

inline bool SimPortData::GetForcedState(const odc::EventType& type, std::size_t index) const
{
	return m_ppoint_data->GetForcedState(type, index);
}

inline void SimPortData::SetUpdateInterval(const odc::EventType& type, std::size_t index, std::size_t value)
{
	m_ppoint_data->SetUpdateInterval(type, index, value);
}

inline std::size_t SimPortData::GetUpdateInterval(const odc::EventType& type, std::size_t index) const
{
	return m_ppoint_data->GetUpdateInterval(type, index);
}

inline double SimPortData::GetStartValue(const odc::EventType& type, const std::size_t index) const
{
	return m_ppoint_data->GetStartValue(type, index);
}

inline double SimPortData::GetStdDev(std::size_t index) const
{
	return m_ppoint_data->GetStdDev(index);
}

inline void SimPortData::SetValue(const odc::EventType& type, std::size_t index, double value)
{
	m_ppoint_data->SetValue(type, index, value);
}

inline double SimPortData::GetValue(const odc::EventType& type, std::size_t index) const
{
	return m_ppoint_data->GetValue(type, index);
}

inline std::vector<std::size_t> SimPortData::GetIndexes(const odc::EventType& type) const
{
	return m_ppoint_data->GetIndexes(type);
}

inline std::unordered_map<std::size_t, double> SimPortData::GetValues(const odc::EventType& type) const
{
	return m_ppoint_data->GetValues(type);
}

inline bool SimPortData::IsIndex(const odc::EventType& type, std::size_t index) const
{
	return m_ppoint_data->IsIndex(type, index);
}
