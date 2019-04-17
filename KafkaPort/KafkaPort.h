/*	opendatacon
 *
 *	Copyright (c) 2019:
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
 * KafkaPort.h
 *
 *  Created on: 16/04/2019
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - derived from DNP3 version
 */

#ifndef KafkaPORT_H_
#define KafkaPORT_H_

#include <unordered_map>
#include <vector>
#include <functional>

#include <opendatacon/DataPort.h>

#include "Kafka.h"
#include "KafkaPortConf.h"


using namespace odc;
typedef std::string KafkaMessage_t;

class KafkaPort: public DataPort
{
public:
	KafkaPort(const std::string& aName, const std::string & aConfFilename, const Json::Value &aConfOverrides);
	~KafkaPort();

	void ProcessElements(const Json::Value& JSONRoot) final;

	void SocketStateHandler(bool state);

	void Enable() override;
	void Disable() override;
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void SendKafkaMessage(const std::string &key, double measurement, QualityFlags quality);


	// Testing use only
	KafkaPointTableAccess *GetPointTable() { return &(MyPointConf->PointTable); }
	KafkaAddrConf *GetAddrAccess() { return &(MyConf->mAddrConf); }
	std::string GetTopic() { return MyPointConf->Topic; };

protected:

	// Worker functions to try and clean up the code...
	KafkaPortConf *MyConf;
	std::shared_ptr<KafkaPointConf> MyPointConf;
};

#endif
