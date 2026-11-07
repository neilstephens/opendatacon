/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * ICCPPortBase.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  What the ICCP client and server ports share, see ICCPPortBase.h.
 */

#include "ICCPPortBase.h"
#include "Log.h"
#include "MmsNames.h"
#include <map>
#include <mutex>
#include <opendatacon/util.h>
#include <regex>

using namespace iccp;

namespace
{
//The UI commands of the ports whose role includes the server side: OperatorRequest and OtherExternalEvent, and the optional parts of Block 5: the Local Reset action,
//local tagging and the list of tags (server side), and Get Tag and Set Tag of the peer's devices (client side, for the ports that operate devices)
class ICCPControl: public IUIResponder
{
public:
	ICCPControl()
	{
		AddCommand("OperatorRequest", [this](const ParamCollection& params) -> const Json::Value
			{
				return Run(tase2::DsCondition::OperatorRequest, params, false);
			}, "Sends a transfer set report with the OperatorRequest condition. Syntax: 'OperatorRequest <ICCPPort|Regex> [<TransferSet>]'");
		AddCommand("OtherExternalEvent", [this](const ParamCollection& params) -> const Json::Value
			{
				return Run(tase2::DsCondition::OtherExternalEvent, params, true);
			}, "Sends a transfer set report with the OtherExternalEvent condition. Syntax: 'OtherExternalEvent <ICCPPort|Regex> [<EventCode> [<TransferSet>]]'");
		AddCommand("LocalReset", [this](const ParamCollection& params) -> const Json::Value
			{
				return RunTag(TagAction::LocalReset, params);
			}, "Local Reset action of a device this port serves: the device goes from ARMED to IDLE (as if by a local action), and gets the tag if one is given. The MMS event notification of the action is not sent. "
			"Syntax: 'LocalReset <ICCPPort|Regex> <Control> [<NO_TAG|OPEN_AND_CLOSE_INHIBIT|CLOSE_ONLY_INHIBIT> [<Reason>]]'");
		AddCommand("LocalSetTag", [this](const ParamCollection& params) -> const Json::Value
			{
				return RunTag(TagAction::LocalSetTag, params);
			}, "Sets the tag of a taggable device this port serves, locally (the device state and the owner of a tag are not checked). "
			"Syntax: 'LocalSetTag <ICCPPort|Regex> <Control> <NO_TAG|OPEN_AND_CLOSE_INHIBIT|CLOSE_ONLY_INHIBIT> [<Reason>]'");
		AddCommand("Tags", [this](const ParamCollection& params) -> const Json::Value
			{
				return RunTags(params);
			}, "Lists the tags of the taggable devices this port serves. Syntax: 'Tags <ICCPPort|Regex>'");
		AddCommand("GetTag", [this](const ParamCollection& params) -> const Json::Value
			{
				return RunTag(TagAction::GetTag, params);
			}, "Get Tag of a device of the peer that this port operates (a Read of its tag object): the tag, the reason and the state are logged. Syntax: 'GetTag <ICCPPort|Regex> <Control>'");
		AddCommand("SetTag", [this](const ParamCollection& params) -> const Json::Value
			{
				return RunTag(TagAction::SetTag, params);
			}, "Set Tag of a device of the peer that this port operates (a Write of its tag object); the peer's answer is logged. "
			"Syntax: 'SetTag <ICCPPort|Regex> <Control> <NO_TAG|OPEN_AND_CLOSE_INHIBIT|CLOSE_ONLY_INHIBIT> [<Reason>]'");
	}

	void Add(ICCPPortBase* p)
	{
		std::lock_guard<std::mutex> lock(mtx);
		ports[p->GetName()] = p;
	}
	void Remove(ICCPPortBase* p)
	{
		std::lock_guard<std::mutex> lock(mtx);
		const auto it = ports.find(p->GetName());
		if(it != ports.end() && it->second == p)
			ports.erase(it);
	}

private:
	std::mutex mtx;
	std::map<std::string, ICCPPortBase*> ports;

	Json::Value Run(uint8_t condition, const ParamCollection& params, bool withEventCode)
	{
		if(params.count("0") == 0)
			return IUIResponder::GenerateResult("Bad parameter: the first parameter is the ICCP port name (or a regex)");
		int64_t code = 0;
		std::string transferSet;
		if(withEventCode)
		{
			if(params.count("1"))
			{
				try
				{
					code = std::stoll(params.at("1"));
				}
				catch(const std::exception&)
				{
					return IUIResponder::GenerateResult("Bad parameter: the event code must be a number");
				}
			}
			if(params.count("2"))
				transferSet = params.at("2");
		}
		else if(params.count("1"))
			transferSet = params.at("1");
		std::regex re;
		try
		{
			re = std::regex(params.at("0"));
		}
		catch(const std::exception&)
		{
			return IUIResponder::GenerateResult("Bad regex");
		}
		size_t matched = 0;
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& [name, port] : ports)
			if(std::regex_match(name, re) && port->ServesDevicesOrPoints())
			{
				port->RaiseCondition(condition, code, transferSet);
				++matched;
			}
		if(matched == 0)
			return IUIResponder::GenerateResult("No ICCP port matched");
		return IUIResponder::GenerateResult("Success");
	}

	enum class TagAction
	{
		LocalReset,
		LocalSetTag,
		GetTag,
		SetTag
	};

	//the parameters from the one numbered `first` on, joined by blanks: the reason of a tag
	static std::string JoinFrom(const ParamCollection& params, size_t first)
	{
		std::string s;
		for(size_t i = first; params.count(std::to_string(i)); ++i)
		{
			if(!s.empty())
				s += ' ';
			s += params.at(std::to_string(i));
		}
		return s;
	}

	//Runs `action` on each port the first parameter names (a name or a regex) that `wanted` accepts. An action returns an empty string if it was started, else why not.
	//Success if it was started on at least one port, else the reasons (a regex that matches several ports will have some without the control).
	template<typename Wanted, typename Action>
	Json::Value ForPorts(const ParamCollection& params, Wanted&& wanted, Action&& action, const char* noPort)
	{
		std::regex re;
		try
		{
			re = std::regex(params.at("0"));
		}
		catch(const std::exception&)
		{
			return IUIResponder::GenerateResult("Bad regex");
		}
		size_t matched = 0, started = 0;
		std::string reasons;
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& [name, port] : ports)
			if(std::regex_match(name, re) && wanted(*port))
			{
				++matched;
				const std::string why = action(*port);
				if(why.empty())
					++started;
				else
					reasons += (reasons.empty() ? "" : "; ") + name + ": " + why;
			}
		if(matched == 0)
			return IUIResponder::GenerateResult(noPort);
		return IUIResponder::GenerateResult(started > 0 ? "Success" : reasons);
	}

	Json::Value RunTag(TagAction action, const ParamCollection& params)
	{
		if(params.count("0") == 0 || params.count("1") == 0)
			return IUIResponder::GenerateResult("Bad parameter: the first two parameters are the ICCP port name (or a regex) and the control name");
		const std::string control = params.at("1");
		std::optional<tase2::DeviceTag> tag;
		std::string reason;
		if(action != TagAction::GetTag && params.count("2"))
		{
			tag = tase2::DeviceTagFromString(params.at("2"));
			if(!tag)
				return IUIResponder::GenerateResult("Bad parameter: the tag must be NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT");
			reason = JoinFrom(params, 3);
		}
		if((action == TagAction::LocalSetTag || action == TagAction::SetTag) && !tag)
			return IUIResponder::GenerateResult("Bad parameter: the tag is required (NO_TAG, OPEN_AND_CLOSE_INHIBIT or CLOSE_ONLY_INHIBIT)");
		const bool clientSide = action == TagAction::GetTag || action == TagAction::SetTag;
		return ForPorts(params, [clientSide](ICCPPortBase& p) { return clientSide ? p.OperatesDevices() : p.ServesDevicesOrPoints(); },
			[&](ICCPPortBase& p)
			{
				switch(action)
				{
					case TagAction::LocalReset: return p.LocalReset(control, tag, reason);
					case TagAction::LocalSetTag: return p.SetLocalTag(control, *tag, reason);
					case TagAction::GetTag: return p.GetPeerTag(control);
					case TagAction::SetTag: return p.SetPeerTag(control, *tag, reason);
				}
				return std::string("unknown action");
			}, clientSide ? "No ICCP port that operates the peer's devices matched" : "No ICCP port that serves devices matched");
	}

	Json::Value RunTags(const ParamCollection& params)
	{
		if(params.count("0") == 0)
			return IUIResponder::GenerateResult("Bad parameter: the first parameter is the ICCP port name (or a regex)");
		std::regex re;
		try
		{
			re = std::regex(params.at("0"));
		}
		catch(const std::exception&)
		{
			return IUIResponder::GenerateResult("Bad regex");
		}
		Json::Value result = IUIResponder::GenerateResult("Success");
		size_t matched = 0;
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto& [name, port] : ports)
			if(std::regex_match(name, re) && port->ServesDevicesOrPoints())
			{
				++matched;
				result["Tags"][name] = port->Tags();
			}
		if(matched == 0)
			return IUIResponder::GenerateResult("No ICCP port that serves devices matched");
		return result;
	}
};

