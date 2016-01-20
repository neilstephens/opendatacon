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
//  ASIOScheduler.h
//  opendatacon
//
//  Created by Alan Murray on 3/11/2014.
//
//

#ifndef __opendatacon__ASIOScheduler__
#define __opendatacon__ASIOScheduler__

#include <queue>
#include <asio.hpp>

class ASIOSchedulerTask
{
public:
	ASIOSchedulerTask(uint32_t periodms_, std::function<void(void)>& action_):
		periodms(periodms_),
		action(action_)
	{ }

	void execute()
	{
		action();
	}

	bool operator<(const ASIOSchedulerTask& other) const
	{
		return this->nextpoll < other.nextpoll;
	}

	bool operator>(const ASIOSchedulerTask& other) const
	{
		return this->nextpoll > other.nextpoll;
	}

	void schedule()
	{
		nextpoll = std::chrono::steady_clock::now() + std::chrono::milliseconds(periodms);
	}

	void reschedule()
	{
		nextpoll = nextpoll + std::chrono::milliseconds(periodms);
	}

	uint32_t periodms;
	std::function<void(void)> action;
	std::chrono::steady_clock::time_point nextpoll;
};

class ASIOSchedulerTaskComparison
{
	bool reverse;
public:
	ASIOSchedulerTaskComparison(const bool& revparam=false)
	{reverse=revparam;}
	bool operator() (const ASIOSchedulerTask* lhs, const ASIOSchedulerTask* rhs) const
	{
		if (reverse)
			return (*lhs < *rhs);
		else
			return (*lhs > *rhs);
	}
};

class ASIOScheduler
{
public:
	ASIOScheduler(asio::io_service& io_service):
		running(false),
		mTimer(io_service)
	{}

	~ASIOScheduler()
	{
		Clear();
	}

	void Start()
	{
		if (Schedule.empty()) return;

		running = true;
		ASIOSchedulerTask* task = Schedule.top();

		mTimer.expires_at(task->nextpoll);
		mTimer.async_wait(
		      [this](asio::error_code err_code)
		      {
		            if(err_code != asio::error::operation_aborted)
					this->DoScheduledTask();
			});
	}

	template<typename F>
	void Add(uint32_t periodms, F &action)
	{
		std::function<void(void)> newaction = action;
		ASIOSchedulerTask* task = new ASIOSchedulerTask(periodms, newaction);
		task->schedule();
		Schedule.push(task);
		if (running) Start();
	}

	void Stop()
	{
		running = false;
		mTimer.cancel();
	}

	void Clear()
	{
		while (!Schedule.empty())
		{
			delete Schedule.top();
			Schedule.pop();
		}
	}

	void DoScheduledTask()
	{
		ASIOSchedulerTask* task = Schedule.top();
		task->execute();
		Schedule.pop();
		task->reschedule();
		Schedule.push(task);
		if (running) Start();
	}

private:
	bool running;
	typedef std::priority_queue<ASIOSchedulerTask*, std::vector<ASIOSchedulerTask*>, ASIOSchedulerTaskComparison> ScheduleType;
	ScheduleType Schedule;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	Timer_t mTimer;
};

#endif /* defined(__opendatacon__ASIOScheduler__) */
