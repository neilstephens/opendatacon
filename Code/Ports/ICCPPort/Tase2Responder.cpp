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
 * Tase2Responder.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The TASE.2 service side of one association, see Tase2Responder.h.
 */

#include "Tase2Responder.h"
#include "MmsNames.h"
#include <algorithm>
#include <cmath>

namespace iccp
{
namespace tase2
{

namespace
{
//MMS error codes used here (ISO 9506-2)
constexpr int64_t DefinitionObjectUndefined = 1;
constexpr int64_t DefinitionTypeInconsistent = 4;
constexpr int64_t DefinitionObjectExists = 5;
constexpr int64_t DefinitionAttributeInconsistent = 6;
constexpr int64_t ResourceMemoryUnavailable = 1;

std::optional<Special> SpecialOfName(const std::string& item)
{
	if(item == Name::TransferSetName)
		return Special::TransferSetName;
	if(item == Name::TransferSetTimeStamp)
		return Special::TimeStamp;
	if(item == Name::DsConditionsDetected)
		return Special::ConditionsDetected;
	if(item == Name::EventCodeDetected)
		return Special::EventCode;
	return std::nullopt;
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

//the leading special objects of a data set definition, in order, each at most once
std::vector<Special> LeadingSpecials(const std::vector<mms::ObjectName>& members)
{
	std::vector<Special> out;
	for(const auto& m : members)
	{
		const auto s = SpecialOfName(m.itemId);
		if(!s || std::find(out.begin(), out.end(), *s) != out.end())
			break;
		out.push_back(*s);
	}
	return out;
}
}

// ---------------------------------------------------------------------------------------------
// model
// ---------------------------------------------------------------------------------------------

namespace
{
std::string NameKey(const mms::ObjectName& n)
{
	return std::string(1, static_cast<char>('0' + static_cast<int>(n.scope))) + "/" + n.domainId + "/" + n.itemId;
}
}

ResponderModel::ResponderModel(const ICCPPortConf& c, bool serve):
	conf(c),
	servePoints(serve),
	createdAt(odc::msSinceEpoch())
{
	tags.resize(conf.controls.size());
	for(size_t i = 0; i < conf.controls.size(); ++i)
		if(conf.controls[i].taggable && conf.controls[i].direction == ControlDirection::Serve)
		{
			tags[i].tag = conf.controls[i].initialTag;
			tags[i].reason = conf.controls[i].initialTagReason; //set locally: no client owns it
		}
	if(!servePoints)
		return;
	states.resize(conf.points.size());
	byName.reserve(conf.points.size());
	for(size_t i = 0; i < conf.points.size(); ++i)
	{
		if(conf.points[i].direction != PointDirection::Serve)
			continue; //points this port reads from the peer are the ClientSession's
		byKindIndex[{static_cast<int>(conf.points[i].kind), conf.points[i].index}] = i;
		byName.emplace(NameKey(conf.points[i].Object()), i); //the first of equal names wins
	}
	for(const auto& d : conf.dataSets)
	{
		if(d.direction != PointDirection::Serve)
			continue;
		DataSet ds;
		ds.name = mms::ObjectName::Domain(d.domain, d.name);
		for(const auto& m : d.members)
			if(const auto* p = conf.FindPoint(m, PointDirection::Serve))
				ds.members.push_back(p->Object());
		staticDataSets.push_back(std::move(ds));
	}
	for(size_t i = 0; i < conf.controls.size(); ++i)
	{
		const auto& c = conf.controls[i];
		if(c.direction != ControlDirection::Serve)
			continue; //the peer's devices that this port operates are the ClientSession's
		const mms::ObjectName device = c.Object();
		controlByName.emplace(NameKey(device), ControlRef{i, false, false});
		controlByName.emplace(NameKey(SelectObject(device)), ControlRef{i, true, false});
	}
	//the tag objects, after every device and select object so that a device that happens to be named like another's tag object keeps its name; a device that is not
	//taggable has none, which is only registered so that a Get Tag or Set Tag of it is answered object-non-existent (unless a point has that name)
	for(size_t i = 0; i < conf.controls.size(); ++i)
	{
		const auto& c = conf.controls[i];
		if(c.direction != ControlDirection::Serve)
			continue;
		const auto key = NameKey(TagObject(c.Object()));
		if(c.taggable || byName.count(key) == 0)
			controlByName.emplace(key, ControlRef{i, false, true});
	}
}

std::optional<ResponderModel::ControlRef> ResponderModel::FindControl(const mms::ObjectName& name) const
{
	const auto it = controlByName.find(NameKey(name));
	if(it == controlByName.end())
		return std::nullopt;
	return it->second;
}

std::optional<size_t> ResponderModel::FindTag(const mms::ObjectName& name) const
{
	const auto ref = FindControl(name);
	if(!ref || !ref->tag || !conf.controls[ref->control].taggable)
		return std::nullopt;
	return ref->control;
}

ResponderModel::TagState ResponderModel::TagOf(size_t control) const
{
	std::lock_guard<std::mutex> lock(tagMtx);
	return tags.at(control);
}

void ResponderModel::SetTag(size_t control, DeviceTag tag, const std::string& reason, std::optional<std::string> owner)
{
	std::lock_guard<std::mutex> lock(tagMtx);
	TagState& t = tags.at(control);
	t.tag = tag;
	t.reason = reason;
	t.owner = tag == DeviceTag::NoTag ? std::nullopt : std::move(owner); //nobody owns "no tag"
}

void ResponderModel::CountTag(TagEvent event)
{
	std::lock_guard<std::mutex> lock(tagMtx);
	switch(event)
	{
		case TagEvent::Set: ++tagCounters.set; break;
		case TagEvent::LocalSet: ++tagCounters.localSet; break;
		case TagEvent::Refused: ++tagCounters.refused; break;
		case TagEvent::SelectRefused: ++tagCounters.selectsRefused; break;
		case TagEvent::OperateRefused: ++tagCounters.operatesRefused; break;
		case TagEvent::LocalReset: ++tagCounters.localResets; break;
	}
}

ResponderModel::TagCounters ResponderModel::TagCounts() const
{
	std::lock_guard<std::mutex> lock(tagMtx);
	return tagCounters;
}

std::vector<PointState> ResponderModel::StatesSnapshot() const
{
	std::lock_guard<std::mutex> lock(stateMtx);
	return states;
}

std::optional<size_t> ResponderModel::Update(PointKind kind, size_t index, bool binary, double analog, uint32_t counter, odc::QualityFlags quality, odc::msSinceEpoch_t timestamp)
{
	if(!servePoints)
		return std::nullopt;
	const auto it = byKindIndex.find({static_cast<int>(kind), index});
	if(it == byKindIndex.end())
		return std::nullopt;
	std::lock_guard<std::mutex> lock(stateMtx);
	PointState& s = states[it->second];
	bool changed = !s.known || s.quality != quality;
	switch(kind)
	{
		case PointKind::Binary:
			changed = changed || s.binary != binary;
			s.binary = binary;
			break;
		case PointKind::Analog:
			changed = changed || s.analog != analog;
			s.analog = analog;
			break;
		case PointKind::Counter:
			changed = changed || s.counter != counter;
			s.counter = counter;
			break;
	}
	if(changed && s.known)
		++s.cov;
	s.known = true;
	s.quality = quality;
	s.timestamp = timestamp;
	if(!changed)
		return std::nullopt;
	return it->second;
}

std::optional<size_t> ResponderModel::FindPoint(const mms::ObjectName& name) const
{
	if(!servePoints)
		return std::nullopt;
	const auto it = byName.find(NameKey(name));
	if(it == byName.end())
		return std::nullopt;
	return it->second;
}

// ---------------------------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------------------------

ResponderSession::ResponderSession(ResponderModel& m, stack::Connection& c, Timebase tb, LogFn l):
	model(m),
	conn(c),
	time(std::move(tb)),
	log(std::move(l))
{
	devices.resize(model.Conf().controls.size());
	if(model.ServesPoints())
		for(const auto& name : model.Conf().transferSetPool)
		{
			TransferSetRuntime rt;
			rt.name = name;
			transferSets.push_back(rt);
		}
}

ResponderSession::~ResponderSession() = default;

std::string ResponderSession::Key(const mms::ObjectName& n)
{
	return std::string(1, static_cast<char>('0' + static_cast<int>(n.scope))) + "/" + n.domainId + "/" + n.itemId;
}

const ResponderModel::DataSet* ResponderSession::FindDataSet(const mms::ObjectName& name) const
{
	const std::string key = Key(name);
	const auto it = clientDataSets.find(key);
	if(it != clientDataSets.end())
		return &it->second;
	for(const auto& d : model.StaticDataSets())
		if(Key(d.name) == key)
			return &d;
	return nullptr;
}

ResponderSession::TransferSetRuntime* ResponderSession::FindTransferSet(const std::string& name)
{
	for(auto& t : transferSets)
		if(t.name == name)
			return &t;
	return nullptr;
}

bool ResponderSession::TransferSetEnabled(const std::string& name) const
{
	for(const auto& t : transferSets)
		if(t.name == name)
			return t.enabled;
	return false;
}

bool ResponderSession::IsSpecialName(const mms::ObjectName& n) const
{
	return SpecialOfName(n.itemId).has_value() && n.scope == mms::ObjectScope::DomainSpecific && n.domainId == model.Conf().localDomain;
}

void ResponderSession::Fail(uint32_t id, int64_t errorClass, int64_t code)
{
	mms::ServiceError e;
	e.errorClass = errorClass;
	e.errorCode = code;
	conn.RespondError(id, e);
}

void ResponderSession::OnRequest(uint32_t id, const mms::ConfirmedServiceRequest& request)
{
	if(std::holds_alternative<mms::IdentifyRequest>(request))
		HandleIdentify(id);
	else if(const auto* r = std::get_if<mms::GetNameListRequest>(&request))
		HandleGetNameList(id, *r);
	else if(const auto* rd = std::get_if<mms::ReadRequest>(&request))
		HandleRead(id, *rd);
	else if(const auto* w = std::get_if<mms::WriteRequest>(&request))
		HandleWrite(id, *w);
	else if(const auto* g = std::get_if<mms::GetVariableAccessAttributesRequest>(&request))
		HandleGetVariableAccessAttributes(id, *g);
	else if(const auto* d = std::get_if<mms::DefineNamedVariableListRequest>(&request))
		HandleDefine(id, *d);
	else if(const auto* del = std::get_if<mms::DeleteNamedVariableListRequest>(&request))
		HandleDelete(id, *del);
	else if(const auto* a = std::get_if<mms::GetNamedVariableListAttributesRequest>(&request))
		HandleGetAttributes(id, *a);
	else
		conn.RejectRequest(id, mms::RejectReason::UnrecognizedService);
}

void ResponderSession::HandleIdentify(uint32_t id)
{
	const auto& c = model.Conf();
	conn.Respond(id, mms::IdentifyResponse{c.vendorName, c.modelName, c.revision, std::nullopt});
}

std::vector<std::string> ResponderSession::NamesIn(mms::ObjectScope scope, const std::string& domain, int64_t objectClass, bool& known) const
{
	const auto& c = model.Conf();
	std::vector<std::string> names;
	known = true;
	auto domainKnown = [&]()
				 {
					 if(!domain.empty() && domain == c.localDomain)
						 return true;
					 if(model.ServesPoints())
					 {
						 for(const auto& p : c.points)
							 if(p.direction == PointDirection::Serve && p.scope == Scope::Icc && p.domain == domain)
								 return true;
						 for(const auto& d : c.controls)
							 if(d.direction == ControlDirection::Serve && d.scope == Scope::Icc && d.domain == domain)
								 return true;
					 }
					 return false;
				 };
	if(objectClass == mms::ObjectClass::NamedVariable)
	{
		if(scope == mms::ObjectScope::VmdSpecific)
		{
			if(model.ServesPoints())
			{
				for(const auto& p : c.points)
					if(p.direction == PointDirection::Serve && p.scope == Scope::Vcc)
						names.push_back(p.name);
				for(const auto& d : c.controls)
					if(d.direction == ControlDirection::Serve && d.scope == Scope::Vcc)
					{
						names.push_back(d.name);
						names.push_back(SelectName(d.name));
						if(d.taggable)
							names.push_back(TagName(d.name));
					}
			}
			names.push_back(Name::Tase2Version);
			names.push_back(Name::SupportedFeatures);
		}
		else if(scope == mms::ObjectScope::DomainSpecific)
		{
			if(!domainKnown())
			{
				known = false;
				return names;
			}
			if(model.ServesPoints())
			{
				for(const auto& p : c.points)
					if(p.direction == PointDirection::Serve && p.scope == Scope::Icc && p.domain == domain)
						names.push_back(p.name);
				for(const auto& d : c.controls)
					if(d.direction == ControlDirection::Serve && d.scope == Scope::Icc && d.domain == domain)
					{
						names.push_back(d.name);
						names.push_back(SelectName(d.name));
						if(d.taggable)
							names.push_back(TagName(d.name));
					}
			}
			if(domain == c.localDomain)
			{
				if(!c.bilateralTableId.empty())
					names.push_back(Name::BilateralTableId);
				if(!transferSets.empty())
				{
					names.push_back(Name::NextDsTransferSet);
					for(const auto& t : transferSets)
						names.push_back(t.name);
					for(Special s : {Special::TransferSetName, Special::TimeStamp, Special::ConditionsDetected, Special::EventCode})
						names.push_back(SpecialItem(s));
				}
			}
		}
	}
	else if(objectClass == mms::ObjectClass::NamedVariableList)
	{
		if(scope == mms::ObjectScope::DomainSpecific)
		{
			if(!domainKnown())
			{
				known = false;
				return names;
			}
			for(const auto& d : model.StaticDataSets())
				if(d.name.domainId == domain)
					names.push_back(d.name.itemId);
			for(const auto& [k, d] : clientDataSets)
				if(d.name.domainId == domain)
					names.push_back(d.name.itemId);
		}
	}
	else if(objectClass == mms::ObjectClass::Domain)
	{
		if(scope == mms::ObjectScope::VmdSpecific && !c.localDomain.empty())
			names.push_back(c.localDomain);
	}
	return names;
}

void ResponderSession::HandleGetNameList(uint32_t id, const mms::GetNameListRequest& r)
{
	bool known = true;
	const auto names = NamesIn(r.scope, r.domainId, r.objectClass, known);
	if(!known)
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
		return;
	}
	const auto page = BuildNamePage(names, r, model.Conf().namePageSize);
	if(!page)
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
		return;
	}
	conn.Respond(id, *page);
}

std::optional<ObjectRef> ResponderSession::NextFreeTransferSet()
{
	for(auto& t : transferSets)
		if(!t.reserved)
		{
			t.reserved = true;
			return ObjectRef{Scope::Icc, model.Conf().localDomain, t.name};
		}
	return std::nullopt;
}

mms::AccessResult ResponderSession::ReadMember(const mms::ObjectName& name) const
{
	if(const auto idx = model.FindPoint(name))
		return mms::AccessResult::Success(ToData(model.Point(*idx), model.State(*idx), time.wallMs()));
	//a taggable device's tag object may be a member of a data set (IEC 60870-6-503 clause 5.3.12.6): it is read, and never reported as changed
	if(const auto tag = model.FindTag(name))
		return mms::AccessResult::Success(TagToData(CurrentTag(*tag)));
	return mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent);
}

