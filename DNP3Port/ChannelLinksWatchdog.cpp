#include "ChannelLinksWatchdog.h"
#include "DNP3Port.h"

ChannelLinksWatchdog::ChannelLinksWatchdog()
{}

void ChannelLinksWatchdog::LinkUp_(DNP3Port* const pPort)
{
	DownSet.erase(pPort);
	UpSet.insert(pPort);
}

void ChannelLinksWatchdog::LinkDown_(DNP3Port* const pPort)
{
	DownSet.insert(pPort);
	if(UpSet.erase(pPort) && UpSet.empty())
	{
		for(auto p : DownSet)
			p->ChannelWatchdogTrigger(true);
		for(auto p : DownSet)
			p->ChannelWatchdogTrigger(false);
	}
}
