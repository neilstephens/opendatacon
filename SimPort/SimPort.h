/*	opendatacon
 *
 *	Copyright (c) 2014:
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
 * SimPort.h
 *
 *  Created on: 29/07/2015
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef SIMPORT_H
#define SIMPORT_H

#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <random>

using namespace odc;

class SimPort: public DataPort
{
public:
	//Implement DataPort interface
	SimPort(const std::string& Name, const std::string& File, const Json::Value& Overrides);
	void Enable() final;
	void Disable() final;
	void BuildOrRebuild() final;
	void ProcessElements(const Json::Value& JSONRoot) final;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) final;

private:
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	typedef std::shared_ptr<Timer_t> pTimer_t;
	std::vector<pTimer_t> Timers;
	void SpawnEvent(size_t index, double mean, double std_dev, unsigned int interval, pTimer_t pTimer, rand_t seed);
	void SpawnEvent(size_t index, bool val, unsigned int interval, pTimer_t pTimer, rand_t seed);
	void PortUp();
	void PortDown();

	std::unique_ptr<asio::strand> pEnableDisableSync;
	bool enabled;
	std::mt19937 RandNumGenerator;
};

#endif // SIMPORT_H
