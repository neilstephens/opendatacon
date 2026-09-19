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
 * UDPMITM.cpp
 *
 *  Created on: 27/05/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "UDPMITM.h"
#include <spdlog/spdlog.h>

UDPMITM::UDPMITM(uint16_t mitm_port_os, uint16_t mitm_port_ms,
	uint16_t os_actual, uint16_t ms_actual,
	const std::string& a_log_name):
	log_name(a_log_name),
	local_ep_os(asio::ip::address_v4::loopback(), mitm_port_os),
	remote_ep_os(asio::ip::address_v4::loopback(), os_actual),
	local_ep_ms(asio::ip::address_v4::loopback(), mitm_port_ms),
	remote_ep_ms(asio::ip::address_v4::loopback(), ms_actual),
	ios(odc::asio_service::Get()),
	pStrand(ios->make_strand()),
	sock_os(ios->make_udp_socket()),
	sock_ms(ios->make_udp_socket()),
	readbuf_os(65536),
	readbuf_ms(65536)
{
	sock_os->open(asio::ip::udp::v4());
	sock_ms->open(asio::ip::udp::v4());

	sock_os->bind(local_ep_os);
	sock_ms->bind(local_ep_ms);

	auto log = spdlog::get(log_name);
	if(log)
	{
		log->debug("[UDPMITM] Bound OS_side to {}:{} and MS_side to {}:{}",
			local_ep_os.address().to_string(), local_ep_os.port(),
			local_ep_ms.address().to_string(), local_ep_ms.port());
		log->debug("[UDPMITM] Forwarding OS->MS to {}:{} and MS->OS to {}:{}",
			remote_ep_ms.address().to_string(), remote_ep_ms.port(),
			remote_ep_os.address().to_string(), remote_ep_os.port());
	}
}

std::shared_ptr<UDPMITM> UDPMITM::create(
	uint16_t mitm_port_os, uint16_t mitm_port_ms,
	uint16_t os_actual, uint16_t ms_actual,
	const std::string& a_log_name)
{
	auto self = std::shared_ptr<UDPMITM>(new UDPMITM(mitm_port_os, mitm_port_ms, os_actual, ms_actual, a_log_name));
	self->StartRead(true);  // OS→MS direction
	self->StartRead(false); // MS→OS direction
	return self;
}

UDPMITM::~UDPMITM()
{
	//Cancel/close on the strand, taking the sockets and their in-flight
	//receive buffers with it, so this can return without waiting for it
	pStrand->post(
		[sock_os = std::move(sock_os), sock_ms = std::move(sock_ms),
		 readbuf_os = std::move(readbuf_os), readbuf_ms = std::move(readbuf_ms)]()
		{
			asio::error_code ec;
			sock_os->cancel(ec);
			sock_ms->cancel(ec);
			sock_os->close(ec);
			sock_ms->close(ec);
		});
}

void UDPMITM::Up()
{
	pStrand->post([weak = weak_from_this()]()
		{
			auto self = weak.lock();
			if(!self)
				return;

			auto log = spdlog::get(self->log_name);
			if(log)
				log->debug("[UDPMITM] Up() — rebinding sockets");

			//Never let an exception escape a strand-posted handler
			try
			{
				self->sock_os = self->ios->make_udp_socket();
				self->sock_os->open(asio::ip::udp::v4());
				self->sock_os->bind(self->local_ep_os);
				self->sock_ms = self->ios->make_udp_socket();
				self->sock_ms->open(asio::ip::udp::v4());
				self->sock_ms->bind(self->local_ep_ms);
			}
			catch(const std::exception& e)
			{
				if(log)
					log->error("[UDPMITM] Up() failed to rebind: {}", e.what());
				return;
			}

			self->allow = true;
			self->StartRead(true);
			self->StartRead(false);
		});
}

void UDPMITM::Down()
{
	pStrand->post([weak = weak_from_this()]()
		{
			auto self = weak.lock();
			if(!self)
				return;

			auto log = spdlog::get(self->log_name);
			if(log)
				log->debug("[UDPMITM] Down() — closing sockets");

			self->allow = false;
			asio::error_code ec;
			self->sock_os->close(ec);
			self->sock_ms->close(ec);
		});
}

void UDPMITM::Drop()
{
	allow = false;
}

void UDPMITM::Allow()
{
	allow = true;
}

void UDPMITM::StartRead(const bool dir)
{
	//Always called on pStrand
	std::weak_ptr<UDPMITM> weak = weak_from_this();
	if(dir)
		sock_os->async_receive(asio::buffer(readbuf_os),
			pStrand->wrap([weak, dir](std::error_code ec, size_t num)
				{
					if(auto self = weak.lock())
						self->ReadHandler(dir, ec, num);
				}));
	else
		sock_ms->async_receive(asio::buffer(readbuf_ms),
			pStrand->wrap([weak, dir](std::error_code ec, size_t num)
				{
					if(auto self = weak.lock())
						self->ReadHandler(dir, ec, num);
				}));
}

void UDPMITM::ReadHandler(const bool dir, std::error_code ec, size_t num)
{
	if(ec)
	{
		if(ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor)
			return;
		StartRead(dir);
		return;
	}

	if(allow)
	{
		if(dir)
		{
			sock_ms->async_send_to(asio::buffer(readbuf_os.data(), num), remote_ep_ms,
				[](std::error_code, size_t) {});
		}
		else
		{
			sock_os->async_send_to(asio::buffer(readbuf_ms.data(), num), remote_ep_os,
				[](std::error_code, size_t) {});
		}
	}

	StartRead(dir);
}