mms::AccessResult ResponderSession::ReadItem(const mms::ObjectName& name)
{
	if(name.itemId == Name::BilateralTableId || name.itemId == Name::Tase2Version || name.itemId == Name::SupportedFeatures)
	{
		mms::ReadRequest single;
		single.specification = mms::ListOfVariable{mms::VariableSpec{name, std::nullopt}
		};
		return BuildAssociationInfoResponse(single, model.Conf().LocalAssociation()).results.front();
	}
	if(name.itemId == Name::NextDsTransferSet && name.scope == mms::ObjectScope::DomainSpecific && name.domainId == model.Conf().localDomain)
	{
		const auto next = NextFreeTransferSet();
		if(!next)
			return mms::AccessResult::Failure(mms::DataAccessError::ObjectNonExistent);
		Log(stack::LogLevel::Debug, "handing out transfer set " + next->name);
		return mms::AccessResult::Success(ToData(*next));
	}
	if(const auto ref = model.FindControl(name))
		return ReadControl(*ref);
	return ReadMember(name);
}

void ResponderSession::HandleRead(uint32_t id, const mms::ReadRequest& r)
{
	mms::ReadResponse resp;
	if(const auto* list = std::get_if<mms::ListOfVariable>(&r.specification))
	{
		for(const auto& v : *list)
			resp.results.push_back(ReadItem(v.name));
		if(r.specificationWithResult)
			resp.specification = r.specification;
	}
	else
	{
		const auto& name = std::get<mms::ObjectName>(r.specification);
		const auto* ds = FindDataSet(name);
		if(ds == nullptr)
		{
			Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
			return;
		}
		for(const auto& m : ds->members)
			resp.results.push_back(ReadMember(m));
		if(r.specificationWithResult)
			resp.specification = r.specification;
	}
	conn.Respond(id, resp);
}

