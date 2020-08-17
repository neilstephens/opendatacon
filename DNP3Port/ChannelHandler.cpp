#include "ChannelHandler.h"
#include "DNP3Port.h"
#include "ChannelStateSubscriber.h"
#include <opendatacon/util.h>

ChannelHandler::ChannelHandler(DNP3Port *p):
	pPort(p),
	pChannel(nullptr),
	status(opendnp3::LinkStatus::UNRESET),
	link_dead(true),
	channel_dead(true)
{}

// Called by OpenDNP3 Thread Pool
void ChannelHandler::StateListener(opendnp3::ChannelState state)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: ChannelState {}.", pPort->Name, opendnp3::ChannelStateToString(state));

	if(state != opendnp3::ChannelState::OPEN)
	{
		channel_dead = true;
		pPort->OnLinkDown();
	}
	else
	{
		channel_dead = false;
	}
}

std::shared_ptr<asiodnp3::IChannel> ChannelHandler::GetChannel()
{
	static std::unordered_map<std::string, std::weak_ptr<asiodnp3::IChannel>> Channels;

	if(pChannel)
		return pChannel;

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
		ChannelID = pConf->mAddrConf.IP +":"+ std::to_string(pConf->mAddrConf.Port);
		isSerial = false;
	}

	//Create a channel listener that will subscribe this port to channel updates
	//if we're the first port on the channel, this listener will get passed to the dnp3 stack below
	//otherwise it can be destroyed, still leaving this port subscribed.
	auto listener = std::make_shared<ChannelListener>(ChannelID,this);

	//create a new channel if needed
	if(!Channels.count(ChannelID) || !(pChannel = Channels[ChannelID].lock()))
	{
		if(isSerial)
		{
			pChannel = pPort->IOMgr->AddSerial(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
				asiopal::ChannelRetry(
					openpal::TimeDuration::Milliseconds(500),
					openpal::TimeDuration::Milliseconds(5000)),
				pConf->mAddrConf.SerialSettings,listener);
		}
		else
		{
			switch (pPort->ClientOrServer())
			{
				case TCPClientServer::SERVER:
				{
					pChannel = pPort->IOMgr->AddTCPServer(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
						pConf->pPointConf->ServerAcceptMode,
						pConf->mAddrConf.IP,
						pConf->mAddrConf.Port,listener);
					break;
				}

				case TCPClientServer::CLIENT:
				{
					pChannel = pPort->IOMgr->AddTCPClient(ChannelID, pConf->LOG_LEVEL.GetBitfield(),
						asiopal::ChannelRetry(
							openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMinms),
							openpal::TimeDuration::Milliseconds(pConf->pPointConf->TCPConnectRetryPeriodMaxms)),
						pConf->mAddrConf.IP,
						"0.0.0.0",
						pConf->mAddrConf.Port,listener);
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
		Channels[ChannelID] = pChannel;
	}

	return pChannel;
}
