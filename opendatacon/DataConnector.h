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
#include <opendatacon/ConfigParser.h>
#include <opendatacon/Transform.h>

using namespace odc;

class DataConnector: public IOHandler, public ConfigParser
{
public:
	DataConnector(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~DataConnector() override {}

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void Event(ConnectState state, const std::string& SenderName) override;

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
	void Build();

protected:
	void ProcessElements(const Json::Value& JSONRoot) override;

	std::unordered_map<std::string,std::pair<IOHandler*,IOHandler*> > Connections;
	std::multimap<std::string,std::string> SenderConnectionsLookup;
	std::unordered_map<std::string,std::vector<std::unique_ptr<Transform, std::function<void(Transform*)>> > > ConnectionTransforms;
};

#endif /* DATACONNECTOR_H_ */
