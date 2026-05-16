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
 * BatchUpdateBuilder.h
 *
 *  Created on: 11/05/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef BATCHUPDATEBUILDER_H
#define BATCHUPDATEBUILDER_H

#include <opendnp3/outstation/IOutstation.h>
#include <opendnp3/outstation/UpdateBuilder.h>
#include <opendatacon/asio.h>
#include <opendatacon/util.h>
#include <memory>
#include <algorithm>
#include <chrono>

/*  Dynamic batching with Exponential Moving Average (EMA) based rate tracking
 *  --------------------------------------------------------------------------
 *  The batch window grows proportionally to the smoothed message arrival rate,
 *  so high sustained rates produce fewer, larger packets,
 *  while low rates shrink the window (minimises latency).
 *
 *  Separate rise/fall EMA weights to allow asymetric window response
 */

class BatchUpdateBuilder: public std::enable_shared_from_this<BatchUpdateBuilder>
{
private:
	const std::weak_ptr<opendnp3::IOutstation> wOutstation;

	// Hard limits
	const size_t maxBatchPeriodms;
	const size_t maxBatchCount;

	// EMA parameters
	const double riseWeight;
	const double fallWeight;

	std::shared_ptr<asio::io_service::strand> pSyncStrand;
	std::shared_ptr<asio::steady_timer> pFlushTimer;
	std::shared_ptr<opendnp3::UpdateBuilder> pBuilder;

	// State
	double smoothedArrivalRate;
	std::chrono::time_point<std::chrono::steady_clock> lastArrivalTime;
	size_t batchCount;
	size_t flushSeq;

	void Flush()
	{
		auto os = wOutstation.lock();
		if(!os) return;
		os->Apply(pBuilder->Build());
		pBuilder = std::make_shared<opendnp3::UpdateBuilder>();
		batchCount = 0;
	}

	size_t ComputeBatchPeriodms() const
	{
		auto periodms = static_cast<size_t>(smoothedArrivalRate/maxBatchCount*maxBatchPeriodms);
		return std::min(periodms, maxBatchPeriodms);
	}

public:
	BatchUpdateBuilder(
		const std::weak_ptr<opendnp3::IOutstation> aOutstation,
		const size_t amaxBatchPeriodms,
		const size_t amaxBatchCount,
		const double ariseWeight,
		const double afallWeight
		):
		wOutstation(aOutstation),
		maxBatchPeriodms(amaxBatchPeriodms),
		maxBatchCount(amaxBatchCount),
		riseWeight(ariseWeight),
		fallWeight(afallWeight),
		pSyncStrand(odc::asio_service::Get()->make_strand()),
		pFlushTimer(odc::asio_service::Get()->make_steady_timer()),
		pBuilder(std::make_shared<opendnp3::UpdateBuilder>()),
		smoothedArrivalRate(0.0),
		lastArrivalTime(std::chrono::steady_clock::now()),
		batchCount(0),
		flushSeq(0)
	{}

	~BatchUpdateBuilder()
	{
		pFlushTimer->cancel();
	}

	template<typename T>
	inline void Event(T meas, uint16_t index, opendnp3::EventMode mode)
	{
		static constexpr auto tick_s = std::chrono::duration<double>(std::chrono::steady_clock::duration(1)).count();
		auto weak_self = weak_from_this();
		pSyncStrand->post([weak_self,meas,index,mode]()
			{
				auto self = weak_self.lock();
				if(!self) return;

				auto os = self->wOutstation.lock();
				if(!os) return;

				self->pBuilder->Update(meas, index, mode);
				self->batchCount++;

				// --- Update EMA rate estimate ---
				const auto now = std::chrono::steady_clock::now();
				const auto dt_s = std::chrono::duration<double>(now - self->lastArrivalTime).count();
				const auto instantRate = 1.0 / (dt_s > 0.0 ? dt_s : tick_s);
				self->lastArrivalTime = now;

				const double weight = (instantRate > self->smoothedArrivalRate)
				                     ? self->riseWeight
				                     : self->fallWeight;
				self->smoothedArrivalRate = weight * instantRate + (1.0 - weight) * self->smoothedArrivalRate;

				if(self->batchCount >= self->maxBatchCount)
				{
					self->flushSeq++;
					self->pFlushTimer->cancel();
					self->Flush();
				}
				else if(self->batchCount == 1)
				{
					self->pFlushTimer->expires_from_now(std::chrono::milliseconds(self->ComputeBatchPeriodms()));
					self->pFlushTimer->async_wait(self->pSyncStrand->wrap(
						[weak_self,seq{self->flushSeq}](const asio::error_code& ec)
						{
							auto self = weak_self.lock();
							if(!self) return;
							if(seq == self->flushSeq)
								self->Flush();
						}));
				}
				// else: timer already running for this batch
			});
	}
};

#endif // BATCHUPDATEBUILDER_H
