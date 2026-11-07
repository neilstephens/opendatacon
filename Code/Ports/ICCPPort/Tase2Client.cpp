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
 * Tase2Client.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The TASE.2 client side of one association, see Tase2Client.h.
 */

#include "Tase2Client.h"
#include "MmsNames.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace iccp
{
namespace tase2
{

namespace
{
//why an outcome did not give the response that was wanted (Describe() says "ok" for a response of the wrong kind)
std::string Why(const stack::Outcome& o)
{
	return o.Ok() ? "the peer answered with an unexpected kind of response" : stack::Describe(o);
}

const char* SpecialItem(Special s)
{
	switch(s)
	{
		case Special::TransferSetName: return Name::TransferSetName;
		case Special::TimeStamp: return Name::TransferSetTimeStamp;
		case Special::ConditionsDetected: return Name::DsConditionsDetected;
		case Special::EventCode: return Name::EventCodeDetected;
	}
	return "";
}
}

void VerifyPeer(stack::Connection& conn, const ICCPPortConf& conf, bool checkBlocks, const LogFn& log,
	std::function<void(bool, const std::string&, const AssociationInfo&)> done)
{
	mms::ReadRequest request;
	if(!conf.remoteDomain.empty())
		request = BuildAssociationInfoRequest(conf.remoteDomain);
	else
		request.specification = mms::ListOfVariable{mms::VariableSpec{VccName(Name::Tase2Version), std::nullopt},
			                                      mms::VariableSpec{VccName(Name::SupportedFeatures), std::nullopt}
		}
	;

	conn.SendRequest(request, [request, &conf, checkBlocks, log, done](const stack::Outcome& o)
		{
			const auto* rr = o.Get<mms::ReadResponse>();
			if(rr == nullptr)
			{
				done(false, "reading the peer's association objects failed: " + Why(o), AssociationInfo());
				return;
			}
			AssociationInfo info;
			try
			{
				info = ParseAssociationInfoResponse(request, *rr);
			}
			catch(const DecodeError& e)
			{
				done(false, std::string("the peer's association objects are malformed: ") + e.what(), AssociationInfo());
				return;
			}
			std::string problem;
			if(info.bilateralTableId && info.bilateralTableId->size() > BilateralTableIdWidth && log)
				log(stack::LogLevel::Warn, "the peer's Bilateral_Table_ID is " + std::to_string(info.bilateralTableId->size()) + " characters long, more than the " + std::to_string(BilateralTableIdWidth) +
					" of its type Visible-String-32 (IEC 60870-6-503 clause 8.3.3): it is used as it is");
			if(!conf.remoteBilateralTableId.empty())
			{
				if(!info.bilateralTableId)
					problem = "the peer did not return its Bilateral_Table_ID";
				else if(*info.bilateralTableId != conf.remoteBilateralTableId)
					problem = "Bilateral_Table_ID mismatch (expected \"" + conf.remoteBilateralTableId + "\", the peer reports \"" + *info.bilateralTableId + "\")";
			}
			if(problem.empty() && checkBlocks && !conf.requiredRemoteBlocks.empty())
			{
				if(!info.features)
					problem = "the peer did not return its Supported_Features";
				else
					for(int b : conf.requiredRemoteBlocks)
						if(!HasBlock(*info.features, static_cast<Block>(b - 1)))
						{
							problem = "the peer does not support TASE.2 block " + std::to_string(b);
							break;
						}
			}
			if(info.version && !(*info.version == conf.edition))
			{
				const std::string mismatch = "the peer reports TASE.2 version " + std::to_string(info.version->major) + "-" + std::to_string(info.version->minor) +
				                             " but this port is configured for " + std::to_string(conf.edition.major) + "-" + std::to_string(conf.edition.minor) +
				                             " (TASE2Edition)";
				if(conf.requireMatchingEdition && problem.empty())
					problem = mismatch + ": set TASE2Edition to the peer's (or RequireMatchingEdition to false to go on anyway)";
				else if(!conf.requireMatchingEdition && log)
					log(stack::LogLevel::Warn, mismatch + ": the editions differ in places (for example the DSTransferSet), so this may not work");
			}
			else if(log && info.version)
				log(stack::LogLevel::Info, "the peer reports TASE.2 version " + std::to_string(info.version->major) + "-" + std::to_string(info.version->minor));
			done(problem.empty(), problem, info);
		});
}

// ---------------------------------------------------------------------------------------------
// client session
// ---------------------------------------------------------------------------------------------

std::string ClientSession::Key(const mms::ObjectName& n)
{
	return std::string(1, static_cast<char>('0' + static_cast<int>(n.scope))) + "/" + n.domainId + "/" + n.itemId;
}

ClientSession::ClientSession(const ICCPPortConf& c, stack::Connection& connection, Timebase tb, LogFn l, Callbacks callbacks):
	conf(c),
	conn(connection),
	time(std::move(tb)),
	log(std::move(l)),
	cb(std::move(callbacks))
{
	for(const auto& p : conf.points)
		if(p.direction == PointDirection::Read)
			pointsByName[Key(p.Object())] = &p;

	auto provisionFor = [this](const DataSetConf& ds) -> Provision&
				  {
					  const mms::ObjectName name = mms::ObjectName::Domain(ds.domain, ds.name);
					  for(auto& p : provisions)
						  if(p.name == name)
							  return p;
					  Provision p;
					  p.dataSet = &ds;
					  p.name = name;
					  provisions.push_back(std::move(p));
					  return provisions.back();
				  };

	for(const auto& t : conf.transferSets)
	{
		const DataSetConf* ds = conf.FindDataSet(t.dataSet, PointDirection::Read);
		if(ds == nullptr)
			continue;
		Provision& p = provisionFor(*ds);
		if(p.transfer != nullptr)
		{
			Log(stack::LogLevel::Warn, "data set " + ds->name + " is used by more than one transfer set; only the first is used");
			continue;
		}
		p.transfer = &t;
		p.layout = t.specials;
	}
	for(const auto& d : conf.dataSets)
		if(d.direction == PointDirection::Read && d.pollIntervalMs > 0)
			provisionFor(d).poll = true;

	for(auto& p : provisions)
	{
		if(p.transfer != nullptr)
			for(Special s : p.layout)
			{
				p.members.push_back(mms::ObjectName::Domain(conf.remoteDomain, SpecialItem(s)));
				p.memberPoints.push_back(nullptr);
			}
		for(const auto& m : p.dataSet->members)
			if(const auto* pt = conf.FindPoint(m, PointDirection::Read))
			{
				p.members.push_back(pt->Object());
				p.memberPoints.push_back(pt);
			}
		p.fullMembers = p.members;
		p.fullPoints = p.memberPoints;
		p.status.dataSet = p.name.itemId;
		p.status.domain = p.name.domainId;
		p.status.members = p.members.size();
		if(p.transfer != nullptr && !p.transfer->name.empty())
			p.status.transferSet = p.transfer->name;
		if(p.transfer != nullptr)
		{
			const TransferSetConf& t = *p.transfer;
			DsTransferSet& d = p.parameters;
			d.dataSetName = ObjectRef{Scope::Icc, p.dataSet->domain, p.dataSet->name};
			d.startTime = t.startTime;
			d.interval = t.interval;
			d.tle = t.tle;
			d.bufferTime = t.bufferTime;
			d.integrityCheck = t.integrityCheck;
			d.conditionsRequested = t.conditions;
			d.blockData = t.blockData;
			d.critical = t.critical;
			d.rbe = t.rbe;
			d.allChangesReported = t.allChangesReported;
			d.allChangesMember = conf.dsTransferSetStatusMember;
			d.eventCodeRequested = t.eventCode;
		}
	}
}

size_t ClientSession::ActiveTransferSets() const
{
	size_t n = 0;
	for(const auto& p : provisions)
		n += p.active ? 1 : 0;
	return n;
}

ClientSession::Provision* ClientSession::FindByName(const mms::ObjectName& n)
{
	for(auto& p : provisions)
		if(p.name == n)
			return &p;
	return nullptr;
}

void ClientSession::Send(mms::ConfirmedServiceRequest request, std::function<void(const stack::Outcome&)> handler)
{
	std::weak_ptr<bool> guard = alive;
	conn.SendRequest(std::move(request), [guard, handler = std::move(handler)](const stack::Outcome& o)
		{
			if(guard.expired())
				return;
			handler(o);
		});
}

void ClientSession::OnAssociated()
{
	std::weak_ptr<bool> guard = alive;
	VerifyPeer(conn, conf, true, log, [this, guard](bool ok, const std::string& problem, const AssociationInfo& peer)
		{
			if(guard.expired() || shuttingDown)
				return;
			if(!ok)
			{
				Log(stack::LogLevel::Error, problem);
				if(cb.onFatal)
					cb.onFatal(problem);
				return;
			}
			//unless configured, the peer's edition decides the DSTransferSet form: 2000-08 peers write and expect the 13 member form
			//(with AllChangesReported), 1996-08 peers the 12 member form
			if(peer.version && !conf.dsTransferSetStatusExplicit)
				for(auto& p : provisions)
					if(p.transfer != nullptr)
						p.parameters.allChangesMember = peer.version->major >= 2000;
			ready = true;
			//Block 3: BlockData asks the peer for blocked reports, which only a peer that advertises Block 3 can be expected to honour
			for(const auto& p : provisions)
				if(p.transfer != nullptr && p.parameters.blockData && !(peer.features && HasBlock(*peer.features, Block::Block3)))
					Log(stack::LogLevel::Warn, "the transfer set for data set " + p.name.itemId + " is started with BlockData true, but the peer " +
						(peer.features ? "does not advertise Block 3 in its Supported_Features" : "did not return its Supported_Features") +
						": it may ignore BlockData (its reports are then ordinary, and are handled) or refuse the transfer set");
			if(cb.onReady)
				cb.onReady();
			ProvisionNext(0);
		});
}

void ClientSession::ProvisionNext(size_t index)
{
	if(shuttingDown)
		return;
	if(index >= provisions.size())
	{
		provisioningWalked = true;
		MaybeFinishProvisioning();
		return;
	}
	Provision& p = provisions[index];

	auto defineAndStart = [this, index]()
				    {
					    Provision& p = provisions[index];
					    //delete first, as the real peers do: the data set may be left over from an earlier association (any answer is fine)
					    Send(BuildDeleteDataSet(p.name), [this, index](const stack::Outcome& o)
						    {
							    if(shuttingDown)
								    return;
							    Provision& p = provisions[index];
							    if(!o.Ok())
							    {
								    //a data set that does not exist is the normal case on a fresh peer: not worth more than a debug line
								    const bool expected = o.kind == stack::Outcome::Kind::Error &&
								                          (o.error.errorClass == mms::ErrorClass::Definition || o.error.errorClass == mms::ErrorClass::Access);
								    Log(expected ? stack::LogLevel::Debug : stack::LogLevel::Warn,
									    "DeleteNamedVariableList failed for " + Subject(p, false) + " (provisioning goes on): " + Describe(o));
							    }
							    DataSetDefinition def;
							    def.name = p.name;
							    def.members = p.members;
							    Send(BuildCreateDataSet(def), [this, index](const stack::Outcome& o)
								    {
									    if(shuttingDown)
										    return;
									    Provision& p = provisions[index];
									    if(o.Get<mms::DefineNamedVariableListResponse>() == nullptr)
									    {
										    //which members does the peer lack? Only a definition class error points at the members. With the Prune policy the probe is
										    //what the recovery rests on (and the first failure is only a warning: it may well be recovered); with Fail it is a diagnostic.
										    const bool prune = conf.missingMemberPolicy == MissingMemberPolicy::Prune;
										    const bool probe = o.kind == stack::Outcome::Kind::Error && o.error.errorClass == mms::ErrorClass::Definition &&
										                       (prune || conf.probeOnDefineFailure);
										    Failed(index, ProvisionState::DefineFailed, "DefineNamedVariableList", Why(o), false,
											    (probe && prune) ? stack::LogLevel::Warn : stack::LogLevel::Error);
										    if(probe)
											    StartProbe(index);
										    ProvisionNext(index + 1);
										    return;
									    }
									    p.defined = true;
									    p.status.state = ProvisionState::Defined;
									    p.status.lastError.clear();
									    NotifyStatus();
									    Log(stack::LogLevel::Info, "data set " + p.name.itemId + " defined with " + std::to_string(p.members.size()) + " members");
									    if(p.transfer != nullptr)
										    StartTransfer(index, [this, index]() { FinishProvision(index); });
									    else
										    FinishProvision(index);
								    });
						    });
				    };

	if(p.transfer == nullptr)
	{
		defineAndStart();
		return;
	}
	if(!p.transfer->name.empty())
	{
		p.transferSetName = mms::ObjectName::Domain(conf.remoteDomain, p.transfer->name);
		defineAndStart();
		return;
	}
	Send(BuildGetNextDsTransferSet(conf.remoteDomain), [this, index, defineAndStart](const stack::Outcome& o)
		{
			if(shuttingDown)
				return;
			Provision& p = provisions[index];
			const auto* rr = o.Get<mms::ReadResponse>();
			std::string problem;
			try
			{
				if(rr == nullptr)
					problem = Why(o);
				else
					p.transferSetName = ParseNextDsTransferSet(*rr).ToName();
			}
			catch(const DecodeError& e)
			{
				problem = std::string("the Next_DSTransfer_Set answer is not usable: ") + e.what();
			}
			if(!problem.empty())
			{
				Failed(index, ProvisionState::TransferFailed, "Next_DSTransfer_Set (Read in domain " + conf.remoteDomain + ")", problem, false);
				ProvisionNext(index + 1);
				return;
			}
			p.status.transferSet = p.transferSetName->itemId;
			NotifyStatus();
			defineAndStart();
		});
}

void ClientSession::StartTransfer(size_t index, std::function<void()> done)
{
	Provision& p = provisions[index];
	Send(BuildStartTransfer(*p.transferSetName, p.parameters), [this, index, done = std::move(done)](const stack::Outcome& o)
		{
			if(shuttingDown)
				return;
			Provision& p = provisions[index];
			const auto* wr = o.Get<mms::WriteResponse>();
			if(wr == nullptr || wr->results.size() != 1 || !wr->results[0].success)
			{
				std::string why = Why(o);
				if(wr != nullptr && wr->results.size() != 1)
					why = "the Write answer has " + std::to_string(wr->results.size()) + " results for 1 object";
				else if(wr != nullptr && !wr->results.empty() && !wr->results[0].success)
				{
					why = "data access error " + mms::DescribeDataAccessError(wr->results[0].error);
					if(wr->results[0].error == mms::DataAccessError::ObjectAttributeInconsistent)
					{
						//the peer parsed the structure and refuses one of the values
						const DsTransferSet& d = p.parameters;
						auto yn = [](bool b) { return b ? "true" : "false"; };
						std::string conditions;
						const std::pair<uint8_t, const char*> names[] = {
							{DsCondition::IntervalTimeOut, "IntervalTimeOut"}, {DsCondition::IntegrityTimeOut, "IntegrityTimeOut"},
							{DsCondition::ObjectChange, "ObjectChange"}, {DsCondition::OperatorRequest, "OperatorRequest"},
							{DsCondition::OtherExternalEvent, "OtherExternalEvent"}
						};
						for(const auto& n : names)
							if(d.conditionsRequested.Has(n.first))
								conditions += (conditions.empty() ? "" : "+") + std::string(n.second);
						why += "; the peer finds a value of the DSTransferSet inconsistent (sent: Interval " + std::to_string(d.interval) + ", TLE " + std::to_string(d.tle) +
						       ", BufferTime " + std::to_string(d.bufferTime) + ", IntegrityCheck " + std::to_string(d.integrityCheck) + ", conditions " +
						       (conditions.empty() ? "none" : conditions) + ", BlockData " + yn(d.blockData) + ", Critical " + yn(d.critical) +
						       ", RBE " + yn(d.rbe) + ", AllChangesReported " + (d.allChangesMember ? yn(d.allChangesReported) : "not sent (12 member form)") +
						       "). A real 2000-08 peer accepts AllChangesReported true only with RBE true, the ObjectChange condition and a BufferTime above 0, and requires false otherwise";
					}
				}
				Failed(index, ProvisionState::TransferFailed, "start transfer (Write of the DSTransferSet)", why, true);
			}
			else
			{
				p.active = true;
				p.status.state = ProvisionState::TransferStarted;
				p.status.lastError.clear();
				NotifyStatus();
				Log(stack::LogLevel::Info, "transfer set " + p.transferSetName->itemId + " started for data set " + p.name.itemId);
			}
			done();
		});
}

void ClientSession::FinishProvision(size_t index)
{
	Provision& p = provisions[index];
	if(p.poll)
		p.nextPoll = time.steady();
	ProvisionNext(index + 1);
}

// ---------------------------------------------------------------------------------------------
// diagnostics
// ---------------------------------------------------------------------------------------------

const char* ToString(ProvisionState s)
{
	switch(s)
	{
		case ProvisionState::Pending: return "pending";
		case ProvisionState::Defined: return "defined";
		case ProvisionState::DefinedPartial: return "defined-partial";
		case ProvisionState::DefineFailed: return "define-failed";
		case ProvisionState::TransferStarted: return "transfer-started";
		case ProvisionState::TransferFailed: return "transfer-failed";
	}
	return "?";
}

std::vector<ProvisionStatus> ClientSession::Status() const
{
	std::vector<ProvisionStatus> v;
	v.reserve(provisions.size());
	for(const auto& p : provisions)
		v.push_back(p.status);
	return v;
}

void ClientSession::NotifyStatus() const
{
	if(cb.onProvisioning)
		cb.onProvisioning(Status());
}

//"data set NAME (domain DOM), N members, first "X", last "Y"" and, for a transfer set operation, the transfer set too
std::string ClientSession::Subject(const Provision& p, bool withTransferSet) const
{
	auto shown = [](const std::string& s)
			 {
				 return "\"" + (s.size() > 80 ? s.substr(0, 80) + "..." : s) + "\"";
			 };
	std::string s;
	if(withTransferSet && p.transferSetName)
		s += "transfer set " + p.transferSetName->itemId + " (domain " + p.transferSetName->domainId + ") for ";
	s += "data set " + p.name.itemId + " (domain " + p.name.domainId + "), " + std::to_string(p.members.size()) + " members";
	if(!p.members.empty())
		s += ", first " + shown(p.members.front().itemId) + ", last " + shown(p.members.back().itemId);
	return s;
}

//one error line for a failed provisioning step, and the state to show
void ClientSession::Failed(size_t index, ProvisionState state, const std::string& operation, const std::string& why, bool withTransferSet,
	stack::LogLevel level)
{
	Provision& p = provisions[index];
	p.status.state = state;
	p.status.lastError = operation + ": " + why;
	NotifyStatus();
	Log(level, operation + " failed for " + Subject(p, withTransferSet) + ": " + why);
}

//"ICC, domain DOMAIN_A; ODC Analog index 11" for a configured point, "ICC, domain D; special object" for a special object
std::string ClientSession::Where(const IccpPointConf* pt, const mms::ObjectName& name) const
{
	if(pt == nullptr)
		return "ICC, domain " + name.domainId + "; special object";
	std::string w = pt->scope == Scope::Icc ? "ICC, domain " + pt->domain : std::string("VCC");
	w += "; ODC " + std::string(pt->kind == PointKind::Binary ? "Binary" : (pt->kind == PointKind::Analog ? "Analog" : "Counter")) + " index " +
	     std::to_string(pt->index);
	return w;
}

void ClientSession::EndRecovery()
{
	if(recoveriesInFlight > 0)
		--recoveriesInFlight;
	MaybeFinishProvisioning();
}

//The end of the provisioning: the points that are not available are flagged (once each), then the summary.
void ClientSession::MaybeFinishProvisioning()
{
	if(!provisioningWalked || recoveriesInFlight > 0 || summaryLogged || shuttingDown || provisions.empty() || !conn.IsAssociated())
		return;
	summaryLogged = true;
	const bool prune = conf.missingMemberPolicy == MissingMemberPolicy::Prune;

	if(prune)
	{
		auto flag = [this](const IccpPointConf* pt)
				{
					if(pt == nullptr || !flagged.insert(Key(pt->Object())).second)
						return;
					flaggedList.push_back(pt);
				};
		//the points whose members a probe found missing on the peer
		for(const IccpPointConf* pt : missingOrder)
			flag(pt);
		//the points of the data sets that could not be provisioned, unless a data set that was provisioned carries them
		std::set<std::string> provided;
		for(const auto& p : provisions)
			if(p.defined)
				for(const IccpPointConf* pt : p.memberPoints)
					if(pt != nullptr)
						provided.insert(Key(pt->Object()));
		for(const auto& p : provisions)
		{
			const bool unprovisioned = p.status.state == ProvisionState::DefineFailed || (p.status.state == ProvisionState::TransferFailed && !p.defined);
			if(!unprovisioned)
				continue;
			for(size_t i = 0; i < p.fullPoints.size(); ++i)
			{
				const IccpPointConf* pt = p.fullPoints[i];
				if(pt == nullptr || provided.count(Key(pt->Object())) || missingKeys.count(Key(pt->Object())))
					continue;
				Log(stack::LogLevel::Warn, "data set " + p.name.itemId + ": member " + pt->name + " (" + Where(pt, p.fullMembers[i]) +
					") is not available because the data set could not be provisioned; flagged " + conf.missingMemberQualityText);
				flag(pt);
			}
		}
		const odc::msSinceEpoch_t now = time.wallMs();
		for(const IccpPointConf* pt : flaggedList)
		{
			PointState s;
			s.known = false;
			s.quality = conf.missingMemberQuality;
			s.timestamp = now;
			if(cb.onUnavailable)
				cb.onUnavailable(*pt, s);
		}
	}

	size_t ok = 0, partial = 0;
	std::string failed;
	size_t failures = 0;
	for(const auto& p : provisions)
	{
		if(p.status.state == ProvisionState::Defined || p.status.state == ProvisionState::DefinedPartial || p.status.state == ProvisionState::TransferStarted)
		{
			++ok;
			partial += p.status.partial ? 1 : 0;
		}
		else
			failed += (failures++ ? ", " : "") + p.status.dataSet;
	}
	std::string text = "provisioning finished: " + std::to_string(ok) + " data sets ok";
	if(partial)
		text += " (" + std::to_string(partial) + " partial)";
	text += ", " + std::to_string(failures) + " failed";
	if(failures)
		text += ": " + failed;
	if(!flaggedList.empty())
		text += "; " + std::to_string(flaggedList.size()) + " points flagged " + conf.missingMemberQualityText + " (not available on the peer)";
	Log(failures ? stack::LogLevel::Error : ((partial || !flaggedList.empty()) ? stack::LogLevel::Warn : stack::LogLevel::Info), text);
}

//The state of one probe: the points among the members are read as a list of variables, a chunk at a time, one Read outstanding at a time.
//The special objects (Transfer_Set_Name and the like) are not variables a peer has to serve for a Read: they are not probed, but counted as present.
struct ClientSession::ProbeRun
{
	size_t index = 0;
	std::vector<mms::ObjectName> members; //all the members of the data set, as configured
	std::vector<const IccpPointConf*> memberPoints;
	std::vector<size_t> positions; //the position in `members` of each variable that is read
	size_t next = 0;               //the first variable of the next chunk
	ProbeResult result;
};

void ClientSession::StartProbe(size_t index)
{
	const Provision& p = provisions[index];
	auto run = std::make_shared<ProbeRun>();
	run->index = index;
	run->members = p.members;
	run->memberPoints = p.memberPoints;
	for(size_t i = 0; i < p.members.size(); ++i)
		if(i < p.memberPoints.size() && p.memberPoints[i] != nullptr)
			run->positions.push_back(i);
	run->result.dataSet = p.name.itemId;
	run->result.domain = p.name.domainId;
	run->result.members = p.members.size();
	run->result.probed = run->result.present = p.members.size() - run->positions.size();
	++recoveriesInFlight;
	Log(stack::LogLevel::Warn, "probing the members of data set " + p.name.itemId + " (domain " + p.name.domainId + ") on the peer to find which ones it rejects: " +
		std::to_string(run->positions.size()) + " points in Reads of up to " + std::to_string(conf.probeChunkSize));
	ProbeNextChunk(run);
}

void ClientSession::ProbeNextChunk(const std::shared_ptr<ProbeRun>& run)
{
	if(run->next >= run->positions.size())
	{
		run->result.complete = true;
		EndProbe(run);
		return;
	}
	const size_t first = run->next;
	const size_t count = std::min(conf.probeChunkSize, run->positions.size() - first);
	mms::ReadRequest request;
	mms::ListOfVariable list;
	list.reserve(count);
	for(size_t i = first; i < first + count; ++i)
		list.push_back(mms::VariableSpec{run->members[run->positions[i]], std::nullopt});
	request.specification = std::move(list);
	Send(std::move(request), [this, run, first, count](const stack::Outcome& o)
		{
			if(shuttingDown)
			{
				run->result.failure = "the association is closing";
				EndProbe(run);
				return;
			}
			const std::string range = "members " + std::to_string(run->positions[first]) + " to " + std::to_string(run->positions[first + count - 1]);
			const auto* rr = o.Get<mms::ReadResponse>();
			if(rr == nullptr)
			{
				run->result.failure = "the Read of " + range + " failed: " + Why(o);
				EndProbe(run);
				return;
			}
			if(rr->results.size() != count)
			{
				run->result.failure = "the Read of " + range + " was answered with " + std::to_string(rr->results.size()) + " results for " +
				                      std::to_string(count) + " variables";
				EndProbe(run);
				return;
			}
			ProbeResult& r = run->result;
			++r.chunks;
			for(size_t i = 0; i < count; ++i)
			{
				const mms::AccessResult& a = rr->results[i];
				const size_t position = run->positions[first + i];
				++r.probed;
				if(a.success)
				{
					++r.present;
					continue;
				}
				const IccpPointConf* pt = run->memberPoints[position];
				ProbeResult::Entry e;
				e.index = position;
				e.name = run->members[position].itemId;
				e.error = a.error;
				e.scope = pt->scope == Scope::Icc ? "ICC" : "VCC";
				e.domain = pt->scope == Scope::Icc ? pt->domain : std::string();
				e.kind = pt->kind == PointKind::Binary ? "Binary" : (pt->kind == PointKind::Analog ? "Analog" : "Counter");
				e.odcIndex = pt->index;
				if(a.error == mms::DataAccessError::ObjectNonExistent || a.error == mms::DataAccessError::ObjectUndefined)
				{
					++r.missing;
					r.missingListed.push_back(std::move(e));
				}
				else
				{
					++r.presentWithError;
					r.errorListed.push_back(std::move(e));
				}
			}
			run->next = first + count;
			ProbeNextChunk(run);
		});
}

struct ClientSession::PrunedSet
{
	std::vector<mms::ObjectName> members;
	std::vector<const IccpPointConf*> points;
	size_t missing = 0;
};

void ClientSession::EndProbe(const std::shared_ptr<ProbeRun>& run)
{
	ProbeResult& r = run->result;
	const bool prune = conf.missingMemberPolicy == MissingMemberPolicy::Prune;
	const std::string subject = "data set " + r.dataSet + " (domain " + r.domain + ")";
	auto shortName = [](const std::string& n)
			     {
				     return n.size() > 80 ? n.substr(0, 80) + "..." : n;
			     };
	//Fail policy, one line per member: "missing member 12 of 129 (position 11, 0-based) in data set D (domain X): NAME (ICC, domain X; ODC Analog index 11) - object-non-existent [10]"
	auto memberLine = [&](const char* what, const ProbeResult::Entry& e)
				{
					std::string where = e.scope;
					if(!e.domain.empty())
						where += ", domain " + e.domain;
					where += "; " + (e.odcIndex ? "ODC " + e.kind + " index " + std::to_string(*e.odcIndex) : e.kind);
					return std::string(what) + " member " + std::to_string(e.index + 1) + " of " + std::to_string(r.members) + " (position " + std::to_string(e.index) +
					       ", 0-based) in " + subject + ": " + shortName(e.name) + " (" + where + ") - " + mms::DescribeDataAccessError(e.error);
				};
	//Prune policy, one warning per member: "data set D: member NAME (ICC, domain X; ODC Analog index 11) does not exist on the peer; flagged COMM_LOST"
	auto flagLine = [&](const ProbeResult::Entry& e, const char* what)
			    {
				    std::string where = e.scope;
				    if(!e.domain.empty())
					    where += ", domain " + e.domain;
				    where += "; " + (e.odcIndex ? "ODC " + e.kind + " index " + std::to_string(*e.odcIndex) : e.kind);
				    return "data set " + r.dataSet + ": member " + shortName(e.name) + " (" + where + ") " + what;
			    };
	for(const auto& e : r.missingListed)
		if(prune)
			Log(stack::LogLevel::Warn, flagLine(e, ("does not exist on the peer; flagged " + conf.missingMemberQualityText).c_str()));
		else
			Log(stack::LogLevel::Error, memberLine("missing", e));
	for(const auto& e : r.errorListed)
		if(prune)
			Log(stack::LogLevel::Warn, flagLine(e, ("exists on the peer but is not readable (" + mms::DescribeDataAccessError(e.error) + "); kept in the data set").c_str()));
		else
			Log(stack::LogLevel::Error, memberLine("present but not readable", e));
	std::string text = "probe of " + subject + ": " + std::to_string(r.probed) + " of " + std::to_string(r.members) +
	                   " members probed, " + std::to_string(r.missing) + " missing, " + std::to_string(r.present) + " present";
	if(r.presentWithError)
		text += ", " + std::to_string(r.presentWithError) + " present but not readable";
	if(!r.complete)
		text += "; the probe is incomplete: " + r.failure;
	if(r.complete && r.missing == 0 && r.presentWithError == 0)
		text += "; every member exists on the peer, so the definition was refused for another reason (the name may already be in use, or a limit was reached)";
	Log(prune ? (r.complete ? stack::LogLevel::Warn : stack::LogLevel::Error) : ((r.missing || r.presentWithError) ? stack::LogLevel::Error : stack::LogLevel::Warn), text);

	if(run->index < provisions.size())
	{
		Provision& p = provisions[run->index];
		p.status.probe = r;
		NotifyStatus();
	}
	if(cb.onProbe)
		cb.onProbe(r);

	if(!prune)
	{
		EndRecovery();
		return;
	}
	if(shuttingDown)
	{
		EndRecovery();
		return;
	}

	//Prune: the points whose members are missing are flagged (whatever else happens to the data set)
	std::set<size_t> missingPositions;
	for(const auto& e : r.missingListed)
	{
		missingPositions.insert(e.index);
		if(e.index < run->memberPoints.size() && run->memberPoints[e.index] != nullptr &&
		   missingKeys.insert(Key(run->memberPoints[e.index]->Object())).second)
			missingOrder.push_back(run->memberPoints[e.index]);
	}
	if(!r.complete)
	{
		GiveUp(run->index, "the probe could not be completed (" + r.failure + ")");
		return;
	}
	if(r.missing == 0)
	{
		GiveUp(run->index, "every member exists on the peer, so there is nothing to prune");
		return;
	}
	auto pruned = std::make_shared<PrunedSet>();
	pruned->missing = missingPositions.size();
	size_t points = 0;
	for(size_t i = 0; i < run->members.size(); ++i)
	{
		const IccpPointConf* pt = i < run->memberPoints.size() ? run->memberPoints[i] : nullptr;
		if(missingPositions.count(i))
		{
			if(pt == nullptr)
			{
				GiveUp(run->index, "the special object " + run->members[i].itemId + " the transfer set needs is missing on the peer");
				return;
			}
			continue;
		}
		pruned->members.push_back(run->members[i]);
		pruned->points.push_back(pt);
		points += pt != nullptr ? 1 : 0;
	}
	if(points == 0)
	{
		GiveUp(run->index, "every point of the data set is missing on the peer");
		return;
	}
	DefinePruned(run->index, pruned, false);
}

//The data set cannot be provisioned: the error, and the state stays define-failed. Its points are flagged when provisioning finishes.
void ClientSession::GiveUp(size_t index, const std::string& reason)
{
	const Provision& p = provisions[index];
	Log(stack::LogLevel::Error, "data set " + p.name.itemId + " (domain " + p.name.domainId + ") is skipped: " + reason);
	EndRecovery();
}

//Define the data set again with only the members that exist. The first definition failed, so nothing exists; if the peer says the object
//exists after all, delete it and define once more.
void ClientSession::DefinePruned(size_t index, const std::shared_ptr<PrunedSet>& pruned, bool afterDelete)
{
	Provision& p = provisions[index];
	DataSetDefinition def;
	def.name = p.name;
	def.members = pruned->members;
	Send(BuildCreateDataSet(def), [this, index, pruned, afterDelete](const stack::Outcome& o)
		{
			if(shuttingDown)
			{
				EndRecovery();
				return;
			}
			Provision& p = provisions[index];
			if(o.Get<mms::DefineNamedVariableListResponse>() != nullptr)
			{
				p.members = pruned->members;
				p.memberPoints = pruned->points;
				p.defined = true;
				p.status.state = ProvisionState::DefinedPartial;
				p.status.partial = true;
				p.status.membersDefined = p.members.size();
				p.status.lastError.clear();
				NotifyStatus();
				Log(stack::LogLevel::Info, "data set " + p.name.itemId + " defined with " + std::to_string(p.members.size()) + " of " + std::to_string(p.fullMembers.size()) +
					" members (" + std::to_string(pruned->missing) + " missing on the peer left out)");
				auto done = [this, index]()
						{
							Provision& p = provisions[index];
							if(p.poll)
								p.nextPoll = time.steady();
							EndRecovery();
						};
				if(p.transfer != nullptr)
					StartTransfer(index, done);
				else
					done();
				return;
			}
			if(!afterDelete && o.kind == stack::Outcome::Kind::Error && o.error.errorClass == mms::ErrorClass::Definition && o.error.errorCode == 5)
			{
				//object-exists
				Send(BuildDeleteDataSet(p.name), [this, index, pruned](const stack::Outcome&)
					{
						if(shuttingDown)
						{
							EndRecovery();
							return;
						}
						DefinePruned(index, pruned, true);
					});
				return;
			}
			Failed(index, ProvisionState::DefineFailed, "DefineNamedVariableList (pruned to " + std::to_string(pruned->members.size()) + " of " +
				std::to_string(p.fullMembers.size()) + " members)", Why(o), false);
			EndRecovery();
		});
}

void ClientSession::PublishValues(const std::vector<mms::ObjectName>& names, const std::vector<mms::AccessResult>& values)
{
	const odc::msSinceEpoch_t now = time.wallMs();
	for(size_t i = 0; i < names.size() && i < values.size(); ++i)
	{
		const std::string key = Key(names[i]);
		const auto it = pointsByName.find(key);
		//a point flagged as not available never gets a value
		if(it == pointsByName.end() || !values[i].success || flagged.count(key))
			continue;
		try
		{
			const PointState s = FromData(*it->second, values[i].data, now);
			++valuesPublished;
			if(cb.onPoint)
				cb.onPoint(*it->second, s);
		}
		catch(const DecodeError& e)
		{
			Log(stack::LogLevel::Warn, "value of " + it->second->name + " has the wrong layout for its configured type: " + e.what());
		}
	}
}

bool ClientSession::NamesTransferSet(const Provision& p, const ObjectRef& transferSet)
{
	return p.transferSetName && p.transferSetName->scope == mms::ObjectScope::DomainSpecific && transferSet.scope == Scope::Icc &&
	       transferSet.domain == p.transferSetName->domainId && transferSet.name == p.transferSetName->itemId;
}

ClientSession::Provision* ClientSession::FindByTransferSet(const ObjectRef& transferSet)
{
	for(auto& p : provisions)
		if(NamesTransferSet(p, transferSet))
			return &p;
	return nullptr;
}

void ClientSession::NackReport(const ObjectRef& transferSet, const std::string& why)
{
	//the standard: a report whose data set or transfer set name is not correct is discarded and answered with Transfer_Report_NACK [T]
	const bool first = nackedTransferSets.insert(transferSet.domain + "/" + transferSet.name).second;
	Log(first ? stack::LogLevel::Warn : stack::LogLevel::Debug, "a transfer report from transfer set " + transferSet.name + " (domain " + transferSet.domain +
		") was discarded: " + why + "; Transfer_Report_NACK is sent" + (first ? " (later ones for this transfer set are logged at debug level)" : ""));
	if(conn.SendReport(BuildTransferReportNack(transferSet)))
		++nacksSent;
	else
		Log(stack::LogLevel::Debug, "Transfer_Report_NACK for " + transferSet.name + " could not be sent");
}

void ClientSession::AcknowledgeReport(const TransferReport& report)
{
	if(!conf.acknowledgeCriticalReports)
	{
		Log(stack::LogLevel::Debug, "report from critical transfer set " + report.transferSet.name + " not acknowledged (AcknowledgeCriticalReports is off)");
		return;
	}
	if(conn.SendReport(BuildTransferReportAck(report.transferSet, report.timeStamp)))
		++acksSent;
	else
		Log(stack::LogLevel::Debug, "Transfer_Report_ACK for " + report.transferSet.name + " could not be sent");
}

void ClientSession::OnReport(const mms::InformationReport& report)
{
	++reportsReceived;
	try
	{
		//Block 3: the blocked form (the special objects and one octet string) is recognised by its shape, whatever the transfer set asked for [S]
		if(const auto blockedReport = blocked::RecognizeBlockedReport(report))
		{
			OnBlockedReport(*blockedReport);
			return;
		}
		std::vector<mms::ObjectName> names;
		TransferReport tr;
		Provision* owner = nullptr; //the transfer set that started this report, when it is known
		if(const auto* dsName = std::get_if<mms::ObjectName>(&report.specification))
		{
			Provision* p = FindByName(*dsName);
			if(p == nullptr)
			{
				Log(stack::LogLevel::Debug, "report for an unknown data set " + dsName->itemId + " ignored");
				if(conf.nackUnknownReports)
				{
					//the first result is the transfer set name (the data set is not known, so its layout is not): NACK it if that much can be read
					std::optional<ObjectRef> transferSet;
					try
					{
						transferSet = ParseTransferReport(report).transferSet;
					}
					catch(const DecodeError&)
					{}
					if(transferSet)
						NackReport(*transferSet, "it names the data set " + dsName->itemId + ", which this client did not define");
				}
				return;
			}
			tr = ParseTransferReport(report, &p->members);
			if(conf.nackUnknownReports && !NamesTransferSet(*p, tr.transferSet))
			{
				NackReport(tr.transferSet, "it names the data set " + dsName->itemId + " but not the transfer set started for it" +
					(p->transferSetName ? " (" + p->transferSetName->itemId + ")" : std::string(", which has none")));
				return;
			}
			owner = p;
			names.assign(p->members.begin() + static_cast<std::ptrdiff_t>(tr.layout.size()), p->members.end());
		}
		else
		{
			tr = ParseTransferReport(report);
			owner = FindByTransferSet(tr.transferSet);
			if(owner == nullptr && conf.nackUnknownReports)
			{
				NackReport(tr.transferSet, "this client started no such transfer set");
				return;
			}
			const auto& list = std::get<mms::ListOfVariable>(report.specification);
			for(size_t i = tr.layout.size(); i < list.size(); ++i)
				names.push_back(list[i].name);
		}
		if(tr.conditionsDetected)
			Log(stack::LogLevel::Debug, "report from " + tr.transferSet.name + " (conditions " + std::to_string(tr.conditionsDetected->mask) + ", " + std::to_string(tr.values.size()) + " values)");
		PublishValues(names, tr.values);
		if(owner != nullptr && owner->parameters.critical)
			AcknowledgeReport(tr);
	}
	catch(const DecodeError& e)
	{
		Log(stack::LogLevel::Warn, std::string("a report could not be interpreted: ") + e.what());
	}
}

void ClientSession::OnBlockedReport(const blocked::BlockedReport& b)
{
	++blockedReceived;
	if(!b.problem.empty())
	{
		BlockedUndecodable(b, b.problem, nullptr);
		return;
	}
	Provision* owner = FindByTransferSet(b.transferSet);
	if(owner == nullptr)
	{
		//no transfer set of ours, so no data set to give the values their types: the same answer as to an ordinary report of a transfer set that is not ours
		if(conf.nackUnknownReports)
		{
			NackReport(b.transferSet, "this client started no such transfer set");
			return;
		}
		BlockedUndecodable(b, "this client started no transfer set " + b.transferSet.name + " (domain " + b.transferSet.domain + "), so the types of the values are not known", nullptr);
		return;
	}
	if(!owner->parameters.blockData && blockedWithoutRequest.insert(b.transferSet.domain + "/" + b.transferSet.name).second)
		Log(stack::LogLevel::Warn, "transfer set " + b.transferSet.name + " was started with BlockData false but a report arrived in the blocked form: it is decoded all the same "
			"(later ones for this transfer set are not mentioned)");
	//the members of the data set, in order: the special objects at its head and the points, as the data set is on the peer now (after a prune, only what exists there); the octet string
	//holds all of them [V]. points is parallel to members (nullptr for a special object).
	std::vector<blocked::Member> members;
	std::vector<const IccpPointConf*> points;
	for(size_t i = 0; i < owner->memberPoints.size(); ++i)
	{
		const IccpPointConf* pt = owner->memberPoints[i];
		if(pt != nullptr)
			members.push_back(blocked::Member::Point(pt->type));
		else
			members.push_back(blocked::Member::Object(i < owner->layout.size() ? owner->layout[i] : Special::TransferSetName));
		points.push_back(pt);
	}
	blocked::Decoded decoded;
	try
	{
		decoded = blocked::Decode(b.data, members);
	}
	catch(const DecodeError& e)
	{
		BlockedUndecodable(b, e.what(), owner);
		return;
	}
	Log(stack::LogLevel::Debug, "blocked report from " + b.transferSet.name + " (variable " + blocked::DisplayName(b.dataName) + ", " + blocked::ToString(decoded.rule) + ", " +
		std::to_string(decoded.items.size()) + " members, " + std::to_string(b.data.size()) + " octets" +
		(b.conditionsDetected ? ", conditions " + std::to_string(b.conditionsDetected->mask) : std::string()) + ")");
	const odc::msSinceEpoch_t now = time.wallMs();
	for(const auto& item : decoded.items)
	{
		if(item.special)
		{
			//the special objects are also separate variables, and those are what the report is taken from (the time stamp, the conditions and the Critical acknowledgement): the copy in the
			//octet string is only compared, and a difference is a note, not a refusal
			const auto& v = item.object;
			if(*item.special == Special::TransferSetName && !(v.transferSet == b.transferSet))
				Log(stack::LogLevel::Debug, "blocked report from " + b.transferSet.name + ": the Transfer_Set_Name inside the octet string (" + v.transferSet.domain + "/" + v.transferSet.name +
					") differs from the variable's (" + b.transferSet.domain + "/" + b.transferSet.name + ")");
			else if(*item.special == Special::TimeStamp && b.timeStamp && v.number != *b.timeStamp)
				Log(stack::LogLevel::Debug, "blocked report from " + b.transferSet.name + ": the Transfer_Set_Time_Stamp inside the octet string (" + std::to_string(v.number) +
					") differs from the variable's (" + std::to_string(*b.timeStamp) + ")");
			else if(*item.special == Special::ConditionsDetected && b.conditionsDetected && v.conditions != b.conditionsDetected->mask)
				Log(stack::LogLevel::Debug, "blocked report from " + b.transferSet.name + ": the DSConditions_Detected inside the octet string (" + std::to_string(v.conditions) +
					") differs from the variable's (" + std::to_string(b.conditionsDetected->mask) + ")");
			continue;
		}
		const IccpPointConf& pt = *points[item.position];
		if(flagged.count(Key(pt.Object())))
			continue; //a point flagged as not available never gets a value
		const PointState s = FromPointValue(pt, item.value, now);
		++valuesPublished;
		if(cb.onPoint)
			cb.onPoint(pt, s);
	}
	if(owner->parameters.critical)
	{
		TransferReport tr;
		tr.transferSet = b.transferSet;
		tr.timeStamp = b.timeStamp;
		AcknowledgeReport(tr);
	}
}

void ClientSession::BlockedUndecodable(const blocked::BlockedReport& b, const std::string& why, const Provision* owner)
{
	++blockedUndecodable;
	std::string variables;
	for(const auto& v : b.variables)
		variables += (variables.empty() ? "" : ", ") + v;
	//the diagnostic for correcting the layout from a capture of a real peer (README, "Blocked transfers (Block 3)")
	Log(stack::LogLevel::Warn, "transfer set " + (b.transferSet.name.empty() ? std::string("(not named)") : b.transferSet.name) + ": a blocked report could not be decoded and none of its values was applied: " + why +
		". Rule byte " + (b.data.empty() ? std::string("(none)") : std::to_string(b.data[0])) + ", " + std::to_string(b.data.size()) + " octets of blocked data" +
		(owner != nullptr ? ", the data set defines " + std::to_string(owner->memberPoints.size()) + " members (" +
		 std::to_string(std::count(owner->memberPoints.begin(), owner->memberPoints.end(), nullptr)) + " of them special objects)" : std::string()) +
		". Variables seen: " + variables + ". Blocked data: " + blocked::HexDump(b.data));
}

void ClientSession::Tick()
{
	if(!ready || shuttingDown)
		return;
	TickControls();
	const auto now = time.steady();
	for(size_t i = 0; i < provisions.size(); ++i)
	{
		Provision& p = provisions[i];
		if(!p.poll || !p.defined || p.pollInFlight || now < p.nextPoll)
			continue;
		p.pollInFlight = true;
		Send(BuildGetDataSetElementValues(p.name), [this, i](const stack::Outcome& o)
			{
				Provision& p = provisions[i];
				p.pollInFlight = false;
				p.nextPoll = time.steady() + std::chrono::milliseconds(p.dataSet->pollIntervalMs);
				const auto* rr = o.Get<mms::ReadResponse>();
				if(rr == nullptr)
				{
					Log(stack::LogLevel::Warn, "polling data set " + p.name.itemId + " failed: " + Why(o));
					return;
				}
				if(rr->results.size() != p.members.size())
				{
					Log(stack::LogLevel::Warn, "polling data set " + p.name.itemId + ": unexpected number of values");
					return;
				}
				PublishValues(p.members, rr->results);
			});
	}
}

void ClientSession::Shutdown(std::function<void()> done)
{
	shuttingDown = true;
	{
		auto ops = controlOps;
		for(auto& [c, op] : ops)
			CompleteControl(op, odc::CommandStatus::UNDEFINED, "the port is being disabled");
	}
	auto remaining = std::make_shared<size_t>(0);
	std::vector<size_t> active;
	for(size_t i = 0; i < provisions.size(); ++i)
		if(provisions[i].active)
			active.push_back(i);
	if(active.empty())
	{
		done();
		return;
	}
	*remaining = active.size();
	for(size_t i : active)
	{
		Provision& p = provisions[i];
		p.active = false;
		//Stop Transfer: the same write with Status FALSE. Not verified against a real peer.
		conn.SendRequest(BuildStopTransfer(*p.transferSetName, p.parameters), [remaining, done, name = p.transferSetName->itemId, log = log](const stack::Outcome& o)
			{
				if(log && !o.Ok())
					log(stack::LogLevel::Warn, "stopping transfer set " + name + ": " + Describe(o));
				if(--*remaining == 0)
					done();
			});
	}
}

// ---------------------------------------------------------------------------------------------
// Block 5 (device control): see Tase2Control.h for the model
// ---------------------------------------------------------------------------------------------

struct ClientSession::ControlOp
{
	const IccpControlConf* control = nullptr;
	ControlValue data; //the value to operate
	std::function<void(odc::CommandStatus)> done;
	stack::Clock::time_point deadline;
	bool finished = false;
	std::string what; //"ON", "OFF" or "setpoint 12.5", for the log
};

struct ClientSession::TagOp
{
	std::function<void(const TagResult&)> done;
	const IccpControlConf* control = nullptr;
	bool set = false;
	bool finished = false;
};

namespace
{
//the ODC status for an MMS outcome that is not a response
odc::CommandStatus StatusOfOutcome(const stack::Outcome& o)
{
	switch(o.kind)
	{
		case stack::Outcome::Kind::Timeout: return odc::CommandStatus::TIMEOUT;
		case stack::Outcome::Kind::Aborted: return odc::CommandStatus::UNDEFINED;
		case stack::Outcome::Kind::Rejected: return odc::CommandStatus::NOT_SUPPORTED;
		case stack::Outcome::Kind::Malformed: return odc::CommandStatus::FORMAT_ERROR;
		case stack::Outcome::Kind::Error:
			return (o.error.errorClass == mms::ErrorClass::Definition || o.error.errorClass == mms::ErrorClass::Access) ? odc::CommandStatus::NOT_SUPPORTED :
			       odc::CommandStatus::HARDWARE_ERROR;
		case stack::Outcome::Kind::Response: break;
	}
	return odc::CommandStatus::FORMAT_ERROR; //a response of the wrong kind
}
}

ClientSession::~ClientSession()
{
	//never lose a command: whatever is still in flight ends here
	auto ops = controlOps;
	for(auto& [c, op] : ops)
		if(!op->finished)
		{
			op->finished = true;
			if(op->done)
				op->done(odc::CommandStatus::UNDEFINED);
		}
	auto tags = tagOps;
	for(auto& op : tags)
		if(!op->finished)
		{
			op->finished = true;
			if(op->done)
			{
				TagResult r;
				r.error = "the association ended";
				op->done(r);
			}
		}
}

void ClientSession::Operate(const IccpControlConf& c, const ControlCommand& command, std::function<void(odc::CommandStatus)> done)
{
	auto fail = [&](odc::CommandStatus s, const std::string& why)
			{
				++controlStats.failures;
				Log(stack::LogLevel::Error, "control " + c.name + " not started: " + why + " (" + odc::ToString(s) + ")");
				done(s);
			};
	if(!ready || shuttingDown || !conn.IsAssociated())
	{
		fail(odc::CommandStatus::UNDEFINED, "the association is not ready");
		return;
	}
	if(controlOps.count(&c))
	{
		fail(odc::CommandStatus::ALREADY_ACTIVE, "an operation of this device is in flight");
		return;
	}
	auto op = std::make_shared<ControlOp>();
	op->control = &c;
	op->done = std::move(done);
	if(!IsSetpoint(c.type))
	{
		op->data.integer = command.on ? c.commandValueOn : c.commandValueOff;
		op->what = command.on ? "ON" : "OFF";
		if(op->data.integer < MinControlValue || op->data.integer > MaxControlValue)
		{
			//Load() already refuses such a value; this guards the wire all the same
			++controlStats.failures;
			Log(stack::LogLevel::Error, "control " + c.name + " not started: the command value " + std::to_string(op->data.integer) + " does not fit an integer of width 16 (" +
				std::to_string(MinControlValue) + " to " + std::to_string(MaxControlValue) + ") (" + odc::ToString(odc::CommandStatus::OUT_OF_RANGE) + ")");
			op->done(odc::CommandStatus::OUT_OF_RANGE);
			return;
		}
	}
	else
	{
		//ODC value = TASE.2 value * scale + offset
		const double tase = (command.value - c.offset) / c.scale;
		op->what = "setpoint " + std::to_string(command.value);
		if(!std::isfinite(tase))
		{
			++controlStats.failures;
			Log(stack::LogLevel::Error, "control " + c.name + " not started: the setpoint is not a finite number (" + odc::ToString(odc::CommandStatus::FORMAT_ERROR) + ")");
			op->done(odc::CommandStatus::FORMAT_ERROR);
			return;
		}
		if(IsRealControl(c.type))
		{
			if(std::fabs(tase) > static_cast<double>(std::numeric_limits<float>::max()))
			{
				++controlStats.failures;
				Log(stack::LogLevel::Error, "control " + c.name + " not started: the setpoint does not fit a 32 bit floating point number (" + odc::ToString(odc::CommandStatus::OUT_OF_RANGE) + ")");
				op->done(odc::CommandStatus::OUT_OF_RANGE);
				return;
			}
			op->data.real = tase;
		}
		else
		{
			//the device variable is an integer of width 16, written as a signed integer (IEC 60870-6-802 clause 6.2.2)
			const double rounded = std::round(tase);
			if(!(rounded >= static_cast<double>(MinControlValue) && rounded <= static_cast<double>(MaxControlValue)))
			{
				++controlStats.failures;
				Log(stack::LogLevel::Error, "control " + c.name + " not started: the setpoint " + std::to_string(command.value) + " is " + std::to_string(rounded) +
					" on the wire, which does not fit an integer of width 16 (" + std::to_string(MinControlValue) + " to " + std::to_string(MaxControlValue) + ") (" +
					odc::ToString(odc::CommandStatus::OUT_OF_RANGE) + ")");
				op->done(odc::CommandStatus::OUT_OF_RANGE);
				return;
			}
			op->data.integer = static_cast<int64_t>(rounded);
		}
	}
	op->deadline = time.steady() + std::chrono::milliseconds(c.EffectiveTimeoutMs());
	controlOps[&c] = op;
	if(c.selectRequired)
		ControlRead(op);
	else
		ControlOperate(op);
}

void ClientSession::CompleteControl(const std::shared_ptr<ControlOp>& op, odc::CommandStatus status, const std::string& why)
{
	if(op->finished)
		return;
	op->finished = true;
	controlOps.erase(op->control);
	const auto& c = *op->control;
	if(status == odc::CommandStatus::SUCCESS)
	{
		++controlStats.operates;
		Log(stack::LogLevel::Info, "control " + c.name + " operated on the peer: " + op->what + " (" + (c.selectRequired ? "select before operate" : "direct operate") + ")");
	}
	else
	{
		++controlStats.failures;
		Log(stack::LogLevel::Error, "control " + c.name + " (" + op->what + ") failed: " + why + " (" + odc::ToString(status) + ")");
	}
	if(op->done)
		op->done(status);
}

void ClientSession::ControlRead(const std::shared_ptr<ControlOp>& op)
{
	const mms::ObjectName device = op->control->Object();
	Send(BuildReadCheckBack(device), [this, op](const stack::Outcome& o)
		{
			if(op->finished)
				return;
			const auto* rr = o.Get<mms::ReadResponse>();
			if(rr == nullptr || rr->results.size() != 1)
			{
				CompleteControl(op, rr == nullptr ? StatusOfOutcome(o) : odc::CommandStatus::FORMAT_ERROR,
					"Read of the CheckBackID: " + (rr == nullptr ? Why(o) : std::string("the answer has ") + std::to_string(rr->results.size()) + " results for 1 object"));
				return;
			}
			if(!rr->results[0].success)
			{
				CompleteControl(op, CommandStatusOf(rr->results[0].error), "Read of the CheckBackID: data access error " + mms::DescribeDataAccessError(rr->results[0].error));
				return;
			}
			int64_t id = 0;
			try
			{
				id = ParseSelectData(rr->results[0].data);
			}
			catch(const DecodeError& e)
			{
				CompleteControl(op, odc::CommandStatus::FORMAT_ERROR, std::string("Read of the CheckBackID: ") + e.what());
				return;
			}
			//what the CheckBackID means to the client is a local matter (IEC 60870-6-503 clause 7.2.6.2.2): here it is only compared with the one configured, if any
			const auto& c = *op->control;
			if(c.checkBackId && id != *c.checkBackId)
			{
				CompleteControl(op, odc::CommandStatus::FORMAT_ERROR, "the select returned CheckBackID " + std::to_string(id) + " but CheckBackID " + std::to_string(*c.checkBackId) +
					" was expected: the wrong device or object may have been selected, so the operate was not sent");
				return;
			}
			++controlStats.selects;
			ControlOperate(op);
		});
}

void ClientSession::ControlOperate(const std::shared_ptr<ControlOp>& op)
{
	const auto& c = *op->control;
	Send(BuildOperate(c.Object(), c.type, op->data), [this, op](const stack::Outcome& o)
		{
			if(op->finished)
				return;
			const auto* wr = o.Get<mms::WriteResponse>();
			if(wr != nullptr && wr->results.size() == 1 && wr->results[0].success)
			{
				CompleteControl(op, odc::CommandStatus::SUCCESS, "");
				return;
			}
			odc::CommandStatus s = wr == nullptr ? StatusOfOutcome(o) : odc::CommandStatus::FORMAT_ERROR;
			std::string why = wr == nullptr ? Why(o) : std::string("the answer has ") + std::to_string(wr->results.size()) + " results for 1 object";
			if(wr != nullptr && wr->results.size() == 1)
			{
				s = CommandStatusOf(wr->results[0].error);
				why = "data access error " + mms::DescribeDataAccessError(wr->results[0].error);
			}
			CompleteControl(op, s, "operate: " + why);
		});
}

void ClientSession::TickControls()
{
	const auto now = time.steady();
	auto ops = controlOps;
	for(auto& [c, op] : ops)
		if(!op->finished && now >= op->deadline)
			CompleteControl(op, odc::CommandStatus::TIMEOUT, "no answer within " + std::to_string(c->EffectiveTimeoutMs()) + " ms");
}

// ---------------------------------------------------------------------------------------------
// Device tags (optional): Get Tag and Set Tag, see Tase2Control.h
// ---------------------------------------------------------------------------------------------

namespace
{
//what a data access error of a Get Tag or Set Tag means (IEC 60870-6-503 clauses 7.2.6.8 and 7.2.6.9), for the log
std::string TagRefusal(int64_t error, bool set)
{
	namespace DA = mms::DataAccessError;
	std::string hint;
	switch(error)
	{
		case DA::ObjectNonExistent: hint = "the peer has no such device, or the device has no Tag attribute"; break;
		case DA::ObjectAccessDenied: hint = std::string("the peer does not permit ") + (set ? "Set Tag" : "Get Tag") + " to this client"; break;
		case DA::TemporarilyUnavailable: hint = set ? "the device is ARMED or busy, or its tag was set by another client" : "the device is temporarily unavailable"; break;
		case DA::TypeInconsistent: hint = "the peer did not accept the Tag_Value"; break;
		default: break;
	}
	return "data access error " + mms::DescribeDataAccessError(error) + (hint.empty() ? "" : ": " + hint);
}
}

void ClientSession::GetTag(const IccpControlConf& control, std::function<void(const TagResult&)> done)
{
	StartTagRequest(control, false, DeviceTag::NoTag, "", std::move(done));
}

void ClientSession::SetTag(const IccpControlConf& control, DeviceTag tag, const std::string& reason, std::function<void(const TagResult&)> done)
{
	StartTagRequest(control, true, tag, reason, std::move(done));
}

void ClientSession::FinishTag(const std::shared_ptr<TagOp>& op, TagResult result)
{
	if(op->finished)
		return;
	op->finished = true;
	tagOps.erase(op);
	const auto& c = *op->control;
	const char* what = op->set ? "Set Tag" : "Get Tag";
	if(result.ok)
	{
		if(op->set)
		{
			++controlStats.tagWrites;
			Log(stack::LogLevel::Info, "control " + c.name + ": Set Tag accepted by the peer: " + ToString(result.value.tag) + (result.value.reason.empty() ? "" : " (" + result.value.reason + ")"));
		}
		else
		{
			++controlStats.tagReads;
			Log(stack::LogLevel::Info, "control " + c.name + ": Get Tag: the peer's tag is " + ToString(result.value.tag) + (result.value.reason.empty() ? "" : " (" + result.value.reason + ")") +
				", the device is " + (result.value.armed ? "ARMED" : "IDLE"));
		}
	}
	else
	{
		++controlStats.tagFailures;
		Log(result.dataAccessError ? stack::LogLevel::Warn : stack::LogLevel::Error, "control " + c.name + ": " + what + " failed: " + result.error);
	}
	if(op->done)
		op->done(result);
}

void ClientSession::StartTagRequest(const IccpControlConf& c, bool set, DeviceTag tag, const std::string& reason, std::function<void(const TagResult&)> done)
{
	auto op = std::make_shared<TagOp>();
	op->done = std::move(done);
	op->control = &c;
	op->set = set;
	tagOps.insert(op);
	TagResult early;
	early.value.tag = tag;
	early.value.reason = reason;
	if(!ready || shuttingDown || !conn.IsAssociated())
	{
		early.error = "the association is not ready";
		FinishTag(op, early);
		return;
	}
	if(set && !ValidTagReason(reason))
	{
		early.error = "the reason must be at most " + std::to_string(MaxTagReasonLength) + " characters of visible ASCII";
		FinishTag(op, early);
		return;
	}
	if(set)
	{
		Send(BuildSetTag(c.Object(), tag, reason), [this, op, early](const stack::Outcome& o) mutable
			{
				if(op->finished)
					return;
				const auto* wr = o.Get<mms::WriteResponse>();
				if(wr == nullptr)
					early.error = Why(o);
				else if(wr->results.size() != 1)
					early.error = "the answer has " + std::to_string(wr->results.size()) + " results for 1 object";
				else if(!wr->results[0].success)
				{
					early.dataAccessError = wr->results[0].error;
					early.error = TagRefusal(wr->results[0].error, true);
				}
				else
					early.ok = true;
				FinishTag(op, early);
			});
		return;
	}
	Send(BuildGetTag(c.Object()), [this, op, early](const stack::Outcome& o) mutable
		{
			if(op->finished)
				return;
			const auto* rr = o.Get<mms::ReadResponse>();
			if(rr == nullptr)
				early.error = Why(o);
			else if(rr->results.size() != 1)
				early.error = "the answer has " + std::to_string(rr->results.size()) + " results for 1 object";
			else if(!rr->results[0].success)
			{
				early.dataAccessError = rr->results[0].error;
				early.error = TagRefusal(rr->results[0].error, false);
			}
			else
			{
				try
				{
					early.value = ParseTagData(rr->results[0].data);
					early.ok = true;
				}
				catch(const DecodeError& e)
				{
					early.error = std::string("the peer's answer is not a Tag_Value: ") + e.what();
				}
			}
			FinishTag(op, early);
		});
}

} // namespace tase2
} // namespace iccp
