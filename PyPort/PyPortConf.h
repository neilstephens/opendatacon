/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 * PyPortConf.h
 *
 *  Created on: 26/03/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef PyPortCONF_H_
#define PyPortCONF_H_

#include <memory>
#include <string>
#include <opendatacon/DataPortConf.h>
#include <opendatacon/ConfigParser.h>

class PyPortConf: public DataPortConf
{
public:
	PyPortConf(std::string FileName, const Json::Value& ConfOverrides):
		pyModuleName("PyPortSim"),
		pyClassName("SimPortClass"),
		pyHTTPAddr("localhost"),
		pyHTTPPort("8000"),
		pyQueueFormatString("{{\"Tag\" : \"{0}\", \"Idx\" : {1}, \"Val\" : \"{4}\", \"Qual\" : \"{3}\", \"TS\" : \"{2}\"}}"),
		pyEventsAreQueued(false),
		pyOnlyQueueEventsWithTags(false),
		GlobalUseSystemPython(false)
	{}

	std::string pyModuleName;
	std::string pyClassName;
	std::string pyHTTPAddr;
	std::string pyHTTPPort;
	std::string pyQueueFormatString;
	bool pyEventsAreQueued;
	bool pyOnlyQueueEventsWithTags;
	bool GlobalUseSystemPython;

	//std::unique_ptr<PyPointConf> pPointConf;
};

#endif /* PyPortCONF_H_ */
