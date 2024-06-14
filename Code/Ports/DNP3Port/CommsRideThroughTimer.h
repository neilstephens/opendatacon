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
 *	ChannelStateSubscriber.h
 *
 *  Created on: 2018-11-20
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef COMMSRIDETHROUGHTIMER_H
#define COMMSRIDETHROUGHTIMER_H

#include <opendatacon/asio.h>
#include <functional>

class CommsRideThroughTimer: public std::enable_shared_from_this<CommsRideThroughTimer>
{
public:
	CommsRideThroughTimer(odc::asio_service& ios,
		const uint32_t aTimeoutms,
		std::function<void()>&& aCommsGoodCB,
		std::function<void()>&& aCommsBadCB,
		std::function<void(bool CommsIsBad)>&& aHeartBeatCB = [] (bool){},
		const uint32_t aHeartBeatTimems = 0);
	~CommsRideThroughTimer();
	void Trigger();
	void FastForward();
	void Cancel();

	inline void StartHeartBeat()
	{
		HeartBeat();
	}
	inline void StopHeartBeat()
	{
		pHeartBeatTimer->cancel();
	}

private:
	void HeartBeat();
	const uint32_t Timeoutms;
	std::unique_ptr<asio::io_service::strand> pTimerAccessStrand;
	bool RideThroughInProgress;
	bool CommsIsBad;
	std::unique_ptr<asio::steady_timer> pCommsRideThroughTimer;
	std::unique_ptr<asio::steady_timer> pHeartBeatTimer;
	const std::function<void()> CommsGoodCB;
	const std::function<void()> CommsBadCB;
	const std::function<void(bool CommsIsBad)> HeartBeatCB;
	const uint32_t HeartBeatTimems;
};

#endif // COMMSRIDETHROUGHTIMER_H
