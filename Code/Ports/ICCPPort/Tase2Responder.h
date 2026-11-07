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
 * Tase2Responder.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The TASE.2 service side of one association: serves the peer's confirmed requests (association objects, data values,
 *  names, types, data sets, transfer sets) from a ResponderModel and sends transfer set reports.
 *
 *  Used by the server port (with its points) and, with an empty model, by the client port, so that a peer that acts as
 *  client and server on one association always gets an answer (an error where there is nothing to serve, never silence).
 *
 *  Pure logic over stack::Connection: no sockets, no timers of its own (Tick() is driven by the owner).
 *  Assumptions, marked [S] in the README: the first report of a started transfer set is an integrity report sent at
 *  once. BlockData (TASE.2 Block 3) is honoured only when 3 is in SupportedBlocks (otherwise it is accepted and has no effect, as before): the reports of such a
 *  transfer set are sent in the blocked form of Tase2Blocked.h, the special objects as separate variables and one octet string in a VMD-specific variable. As a real peer sends it [V]: a
 *  COMPLETE report (start-up, integrity, operator request, other external event, whatever RBE says) is rule 0, every member of the data set with the special objects included; a PARTIAL report
 *  (RBE: object change, interval, AllChangesReported) is rule 1, (1 octet position, value) pairs of the changed points only (several points make several pairs, ascending), positions counting
 *  the special objects [V]. Rule 2 (a position above 255, 2 octet positions; sent unless the option BlockedRule2 is false, then that report is ordinary with one warning per start) is [S]: the
 *  standard's text only, never seen from a real peer. So are the other point types'
 *  octets and the byte order of a non zero 2 octet Event_Code_Detected. A data set that has a member which is not a point is reported in the ordinary form (BlockedUnsendable has the conditions).
 *  With AllChangesReported (and RBE,
 *  ObjectChange and a BufferTime above 0) every change of a point in the buffer time is reported, as consecutive ordinary reports
 *  by exception in the order of the changes, none listing a point twice (the standard does not define the layout).
 *
 *  Critical (IEC 60870-6-503 clauses 5.3.10 and 7.2.5.5.2 [T]): each report of a transfer set started with Critical TRUE is
 *  remembered (transfer set name and Transfer_Set_Time_Stamp) until the client's Transfer_Report_ACK for it arrives
 *  (OnInformationReport). A Transfer_Report_NACK, no acknowledgement within ReportAckTimeoutms, or more than
 *  MaxUnacknowledgedReports outstanding reports (the oldest goes) is the standard's Data_failure condition. This port has no
 *  event conditions, so it is logged (warning) and counted (ReportCounters) only [S]; nothing is resent, no report is held back and
 *  the association is never touched. Reports of a transfer set that is not critical are not tracked at all.
 *
 *  TLE (the time limit for execution, seconds; 0 = not enforced) [T]: a report that is handed to the transport more than TLE
 *  after it was due (the end of the buffer time, the interval or integrity time) is discarded, logged and counted, and the next report
 *  of the transfer set is then sent whatever its age. The stack gives no transport back-pressure, so "due" to "handed over" is
 *  what the owner's Tick() lateness shows: nothing else is measured [S] (see the README).
 *
 *  Block 5 (device control) and its optional parts: the select / operate state of a device is the association's (ResponderSession::Device); the tag of a taggable device
 *  (the variable <Device>_TAG, Get Tag and Set Tag, IEC 60870-6-503 clauses 5.3.12.5, 5.3.12.6, 7.2.6.8, 7.2.6.9) belongs to the device and is held in the ResponderModel, so it survives
 *  the association (and is lost when the port restarts). Set Tag is refused while the device is ARMED or busy, and for a VCC device by any client but the one that set the tag
 *  (the client is the identity the owner gives with SetClientIdentity: the calling AP title and AE qualifier, else none); a tag prevents the select (OPEN_AND_CLOSE_INHIBIT) and
 *  the operate (see TagPrevents). The Local Reset action takes ARMED to IDLE (LocalReset): the MMS event notification of it is not sent (Block 7 is not supported), and a change of
 *  a tag, by Set Tag or locally, is not reported either. No real peer has been seen using tags: all of it is [T] or [S], see the README.
 */

#ifndef ICCP_TASE2_RESPONDER_H_
#define ICCP_TASE2_RESPONDER_H_

