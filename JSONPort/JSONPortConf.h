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
#include "JSONPointConf.h"
#include <memory>
#include <opendatacon/DataPortConf.h>

struct JSONAddrConf
{
	std::string IP = "127.0.0.1";
	uint16_t Port = 2598;
};

class JSONPortConf: public DataPortConf
{
public:
	JSONPortConf(const std::string& FileName, const Json::Value& ConfOverrides):
		retry_time_ms(3000),
		evt_buffer_size(1000),
		style_output(false),
		print_all(false)
	{
		pPointConf = std::make_unique<JSONPointConf>(FileName, ConfOverrides);
	}

	std::unique_ptr<JSONPointConf> pPointConf;
	JSONAddrConf mAddrConf;
	unsigned int retry_time_ms;
	unsigned int evt_buffer_size;
	bool style_output;
	bool print_all;
};

#endif /* JSONPORTCONF_H_ */
