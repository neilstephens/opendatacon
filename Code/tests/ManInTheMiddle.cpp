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
 * ManInTheMiddle.cpp
 *
 *  Created on: 01/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "ManInTheMiddle.h"

ManInTheMiddle::ManInTheMiddle(MITMConfig conf, unsigned int port1, unsigned int port2, const std::string& a_log_name):
	log_name(a_log_name),
	SockMan1(odc::asio_service::Get(),
		(conf == MITMConfig::SERVER_CLIENT || conf == MITMConfig::SERVER_SERVER),
		"127.0.0.1",std::to_string(port1),
		[this](odc::buf_t& buf){ReadHandler(false,buf);},[this](bool s){StateHandler(true,s);},
		10,true,0,0,0,[a_log_name,port1](const std::string& level, const std::string& msg)
		{
			if(auto log = odc::spdlog_get(a_log_name))
			{
			      auto lvl = spdlog::level::from_str(level);
			      log->log(lvl,"ManInTheMiddle port {}: {}",port1,msg);
			}
		}),
	SockMan2(odc::asio_service::Get(),
		(conf == MITMConfig::CLIENT_SERVER || conf == MITMConfig::SERVER_SERVER),
		"127.0.0.1",std::to_string(port2),
		[this](odc::buf_t& buf){ReadHandler(true,buf);},[this](bool s){StateHandler(false,s);},
		10,true,0,0,0,[a_log_name,port2](const std::string& level, const std::string& msg)
		{
			if(auto log = odc::spdlog_get(a_log_name))
			{
			      auto lvl = spdlog::level::from_str(level);
			      log->log(lvl,"ManInTheMiddle port {}: {}",port2,msg);
			}
		})
{
	Up();
}

ManInTheMiddle::~ManInTheMiddle()
{
	Drop();
	Down();
}

void ManInTheMiddle::Up()
{
	SockMan1.Open();
	SockMan2.Open();
}
void ManInTheMiddle::Down()
{
	SockMan1.Close();
	SockMan2.Close();
}
void ManInTheMiddle::Drop()
{
	allow = false;
	if(auto log = odc::spdlog_get(log_name))
		log->debug("ManInTheMiddle Dropping data.");
}
void ManInTheMiddle::Allow()
{
	allow = true;
	if(auto log = odc::spdlog_get(log_name))
		log->debug("ManInTheMiddle Allowing data.");
}
unsigned int ManInTheMiddle::ConnectionCount(const bool direction)
{
	return direction ? Sock1Count : Sock2Count;
}
bool ManInTheMiddle::ConnectionState(const bool direction)
{
	return direction ? state1 : state2;
}

void ManInTheMiddle::ReadHandler(const bool direction, odc::buf_t& readbuf)
{
	if(!allow)
	{
		readbuf.consume(readbuf.size());
		return;
	}

	auto pSockMan = direction ? &SockMan1 : &SockMan2;

	std::vector<char> data_vec(readbuf.size());
	readbuf.sgetn(data_vec.data(),readbuf.size());
	pSockMan->Write(std::move(data_vec));
}
void ManInTheMiddle::StateHandler(const bool direction, const bool state)
{
	if(direction)
		state1 = state;
	else
		state2 = state;
	if(state)
	{
		if(direction)
			++Sock1Count;
		else
			++Sock2Count;
	}
}
