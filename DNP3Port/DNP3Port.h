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
 * DNP3Port.h
 *
 *  Created on: 18/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3PORT_H_
#define DNP3PORT_H_

#include <opendatacon/DataPort.h>
#include "DNP3PortConf.h"

class DNP3Port: public DataPort
{
public:
	DNP3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	virtual void Enable()=0;
	virtual void Disable()=0;
	virtual void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)=0;

	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};

	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};
	virtual std::future<opendnp3::CommandStatus> Event(bool connected, uint16_t index, const std::string& SenderName)
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	};

	void ProcessElements(const Json::Value& JSONRoot);

protected:
    asiodnp3::IChannel* pChannel;    
	static std::unordered_map<std::string, asiodnp3::IChannel*> TCPChannels;

};

#endif /* DNP3PORT_H_ */
