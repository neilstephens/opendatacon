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
 * ICCPPortBase.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  What the ICCP client and server ports share: configuration, the TCP connection (TCPSessionManager), the association
 *  (stack::Connection with its TASE.2 sessions), timers and the ODC connection state. All work is serialised on one strand.
 *
 *  The port type (ICCPClientPort / ICCPServerPort) decides only the TCP direction: the client port connects (and so initiates the
 *  association), the server port listens and responds. What the port does on the association is the ServiceRole (Client, Server
 *  or Peer), set in the configuration for either port type.
 */

#ifndef ICCPPORTBASE_H_
#define ICCPPORTBASE_H_

#include "ICCPPortConf.h"
#include "Tase2Client.h"
#include "Tase2Responder.h"
#include <atomic>
#include <mutex>
#include <opendatacon/DataPort.h>
#include <opendatacon/TCPSessionManager.h>

using namespace odc;

class ICCPPortBase: public DataPort
{
public:
	ICCPPortBase(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool tcpListener);
	~ICCPPortBase() override;

	void Enable() override;
	void Disable() override;
	void Build() override;
	void ProcessElements(const Json::Value& JSONRoot) override;
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	const Json::Value GetStatus() const override;
	const Json::Value GetStatistics() const override;
	//The last known value of each point this port serves (a port with a server side: Server, Peer), and its devices; a Client returns the default (empty) value. Any thread. See the README.
	const Json::Value GetCurrentState() const override;

	//the UI commands (OperatorRequest, OtherExternalEvent): present whenever the server side of the service role is active
	std::pair<std::string,const IUIResponder*> GetUIResponder() override;
	//OperatorRequest / OtherExternalEvent (an iccp::tase2::DsCondition bit) on the current association's transfer sets.
	//transferSet empty = all. Runs asynchronously on the port strand.
	void RaiseCondition(uint8_t condition, int64_t eventCode, const std::string& transferSet = "");

	//Optional Block 5 parts: device tags and the Local Reset action (see the README, "Device control"). Each returns an empty string when the action was started (it runs
	//on the port strand and the log says what came of it), else why it was not: no such control, the control is not taggable, a bad reason, the association is down, ...
	//The server side, for the devices this port serves (Direction Serve):
	//  Local Reset: the device goes from ARMED to IDLE as if by a local action; with a tag, the tag is set too
	std::string LocalReset(const std::string& control, const std::optional<iccp::tase2::DeviceTag>& tag, const std::string& reason);
	//  set the tag of a taggable device locally (no checks of the device state or of the owner of the tag)
	std::string SetLocalTag(const std::string& control, iccp::tase2::DeviceTag tag, const std::string& reason);
	//  the tags of the taggable devices: [{Control, Scope, Domain, Tag, Reason, SetBy}], from any thread
	Json::Value Tags() const;
	//The client side, for the devices of the peer that this port operates (Direction Operate):
	//  Get Tag (a Read of <Device>_TAG): the tag, the reason and the state are logged
	std::string GetPeerTag(const std::string& control);
	//  Set Tag (a Write of it)
	std::string SetPeerTag(const std::string& control, iccp::tase2::DeviceTag tag, const std::string& reason);
	//what the port does on the association
	bool ServesDevicesOrPoints() const
	{
		return Conf().HasServerSide();
	}
	bool OperatesDevices() const
	{
		return Conf().HasControls(ControlDirection::Operate);
	}

	//false if the configuration had errors (the port will not enable)
	bool ConfigValid() const
	{
		return configErrors.empty();
	}
	const std::vector<std::string>& ConfigErrors() const
	{
		return configErrors;
	}
	//true while the association is up (and, for a client, the peer verified)
	bool AssociationUp() const
	{
		return associationUp.load();
	}

	//Test hooks (they act on the port strand, like the transport callbacks do; only the unit tests use them):
	//deliver a transport state event as if the TCP connection had come up or gone away, without anything happening to the socket
	void TestSimulateTransportState(bool connected);
	//run f on the port strand
	void TestPost(std::function<void(ICCPPortBase&)> f);
	//call only from a function run by TestPost: abort the current association (the stack writes its abort PDUs, as it does in the field)
	void TestAbortAssociation(const std::string& reason);
	//bytes the port refused to write to the socket because they belonged to an earlier connection or broke the first-bytes rule
	size_t StrayBytesDropped() const
	{
		return statStrayDropped.load();
	}
	//switch off the stale-write guards (the epoch checks, the first-bytes rule, the socket manager's dropping of writes made while disconnected,
	//and on a TLS port the serial a write is tied to),
	//which makes the port behave as it did when a stale write could reach a new connection. Call after Build() and before Enable(). Only to show
	//in a test that the guards are what prevents it.
	void TestDisableStaleGuards();

protected:
	const ICCPPortConf& Conf() const
	{
		return *static_cast<ICCPPortConf*>(pConf.get());
	}

