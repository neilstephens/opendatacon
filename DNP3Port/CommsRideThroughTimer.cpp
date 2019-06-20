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
#include "CommsRideThroughTimer.h"

CommsRideThroughTimer::CommsRideThroughTimer(asio::io_service &ios,
	const uint32_t aTimeoutms,
	std::function<void()>&& aCommsGoodCB,
	std::function<void()>&& aCommsBadCB):
	busy(0),
	Timeoutms(aTimeoutms),
	TimerAccessStrand(ios),
	RideThroughInProgress(false),
	aCommsRideThroughTimer(ios),
	CommsGoodCB(aCommsGoodCB),
	CommsBadCB(aCommsBadCB)
{}

CommsRideThroughTimer::~CommsRideThroughTimer()
{
	aCommsRideThroughTimer.cancel();
	while(busy)
	{}
}

void CommsRideThroughTimer::Trigger()
{
	++busy;
	TimerAccessStrand.post([this]()
		{
			if(RideThroughInProgress)
			{
			      busy--;
			      return;
			}

			RideThroughInProgress = true;
			aCommsRideThroughTimer.expires_from_now(std::chrono::milliseconds(Timeoutms));
			aCommsRideThroughTimer.async_wait(TimerAccessStrand.wrap([this](asio::error_code err)
				{
					if(RideThroughInProgress)
						CommsBadCB();
					RideThroughInProgress = false;
					busy--;
				}));
		});
}

void CommsRideThroughTimer::FastForward()
{
	++busy;
	TimerAccessStrand.post([this]()
		{
			if(RideThroughInProgress)
				aCommsRideThroughTimer.cancel();
			RideThroughInProgress = false;
			CommsBadCB();
			busy--;
		});
}

void CommsRideThroughTimer::Cancel()
{
	++busy;
	TimerAccessStrand.post([this]()
		{
			if(RideThroughInProgress)
				aCommsRideThroughTimer.cancel();
			RideThroughInProgress = false;
			CommsGoodCB();
			busy--;
		});
}
