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

CommsRideThroughTimer::CommsRideThroughTimer(odc::asio_service &ios,
	const uint32_t aTimeoutms,
	std::function<void()>&& aCommsGoodCB,
	std::function<void()>&& aCommsBadCB):
	Timeoutms(aTimeoutms),
	pTimerAccessStrand(ios.make_strand()),
	RideThroughInProgress(false),
	pCommsRideThroughTimer(ios.make_steady_timer()),
	CommsGoodCB(aCommsGoodCB),
	CommsBadCB(aCommsBadCB)
{}

CommsRideThroughTimer::~CommsRideThroughTimer()
{
	pCommsRideThroughTimer->cancel();
}

void CommsRideThroughTimer::Trigger()
{
	std::weak_ptr<CommsRideThroughTimer> weak_self = shared_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			if(self->RideThroughInProgress)
				return;

			self->RideThroughInProgress = true;
			self->pCommsRideThroughTimer->expires_from_now(std::chrono::milliseconds(self->Timeoutms));
			self->pCommsRideThroughTimer->async_wait(self->pTimerAccessStrand->wrap([weak_self](asio::error_code err)
					{
						auto self = weak_self.lock();
						if(!self)
							return;
						if(self->RideThroughInProgress)
							self->CommsBadCB();
						self->RideThroughInProgress = false;
					}));
		});
}

void CommsRideThroughTimer::FastForward()
{
	std::weak_ptr<CommsRideThroughTimer> weak_self = shared_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;
			if(self->RideThroughInProgress)
				self->pCommsRideThroughTimer->cancel();
			self->RideThroughInProgress = false;
			self->CommsBadCB();
		});
}

void CommsRideThroughTimer::Cancel()
{
	std::weak_ptr<CommsRideThroughTimer> weak_self = shared_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;
			if(self->RideThroughInProgress)
				self->pCommsRideThroughTimer->cancel();
			self->RideThroughInProgress = false;
			self->CommsGoodCB();
		});
}
