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
 * UDPMITM.h
 *
 *  Created on: 27/05/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef UDPMITM_H
#define UDPMITM_H

#include "MITM.h"
#include <opendatacon/asio.h>
#include <atomic>
#include <memory>
#include <string>

class UDPMITM: public MITM, public std::enable_shared_from_this<UDPMITM>
{
public:
	UDPMITM() = delete;
	UDPMITM(const UDPMITM&) = delete;
	~UDPMITM();

	static std::shared_ptr<UDPMITM> create(
		uint16_t mitm_port_os, uint16_t mitm_port_ms,
		uint16_t os_actual, uint16_t ms_actual,
		const std::string& a_log_name = "opendatacon");

	void Up();
	void Down();
	void Drop();
	void Allow();

private:
	UDPMITM(uint16_t mitm_port_os, uint16_t mitm_port_ms,
		uint16_t os_actual, uint16_t ms_actual,
		const std::string& a_log_name);

	void StartRead(const bool dir);
	void ReadHandler(const bool dir, std::error_code ec, size_t num);

	std::atomic_bool allow = true;
	const std::string log_name;

	asio::ip::udp::endpoint local_ep_os;
	asio::ip::udp::endpoint remote_ep_os;
	asio::ip::udp::endpoint local_ep_ms;
	asio::ip::udp::endpoint remote_ep_ms;

	std::shared_ptr<odc::asio_service> ios;

	std::unique_ptr<asio::ip::udp::socket, odc::deleter> sock_os;
	std::unique_ptr<asio::ip::udp::socket, odc::deleter> sock_ms;

	std::vector<char> readbuf_os;
	std::vector<char> readbuf_ms;
};

#endif // UDPMITM_H