//One control object shared by all such ports; it is released with the last of them (it holds the ODC io service)
std::shared_ptr<IUIResponder> AcquireControl()
{
	static std::mutex mtx;
	static std::weak_ptr<IUIResponder> weak;
	std::lock_guard<std::mutex> lock(mtx);
	auto sp = weak.lock();
	if(!sp)
	{
		sp = std::make_shared<ICCPControl>();
		weak = sp;
	}
	return sp;
}
}

ICCPPortBase::ICCPPortBase(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool listener):
	DataPort(aName, aConfFilename, aConfOverrides),
	tcpListener(listener)
{
	pConf = std::make_unique<ICCPPortConf>();
	static_cast<ICCPPortConf*>(pConf.get())->tcpListener = listener;
	//the configuration is read here: ProcessElements() is this class's own, the schema does not depend on the port type
	ProcessFile();
	if(Conf().HasServerSide() || Conf().HasControls(ControlDirection::Operate))
	{
		control = AcquireControl();
		static_cast<ICCPControl&>(*control).Add(this);
	}
}

std::pair<std::string,const IUIResponder*> ICCPPortBase::GetUIResponder()
{
	if(!control)
		return std::pair<std::string,const IUIResponder*>("", nullptr);
	return std::pair<std::string,const IUIResponder*>("ICCPControl", control.get());
}

void ICCPPortBase::RaiseCondition(uint8_t condition, int64_t eventCode, const std::string& transferSet)
{
	Post([condition, eventCode, transferSet](ICCPPortBase& p)
		{
			if(p.assoc && !p.assoc->closed && p.assoc->responder)
				p.assoc->responder->RaiseCondition(condition, eventCode, transferSet);
		});
}

// ---------------------------------------------------------------------------------------------
// optional Block 5 parts: device tags and the Local Reset action
// ---------------------------------------------------------------------------------------------

std::string ICCPPortBase::LocalReset(const std::string& control, const std::optional<tase2::DeviceTag>& tag, const std::string& reason)
{
	const IccpControlConf* c = Conf().FindControl(control, ControlDirection::Serve);
	if(c == nullptr)
		return "this port does not serve a control device named '" + control + "'";
	if(tag && !c->taggable)
		return "control '" + control + "' is not taggable (Taggable is not set): it has no tag to change";
	if(!tase2::ValidTagReason(reason))
		return "the reason must be at most " + std::to_string(tase2::MaxTagReasonLength) + " characters of visible ASCII";
	if(!pStrand)
		return "the port is not built";
	const size_t index = static_cast<size_t>(c - Conf().controls.data());
	Post([index, tag, reason](ICCPPortBase& p) { p.DoLocalReset(index, tag, reason); });
	return "";
}

std::string ICCPPortBase::SetLocalTag(const std::string& control, tase2::DeviceTag tag, const std::string& reason)
{
	const IccpControlConf* c = Conf().FindControl(control, ControlDirection::Serve);
	if(c == nullptr)
		return "this port does not serve a control device named '" + control + "'";
	if(!c->taggable)
		return "control '" + control + "' is not taggable (Taggable is not set): it has no tag to change";
	if(!tase2::ValidTagReason(reason))
		return "the reason must be at most " + std::to_string(tase2::MaxTagReasonLength) + " characters of visible ASCII";
	if(!pStrand)
		return "the port is not built";
	const size_t index = static_cast<size_t>(c - Conf().controls.data());
	Post([index, tag, reason](ICCPPortBase& p) { p.DoSetLocalTag(index, tag, reason); });
	return "";
}

void ICCPPortBase::DoLocalReset(size_t control, const std::optional<tase2::DeviceTag>& tag, const std::string& reason)
{
	const auto& c = Conf().controls[control];
	//the device state is the association's: with no association every device is idle already
	bool wasArmed = false;
	if(assoc && !assoc->closed && assoc->responder)
		wasArmed = assoc->responder->LocalReset(control);
	if(!wasArmed)
		Log(stack::LogLevel::Info, "control " + c.name + ": local reset: the device was not ARMED, so its state is unchanged");
	if(tag)
		DoSetLocalTag(control, *tag, reason);
}

void ICCPPortBase::DoSetLocalTag(size_t control, tase2::DeviceTag tag, const std::string& reason)
{
	const auto& c = Conf().controls[control];
	model->SetTag(control, tag, reason, std::nullopt); //set locally: no client owns it
	model->CountTag(tase2::ResponderModel::TagEvent::LocalSet);
	Log(stack::LogLevel::Info, "control " + c.name + ": tag set locally: " + tase2::ToString(tag) + (reason.empty() ? "" : " (" + reason + ")") +
		" (a tag change is not reported to the peer: there is no change reporting for tags)");
}

Json::Value ICCPPortBase::Tags() const
{
	Json::Value list(Json::arrayValue);
	if(!model)
		return list;
	const auto& controls = Conf().controls;
	for(size_t i = 0; i < controls.size(); ++i)
	{
		const auto& c = controls[i];
		if(!c.taggable || c.direction != ControlDirection::Serve)
			continue;
		const auto t = model->TagOf(i);
		Json::Value j;
		j["Control"] = c.name;
		j["Scope"] = c.scope == tase2::Scope::Icc ? "ICC" : "VCC";
		if(c.scope == tase2::Scope::Icc)
			j["Domain"] = c.domain;
		j["Tag"] = tase2::ToString(t.tag);
		j["Reason"] = t.reason;
		j["SetBy"] = t.tag == tase2::DeviceTag::NoTag ? "" : (t.owner ? "Client" : "Local");
		list.append(j);
	}
	return list;
}

std::string ICCPPortBase::GetPeerTag(const std::string& control)
{
	const IccpControlConf* c = Conf().FindControl(control, ControlDirection::Operate);
	if(c == nullptr)
		return "this port does not operate a control device of the peer named '" + control + "'";
	if(!associationUp.load())
		return "the association is not up";
	Post([c](ICCPPortBase& p)
		{
			if(!p.assoc || p.assoc->closed || !p.assoc->client)
			{
				p.Log(stack::LogLevel::Error, "control " + c->name + ": Get Tag not started: there is no association");
				return;
			}
			p.assoc->client->GetTag(*c, nullptr);
		});
	return "";
}

std::string ICCPPortBase::SetPeerTag(const std::string& control, tase2::DeviceTag tag, const std::string& reason)
{
	const IccpControlConf* c = Conf().FindControl(control, ControlDirection::Operate);
	if(c == nullptr)
		return "this port does not operate a control device of the peer named '" + control + "'";
	if(!tase2::ValidTagReason(reason))
		return "the reason must be at most " + std::to_string(tase2::MaxTagReasonLength) + " characters of visible ASCII";
	if(!associationUp.load())
		return "the association is not up";
	Post([c, tag, reason](ICCPPortBase& p)
		{
			if(!p.assoc || p.assoc->closed || !p.assoc->client)
			{
				p.Log(stack::LogLevel::Error, "control " + c->name + ": Set Tag not started: there is no association");
				return;
			}
			p.assoc->client->SetTag(*c, tag, reason, nullptr);
		});
	return "";
}

ICCPPortBase::~ICCPPortBase()
{
	if(control)
		static_cast<ICCPControl&>(*control).Remove(this);
	//Explicit order. Every handler that can reach this object holds only a weak_ptr to it (Post(), the socket and timer
	//callbacks), and none can run once the destructor has started, so what remains is to stop the sources:
	//1. timers, so nothing new is scheduled
	if(pTickTimer)
		pTickTimer->cancel();
	if(pReopenTimer)
		pReopenTimer->cancel();
	if(pDisableTimer)
		pDisableTimer->cancel();
	//2. the socket: Close() then destroy, the socket manager waits for its outstanding handlers
	if(pSockMan)
	{
		pSockMan->Close();
		pSockMan.reset();
	}
	//3. the association (it refers to the model), then the model; the strand goes with the other members
	assoc.reset();
	modelView = nullptr;
	model.reset();
}

void ICCPPortBase::Log(stack::LogLevel level, const std::string& text) const
{
	//::Log is the LogHelpers instance (Log.h); the member of the same name hides it here
	switch(level)
	{
		case stack::LogLevel::Trace: ::Log.Trace("{}: {}", Name, text); break;
		case stack::LogLevel::Debug: ::Log.Debug("{}: {}", Name, text); break;
		case stack::LogLevel::Info: ::Log.Info("{}: {}", Name, text); break;
		case stack::LogLevel::Warn: ::Log.Warn("{}: {}", Name, text); break;
		case stack::LogLevel::Error: ::Log.Error("{}: {}", Name, text); break;
	}
}

