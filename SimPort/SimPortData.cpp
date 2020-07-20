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

inline void SimPortData::SetAnalogPoint(std::size_t index, const std::shared_ptr<AnalogPoint>& point)
{
	m_ppoint_data->SetAnalogPoint(index, point);
}

inline double SimPortData::GetAnalogStartValue(std::size_t index) const
{
	return m_ppoint_data->GetAnalogStartValue(index);
}

inline double SimPortData::GetAnalogStdDev(std::size_t index) const
{
	return m_ppoint_data->GetAnalogStdDev(index);
}

inline void SimPortData::SetAnalogValue(std::size_t index, double value)
{
	m_ppoint_data->SetAnalogValue(index, value);
}

inline std::map<std::size_t, double> SimPortData::GetAnalogValues() const
{
	return m_ppoint_data->GetAnalogValues();
}
