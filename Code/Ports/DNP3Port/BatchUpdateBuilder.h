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

class BatchUpdateBuilder: public std::enable_shared_from_this<BatchUpdateBuilder>
{
private:
	const std::weak_ptr<opendnp3::IOutstation> wOutstation;
	const size_t maxBatchPeriodms;
	const size_t maxBatchCount;
	std::shared_ptr<asio::io_service::strand> pSyncStrand;
	odc::msSinceEpoch_t batchStartTime;
	size_t batchCount;
	std::shared_ptr<opendnp3::UpdateBuilder> pBuilder;

public:
	BatchUpdateBuilder(const std::weak_ptr<opendnp3::IOutstation> aOutstation, const size_t amaxBatchPeriodms, const size_t amaxBatchCount):
		wOutstation(aOutstation),
		maxBatchPeriodms(amaxBatchPeriodms),
		maxBatchCount(amaxBatchCount),
		pSyncStrand(odc::asio_service::Get()->make_strand()),
		batchStartTime(odc::msSinceEpoch()),
		batchCount(0),
		pBuilder(std::make_shared<opendnp3::UpdateBuilder>())
	{}

	template<typename T>
	inline void Event(T meas, uint16_t index, opendnp3::EventMode mode)
	{
		auto weak_self = weak_from_this();
		pSyncStrand->post([weak_self,meas,index,mode]()
			{
				auto self = weak_self.lock();
				if(!self) return;

				auto os = self->wOutstation.lock();
				if(!os) return;

				self->pBuilder->Update(meas, index, mode);
				self->batchCount++;

				auto now = odc::msSinceEpoch();
				if(self->batchStartTime + self->maxBatchPeriodms <= now
				   || self->batchCount > self->maxBatchCount)
				{
					os->Apply(self->pBuilder->Build());
					self->pBuilder.reset();
					self->pBuilder = std::make_shared<opendnp3::UpdateBuilder>();
					self->batchCount = 0;
					self->batchStartTime = odc::msSinceEpoch();
				}
			});
	}
};

#endif // BATCHUPDATEBUILDER_H
