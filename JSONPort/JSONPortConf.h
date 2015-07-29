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
 * JSONPortConf.h
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONPORTCONF_H_
#define JSONPORTCONF_H_

#include <memory>
#include <opendatacon/DataPortConf.h>
#include "JSONPointConf.h"

typedef struct
{
	std::string IP;
	uint16_t Port;
}JSONAddrConf;

class JSONPortConf: public DataPortConf
{
public:
	JSONPortConf(std::string FileName):
		retry_time_ms(20000)
	{
		pPointConf.reset(new JSONPointConf(FileName));
	};

	std::unique_ptr<JSONPointConf> pPointConf;
	JSONAddrConf mAddrConf;
	unsigned int retry_time_ms;
};

#endif /* JSONPORTCONF_H_ */
