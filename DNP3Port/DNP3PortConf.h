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
 * DNP3OutstationPortConf.h
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3OUTSTATIONPORTCONF_H_
#define DNP3OUTSTATIONPORTCONF_H_

#include <opendatacon/DataPort.h>
#include <opendatacon/AddrConf.h>
#include "DNP3PointConf.h"

class DNP3PortConf: public DataPortConf
{
public:
	DNP3PortConf(std::string FileName)
	{
		pPointConf.reset(new DNP3PointConf(FileName));
	};

	std::unique_ptr<DNP3PointConf> pPointConf;
	DNP3AddrConf mAddrConf;
};

#endif /* DNP3OUTSTATIONPORTCONF_H_ */
