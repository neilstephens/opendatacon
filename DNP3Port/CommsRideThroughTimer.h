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
#ifndef COMMSRIDETHROUGHTIMER_H
#define COMMSRIDETHROUGHTIMER_H

#include <functional>
#include <opendatacon/asio.h>


class CommsRideThroughTimer
{
public:
	CommsRideThroughTimer(asio::io_service& ios,
		const uint32_t aTimeoutms,
		std::function<void()>&& aCommsGoodCB,
		std::function<void()>&& aCommsBadCB);
	void Trigger();
	void FastForward();
	void Cancel();

private:
	uint32_t Timeoutms;
	asio::io_service::strand TimerAccessStrand;
	bool RideThroughInProgress;
	asio::basic_waitable_timer<std::chrono::steady_clock> aCommsRideThroughTimer;
	const std::function<void()> CommsGoodCB;
	const std::function<void()> CommsBadCB;
};

#endif // COMMSRIDETHROUGHTIMER_H
