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

#include "DataPortConf.h"
#include "IOHandler.h"
#include "ConfigParser.h"
#include "IUIResponder.h"
#include "EventDB.h"

namespace odc
{

class DataPort: public IOHandler, public ConfigParser, public std::enable_shared_from_this<DataPort>
{
public:
	DataPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		IOHandler(aName),
		ConfigParser(aConfFilename, aConfOverrides),
		pConf(nullptr)
	{}
	~DataPort() override {}

	virtual void Enable() override = 0;
	virtual void Disable() override = 0;
	virtual void Build() = 0;
	virtual void ProcessElements(const Json::Value& JSONRoot) override = 0;
	virtual void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

	void Event(ConnectState state, const std::string& SenderName) final
	{
		MuxConnectionEvents(state, SenderName);
	}

	virtual const std::unique_ptr<EventDB>& pEventDB() const
	{
		return pDB;
	}

	virtual std::pair<std::string,const IUIResponder*> GetUIResponder()
	{
		IUIResponder* pR(nullptr);
		std::string s = "";
		std::pair<std::string,const IUIResponder*> pair(s,pR);
		return pair;
	}

	virtual const Json::Value GetStatistics() const
	{
		return Json::Value();
	}

	virtual const Json::Value GetCurrentState() const
	{
		return Json::Value();
	}

	virtual const Json::Value GetStatus() const
	{
		return Json::Value();
	}

protected:
	std::unique_ptr<DataPortConf> pConf;
	std::unique_ptr<EventDB> pDB;
};

}

#endif /* DATAPORT_H_ */