void ResponderSession::DisableTransferSet(TransferSetRuntime& rt)
{
	if(!rt.outstanding.empty())
		Log(stack::LogLevel::Debug, "transfer set " + rt.name + ": " + std::to_string(rt.outstanding.size()) + " critical report(s) still unacknowledged are no longer tracked");
	rt.outstanding.clear();
	rt.tleExceeded = false;
	rt.enabled = false;
	rt.reserved = false;
	rt.nextInterval.reset();
	rt.nextIntegrity.reset();
	rt.bufferDeadline.reset();
	rt.pending.clear();
	rt.changes.clear();
	rt.overflowLogged = false;
	rt.blockedFallbackLogged = false;
	rt.blockedRule2Logged = false;
	rt.pointPositions.clear();
	rt.positionsBuilt = false;
}

bool ResponderSession::ApplyTransferSetWrite(TransferSetRuntime& rt, const DsTransferSet& p)
{
	if(!p.status)
	{
		Log(stack::LogLevel::Info, "transfer set " + rt.name + " stopped");
		DisableTransferSet(rt);
		return true;
	}
	if(p.dataSetName.scope != Scope::Icc || FindDataSet(p.dataSetName.ToName()) == nullptr)
		return false;
	const auto now = time.steady();
	rt.params = p;
	rt.dataSetKey = Key(p.dataSetName.ToName());
	rt.reserved = true;
	rt.enabled = true;
	rt.outstanding.clear(); //a (re)start begins with nothing outstanding
	rt.tleExceeded = false;
	rt.pending.clear();
	rt.changes.clear();
	rt.overflowLogged = false;
	rt.blockedFallbackLogged = false;
	rt.blockedRule2Logged = false;
	rt.pointPositions.clear();
	rt.positionsBuilt = false;
	rt.bufferDeadline.reset();
	std::chrono::milliseconds delay{0};
	if(p.startTime > 0)
	{
		const int64_t waitMs = p.startTime * 1000 - static_cast<int64_t>(time.wallMs());
		if(waitMs > 0)
			delay = std::chrono::milliseconds(waitMs);
	}
	rt.activeFrom = now + delay;
	rt.nextInterval.reset();
	rt.nextIntegrity.reset();
	if(p.conditionsRequested.Has(DsCondition::IntervalTimeOut) && p.interval > 0)
		rt.nextInterval = rt.activeFrom + std::chrono::seconds(p.interval);
	if(p.conditionsRequested.Has(DsCondition::IntegrityTimeOut) && p.integrityCheck > 0)
		rt.nextIntegrity = rt.activeFrom; //the first integrity report goes out at once
	Log(stack::LogLevel::Info, "transfer set " + rt.name + " started for data set " + p.dataSetName.name);
	if(AllChanges(rt))
		Log(stack::LogLevel::Info, "transfer set " + rt.name + ": AllChangesReported is honoured: every change of a point within the " + std::to_string(p.bufferTime) +
			" s buffer time is reported, in order, one report per change of the same point (at most " + std::to_string(model.Conf().maxBufferedChanges) + " changes are buffered)");
	else if(p.allChangesReported)
		Log(stack::LogLevel::Info, "transfer set " + rt.name + ": AllChangesReported has no effect without RBE, the ObjectChange condition and a BufferTime above 0: only the last value of a changed point is reported");
	if(p.blockData)
	{
		if(BlockedHonoured(rt))
		{
			const ResponderModel::DataSet* ds = FindDataSet(p.dataSetName.ToName());
			const auto reason = ds != nullptr ? BlockedUnsendable(rt, *ds) : std::nullopt;
			if(reason)
			{
				rt.blockedFallbackLogged = true; //said now, once per start
				Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": BlockData is true but " + *reason + ": the reports of this transfer set are sent in the ordinary form");
			}
			else
				Log(stack::LogLevel::Info, "transfer set " + rt.name + ": BlockData is honoured (Block 3): each report carries the special objects as variables and one octet string in the VMD-specific variable " +
					model.Conf().blockedDataName + ": rule 0 (every member of the data set) for a complete report" + (p.rbe ? ", rule 1 (the changed points only, 1 octet positions) for a report of what changed" : ""));
		}
		else
			Log(stack::LogLevel::Info, "transfer set " + rt.name + ": BlockData is accepted and not acted on: this port does not advertise Block 3 (SupportedBlocks), so the reports are ordinary");
	}
	if(p.critical)
		Log(stack::LogLevel::Info, "transfer set " + rt.name + ": Critical: each report waits for the client's Transfer_Report_ACK" +
			(model.Conf().reportAckTimeoutMs > 0 ? " for at most " + std::to_string(model.Conf().reportAckTimeoutMs) + " ms (ReportAckTimeoutms)" : std::string(", with no timeout (ReportAckTimeoutms 0)")) +
			"; a missing or negative acknowledgement is the Data_failure condition, which is logged and counted, nothing more");
	if(p.tle > 0)
		Log(stack::LogLevel::Info, "transfer set " + rt.name + ": TLE " + std::to_string(p.tle) + " s: a report handed over more than that after it was due is discarded");
	return true;
}

void ResponderSession::HandleWrite(uint32_t id, const mms::WriteRequest& r)
{
	const auto* list = std::get_if<mms::ListOfVariable>(&r.specification);
	if(list == nullptr || list->size() != r.data.size())
	{
		conn.RejectRequest(id, mms::RejectReason::InvalidArgument);
		return;
	}
	//The results are collected in a shared state: a control operate completes later (when downstream has answered), and may complete at once.
	auto pending = std::make_shared<PendingWrite>();
	pending->id = id;
	pending->results.assign(list->size(), mms::WriteResult{false, mms::DataAccessError::ObjectAccessDenied});
	std::weak_ptr<bool> guard = alive;
	auto respond = [this, pending]()
			   {
				   mms::WriteResponse resp;
				   resp.results = pending->results;
				   conn.Respond(pending->id, resp);
			   };
	for(size_t i = 0; i < list->size(); ++i)
	{
		const mms::ObjectName& n = (*list)[i].name;
		if(const auto ref = model.FindControl(n))
		{
			++pending->remaining;
			WriteControl(*ref, r.data[i], [guard, pending, i, respond](std::optional<ControlError> error)
				{
					if(guard.expired())
						return;
					pending->results[i] = error ? mms::WriteResult{false, DataAccessErrorOf(*error)} : mms::WriteResult{true, 0};
					if(--pending->remaining == 0 && pending->dispatched)
						respond();
				});
			continue;
		}
		TransferSetRuntime* rt = (n.scope == mms::ObjectScope::DomainSpecific && n.domainId == model.Conf().localDomain) ? FindTransferSet(n.itemId) : nullptr;
		if(rt == nullptr)
			continue; //object-access-denied, as set above
		try
		{
			const DsTransferSet p = ParseDsTransferSet(r.data[i]);
			if(ApplyTransferSetWrite(*rt, p))
				pending->results[i] = mms::WriteResult{true, 0}
			;
			else
				pending->results[i] = mms::WriteResult{false, mms::DataAccessError::ObjectNonExistent}
			;
		}
		catch(const DecodeError& e)
		{
			Log(stack::LogLevel::Warn, std::string("bad transfer set write: ") + e.what());
			pending->results[i] = mms::WriteResult{false, mms::DataAccessError::TypeInconsistent};
		}
	}
	pending->dispatched = true;
	if(pending->remaining == 0)
		respond();
}

void ResponderSession::HandleGetVariableAccessAttributes(uint32_t id, const mms::GetVariableAccessAttributesRequest& r)
{
	if(model.FindTag(r.name))
	{
		mms::GetVariableAccessAttributesResponse resp;
		resp.mmsDeletable = false;
		resp.typeDescription = TagTypeDescription();
		conn.Respond(id, resp);
		return;
	}
	if(const auto ref = model.FindControl(r.name); ref && !ref->tag)
	{
		mms::GetVariableAccessAttributesResponse resp;
		resp.mmsDeletable = false;
		resp.typeDescription = ref->select ? SelectTypeDescription() : DeviceTypeDescription(model.Conf().controls[ref->control].type);
		conn.Respond(id, resp);
		return;
	}
	const auto idx = model.FindPoint(r.name);
	if(!idx)
	{
		if(r.name.itemId == Name::BilateralTableId && r.name.scope == mms::ObjectScope::DomainSpecific && model.Conf().LocalAssociation().bilateralTableIds.count(r.name.domainId))
		{
			mms::GetVariableAccessAttributesResponse resp;
			resp.mmsDeletable = false;
			resp.typeDescription = BilateralTableIdTypeDescription();
			conn.Respond(id, resp);
			return;
		}
		Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
		return;
	}
	mms::GetVariableAccessAttributesResponse resp;
	resp.mmsDeletable = false;
	resp.typeDescription = TypeDescriptionOf(model.Point(*idx).type);
	conn.Respond(id, resp);
}

