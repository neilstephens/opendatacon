#include "ChannelHandler.h"
#include "ChannelStateSubscriber.h"
#include "DNP3Port.h"
#include <opendatacon/util.h>

ChannelHandler::ChannelHandler(DNP3Port *p):
	pIOS(odc::asio_service::Get()),
	pSyncStrand(pIOS->make_strand()),
	handler_tracker(std::make_shared<char>()),
	SetLinkStatus(pSyncStrand->wrap([this,h {handler_tracker}](opendnp3::LinkStatus status){SetLinkStatus_(status);})),
	LinkUp(pSyncStrand->wrap([this,h{handler_tracker}](){LinkUp_();})),
	LinkDown(pSyncStrand->wrap([this,h{handler_tracker}](){LinkDown_();})),
	StateListener(pSyncStrand->wrap([this,h{handler_tracker}](opendnp3::ChannelState state){StateListener_(state);})),
	pPort(p),
	pChannel(nullptr),
	pWatchdog(nullptr),
	link_status(opendnp3::LinkStatus::UNRESET),
	link_deadness(LinkDeadness::LinkDownChannelDown)
{}

ChannelHandler::~ChannelHandler()
{
	//re-assign these because they hold shared pointers to the handler tracker
	StateListener = [](opendnp3::ChannelState){};
	LinkDown = [](){};
	LinkUp = [](){};
	SetLinkStatus = [](opendnp3::LinkStatus){};

	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	//now the only tracker shared pointers will be in actual outstanding handlers

	//wait til they're all gone, or harmless
	while(!tracker.expired() && !pIOS->stopped() && !pSyncStrand->running_in_this_thread())
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
		pWatchdog->LinkDown(pPort->ptr());
	}
	else if(state != opendnp3::ChannelState::OPEN && link_deadness != LinkDeadness::LinkDownChannelDown)
	{
		link_deadness = LinkDeadness::LinkDownChannelDown;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkDown(pPort->ptr());
	}
}

void ChannelHandler::SetLinkStatus_(opendnp3::LinkStatus status)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkStatus {}.", pPort->Name, opendnp3::LinkStatusSpec::to_human_string(status));

	link_status = status;
}
void ChannelHandler::LinkDown_()
{
	auto previous_deadness = link_deadness.load();

	if(link_deadness == LinkDeadness::LinkUpChannelUp)
	{
		link_deadness = LinkDeadness::LinkDownChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkDown(pPort->ptr());
	}
}
void ChannelHandler::LinkUp_()
{
	auto previous_deadness = link_deadness.load();

	if(link_deadness != LinkDeadness::LinkUpChannelUp)
	{
		link_deadness = LinkDeadness::LinkUpChannelUp;
		pPort->LinkDeadnessChange(previous_deadness,link_deadness);
		pWatchdog->LinkUp(pPort->ptr());
	}
}

std::shared_ptr<opendnp3::IChannel> ChannelHandler::SetChannel()
{
	static std::unordered_map<std::string, std::pair<std::weak_ptr<opendnp3::IChannel>,std::weak_ptr<ChannelLinksWatchdog>>> Channels;

	auto pConf = static_cast<DNP3PortConf*>(pPort->pConf.get());

	std::string ChannelID;
	bool isSerial;
	auto udp_listen_port = (pConf->mAddrConf.UDPListenPort == 0) ? pConf->mAddrConf.Port : pConf->mAddrConf.UDPListenPort;

	if(pConf->mAddrConf.IP.empty())
	{
		ChannelID = pConf->mAddrConf.SerialSettings.deviceName;
		isSerial = true;
	}
	else
	{
		if(pConf->mAddrConf.Transport == IPTransport::UDP)
			ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port)+":"+std::to_string(udp_listen_port);
		else
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
	auto watchdog_mode = pConf->mAddrConf.ChannelLinksWatchdogBark;
	if(isSerial)
	{
		pChannel = pPort->IOMgr->AddSerial(ChannelID, pConf->LOG_LEVEL,
			opendnp3::ChannelRetry(
				opendnp3::TimeDuration::Milliseconds(500),
				opendnp3::TimeDuration::Milliseconds(5000),
				opendnp3::TimeDuration::Milliseconds(500)),
			pConf->mAddrConf.SerialSettings,listener);
		if(watchdog_mode == WatchdogBark::DEFAULT)
			watchdog_mode = WatchdogBark::NEVER;
	}
	else
	{
		if(pConf->mAddrConf.Transport == IPTransport::UDP)
		{
			pChannel = pPort->IOMgr->AddUDPChannel(ChannelID, pConf->LOG_LEVEL,
				opendnp3::ChannelRetry(
					opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms),
					opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMaxms),
					opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms)),
				opendnp3::IPEndpoint("0.0.0.0",udp_listen_port),
				opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port),
				listener);
			if(watchdog_mode == WatchdogBark::DEFAULT)
				watchdog_mode = WatchdogBark::NEVER;
		}
		else //TCP or TLS
		{
			switch (pPort->ClientOrServer())
			{
				case TCPClientServer::SERVER:
				{
					if(pConf->mAddrConf.Transport == IPTransport::TLS)
					{
						pChannel = pPort->IOMgr->AddTLSServer(ChannelID, pConf->LOG_LEVEL,
							pConf->pPointConf->ServerAcceptMode,
							opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port),
							opendnp3::TLSConfig(pConf->mAddrConf.TLSFiles.PeerCertFile,pConf->mAddrConf.TLSFiles.LocalCertFile,pConf->mAddrConf.TLSFiles.PrivateKeyFile),
							listener);
					}
					else
					{
						pChannel = pPort->IOMgr->AddTCPServer(ChannelID, pConf->LOG_LEVEL,
							pConf->pPointConf->ServerAcceptMode,
							opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port)
							,listener);
					}
					break;
				}

				case TCPClientServer::CLIENT:
				{
					if(pConf->mAddrConf.Transport == IPTransport::TLS)
					{
						pChannel = pPort->IOMgr->AddTLSClient(ChannelID, pConf->LOG_LEVEL,
							opendnp3::ChannelRetry(
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms),
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMaxms),
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms)),
							std::vector<opendnp3::IPEndpoint>({opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port)}),"0.0.0.0",
							opendnp3::TLSConfig(pConf->mAddrConf.TLSFiles.PeerCertFile,pConf->mAddrConf.TLSFiles.LocalCertFile,pConf->mAddrConf.TLSFiles.PrivateKeyFile),
							listener);
					}
					else
					{
						pChannel = pPort->IOMgr->AddTCPClient(ChannelID, pConf->LOG_LEVEL,
							opendnp3::ChannelRetry(
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms),
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMaxms),
								opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->IPConnectRetryPeriodMinms)),
							std::vector<opendnp3::IPEndpoint>({opendnp3::IPEndpoint(pConf->mAddrConf.IP,pConf->mAddrConf.Port)}),
							"0.0.0.0",listener);
					}
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
			if(watchdog_mode == WatchdogBark::DEFAULT)
				watchdog_mode = WatchdogBark::ONFINAL;
		} //TCP or TLS
	}
	pWatchdog = std::make_shared<ChannelLinksWatchdog>(watchdog_mode);
	Channels[ChannelID] = {pChannel,pWatchdog};
	return pChannel;
}
