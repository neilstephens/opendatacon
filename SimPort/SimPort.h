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
#include "SimPortConf.h"
#include "../HTTP/HttpServerManager.h"
#include "sqlite3/sqlite3.h"
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <opendatacon/EnumClassFlags.h>
#include <shared_mutex>
#include <random>

using days = std::chrono::duration<int, std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>;

class SimPortCollection;
class SimPort: public DataPort
{
public:
	//Implement DataPort interface
	SimPort(const std::string& Name, const std::string& File, const Json::Value& Overrides);
	void Enable() final;
	void Disable() final;
	void Build() final;
	void ProcessElements(const Json::Value& json_root) final;
	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) final;
	std::pair<std::string,std::shared_ptr<IUIResponder>> GetUIResponder() final;

	//Implement ODC::DataPort functions for UI
	const Json::Value GetCurrentState() const override;
	const Json::Value GetStatistics() const override;
	const Json::Value GetStatus() const override;

	bool UILoad(odc::EventType type, const std::string &index, const std::string &value, const std::string &quality, const std::string &timestamp, const bool force);
	bool UIRelease(odc::EventType type, const std::string& index);
	bool SetForcedState(const std::string& index, odc::EventType type, bool forced);
	bool UISetUpdateInterval(odc::EventType type, const std::string& index, const std::string& period);

private:
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	typedef std::shared_ptr<Timer_t> pTimer_t;
	std::unordered_map<std::string, pTimer_t> Timers;

	// use this instead of PublishEvent, it catches current values and saves them.
	void PostPublishEvent(std::shared_ptr<odc::EventInfo> event, SharedStatusCallback_t pStatusCallback);

	void NextEventFromDB(const std::shared_ptr<odc::EventInfo>& event);
	void PopulateNextEvent(const std::shared_ptr<odc::EventInfo>& event, int64_t time_offset);
	void SpawnEvent(const std::shared_ptr<odc::EventInfo>& event, int64_t time_offset = 0);
	inline void RandomiseAnalog(std::shared_ptr<odc::EventInfo> event)
	{
		double mean = pSimConf->StartValue(odc::EventType::Analog, event->GetIndex());
		double std_dev = pSimConf->StdDev(event->GetIndex());
		//change value around mean - handle 0 which windows does not...
		if (std_dev != 0)
		{
			std::normal_distribution<double> distribution(mean, std_dev);
			event->SetPayload<odc::EventType::Analog>(distribution(RandNumGenerator));
		}
		else
		{
			event->SetPayload<odc::EventType::Analog>(std::move(mean));
		}
	}
	inline void StartAnalogEvents(size_t index)
	{
		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Analog,index,Name);
		RandomiseAnalog(event);
		SpawnEvent(event);
	}
	inline void StartAnalogEvents(size_t index, double val)
	{
		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Analog,index,Name);
		event->SetPayload<odc::EventType::Analog>(std::move(val));
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
		auto event = std::make_shared<odc::EventInfo>(odc::EventType::Binary,index,Name);
		event->SetPayload<odc::EventType::Binary>(std::move(val));
		SpawnEvent(event);
	}
	void PortUp();
	void PortDown();
	std::vector<std::size_t> IndexesFromString(const std::string& index_str, odc::EventType type);

	CommandStatus HandleBinaryFeedback(const std::vector<std::shared_ptr<BinaryFeedback>>& feedbacks,
		std::size_t index, ControlRelayOutputBlock command);
	CommandStatus HandleBinaryPosition(const std::shared_ptr<BinaryPosition>& binary_position);
	CommandStatus HandleBinaryPositionForAnalog(const std::shared_ptr<BinaryPosition>& binary_position);
	CommandStatus HandleBinaryPositionForBinary(const std::shared_ptr<BinaryPosition>& binary_position);
	CommandStatus HandleBinaryPositionForBCD(const std::shared_ptr<BinaryPosition>& binary_position);

	std::shared_ptr<SimPortCollection> SimCollection;

	mutable std::shared_timed_mutex ConfMutex;

	std::unique_ptr<asio::io_service::strand> pEnableDisableSync;
	static thread_local std::mt19937 RandNumGenerator;
	ServerTokenType pServer;
	SimPortConf* pSimConf = nullptr; // Set in constructor
};

#endif // SIMPORT_H