void ResponderSession::HandleDefine(uint32_t id, const mms::DefineNamedVariableListRequest& r)
{
	const auto& c = model.Conf();
	if(r.name.scope != mms::ObjectScope::DomainSpecific || r.name.domainId != c.localDomain)
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionAttributeInconsistent);
		return;
	}
	if(FindDataSet(r.name) != nullptr)
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionObjectExists);
		return;
	}
	if(clientDataSets.size() >= c.maxClientDataSets || r.variables.size() > c.maxDataSetMembers)
	{
		Log(stack::LogLevel::Warn, "data set " + r.name.itemId + " refused: limits reached");
		Fail(id, mms::ErrorClass::Resource, ResourceMemoryUnavailable);
		return;
	}
	ResponderModel::DataSet ds;
	ds.name = r.name;
	for(const auto& v : r.variables)
	{
		if(v.alternateAccess)
		{
			Fail(id, mms::ErrorClass::Definition, DefinitionAttributeInconsistent);
			return;
		}
		if(!model.FindPoint(v.name) && !IsSpecialName(v.name) && !model.FindTag(v.name))
		{
			Log(stack::LogLevel::Warn, "data set " + r.name.itemId + ": unknown member " + v.name.itemId);
			Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
			return;
		}
		ds.members.push_back(v.name);
	}
	clientDataSets[Key(r.name)] = std::move(ds);
	Log(stack::LogLevel::Info, "data set " + r.name.itemId + " defined with " + std::to_string(r.variables.size()) + " members");
	conn.Respond(id, mms::DefineNamedVariableListResponse{});
}

void ResponderSession::HandleDelete(uint32_t id, const mms::DeleteNamedVariableListRequest& r)
{
	uint32_t matched = 0, deleted = 0;
	auto remove = [&](const std::string& key)
			  {
				  const auto it = clientDataSets.find(key);
				  if(it != clientDataSets.end())
				  {
					  for(auto& t : transferSets)
						  if(t.enabled && t.dataSetKey == key)
							  DisableTransferSet(t);
					  clientDataSets.erase(it);
					  ++matched;
					  ++deleted;
					  return true;
				  }
				  return false;
			  };
	const int64_t scope = r.scopeOfDelete.value_or(mms::DeleteScope::Specific);
	if(scope == mms::DeleteScope::Specific && r.listOfVariableListName)
	{
		for(const auto& n : *r.listOfVariableListName)
		{
			if(remove(Key(n)))
				continue;
			if(FindDataSet(n) != nullptr)
				++matched; //a configured data set: matched but not deletable
		}
		if(matched == 0)
		{
			Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
			return;
		}
	}
	else if(scope == mms::DeleteScope::Domain && r.domainName)
	{
		std::vector<std::string> keys;
		for(const auto& [k, d] : clientDataSets)
			if(d.name.domainId == *r.domainName)
				keys.push_back(k);
		for(const auto& k : keys)
			remove(k);
	}
	else if(scope == mms::DeleteScope::Vmd)
	{
		std::vector<std::string> keys;
		for(const auto& [k, d] : clientDataSets)
			keys.push_back(k);
		for(const auto& k : keys)
			remove(k);
	}
	else
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionAttributeInconsistent);
		return;
	}
	conn.Respond(id, BuildDeleteResponse(matched, deleted));
}

void ResponderSession::HandleGetAttributes(uint32_t id, const mms::GetNamedVariableListAttributesRequest& r)
{
	const auto* ds = FindDataSet(r.name);
	if(ds == nullptr)
	{
		Fail(id, mms::ErrorClass::Definition, DefinitionObjectUndefined);
		return;
	}
	DataSetDefinition def;
	def.name = ds->name;
	def.members = ds->members;
	const bool deletable = clientDataSets.count(Key(r.name)) != 0;
	conn.Respond(id, BuildDataSetElementNamesResponse(def, deletable));
}

// ---------------------------------------------------------------------------------------------
// reports
// ---------------------------------------------------------------------------------------------

void ResponderSession::SendTransferReport(TransferSetRuntime& rt, uint8_t condition, bool onlyPending, int64_t eventCode, const std::vector<BufferedChange>* changes, Due due)
{
	const ResponderModel::DataSet* ds = nullptr;
	{
		const auto it = clientDataSets.find(rt.dataSetKey);
		if(it != clientDataSets.end())
			ds = &it->second;
		else
			for(const auto& d : model.StaticDataSets())
				if(Key(d.name) == rt.dataSetKey)
					ds = &d;
	}
	if(ds == nullptr)
		return;
	if(!TleAllows(rt, due))
		return;
	const auto& c = model.Conf();
	const odc::msSinceEpoch_t nowMs = time.wallMs();

	TransferReport r;
	std::vector<Special> layout = LeadingSpecials(ds->members);
	const bool fromDefinition = !layout.empty() && layout.front() == Special::TransferSetName;
	if(!fromDefinition)
		layout = {Special::TransferSetName, Special::ConditionsDetected}
	;
	r.layout = layout;
	r.transferSet = ObjectRef{Scope::Icc, c.localDomain, rt.name};
	if(std::find(layout.begin(), layout.end(), Special::TimeStamp) != layout.end())
		r.timeStamp = static_cast<int64_t>(nowMs / 1000);
	if(std::find(layout.begin(), layout.end(), Special::ConditionsDetected) != layout.end())
		r.conditionsDetected = DsConditions{condition, DsConditionBitCount}
	;
	if(std::find(layout.begin(), layout.end(), Special::EventCode) != layout.end())
		r.eventCode = eventCode;

	const size_t first = fromDefinition ? layout.size() : 0;
	std::vector<mms::ObjectName> specialNames;
	for(size_t i = 0; i < layout.size(); ++i)
		specialNames.push_back(fromDefinition ? ds->members[i] : mms::ObjectName::Domain(c.localDomain, SpecialItem(layout[i])));

	//Block 3: the same report content in the blocked form (the standard's text only: nothing here has been seen from a real peer). The Critical bookkeeping, TLE and the
	//statistics below are those of any report.
	const bool blocked = BlockedHonoured(rt) && BuildBlockedValues(rt, *ds, first, onlyPending, changes, nowMs, specialNames, r);

	if(!blocked && onlyPending)
	{
		mms::ListOfVariable list;
		for(const auto& n : specialNames)
			list.push_back(mms::VariableSpec{n, std::nullopt});
		if(changes != nullptr)
		{
			//AllChangesReported: the members in the order the changes happened, with the values they had then
			for(const auto& change : *changes)
			{
				const auto positions = rt.pointPositions.find(change.point);
				if(positions == rt.pointPositions.end())
					continue;
				for(size_t pos : positions->second)
				{
					const size_t memberIndex = first + pos;
					if(memberIndex >= ds->members.size())
						continue;
					list.push_back(mms::VariableSpec{ds->members[memberIndex], std::nullopt});
					r.values.push_back(mms::AccessResult::Success(ToData(model.Point(change.point), change.state, nowMs)));
				}
			}
		}
		else
			for(size_t pos : rt.pending)
			{
				const size_t memberIndex = first + pos;
				if(memberIndex >= ds->members.size())
					continue;
				list.push_back(mms::VariableSpec{ds->members[memberIndex], std::nullopt});
				r.values.push_back(ReadMember(ds->members[memberIndex]));
			}
		r.specification = std::move(list);
	}
	else if(!blocked)
	{
		for(size_t i = first; i < ds->members.size(); ++i)
			r.values.push_back(ReadMember(ds->members[i]));
		if(fromDefinition)
			r.specification = ds->name;
		else
		{
			mms::ListOfVariable list;
			for(const auto& n : specialNames)
				list.push_back(mms::VariableSpec{n, std::nullopt});
			for(const auto& m : ds->members)
				list.push_back(mms::VariableSpec{m, std::nullopt});
			r.specification = std::move(list);
		}
	}
	try
	{
		if(conn.SendReport(BuildTransferReport(r)))
		{
			++reportsSent;
			if(blocked)
				++reportStats.blockedSent;
			if(rt.params.critical)
				TrackCriticalReport(rt, r.timeStamp);
		}
	}
	catch(const std::exception& e)
	{
		Log(stack::LogLevel::Error, std::string("could not build a transfer report: ") + e.what());
	}
}

// ----------
// Block 3: blocked reports
// ----------

bool ResponderSession::BlockedHonoured(const TransferSetRuntime& rt) const
{
	return rt.params.blockData && model.Conf().supportedBlocks.count(3) != 0;
}