#include "ICCPPortConf.h"
#include "Tase2Points.h"
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
namespace iccp
{
namespace tase2
{

using LogFn = std::function<void (stack::LogLevel, const std::string&)>;

struct Timebase
{
	std::function<stack::Clock::time_point()> steady = [] { return stack::Clock::now(); };
	std::function<odc::msSinceEpoch_t()> wallMs = [] { return odc::msSinceEpoch(); };
};

//The values and definitions a responder serves; shared by successive associations (single threaded).
class ResponderModel
{
public:
	ResponderModel(const ICCPPortConf& conf, bool servePoints);

	const ICCPPortConf& Conf() const
	{
		return conf;
	}
	bool ServesPoints() const
	{
		return servePoints;
	}

	//ODC value updates. Returns the index into Conf().points if the value or quality changed (or was first set).
	std::optional<size_t> Update(PointKind kind, size_t index, bool binary, double analog, uint32_t counter, odc::QualityFlags quality, odc::msSinceEpoch_t timestamp);
	const PointState& State(size_t pointIndex) const
	{
		return states[pointIndex]; //port strand only (the only writer of the states); another thread uses StatesSnapshot()
	}
	//A copy of the states of all points (parallel to Conf().points; empty if the model serves no points), safe from any thread: for the UI (GetCurrentState)
	std::vector<PointState> StatesSnapshot() const;
	//when the model was made (the "no value yet" time of a point that was never set)
	odc::msSinceEpoch_t CreatedAt() const
	{
		return createdAt;
	}
	const IccpPointConf& Point(size_t pointIndex) const
	{
		return conf.points[pointIndex];
	}
	std::optional<size_t> FindPoint(const mms::ObjectName& name) const;

	//TASE.2 Block 5: a served control device (index into Conf().controls) by the name of the device, of its select object or of its tag object
	struct ControlRef
	{
		size_t control = 0;
		bool select = false; //the "_SBO" object
		bool tag = false;    //the "_TAG" object (of any served device; only a taggable device has one that exists, see FindTag)
	};
	std::optional<ControlRef> FindControl(const mms::ObjectName& name) const;
	//the tag object of a taggable served device: the index of the control (nullopt for any other name, and for a device that is not taggable)
	std::optional<size_t> FindTag(const mms::ObjectName& name) const;

	//Device tags (optional, see Tase2Control.h). The tag belongs to the device, not to the association: it is kept when the association ends (and lost when the
	//port is restarted). It is read and written from the port's strand and read by the UI thread too, so it has a lock.
	struct TagState
	{
		DeviceTag tag = DeviceTag::NoTag;
		std::string reason;
		std::optional<std::string> owner; //the identity of the client that set the tag (ResponderSession::SetClientIdentity); none: set locally (configuration or operator)
	};
	TagState TagOf(size_t control) const;
	void SetTag(size_t control, DeviceTag tag, const std::string& reason, std::optional<std::string> owner);
	//what happened to the tags, cumulative over the associations
	enum class TagEvent : uint8_t
	{
		Set,            //a Set Tag of the peer was accepted
		LocalSet,       //the tag was set locally (the UI command)
		Refused,        //a Get Tag or Set Tag of the peer was refused
		SelectRefused,  //a select was refused because of the tag
		OperateRefused, //an operate was refused because of the tag
		LocalReset      //a local reset took a device from ARMED to IDLE
	};
	struct TagCounters
	{
		size_t set = 0, localSet = 0, refused = 0, selectsRefused = 0, operatesRefused = 0, localResets = 0;
	};
	void CountTag(TagEvent event);
	TagCounters TagCounts() const;

