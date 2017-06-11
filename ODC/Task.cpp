//
//  TaskScheduler.cpp
//  opendatacon_suite
//
//  Created by Alan Murray on 30/05/2017.
//
//

#include <opendatacon/Task.h>
#include <opendatacon/IOManager.h>

namespace odc {
	Task::Task(std::function<void(void)> const& action, std::function<odc::Clock::time_point(odc::Clock::time_point)> const& scheduler, bool repeats):
	IOMgr_(nullptr),
	action_(action),
	scheduler_(scheduler),
	repeats_(repeats)
	{}
	
	Task::~Task() {
		if (IOMgr_) {
			IOMgr_->stop(*this);
		}
	}

	void Task::execute() {
		if (IOMgr_) {
			IOMgr_->post(action_);
		}
	}
	
	bool Task::operator<(const Task& other) const
	{
		return this->nextpoll_ < other.nextpoll_;
	}
	
	bool Task::operator>(const Task& other) const
	{
		return this->nextpoll_ > other.nextpoll_;
	}
	
	void Task::schedule(IOManager* IOMgr)
	{
		if (IOMgr_) {
			// TODO: log error?
			return;
		}
		IOMgr_ = IOMgr;
		nextpoll_ = scheduler_(nextpoll_);
	}
	
	void Task::reschedule()
	{
		nextpoll_ = scheduler_(nextpoll_);
	}
	
	odc::Clock::time_point Task::GetNextPoll()
	{
		return nextpoll_;
	}
	
	bool Task::GetRepeats()
	{
		return repeats_;
	}
	

}
