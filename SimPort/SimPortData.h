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
 * SimPortData.h
 *
 *  Created on: 2020-07-15
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#ifndef SIMPORTDATA_H
#define SIMPORTDATA_H

#include "SimPortPointData.h"
#include <string>

class SimPortData
{
public:
	SimPortData();

	void HttpAddress(const std::string& http_addr);
	std::string HttpAddress() const;
	void HttpPort(const std::string& http_port);
	std::string HttpPort() const;
	void Version(const std::string& version);
	std::string Version() const;
	double DefaultStdDev() const;

	void CreateEvent(odc::EventType type, std::size_t index, const std::string& name,
		double std_dev, std::size_t update_interal, double value);
	void Event(std::shared_ptr<odc::EventInfo> event);
	std::shared_ptr<odc::EventInfo> Event(odc::EventType type, std::size_t index) const;
	void ForcedState(odc::EventType type, std::size_t index, bool state);
	bool ForcedState(odc::EventType type, std::size_t) const;
	void UpdateInterval(odc::EventType type, std::size_t index, std::size_t value);
	std::size_t UpdateInterval(odc::EventType type, std::size_t index) const;
	void Payload(odc::EventType type, std::size_t index, double payload);
	double Payload(odc::EventType type, std::size_t index) const;
	double StartValue(odc::EventType type, std::size_t index) const;
	double StdDev(std::size_t index) const;

	std::vector<std::size_t> Indexes(odc::EventType type) const;
	std::unordered_map<std::size_t, double> Values(odc::EventType type) const;

	bool IsIndex(odc::EventType type, std::size_t index) const;

private:
	std::string m_http_addr;
	std::string m_http_port;
	std::string m_version;
	double m_default_std_dev_factor;

	std::shared_ptr<SimPortPointData> m_ppoint_data;
};

#endif // SIMPORTDATA_H