	//Data sets configured on the server (shared by all associations)
	struct DataSet
	{
		mms::ObjectName name;
		std::vector<mms::ObjectName> members;
	};
	const std::vector<DataSet>& StaticDataSets() const
	{
		return staticDataSets;
	}

private:
	const ICCPPortConf& conf;
	bool servePoints;
	const odc::msSinceEpoch_t createdAt;
	mutable std::mutex stateMtx; //protects the writes to states (Update) against StatesSnapshot() on another thread
	std::vector<PointState> states;
	std::map<std::pair<int, size_t>, size_t> byKindIndex;
	std::unordered_map<std::string, size_t> byName; //object name key -> index into conf.points
	std::unordered_map<std::string, ControlRef> controlByName;
	std::vector<DataSet> staticDataSets;
	mutable std::mutex tagMtx;
	std::vector<TagState> tags; //parallel to conf.controls
	TagCounters tagCounters;
};

//TASE.2 Block 5: an operate the peer asked for, validated, to be carried out downstream (as an ODC control event)
struct ControlOperation
{
	const IccpControlConf* control = nullptr;
	bool on = false;     //Command types: ON (true) or OFF
	double value = 0;    //setpoints: the ODC value (TASE.2 value * scale + offset)
	int64_t integer = 0; //the TASE.2 integer of a command or discrete setpoint, as received
};
//Carry out an operate downstream and call done exactly once with its status (any thread-safe context of the owner's strand). A session ignores a done
//that comes after the association ended.
using ControlHandler = std::function<void (const ControlOperation&, std::function<void (odc::CommandStatus)> done)>;

class ResponderSession
{
public:
	ResponderSession(ResponderModel& model, stack::Connection& connection, Timebase timebase, LogFn log);
	~ResponderSession();

	void SetControlHandler(ControlHandler handler)
	{
		controlHandler = std::move(handler);
	}
	//Who the peer is, for the VCC tag owner rule (IEC 60870-6-503 clause 7.2.6.8.3): the same client is the same identity. The port gives the calling AP title and AE
	//qualifier of the association request, empty if there are none (a port that initiated the association does not learn them): the empty identity is one client too [S].
	void SetClientIdentity(std::string identity)
	{
		clientId = std::move(identity);
	}
	//The Local Reset action (IEC 60870-6-503 clause 5.3.12.8) on the device that is control `control` (index into Conf().controls): ARMED goes to IDLE. true if the
	//device was ARMED. The MMS event notification of the action (the event condition <device>_reset, Block 7) is not sent; the log and the counter are all there is [S].
	bool LocalReset(size_t control);
	//is the device ARMED now (a select that has not lapsed)
	bool DeviceArmed(size_t control) const;

	//the peer's confirmed request
	void OnRequest(uint32_t invokeId, const mms::ConfirmedServiceRequest& request);
	//time: interval / integrity / buffer timers and start times
	void Tick();
	//a point of the model changed (as returned by ResponderModel::Update): ObjectChange reports
	void OnPointChanged(size_t pointIndex);
	//OperatorRequest or OtherExternalEvent (a DsCondition bit): a full report from each enabled transfer set that asked for it.
	//transferSet empty = all. Returns the number of reports sent.
	size_t RaiseCondition(uint8_t condition, int64_t eventCode, const std::string& transferSet = "");
	//The peer's unconfirmed InformationReport. true = it was a Transfer_Report_ACK or Transfer_Report_NACK (consumed, whether or not it matched
	//anything: the log says what became of it); false = some other report, for the client side to handle.
	bool OnInformationReport(const mms::InformationReport& report);
	//The association ended: forget the outstanding critical reports (not a failure: the association's end says more than they could)
	void AssociationEnded();

