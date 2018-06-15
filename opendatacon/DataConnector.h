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
 * DataConnector.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DATACONNECTOR_H_
#define DATACONNECTOR_H_

#include <opendatacon/IOHandler.h>
#include <opendatacon/IOManager.h>
#include <opendatacon/ConfigParser.h>
#include <opendatacon/Transform.h>

using namespace odc;

class DataConnector: public IOHandler, public ConfigParser
{
public:
	DataConnector(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~DataConnector() override {}

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	void Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	void Event(ConnectState state, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	virtual const Json::Value GetStatistics() const
	{
		return Json::Value();
	}

	virtual const Json::Value GetCurrentState() const
	{
		return Json::Value();
	}

	void Enable() override;
	void Disable() override;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL);

protected:
	void ProcessElements(const Json::Value& JSONRoot) override;
	template<typename T> void EventT(const T& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback);

	std::unordered_map<std::string,std::pair<IOHandler*,IOHandler*> > Connections;
	std::multimap<std::string,std::string> SenderConnectionsLookup;
	std::unordered_map<std::string,std::vector<std::unique_ptr<Transform, void (*)(Transform*)> > > ConnectionTransforms;
};

#endif /* DATACONNECTOR_H_ */
