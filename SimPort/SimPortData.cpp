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
	m_default_std_dev_factor(0.01f) {}

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

