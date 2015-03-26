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
#include <asiopal/UTCTimeSource.h>
#include <unordered_map>

class RateLimitTransform: public Transform
{
public:
	RateLimitTransform(Json::Value params):
    Transform(params),
    outputRateLimit(10),
    updatePeriodms(100),
    updatePeriodMultiplier(10)
    {
        std::string name = "DEFAULT";
        if(!params["Name"].isNull() && params["Name"].isString())
        {
            name = params["Name"].asString();
        }
        if (rateStatsCollection.count(name))
        {
            rateStats = &rateStatsCollection[name];
        }
        else
        {
            rateStatsCollection.emplace(std::piecewise_construct,std::forward_as_tuple(name),std::forward_as_tuple());
            rateStats = &rateStatsCollection[name];
            rateStats->droppedUpdates = 0;
            rateStats->inputRate = 0;
            rateStats->outputRate = 0;
            rateStats->nextUpdatems = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch + updatePeriodms;
        }
        
        if(!params["outputRateLimit"].isNull() && params["outputRateLimit"].isUInt())
        {
            outputRateLimit = params["outputRateLimit"].asUInt();
        }
        if(!params["updatePeriodms"].isNull() && params["updatePeriodms"].isUInt())
        {
            outputRateLimit = params["updatePeriodms"].asUInt();
        }
        if(!params["updatePeriodMultiplier"].isNull() && params["updatePeriodMultiplier"].isUInt())
        {
            outputRateLimit = params["updatePeriodMultiplier"].asUInt();
        }
    };

	bool Event(opendnp3::Binary& meas, uint16_t& index) { return CheckPass(meas); };
    bool Event(opendnp3::Analog& meas, uint16_t& index) {return CheckPass(meas);};
	bool Event(opendnp3::DoubleBitBinary& meas, uint16_t& index) { return CheckPass(meas); };
	bool Event(opendnp3::Counter& meas, uint16_t& index) { return CheckPass(meas); };
	bool Event(opendnp3::FrozenCounter& meas, uint16_t& index) { return CheckPass(meas); };
	bool Event(opendnp3::BinaryOutputStatus& meas, uint16_t& index) { return CheckPass(meas); };
	bool Event(opendnp3::AnalogOutputStatus& meas, uint16_t& index) { return CheckPass(meas); };

    bool Event(opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index){return true;};
	bool Event(opendnp3::AnalogOutputInt16& arCommand, uint16_t index){return true;};
	bool Event(opendnp3::AnalogOutputInt32& arCommand, uint16_t index){return true;};
	bool Event(opendnp3::AnalogOutputFloat32& arCommand, uint16_t index){return true;};
	bool Event(opendnp3::AnalogOutputDouble64& arCommand, uint16_t index){return true;};

private:
    template<class T>
    bool CheckPass(T& meas)
    {
        // check if rollover of update count period
        auto eventTime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;
        
        // see if we need to subtract updates from the update counter
        // as this section isn't atomic this might overshoot and subtract too many updates from the counter
        // however, this will smooth out and self correct over time
        if (rateStats->nextUpdatems < eventTime)
        {
            // update nextUpdatems as soon as possible to reduce the likelihood of update overshoot
            rateStats->nextUpdatems += updatePeriodms;
            // reduce update count
            rateStats->outputRate -= outputRateLimit;
            // if we reduce the outputRate to below 0, reset to 0 and set nextUpdatems to in the future
            if (rateStats->outputRate < 0)
            {
                rateStats->nextUpdatems = eventTime + updatePeriodms;
                rateStats->outputRate = 0;
            }
            rateStats->inputRate = 0;
        }
        ++rateStats->inputRate;
        
        if (rateStats->outputRate < outputRateLimit * updatePeriodMultiplier)
        {
            ++rateStats->outputRate;
            return true;
        }
        else
        {
            ++rateStats->droppedUpdates;
            return false;
        }
    }
    
    /// Parameters
    uint32_t outputRateLimit; // maximum number of updates allowed per updatePeriodms
    uint32_t updatePeriodms; // how often the limit is updated
    uint16_t updatePeriodMultiplier; // how many update periods can be saved up for a burst of updates
    
    /// Runtime variables
    struct rateStats_t
    {
    public:
        std::atomic_int_fast32_t outputRate; // current update count over the last second
        std::atomic_int_fast32_t inputRate; // current input update rate
        std::atomic_uint_fast64_t droppedUpdates; // number of dropped updates
        std::atomic_uint_fast64_t nextUpdatems; // last time that updates were subtracted from the update count
        
    };
    
    rateStats_t* rateStats;
    static std::unordered_map<std::string, rateStats_t> rateStatsCollection;
};


std::unordered_map<std::string, RateLimitTransform::rateStats_t> RateLimitTransform::rateStatsCollection;

#endif /* RATELIMITTRANSFORM_H_ */