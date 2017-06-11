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
//
//  Task.h
//  opendatacon
//
//  Created by Alan Murray on 3/11/2014.
//
//


#ifndef __opendatacon__Task__
#define __opendatacon__Task__

#include <queue>
#include <set>
#include <asio.hpp>
#include "Timestamp.h"

#include <opendatacon/util.h>

namespace odc {
	class IOManager;
	
	inline odc::Clock::time_point random_interval(const odc::Clock::time_point& lasttime, const unsigned int& average_interval_ms)
	{
		//use last time as a random seed
		auto seed = (rand_t)(lasttime.time_since_epoch().count());
		
		//random interval - uniform distribution, minimum 1ms
		auto delay_ms = (unsigned int)((2*average_interval_ms-2)*ZERO_TO_ONE(seed)+1.5); //the .5 is for rounding down
		return lasttime + std::chrono::milliseconds(delay_ms);
	}

	inline odc::Clock::time_point scheduler_interval(const odc::Clock::time_point& lasttime, const unsigned int& interval_ms)
	{
		//random interval - uniform distribution, minimum 1ms
		return lasttime + std::chrono::milliseconds(interval_ms);
	}
	
	class Task
	{
	public:
		Task(std::function<void(void)> const& action, std::function<odc::Clock::time_point(odc::Clock::time_point)> const& scheduler, bool repeats = true);
		~Task();
		
		bool operator<(const Task& other) const;
		bool operator>(const Task& other) const;

		void execute();
		
		void schedule(IOManager* IOMgr);
		void reschedule();
				
		odc::Clock::time_point GetNextPoll();
		bool GetRepeats();

	private:
		IOManager* IOMgr_;
		const std::function<void(void)> action_;
		const std::function<odc::Clock::time_point(odc::Clock::time_point)> scheduler_;
		bool repeats_;
		odc::Clock::time_point nextpoll_;
	};
	
	class TaskComparison
	{
		bool reverse;
	public:
		TaskComparison(const bool& revparam=false)
		{reverse=revparam;}
		bool operator() (const Task* lhs, const Task* rhs) const
		{
			if (reverse)
				return (*lhs < *rhs);
			else
				return (*lhs > *rhs);
		}
	};
}

#endif /* defined(__opendatacon__Task__) */
