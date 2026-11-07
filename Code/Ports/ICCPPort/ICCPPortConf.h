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
 * ICCPPortConf.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Port configuration shared by the ICCP client and server ports. Load() parses and validates the JSON
 *  configuration (the reference is in README.md) and returns the list of problems; an empty list means valid.
 */

#ifndef ICCPPORTCONF_H_
#define ICCPPORTCONF_H_

#include "IccpStack.h"
#include "IccpTlsDefaults.h"
#include "Mace.h"
#include "Tase2.h"
#include "Tase2Blocked.h"
#include "Tase2Control.h"
#include <json/json.h>
#include <opendatacon/DataPort.h>
#include <set>
#include <string>
#include <vector>

enum class PointKind : uint8_t
{
	Binary,
	Analog,
	Counter
};

//What this port does at the TASE.2 level, independent of the TCP direction.
enum class ServiceRole : uint8_t
{
	Client, //only issues requests: reads the peer's association objects, defines data sets, starts transfer sets, consumes reports
	Server, //only serves: association objects, points, data sets, transfer sets, sends reports
	Peer    //both, on the one association
};

//What a client does with a data set the peer refuses to define because it lacks some members
enum class MissingMemberPolicy : uint8_t
{
	Prune, //find the missing members (a probe), define the data set again without them, flag their points
	Fail   //log the error (and run the probe for the diagnostics), skip the data set
};

//The direction of a point or data set.
enum class PointDirection : uint8_t
{
	Serve, //the value comes from ODC Event() into our cache and is served to the peer (domain defaults to LocalDomain)
	Read   //the value is read from / reported by the peer and published to ODC with PublishEvent (domain defaults to RemoteDomain)
};

struct IccpPointConf
{
	PointKind kind = PointKind::Binary;
	PointDirection direction = PointDirection::Serve;
	size_t index = 0;   //ODC event index (used once per kind and direction)
	std::string name;   //TASE.2 object name
	std::string domain; //ICC domain (ICC scope)
	iccp::tase2::Scope scope = iccp::tase2::Scope::Icc;
	iccp::tase2::PointType type = iccp::tase2::PointType::StateQ;
	double scale = 1.0; //ODC value = TASE.2 value * scale + offset
	double offset = 0.0;

	iccp::mms::ObjectName Object() const
	{
		return scope == iccp::tase2::Scope::Icc ? iccp::mms::ObjectName::Domain(domain, name) : iccp::mms::ObjectName::Vmd(name);
	}
};

//TASE.2 Block 5 [S]: the kind of ODC control a device maps to
enum class ControlKind : uint8_t
{
	Binary, //"BinaryControls": ODC ControlRelayOutputBlock events
	Analog  //"AnalogControls": ODC AnalogOutput events (Int16, Int32, Float32, Double64)
};

//Who owns the device
enum class ControlDirection : uint8_t
{
	Serve,  //the device is ours: the peer operates it, we publish the ODC control event downstream (domain defaults to LocalDomain)
	Operate //the device is the peer's: we operate it from ODC control events (domain defaults to RemoteDomain)
};

struct IccpControlConf
{
	ControlKind kind = ControlKind::Binary;
	ControlDirection direction = ControlDirection::Serve;
	size_t index = 0;   //ODC event index (used once per kind and direction)
	std::string name;   //TASE.2 device name
	std::string domain; //ICC domain (ICC scope)
	iccp::tase2::Scope scope = iccp::tase2::Scope::Icc;
	iccp::tase2::ControlType type = iccp::tase2::ControlType::Command;
	bool selectRequired = true;                                        //Serve: the device is an SBO device: an operate needs the select (a Read of the select object) first. Operate: read the select object before the operate
	uint32_t selectTimeoutMs = 10000;                                  //Serve: how long a select (ARMED) lasts. Operate: not used
	uint32_t timeoutMs = 0;                                            //Serve: wait for downstream to answer an operate; Operate: the whole select + operate sequence (0 = defaults)
	std::optional<int64_t> checkBackId;                                //Serve: the CheckBackID a select returns (default 1). Operate: the CheckBackID that must come back from the select (absent: any)
	int64_t commandValueOn = 1;                                        //Command types: the integer that means ON
	int64_t commandValueOff = 0;                                       //and OFF
	odc::ControlCode codeOn = odc::ControlCode::LATCH_ON;              //Serve: the ODC control code published for ON
	odc::ControlCode codeOff = odc::ControlCode::LATCH_OFF;            //and for OFF
	odc::EventType analogEvent = odc::EventType::AnalogOutputDouble64; //Serve setpoints: the ODC event type published
	double scale = 1.0;                                                //setpoints: ODC value = TASE.2 value * scale + offset
	double offset = 0.0;
	bool taggable = false;                                             //Serve: the device has the Tag attribute: the variable D_TAG (Get Tag, Set Tag), enforced on select and operate. Not for Operate
	iccp::tase2::DeviceTag initialTag = iccp::tase2::DeviceTag::NoTag; //Serve, taggable: the tag the device has when the port starts (set locally: only the local operator can change a VCC device's tag from it)
	std::string initialTagReason;                                      //and its reason (at most 255 visible ASCII characters)

