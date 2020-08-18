#ifndef CHANNELHANDLER_H
#define CHANNELHANDLER_H

#include <opendatacon/asio.h>
#include <opendnp3/gen/LinkStatus.h>
#include <opendnp3/gen/ChannelState.h>
#include <asiodnp3/IChannel.h>
#include <atomic>

enum class LinkDeadness : uint8_t
{
	LinkDownChannelDown,
	LinkDownChannelUp,
	LinkUpChannelUp
};

class DNP3Port;

class ChannelHandler
{
private:
	//Strand for synchronising channel/link state changes
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unique_ptr<asio::io_service::strand> pSetStrand = pIOS->make_strand();

public:
	ChannelHandler() = delete;
	ChannelHandler(DNP3Port* p);

	//Synchronised versions of their private couterparts
	std::function<void(opendnp3::LinkStatus status)> SetLinkStatus = pSetStrand->wrap([this](opendnp3::LinkStatus status){SetLinkStatus_(status);});
	std::function<void()> LinkUp = pSetStrand->wrap([this](){LinkUp_();});
	std::function<void()> LinkDown = pSetStrand->wrap([this](){LinkDown_();});
	std::function<void(opendnp3::ChannelState state)> StateListener = pSetStrand->wrap([this](opendnp3::ChannelState state){StateListener_(state);});

	//Factory function for the Channel
	std::shared_ptr<asiodnp3::IChannel> SetChannel();
	//inline Getters, to ensure readonly atomic access outside synchronisation
	inline opendnp3::LinkStatus GetLinkStatus() const
	{
		return link_status.load();
	}
	inline LinkDeadness GetLinkDeadness() const
	{
		return link_deadness.load();
	}
	inline std::shared_ptr<asiodnp3::IChannel> GetChannel() const
	{
		return pChannel;
	}

private:
	//unsynchronised state logic functions
	void StateListener_(opendnp3::ChannelState state);
	void SetLinkStatus_(opendnp3::LinkStatus status);
	void LinkDown_();
	void LinkUp_();

	//data members
	DNP3Port* const pPort;
	std::shared_ptr<asiodnp3::IChannel> pChannel;
	//use atomic for status data - so the getters can read them concurrently
	std::atomic<opendnp3::LinkStatus> link_status;
	std::atomic<LinkDeadness> link_deadness;
};

#endif // CHANNELHANDLER_H
