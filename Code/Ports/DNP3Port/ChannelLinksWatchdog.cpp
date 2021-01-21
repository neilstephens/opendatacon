#include "ChannelLinksWatchdog.h"
#include "DNP3Port.h"

ChannelLinksWatchdog::ChannelLinksWatchdog()
{}

void ChannelLinksWatchdog::LinkUp_(std::weak_ptr<odc::DataPort> pPort)
{
	DownSet.erase(pPort);
	UpSet.insert(pPort);
}

void ChannelLinksWatchdog::LinkDown_(std::weak_ptr<odc::DataPort> pPort)
{
	DownSet.insert(pPort);
	if(UpSet.erase(pPort) && UpSet.empty())
	{
		for(auto weak : DownSet)
			if(auto p = weak.lock())
				std::static_pointer_cast<DNP3Port>(p)->ChannelWatchdogTrigger(true);
		for(auto weak : DownSet)
			if(auto p = weak.lock())
				std::static_pointer_cast<DNP3Port>(p)->ChannelWatchdogTrigger(false);
	}
}
