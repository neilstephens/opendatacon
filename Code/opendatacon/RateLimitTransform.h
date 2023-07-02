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
 * RateLimitTransform.h
 *
 *  Created on: 06/03/2015
 *      Author: Alan Murray
 */

#ifndef RATELIMITTRANSFORM_H_
#define RATELIMITTRANSFORM_H_

#include <atomic>
#include <opendatacon/Transform.h>
#include <unordered_map>
#include <opendatacon/util.h>

using namespace odc;

class RateLimitTransform: public Transform
{
public:
	RateLimitTransform(const std::string& Name, const Json::Value& params): Transform(Name,params)
	{
		std::string name = "DEFAULT";
		if (params.isMember("Name") && params["Name"].isString())
		{
			name = params["Name"].asString();
		}
		if (rateStatsCollection.count(name))
		{
			rateStats = &rateStatsCollection[name];
		}
		else
		{
			rateStatsCollection.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple());
			rateStats = &rateStatsCollection[name];
			rateStats->droppedUpdates = 0;
			rateStats->inputRate = 0;
			rateStats->outputRate = 0;

			if (params.isMember("outputRateLimit") && params["outputRateLimit"].isUInt())
			{
				rateStats->outputRateLimit = params["outputRateLimit"].asUInt();
			}
			else
				rateStats->outputRateLimit = 10;
			if (params.isMember("updatePeriodms") && params["updatePeriodms"].isUInt())
			{
				rateStats->updatePeriodms = params["updatePeriodms"].asUInt();
			}
			else
				rateStats->updatePeriodms = 100;
			if (params.isMember("updatePeriodMultiplier") && params["updatePeriodMultiplier"].isUInt())
			{
				rateStats->updatePeriodMultiplier = params["updatePeriodMultiplier"].asUInt();
			}
			else
				rateStats->updatePeriodMultiplier = 10;

			rateStats->nextUpdatems = msSinceEpoch() + rateStats->updatePeriodms;
		}
	}

private:
	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		switch(event->GetEventType())
		{
			case EventType::Binary:
			case EventType::Analog:
			case EventType::DoubleBitBinary:
			case EventType::Counter:
			case EventType::FrozenCounter:
			case EventType::BinaryOutputStatus:
			case EventType::AnalogOutputStatus:
				break;
			default:
				return (*pAllow)(event);
		}
		// check if rollover of update count period
		auto eventTime = msSinceEpoch();

		// see if we need to subtract updates from the update counter
		// as this section isn't atomic this might overshoot and subtract too many updates from the counter
		// however, this will smooth out and self correct over time
		if (rateStats->nextUpdatems < eventTime)
		{
			// update nextUpdatems as soon as possible to reduce the likelihood of update overshoot
			rateStats->nextUpdatems += rateStats->updatePeriodms;
			// reduce update count
			rateStats->outputRate -= rateStats->outputRateLimit;
			// if we reduce the outputRate to below 0, reset to 0 and set nextUpdatems to in the future
			if (rateStats->outputRate < 0)
			{
				rateStats->nextUpdatems = eventTime + rateStats->updatePeriodms;
				rateStats->outputRate = 0;
			}
			rateStats->inputRate = 0;
		}
		++rateStats->inputRate;

		if (rateStats->outputRate < rateStats->outputRateLimit * rateStats->updatePeriodMultiplier)
		{
			++rateStats->outputRate;
			return (*pAllow)(event);
		}
		else
		{
			++rateStats->droppedUpdates;
			return (*pAllow)(nullptr); //drop
		}
	}

	struct rateStats_t
	{
		/// Parameters
		uint32_t outputRateLimit;        // maximum number of updates allowed per updatePeriodms
		uint32_t updatePeriodms;         // how often the limit is updated
		uint16_t updatePeriodMultiplier; // how many update periods can be saved up for a burst of updates

		/// Runtime variables
		std::atomic_uint_fast32_t outputRate;     // current update count over the last second
		std::atomic_uint_fast32_t inputRate;      // current input update rate
		std::atomic_uint_fast64_t droppedUpdates; // number of dropped updates
		std::atomic_uint_fast64_t nextUpdatems;   // last time that updates were subtracted from the update count

	};

	rateStats_t* rateStats;
	static std::unordered_map<std::string, rateStats_t> rateStatsCollection;
};


std::unordered_map<std::string, RateLimitTransform::rateStats_t> RateLimitTransform::rateStatsCollection;

#endif /* RATELIMITTRANSFORM_H_ */
