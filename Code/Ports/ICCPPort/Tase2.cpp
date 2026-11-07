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
 * Tase2.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Blocks 1 and 2 on top of the MMS codec.
 */

#include "Tase2.h"
#include <algorithm>
#include <stdexcept>

namespace iccp
{
namespace tase2
{

using mms::AccessResult;
using mms::DataType;

namespace
{
void RequireType(const Data& d, DataType type, const char* what)
{
	if(d.type != type)
		throw DecodeError(std::string("TASE.2: ") + what + " has the wrong type");
}

//Integer or Unsigned data as a signed value
int64_t IntValue(const Data& d, const char* what)
{
	if(d.type == DataType::Integer)
		return d.AsInteger();
	if(d.type == DataType::Unsigned)
	{
		const uint64_t v = d.AsUnsigned();
		if(v > static_cast<uint64_t>(INT64_MAX))
			throw DecodeError(std::string("TASE.2: ") + what + " out of range");
		return static_cast<int64_t>(v);
	}
	throw DecodeError(std::string("TASE.2: ") + what + " is not an integer");
}

std::string VisibleValue(const Data& d, const char* what)
{
	RequireType(d, DataType::VisibleString, what);
	return d.AsString();
}

bool BoolValue(const Data& d, const char* what)
{
	RequireType(d, DataType::Boolean, what);
	return d.AsBoolean();
}

mms::ListOfVariable ListOf(std::initializer_list<ObjectName> names)
{
	mms::ListOfVariable l;
	for(const auto& n : names)
		l.push_back(mms::VariableSpec{n, std::nullopt});
	return l;
}

const mms::ListOfVariable& RequireList(const mms::VariableAccessSpecification& spec, const char* what)
{
	const auto* list = std::get_if<mms::ListOfVariable>(&spec);
	if(list == nullptr)
		throw DecodeError(std::string("TASE.2: ") + what + " is not a list of variables");
	return *list;
}

//Eight bit flags octet as a bit string and back
Data FlagsBits(uint8_t byte)
{
	mms::BitString bs;
	bs.data = Bytes{byte};
	return Data::Bits(bs);
}

uint8_t ParseFlagsBits(const Data& d, const char* what)
{
	RequireType(d, DataType::BitString, what);
	if(d.content.size() != 2 || d.content[0] != 0)
		throw DecodeError(std::string("TASE.2: ") + what + " is not an 8 bit string");
	return d.content[1];
}

Data StateBits(StateValue s)
{
	mms::BitString bs;
	bs.data = Bytes{static_cast<uint8_t>(static_cast<uint8_t>(s) << 6)};
	bs.unusedBits = 6;
	return Data::Bits(bs);
}

StateValue ParseStateBits(const Data& d)
{
	RequireType(d, DataType::BitString, "state");
	if(d.content.size() != 2 || d.content[0] != 6 || (d.content[1] & 0x3F) != 0)
		throw DecodeError("TASE.2: state is not a 2 bit string");
	return static_cast<StateValue>(d.content[1] >> 6);
}

Data TimeStampExtended(int64_t seconds, int64_t milliseconds)
{
	return Data::Structure({Data::Integer(seconds), Data::Integer(milliseconds)});
}

void ParseTimeStampExtended(const Data& d, PointValue& p)
{
	RequireType(d, DataType::Structure, "extended time stamp");
	if(d.elements.size() != 2)
		throw DecodeError("TASE.2: extended time stamp must have 2 members");
	p.timeStamp = IntValue(d.elements[0], "time stamp");
	p.milliseconds = IntValue(d.elements[1], "milliseconds");
}

std::optional<Special> SpecialOf(const std::string& item)
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
}

// ---------------------------------------------------------------------------------------------
// names
// ---------------------------------------------------------------------------------------------

Scope ScopeOf(const ObjectName& name)
{
	switch(name.scope)
	{
		case mms::ObjectScope::VmdSpecific: return Scope::Vcc;
		case mms::ObjectScope::DomainSpecific: return Scope::Icc;
		default: throw DecodeError("TASE.2: aa-specific names are not used");
	}
}

ObjectRef ObjectRef::FromName(const ObjectName& n)
{
	ObjectRef r;
	r.scope = ScopeOf(n) == Scope::Icc ? Scope::Icc : Scope::Vcc;
	r.domain = n.domainId;
	r.name = n.itemId;
	return r;
}

ObjectName ObjectRef::ToName() const
{
	return scope == Scope::Icc ? ObjectName::Domain(domain, name) : ObjectName::Vmd(name);
}

Data ToData(const ObjectRef& ref)
{
	return Data::Structure({Data::Unsigned(static_cast<uint64_t>(ref.scope)), Data::VisibleString(ref.domain), Data::VisibleString(ref.name)});
}

ObjectRef ParseObjectRef(const Data& data)
{
	RequireType(data, DataType::Structure, "object name");
	if(data.elements.size() != 2 && data.elements.size() != 3)
		throw DecodeError("TASE.2: object name must have 2 or 3 members");
	ObjectRef r;
	const int64_t scope = IntValue(data.elements[0], "scope");
	if(scope != 0 && scope != 1)
		throw DecodeError("TASE.2: unknown scope");
	r.scope = static_cast<Scope>(scope);
	if(data.elements.size() == 3)
	{
		r.domain = VisibleValue(data.elements[1], "domain");
		r.name = VisibleValue(data.elements[2], "name");
	}
	else
	{
		if(r.scope == Scope::Icc)
			throw DecodeError("TASE.2: an ICC object name needs a domain");
		r.name = VisibleValue(data.elements[1], "name");
	}
	return r;
}

// ---------------------------------------------------------------------------------------------
// association objects
// ---------------------------------------------------------------------------------------------

Data ToData(const Version& v)
{
	return Data::Structure({Data::Integer(v.major), Data::Integer(v.minor)});
}

Version ParseVersion(const Data& data)
{
	RequireType(data, DataType::Structure, "version");
	if(data.elements.size() != 2)
		throw DecodeError("TASE.2: version must have 2 members");
	return Version{IntValue(data.elements[0], "major version"), IntValue(data.elements[1], "minor version")};
}

mms::BitString MakeSupportedFeatures(std::initializer_list<Block> blocks, size_t bitCount)
{
	if(bitCount < 9)
		throw std::invalid_argument("TASE.2: Supported_Features needs at least 9 bits");
	mms::BitString bs(bitCount);
	for(Block b : blocks)
		bs.Set(static_cast<size_t>(b), true);
	return bs;
}

bool HasBlock(const mms::BitString& features, Block block)
{
	return features.Get(static_cast<size_t>(block));
}

Data ToData(const mms::BitString& features)
{
	return Data::Bits(features);
}

mms::BitString ParseSupportedFeatures(const Data& data)
{
	RequireType(data, DataType::BitString, "Supported_Features");
	return data.AsBits();
}

mms::ReadRequest BuildAssociationInfoRequest(const std::string& iccDomain, bool version, bool features)
{
	mms::ReadRequest r;
	mms::ListOfVariable list = ListOf({IccName(iccDomain, Name::BilateralTableId)});
	if(version)
		list.push_back(mms::VariableSpec{VccName(Name::Tase2Version), std::nullopt});
	if(features)
		list.push_back(mms::VariableSpec{VccName(Name::SupportedFeatures), std::nullopt});
	r.specification = std::move(list);
	return r;
}

AssociationInfo ParseAssociationInfoResponse(const mms::ReadRequest& request, const mms::ReadResponse& response)
{
	const mms::ListOfVariable& vars = RequireList(request.specification, "request");
	if(response.results.size() != vars.size())
		throw DecodeError("TASE.2: result count does not match the request");
	AssociationInfo info;
	for(size_t i = 0; i < vars.size(); ++i)
	{
		const std::string& item = vars[i].name.itemId;
		const bool known = item == Name::BilateralTableId || item == Name::Tase2Version || item == Name::SupportedFeatures;
		if(!known)
			continue;
		const AccessResult& r = response.results[i];
		if(!r.success)
		{
			info.failures[item] = r.error;
			continue;
		}
		if(item == Name::BilateralTableId)
			info.bilateralTableId = VisibleValue(r.data, "Bilateral_Table_ID");
		else if(item == Name::Tase2Version)
			info.version = ParseVersion(r.data);
		else
			info.features = ParseSupportedFeatures(r.data);
	}
	return info;
}

bool IsAssociationInfoRequest(const mms::ReadRequest& request)
{
	const auto* list = std::get_if<mms::ListOfVariable>(&request.specification);
	if(list == nullptr || list->empty())
		return false;
	for(const auto& v : *list)
	{
		const std::string& item = v.name.itemId;
		if(item != Name::BilateralTableId && item != Name::Tase2Version && item != Name::SupportedFeatures)
			return false;
	}
	return true;
}

mms::ReadResponse BuildAssociationInfoResponse(const mms::ReadRequest& request, const LocalAssociation& local)
{
	const mms::ListOfVariable& vars = RequireList(request.specification, "request");
	mms::ReadResponse resp;
	for(const auto& v : vars)
	{
		const ObjectName& n = v.name;
		if(n.itemId == Name::BilateralTableId && n.scope == mms::ObjectScope::DomainSpecific)
		{
			const auto it = local.bilateralTableIds.find(n.domainId);
			resp.results.push_back(it == local.bilateralTableIds.end() ? AccessResult::Failure(mms::DataAccessError::ObjectNonExistent)
				: AccessResult::Success(Data::VisibleString(it->second)));
		}
		else if(n.itemId == Name::Tase2Version && n.scope == mms::ObjectScope::VmdSpecific)
			resp.results.push_back(AccessResult::Success(ToData(local.version)));
		else if(n.itemId == Name::SupportedFeatures && n.scope == mms::ObjectScope::VmdSpecific)
			resp.results.push_back(AccessResult::Success(ToData(local.features)));
		else
			resp.results.push_back(AccessResult::Failure(mms::DataAccessError::ObjectNonExistent));
	}
	return resp;
}

// ---------------------------------------------------------------------------------------------
// flags and points
// ---------------------------------------------------------------------------------------------

uint8_t PackFlags(const DataFlags& f)
{
	return static_cast<uint8_t>((static_cast<unsigned>(f.state) << 6) | (static_cast<unsigned>(f.validity) << 4) |
	                            (static_cast<unsigned>(f.source) << 2) | (f.abnormal ? 0x02u : 0u) | (f.timeStampInvalid ? 0x01u : 0u));
}

DataFlags UnpackFlags(uint8_t b)
{
	DataFlags f;
	f.state = static_cast<StateValue>((b >> 6) & 3);
	f.validity = static_cast<Validity>((b >> 4) & 3);
	f.source = static_cast<CurrentSource>((b >> 2) & 3);
	f.abnormal = (b & 0x02) != 0;
	f.timeStampInvalid = (b & 0x01) != 0;
	return f;
}

DataFlags PointValue::Flags() const
{
	return UnpackFlags(flags);
}

bool IsRealType(PointType t)
{
	return t == PointType::Real || t == PointType::RealQ || t == PointType::RealQTimeTag || t == PointType::RealExtended;
}
bool IsStateType(PointType t)
{
	return t == PointType::State || t == PointType::StateQ || t == PointType::StateQTimeTag || t == PointType::StateExtended;
}
bool IsDiscreteType(PointType t)
{
	return t == PointType::Discrete || t == PointType::DiscreteQ || t == PointType::DiscreteQTimeTag || t == PointType::DiscreteExtended;
}

namespace
{
bool HasTimeTag(PointType t)
{
	return t == PointType::RealQTimeTag || t == PointType::StateQTimeTag || t == PointType::DiscreteQTimeTag;
}
bool IsExtended(PointType t)
{
	return t == PointType::RealExtended || t == PointType::StateExtended || t == PointType::DiscreteExtended;
}
//the value member of a point (nothing for State types, which carry their value in the flags)
Data ValueData(const PointValue& p)
{
	return IsRealType(p.type) ? Data::Float32(static_cast<float>(p.real)) : Data::Integer(p.discrete);
}
int64_t Required(const std::optional<int64_t>& v, const char* what)
{
	if(!v)
		throw std::invalid_argument(std::string("TASE.2: point value is missing its ") + what);
	return *v;
}
}

Data ToData(const PointValue& p)
{
	switch(p.type)
	{
		case PointType::Real:
			return Data::Float32(static_cast<float>(p.real));
		case PointType::Discrete:
			return Data::Integer(p.discrete);
		case PointType::State:
			return StateBits(p.state);
		case PointType::StateQ:
			return FlagsBits(p.flags);
		default:
			break;
	}
	std::vector<Data> members;
	if(HasTimeTag(p.type))
		members.push_back(Data::Integer(Required(p.timeStamp, "time stamp")));
	else if(IsExtended(p.type))
		members.push_back(TimeStampExtended(Required(p.timeStamp, "time stamp"), Required(p.milliseconds, "milliseconds")));
	if(!IsStateType(p.type))
		members.push_back(ValueData(p));
	members.push_back(FlagsBits(p.flags));
	if(IsExtended(p.type))
		members.push_back(Data::Integer(Required(p.cov, "COV")));
	return Data::Structure(std::move(members));
}

PointValue ParsePoint(const Data& data, PointType type)
{
	PointValue p;
	p.type = type;
	switch(type)
	{
		case PointType::Real:
			RequireType(data, DataType::FloatingPoint, "Data_Real");
			p.real = data.AsFloat();
			return p;
		case PointType::Discrete:
			p.discrete = IntValue(data, "Data_Discrete");
			return p;
		case PointType::State:
			p.state = ParseStateBits(data);
			p.flags = static_cast<uint8_t>(static_cast<uint8_t>(p.state) << 6);
			return p;
		case PointType::StateQ:
			p.flags = ParseFlagsBits(data, "Data_StateQ");
			p.state = UnpackFlags(p.flags).state;
			return p;
		default:
			break;
	}
	RequireType(data, DataType::Structure, "point value");
	size_t expected = 1; //the flags
	if(HasTimeTag(type) || IsExtended(type))
		++expected;
	if(!IsStateType(type))
		++expected;
	if(IsExtended(type))
		++expected;
	if(data.elements.size() != expected)
		throw DecodeError("TASE.2: point value has the wrong number of members");
	size_t i = 0;
	if(HasTimeTag(type))
		p.timeStamp = IntValue(data.elements[i++], "time stamp");
	else if(IsExtended(type))
		ParseTimeStampExtended(data.elements[i++], p);
	if(IsRealType(type))
	{
		RequireType(data.elements[i], DataType::FloatingPoint, "value");
		p.real = data.elements[i++].AsFloat();
	}
	else if(IsDiscreteType(type))
		p.discrete = IntValue(data.elements[i++], "value");
	p.flags = ParseFlagsBits(data.elements[i++], "flags");
	if(IsStateType(type))
		p.state = UnpackFlags(p.flags).state;
	if(IsExtended(type))
		p.cov = IntValue(data.elements[i++], "COV");
	return p;
}

namespace
{
//the shape of a data value or type, ignoring widths and names: F float, I integer, B8 8 bit string, B2 2 bit string
std::string ShapeOf(const Data& d)
{
	switch(d.type)
	{
		case DataType::FloatingPoint: return "F";
		case DataType::Integer:
		case DataType::Unsigned: return "I";
		case DataType::BitString:
			if(d.content.size() == 2 && d.content[0] == 0)
				return "B8";
			if(d.content.size() == 2 && d.content[0] == 6)
				return "B2";
			return "?";
		case DataType::Structure:
		{
			std::string s = "(";
			for(const auto& e : d.elements)
				s += ShapeOf(e) + ",";
			return s + ")";
		}
		default: return "?";
	}
}

std::string ShapeOf(const mms::TypeDescription& t)
{
	using Kind = mms::TypeDescription::Kind;
	switch(t.kind)
	{
		case Kind::FloatingPoint: return "F";
		case Kind::Integer:
		case Kind::Unsigned: return "I";
		case Kind::BitString: return t.size == 8 ? "B8" : (t.size == 2 ? "B2" : "?");
		case Kind::Structure:
		{
			std::string s = "(";
			for(const auto& e : t.subtypes)
				s += ShapeOf(e) + ",";
			return s + ")";
		}
		default: return "?";
	}
}

const std::vector<std::pair<PointType, std::string>>& Shapes()
{
	static const std::vector<std::pair<PointType, std::string>> shapes = {
		{PointType::Real, "F"}, {PointType::RealQ, "(F,B8,)"}, {PointType::RealQTimeTag, "(I,F,B8,)"},
		{PointType::RealExtended, "((I,I,),F,B8,I,)"}, {PointType::State, "B2"}, {PointType::StateQ, "B8"},
		{PointType::StateExtended, "((I,I,),B8,I,)"}, {PointType::Discrete, "I"},
		{PointType::DiscreteQ, "(I,B8,)"}, //also StateQTimeTag: ambiguous, see InferPointType
		{PointType::DiscreteQTimeTag, "(I,I,B8,)"}, {PointType::DiscreteExtended, "((I,I,),I,B8,I,)"}
	};
	return shapes;
}

template<class X>
std::optional<PointType> Classify(const X& x)
{
	const std::string shape = ShapeOf(x);
	for(const auto& [type, s] : Shapes())
		if(s == shape)
			return type;
	return std::nullopt;
}
}

std::optional<PointType> InferPointType(const Data& data)
{
	return Classify(data);
}

mms::TypeDescription TypeDescriptionOf(PointType type)
{
	using Kind = mms::TypeDescription::Kind;
	auto scalar = [](Kind k, int64_t size)
			  {
				  mms::TypeDescription t;
				  t.kind = k;
				  t.size = size;
				  return t;
			  };
	mms::TypeDescription real;
	real.kind = Kind::FloatingPoint;
	real.formatWidth = 32;
	real.exponentWidth = 8;
	const mms::TypeDescription flags = scalar(Kind::BitString, 8);
	const mms::TypeDescription integer = scalar(Kind::Integer, 32);
	const mms::TypeDescription cov = scalar(Kind::Integer, 16);
	auto structure = [](std::vector<mms::TypeDescription> parts)
			     {
				     mms::TypeDescription t;
				     t.kind = Kind::Structure;
				     t.componentNames.assign(parts.size(), std::nullopt);
				     t.subtypes = std::move(parts);
				     return t;
			     };
	const mms::TypeDescription ext = structure({integer, integer});
	switch(type)
	{
		case PointType::Real: return real;
		case PointType::RealQ: return structure({real, flags});
		case PointType::RealQTimeTag: return structure({integer, real, flags});
		case PointType::RealExtended: return structure({ext, real, flags, cov});
		case PointType::State: return scalar(Kind::BitString, 2);
		case PointType::StateQ: return flags;
		case PointType::StateQTimeTag: return structure({integer, flags});
		case PointType::StateExtended: return structure({ext, flags, cov});
		case PointType::Discrete: return integer;
		case PointType::DiscreteQ: return structure({integer, flags});
		case PointType::DiscreteQTimeTag: return structure({integer, integer, flags});
		case PointType::DiscreteExtended: return structure({ext, integer, flags, cov});
	}
	throw std::invalid_argument("TASE.2: bad point type");
}

std::optional<PointType> ClassifyTypeDescription(const mms::TypeDescription& type)
{
	return Classify(type);
}

mms::TypeDescription BilateralTableIdTypeDescription()
{
	mms::TypeDescription t;
	t.kind = mms::TypeDescription::Kind::VisibleString;
	t.size = -static_cast<int64_t>(BilateralTableIdWidth);
	return t;
}

// ---------------------------------------------------------------------------------------------
// Block 1 services
// ---------------------------------------------------------------------------------------------

mms::ReadRequest BuildGetDataValue(const std::vector<ObjectName>& names)
{
	mms::ReadRequest r;
	mms::ListOfVariable list;
	for(const auto& n : names)
		list.push_back(mms::VariableSpec{n, std::nullopt});
	r.specification = std::move(list);
	return r;
}

namespace
{
mms::GetNameListRequest NameListRequest(int64_t objectClass, const std::optional<std::string>& iccDomain, const std::optional<std::string>& continueAfter)
{
	mms::GetNameListRequest r;
	r.objectClass = objectClass;
	if(iccDomain)
	{
		r.scope = mms::ObjectScope::DomainSpecific;
		r.domainId = *iccDomain;
	}
	else
		r.scope = mms::ObjectScope::VmdSpecific;
	r.continueAfter = continueAfter;
	return r;
}
}

mms::GetNameListRequest BuildGetDataValueNames(const std::optional<std::string>& iccDomain, const std::optional<std::string>& continueAfter)
{
	return NameListRequest(mms::ObjectClass::NamedVariable, iccDomain, continueAfter);
}

mms::GetVariableAccessAttributesRequest BuildGetDataValueType(const ObjectName& name)
{
	return mms::GetVariableAccessAttributesRequest{name};
}

std::optional<mms::GetNameListRequest> NextNamePage(const mms::GetNameListRequest& request, const mms::GetNameListResponse& response)
{
	if(response.identifiers.empty() || !response.MoreFollows())
		return std::nullopt;
	mms::GetNameListRequest next = request;
	next.continueAfter = response.identifiers.back();
	return next;
}

std::optional<mms::GetNameListResponse> BuildNamePage(const std::vector<std::string>& all, const mms::GetNameListRequest& request, size_t maxPerPage)
{
	size_t start = 0;
	if(request.continueAfter)
	{
		const auto it = std::find(all.begin(), all.end(), *request.continueAfter);
		if(it == all.end())
			return std::nullopt;
		start = static_cast<size_t>(it - all.begin()) + 1;
	}
	const size_t remaining = all.size() - start;
	const size_t count = (maxPerPage == 0 || maxPerPage > remaining) ? remaining : maxPerPage;
	mms::GetNameListResponse resp;
	resp.identifiers.assign(all.begin() + static_cast<std::ptrdiff_t>(start), all.begin() + static_cast<std::ptrdiff_t>(start + count));
	if(start + count >= all.size())
		resp.moreFollows = false;
	return resp;
}

mms::DefineNamedVariableListRequest BuildCreateDataSet(const DataSetDefinition& def)
{
	mms::DefineNamedVariableListRequest r;
	r.name = def.name;
	for(const auto& m : def.members)
		r.variables.push_back(mms::VariableSpec{m, std::nullopt});
	return r;
}

mms::DeleteNamedVariableListRequest BuildDeleteDataSet(const ObjectName& name)
{
	mms::DeleteNamedVariableListRequest r;
	r.listOfVariableListName = std::vector<ObjectName>{name};
	return r;
}

mms::GetNameListRequest BuildGetDataSetNames(const std::optional<std::string>& iccDomain, const std::optional<std::string>& continueAfter)
{
	return NameListRequest(mms::ObjectClass::NamedVariableList, iccDomain, continueAfter);
}

mms::GetNamedVariableListAttributesRequest BuildGetDataSetElementNames(const ObjectName& name)
{
	return mms::GetNamedVariableListAttributesRequest{name};
}

DataSetDefinition ParseDataSetElementNames(const ObjectName& name, const mms::GetNamedVariableListAttributesResponse& response)
{
	DataSetDefinition def;
	def.name = name;
	for(const auto& v : response.variables)
	{
		if(v.alternateAccess)
			throw DecodeError("TASE.2: alternate access in a data set is not supported");
		def.members.push_back(v.name);
	}
	return def;
}

mms::GetNamedVariableListAttributesResponse BuildDataSetElementNamesResponse(const DataSetDefinition& def, bool deletable)
{
	mms::GetNamedVariableListAttributesResponse r;
	r.mmsDeletable = deletable;
	for(const auto& m : def.members)
		r.variables.push_back(mms::VariableSpec{m, std::nullopt});
	return r;
}

mms::ReadRequest BuildGetDataSetElementValues(const ObjectName& name)
{
	mms::ReadRequest r;
	r.specification = name;
	return r;
}

std::vector<AccessResult> ParseDataSetValues(const DataSetDefinition& def, const mms::ReadResponse& response)
{
	if(response.results.size() != def.members.size())
		throw DecodeError("TASE.2: data set value count does not match the definition");
	return response.results;
}

mms::DeleteNamedVariableListResponse BuildDeleteResponse(uint32_t matched, uint32_t deleted)
{
	return mms::DeleteNamedVariableListResponse{matched, deleted};
}

// ---------------------------------------------------------------------------------------------
// Block 2
// ---------------------------------------------------------------------------------------------

Data ToData(const DsConditions& c)
{
	if(c.bitCount > 8)
		throw std::invalid_argument("TASE.2: DSConditions has at most 8 bits");
	mms::BitString bs(c.bitCount);
	for(size_t i = 0; i < c.bitCount; ++i)
		if(c.mask & (1u << i))
			bs.Set(i, true);
	if(c.bitCount < 8 && (c.mask >> c.bitCount) != 0)
		throw std::invalid_argument("TASE.2: DSConditions bit beyond its length");
	return Data::Bits(bs);
}

DsConditions ParseDsConditions(const Data& data)
{
	RequireType(data, DataType::BitString, "DSConditions");
	const mms::BitString bs = data.AsBits();
	if(bs.BitCount() > 8)
		throw DecodeError("TASE.2: DSConditions longer than 8 bits");
	DsConditions c;
	c.bitCount = bs.BitCount();
	for(size_t i = 0; i < c.bitCount; ++i)
		if(bs.Get(i))
			c.mask = static_cast<uint8_t>(c.mask | (1u << i));
	//padding bits must be zero for the value to rebuild identically
	if(!bs.data.empty() && bs.unusedBits != 0 && (bs.data.back() & ((1u << bs.unusedBits) - 1)) != 0)
		throw DecodeError("TASE.2: DSConditions padding bits set");
	return c;
}

Data ToData(const DsTransferSet& t)
{
	std::vector<Data> m;
	m.push_back(ToData(t.dataSetName));
	m.push_back(Data::Integer(t.startTime));
	m.push_back(Data::Integer(t.interval));
	m.push_back(Data::Integer(t.tle));
	m.push_back(Data::Integer(t.bufferTime));
	m.push_back(Data::Integer(t.integrityCheck));
	m.push_back(ToData(t.conditionsRequested));
	m.push_back(Data::Boolean(t.blockData));
	m.push_back(Data::Boolean(t.critical));
	m.push_back(Data::Boolean(t.rbe));
	if(t.allChangesMember)
		m.push_back(Data::Boolean(t.allChangesReported));
	m.push_back(Data::Boolean(t.status));
	m.push_back(Data::Integer(t.eventCodeRequested));
	return Data::Structure(std::move(m));
}

DsTransferSet ParseDsTransferSet(const Data& data)
{
	RequireType(data, DataType::Structure, "DSTransferSet");
	const auto& m = data.elements;
	if(m.size() != 12 && m.size() != 13)
		throw DecodeError("TASE.2: DSTransferSet must have 12 or 13 members");
	DsTransferSet t;
	t.dataSetName = ParseObjectRef(m[0]);
	t.startTime = IntValue(m[1], "StartTime");
	t.interval = IntValue(m[2], "Interval");
	t.tle = IntValue(m[3], "TLE");
	t.bufferTime = IntValue(m[4], "BufferTime");
	t.integrityCheck = IntValue(m[5], "IntegrityCheck");
	t.conditionsRequested = ParseDsConditions(m[6]);
	t.blockData = BoolValue(m[7], "BlockData");
	t.critical = BoolValue(m[8], "Critical");
	t.rbe = BoolValue(m[9], "RBE");
	size_t i = 10;
	t.allChangesMember = m.size() == 13;
	if(t.allChangesMember)
		t.allChangesReported = BoolValue(m[i++], "AllChangesReported");
	t.status = BoolValue(m[i++], "Status");
	t.eventCodeRequested = IntValue(m[i], "EventCodeRequested");
	return t;
}

mms::WriteRequest BuildStartTransfer(const ObjectName& transferSet, const DsTransferSet& parameters)
{
	mms::WriteRequest w;
	w.specification = ListOf({transferSet});
	w.data.push_back(ToData(parameters));
	return w;
}

mms::WriteRequest BuildStopTransfer(const ObjectName& transferSet, DsTransferSet parameters)
{
	parameters.status = false;
	return BuildStartTransfer(transferSet, parameters);
}

TransferSetWrite ParseTransferSetWrite(const mms::WriteRequest& request)
{
	const mms::ListOfVariable& vars = RequireList(request.specification, "write");
	if(vars.size() != 1 || vars[0].alternateAccess || request.data.size() != 1)
		throw DecodeError("TASE.2: a transfer set write has one variable and one value");
	TransferSetWrite w;
	w.transferSet = vars[0].name;
	w.parameters = ParseDsTransferSet(request.data[0]);
	return w;
}

mms::WriteResponse BuildWriteSuccess(size_t count)
{
	mms::WriteResponse r;
	r.results.assign(count, mms::WriteResult{true, 0});
	return r;
}

mms::ReadRequest BuildGetNextDsTransferSet(const std::string& iccDomain)
{
	mms::ReadRequest r;
	r.specification = ListOf({IccName(iccDomain, Name::NextDsTransferSet)});
	return r;
}

ObjectRef ParseNextDsTransferSet(const mms::ReadResponse& response)
{
	if(response.results.size() != 1 || !response.results[0].success)
		throw DecodeError("TASE.2: Next_DSTransfer_Set could not be read");
	return ParseObjectRef(response.results[0].data);
}

mms::ReadResponse BuildNextDsTransferSetResponse(const std::optional<ObjectRef>& next)
{
	mms::ReadResponse r;
	r.results.push_back(next ? AccessResult::Success(ToData(*next)) : AccessResult::Failure(mms::DataAccessError::ObjectNonExistent));
	return r;
}

Data TransferSetTimeStampData(int64_t seconds)
{
	return Data::Integer(seconds);
}

Data EventCodeData(int64_t code)
{
	return Data::Integer(code);
}

int64_t ParseTransferSetTimeStamp(const Data& d)
{
	return IntValue(d, "Transfer_Set_Time_Stamp");
}

int64_t ParseEventCode(const Data& d)
{
	return IntValue(d, "Event_Code_Detected");
}

namespace
{
const AccessResult& SpecialResult(const AccessResult& r)
{
	if(!r.success)
		throw DecodeError("TASE.2: a report's special object failed");
	return r;
}
}

TransferReport ParseTransferReport(const mms::InformationReport& report, const std::vector<ObjectName>* members)
{
	TransferReport out;
	out.specification = report.specification;
	out.layout.clear();
	const auto& results = report.results;

	//a report that lists its variables inline carries its own definition
	std::vector<ObjectName> inlineMembers;
	if(const auto* list = std::get_if<mms::ListOfVariable>(&report.specification))
	{
		for(const auto& v : *list)
		{
			if(v.alternateAccess)
				throw DecodeError("TASE.2: alternate access in a transfer report is not supported");
			inlineMembers.push_back(v.name);
		}
		members = &inlineMembers;
	}
	if(members != nullptr && members->size() != results.size())
		throw DecodeError("TASE.2: report size does not match the data set definition");
	if(results.empty())
		throw DecodeError("TASE.2: empty transfer report");

	size_t n = 0;
	std::vector<Special> seen;
	auto take = [&](Special s)
			{
				const Data& d = SpecialResult(results[n]).data;
				switch(s)
				{
					case Special::TransferSetName: out.transferSet = ParseObjectRef(d); break;
					case Special::TimeStamp: out.timeStamp = ParseTransferSetTimeStamp(d); break;
					case Special::ConditionsDetected: out.conditionsDetected = ParseDsConditions(d); break;
					case Special::EventCode: out.eventCode = ParseEventCode(d); break;
				}
				out.layout.push_back(s);
				++n;
			};

	if(members == nullptr)
		take(Special::TransferSetName);
	else
	{
		while(n < members->size())
		{
			const auto s = SpecialOf((*members)[n].itemId);
			if(!s || std::find(out.layout.begin(), out.layout.end(), *s) != out.layout.end())
				break;
			take(*s);
		}
		if(out.layout.empty() || out.layout.front() != Special::TransferSetName)
			throw DecodeError("TASE.2: the data set does not start with Transfer_Set_Name");
	}
	out.values.assign(results.begin() + static_cast<std::ptrdiff_t>(n), results.end());
	return out;
}

mms::InformationReport BuildTransferReport(const TransferReport& report)
{
	if(report.layout.empty() || report.layout.front() != Special::TransferSetName)
		throw std::invalid_argument("TASE.2: a transfer report starts with Transfer_Set_Name");
	mms::InformationReport r;
	r.specification = report.specification;
	std::set<Special> used;
	for(Special s : report.layout)
	{
		if(!used.insert(s).second)
			throw std::invalid_argument("TASE.2: duplicate special object in a transfer report");
		switch(s)
		{
			case Special::TransferSetName:
				r.results.push_back(AccessResult::Success(ToData(report.transferSet)));
				break;
			case Special::TimeStamp:
				if(!report.timeStamp)
					throw std::invalid_argument("TASE.2: report layout has a time stamp but none is set");
				r.results.push_back(AccessResult::Success(TransferSetTimeStampData(*report.timeStamp)));
				break;
			case Special::ConditionsDetected:
				if(!report.conditionsDetected)
					throw std::invalid_argument("TASE.2: report layout has conditions but none are set");
				r.results.push_back(AccessResult::Success(ToData(*report.conditionsDetected)));
				break;
			case Special::EventCode:
				if(!report.eventCode)
					throw std::invalid_argument("TASE.2: report layout has an event code but none is set");
				r.results.push_back(AccessResult::Success(EventCodeData(*report.eventCode)));
				break;
		}
	}
	r.results.insert(r.results.end(), report.values.begin(), report.values.end());
	return r;
}

namespace
{
mms::ObjectName AckVariable(const ObjectRef& transferSet, const char* item)
{
	return transferSet.scope == Scope::Icc ? ObjectName::Domain(transferSet.domain, item) : ObjectName::Vmd(item);
}
}

mms::InformationReport BuildTransferReportAck(const ObjectRef& transferSet, const std::optional<int64_t>& timeStamp)
{
	mms::InformationReport r;
	mms::ListOfVariable list;
	list.push_back(mms::VariableSpec{AckVariable(transferSet, Name::TransferReportAck), std::nullopt});
	r.results.push_back(AccessResult::Success(ToData(transferSet)));
	if(timeStamp)
	{
		list.push_back(mms::VariableSpec{AckVariable(transferSet, Name::TransferSetTimeStamp), std::nullopt});
		r.results.push_back(AccessResult::Success(TransferSetTimeStampData(*timeStamp)));
	}
	r.specification = std::move(list);
	return r;
}

mms::InformationReport BuildTransferReportNack(const ObjectRef& transferSet)
{
	mms::InformationReport r;
	r.specification = mms::ListOfVariable{mms::VariableSpec{AckVariable(transferSet, Name::TransferReportNack), std::nullopt}
	};
	r.results.push_back(AccessResult::Success(ToData(transferSet)));
	return r;
}

std::optional<ReportAck> ParseReportAck(const mms::InformationReport& report)
{
	const auto* list = std::get_if<mms::ListOfVariable>(&report.specification);
	if(list == nullptr || list->size() != report.results.size())
		return std::nullopt; //(a report that names a data set, or whose results do not match its variables, is not one of ours)
	std::optional<size_t> ackAt;
	bool negative = false;
	std::optional<size_t> stampAt;
	for(size_t i = 0; i < list->size(); ++i)
	{
		const std::string& item = (*list)[i].name.itemId;
		if(!ackAt && item == Name::TransferReportAck)
			ackAt = i;
		else if(!ackAt && item == Name::TransferReportNack)
		{
			ackAt = i;
			negative = true;
		}
		else if(!stampAt && item == Name::TransferSetTimeStamp)
			stampAt = i;
	}
	if(!ackAt)
		return std::nullopt;
	if(!report.results[*ackAt].success)
		throw DecodeError("TASE.2: a transfer report acknowledgement whose object name could not be read");
	ReportAck out;
	out.negative = negative;
	out.transferSet = ParseObjectRef(report.results[*ackAt].data);
	if(stampAt && report.results[*stampAt].success)
	{
		try
		{
			out.timeStamp = ParseTransferSetTimeStamp(report.results[*stampAt].data);
		}
		catch(const DecodeError&)
		{
			//an unreadable time stamp is as good as none: the acknowledgement then matches the oldest outstanding report
		}
	}
	return out;
}

} // namespace tase2
} // namespace iccp
