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
 * DataPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DATAPORT_H_
#define DATAPORT_H_

#include <asiodnp3/DNP3Manager.h>
#include "DataPortConf.h"
#include "IOHandler.h"
#include "ConfigParser.h"
#include "IJsonResponder.h"

class DataPort: public IOHandler, public ConfigParser, public IJsonResponder
{
public:
	DataPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
		IOHandler(aName),
		ConfigParser(aConfFilename, aConfOverrides),
		pConf(nullptr)
	{};

	virtual void Enable()=0;
	virtual void Disable()=0;
	virtual void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)=0;
	virtual void ProcessElements(const Json::Value& JSONRoot)=0;

	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName)=0;

	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName)=0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName)=0;
    
    //Default implementation of IJsonResponder
    virtual Json::Value GetCurrentState(const ParamCollection& params) const
    {
        Json::Value event;
        return event;
    }
    
    virtual Json::Value GetStatistics(const ParamCollection& params) const
    {
        Json::Value event;
        return event;
    }

    virtual Json::Value GetResponse(const ParamCollection& params) const
    {
        Json::Value event;
        
        event["Configuration"] = GetConfiguration();
        event["CurrentState"] = GetCurrentState(params);
        event["Statistics"] = GetStatistics(params);
        
        return event;
    };

protected:
	std::unique_ptr<DataPortConf> pConf;
};

#endif /* DATAPORT_H_ */
