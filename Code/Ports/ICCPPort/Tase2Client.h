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
 * Tase2Client.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The TASE.2 client side of one association: verifies the peer (bilateral table, version, supported blocks), provisions the
 *  configured data sets and transfer sets, polls data sets, and turns transfer set reports into point updates.
 *
 *  Per configured transfer set, in order: Next_DSTransfer_Set (unless the transfer set has a fixed name), delete the data set
 *  (any answer is accepted: it may not exist), define it (the special objects first, then the points), write the DSTransferSet
 *  to start the transfer. Data sets configured with a poll interval are deleted, defined and then read by list name.
 *
 *  A transfer report must come from a transfer set started here (the transfer set name it carries, and the data set it names, if it does); a report that
 *  does not is discarded and answered with Transfer_Report_NACK, and the reports of a transfer set started with Critical TRUE are answered with
 *  Transfer_Report_ACK (IEC 60870-6-503, 5.3.10 / 7.2.5.5.2 [T]; the wire layout of both is [S], see Tase2.h). Both can be switched off (AcknowledgeCriticalReports,
 *  NackUnknownReports).
 *
 *  Block 3 (blocked transfers): a report in the blocked form (the special objects as variables, and one octet string of ALL the members of the data set, the special objects included:
 *  Tase2Blocked.h) is split by the data set's members and published like any report, whether or not the transfer set was started with BlockData (a report that is clearly blocked is not
 *  thrown away). The report's time stamp, conditions and acknowledgement come from the separate variables, as for an ordinary report; the copies in the octet string are only compared.
 *  A block that cannot be decoded is logged with a hex dump of the data and counted, and none of it is applied. Rule 0 with RealQ points is as a real peer sends it [V]; the rest is [S].
 *
 *  Block 5: Operate (select before operate or direct) of the peer's devices, and the optional device tags: GetTag and SetTag (a Read and a Write of <Device>_TAG, see
 *  Tase2Control.h; [T] / [S], never seen from a real peer).
 *
 *  Pure logic over stack::Connection, no sockets or timers (Tick() drives polling).
 */

#ifndef ICCP_TASE2_CLIENT_H_
#define ICCP_TASE2_CLIENT_H_

#include "Tase2Responder.h"
#include <memory>
#include <set>

namespace iccp
{
namespace tase2
{

//Read the peer's association objects and check them against the configuration. done(ok, problem, info).
void VerifyPeer(stack::Connection& conn, const ICCPPortConf& conf, bool checkBlocks, const LogFn& log,
	std::function<void(bool, const std::string&, const AssociationInfo&)> done);

//How far the provisioning of one data set (and its transfer set) got
enum class ProvisionState : uint8_t
{
	Pending,
	Defined,
	DefinedPartial, //defined without the members the peer lacks (MissingMemberPolicy Prune), no transfer set started (yet)
	DefineFailed,
	TransferStarted, //also for a data set defined partially: see ProvisionStatus::partial
	TransferFailed
};
const char* ToString(ProvisionState s); //pending, defined, defined-partial, define-failed, transfer-started, transfer-failed

//The outcome of the diagnostic probe after a failed data set definition: one Read of the members as a list of variables
//(in chunks), to find which of them the peer lacks.
struct ProbeResult
{
	struct Entry
	{
		size_t index = 0;               //position within the data set definition, 0-based (the special objects come first)
		std::string name;               //the item name, as configured (the TASE.2 object name)
		std::string scope;              //"ICC" or "VCC"
		std::string domain;             //the effective domain (empty for VCC scope)
		std::string kind;               //the ODC kind: "Binary", "Analog" or "Counter" (the special objects are not probed)
		std::optional<size_t> odcIndex; //the ODC index (not for a special object)
		int64_t error = 0;              //the data access error the peer returned for it
	};
	std::string dataSet;
	std::string domain;
	size_t members = 0;
	size_t chunks = 0;                //Read requests that were answered
	size_t probed = 0;                //members whose state is known
	size_t present = 0;               //read fine
	size_t presentWithError = 0;      //exist but are not readable (a data access error other than object-undefined / object-non-existent)
	size_t missing = 0;               //data access error object-undefined or object-non-existent
	bool complete = false;            //every member was probed
	std::string failure;              //if not complete: why (a rejected or failed Read, a malformed reply, the association ended)
	std::vector<Entry> missingListed; //every missing member, in data set order
	std::vector<Entry> errorListed;   //every present-but-unreadable member, in data set order
};

struct ProvisionStatus
{
	std::string dataSet;
	std::string domain;
	std::string transferSet; //empty until known (the peer assigns it when none is configured)
	ProvisionState state = ProvisionState::Pending;
	size_t members = 0;
	bool partial = false;      //defined with only the members the peer has; the missing ones are in probe->missingListed
	size_t membersDefined = 0; //if partial: how many of `members` were defined
	std::string lastError;     //empty if none
	std::optional<ProbeResult> probe;
};

class ClientSession
{
public:
	struct Callbacks
	{
		std::function<void()> onReady;                                        //the peer was verified
		std::function<void(const std::string&)> onFatal;                      //the association must be dropped
		std::function<void(const IccpPointConf&, const PointState&)> onPoint; //a value to publish
		//a point that is not available on the peer (its data set member is missing, or its data set could not be provisioned): publish the
		//state's quality (MissingMemberQuality) and timestamp instead of a value. Once per point per association, after provisioning.
		std::function<void(const IccpPointConf&, const PointState&)> onUnavailable;
		//diagnostics, all optional
		std::function<void(const ProbeResult&)> onProbe;                         //a probe ended (complete or not)
		std::function<void(const std::vector<ProvisionStatus>&)> onProvisioning; //any change of the provisioning state
	};

