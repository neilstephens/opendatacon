#include "ChannelLinksWatchdog.h"
#include "DNP3Port.h"

ChannelLinksWatchdog::ChannelLinksWatchdog(const WatchdogBark& mode):
	mode(mode)
{}

void ChannelLinksWatchdog::LinkUp_(std::weak_ptr<odc::DataPort> pPort)
{
	DownSet.erase(pPort);
	UpSet.insert(pPort);
}

void ChannelLinksWatchdog::LinkDown_(std::weak_ptr<odc::DataPort> pPort)
{
	DownSet.insert(pPort);
	const auto wasUp = UpSet.erase(pPort);
	switch(mode)
	{
		case WatchdogBark::ONFIRST:
			if(wasUp)
				Bark();
			return;
		case WatchdogBark::ONFINAL:
			if(wasUp && UpSet.empty())
				Bark();
			return;
		default:
			return;
	}
}

void ChannelLinksWatchdog::Bark()
{
	for(auto& Set : {UpSet,DownSet})
		for(auto weak : Set)
			if(auto p = weak.lock())
				std::static_pointer_cast<DNP3Port>(p)->ChannelWatchdogTrigger(true);
	for(auto& Set : {UpSet,DownSet})
		for(auto weak : Set)
			if(auto p = weak.lock())
				std::static_pointer_cast<DNP3Port>(p)->ChannelWatchdogTrigger(false);
}
