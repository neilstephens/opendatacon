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
 * KafkaOutstationPortConf.h
 *
 *  Created on: 16/04/2019
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - derived from DNP3 version
 */

#ifndef KafkaOUTSTATIONPORTCONF_H_
#define KafkaOUTSTATIONPORTCONF_H_

#include <opendatacon/DataPort.h>
#include "KafkaPointConf.h"


struct KafkaAddrConf
{
	//IP
	std::string IP;
	std::string Port;

	// Default address values can minimally set IP.
	KafkaAddrConf():
		IP("127.0.0.1"),
		Port("20000")
	{}

private:

};

class KafkaPortConf: public DataPortConf
{
public:
	KafkaPortConf(std::string FileName, const Json::Value& ConfOverrides)
	{
		pPointConf.reset(new KafkaPointConf(FileName, ConfOverrides));
	}

	std::shared_ptr<KafkaPointConf> pPointConf;
	KafkaAddrConf mAddrConf;
};

#endif