	//introspection
	size_t ClientDataSetCount() const
	{
		return clientDataSets.size();
	}
	bool TransferSetEnabled(const std::string& name) const;
	size_t ReportsSent() const
	{
		return reportsSent;
	}
	//Critical reports and TLE, counters of this association
	struct ReportStats
	{
		size_t criticalSent = 0;  //reports sent from a transfer set that is critical
		size_t acknowledged = 0;  //of them, acknowledged by a Transfer_Report_ACK
		size_t nacksReceived = 0; //Transfer_Report_NACK received (for any transfer set)
		size_t ackTimeouts = 0;   //critical reports not acknowledged within ReportAckTimeoutms
		size_t ackOverflows = 0;  //critical reports dropped from the list of outstanding ones by MaxUnacknowledgedReports
		size_t dataFailures = 0;  //the Data_failure condition: a NACK of a critical transfer set, a timeout or an overflow
		size_t tleDiscarded = 0;  //reports discarded because TLE was exceeded
		size_t blockedSent = 0;   //reports sent in the blocked form (Block 3, BlockData honoured)
	};
	const ReportStats& ReportCounters() const
	{
		return reportStats;
	}
	//critical reports waiting for their acknowledgement: all transfer sets, or the one named
	size_t OutstandingCriticalReports() const;
	size_t OutstandingCriticalReports(const std::string& transferSet) const;
	//Block 5 counters of this association
	struct ControlStats
	{
		size_t selects = 0;  //selects accepted
		size_t operates = 0; //operates accepted and carried out downstream successfully
		size_t refused = 0;  //selects and operates refused or failed
	};
	const ControlStats& ControlCounters() const
	{
		return controlStats;
	}

private:
	//TASE.2 Block 5: the state of one served device on this association (IEC 60870-6-503 clause 5.3.12: IDLE and ARMED, here with the operation in flight)
	struct Device
	{
		enum class State : uint8_t
		{
			Idle,
			Armed,
			Operating
		};
		State state = State::Idle;
		stack::Clock::time_point expiry;                           //Armed: when the select lapses; Operating: when downstream is given up on
		uint64_t generation = 0;                                   //identifies the operation in flight: a late answer from downstream is ignored
		std::function<void(std::optional<ControlError>)> complete; //Operating: finishes the Write
	};
	struct PendingWrite
	{
		uint32_t id = 0;
		std::vector<mms::WriteResult> results;
		size_t remaining = 0;
		bool dispatched = false;
	};
	//AllChangesReported: one change of a point, with the state it had at that moment (the value, quality and time stamp that the report carries)
	struct BufferedChange
	{
		size_t point = 0; //index into the model's points
		PointState state;
	};
	struct TransferSetRuntime
	{
		std::string name;
		bool reserved = false; //handed out by Next_DSTransfer_Set or written
		bool enabled = false;
		DsTransferSet params;
		std::string dataSetKey;
		stack::Clock::time_point activeFrom;
		std::optional<stack::Clock::time_point> nextInterval;
		std::optional<stack::Clock::time_point> nextIntegrity;
		std::optional<stack::Clock::time_point> bufferDeadline;
		std::set<size_t> pending; //changed member positions (index into the data set's members)
		//Critical: the reports sent and not yet acknowledged, oldest first
		struct Outstanding
		{
			std::optional<int64_t> timeStamp; //the Transfer_Set_Time_Stamp the report carried (none: its layout has no such object)
			stack::Clock::time_point sentAt;
		};
		std::deque<Outstanding> outstanding;
		bool tleExceeded = false; //the last report that was due was discarded for TLE: the next is sent whatever its age
		//point index -> the member positions of the data set that are that point, built when the transfer set is started
		//(a data set cannot change while a transfer set uses it: deleting it stops the transfer set)
		std::unordered_map<size_t, std::vector<size_t>> pointPositions;
		bool positionsBuilt = false;
		//AllChangesReported (see AllChanges()): every change of the buffer period in the order it happened, instead of the last value of each member in `pending`
		std::vector<BufferedChange> changes;
		bool overflowLogged = false;        //the cap on `changes` was reached (logged once per start)
		bool blockedFallbackLogged = false; //BlockData could not be honoured for this data set (a member that is not a point): said once per start
		bool blockedRule2Logged = false;    //a report needed rule 2 and was sent ordinary: said once per start
	};

	ResponderModel& model;
	stack::Connection& conn;
	Timebase time;
	LogFn log;
	std::vector<TransferSetRuntime> transferSets;
	std::map<std::string, ResponderModel::DataSet> clientDataSets;
	size_t reportsSent = 0;
	ReportStats reportStats;
	ControlHandler controlHandler;
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);
	std::vector<Device> devices; //parallel to the model's Conf().controls
	uint64_t nextGeneration = 1;
	ControlStats controlStats;

	//Block 5
	//a Read of a control object: the select object is the SELECT (IDLE -> ARMED, answers the configured CheckBackID); the device itself is write only
	mms::AccessResult ReadControl(const ResponderModel::ControlRef& ref);
	//the Write of a device (operate); a Write of the select object is refused. complete is called once with the outcome (possibly before this returns, possibly later)
	void WriteControl(const ResponderModel::ControlRef& ref, const mms::Data& data, std::function<void(std::optional<ControlError>)> complete);
	void WriteOperate(size_t control, const mms::Data& data, const std::function<void(std::optional<ControlError>)>& complete);
	void RefuseControl(const IccpControlConf& c, const char* what, ControlError e, const std::function<void(std::optional<ControlError>)>& complete, const std::string& detail = std::string());
	void TickControls();
	//Device tags: Get Tag is a Read of D_TAG, Set Tag a Write of it. Both answer object-non-existent for a device that is not taggable.
	mms::AccessResult ReadTag(size_t control);
	void WriteTag(size_t control, const mms::Data& data, const std::function<void(std::optional<ControlError>)>& complete);
	//the value of the tag now, with the state bit
	TagValue CurrentTag(size_t control) const;
	std::string clientId; //see SetClientIdentity