	struct Association
	{
		std::unique_ptr<iccp::stack::Connection> conn;
		std::unique_ptr<iccp::tase2::ResponderSession> responder;
		std::unique_ptr<iccp::tase2::ClientSession> client;
		bool closed = false;
		bool published = false; //CONNECTED sent
	};

	const bool tcpListener; //true: this port accepts the TCP connection (and responds to the association); false: it connects (and initiates)
	std::vector<std::string> configErrors;
	Json::Value mergedConf = Json::Value(Json::objectValue); //the configuration sources merged, see ProcessElements()
	bool sourceUnreadable = false;                           //a configuration source failed to parse: the configuration stays invalid
	std::unique_ptr<iccp::tase2::ResponderModel> model;
	std::atomic<const iccp::tase2::ResponderModel*> modelView{nullptr}; //the model for GetCurrentState(), which may be called from any thread (set by Build())
	std::unique_ptr<Association> assoc;
	iccp::tase2::Timebase timebase;

	//run f on the port strand (if the port still exists)
	void Post(std::function<void(ICCPPortBase&)> f);
	void Log(iccp::stack::LogLevel level, const std::string& text) const;
	void PublishConnection(ConnectState state);
	//the association is up (client side: the peer was verified): publish CONNECTED
	void MarkConnected();

	//statistics, read from any thread
	std::atomic<size_t> statAssociations{0};
	std::atomic<size_t> statReportsSent{0};
	std::atomic<size_t> statReportsReceived{0};
	std::atomic<size_t> statValuesPublished{0};
	std::atomic<size_t> statUnavailable{0};
	//Block 5 [S] counters, cumulative over the associations: sent by us (client side) and received from the peer (server side)
	std::atomic<size_t> statControlSelectsSent{0};
	std::atomic<size_t> statControlOperatesSent{0};
	std::atomic<size_t> statControlFailures{0};
	std::atomic<size_t> statControlSelectsReceived{0};
	std::atomic<size_t> statControlOperatesReceived{0};
	std::atomic<size_t> statControlRefused{0};
	struct ControlTotals
	{
		size_t selectsSent = 0, operatesSent = 0, failures = 0, selectsReceived = 0, operatesReceived = 0, refused = 0;
		size_t tagReads = 0, tagWrites = 0, tagFailures = 0;
	} controlFolded; //the counters of the associations that have ended (port strand only)
	//Device tags [S]: what the peer's Get Tag, Set Tag, select and operate did to the tags of our devices (the model's counters, cumulative over the associations), and the
	//Get Tag and Set Tag requests this port sent (client side, folded like the Block 5 ones). Updated with them.
	std::atomic<size_t> statTagsSet{0};
	std::atomic<size_t> statLocalTagsSet{0};
	std::atomic<size_t> statTagRequestsRefused{0};
	std::atomic<size_t> statSelectsRefusedByTag{0};
	std::atomic<size_t> statOperatesRefusedByTag{0};
	std::atomic<size_t> statLocalResets{0};
	std::atomic<size_t> statTagsRead{0};
	std::atomic<size_t> statTagsWritten{0};
	std::atomic<size_t> statTagRequestFailures{0};
	//Critical reports and TLE [S], cumulative over the associations like the Block 5 ones: the server side's (what the peer's acknowledgements and the time limits did to
	//the reports sent) and the client side's (the acknowledgements sent). They are updated and folded together with the Block 5 counters.
	std::atomic<size_t> statCriticalSent{0};
	std::atomic<size_t> statCriticalAcknowledged{0};
	std::atomic<size_t> statCriticalOutstanding{0}; //now, not cumulative
	std::atomic<size_t> statNacksReceived{0};
	std::atomic<size_t> statAckTimeouts{0};
	std::atomic<size_t> statAckOverflows{0};
	std::atomic<size_t> statDataFailures{0};
	std::atomic<size_t> statReportsDiscardedTle{0};
	std::atomic<size_t> statReportAcksSent{0};
	std::atomic<size_t> statReportNacksSent{0};
	//Block 3, blocked transfers [S]: reports this port sent in the blocked form (server side), received in it and not decodable (client side); cumulative like the above
	std::atomic<size_t> statBlockedSent{0};
	std::atomic<size_t> statBlockedReceived{0};
	std::atomic<size_t> statBlockedUndecodable{0};
	struct ReportTotals
	{
		iccp::tase2::ResponderSession::ReportStats server;
		size_t acksSent = 0, nacksSent = 0;
		size_t blockedReceived = 0, blockedUndecodable = 0;
	} reportFolded;

private:
	//the client side of the service role (Client, Peer): verify the peer, define data sets, start transfer sets, publish what is read
	void CreateClientSession(Association& a);
	//keep a copy of the provisioning state for GetStatus(), which may be called from any thread
	void StoreProvisioning(const std::vector<iccp::tase2::ProvisionStatus>& statuses);
	mutable std::mutex provisioningMtx;
	Json::Value provisioning = Json::Value(Json::arrayValue);
	//the association is established: start the client side, or (Server) check the peer's bilateral table
	void OnAssociationReady(Association& a);
	//an ODC event: Serve points update the responder's cache and drive reports
	void HandleEvent(const std::shared_ptr<const EventInfo>& event, const SharedStatusCallback_t& pStatusCallback);
	//TASE.2 Block 5 [S]: an ODC control event for one of the peer's devices that this port operates
	void HandleControl(const std::shared_ptr<const EventInfo>& event, const SharedStatusCallback_t& pStatusCallback);
	//the peer operated one of our devices: publish the ODC control event and call done with its status
	void PublishControl(const iccp::tase2::ControlOperation& op, std::function<void(CommandStatus)> done);
	//the Local Reset and the local tag, on the port strand (index into Conf().controls)
	void DoLocalReset(size_t control, const std::optional<iccp::tase2::DeviceTag>& tag, const std::string& reason);
	void DoSetLocalTag(size_t control, iccp::tase2::DeviceTag tag, const std::string& reason);
	//update the Block 5 counters from the current association, and (fold) move them to the totals before the association goes
	void UpdateControlStats();
	void FoldControlStats();
	std::shared_ptr<IUIResponder> control; //shared by the ports whose role includes the server side, released with the last of them
	std::unique_ptr<strand_t, deleter> pStrand;
	std::unique_ptr<TCPSessionManager> pSockMan; //the TCP connection, with a TLS session on it when TLS is enabled
	std::unique_ptr<steady_timer, deleter> pTickTimer;
	std::unique_ptr<steady_timer, deleter> pReopenTimer;
	std::unique_ptr<steady_timer, deleter> pDisableTimer;
	std::atomic_bool associationUp{false};
	bool built = false;
	bool disabling = false;
	bool transportOpen = false;
	bool wantReopen = false;
	std::vector<uint8_t> readBytes;

