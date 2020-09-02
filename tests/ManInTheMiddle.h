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
 * ManInTheMiddle.h
 *
 *  Created on: 01/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef MANINTHEMIDDLE_H
#define MANINTHEMIDDLE_H

#include <opendatacon/TCPSocketManager.h>
#include <atomic>
#include <vector>

enum class MITMConfig
{
	CLIENT_SERVER,
	SERVER_CLIENT,
	CLIENT_CLIENT,
	SERVER_SERVER
};
inline std::string to_string(const MITMConfig MC)
{
	switch(MC)
	{
		case MITMConfig::CLIENT_SERVER:
			return "CLIENT_SERVER";
		case MITMConfig::SERVER_CLIENT:
			return "SERVER_CLIENT";
		case MITMConfig::CLIENT_CLIENT:
			return "CLIENT_CLIENT";
		case MITMConfig::SERVER_SERVER:
			return "SERVER_SERVER";
	}
	return "UNKNOWN";
}

class ManInTheMiddle
{
public:
	ManInTheMiddle() = delete;
	ManInTheMiddle(const ManInTheMiddle&) = delete;
	ManInTheMiddle(MITMConfig conf, unsigned int port1, unsigned int port2);
	~ManInTheMiddle();
	void Up();
	void Down();
	void Drop();
	void Allow();
	unsigned int ConnectionCount(const bool direction);
	bool ConnectionState(const bool direction);
private:
	void ReadHandler(const bool direction, odc::buf_t& readbuf);
	void StateHandler(const bool direction, const bool state);
	std::atomic_bool up = true;
	std::atomic_bool allow = true;
	std::atomic_bool state1 = false;
	std::atomic_bool state2 = false;
	std::atomic<unsigned int> Sock1Count = 0;
	std::atomic<unsigned int> Sock2Count = 0;
	odc::TCPSocketManager<std::vector<char>> SockMan1;
	odc::TCPSocketManager<std::vector<char>> SockMan2;
};

#endif // MANINTHEMIDDLE_H
