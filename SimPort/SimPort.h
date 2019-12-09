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
#include <shared_mutex>
#include <random>

#include "SimPortConf.h"
#include "sqlite3/sqlite3.h"

using namespace odc;

class SimPortCollection;
class SimPort: public DataPort
{
public:
	//Implement DataPort interface
	SimPort(const std::string& Name, const std::string& File, const Json::Value& Overrides);
	void Enable() final;
	void Disable() final;
	void Build() final;
	void ProcessElements(const Json::Value& JSONRoot) final;
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) final;
	std::pair<std::string,std::shared_ptr<IUIResponder>> GetUIResponder() final;

	bool UILoad(const std::string &type, const std::string &index, const std::string &value, const std::string &quality, const std::string &timestamp, const bool force);
	bool UIRelease(const std::string& type, const std::string& index);
	bool UISetUpdateInterval(const std::string& type, const std::string& index, const std::string& period);

private:
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	typedef std::shared_ptr<Timer_t> pTimer_t;
	std::unordered_map<std::string, pTimer_t> Timers;
	typedef std::shared_ptr<sqlite3> pDBConnection;
	std::unordered_map<std::string, pDBConnection> DBConns;
	typedef std::shared_ptr<sqlite3_stmt> pDBStatement;
	std::unordered_map<std::string, pDBStatement> DBStats;
	void PopulateNextEvent(std::shared_ptr<EventInfo> event);
	void SpawnEvent(std::shared_ptr<EventInfo> event);
	inline void RandomiseAnalog(std::shared_ptr<EventInfo> event)
	{
		auto pConf = static_cast<SimPortConf*>(this->pConf.get());
		double mean, std_dev;
		{ //lock scope
			std::shared_lock<std::shared_timed_mutex> lck(ConfMutex);
			mean = pConf->AnalogStartVals.at(event->GetIndex());
			std_dev = pConf->AnalogStdDevs.at(event->GetIndex());
		}
		//change value around mean
		std::normal_distribution<double> distribution(mean, std_dev);
		event->SetPayload<EventType::Analog>(distribution(RandNumGenerator));
	}
	inline void StartAnalogEvents(size_t index)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog,index,Name);
		RandomiseAnalog(event);
		SpawnEvent(event);
	}
	inline void StartAnalogEvents(size_t index, double val)
	{
		auto event = std::make_shared<EventInfo>(EventType::Analog,index,Name);
		event->SetPayload<EventType::Analog>(std::move(val));
		SpawnEvent(event);
	}
	inline void StartBinaryEvents(size_t index)
	{
		std::uniform_int_distribution<int> distribution(0,1);
		bool val = static_cast<bool>(distribution(RandNumGenerator));
		StartBinaryEvents(index,val);
	}
	inline void StartBinaryEvents(size_t index, bool val)
	{
		auto event = std::make_shared<EventInfo>(EventType::Binary,index,Name);
		event->SetPayload<EventType::Binary>(std::move(val));
		SpawnEvent(event);
	}
	void PortUp();
	void PortDown();
	std::vector<uint32_t> IndexesFromString(const std::string& index_str, const std::string &type);

	std::shared_ptr<SimPortCollection> SimCollection;

	std::shared_timed_mutex ConfMutex;

	std::unique_ptr<asio::io_service::strand> pEnableDisableSync;
	static thread_local std::mt19937 RandNumGenerator;
};

#endif // SIMPORT_H
