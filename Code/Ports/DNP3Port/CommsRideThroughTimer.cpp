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
	std::function<void ()>&& aStaleCB,
	const uint32_t aStaleTimeoutms,
	std::function<void(bool)>&& aHeartBeatCB,
	const uint32_t aHeartBeatTimems):
	Timeoutms(aTimeoutms),
	pTimerAccessStrand(ios.make_strand()),
	RideThroughInProgress(false),
	Comms(CommsState::NUL),
	Paused(false),
	PendingTrigger(false),
	TimerHandlerSequence(0),
	StaleTimerSequence(0),
	ExpiryTime(0),
	msRemaining(Timeoutms),
	pCommsRideThroughTimer(ios.make_steady_timer()),
	pHeartBeatTimer(ios.make_steady_timer()),
	pStaleTimer(ios.make_steady_timer()),
	CommsGoodCB(aCommsGoodCB),
	CommsBadCB(aCommsBadCB),
	StaleCB(aStaleCB),
	StaleTimems(aStaleTimeoutms),
	HeartBeatCB(aHeartBeatCB),
	HeartBeatTimems(aHeartBeatTimems)
{}

CommsRideThroughTimer::~CommsRideThroughTimer()
{
	pCommsRideThroughTimer->cancel();
	pHeartBeatTimer->cancel();
	pStaleTimer->cancel();
}

//Only called from on the strand in Trigger and Resume
//	that's why it doesn't have to post
void CommsRideThroughTimer::StartTimer()
{
	RideThroughInProgress = true;
	ExpiryTime = odc::msSinceEpoch()+msRemaining;
	pCommsRideThroughTimer->expires_from_now(std::chrono::milliseconds(msRemaining));
	pCommsRideThroughTimer->async_wait(pTimerAccessStrand->wrap([weak_self{weak_from_this()},seq{++TimerHandlerSequence}](asio::error_code err)
		{
			auto self = weak_self.lock();
			if(!self || seq != self->TimerHandlerSequence)
				return;

			if(self->RideThroughInProgress)
			{
				self->CommsBadCB();
				self->Comms = CommsState::BAD;
				self->msRemaining = self->Timeoutms;
			}
			self->RideThroughInProgress = false;
			self->ExpiryTime = 0;
		}));
}

//Only called from on the strand in Pause
//	that's why it doesn't have to post
void CommsRideThroughTimer::StartStaleTimer()
{
	if(StaleTimems == 0)
		return;
	pStaleTimer->expires_from_now(std::chrono::milliseconds(StaleTimems));
	pStaleTimer->async_wait(pTimerAccessStrand->wrap([weak_self{weak_from_this()},seq{++StaleTimerSequence}](asio::error_code err)
		{
			auto self = weak_self.lock();
			if(!self || seq != self->StaleTimerSequence || !self->Paused)
				return;

			self->StaleCB();
		}));
}

void CommsRideThroughTimer::Trigger()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			if(self->RideThroughInProgress || self->Comms == CommsState::BAD)
				return;

			if(self->Paused)
			{
				self->PendingTrigger = true;
				return;
			}

			self->StartTimer();
		});
}

void CommsRideThroughTimer::Pause()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			if(self->Paused)
				return;

			self->Paused = true;
			self->StartStaleTimer();

			if(self->RideThroughInProgress)
			{
				auto now = odc::msSinceEpoch();
				self->msRemaining = (self->ExpiryTime > now) ? (self->ExpiryTime - now) : 0;

				self->PendingTrigger = true;
				self->RideThroughInProgress = false;
				self->ExpiryTime = 0;
				//cancel the timer and bump the sequence number so the current handler does nothing
				self->TimerHandlerSequence++;
				self->pCommsRideThroughTimer->cancel();
			}
		});
}

void CommsRideThroughTimer::Resume()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			if(!self->Paused)
				return;

			self->Paused = false;
			//cancel the stale timer and bump the sequence number so the current handler does nothing
			self->StaleTimerSequence++;
			self->pStaleTimer->cancel();

			if(self->PendingTrigger)
			{
				self->PendingTrigger = false;
				self->StartTimer();
			}
		});
}

void CommsRideThroughTimer::FastForward()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;
			if(self->Paused && self->PendingTrigger)
				self->msRemaining = 0;
			if(self->RideThroughInProgress)
			{
				self->ExpiryTime = odc::msSinceEpoch();
				self->pCommsRideThroughTimer->cancel();
			}
		});
}

void CommsRideThroughTimer::Cancel()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			if(self->RideThroughInProgress)
			{
				self->RideThroughInProgress = false;
				self->ExpiryTime = 0;
				//cancel the timer and bump the sequence number so the current handler does nothing
				self->TimerHandlerSequence++;
				self->pCommsRideThroughTimer->cancel();
			}

			if(self->Comms != CommsState::GOOD)
			{
				self->CommsGoodCB();
				self->Comms = CommsState::GOOD;
			}

			self->PendingTrigger = false;
			self->msRemaining = self->Timeoutms;
		});
}

void CommsRideThroughTimer::StartHeartBeat()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			self->HeartBeatStopped = false;
			self->HeartBeat();
		});
}

void CommsRideThroughTimer::StopHeartBeat()
{
	auto weak_self = weak_from_this();
	pTimerAccessStrand->post([weak_self]()
		{
			auto self = weak_self.lock();
			if(!self)
				return;

			self->HeartBeatStopped = true;
			self->pHeartBeatTimer->cancel();
		});
}

void CommsRideThroughTimer::HeartBeat()
{
	auto weak_self = weak_from_this();
	pHeartBeatTimer->expires_from_now(std::chrono::milliseconds(HeartBeatTimems));
	pHeartBeatTimer->async_wait(pTimerAccessStrand->wrap([weak_self](asio::error_code err)
		{
			auto self = weak_self.lock();
			if(!self || self->HeartBeatTimems == 0 || self->HeartBeatStopped)
				return;
			if(!self->Paused && self->Comms != CommsState::NUL)
				self->HeartBeatCB(self->Comms == CommsState::BAD);
			self->HeartBeat();
		}));
}
