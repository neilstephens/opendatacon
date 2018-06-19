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
 * DNP3Log2spdlog.h
 *
 *  Created on: 2018-06-19
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#ifndef DNP3Log2spdlog_H
#define DNP3Log2spdlog_H

#include <openpal/logging/ILogHandler.h>
#include <chrono>
#include <opendatacon/TCPSocketManager.h>
#include <spdlog/spdlog.h>

class DNP3Log2spdlog: public openpal::ILogHandler
{
public:
	DNP3Log2spdlog();
	void Log( const openpal::LogEntry& arEntry ) override;
private:
	std::string FilterToString(const openpal::LogFilters& filters);
	spdlog::level::level_enum FilterToLevel(const openpal::LogFilters& filters);
};

#endif // DNP3Log2spdlog_H