void ICCPPortBase::ProcessElements(const Json::Value& JSONRoot)
{
	//ConfigParser calls this once per configuration source (the file, any inherited files, then the overrides).
	//Each call carries only the keys of its source, and Load() rebuilds the whole configuration, so merge them first:
	//later sources replace the top-level keys of earlier ones.
	//A source that could not be parsed (a JSON syntax error) arrives as a value that is not an object: the sources after it
	//must not make the configuration valid again (it would run on defaults).
	const bool unreadable = !JSONRoot.isObject();
	if(unreadable)
		sourceUnreadable = true;
	if(JSONRoot.isObject() && mergedConf.isObject())
		for(const auto& key : JSONRoot.getMemberNames())
			mergedConf[key] = JSONRoot[key];
	else
		mergedConf = JSONRoot;
	auto* conf = static_cast<ICCPPortConf*>(pConf.get());
	configErrors = conf->Load(mergedConf, tcpListener);
	if(sourceUnreadable && !unreadable)
		configErrors.insert(configErrors.begin(), "a configuration file could not be read (a JSON syntax error, see the error above): the port stays disabled");
	for(const auto& e : configErrors)
		Log(stack::LogLevel::Error, "configuration error: " + e);
}

void ICCPPortBase::Post(std::function<void(ICCPPortBase&)> f)
{
	if(!pStrand)
		return;
	auto weak = weak_from_this();
	pStrand->post([weak, f = std::move(f)]()
		{
			if(auto sp = weak.lock())
				f(static_cast<ICCPPortBase&>(*sp));
		});
}

void ICCPPortBase::Build()
{
	if(built)
		return;
	built = true;
	pStrand = pIOS->make_strand();
	pTickTimer = pIOS->make_steady_timer();
	pReopenTimer = pIOS->make_steady_timer();
	pDisableTimer = pIOS->make_steady_timer();
	model = std::make_unique<tase2::ResponderModel>(Conf(), Conf().HasServerSide());
	modelView = model.get();
	if(!ConfigValid())
		return;

	const auto& c = Conf();
	for(const auto& w : c.warnings)
		Log(stack::LogLevel::Warn, w);
	if(c.tlsConf.enabled)
	{
		SetTlsStatus("Idle");
		if(c.tlsContext)
			for(const auto& w : c.tlsContext->Warnings())
				Log(stack::LogLevel::Warn, "TLS: " + w);
	}
	auto weak = weak_from_this();
	if(c.maceConf.enabled)
	{
		SetMaceStatus([&]
			{
				mace::Status st;
				st.enabled = true;
				return st;
			}());
		if(c.maceContext)
			for(const auto& w : c.maceContext->Warnings())
				Log(stack::LogLevel::Warn, "MACE: " + w);
	}
	auto readCallback = [weak](buf_t& buf)
				  {
					  std::string bytes(asio::buffers_begin(buf.data()), asio::buffers_end(buf.data()));
					  buf.consume(buf.size());
					  if(auto sp = weak.lock())
						  static_cast<ICCPPortBase&>(*sp).Post([bytes = std::move(bytes)](ICCPPortBase& p) { p.OnTransportData(bytes); });
				  };
	auto stateCallback = [](bool) {};
	auto logCallback = [weak](const std::string& level, const std::string& msg)
				 {
					 if(auto sp = weak.lock())
					 {
						 auto& p = static_cast<ICCPPortBase&>(*sp);
						 //errors from the socket while the port is being disabled (e.g. the accept being cancelled) are expected: not errors.
						 //(What the TLS session logs, "TLS: ...", is always reported as it is.)
						 const bool expected = !p.enabled && msg.compare(0, 5, "TLS: ") != 0;
						 p.Log(level == "error" ? (expected ? stack::LogLevel::Debug : stack::LogLevel::Error) :
							 (level == "warn" || level == "warning" ? stack::LogLevel::Warn : (level == "info" ? stack::LogLevel::Info : stack::LogLevel::Debug)), msg);
					 }
				 };
	//TLS session on each TCP connection when TLS is enabled
	if(c.tlsConf.enabled)
		pSockMan = std::make_unique<TCPSessionManager>(pIOS, tcpListener, c.ip, c.port, c.tlsContext, "" /*the context's ServerName, else the IP*/,
			readCallback, stateCallback, std::numeric_limits<size_t>::max(), true, c.tcpConnectRetryMs, c.tcpThrottleBitrate, c.tcpThrottleChunksize,
			c.tcpThrottleWriteDelayMs, logCallback);
	else
		pSockMan = std::make_unique<TCPSessionManager>(pIOS, tcpListener, c.ip, c.port, readCallback, stateCallback,
			std::numeric_limits<size_t>::max(), true, c.tcpConnectRetryMs, c.tcpThrottleBitrate, c.tcpThrottleChunksize,
			c.tcpThrottleWriteDelayMs, logCallback);
	TCPSessionManager::SessionEvents events;
	events.onConnection = [weak](bool connected, uint64_t serial)
				    {
					    if(auto sp = weak.lock())
						    static_cast<ICCPPortBase&>(*sp).Post([connected, serial](ICCPPortBase& p) { p.OnTransportState(connected, serial); });
				    };
	if(c.tlsConf.enabled)
	{
		events.onEstablished = [weak](const tls::PeerInfo& peer, uint64_t serial)
					     {
						     if(auto sp = weak.lock())
							     static_cast<ICCPPortBase&>(*sp).Post([peer, serial](ICCPPortBase& p)
								     {
									     if(p.SerialIsCurrent(serial))
										     p.OnTlsEstablished(peer);
								     });
					     };
		events.onEnded = [weak](const TCPSessionManager::SessionEnd& end)
				     {
					     if(auto sp = weak.lock())
						     static_cast<ICCPPortBase&>(*sp).Post([end](ICCPPortBase& p) { p.OnTlsSessionEnded(end); });
				     };
		pSockMan->SetTickPeriod(std::chrono::milliseconds(50)); //for the handshake timeout and the renegotiation interval
		//after a failed handshake (or a close by the peer) the port decides when to connect again, with a back-off, see ScheduleReopen()
		pSockMan->SetReconnectAfterSessionEnd(false);
	}
	pSockMan->SetSessionEvents(events);
}

void ICCPPortBase::Enable()
{
	if(!built)
		Build();
	if(!ConfigValid())
	{
		Log(stack::LogLevel::Error, "not enabled: the configuration is invalid");
		return;
	}
	if(enabled)
		return;
	enabled = true;
	disabling = false;
	PublishEvent(ConnectState::PORT_UP);
	SocketOpen();
	Post([](ICCPPortBase& p) { p.ScheduleTick(); });
}

void ICCPPortBase::Disable()
{
	if(!built || !enabled)
		return;
	enabled = false;
	Post([](ICCPPortBase& p)
		{
			p.disabling = true;
			p.pReopenTimer->cancel();
			if(p.assoc && !p.assoc->closed)
			{
				auto conclude = [&p]()
						    {
							    if(p.assoc && !p.assoc->closed)
								    p.assoc->conn->Conclude();
						    };
				if(p.assoc->client)
					p.assoc->client->Shutdown(conclude);
				else
					conclude();
				const auto& c = p.Conf();
				p.pDisableTimer->expires_from_now(std::chrono::milliseconds(c.releaseTimeoutMs + c.requestTimeoutMs + 500));
				auto weak = p.weak_from_this();
				p.pDisableTimer->async_wait(p.pStrand->wrap([weak](std::error_code ec)
					{
						if(ec)
							return;
						if(auto sp = weak.lock())
							static_cast<ICCPPortBase&>(*sp).ForceClose();
					}));
			}
			else
			{
				p.FoldControlStats();
				p.assoc.reset();
				p.NewEpoch(false);
				p.disabling = false;
				p.SocketClose();
				p.PublishEvent(ConnectState::PORT_DOWN);
			}
		});
}

void ICCPPortBase::ForceClose()
{
	if(assoc && !assoc->closed)
		assoc->conn->Abort("the release did not complete");
}

void ICCPPortBase::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(::Log.ShouldLog(spdlog::level::trace))
		::Log.Trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);
	if(!enabled || !pStrand)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}
	Post([event, pStatusCallback](ICCPPortBase& p) { p.HandleEvent(event, pStatusCallback); });
}

void ICCPPortBase::PublishConnection(ConnectState state)
{
	PublishEvent(state);
}

