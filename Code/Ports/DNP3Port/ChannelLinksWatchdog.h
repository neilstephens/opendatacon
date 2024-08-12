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
 *	ChannelLinksWatchdog.h
 *
 *  Created on: 2020-08-17
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef CHANNELLINKSWATCHDOG_H
#define CHANNELLINKSWATCHDOG_H

#include "DNP3PortConf.h"
#include <opendatacon/asio.h>
#include <set>
#include <memory>

namespace odc
{
class DataPort;
}

class ChannelLinksWatchdog: public std::enable_shared_from_this<ChannelLinksWatchdog>
{
private:
	//Strand for synchronising access to sets
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unique_ptr<asio::io_service::strand> pSyncStrand = pIOS->make_strand();

public:
	ChannelLinksWatchdog(const WatchdogBark& mode);

	//Synchronised versions of their private couterparts
	void LinkUp(std::weak_ptr<odc::DataPort> pPort)
	{
		pSyncStrand->post([this,guard{shared_from_this()},pPort]()
			{
				LinkUp_(pPort);
			});
	}
	void LinkDown(std::weak_ptr<odc::DataPort> pPort)
	{
		pSyncStrand->post([this,guard{shared_from_this()},pPort]()
			{
				LinkDown_(pPort);
			});
	}

private:
	//these access sets, so they're only called by sync'd public counterparts
	void LinkUp_(std::weak_ptr<odc::DataPort> pPort);
	void LinkDown_(std::weak_ptr<odc::DataPort> pPort);
	void Bark();

	const WatchdogBark mode;
	std::set<std::weak_ptr<odc::DataPort>,std::owner_less<std::weak_ptr<odc::DataPort>>> UpSet;
	std::set<std::weak_ptr<odc::DataPort>,std::owner_less<std::weak_ptr<odc::DataPort>>> DownSet;
};

#endif // CHANNELLINKSWATCHDOG_H
