#include "ChannelLinksWatchdog.h"
#include "DNP3Port.h"

ChannelLinksWatchdog::ChannelLinksWatchdog()
{}

void ChannelLinksWatchdog::LinkUp_(std::weak_ptr<DNP3Port> pPort)
{
	DownSet.erase(pPort);
	UpSet.insert(pPort);
}

void ChannelLinksWatchdog::LinkDown_(std::weak_ptr<DNP3Port> pPort)
{
	DownSet.insert(pPort);
	if(UpSet.erase(pPort) && UpSet.empty())
	{
		for(auto weak : DownSet)
			if(auto p = weak.lock())
				p->ChannelWatchdogTrigger(true);
		for(auto weak : DownSet)
			if(auto p = weak.lock())
				p->ChannelWatchdogTrigger(false);
	}
}