void ICCPPortBase::MarkConnected()
{
	if(!assoc || assoc->closed || assoc->published)
		return;
	assoc->published = true;
	associationUp = true;
	PublishConnection(ConnectState::CONNECTED);
}

// ---------------------------------------------------------------------------------------------
// transport and association
// ---------------------------------------------------------------------------------------------

void ICCPPortBase::OnTransportState(bool connected, uint64_t serial)
{
	transportOpen = connected;
	NewEpoch(connected);
	if(connected)
	{
		liveSerial = serial;
	}
	if(connected)
	{
		//The previous connection's association is told the transport is gone; this writes nothing (its callbacks are of an older epoch).
		//Abort() is for a live association on a live connection only.
		if(assoc)
		{
			if(!assoc->closed)
			{
				Log(stack::LogLevel::Warn, "a new transport connection arrived while the association of the previous one was still up: it is ended, nothing is written for it");
				assoc->conn->OnTransportClosed();
			}
			FoldControlStats();
			assoc.reset();
		}
		if(!enabled || disabling)
		{
			DropTransport();
			return;
		}
		if(Conf().tlsConf.enabled)
			SetTlsStatus("Handshaking"); //the association starts when the handshake completes
		else
			StartAssociation();
		return;
	}
	if(assoc && !assoc->closed)
		assoc->conn->OnTransportClosed();
}