	ClientSession(const ICCPPortConf& conf, stack::Connection& connection, Timebase timebase, LogFn log, Callbacks callbacks);

	void OnAssociated();
	void OnReport(const mms::InformationReport& report);
	void Tick();
	//stop the active transfers (Stop Transfer: unverified against real peers), then call done
	void Shutdown(std::function<void()> done);

	bool Ready() const
	{
		return ready;
	}
	size_t ActiveTransferSets() const;
	//the provisioning state of each data set, in configuration order
	std::vector<ProvisionStatus> Status() const;
	size_t ReportsReceived() const
	{
		return reportsReceived;
	}
	size_t ValuesPublished() const
	{
		return valuesPublished;
	}
	//Critical reports [S]: Transfer_Report_ACK sent for the reports of transfer sets started with Critical TRUE, and Transfer_Report_NACK sent for reports
	//that are not from a transfer set started here (which are discarded)
	size_t ReportAcksSent() const
	{
		return acksSent;
	}
	size_t ReportNacksSent() const
	{
		return nacksSent;
	}
	//Block 3 (blocked transfers) [S]: reports received in the blocked form (one octet string of values), and those of them that could not be decoded (nothing of them was applied)
	size_t BlockedReportsReceived() const
	{
		return blockedReceived;
	}
	size_t BlockedReportsUndecodable() const
	{
		return blockedUndecodable;
	}
	//the points flagged as not available so far on this association
	size_t UnavailablePoints() const
	{
		return flaggedList.size();
	}

	//TASE.2 Block 5: operate one of the peer's devices from an ODC control (a Read of the select object when the control has SelectRequired, then a Write of the bare value). `command` is what the ODC event carries: ON / OFF for a command device, the ODC value for a
	//setpoint. done is called exactly once with the ODC command status (also at once when the command cannot be started, and with UNDEFINED if the association
	//ends first). One operation per device at a time: another is answered ALREADY_ACTIVE.
	struct ControlCommand
	{
		bool on = false;
		double value = 0;
	};
	void Operate(const IccpControlConf& control, const ControlCommand& command, std::function<void(odc::CommandStatus)> done);
	//Device tags (optional, see Tase2Control.h): Get Tag (a Read of the tag object <Device>_TAG) and Set Tag (a Write of it) of one of the peer's devices (a control with
	//Direction Operate; for either scope). done is called exactly once with the outcome: also at once when the request cannot be started, and when the association ends first.
	struct TagResult
	{
		bool ok = false;
		TagValue value;                         //Get Tag: the tag, the state bit (ARMED) and the reason the peer returned. Set Tag: what was written
		std::optional<int64_t> dataAccessError; //the data access error the peer answered with, if it did
		std::string error;                      //when !ok: what went wrong, readable
	};
	void GetTag(const IccpControlConf& control, std::function<void(const TagResult&)> done);
	void SetTag(const IccpControlConf& control, DeviceTag tag, const std::string& reason, std::function<void(const TagResult&)> done);
	struct ControlStats
	{
		size_t selects = 0;     //selects the peer accepted
		size_t operates = 0;    //operates that succeeded
		size_t failures = 0;    //commands that did not succeed
		size_t tagReads = 0;    //Get Tag requests the peer answered with a tag
		size_t tagWrites = 0;   //Set Tag requests the peer accepted
		size_t tagFailures = 0; //Get Tag and Set Tag requests that did not succeed
	};
	const ControlStats& ControlCounters() const
	{
		return controlStats;
	}
	~ClientSession();

private:
	struct ControlOp;
	struct TagOp;
	struct Provision
	{
		const DataSetConf* dataSet = nullptr;
		const TransferSetConf* transfer = nullptr;
		mms::ObjectName name;
		std::vector<mms::ObjectName> members;           //specials first, then points; after a prune only what exists on the peer
		std::vector<const IccpPointConf*> memberPoints; //parallel to members: the configured point, nullptr for a special object
		std::vector<mms::ObjectName> fullMembers;       //as configured
		std::vector<const IccpPointConf*> fullPoints;
		std::vector<Special> layout;
		std::optional<mms::ObjectName> transferSetName;
		DsTransferSet parameters;
		bool active = false;
		bool defined = false;
		bool poll = false;
		stack::Clock::time_point nextPoll;
		bool pollInFlight = false;
		ProvisionStatus status;
	};
	struct ProbeRun;

