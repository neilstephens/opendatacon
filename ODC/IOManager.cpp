//
//  IOManager.cpp
//  opendatacon_suite
//
//  Created by Alan Murray on 3/06/2017.
//
//

#include <opendatacon/IOManager.h>
#include <opendatacon/Task.h>

namespace odc {
	void AsyncIOManager::post(Task& task)
	{
		task.schedule(this);
		impl->post(task);
	}
	
	void AsyncIOManager::stop(Task& task)
	{
		impl->stop(task);
	}

	void SyncIOManager::post(Task& task)
	{
		task.schedule(this);
		impl->post(task);
	}
	
	void SyncIOManager::stop(Task& task)
	{
		impl->stop(task);
	}
}