//When a transfer set with BlockData (and Block 3 advertised) is reported in the ORDINARY form all the same, and why. The ONE place for these conditions: lift one here when a capture of a real
//peer has settled what it guards. Each is logged once per start of the transfer set.
//  - a member that is not a point (a device tag object): there is no octet layout for it.
//A single report can also fall back: a partial report whose largest position is above 255 needs rule 2 (2 octet positions: BuildBlockedValues; only when BlockedRule2 is false is it sent in the ordinary form),
//and one whose largest position is above 65535 fits no rule and is always sent in the ordinary form.
std::optional<std::string> ResponderSession::BlockedUnsendable(const TransferSetRuntime& rt, const ResponderModel::DataSet& ds) const
{
	const auto specials = LeadingSpecials(ds.members);
	const size_t first = specials.empty() || specials.front() != Special::TransferSetName ? 0 : specials.size();
	for(size_t i = first; i < ds.members.size(); ++i)
		if(!model.FindPoint(ds.members[i]))
			return "the data set member " + ds.members[i].itemId + " is not a point this port serves, which the blocked layout has no value for";
	return std::nullopt;
}

bool ResponderSession::BuildBlockedValues(TransferSetRuntime& rt, const ResponderModel::DataSet& ds, size_t first, bool onlyPending, const std::vector<BufferedChange>* changes,
	odc::msSinceEpoch_t nowMs, const std::vector<mms::ObjectName>& specialNames, TransferReport& report)
{
	//the layout has a storage form for the special objects and the points only; some transfer sets are reported in the ordinary form all the same (see BlockedUnsendable)
	if(const auto reason = BlockedUnsendable(rt, ds))
	{
		if(!rt.blockedFallbackLogged)
		{
			rt.blockedFallbackLogged = true;
			Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": BlockData is true but " + *reason + ": the reports of this transfer set are sent in the ordinary form");
		}
		return false;
	}
	//As a real peer sends it [V]: a COMPLETE report (the whole data set: the start-up and integrity reports, operator request, other external event, whatever the RBE attribute) is RULE 0:
	//every member in data set order, the special objects included. A PARTIAL report (only what changed: RBE) is RULE 1: (1 octet position, value) pairs of the changed points ONLY, no special
	//objects, a position the index in the data set's FULL member list, the special objects counted. Several points in one report are several pairs, in ascending position order [V]
	//(AllChangesReported lists them in the order of the changes instead [S]). Rule 2 (2 octet positions, unsigned, when a position is above 255) is the standard's text only [S]: it has never been
	//seen from a real peer, and ALL pairs of a report use the one rule, so one position above 255 makes every position of that report 2 octets.
	std::vector<blocked::Item> items;
	auto addPoint = [&](size_t position, size_t point, const PointState& state)
			    {
				    items.push_back(blocked::Item{position, ToPointValue(model.Point(point), state, nowMs), std::nullopt, {}
					    });
			    };
	blocked::Rule rule = blocked::Rule::NoRbe;
	if(!onlyPending)
	{
		for(size_t i = 0; i < first; ++i)
		{
			blocked::SpecialValue v;
			switch(report.layout[i])
			{
				case Special::TransferSetName: v.transferSet = report.transferSet; break;
				case Special::TimeStamp: v.number = report.timeStamp.value_or(0); break;
				case Special::ConditionsDetected: v.conditions = report.conditionsDetected ? report.conditionsDetected->mask : 0; break;
				case Special::EventCode: v.number = report.eventCode.value_or(0); break;
			}
			items.push_back(blocked::ObjectItem(i, report.layout[i], v));
		}
		for(size_t i = first; i < ds.members.size(); ++i)
		{
			const size_t point = *model.FindPoint(ds.members[i]);
			addPoint(i, point, model.State(point));
		}
	}
	else
	{
		if(changes != nullptr)
		{
			//AllChangesReported: the members in the order the changes happened, with the values they had then
			for(const auto& change : *changes)
			{
				const auto positions = rt.pointPositions.find(change.point);
				if(positions == rt.pointPositions.end())
					continue;
				for(size_t pos : positions->second)
					if(first + pos < ds.members.size())
						addPoint(first + pos, change.point, change.state);
			}
		}
		else
			for(size_t pos : rt.pending)
				if(first + pos < ds.members.size())
				{
					const size_t point = *model.FindPoint(ds.members[first + pos]);
					addPoint(first + pos, point, model.State(point));
				}
		size_t largest = 0;
		for(const auto& item : items)
			largest = std::max(largest, item.position);
		rule = blocked::ChooseRule(true, largest);
		if(rule == blocked::Rule::Offset2 && !model.Conf().blockedRule2)
		{
			//BlockedRule2 false: a report that needs a position above 255 is sent in the ordinary form
			if(!rt.blockedRule2Logged)
			{
				rt.blockedRule2Logged = true;
				Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": BlockData is true but a report needs a position above 255 (rule 2 of the blocked layout) and BlockedRule2 is false: such reports are sent in the ordinary form");
			}
			return false;
		}
	}
	Bytes data;
	try
	{
		data = blocked::Encode(rule, items);
	}
	catch(const std::exception& e)
	{
		//a name longer than its 32 octet storage form, or a position beyond the two octets of rule 2 (above 65535): this report is sent in the ordinary form
		if(!rt.blockedFallbackLogged)
		{
			rt.blockedFallbackLogged = true;
			Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": BlockData is true but the blocked form cannot be built (" + e.what() + "): such reports are sent in the ordinary form");
		}
		return false;
	}
	mms::ListOfVariable list;
	for(const auto& n : specialNames)
		list.push_back(mms::VariableSpec{n, std::nullopt});
	list.push_back(mms::VariableSpec{mms::ObjectName::Vmd(model.Conf().blockedDataName), std::nullopt}); //VMD-specific, as a real peer's [V]
	report.specification = std::move(list);
	report.values.clear();
	report.values.push_back(mms::AccessResult::Success(mms::Data::OctetString(data)));
	Log(stack::LogLevel::Debug, "transfer set " + rt.name + ": blocked report, " + blocked::ToString(static_cast<blocked::Rule>(data[0])) + ", " + std::to_string(items.size()) + " values, " +
		std::to_string(data.size()) + " octets in the variable " + model.Conf().blockedDataName);
	return true;
}

// ----------
// TLE and critical reports
// ----------

bool ResponderSession::TleAllows(TransferSetRuntime& rt, const Due& due)
{
	const auto limit = std::chrono::seconds(rt.params.tle);
	const bool late = rt.params.tle > 0 && due && (time.steady() - *due) > limit;
	if(!late)
	{
		rt.tleExceeded = false;
		return true;
	}
	const auto lateMs = std::chrono::duration_cast<std::chrono::milliseconds>(time.steady() - *due).count();
	if(rt.tleExceeded)
	{
		//the standard: after a report that exceeded TLE the next one is generated whether or not it would exceed it too
		rt.tleExceeded = false;
		Log(stack::LogLevel::Debug, "transfer set " + rt.name + ": this report is " + std::to_string(lateMs) + " ms late (TLE " + std::to_string(rt.params.tle) +
			" s), but the previous one was discarded for TLE: sent regardless");
		return true;
	}
	rt.tleExceeded = true;
	++reportStats.tleDiscarded;
	Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": a report was due " + std::to_string(lateMs) + " ms before it could be sent, more than its time limit TLE (" +
		std::to_string(rt.params.tle) + " s): it is discarded, the next report is sent regardless");
	return false;
}

void ResponderSession::TrackCriticalReport(TransferSetRuntime& rt, const std::optional<int64_t>& timeStamp)
{
	++reportStats.criticalSent;
	const size_t cap = std::max<size_t>(model.Conf().maxUnacknowledgedReports, 1);
	while(rt.outstanding.size() >= cap)
	{
		const auto oldest = rt.outstanding.front();
		rt.outstanding.pop_front();
		++reportStats.ackOverflows;
		ReportFailure(rt, oldest, "more than " + std::to_string(cap) + " critical reports are waiting for an acknowledgement (MaxUnacknowledgedReports): the oldest is given up");
	}
	rt.outstanding.push_back(TransferSetRuntime::Outstanding{timeStamp, time.steady()});
}

void ResponderSession::ReportFailure(TransferSetRuntime& rt, const TransferSetRuntime::Outstanding& report, const std::string& why)
{
	++reportStats.dataFailures;
	//the standard raises the Data_failure event condition here; this port has no event conditions, so it is a log line and a counter [S]
	Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": Data_failure: the critical report" + (report.timeStamp ? " with time stamp " + std::to_string(*report.timeStamp) : std::string()) + " was not acknowledged: " + why);
}

void ResponderSession::TickAcks()
{
	const auto timeoutMs = model.Conf().reportAckTimeoutMs;
	if(timeoutMs == 0)
		return;
	const auto timeout = std::chrono::milliseconds(timeoutMs);
	const auto now = time.steady();
	for(auto& rt : transferSets)
		//oldest first: the first one that has not run out ends the check for this transfer set
		while(!rt.outstanding.empty() && now - rt.outstanding.front().sentAt >= timeout)
		{
			const auto report = rt.outstanding.front();
			rt.outstanding.pop_front();
			++reportStats.ackTimeouts;
			ReportFailure(rt, report, "no Transfer_Report_ACK within " + std::to_string(timeoutMs) + " ms (ReportAckTimeoutms)");
		}
}

