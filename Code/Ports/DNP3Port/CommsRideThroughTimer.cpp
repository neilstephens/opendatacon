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
	std::function<void()>&& aCommsBadCB,
	std::function<void(bool)>&& aHeartBeatCB,
	const uint32_t aHeartBeatTimems):
	Timeoutms(aTimeoutms),
	pTimerAccessStrand(ios.make_strand()),
	RideThroughInProgress(false),
	CommsIsBad(false),
	pCommsRideThroughTimer(ios.make_steady_timer()),
	pHeartBeatTimer(ios.make_steady_timer()),
	CommsGoodCB(aCommsGoodCB),
	CommsBadCB(aCommsBadCB),
	HeartBeatCB(aHeartBeatCB),
	HeartBeatTimems(aHeartBeatTimems)
{}

CommsRideThroughTimer::~CommsRideThroughTimer()
{
	pCommsRideThroughTimer->cancel();
	pHeartBeatTimer->cancel();
}

void CommsRideThroughTimer::HeartBeat()
{
	std::weak_ptr<CommsRideThroughTimer> weak_self = shared_from_this();
	pHeartBeatTimer->expires_from_now(std::chrono::milliseconds(HeartBeatTimems));
	pHeartBeatTimer->async_wait(pTimerAccessStrand->wrap([weak_self](asio::error_code err)
		{
			if(err)
				return;
			auto self = weak_self.lock();
			if(!self || self->HeartBeatTimems == 0)
				return;
			self->HeartBeatCB(self->CommsIsBad);
			self->HeartBeat();
		}));
}

void CommsRideThroughTimer::ReassertCommsState()
{
	std::weak_ptr<CommsRideThroughTimer> weak_self = shared_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;
			if(self->CommsIsBad)
				self->CommsBadCB();
			else
				self->CommsGoodCB();
		});
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
					{
					      self->CommsBadCB();
					      self->CommsIsBad = true;
					}
					self->RideThroughInProgress = false;
				}));

			//if comms is already bad - don't wait
			if(self->CommsIsBad)
				self->FastForward();
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
			{
			      self->RideThroughInProgress = false;
			      self->pCommsRideThroughTimer->cancel();
			}
			else
			{
			      self->CommsGoodCB();
			      self->CommsIsBad = false;
			}
		});
}