	const ICCPPortConf& conf;
	stack::Connection& conn;
	Timebase time;
	LogFn log;
	Callbacks cb;
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);
	std::vector<Provision> provisions;
	std::map<std::string, const IccpPointConf*> pointsByName;
	bool ready = false;
	bool shuttingDown = false;
	bool provisioningWalked = false; //every data set has been taken through its steps (recoveries may still be running)
	bool summaryLogged = false;
	size_t recoveriesInFlight = 0;                  //probes, and the pruned redefinitions and transfer set starts that follow them
	std::set<std::string> missingKeys;              //points whose member a probe found missing on the peer
	std::vector<const IccpPointConf*> missingOrder; //the same, in the order found
	std::set<std::string> flagged;                  //points flagged as not available (never published with a value)
	std::vector<const IccpPointConf*> flaggedList;
	size_t reportsReceived = 0;
	size_t valuesPublished = 0;
	size_t acksSent = 0;
	size_t nacksSent = 0;
	size_t blockedReceived = 0;
	size_t blockedUndecodable = 0;
	std::set<std::string> blockedWithoutRequest; //the transfer sets whose blocked report arrived although BlockData was false (the warning is logged for the first only)
	std::set<std::string> nackedTransferSets;    //the names that have been NACKed (the warning is logged for the first only)
	std::map<const IccpControlConf*, std::shared_ptr<ControlOp>> controlOps;
	std::set<std::shared_ptr<TagOp>> tagOps; //Get Tag and Set Tag requests in flight
	ControlStats controlStats;

	void StartTagRequest(const IccpControlConf& control, bool set, DeviceTag tag, const std::string& reason, std::function<void(const TagResult&)> done);
	void FinishTag(const std::shared_ptr<TagOp>& op, TagResult result);

	//the transfer set a report names is one this client started
	static bool NamesTransferSet(const Provision& p, const ObjectRef& transferSet);
	Provision* FindByTransferSet(const ObjectRef& transferSet);
	//the data of the report is discarded and the peer told (Transfer_Report_NACK), unless NackUnknownReports is off
	void NackReport(const ObjectRef& transferSet, const std::string& why);
	//a report of a transfer set started with Critical TRUE has been processed: Transfer_Report_ACK
	void AcknowledgeReport(const TransferReport& report);
	//Block 3: a report in the blocked form (Tase2Blocked.h): split the octet string by the types of the data set's members and publish the values; a block that does not decode is logged
	//with a hex dump and counted, nothing of it is applied, and it is not answered
	void OnBlockedReport(const blocked::BlockedReport& report);
	void BlockedUndecodable(const blocked::BlockedReport& report, const std::string& why, const Provision* owner);

	void ControlRead(const std::shared_ptr<ControlOp>& op);
	void ControlOperate(const std::shared_ptr<ControlOp>& op);
	void CompleteControl(const std::shared_ptr<ControlOp>& op, odc::CommandStatus status, const std::string& why);
	void TickControls();

	void Log(stack::LogLevel level, const std::string& text) const
	{
		if(log)
			log(level, text);
	}
	void Send(mms::ConfirmedServiceRequest request, std::function<void(const stack::Outcome&)> handler);
	void ProvisionNext(size_t index);
	void StartTransfer(size_t index, std::function<void()> done);
	void FinishProvision(size_t index);
	//the diagnostics
	std::string Subject(const Provision& p, bool withTransferSet) const;
	void Failed(size_t index, ProvisionState state, const std::string& operation, const std::string& why, bool withTransferSet,
		stack::LogLevel level = stack::LogLevel::Error);
	void NotifyStatus() const;
	void MaybeFinishProvisioning();
	void StartProbe(size_t index);
	void ProbeNextChunk(const std::shared_ptr<ProbeRun>& run);
	void EndProbe(const std::shared_ptr<ProbeRun>& run);
	//MissingMemberPolicy Prune: what follows a probe
	struct PrunedSet;
	void DefinePruned(size_t index, const std::shared_ptr<PrunedSet>& pruned, bool afterDelete);
	void GiveUp(size_t index, const std::string& reason);
	void EndRecovery();
	std::string Where(const IccpPointConf* point, const mms::ObjectName& name) const;
	void PublishValues(const std::vector<mms::ObjectName>& names, const std::vector<mms::AccessResult>& values);
	static std::string Key(const mms::ObjectName& n);
	Provision* FindByName(const mms::ObjectName& n);
};

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_CLIENT_H_ */
