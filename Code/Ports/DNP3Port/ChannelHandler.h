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
 *	ChannelHandler.h
 *
 *  Created on: 2020-08-17
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef CHANNELHANDLER_H
#define CHANNELHANDLER_H

#include "ChannelLinksWatchdog.h"
#include <opendatacon/asio.h>
#include <opendnp3/gen/LinkStatus.h>
#include <opendnp3/gen/ChannelState.h>
#include <opendnp3/channel/IChannel.h>
#include <atomic>

enum class LinkDeadness : uint8_t
{
	LinkDownChannelDown,
	LinkDownChannelUp,
	LinkUpChannelUp
};

class DNP3Port;

class ChannelHandler
{
private:
	//Strand for synchronising channel/link state changes
	std::shared_ptr<odc::asio_service> pIOS;
	std::unique_ptr<asio::io_service::strand> pSyncStrand;
	std::shared_ptr<void> handler_tracker;

public:
	ChannelHandler() = delete;
	ChannelHandler(DNP3Port* p);
	~ChannelHandler();

	//Synchronised versions of their private couterparts
	inline void SetLinkStatus(opendnp3::LinkStatus status)
	{
		pSyncStrand->post([this,status,h{handler_tracker}](){SetLinkStatus_(status);});
	}
	inline void LinkUp()
	{
		pSyncStrand->post([this,h{handler_tracker}](){LinkUp_();});
	}
	inline void LinkDown()
	{
		pSyncStrand->post([this,h{handler_tracker}](){LinkDown_();});
	}
	inline void StateListener(opendnp3::ChannelState state)
	{
		pSyncStrand->post([this,state,h{handler_tracker}](){StateListener_(state);});
	}

	//Facility to post actions that need to be sync'd with the link state
	template<typename T>
	inline void Post(T&& handler)
	{
		pSyncStrand->post([handler,h{handler_tracker}](){handler();});
	}

	//Factory function for the Channel
	std::shared_ptr<opendnp3::IChannel> SetChannel();
	//inline Getters, to ensure readonly atomic access outside synchronisation
	inline opendnp3::LinkStatus GetLinkStatus() const
	{
		return link_status.load();
	}
	inline LinkDeadness GetLinkDeadness() const
	{
		return link_deadness.load();
	}
	inline std::shared_ptr<opendnp3::IChannel> GetChannel() const
	{
		return pChannel;
	}

private:
	//unsynchronised state logic functions
	void StateListener_(opendnp3::ChannelState state);
	void SetLinkStatus_(opendnp3::LinkStatus status);
	void LinkDown_();
	void LinkUp_();

	//data members
	DNP3Port* const pPort;
	std::shared_ptr<opendnp3::IChannel> pChannel;
	std::shared_ptr<ChannelLinksWatchdog> pWatchdog;
	//use atomic for status data - so the getters can read them concurrently
	std::atomic<opendnp3::LinkStatus> link_status;
	std::atomic<LinkDeadness> link_deadness;
};

#endif // CHANNELHANDLER_H
