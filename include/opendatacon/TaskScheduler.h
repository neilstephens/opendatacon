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
//  TaskScheduler.h
//  opendatacon
//
//  Created by Alan Murray on 3/11/2014.
//
//

#ifndef __opendatacon__TaskScheduler__
#define __opendatacon__TaskScheduler__

#include <queue>
#include <set>
#include <asio.hpp>
#include "IOManager.h"
#include "Task.h"
#include "Timestamp.h"

namespace odc {	
	class TaskScheduler
	{
	public:
		TaskScheduler(asio::io_service& io_service):
		Strand_(io_service),
		Timer_(io_service)
		{
		
		}
		
		~TaskScheduler()
		{
			Shutdown();
		}
		
		void Shutdown()
		{
			Timer_.cancel();
			
			Strand_.dispatch([this](){
				std::unique_lock<std::mutex> lk(m);
				
				Tasks_.clear();
				
				lk.unlock();
				cv.notify_one();
			});
			
			// wait for the strand to complete
			{
				std::unique_lock<std::mutex> lk(m);
				cv.wait(lk, [this]{return Tasks_.empty();});
			}
		}
		
		void Schedule(Task& task)
		{
			Strand_.dispatch([this,&task](){
				//Schedule.push(task);
				Tasks_.insert(&task);
				RefreshTimer();
			});
		}
		
		void Unschedule(Task& task)
		{
			Strand_.dispatch([this,&task](){
				std::unique_lock<std::mutex> lk(m);
				
				while (Tasks_.count(&task))
					Tasks_.erase(&task);
				
				lk.unlock();
				cv.notify_one();
			});
			
			// wait for the strand to complete
			{
				std::unique_lock<std::mutex> lk(m);
				cv.wait(lk, [this,&task]{ return Tasks_.count(&task) == 0; });
			}
		}
		
	private:
		void RefreshTimer()
		{
			if (Tasks_.empty()) return;
			
			//ScheduledTask* task = Schedule.top();
			Task* task = *(Tasks_.begin());
			
			Timer_.expires_at(task->GetNextPoll());
			Timer_.async_wait(Strand_.wrap(
							  [this](asio::error_code err_code)
							  {
								  if(err_code != asio::error::operation_aborted)
									  this->DoScheduledTask();
							  }));
		}
		
		void DoScheduledTask()
		{
			//ScheduledTask* task = Schedule.top();
			Task* task = *(Tasks_.begin());

			task->execute();
			//Schedule.pop();
			Tasks_.erase(Tasks_.begin());
			if (task->GetRepeats()) {
				task->reschedule();
				//Schedule.push(task);
				Tasks_.insert(task);
			} else {
				delete task;
			}
			RefreshTimer();
		}
		
	private:
		//typedef std::priority_queue<ScheduledTask*, std::vector<ScheduledTask*>, ScheduledTaskComparison> ScheduleType;
		typedef std::multiset<Task*, ScheduledTaskComparison> ScheduleType;
		
		typedef asio::basic_waitable_timer<odc::Clock> TaskShedulerTimer;

		asio::strand Strand_;
		TaskShedulerTimer Timer_;
		ScheduleType Tasks_;
		std::mutex m;
		std::condition_variable cv;
	};
}

#endif /* defined(__opendatacon__TaskScheduler__) */
