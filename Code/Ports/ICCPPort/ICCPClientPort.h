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
 * ICCPClientPort.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The ICCP port that CONNECTS the TCP connection (and so initiates the association). Its configuration schema and its TASE.2
 *  behaviour are those of every ICCP port (see ICCPPortBase and the README); only the TCP direction is fixed here. The default
 *  ServiceRole is "Client".
 */

#ifndef ICCPCLIENTPORT_H_
#define ICCPCLIENTPORT_H_

#include "ICCPPortBase.h"

class ICCPClientPort: public ICCPPortBase
{
public:
	ICCPClientPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		ICCPPortBase(aName, aConfFilename, aConfOverrides, false)
	{}
};

#endif /* ICCPCLIENTPORT_H_ */
