#ifndef CHANNELLINKSWATCHDOG_H
#define CHANNELLINKSWATCHDOG_H

#include <opendatacon/asio.h>
#include <unordered_set>

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
	std::function<void(DNP3Port* const pPort)> LinkUp = pSyncStrand->wrap([this](DNP3Port* const pPort){LinkUp_(pPort);});
	std::function<void(DNP3Port* const pPort)> LinkDown = pSyncStrand->wrap([this](DNP3Port* const pPort){LinkDown_(pPort);});

private:
	//these access sets, so they're only called by sync'd public counterparts
	void LinkUp_(DNP3Port* const pPort);
	void LinkDown_(DNP3Port* const pPort);

	std::unordered_set<DNP3Port*> UpSet;
	std::unordered_set<DNP3Port*> DownSet;
};

#endif // CHANNELLINKSWATCHDOG_H