bool ResponderSession::OnInformationReport(const mms::InformationReport& report)
{
	std::optional<ReportAck> ack;
	try
	{
		ack = ParseReportAck(report);
	}
	catch(const DecodeError& e)
	{
		Log(stack::LogLevel::Warn, std::string("a transfer report acknowledgement could not be read, ignored: ") + e.what());
		return true;
	}
	if(!ack)
		return false;
	const std::string what = ack->negative ? Name::TransferReportNack : Name::TransferReportAck;
	const std::string stampText = ack->timeStamp ? " (time stamp " + std::to_string(*ack->timeStamp) + ")" : std::string();
	TransferSetRuntime* rt = (ack->transferSet.scope == Scope::Icc && ack->transferSet.domain == model.Conf().localDomain) ? FindTransferSet(ack->transferSet.name) : nullptr;
	if(rt == nullptr)
	{
		Log(stack::LogLevel::Debug, what + " for " + ack->transferSet.name + ", which is not a transfer set here: ignored");
		return true;
	}
	//the oldest outstanding report it can be for: by time stamp when both have one (a missing one matches any: the peer's report had none to echo, or the
	//acknowledgement left it out)
	auto match = rt->outstanding.end();
	for(auto it = rt->outstanding.begin(); it != rt->outstanding.end(); ++it)
		if(!ack->timeStamp || !it->timeStamp || *ack->timeStamp == *it->timeStamp)
		{
			match = it;
			break;
		}
	if(ack->negative)
	{
		++reportStats.nacksReceived;
		if(!rt->params.critical)
		{
			Log(stack::LogLevel::Warn, "transfer set " + rt->name + ": the client sent " + what + stampText + ", though the transfer set is not critical: a report was discarded by the client (logged only)");
			return true;
		}
		const std::optional<int64_t> stamp = ack->timeStamp;
		TransferSetRuntime::Outstanding failed{stamp, time.steady()};
		bool matched = false;
		if(match != rt->outstanding.end())
		{
			failed = *match;
			rt->outstanding.erase(match);
			matched = true;
		}
		ReportFailure(*rt, failed, "the client sent " + what + ": it discarded the report" + (matched ? "" : " (none outstanding matched it: counted all the same)"));
		return true;
	}
	if(match == rt->outstanding.end())
	{
		Log(stack::LogLevel::Debug, "transfer set " + rt->name + ": " + what + stampText + " matches no outstanding critical report (it was late, repeated or for another time stamp): ignored");
		return true;
	}
	if(!ack->timeStamp)
		Log(stack::LogLevel::Debug, "transfer set " + rt->name + ": " + what + " without a time stamp: taken as the acknowledgement of the oldest outstanding report");
	rt->outstanding.erase(match);
	++reportStats.acknowledged;
	return true;
}

void ResponderSession::AssociationEnded()
{
	for(auto& rt : transferSets)
	{
		rt.outstanding.clear();
		rt.tleExceeded = false;
	}
}

size_t ResponderSession::OutstandingCriticalReports() const
{
	size_t n = 0;
	for(const auto& rt : transferSets)
		n += rt.outstanding.size();
	return n;
}

size_t ResponderSession::OutstandingCriticalReports(const std::string& transferSet) const
{
	for(const auto& rt : transferSets)
		if(rt.name == transferSet)
			return rt.outstanding.size();
	return 0;
}

void ResponderSession::FlushBuffer(TransferSetRuntime& rt, Due due)
{
	if(AllChanges(rt))
	{
		FlushChanges(rt, DsCondition::ObjectChange, due);
		return;
	}
	rt.bufferDeadline.reset();
	if(rt.pending.empty())
		return;
	SendTransferReport(rt, DsCondition::ObjectChange, rt.params.rbe, 0, nullptr, due);
	rt.pending.clear();
}

bool ResponderSession::AllChanges(const TransferSetRuntime& rt)
{
	return rt.params.allChangesReported && rt.params.rbe && rt.params.bufferTime > 0 && rt.params.conditionsRequested.Has(DsCondition::ObjectChange);
}

void ResponderSession::BufferChange(TransferSetRuntime& rt, size_t pointIndex)
{
	if(rt.changes.size() >= model.Conf().maxBufferedChanges)
	{
		//the first overflow of a start is a warning, the later ones are only traced
		const bool first = !rt.overflowLogged;
		if(first)
		{
			rt.overflowLogged = true;
			Log(stack::LogLevel::Warn, "transfer set " + rt.name + ": more than " + std::to_string(model.Conf().maxBufferedChanges) +
				" changes within the buffer time (MaxBufferedChanges): from now on only the last value of a point that already has a change in the buffer is kept");
		}
		//the last value wins: replace the latest buffered change of the same point (a point with none is still added, so no change is lost altogether)
		bool replaced = false;
		for(auto it = rt.changes.rbegin(); it != rt.changes.rend(); ++it)
			if(it->point == pointIndex)
			{
				it->state = model.State(pointIndex);
				replaced = true;
				break;
			}
		if(!first)
			Log(stack::LogLevel::Trace, "transfer set " + rt.name + ": the buffer is full (" + std::to_string(rt.changes.size()) + " changes): " +
				(replaced ? "the latest buffered change of point " + model.Point(pointIndex).name + " was replaced by its new value"
				          : "a change of point " + model.Point(pointIndex).name + " was added, it had none in the buffer"));
		if(replaced)
			return;
	}
	rt.changes.push_back(BufferedChange{pointIndex, model.State(pointIndex)});
}

void ResponderSession::FlushChanges(TransferSetRuntime& rt, uint8_t condition, Due due)
{
	rt.bufferDeadline.reset();
	std::vector<BufferedChange> changes;
	changes.swap(rt.changes);
	//a report ends where a point would be in it twice: the reports follow the order of the changes, and none repeats a point
	std::vector<BufferedChange> report;
	std::set<size_t> inReport;
	for(auto& change : changes)
	{
		if(!inReport.insert(change.point).second)
		{
			SendTransferReport(rt, condition, true, 0, &report, due);
			report.clear();
			inReport = {change.point};
		}
		report.push_back(std::move(change));
	}
	if(!report.empty())
		SendTransferReport(rt, condition, true, 0, &report, due);
}

void ResponderSession::OnPointChanged(size_t pointIndex)
{
	const auto now = time.steady();
	for(auto& rt : transferSets)
	{
		//changes are tracked for ObjectChange reports, and for RBE interval reports (which send what changed since the last one)
		const bool onChange = rt.params.conditionsRequested.Has(DsCondition::ObjectChange);
		const bool rbeInterval = rt.params.rbe && rt.params.conditionsRequested.Has(DsCondition::IntervalTimeOut);
		if(!rt.enabled || now < rt.activeFrom || (!onChange && !rbeInterval))
			continue;
		const ResponderModel::DataSet* ds = nullptr;
		const auto it = clientDataSets.find(rt.dataSetKey);
		if(it != clientDataSets.end())
			ds = &it->second;
		else
			for(const auto& d : model.StaticDataSets())
				if(Key(d.name) == rt.dataSetKey)
					ds = &d;
		if(ds == nullptr)
			continue;
		if(!rt.positionsBuilt)
		{
			//once per transfer set start: which member positions are which point
			const auto specials = LeadingSpecials(ds->members);
			const size_t first = specials.empty() || specials.front() != Special::TransferSetName ? 0 : specials.size();
			for(size_t i = first; i < ds->members.size(); ++i)
				if(const auto idx = model.FindPoint(ds->members[i]))
					rt.pointPositions[*idx].push_back(i - first);
			rt.positionsBuilt = true;
		}
		const auto positions = rt.pointPositions.find(pointIndex);
		if(positions == rt.pointPositions.end())
			continue;
		if(AllChanges(rt))
			BufferChange(rt, pointIndex);
		else
			for(size_t pos : positions->second)
				rt.pending.insert(pos);
		if(!onChange)
			continue; //RBE interval reports only: the changes wait for the next interval
		if(rt.params.bufferTime <= 0)
			FlushBuffer(rt);
		else if(!rt.bufferDeadline)
			rt.bufferDeadline = now + std::chrono::seconds(rt.params.bufferTime);
	}
}

// ---------------------------------------------------------------------------------------------
// Block 5 (device control): see Tase2Control.h for the model
// ---------------------------------------------------------------------------------------------

