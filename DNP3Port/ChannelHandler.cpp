#include "ChannelHandler.h"
#include "ChannelStateSubscriber.h"
#include "DNP3Port.h"
#include <opendatacon/util.h>

ChannelHandler::ChannelHandler(DNP3Port *p):
	pPort(p),
	pChannel(nullptr),
	pWatchdog(nullptr),
	link_status(opendnp3::LinkStatus::UNRESET),
	link_deadness(LinkDeadness::LinkDownChannelDown)
{}

ChannelHandler::~ChannelHandler()
{
	StateListener = nullptr;
	LinkDown = nullptr;
	LinkUp = nullptr;
	SetLinkStatus = nullptr;
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();
}

// Called by OpenDNP3 Thread Pool
void ChannelHandler::StateListener_(opendnp3::ChannelState state)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: ChannelState {}.", pPort->Name, opendnp3::ChannelStateSpec::to_human_string(state));

	auto previous_deadness = link_deadness.load();

	if(state == opendnp3::ChannelState::OPEN && link_deadness == LinkDeadness::LinkDownChannelDown)
	{
		link_deadness = LinkDeadness::LinkDownChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkDown(pPort);
	}
	else if(state != opendnp3::ChannelState::OPEN && link_deadness != LinkDeadness::LinkDownChannelDown)
	{
		link_deadness = LinkDeadness::LinkDownChannelDown;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkDown(pPort);
	}
}

void ChannelHandler::SetLinkStatus_(opendnp3::LinkStatus status)
{
	link_status = status;
	auto previous_deadness = link_deadness.load();

	if(link_deadness == LinkDeadness::LinkDownChannelUp) //this is the initial link up event
	{
		link_deadness = LinkDeadness::LinkUpChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkUp(pPort);
	}
}
void ChannelHandler::LinkDown_()
{
	auto previous_deadness = link_deadness.load();

	if(link_deadness == LinkDeadness::LinkUpChannelUp)
	{
		link_deadness = LinkDeadness::LinkDownChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkDown(pPort);
	}
}
void ChannelHandler::LinkUp_()
{
	auto previous_deadness = link_deadness.load();

	if(link_deadness != LinkDeadness::LinkUpChannelUp)
	{
		link_deadness = LinkDeadness::LinkUpChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkUp(pPort);
	}
}

std::shared_ptr<opendnp3::IChannel> ChannelHandler::SetChannel()
{
	static std::unordered_map<std::string, std::pair<std::weak_ptr<opendnp3::IChannel>,std::weak_ptr<ChannelLinksWatchdog>>> Channels;

	auto pConf = static_cast<DNP3PortConf*>(pPort->pConf.get());

	std::string ChannelID;
	bool isSerial;
	if(pConf->mAddrConf.IP.empty())
	{
		ChannelID = pConf->mAddrConf.SerialSettings.deviceName;
		isSerial = true;
	}
	else
	{
		ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port)+":"+to_string(pPort->ClientOrServer());
		isSerial = false;
	}

	//Create a channel listener that will subscribe this port to channel updates
	//if we're the first port on the channel, this listener will get passed to the dnp3 stack below
	//otherwise it can be destroyed, still leaving this port subscribed.
	auto listener = std::make_shared<ChannelListener>(ChannelID,this);

	//if there's already a channel, just take some pointers and return
	if(Channels.count(ChannelID))
	{
		pChannel = Channels[ChannelID].first.lock();
		pWatchdog = Channels[ChannelID].second.lock();
		if(pChannel && pWatchdog)
			return pChannel;
	}

	//create a new channel and watchdog if we get to here
	if(isSerial)
	{
		pChannel = pPort->IOMgr->AddSerial(ChannelID, pConf->LOG_LEVEL,
			opendnp3::ChannelRetry(
				opendnp3::TimeDuration::Milliseconds(500),
				opendnp3::TimeDuration::Milliseconds(5000)),
			pConf->mAddrConf.SerialSettings,listener);
	}
	else
	{
		switch (pPort->ClientOrServer())
		{
			case TCPClientServer::SERVER:
			{
				pChannel = pPort->IOMgr->AddTCPServer(ChannelID, pConf->LOG_LEVEL,
					pConf->pPointConf->ServerAcceptMode,
					opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port)
					,listener);
				break;
			}

			case TCPClientServer::CLIENT:
			{
				pChannel = pPort->IOMgr->AddTCPClient(ChannelID, pConf->LOG_LEVEL,
					opendnp3::ChannelRetry(
						opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMinms),
						opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMaxms)),
					std::vector<opendnp3::IPEndpoint>({opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port)}),
					"0.0.0.0",listener);
				break;
			}

			default:
			{
				const std::string msg(pPort->Name + ": Can't determine if TCP socket is client or server");
				if(auto log = odc::spdlog_get("DNP3Port"))
					log->error(msg);
				throw std::runtime_error(msg);
			}
		}
	}
	pWatchdog = std::make_shared<ChannelLinksWatchdog>();
	Channels[ChannelID] = {pChannel,pWatchdog};
	return pChannel;
}
