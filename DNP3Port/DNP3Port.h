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
 * DNP3Port.h
 *
 *  Created on: 18/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3PORT_H_
#define DNP3PORT_H_
#include "DNP3PortConf.h"
#include "DNP3Log2spdlog.h"
#include <unordered_map>
#include <opendatacon/DataPort.h>
#include <opendnp3/gen/LinkStatus.h>
#include <opendnp3/gen/ChannelState.h>
#include <asiodnp3/DNP3Manager.h>

using namespace odc;

class DNP3Port: public DataPort
{
public:
	DNP3Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~DNP3Port() override;

	void Enable() override =0;
	void Disable() override =0;
	void Build() override =0;

	void StateListener(opendnp3::ChannelState state);

	//Override DataPort for UI
	const Json::Value GetStatus() const override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

	void ProcessElements(const Json::Value& JSONRoot) override;

protected:
	std::shared_ptr<asiodnp3::DNP3Manager> IOMgr;
	std::shared_ptr<asiodnp3::IChannel> GetChannel();

	std::shared_ptr<asiodnp3::IChannel> pChannel;
	opendnp3::LinkStatus status;
	bool link_dead;
	bool channel_dead;

	virtual void OnLinkDown() = 0;
	virtual TCPClientServer ClientOrServer() = 0;

};

#endif /* DNP3PORT_H_ */
