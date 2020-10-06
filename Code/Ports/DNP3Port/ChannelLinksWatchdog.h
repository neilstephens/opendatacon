#ifndef CHANNELLINKSWATCHDOG_H
#define CHANNELLINKSWATCHDOG_H

#include <opendatacon/asio.h>
#include <set>

namespace odc
{
class DataPort;
}

class ChannelLinksWatchdog
{
private:
	//Strand for synchronising access to sets
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unique_ptr<asio::io_service::strand> pSyncStrand = pIOS->make_strand();
public:
	ChannelLinksWatchdog();
	//Synchronised versions of their private couterparts
	std::function<void(std::weak_ptr<odc::DataPort> pPort)> LinkUp = pSyncStrand->wrap([this](std::weak_ptr<odc::DataPort> pPort){LinkUp_(pPort);});
	std::function<void(std::weak_ptr<odc::DataPort> pPort)> LinkDown = pSyncStrand->wrap([this](std::weak_ptr<odc::DataPort> pPort){LinkDown_(pPort);});

private:
	//these access sets, so they're only called by sync'd public counterparts
	void LinkUp_(std::weak_ptr<odc::DataPort> pPort);
	void LinkDown_(std::weak_ptr<odc::DataPort> pPort);

	std::set<std::weak_ptr<odc::DataPort>,std::owner_less<std::weak_ptr<odc::DataPort>>> UpSet;
	std::set<std::weak_ptr<odc::DataPort>,std::owner_less<std::weak_ptr<odc::DataPort>>> DownSet;
};

#endif // CHANNELLINKSWATCHDOG_H
