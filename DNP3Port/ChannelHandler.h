#ifndef CHANNELHANDLER_H
#define CHANNELHANDLER_H

#include <opendnp3/gen/LinkStatus.h>
#include <opendnp3/gen/ChannelState.h>
#include <asiodnp3/IChannel.h>
#include <atomic>

class DNP3Port;
class ChannelHandler
{
	friend class DNP3Port;
	friend class DNP3MasterPort;
	friend class DNP3OutstationPort;
public:
	void StateListener(opendnp3::ChannelState state);
protected:
	ChannelHandler(DNP3Port* p);
	std::shared_ptr<asiodnp3::IChannel> GetChannel();

	DNP3Port* const pPort;
	std::shared_ptr<asiodnp3::IChannel> pChannel;
	std::atomic<opendnp3::LinkStatus> status;
	std::atomic_bool link_dead;
	std::atomic_bool channel_dead;
};

#endif // CHANNELHANDLER_H