	iccp::mms::ObjectName Object() const
	{
		return scope == iccp::tase2::Scope::Icc ? iccp::mms::ObjectName::Domain(domain, name) : iccp::mms::ObjectName::Vmd(name);
	}
	//the CheckBackID a served device returns to a select
	int64_t ServedCheckBackId() const
	{
		return checkBackId.value_or(iccp::tase2::DefaultCheckBackId);
	}
	//the time the whole operation waits for the peer (Operate), or for downstream (Serve), with the default applied
	uint32_t EffectiveTimeoutMs() const
	{
		return timeoutMs != 0 ? timeoutMs : (direction == ControlDirection::Serve ? 4000u : 15000u);
	}
};

struct DataSetConf
{
	std::string name;
	std::string domain;
	PointDirection direction = PointDirection::Serve; //Serve: held by us (the peer may use it); Read: we define it on the peer
	std::vector<std::string> members;                 //names of configured points of the same direction
	uint64_t pollIntervalMs = 0;                      //Read data sets: poll with a Read, 0 = no polling
};

struct TransferSetConf
{
	std::string name;    //a fixed transfer set name on the peer; empty = ask Next_DSTransfer_Set
	std::string dataSet; //the Read data set this transfer set reports

	iccp::tase2::DsConditions conditions{iccp::tase2::DsCondition::IntervalTimeOut | iccp::tase2::DsCondition::IntegrityTimeOut |
		                               iccp::tase2::DsCondition::ObjectChange, iccp::tase2::DsConditionBitCount};
	int64_t startTime = 0;
	int64_t interval = 0;
	int64_t tle = 0;
	int64_t bufferTime = 0;
	int64_t integrityCheck = 0;
	bool blockData = false;
	bool critical = false;
	bool rbe = false;
	bool allChangesReported = false; //TRUE only with 2000-08, RBE, the ObjectChange condition and a BufferTime above 0 (checked at load)
	int64_t eventCode = 0;
	//the special objects at the head of the data set the client defines, in order
	std::vector<iccp::tase2::Special> specials{iccp::tase2::Special::TransferSetName, iccp::tase2::Special::ConditionsDetected};
};

class ICCPPortConf: public DataPortConf
{
public:
	ICCPPortConf() = default;

	std::vector<std::string> warnings; //found by Load(): logged by the port when it is built, the configuration is still valid
	bool tcpListener = false;          //true: this port accepts the TCP connection; false: it connects. Decides who initiates the association.
	ServiceRole role = ServiceRole::Client;

	bool HasServerSide() const
	{
		return role != ServiceRole::Client;
	}
	bool HasClientSide() const
	{
		return role != ServiceRole::Server;
	}

	//connection
	std::string ip = "127.0.0.1";
	std::string port = "102";
	uint16_t tcpConnectRetryMs = 0; //0 = exponential back-off
	uint64_t tcpThrottleBitrate = 0;
	uint64_t tcpThrottleChunksize = 0;
	uint64_t tcpThrottleWriteDelayMs = 0;

	//secure ICCP (IEC 62351-3): TLS over the TCP connection, the "TLS" section. The TLS role follows the TCP direction.
	//tlsContext is made by Load() (so that a missing or wrong file is a configuration error) when tlsConf.enabled.
	iccp::tls::TlsConf tlsConf = iccp::IccpTlsDefaults();
	std::shared_ptr<iccp::tls::Context> tlsContext;

	//authentication in the ACSE association (MACE), the "MACE" section; independent of TLS and of the TCP direction.
	//maceContext is made by Load() (so that a missing or wrong file is a configuration error) when maceConf.enabled.
	iccp::mace::MaceConf maceConf;
	std::shared_ptr<iccp::mace::Context> maceContext;

	//addressing
	iccp::stack::Identity local;
	iccp::stack::Identity remote;
	uint8_t tpduSizeCode = iccp::cotp::TpduSize1024;

	//TASE.2
	iccp::tase2::Version edition = iccp::tase2::Edition2000;
	std::string bilateralTableId;             //advertised by us
	std::string remoteBilateralTableId;       //expected from the peer, empty = not checked
	std::string localDomain;                  //the ICC domain this side serves
	std::string remoteDomain;                 //the peer's ICC domain that this side reads from
	std::set<int> supportedBlocks{1, 2};      //advertised
	std::set<int> requiredRemoteBlocks{1};    //client: the peer must advertise these
	bool dsTransferSetStatusMember = false;   //client: write the 13 member DSTransferSet (the 2000-08 form, with AllChangesReported) instead of the 12 member 1996-08 form
	bool dsTransferSetStatusExplicit = false; //DSTransferSetStatusMember was given: otherwise the form follows the peer's TASE2_Version once it is known
	bool requireMatchingEdition = true;       //the peer's TASE2_Version must equal ours: otherwise a mismatch is only a warning
	std::string vendorName = "opendatacon";
	std::string modelName = "ICCP";
	std::string revision = "1.0";

