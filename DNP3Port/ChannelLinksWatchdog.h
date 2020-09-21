#ifndef CHANNELLINKSWATCHDOG_H
#define CHANNELLINKSWATCHDOG_H

#include <opendatacon/asio.h>
#include <set>

class DNP3Port;
class ChannelLinksWatchdog
{
private:
	//Strand for synchronising access to sets
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unique_ptr<asio::io_service::strand> pSyncStrand = pIOS->make_strand();
public:
	ChannelLinksWatchdog();
	//Synchronised versions of their private couterparts
	std::function<void(std::weak_ptr<DNP3Port> pPort)> LinkUp = pSyncStrand->wrap([this](std::weak_ptr<DNP3Port> pPort){LinkUp_(pPort);});
	std::function<void(std::weak_ptr<DNP3Port> pPort)> LinkDown = pSyncStrand->wrap([this](std::weak_ptr<DNP3Port> pPort){LinkDown_(pPort);});

private:
	//these access sets, so they're only called by sync'd public counterparts
	void LinkUp_(std::weak_ptr<DNP3Port> pPort);
	void LinkDown_(std::weak_ptr<DNP3Port> pPort);

	std::set<std::weak_ptr<DNP3Port>,std::owner_less<std::weak_ptr<DNP3Port>>> UpSet;
	std::set<std::weak_ptr<DNP3Port>,std::owner_less<std::weak_ptr<DNP3Port>>> DownSet;
};

#endif // CHANNELLINKSWATCHDOG_H