	void Log(stack::LogLevel level, const std::string& text) const
	{
		if(log)
			log(level, text);
	}
	static std::string Key(const mms::ObjectName& n);
	const ResponderModel::DataSet* FindDataSet(const mms::ObjectName& name) const;
	TransferSetRuntime* FindTransferSet(const std::string& name);

	//handlers
	void HandleIdentify(uint32_t id);
	void HandleGetNameList(uint32_t id, const mms::GetNameListRequest& r);
	void HandleRead(uint32_t id, const mms::ReadRequest& r);
	void HandleWrite(uint32_t id, const mms::WriteRequest& r);
	void HandleGetVariableAccessAttributes(uint32_t id, const mms::GetVariableAccessAttributesRequest& r);
	void HandleDefine(uint32_t id, const mms::DefineNamedVariableListRequest& r);
	void HandleDelete(uint32_t id, const mms::DeleteNamedVariableListRequest& r);
	void HandleGetAttributes(uint32_t id, const mms::GetNamedVariableListAttributesRequest& r);
	void Fail(uint32_t id, int64_t errorClass, int64_t code);

	std::vector<std::string> NamesIn(mms::ObjectScope scope, const std::string& domain, int64_t objectClass, bool& known) const;
	mms::AccessResult ReadItem(const mms::ObjectName& name);
	mms::AccessResult ReadMember(const mms::ObjectName& name) const;
	std::optional<ObjectRef> NextFreeTransferSet();
	bool ApplyTransferSetWrite(TransferSetRuntime& rt, const DsTransferSet& p);
	void DisableTransferSet(TransferSetRuntime& rt);
	bool IsSpecialName(const mms::ObjectName& n) const;

	//reports
	//changes: with onlyPending, report these buffered changes (in order) instead of rt.pending.
	//due: when the report was due (the end of the buffer time, the interval or integrity time); absent = it is sent as soon as it arises. Used for TLE.
	using Due = std::optional<stack::Clock::time_point>;
	void SendTransferReport(TransferSetRuntime& rt, uint8_t condition, bool onlyPending, int64_t eventCode, const std::vector<BufferedChange>* changes = nullptr, Due due = std::nullopt);
	//Block 3: the transfer set was started with BlockData and this port advertises Block 3 (3 in SupportedBlocks)
	bool BlockedHonoured(const TransferSetRuntime& rt) const;
	//Block 3: why a transfer set with BlockData is reported in the ordinary form all the same (nullopt: it is sent blocked); the one place for these conditions
	std::optional<std::string> BlockedUnsendable(const TransferSetRuntime& rt, const ResponderModel::DataSet& ds) const;
	//Block 3: fill in the report's variable list and data in the blocked form (the special objects, then the blocked variable; the values as one octet string). first: the index of the
	//first data member in the data set (the special objects before it). false = the data set cannot be sent blocked (a member that is not a point): nothing was changed.
	bool BuildBlockedValues(TransferSetRuntime& rt, const ResponderModel::DataSet& ds, size_t first, bool onlyPending, const std::vector<BufferedChange>* changes,
		odc::msSinceEpoch_t nowMs, const std::vector<mms::ObjectName>& specialNames, TransferReport& report);
	void FlushBuffer(TransferSetRuntime& rt, Due due = std::nullopt);
	//AllChangesReported is honoured only where the standard defines it: RBE true, the ObjectChange condition and a BufferTime above 0
	static bool AllChanges(const TransferSetRuntime& rt);
	void BufferChange(TransferSetRuntime& rt, size_t pointIndex);
	//send the buffered changes as reports in chronological order, each report with at most one change of any point; clears the buffer and its timer
	void FlushChanges(TransferSetRuntime& rt, uint8_t condition, Due due = std::nullopt);
	//TLE: false = the report is discarded (logged and counted); the next one of the transfer set is then allowed whatever its age
	bool TleAllows(TransferSetRuntime& rt, const Due& due);
	//Critical
	void TrackCriticalReport(TransferSetRuntime& rt, const std::optional<int64_t>& timeStamp);
	void ReportFailure(TransferSetRuntime& rt, const TransferSetRuntime::Outstanding& report, const std::string& why);
	void TickAcks();
};

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_RESPONDER_H_ */