mms::AccessResult ResponderSession::ReadControl(const ResponderModel::ControlRef& ref)
{
	if(ref.tag)
		return ReadTag(ref.control);
	if(!ref.select)
	{
		//the device variable is write only: nothing of it is readable
		++controlStats.refused;
		Log(stack::LogLevel::Warn, "control " + model.Conf().controls[ref.control].name + ": a Read of the device is refused: " + ToString(ControlError::Refused) +
			" (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(ControlError::Refused)) + "): the device is write only");
		return mms::AccessResult::Failure(DataAccessErrorOf(ControlError::Refused));
	}
	//the Read of the select object is the SELECT: IDLE -> ARMED, and the answer is the CheckBackID of the bilateral table
	const auto& c = model.Conf().controls[ref.control];
	Device& d = devices[ref.control];
	if(d.state == Device::State::Armed && time.steady() >= d.expiry)
		TickControls(); //the select has lapsed
	auto refuse = [&](ControlError e, const char* detail)
			  {
				  ++controlStats.refused;
				  Log(e == ControlError::Hardware ? stack::LogLevel::Error : stack::LogLevel::Warn,
					  "control " + c.name + ": select refused: " + ToString(e) + (*detail ? std::string(": ") + detail : std::string()) +
					  " (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(e)) + ")");
				  return mms::AccessResult::Failure(DataAccessErrorOf(e));
			  };
	if(d.state == Device::State::Armed)
		return refuse(ControlError::Busy, "already armed");
	if(d.state == Device::State::Operating)
		return refuse(ControlError::Busy, "an operation is in flight");
	if(!controlHandler)
		return refuse(ControlError::Hardware, "nothing downstream to carry out an operate");
	//a device tagged OPEN_AND_CLOSE_INHIBIT cannot be selected (IEC 60870-6-503 clauses 5.3.12.3 and 7.2.6.2.3); the other tags allow the select [T]
	if(c.taggable)
	{
		const auto tag = model.TagOf(ref.control);
		if(tag.tag == DeviceTag::OpenAndCloseInhibit)
		{
			model.CountTag(ResponderModel::TagEvent::SelectRefused);
			return refuse(ControlError::Tagged, (std::string("the device is tagged ") + ToString(tag.tag) + (tag.reason.empty() ? "" : " (" + tag.reason + ")")).c_str());
		}
	}
	d.state = Device::State::Armed;
	d.expiry = time.steady() + std::chrono::milliseconds(c.selectTimeoutMs);
	++controlStats.selects;
	const int64_t id = c.ServedCheckBackId();
	Log(stack::LogLevel::Info, "control " + c.name + " selected (CheckBackID " + std::to_string(id) + ", for " + std::to_string(c.selectTimeoutMs) + " ms)");
	return mms::AccessResult::Success(SelectToData(id));
}

void ResponderSession::WriteControl(const ResponderModel::ControlRef& ref, const mms::Data& data, std::function<void(std::optional<ControlError>)> complete)
{
	if(ref.tag)
		WriteTag(ref.control, data, complete);
	else if(ref.select)
		RefuseControl(model.Conf().controls[ref.control], "write", ControlError::Refused, complete, "the select object is only read (a Read is the select)");
	else
		WriteOperate(ref.control, data, complete);
}

void ResponderSession::RefuseControl(const IccpControlConf& c, const char* what, ControlError e, const std::function<void(std::optional<ControlError>)>& complete, const std::string& detail)
{
	++controlStats.refused;
	Log(e == ControlError::Hardware ? stack::LogLevel::Error : stack::LogLevel::Warn,
		std::string("control ") + c.name + ": " + what + " refused: " + ToString(e) + (detail.empty() ? "" : ": " + detail) +
		" (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(e)) + ")");
	complete(e);
}

void ResponderSession::WriteOperate(size_t control, const mms::Data& data, const std::function<void(std::optional<ControlError>)>& complete)
{
	const auto& c = model.Conf().controls[control];
	Device& d = devices[control];
	if(d.state == Device::State::Armed && time.steady() >= d.expiry)
		TickControls();
	//the device state first (IEC 60870-6-503 clause 7.2.6.3.3): busy, and for a device that needs a select, armed by this association (a new association starts idle)
	if(d.state == Device::State::Operating)
	{
		RefuseControl(c, "operate", ControlError::Busy, complete, "an operation is in flight");
		return;
	}
	if(c.selectRequired && d.state != Device::State::Armed)
	{
		RefuseControl(c, "operate", ControlError::NotSelected, complete, "the device is not selected (armed) by this association");
		return;
	}
	ControlValue dd;
	try
	{
		dd = ParseOperateData(c.type, data);
	}
	catch(const DecodeError&)
	{
		if(!IsRealControl(c.type) && data.type == mms::DataType::Unsigned)
		{
			//an unsigned integer too big for 63 bits: far outside the width of 16
			RefuseControl(c, "operate", ControlError::OutOfRange, complete, "the value " + std::to_string(data.AsUnsigned()) + " does not fit an integer of width 16 (accepted: " +
				std::to_string(MinControlValue) + " to " + std::to_string(MaxAcceptedControlValue) + ")");
			return;
		}
		RefuseControl(c, "operate", ControlError::TypeMismatch, complete, "got " + DescribeData(data) + ", expected " + ExpectedControlData(c.type));
		return;
	}
	if(!IsRealControl(c.type) && (dd.integer < MinControlValue || dd.integer > MaxAcceptedControlValue))
	{
		//the device variable is an integer of width 16 (IEC 60870-6-802 clause 6.2.2); a client may send it signed or unsigned
		RefuseControl(c, "operate", ControlError::OutOfRange, complete, "the value " + std::to_string(dd.integer) + " does not fit an integer of width 16 (accepted: " +
			std::to_string(MinControlValue) + " to " + std::to_string(MaxAcceptedControlValue) + ")");
		return;
	}
	//the value: what the downstream control event carries
	ControlOperation op;
	op.control = &c;
	op.integer = dd.integer;
	if(!IsSetpoint(c.type))
	{
		if(dd.integer == c.commandValueOn)
			op.on = true;
		else if(dd.integer == c.commandValueOff)
			op.on = false;
		else
		{
			RefuseControl(c, "operate", ControlError::NotSupported, complete, "the value " + std::to_string(dd.integer) + " is neither ON nor OFF");
			return;
		}
	}
	else
	{
		const double tase = IsRealControl(c.type) ? dd.real : static_cast<double>(dd.integer);
		op.value = tase * c.scale + c.offset;
		if(!std::isfinite(op.value))
		{
			RefuseControl(c, "operate", ControlError::OutOfRange, complete);
			return;
		}
	}
	//the tag of the device against the command (IEC 60870-6-503 clauses 5.3.12.4 and 7.2.6.3.3): NO_TAG never prevents, OPEN_AND_CLOSE_INHIBIT always, CLOSE_ONLY_INHIBIT a
	//Close or Raise command only. Checked after the device state and the data (the command is known by then); a refused operate leaves an armed device armed [S]
	if(c.taggable)
	{
		const auto tag = model.TagOf(control);
		if(TagPrevents(tag.tag, c.type, op.on))
		{
			model.CountTag(ResponderModel::TagEvent::OperateRefused);
			RefuseControl(c, "operate", ControlError::Tagged, complete, std::string("the device is tagged ") + ToString(tag.tag) + (tag.reason.empty() ? "" : " (" + tag.reason + ")") +
				(IsSetpoint(c.type) ? "" : std::string(", which prevents ") + (op.on ? "ON (Close)" : "OFF (Open)")));
			return;
		}
	}
	if(!controlHandler)
	{
		RefuseControl(c, "operate", ControlError::NotSupported, complete);
		return;
	}
	//carry it out downstream; the Write is answered when that has finished (or when it has not, after the timeout). The select is used up.
	d.state = Device::State::Operating;
	d.generation = nextGeneration++;
	d.expiry = time.steady() + std::chrono::milliseconds(c.EffectiveTimeoutMs());
	d.complete = complete;
	const uint64_t generation = d.generation;
	std::weak_ptr<bool> guard = alive;
	controlHandler(op, [this, guard, control, generation, dd](odc::CommandStatus status)
		{
			if(guard.expired())
				return;
			Device& dev = devices[control];
			if(dev.state != Device::State::Operating || dev.generation != generation)
				return; //it has timed out already
			const auto& cc = model.Conf().controls[control];
			auto finish = std::move(dev.complete);
			dev.complete = nullptr;
			dev.state = Device::State::Idle;
			if(status == odc::CommandStatus::SUCCESS)
			{
				++controlStats.operates;
				Log(stack::LogLevel::Info, "control " + cc.name + " operated by the peer: " + (IsSetpoint(cc.type) ? "setpoint " + std::to_string(IsRealControl(cc.type) ? dd.real : static_cast<double>(dd.integer))
					                                                                                            : std::string(dd.integer == cc.commandValueOn ? "ON" : "OFF")));
				finish(std::nullopt);
			}
			else
			{
				++controlStats.refused;
				const ControlError e = ControlErrorOf(status);
				Log(stack::LogLevel::Error, "control " + cc.name + ": downstream answered the operate with " + odc::ToString(status) + ": " + ToString(e) +
					" (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(e)) + ")");
				finish(e);
			}
		});
}

