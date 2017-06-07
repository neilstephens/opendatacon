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

namespace odc {
	class Task;
	class ScheduledTaskComparison;
}

#ifndef __opendatacon__Task__
#define __opendatacon__Task__

#include <queue>
#include <set>
#include <asio.hpp>
#include "IOManager.h"
#include "Timestamp.h"

namespace odc {
	class IOManager;
	
	class Task
	{
	public:
		Task(std::function<void(void)> const& action, uint32_t periodms, bool repeats);
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
		uint32_t periodms_;
		bool repeats_;
		odc::Clock::time_point nextpoll_;
	};
	
	class ScheduledTaskComparison
	{
		bool reverse;
	public:
		ScheduledTaskComparison(const bool& revparam=false)
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