	//What ties the bytes we generate to the TCP connection they were generated for: the transport epoch. It is incremented when a
	//connection comes up and when it goes away or is dropped. The stack's callbacks capture the epoch they were made in; anything they
	//produce for an older epoch is dropped, never written to the (possibly new) socket.
	uint64_t transportEpoch = 0;
	uint64_t liveSerial = 0;      //the socket manager's serial of the connection of the current epoch
	bool transportLive = false;   //a connection is up in the current epoch
	size_t epochBytesWritten = 0; //bytes written to the socket in the current epoch
	std::atomic<size_t> statStrayDropped{0};
	//the one place bytes go to the socket of a port without TLS: checks the epoch and the rule for the first bytes of a connection (a connection
	//request from a plain initiator, a confirm from a plain listener), traces the write
	void WriteToSocket(uint64_t epoch, const uint8_t* data, size_t length);
	bool EpochIsCurrent(uint64_t epoch) const
	{
		return guardsDisabled || (transportLive && epoch == transportEpoch);
	}
	//the same for the session of connection `serial` of a TLS port: it is the connection of the current epoch
	bool SerialIsCurrent(uint64_t serial) const
	{
		return guardsDisabled || (transportLive && serial == liveSerial);
	}
	bool guardsDisabled = false; //tests only, see TestDisableStaleGuards()
	void NewEpoch(bool live);
	//secure ICCP (IEC 62351-3): pSockMan has a TLS session on each connection; the association starts when the handshake completes.
	//The port keeps the status text, the failure count that backs off the reconnects, and the log lines.
	unsigned tlsFailures = 0; //consecutive handshake failures, for the retry back-off
	mutable std::mutex tlsStatusMtx;
	Json::Value tlsStatus = Json::Value(Json::objectValue); //for GetStatus(), which may be called from any thread
	void OnTlsEstablished(const iccp::tls::PeerInfo& peer);
	//the TLS session ended (failed, or the peer closed it): close the transport and have the connection made again
	void OnTlsDown(const std::string& why, bool failed);
	void OnTlsSessionEnded(const TCPSessionManager::SessionEnd& end);
	void SetTlsStatus(const std::string& state, const iccp::tls::PeerInfo* peer = nullptr);
	void SocketOpen();
	void SocketClose();
	uint64_t SocketSerial() const;
	//TraceHex, plain TCP: what is written to the socket, with its first bytes
	void TraceSocketWrite(uint64_t epoch, bool first, const uint8_t* data, size_t length) const;
	//authentication in the ACSE association (MACE): the status of the current (or last) association, for GetStatus()
	mutable std::mutex maceStatusMtx;
	iccp::mace::Status maceStatus;
	void SetMaceStatus(const iccp::mace::Status& status);
	void StartAssociation();
	void OnTransportState(bool connected, uint64_t serial);
	void OnTransportData(const std::string& bytes);
	void OnAssociationClosed(iccp::stack::EndReason reason, const std::string& text);
	void DropTransport();
	void ScheduleReopen();
	void ScheduleTick();
	void Tick();
	void ForceClose();
	//TraceHex: log bytes as hex at debug level
	void Trace(const char* direction, const uint8_t* data, size_t length) const;
};

#endif /* ICCPPORTBASE_H_ */