bool ResponderSession::DeviceArmed(size_t control) const
{
	if(control >= devices.size())
		return false;
	const Device& d = devices[control];
	return d.state == Device::State::Armed && time.steady() < d.expiry;
}

TagValue ResponderSession::CurrentTag(size_t control) const
{
	const auto t = model.TagOf(control);
	TagValue v;
	v.tag = t.tag;
	v.reason = t.reason;
	v.armed = DeviceArmed(control); //tag_state: ARMED=1
	return v;
}

namespace
{
std::string DescribeTag(const TagValue& v)
{
	return std::string(ToString(v.tag)) + (v.reason.empty() ? "" : " (" + v.reason + ")") + ", " + (v.armed ? "ARMED" : "IDLE");
}
}

mms::AccessResult ResponderSession::ReadTag(size_t control)
{
	const auto& c = model.Conf().controls[control];
	if(!c.taggable)
	{
		//the device has no Tag attribute (IEC 60870-6-503 clause 7.2.6.9.3)
		model.CountTag(ResponderModel::TagEvent::Refused);
		Log(stack::LogLevel::Warn, "control " + c.name + ": Get Tag refused: " + ToString(ControlError::Unknown) + ": the device has no Tag attribute (Taggable is not set) (data access error " +
			mms::DescribeDataAccessError(DataAccessErrorOf(ControlError::Unknown)) + ")");
		return mms::AccessResult::Failure(DataAccessErrorOf(ControlError::Unknown));
	}
	const TagValue v = CurrentTag(control);
	Log(stack::LogLevel::Debug, "control " + c.name + ": Get Tag: " + DescribeTag(v));
	return mms::AccessResult::Success(TagToData(v));
}

void ResponderSession::WriteTag(size_t control, const mms::Data& data, const std::function<void(std::optional<ControlError>)>& complete)
{
	const auto& c = model.Conf().controls[control];
	auto refuse = [&](ControlError e, const std::string& detail)
			  {
				  model.CountTag(ResponderModel::TagEvent::Refused);
				  Log(stack::LogLevel::Warn, "control " + c.name + ": Set Tag refused: " + ToString(e) + ": " + detail + " (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(e)) + ")");
				  complete(e);
			  };
	//the checks of IEC 60870-6-503 clause 7.2.6.8.3, in its order: the Tag attribute exists, the device is not ARMED or busy, the owner of a VCC device's tag. There are no access
	//control specifications in this port (any client may Get Tag and Set Tag), so object-access-denied does not arise. The data is checked after the existence [S].
	if(!c.taggable)
	{
		refuse(ControlError::Unknown, "the device has no Tag attribute (Taggable is not set)");
		return;
	}
	TagValue v;
	try
	{
		v = ParseTagData(data);
	}
	catch(const DecodeError& e)
	{
		refuse(ControlError::TypeMismatch, "got " + DescribeData(data) + ", expected a Tag_Value (" + e.what() + ")");
		return;
	}
	Device& d = devices[control];
	if(d.state == Device::State::Armed && time.steady() >= d.expiry)
		TickControls(); //the select has lapsed
	if(d.state == Device::State::Armed)
	{
		refuse(ControlError::Busy, "the device is ARMED (selected)");
		return;
	}
	if(d.state == Device::State::Operating)
	{
		refuse(ControlError::Busy, "an operation is in flight");
		return;
	}
	const auto current = model.TagOf(control);
	if(c.scope == Scope::Vcc && current.tag != DeviceTag::NoTag && (!current.owner || *current.owner != clientId))
	{
		refuse(ControlError::Tagged, current.owner ? "the device is tagged " + std::string(ToString(current.tag)) + " by another client: only the client that set the tag of a VCC device can change it" :
			"the device is tagged " + std::string(ToString(current.tag)) + " locally: only the local operator can change the tag of a VCC device that was tagged there");
		return;
	}
	model.SetTag(control, v.tag, v.reason, clientId);
	model.CountTag(ResponderModel::TagEvent::Set);
	Log(stack::LogLevel::Info, "control " + c.name + ": tag set by the peer: " + ToString(v.tag) + (v.reason.empty() ? "" : " (" + v.reason + ")"));
	complete(std::nullopt);
}

bool ResponderSession::LocalReset(size_t control)
{
	if(control >= devices.size())
		return false;
	Device& d = devices[control];
	if(d.state == Device::State::Armed && time.steady() >= d.expiry)
		TickControls(); //the select has lapsed: nothing is left to reset
	if(d.state != Device::State::Armed)
		return false;
	d.state = Device::State::Idle;
	model.CountTag(ResponderModel::TagEvent::LocalReset);
	Log(stack::LogLevel::Info, "control " + model.Conf().controls[control].name + ": local reset: the device went from ARMED to IDLE (the MMS event notification of the action is not sent)");
	return true;
}

void ResponderSession::TickControls()
{
	const auto now = time.steady();
	for(size_t i = 0; i < devices.size(); ++i)
	{
		Device& d = devices[i];
		const auto& c = model.Conf().controls[i];
		if(d.state == Device::State::Armed && now >= d.expiry)
		{
			Log(stack::LogLevel::Info, "the selection of control " + c.name + " expired");
			d.state = Device::State::Idle;
		}
		else if(d.state == Device::State::Operating && now >= d.expiry)
		{
			//downstream has not answered: give up on it (a late answer is ignored) so the peer's Write is answered
			auto finish = std::move(d.complete);
			d.complete = nullptr;
			d.state = Device::State::Idle;
			d.generation = 0;
			++controlStats.refused;
			Log(stack::LogLevel::Error, "control " + c.name + ": downstream did not answer the operate within " + std::to_string(c.EffectiveTimeoutMs()) + " ms: " +
				ToString(ControlError::Hardware) + " (data access error " + mms::DescribeDataAccessError(DataAccessErrorOf(ControlError::Hardware)) + ")");
			if(finish)
				finish(ControlError::Hardware);
		}
	}
}

void ResponderSession::Tick()
{
	TickControls();
	TickAcks();
	const auto now = time.steady();
	for(auto& rt : transferSets)
	{
		if(!rt.enabled || now < rt.activeFrom)
			continue;
		if(rt.bufferDeadline && now >= *rt.bufferDeadline)
			FlushBuffer(rt, *rt.bufferDeadline); //due when the buffer time ended (TLE counts from there)
		uint8_t cond = 0;
		Due due; //the earliest time a periodic report of this tick was due
		if(rt.nextIntegrity && now >= *rt.nextIntegrity)
		{
			cond = static_cast<uint8_t>(cond | DsCondition::IntegrityTimeOut);
			due = *rt.nextIntegrity;
			auto next = *rt.nextIntegrity + std::chrono::seconds(rt.params.integrityCheck);
			while(next <= now)
				next += std::chrono::seconds(rt.params.integrityCheck);
			rt.nextIntegrity = next;
		}
		if(rt.nextInterval && now >= *rt.nextInterval)
		{
			cond = static_cast<uint8_t>(cond | DsCondition::IntervalTimeOut);
			due = due ? std::min(*due, *rt.nextInterval) : *rt.nextInterval;
			auto next = *rt.nextInterval + std::chrono::seconds(rt.params.interval);
			while(next <= now)
				next += std::chrono::seconds(rt.params.interval);
			rt.nextInterval = next;
		}
		if(cond != 0)
		{
			if(cond == DsCondition::IntervalTimeOut && rt.params.rbe)
			{
				//report by exception: the interval report carries only what changed since the last report (nothing, if nothing did)
				if(AllChanges(rt))
				{
					FlushChanges(rt, cond, due);
					continue;
				}
				rt.bufferDeadline.reset();
				if(!rt.pending.empty())
					SendTransferReport(rt, cond, true, 0, nullptr, due);
				rt.pending.clear();
				continue;
			}
			//any other periodic report is complete and supersedes pending changes, but with AllChangesReported they were all to be reported: they go first
			if(AllChanges(rt))
				FlushChanges(rt, DsCondition::ObjectChange, due);
			rt.pending.clear();
			rt.bufferDeadline.reset();
			SendTransferReport(rt, cond, false, 0, nullptr, due);
		}
	}
}

size_t ResponderSession::RaiseCondition(uint8_t condition, int64_t eventCode, const std::string& transferSet)
{
	size_t sent = 0;
	for(auto& rt : transferSets)
	{
		if(!rt.enabled || !rt.params.conditionsRequested.Has(condition))
			continue;
		if(!transferSet.empty() && rt.name != transferSet)
			continue;
		if(AllChanges(rt))
			FlushChanges(rt, DsCondition::ObjectChange); //the changes that came before this report are reported before it
		const size_t before = reportsSent;
		SendTransferReport(rt, condition, false, eventCode);
		if(reportsSent > before)
			++sent;
	}
	return sent;
}

} // namespace tase2
} // namespace iccp