	//timers and MMS parameters
	uint32_t requestTimeoutMs = 5000;
	uint32_t associateTimeoutMs = 10000;
	uint32_t releaseTimeoutMs = 5000;
	int64_t maxOutstandingCalling = 10;
	int64_t maxOutstandingCalled = 10;
	int64_t nestingLevel = 4;

	//server limits
	bool abortOnBilateralTableMismatch = false;
	size_t maxClientDataSets = 16;
	size_t maxDataSetMembers = 5000;
	size_t namePageSize = 200;
	size_t maxBufferedChanges = 1000; //server: changes buffered per transfer set with AllChangesReported (see Tase2Responder.h); beyond it only the last value of a point is kept
	//server, Block 3 (blocked transfers, only with 3 in SupportedBlocks): the item id of the VMD-specific variable that carries the blocked octet string. A real peer uses Block_Data [V]
	std::string blockedDataName = iccp::tase2::blocked::DefaultVariableName;
	//server, Block 3: send a partial report (RBE) that needs a position above 255 blocked with rule 2 (2 octet positions, from the standard's text only: never seen from a real peer, which may
	//not implement it). false = the old behaviour: such a report is sent in the ordinary form, with one warning per start of the transfer set.
	bool blockedRule2 = true;
	//server: critical reports (a transfer set the peer started with Critical TRUE) wait for the client's acknowledgement; none is not a reason to stop reporting [S]
	uint32_t reportAckTimeoutMs = 10000;   //how long an acknowledgement may take before it counts as a Data_failure; 0 = no timeout check
	size_t maxUnacknowledgedReports = 100; //reports awaiting acknowledgement per transfer set; beyond it the oldest counts as a Data_failure

	//client: what to do with the transfer reports received [S]
	bool acknowledgeCriticalReports = true; //send Transfer_Report_ACK for the reports of a transfer set started with Critical TRUE
	bool nackUnknownReports = true;         //discard a transfer report that is not from a transfer set started here and send Transfer_Report_NACK; false: as before (published, no NACK)

	//diagnostics: log the bytes sent and received on the TCP connection (hex, at debug level)
	bool traceHex = false;
	size_t traceHexMaxBytes = 512; //per log line; longer chunks are cut

	//diagnostics: after a data set definition fails with a definition class error, read its members to find the ones the peer lacks
	bool probeOnDefineFailure = true;
	size_t probeChunkSize = 50; //variables per probe Read

	//what to do with a data set whose definition the peer refuses because it lacks some of the members (client side)
	MissingMemberPolicy missingMemberPolicy = MissingMemberPolicy::Prune;
	//the quality published for the points of members that are not available on the peer
	odc::QualityFlags missingMemberQuality = odc::QualityFlags::COMM_LOST;
	std::string missingMemberQualityText = "COMM_LOST";

	//points, data sets, transfer sets
	std::vector<IccpPointConf> points;
	std::vector<IccpControlConf> controls; //TASE.2 Block 5 [S]: the devices this port serves or operates
	std::vector<DataSetConf> dataSets;
	std::vector<TransferSetConf> transferSets; //the transfer sets this port starts on the peer (Client side)
	std::vector<std::string> transferSetPool;  //the transfer set names this port offers to the peer (Server side)

	//Parse the JSON root. Returns the problems found (empty = valid). Clears and refills the lists.
	//tcpListener only chooses the default ServiceRole (a listener serves, an initiator is a client); the schema is the same for both.
	std::vector<std::string> Load(const Json::Value& root, bool tcpListener);

	//the stack configuration this implies (clock left at its default)
	iccp::stack::Config StackConfig() const;
	iccp::tase2::LocalAssociation LocalAssociation() const;
	//the configured point / data set with this name and direction
	const IccpPointConf* FindPoint(const std::string& name, PointDirection direction) const;
	const IccpControlConf* FindControl(const std::string& name, ControlDirection direction) const;
	const IccpControlConf* FindControl(ControlKind kind, size_t index, ControlDirection direction) const;
	bool HasControls(ControlDirection direction) const;
	//a device this port serves has the Tag attribute (Taggable)
	bool HasTaggableControls() const;
	const DataSetConf* FindDataSet(const std::string& name, PointDirection direction) const;
};

#endif /* ICCPPORTCONF_H_ */
