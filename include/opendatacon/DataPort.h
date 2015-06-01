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

#include "IUIResponder.h"

class DataPort: public IOHandler, public ConfigParser
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

	std::future<opendnp3::CommandStatus> Event(ConnectState state, uint16_t index, const std::string& SenderName) final
	{
		if(MuxConnectionEvents(state, SenderName))
			return ConnectionEvent(state, SenderName);
		else
			return IOHandler::CommandFutureUndefined();
	};

	virtual std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) = 0;

    virtual const Json::Value GetStatistics() const
    {
        return Json::Value();
    };
    
    virtual const Json::Value GetCurrentState() const
    {
        return Json::Value();
    };

    virtual const Json::Value GetStatus() const
    {
        return Json::Value();
    };

protected:
	std::unique_ptr<DataPortConf> pConf;
};

#endif /* DATAPORT_H_ */