void ICCPPortBase::OnTransportData(const std::string& bytes)
{
	//On a TLS port these are the decrypted bytes
	if(Conf().tlsConf.enabled && !EpochIsCurrent(transportEpoch))
		return;
	Trace("received", reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
	if(assoc && !assoc->closed)
		assoc->conn->OnData(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

void ICCPPortBase::Trace(const char* direction, const uint8_t* data, size_t length) const
{
	if(!Conf().traceHex)
		return;
	static const char digits[] = "0123456789abcdef";
	const size_t shown = std::min(length, Conf().traceHexMaxBytes);
	std::string hex;
	hex.reserve(shown * 2);
	for(size_t i = 0; i < shown; ++i)
	{
		hex.push_back(digits[data[i] >> 4]);
		hex.push_back(digits[data[i] & 0x0F]);
	}
	if(shown < length)
		hex += "... (" + std::to_string(length - shown) + " more bytes)";
	Log(stack::LogLevel::Debug, std::string("trace ") + direction + " " + std::to_string(length) + " bytes: " + hex);
}

void ICCPPortBase::DropTransport()
{
	wantReopen = true;
	if(Conf().tlsConf.enabled && transportLive)
	{
		//close_notify is sent before the close: that is the status from now on, unless the session failed or the peer closed it (set by OnTlsDown)
		std::string state;
		{
			std::lock_guard<std::mutex> lock(tlsStatusMtx);
			state = tlsStatus["State"].asString();
		}
		if(state == "Handshaking" || state == "Established")
			SetTlsStatus("Closed");
	}
	NewEpoch(false); //nothing more is written for it
	SocketClose();   //(on a TLS port: close_notify, written to THIS connection before it is closed)
}

void ICCPPortBase::SocketOpen()
{
	if(pSockMan)
		pSockMan->Open();
}

void ICCPPortBase::SocketClose()
{
	if(pSockMan)
		pSockMan->Close();
}

uint64_t ICCPPortBase::SocketSerial() const
{
	return pSockMan ? pSockMan->ConnectionSerial() : 0;
}

void ICCPPortBase::NewEpoch(bool live)
{
	++transportEpoch;
	transportLive = live;
	epochBytesWritten = 0;
	Log(stack::LogLevel::Debug, "transport epoch " + std::to_string(transportEpoch) + (live ? ": connection up" : ": no connection"));
}

namespace
{
//what the first octets of a write are, for the trace
std::string DescribeFirstBytes(const uint8_t* d, size_t n)
{
	char hex[8];
	std::snprintf(hex, sizeof(hex), "0x%02X", n ? d[0] : 0);
	std::string s = std::string("first byte ") + hex;
	if(n >= 1 && d[0] == 0x16)
		s += " (TLS handshake record)";
	else if(n >= 1 && d[0] == 0x15)
		s += " (TLS alert record)";
	else if(n >= 1 && d[0] == 0x17)
		s += " (TLS application data record)";
	else if(n >= 1 && d[0] == 0x14)
		s += " (TLS change cipher spec record)";
	else if(n >= 6 && d[0] == 0x03 && d[1] == 0x00)
	{
		const char* type = d[5] == 0xE0 ? "COTP CR" : (d[5] == 0xD0 ? "COTP CC" : (d[5] == 0x80 ? "COTP DR" : (d[5] == 0xF0 ? "COTP DT" : "TPKT")));
		s += std::string(" (") + type + ")";
	}
	return s;
}
}

void ICCPPortBase::WriteToSocket(uint64_t epoch, const uint8_t* data, size_t length)
{
	if(!pSockMan || length == 0)
		return;
	if(!EpochIsCurrent(epoch))
	{
		statStrayDropped += length;
		Log(stack::LogLevel::Debug, "dropped " + std::to_string(length) + " bytes written for the connection of epoch " + std::to_string(epoch) +
			" (" + (transportLive ? "now epoch " + std::to_string(transportEpoch) : std::string("no connection")) + "): " + DescribeFirstBytes(data, length));
		return;
	}
	if(epochBytesWritten == 0 && !guardsDisabled)
	{
		//The first bytes on a plain connection: an initiator sends a COTP connection request, a listener a connection confirm (or refusal).
		//Anything else belongs to some other connection or is a bug: never put it on the wire.
		const bool ok = length >= 6 && data[0] == 0x03 && data[1] == 0x00 && (tcpListener ? (data[5] == 0xD0 || data[5] == 0x80) : data[5] == 0xE0);
		if(!ok)
		{
			statStrayDropped += length;
			Log(stack::LogLevel::Error, "internal error: refusing to write " + std::to_string(length) + " bytes as the first bytes of a connection (" +
				DescribeFirstBytes(data, length) + "): " + (tcpListener ? "a plain listener starts with a COTP connection confirm" : "a plain initiator starts with a COTP connection request"));
			return;
		}
	}
	if(Conf().traceHex)
		TraceSocketWrite(epoch, epochBytesWritten == 0, data, length);
	epochBytesWritten += length;
	ber::Bytes copy(data, data + length);
	//bound to the connection (serial) this epoch is about: if the socket has moved on to a newer one meanwhile, it is dropped there
	if(guardsDisabled)
		pSockMan->Write(std::move(copy));
	else
		pSockMan->Write(std::move(copy), liveSerial);
}

void ICCPPortBase::TraceSocketWrite(uint64_t epoch, bool first, const uint8_t* data, size_t length) const
{
	Log(stack::LogLevel::Debug, "trace socket write, epoch " + std::to_string(epoch) + ", " +
		std::to_string(length) + " bytes, " + DescribeFirstBytes(data, length) + (first ? ", the first on this connection" : ""));
}

void ICCPPortBase::TestSimulateTransportState(bool connected)
{
	Post([connected](ICCPPortBase& p) { p.OnTransportState(connected, p.SocketSerial()); });
}

void ICCPPortBase::TestPost(std::function<void(ICCPPortBase&)> f)
{
	Post(std::move(f));
}

void ICCPPortBase::TestAbortAssociation(const std::string& reason)
{
	if(assoc && !assoc->closed)
		assoc->conn->Abort(reason);
}

void ICCPPortBase::TestDisableStaleGuards()
{
	guardsDisabled = true;
}

void ICCPPortBase::StartAssociation()
{
	FoldControlStats();
	assoc = std::make_unique<Association>();
	//the association is initiated by the TCP initiator (COTP CR, session CN, ACSE AARQ, MMS Initiate), whatever the service role
	const auto role = tcpListener ? stack::Role::Responder : stack::Role::Initiator;
	stack::Config sc = Conf().StackConfig();
	sc.clock = timebase.steady;

	stack::Callbacks cb;
	const uint64_t epoch = transportEpoch; //this association belongs to this connection
	cb.send = [this, epoch](const ber::Bytes& b)
		    {
			    if(!EpochIsCurrent(epoch))
			    {
				    statStrayDropped += b.size();
				    Log(stack::LogLevel::Debug, "dropped " + std::to_string(b.size()) + " bytes the association produced for the connection of epoch " + std::to_string(epoch));
				    return;
			    }
			    Trace("sent", b.data(), b.size());
			    if(Conf().tlsConf.enabled)
			    {
				    //never plaintext on a TLS port: the session encrypts it, and drops it if this connection's session is gone
				    if(pSockMan)
				    {
					    if(guardsDisabled)
						    pSockMan->Write(b);
					    else
						    pSockMan->Write(b, liveSerial);
				    }
				    return;
			    }
			    WriteToSocket(epoch, b.data(), b.size());
		    };
	cb.closeTransport = [this, epoch]()
				  {
					  if(!EpochIsCurrent(epoch))
					  {
						  Log(stack::LogLevel::Debug, "not closing the transport for the association of epoch " + std::to_string(epoch) + ": that connection is gone");
						  return;
					  }
					  DropTransport();
				  };
	cb.onAssociated = [this]()
				{
					++statAssociations;
					Log(stack::LogLevel::Info, "association established");
					if(assoc)
						OnAssociationReady(*assoc);
				};
	//who the peer is, for the owner of a VCC device's tag (the responder side learns it from the association request; accepting is what the stack does without this callback)
	cb.onAssociateRequest = [this](const stack::AssociateRequestInfo& info)
					{
						std::string identity;
						if(info.callingApTitle)
							for(size_t i = 0; i < info.callingApTitle->size(); ++i)
								identity += (i ? "." : "") + std::to_string((*info.callingApTitle)[i]);
						if(info.callingAeQualifier)
							identity += "/" + std::to_string(*info.callingAeQualifier);
						if(assoc && assoc->responder)
							assoc->responder->SetClientIdentity(identity);
						return true;
					};
	cb.onClosed = [this](stack::EndReason r, const std::string& t) { OnAssociationClosed(r, t); };
	cb.onRequest = [this](uint32_t id, const mms::ConfirmedServiceRequest& req)
			   {
				   if(assoc && assoc->responder)
					   assoc->responder->OnRequest(id, req);
				   else if(assoc)
					   assoc->conn->RejectRequest(id);
			   };
	cb.onInformationReport = [this](const mms::InformationReport& r)
					 {
						 if(!assoc)
							 return;
						 //the peer's acknowledgement of a critical report of ours is for the server side; any other report is a transfer report for the client side
						 if(assoc->responder && assoc->responder->OnInformationReport(r))
							 return;
						 if(assoc->client)
							 assoc->client->OnReport(r);
					 };
	cb.log = [this](stack::LogLevel l, const std::string& t) { Log(l, t); };
	cb.onMaceStatus = [this](const mace::Status& st) { SetMaceStatus(st); };

	assoc->conn = std::make_unique<stack::Connection>(role, sc, std::move(cb));
	auto logFn = [this](stack::LogLevel l, const std::string& t) { Log(l, t); };
	assoc->responder = std::make_unique<tase2::ResponderSession>(*model, *assoc->conn, timebase, logFn);
	assoc->responder->SetControlHandler([this](const tase2::ControlOperation& op, std::function<void(CommandStatus)> done) { PublishControl(op, std::move(done)); });
	CreateClientSession(*assoc);
	assoc->conn->Start();
}

void ICCPPortBase::OnAssociationClosed(stack::EndReason reason, const std::string& text)
{
	if(!assoc)
		return;
	assoc->closed = true;
	associationUp = false;
	if(assoc->responder)
		assoc->responder->AssociationEnded(); //the critical reports still unacknowledged are forgotten with the association
	if(assoc->published)
	{
		assoc->published = false;
		PublishConnection(ConnectState::DISCONNECTED);
	}
	const bool unremarkable = reason == stack::EndReason::Released || reason == stack::EndReason::ReleasedByPeer || reason == stack::EndReason::TransportLost;
	Log(unremarkable ? stack::LogLevel::Info : stack::LogLevel::Warn, std::string("association ended: ") + stack::ToString(reason) + " (" + text + ")");
	//the connection is still on the call stack: tear it down afterwards
	Post([](ICCPPortBase& p)
		{
			if(p.assoc && p.assoc->closed)
			{
				p.FoldControlStats();
				p.assoc.reset();
			}
			p.pDisableTimer->cancel();
			if(p.disabling)
			{
				p.disabling = false;
				p.SocketClose();
				p.PublishEvent(ConnectState::PORT_DOWN);
			}
			else if(p.enabled && p.wantReopen)
				p.ScheduleReopen();
		});
}

void ICCPPortBase::ScheduleReopen()
{
	wantReopen = false;
	const auto& c = Conf();
	const auto delay = tcpListener ? std::chrono::milliseconds(100) : std::chrono::milliseconds(std::max<uint16_t>(c.tcpConnectRetryMs, 1000));
	//repeated TLS handshake failures (a wrong certificate does not get better by itself): back off, up to 30 s
	const auto tlsDelay = (tlsFailures > 1 && !tcpListener) ? std::min(delay * (1 << std::min(tlsFailures - 1, 5u)), std::chrono::milliseconds(30000)) : delay;
	pReopenTimer->expires_from_now(tlsDelay);
	auto weak = weak_from_this();
	pReopenTimer->async_wait(pStrand->wrap([weak](std::error_code ec)
		{
			if(ec)
				return;
			if(auto sp = weak.lock())
			{
				auto& p = static_cast<ICCPPortBase&>(*sp);
				if(p.enabled && !p.disabling && !p.assoc)
					p.SocketOpen();
			}
		}));
}

void ICCPPortBase::ScheduleTick()
{
	pTickTimer->expires_from_now(std::chrono::milliseconds(50));
	auto weak = weak_from_this();
	pTickTimer->async_wait(pStrand->wrap([weak](std::error_code ec)
		{
			if(ec)
				return;
			if(auto sp = weak.lock())
				static_cast<ICCPPortBase&>(*sp).Tick();
		}));
}

void ICCPPortBase::Tick()
{
	if(assoc && !assoc->closed)
	{
		assoc->conn->Tick();
		if(assoc && !assoc->closed)
		{
			assoc->responder->Tick();
			if(assoc->client)
				assoc->client->Tick();
		}
	}
	if(assoc)
	{
		statReportsSent = assoc->responder->ReportsSent();
		if(assoc->client)
		{
			statReportsReceived = assoc->client->ReportsReceived();
			statValuesPublished = assoc->client->ValuesPublished();
			statUnavailable = assoc->client->UnavailablePoints();
		}
	}
	UpdateControlStats();
	if(enabled || assoc)
		ScheduleTick();
}

void ICCPPortBase::UpdateControlStats()
{
	ControlTotals t = controlFolded;
	ReportTotals r = reportFolded;
	size_t outstanding = 0;
	if(assoc)
	{
		if(assoc->responder)
		{
			const auto& c = assoc->responder->ControlCounters();
			t.selectsReceived += c.selects;
			t.operatesReceived += c.operates;
			t.refused += c.refused;
			const auto& s = assoc->responder->ReportCounters();
			r.server.criticalSent += s.criticalSent;
			r.server.acknowledged += s.acknowledged;
			r.server.nacksReceived += s.nacksReceived;
			r.server.ackTimeouts += s.ackTimeouts;
			r.server.ackOverflows += s.ackOverflows;
			r.server.dataFailures += s.dataFailures;
			r.server.tleDiscarded += s.tleDiscarded;
			r.server.blockedSent += s.blockedSent;
			outstanding = assoc->responder->OutstandingCriticalReports();
		}
		if(assoc->client)
		{
			const auto& c = assoc->client->ControlCounters();
			t.selectsSent += c.selects;
			t.operatesSent += c.operates;
			t.failures += c.failures;
			t.tagReads += c.tagReads;
			t.tagWrites += c.tagWrites;
			t.tagFailures += c.tagFailures;
			r.acksSent += assoc->client->ReportAcksSent();
			r.nacksSent += assoc->client->ReportNacksSent();
			r.blockedReceived += assoc->client->BlockedReportsReceived();
			r.blockedUndecodable += assoc->client->BlockedReportsUndecodable();
		}
	}
	statControlSelectsSent = t.selectsSent;
	statControlOperatesSent = t.operatesSent;
	statControlFailures = t.failures;
	statControlSelectsReceived = t.selectsReceived;
	statControlOperatesReceived = t.operatesReceived;
	statControlRefused = t.refused;
	statTagsRead = t.tagReads;
	statTagsWritten = t.tagWrites;
	statTagRequestFailures = t.tagFailures;
	if(model)
	{
		//the tags belong to the device, not to the association: the model's counters are cumulative already
		const auto tc = model->TagCounts();
		statTagsSet = tc.set;
		statLocalTagsSet = tc.localSet;
		statTagRequestsRefused = tc.refused;
		statSelectsRefusedByTag = tc.selectsRefused;
		statOperatesRefusedByTag = tc.operatesRefused;
		statLocalResets = tc.localResets;
	}
	statCriticalSent = r.server.criticalSent;
	statCriticalAcknowledged = r.server.acknowledged;
	statCriticalOutstanding = outstanding;
	statNacksReceived = r.server.nacksReceived;
	statAckTimeouts = r.server.ackTimeouts;
	statAckOverflows = r.server.ackOverflows;
	statDataFailures = r.server.dataFailures;
	statReportsDiscardedTle = r.server.tleDiscarded;
	statReportAcksSent = r.acksSent;
	statReportNacksSent = r.nacksSent;
	statBlockedSent = r.server.blockedSent;
	statBlockedReceived = r.blockedReceived;
	statBlockedUndecodable = r.blockedUndecodable;
}

void ICCPPortBase::FoldControlStats()
{
	UpdateControlStats();
	controlFolded.selectsSent = statControlSelectsSent;
	controlFolded.operatesSent = statControlOperatesSent;
	controlFolded.failures = statControlFailures;
	controlFolded.selectsReceived = statControlSelectsReceived;
	controlFolded.operatesReceived = statControlOperatesReceived;
	controlFolded.refused = statControlRefused;
	controlFolded.tagReads = statTagsRead;
	controlFolded.tagWrites = statTagsWritten;
	controlFolded.tagFailures = statTagRequestFailures;
	reportFolded.server.criticalSent = statCriticalSent;
	reportFolded.server.acknowledged = statCriticalAcknowledged;
	reportFolded.server.nacksReceived = statNacksReceived;
	reportFolded.server.ackTimeouts = statAckTimeouts;
	reportFolded.server.ackOverflows = statAckOverflows;
	reportFolded.server.dataFailures = statDataFailures;
	reportFolded.server.tleDiscarded = statReportsDiscardedTle;
	reportFolded.acksSent = statReportAcksSent;
	reportFolded.nacksSent = statReportNacksSent;
	reportFolded.server.blockedSent = statBlockedSent;
	reportFolded.blockedReceived = statBlockedReceived;
	reportFolded.blockedUndecodable = statBlockedUndecodable;
}

const Json::Value ICCPPortBase::GetStatus() const
{
	Json::Value s;
	s["Association"] = associationUp.load() ? "UP" : "DOWN";
	s["ServiceRole"] = Conf().role == ServiceRole::Client ? "Client" : (Conf().role == ServiceRole::Server ? "Server" : "Peer");
	s["TCP"] = tcpListener ? "Listener" : "Initiator";
	if(Conf().tlsConf.enabled)
	{
		std::lock_guard<std::mutex> lock(tlsStatusMtx);
		s["TLS"] = tlsStatus;
	}
	else
		s["TLS"]["Enabled"] = false;
	{
		std::lock_guard<std::mutex> lock(maceStatusMtx);
		Json::Value m;
		m["Enabled"] = maceStatus.enabled;
		m["State"] = mace::ToString(maceStatus.state);
		if(maceStatus.enabled)
		{
			m["PeerSubject"] = maceStatus.peerSubject;
			m["PeerNotAfter"] = maceStatus.peerNotAfter;
			m["SigningTime"] = maceStatus.signingTime != 0 ? mace::IsoTime(maceStatus.signingTime) : "";
			m["PeerSigningTime"] = maceStatus.peerSigningTime != 0 ? mace::IsoTime(maceStatus.peerSigningTime) : "";
			m["LastError"] = maceStatus.lastError;
		}
		s["MACE"] = m;
	}
	if(Conf().HasClientSide())
	{
		//how far the provisioning of each data set got on the current (or last) association
		std::lock_guard<std::mutex> lock(provisioningMtx);
		s["Provisioning"] = provisioning;
	}
	return s;
}

const Json::Value ICCPPortBase::GetStatistics() const
{
	Json::Value s;
	s["Associations"] = static_cast<Json::UInt64>(statAssociations.load());
	s["ReportsSent"] = static_cast<Json::UInt64>(statReportsSent.load());
	s["ReportsReceived"] = static_cast<Json::UInt64>(statReportsReceived.load());
	s["ValuesPublished"] = static_cast<Json::UInt64>(statValuesPublished.load());
	s["UnavailablePoints"] = static_cast<Json::UInt64>(statUnavailable.load());
	if(Conf().HasServerSide())
	{
		//Critical reports and TLE [S]: the reports this port sent that the peer has to acknowledge (the transfer sets it started with Critical TRUE), and what the
		//peer's acknowledgements, NACKs and the time limits did to them. DataFailures is the standard's Data_failure condition (a NACK, a timeout or an overflow):
		//this port has no event conditions, so it is counted and logged.
		s["CriticalReportsSent"] = static_cast<Json::UInt64>(statCriticalSent.load());
		s["CriticalReportsAcknowledged"] = static_cast<Json::UInt64>(statCriticalAcknowledged.load());
		s["CriticalReportsOutstanding"] = static_cast<Json::UInt64>(statCriticalOutstanding.load());
		s["ReportNacksReceived"] = static_cast<Json::UInt64>(statNacksReceived.load());
		s["ReportAckTimeouts"] = static_cast<Json::UInt64>(statAckTimeouts.load());
		s["ReportAckOverflows"] = static_cast<Json::UInt64>(statAckOverflows.load());
		s["DataFailures"] = static_cast<Json::UInt64>(statDataFailures.load());
		s["ReportsDiscardedTLE"] = static_cast<Json::UInt64>(statReportsDiscardedTle.load());
		s["BlockedReportsSent"] = static_cast<Json::UInt64>(statBlockedSent.load());
	}
	if(Conf().HasClientSide())
	{
		s["ReportAcksSent"] = static_cast<Json::UInt64>(statReportAcksSent.load());
		s["ReportNacksSent"] = static_cast<Json::UInt64>(statReportNacksSent.load());
		s["BlockedReportsReceived"] = static_cast<Json::UInt64>(statBlockedReceived.load());
		s["BlockedReportsUndecodable"] = static_cast<Json::UInt64>(statBlockedUndecodable.load());
	}
	if(!Conf().controls.empty())
	{
		//TASE.2 Block 5 [S]: what we sent to the peer's devices (client side) and what the peer did to ours (server side)
		s["ControlSelectsSent"] = static_cast<Json::UInt64>(statControlSelectsSent.load());
		s["ControlOperatesSent"] = static_cast<Json::UInt64>(statControlOperatesSent.load());
		s["ControlFailures"] = static_cast<Json::UInt64>(statControlFailures.load());
		s["ControlSelectsReceived"] = static_cast<Json::UInt64>(statControlSelectsReceived.load());
		s["ControlOperatesReceived"] = static_cast<Json::UInt64>(statControlOperatesReceived.load());
		s["ControlRefused"] = static_cast<Json::UInt64>(statControlRefused.load());
	}
	if(Conf().HasTaggableControls())
	{
		//device tags [S]: what the peer's Get Tag, Set Tag, select and operate did to the tags of our devices; the tag counters are cumulative like the others.
		//SelectsRefusedByTag and OperatesRefusedByTag are included in ControlRefused.
		s["TagsSet"] = static_cast<Json::UInt64>(statTagsSet.load());
		s["LocalTagsSet"] = static_cast<Json::UInt64>(statLocalTagsSet.load());
		s["TagRequestsRefused"] = static_cast<Json::UInt64>(statTagRequestsRefused.load());
		s["ControlSelectsRefusedByTag"] = static_cast<Json::UInt64>(statSelectsRefusedByTag.load());
		s["ControlOperatesRefusedByTag"] = static_cast<Json::UInt64>(statOperatesRefusedByTag.load());
		s["LocalResets"] = static_cast<Json::UInt64>(statLocalResets.load());
	}
	if(Conf().HasControls(ControlDirection::Operate))
	{
		//Get Tag and Set Tag sent to the peer's devices: the answers that gave a tag, the ones the peer accepted, the failures
		s["TagsRead"] = static_cast<Json::UInt64>(statTagsRead.load());
		s["TagsWritten"] = static_cast<Json::UInt64>(statTagsWritten.load());
		s["TagRequestFailures"] = static_cast<Json::UInt64>(statTagRequestFailures.load());
	}
	return s;
}

// ---------------------------------------------------------------------------------------------
// secure ICCP: TLS (IEC 62351-3) between the TCP connection and the stack
// ---------------------------------------------------------------------------------------------

const Json::Value ICCPPortBase::GetCurrentState() const
{
	//The server side only: the points this port serves (the values the other ports gave it) and its devices. A Client has no such state: the default.
	//May be called from any thread: the configuration is read-only once loaded, the point states are copied under the model's lock and the tags are read under theirs,
	//so nothing here touches the association or waits for the port's strand.
	if(!Conf().HasServerSide())
		return Json::Value();
	try
	{
		const tase2::ResponderModel* m = modelView.load();
		const std::vector<tase2::PointState> states = m ? m->StatesSnapshot() : std::vector<tase2::PointState>();
		const auto now = msSinceEpoch();
		const auto built = m ? m->CreatedAt() : now;

		const auto time_str = since_epoch_to_datetime(now);
		Json::Value ret;
		auto& s = ret[time_str];
		s["Analogs"] = Json::arrayValue;
		s["Binaries"] = Json::arrayValue;
		s["Counters"] = Json::arrayValue;
		s["Controls"] = Json::arrayValue;

		const auto& conf = Conf();
		for(size_t i = 0; i < conf.points.size(); ++i)
		{
			const auto& p = conf.points[i];
			if(p.direction != PointDirection::Serve)
				continue; //the points this port reads from the peer are the client side's
			//a point that was never set looks as the DNP3 port's: no Value, quality RESTART, the time the port was made
			const tase2::PointState st = i < states.size() ? states[i] : tase2::PointState();
			Json::Value j;
			j["Index"] = Json::UInt64(p.index);
			j["Name"] = p.name;
			j["Scope"] = p.scope == tase2::Scope::Icc ? "ICC" : "VCC";
			if(p.scope == tase2::Scope::Icc)
				j["Domain"] = p.domain;
			if(st.known)
			{
				switch(p.kind)
				{
					case PointKind::Binary: j["Value"] = st.binary; break;
					case PointKind::Analog: j["Value"] = st.analog; break;
					case PointKind::Counter: j["Value"] = Json::UInt(st.counter); break;
				}
			}
			j["Quality"] = ToString(st.known ? st.quality : QualityFlags::RESTART);
			j["Timestamp"] = since_epoch_to_datetime(st.known ? st.timestamp : built);
			switch(p.kind)
			{
				case PointKind::Binary: s["Binaries"].append(j); break;
				case PointKind::Analog: s["Analogs"].append(j); break;
				case PointKind::Counter: s["Counters"].append(j); break;
			}
		}
		for(size_t i = 0; i < conf.controls.size(); ++i)
		{
			const auto& c = conf.controls[i];
			if(c.direction != ControlDirection::Serve)
				continue; //the peer's devices that this port operates are the client side's
			Json::Value j;
			j["Index"] = Json::UInt64(c.index);
			j["Name"] = c.name;
			j["Kind"] = c.kind == ControlKind::Binary ? "Binary" : "Analog";
			j["Scope"] = c.scope == tase2::Scope::Icc ? "ICC" : "VCC";
			if(c.scope == tase2::Scope::Icc)
				j["Domain"] = c.domain;
			j["Taggable"] = c.taggable;
			if(c.taggable)
			{
				tase2::ResponderModel::TagState t;
				if(m)
					t = m->TagOf(i);
				else
				{
					t.tag = c.initialTag;
					t.reason = c.initialTagReason;
				}
				j["Tag"] = tase2::ToString(t.tag);
				j["Reason"] = t.reason;
				j["SetBy"] = t.tag == tase2::DeviceTag::NoTag ? "" : (t.owner ? "Client" : "Local");
			}
			s["Controls"].append(j);
		}
		return ret;
	}
	catch(const std::exception& e)
	{
		Log(stack::LogLevel::Debug, std::string("CurrentState: ") + e.what());
	}
	catch(...)
	{}
	return Json::Value();
}

void ICCPPortBase::SetTlsStatus(const std::string& state, const tls::PeerInfo* peer)
{
	Json::Value j;
	j["Enabled"] = true;
	j["Role"] = tcpListener ? "Server" : "Client";
	j["State"] = state;
	if(peer)
	{
		j["Protocol"] = peer->protocol;
		j["Cipher"] = peer->cipher;
		j["Resumed"] = peer->resumed;
		if(peer->certificate)
		{
			j["PeerSubject"] = peer->subject;
			j["PeerNotAfter"] = peer->notAfter;
		}
	}
	std::lock_guard<std::mutex> lock(tlsStatusMtx);
	tlsStatus = std::move(j);
}

void ICCPPortBase::SetMaceStatus(const mace::Status& status)
{
	std::lock_guard<std::mutex> lock(maceStatusMtx);
	maceStatus = status;
}

void ICCPPortBase::OnTlsEstablished(const tls::PeerInfo& peer)
{
	tlsFailures = 0;
	SetTlsStatus("Established", &peer);
	std::string text = "TLS established: " + peer.protocol + " " + peer.cipher + (peer.resumed ? " (resumed session)" : "");
	if(peer.certificate)
		text += ", peer certificate subject \"" + peer.subject + "\" issuer \"" + peer.issuer + "\" serial " + peer.serial +
		        " SHA-256 " + peer.sha256 + " valid until " + peer.notAfter;
	else
		text += ", the peer presented no certificate";
	Log(stack::LogLevel::Info, text);
	//HOOK for IEC 62351-4 (authentication in ACSE, "Stage 2"): the peer's certificate is available in `peer`, and the ACSE layer (Acse.h, its
	//Authentication hook) is where the authentication-value / mechanism-name goes. Only the MACE mechanism (Mace.h) exists so far, independent of TLS.
	StartAssociation(); //the initiator's COTP connection request goes out now, inside the TLS session
}

void ICCPPortBase::OnTlsDown(const std::string& why, bool failed)
{
	if(failed)
	{
		++tlsFailures;
		Log(stack::LogLevel::Error, "TLS: " + why);
		SetTlsStatus("Failed");
	}
	else
	{
		Log(stack::LogLevel::Info, "TLS: " + why);
		SetTlsStatus("Closed");
	}
	DropTransport();
	if(assoc && !assoc->closed)
		assoc->conn->OnTransportClosed(); //ends the association, which schedules the reopen
	else if(!assoc && enabled && !disabling)
		ScheduleReopen();
}

void ICCPPortBase::OnTlsSessionEnded(const TCPSessionManager::SessionEnd& end)
{
	using Kind = TCPSessionManager::SessionEnd::Kind;
	switch(end.kind)
	{
		case Kind::Failed:
			if(SerialIsCurrent(end.serial))
				OnTlsDown(end.reason, true);
			else
				Log(stack::LogLevel::Debug, "TLS: ignoring a failure of the session of an earlier connection: " + end.reason);
			break;
		case Kind::PeerClosed:
			if(SerialIsCurrent(end.serial))
				OnTlsDown("the peer closed the TLS connection (close_notify)", false);
			break;
		case Kind::ConnectionLost:
			//the TCP connection ended under the session: the socket manager connects again by itself
			if(!end.established)
			{
				Log(stack::LogLevel::Error, "TLS: the peer closed the TCP connection during the TLS handshake" +
					std::string(tcpListener ? "" : " (is the peer using TLS on this port?)") + " [" + end.handshakeSummary + "]");
				SetTlsStatus("Failed");
			}
			else
				SetTlsStatus("Down");
			break;
		case Kind::ClosedLocally:
			SetTlsStatus("Closed");
			break;
	}
}

// ---------------------------------------------------------------------------------------------
// the service role
// ---------------------------------------------------------------------------------------------

void ICCPPortBase::CreateClientSession(Association& a)
{
	if(!Conf().HasClientSide())
		return;
	tase2::ClientSession::Callbacks cb;
	cb.onReady = [this]() { MarkConnected(); };
	cb.onFatal = [this](const std::string& why)
			 {
				 if(assoc && !assoc->closed)
					 assoc->conn->Abort(why);
			 };
	cb.onPoint = [this](const IccpPointConf& pt, const tase2::PointState& s)
			 {
				 EventType type = EventType::Binary;
				 switch(pt.kind)
				 {
					 case PointKind::Binary: type = EventType::Binary; break;
					 case PointKind::Analog: type = EventType::Analog; break;
					 case PointKind::Counter: type = EventType::Counter; break;
				 }
				 auto event = std::make_shared<EventInfo>(type, pt.index, Name, s.quality, s.timestamp);
				 switch(pt.kind)
				 {
					 case PointKind::Binary: event->SetPayload<EventType::Binary>(bool(s.binary)); break;
					 case PointKind::Analog: event->SetPayload<EventType::Analog>(double(s.analog)); break;
					 case PointKind::Counter: event->SetPayload<EventType::Counter>(uint32_t(s.counter)); break;
				 }
				 PublishEvent(event);
			 };
	//a point that is not available on the peer is published like a value, with the quality MissingMemberQuality and a neutral value
	cb.onUnavailable = cb.onPoint;
	cb.onProvisioning = [this](const std::vector<tase2::ProvisionStatus>& s) { StoreProvisioning(s); };
	a.client = std::make_unique<tase2::ClientSession>(Conf(), *a.conn, timebase,
		[this](stack::LogLevel l, const std::string& t) { Log(l, t); }, std::move(cb));
	StoreProvisioning(a.client->Status());
}

void ICCPPortBase::StoreProvisioning(const std::vector<tase2::ProvisionStatus>& statuses)
{
	Json::Value list(Json::arrayValue);
	for(const auto& s : statuses)
	{
		Json::Value j;
		j["DataSet"] = s.dataSet;
		j["Domain"] = s.domain;
		if(!s.transferSet.empty())
			j["TransferSet"] = s.transferSet;
		j["State"] = tase2::ToString(s.state);
		j["Members"] = static_cast<Json::UInt64>(s.members);
		if(!s.lastError.empty())
			j["LastError"] = s.lastError;
		if(s.partial)
		{
			j["Partial"] = true;
			j["MembersDefined"] = static_cast<Json::UInt64>(s.membersDefined);
		}
		if(s.probe)
		{
			Json::Value pr;
			pr["Complete"] = s.probe->complete;
			pr["Probed"] = static_cast<Json::UInt64>(s.probe->probed);
			pr["MissingMembers"] = static_cast<Json::UInt64>(s.probe->missing);
			pr["PresentMembers"] = static_cast<Json::UInt64>(s.probe->present);
			pr["PresentButNotReadable"] = static_cast<Json::UInt64>(s.probe->presentWithError);
			if(!s.probe->failure.empty())
				pr["Failure"] = s.probe->failure;
			//every missing member, compactly (the log has the full identification of each)
			Json::Value missing(Json::arrayValue);
			for(const auto& e : s.probe->missingListed)
			{
				Json::Value m;
				m["Position"] = static_cast<Json::UInt64>(e.index); //0-based within the data set
				m["Name"] = e.name;
				m["Kind"] = e.kind;
				if(e.odcIndex)
					m["Index"] = static_cast<Json::UInt64>(*e.odcIndex);
				missing.append(m);
			}
			pr["Missing"] = missing;
			j["Probe"] = pr;
		}
		list.append(j);
	}
	std::lock_guard<std::mutex> lock(provisioningMtx);
	provisioning = std::move(list);
}

void ICCPPortBase::OnAssociationReady(Association& a)
{
	if(a.client)
	{
		//Client, Peer: verify the peer, then provision. The responder side (Peer) already answers the peer's requests meanwhile.
		a.client->OnAssociated();
		return;
	}
	//Server: serve; read the peer's TASE2_Version (a mismatch is warned about) and check its Bilateral_Table_ID if we were told which one to expect
	MarkConnected();
	const auto& c = Conf();
	const bool checking = !c.remoteBilateralTableId.empty() || c.requireMatchingEdition;
	tase2::VerifyPeer(*a.conn, c, false, [this](stack::LogLevel l, const std::string& t) { Log(l, t); },
		[this, checking](bool ok, const std::string& problem, const tase2::AssociationInfo&)
		{
			if(ok)
				return;
			Log(checking ? stack::LogLevel::Warn : stack::LogLevel::Debug, problem);
			if(checking && (Conf().abortOnBilateralTableMismatch || Conf().requireMatchingEdition) && assoc && !assoc->closed)
				assoc->conn->Abort(problem);
		});
}

void ICCPPortBase::HandleEvent(const std::shared_ptr<const EventInfo>& event, const SharedStatusCallback_t& pStatusCallback)
{
	switch(event->GetEventType())
	{
		case EventType::ControlRelayOutputBlock:
		case EventType::AnalogOutputInt16:
		case EventType::AnalogOutputInt32:
		case EventType::AnalogOutputFloat32:
		case EventType::AnalogOutputDouble64:
			HandleControl(event, pStatusCallback);
			return;
		default:
			break;
	}
	PointKind kind;
	bool binary = false;
	double analog = 0;
	uint32_t counter = 0;
	switch(event->GetEventType())
	{
		case EventType::Binary:
			kind = PointKind::Binary;
			binary = event->GetPayload<EventType::Binary>();
			break;
		case EventType::Analog:
			kind = PointKind::Analog;
			analog = event->GetPayload<EventType::Analog>();
			break;
		case EventType::Counter:
			kind = PointKind::Counter;
			counter = event->GetPayload<EventType::Counter>();
			break;
		default:
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
	}
	if(!Conf().HasServerSide())
	{
		//a Client has no points to serve: its points are read from the peer
		(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
		return;
	}
	const auto changed = model->Update(kind, event->GetIndex(), binary, analog, counter, event->GetQuality(), event->GetTimestamp());
	if(changed && assoc && !assoc->closed && assoc->responder)
		assoc->responder->OnPointChanged(*changed);
	(*pStatusCallback)(CommandStatus::SUCCESS);
}

// ---------------------------------------------------------------------------------------------
// TASE.2 Block 5 (device control) [S]
// ---------------------------------------------------------------------------------------------

void ICCPPortBase::HandleControl(const std::shared_ptr<const EventInfo>& event, const SharedStatusCallback_t& pStatusCallback)
{
	const auto type = event->GetEventType();
	const ControlKind kind = type == EventType::ControlRelayOutputBlock ? ControlKind::Binary : ControlKind::Analog;
	const IccpControlConf* control = Conf().FindControl(kind, event->GetIndex(), ControlDirection::Operate);
	if(control == nullptr)
	{
		Log(stack::LogLevel::Debug, std::string(ToString(type)) + " " + std::to_string(event->GetIndex()) + " is not a control this port operates (ignored)");
		(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
		return;
	}
	if(!event->HasPayload())
	{
		Log(stack::LogLevel::Error, "control " + control->name + ": the ODC event has no payload (" + ToString(CommandStatus::FORMAT_ERROR) + ")");
		(*pStatusCallback)(CommandStatus::FORMAT_ERROR);
		return;
	}
	tase2::ClientSession::ControlCommand command;
	switch(type)
	{
		case EventType::ControlRelayOutputBlock:
		{
			const auto code = event->GetPayload<EventType::ControlRelayOutputBlock>().functionCode;
			const auto on = tase2::CommandOnOff(code);
			if(!on)
			{
				Log(stack::LogLevel::Error, "control " + control->name + ": the control code " + ToString(code) + " is not supported (" + ToString(CommandStatus::NOT_SUPPORTED) + ")");
				(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
				return;
			}
			command.on = *on;
			break;
		}
		case EventType::AnalogOutputInt16: command.value = event->GetPayload<EventType::AnalogOutputInt16>().first; break;
		case EventType::AnalogOutputInt32: command.value = event->GetPayload<EventType::AnalogOutputInt32>().first; break;
		case EventType::AnalogOutputFloat32: command.value = event->GetPayload<EventType::AnalogOutputFloat32>().first; break;
		default: command.value = event->GetPayload<EventType::AnalogOutputDouble64>().first; break;
	}
	if(!assoc || assoc->closed || !assoc->client)
	{
		Log(stack::LogLevel::Error, "control " + control->name + " not started: there is no association (" + ToString(CommandStatus::UNDEFINED) + ")");
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}
	assoc->client->Operate(*control, command, [pStatusCallback](CommandStatus status) { (*pStatusCallback)(status); });
}

void ICCPPortBase::PublishControl(const tase2::ControlOperation& op, std::function<void(CommandStatus)> done)
{
	const IccpControlConf& c = *op.control;
	std::shared_ptr<EventInfo> event;
	if(c.kind == ControlKind::Binary)
	{
		event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, c.index, Name, QualityFlags::ONLINE);
		ControlRelayOutputBlock crob;
		crob.functionCode = op.on ? c.codeOn : c.codeOff;
		event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(crob));
	}
	else
	{
		event = std::make_shared<EventInfo>(c.analogEvent, c.index, Name, QualityFlags::ONLINE);
		const double v = op.value;
		auto fits = [&](double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; };
		switch(c.analogEvent)
		{
			case EventType::AnalogOutputInt16:
				if(!fits(-32768.0, 32767.0))
				{
					done(CommandStatus::OUT_OF_RANGE);
					return;
				}
				event->SetPayload<EventType::AnalogOutputInt16>(AO16(static_cast<int16_t>(std::llround(v)), CommandStatus::SUCCESS));
				break;
			case EventType::AnalogOutputInt32:
				if(!fits(-2147483648.0, 2147483647.0))
				{
					done(CommandStatus::OUT_OF_RANGE);
					return;
				}
				event->SetPayload<EventType::AnalogOutputInt32>(AO32(static_cast<int32_t>(std::llround(v)), CommandStatus::SUCCESS));
				break;
			case EventType::AnalogOutputFloat32:
				event->SetPayload<EventType::AnalogOutputFloat32>(AOF(static_cast<float>(v), CommandStatus::SUCCESS));
				break;
			default:
				event->SetPayload<EventType::AnalogOutputDouble64>(AOD(v, CommandStatus::SUCCESS));
				break;
		}
	}
	//downstream answers on any thread: hop back onto the port strand before the session hears of it (the port may be gone by then)
	auto weak = weak_from_this();
	auto callback = std::make_shared<std::function<void(CommandStatus)>>([weak, done = std::move(done)](CommandStatus status)
		{
			if(auto sp = weak.lock())
				static_cast<ICCPPortBase&>(*sp).Post([done, status](ICCPPortBase&) { done(status); });
		});
	PublishEvent(event, callback);
}
