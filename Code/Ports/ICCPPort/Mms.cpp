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
 * Mms.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 9506-2 manufacturing message specification, ICCP subset.
 */

#include "Mms.h"
#include <cstring>
#include <stdexcept>

namespace iccp
{
namespace mms
{

using namespace ber;

namespace
{
// ---------------------------------------------------------------------------------------------
// shared helpers
// ---------------------------------------------------------------------------------------------

struct Budget
{
	size_t nodes = 0;
	void Take()
	{
		if(++nodes > MaxDataNodes)
			throw DecodeError("MMS: too many data nodes");
	}
};

constexpr Tag SequenceTag{TagClass::Universal, true, static_cast<uint32_t>(UniversalTag::Sequence)};

Bytes MinimalInteger(int64_t v)
{
	uint8_t b[8];
	for(size_t i = 0; i < 8; ++i)
		b[i] = static_cast<uint8_t>(static_cast<uint64_t>(v) >> (56 - 8 * i));
	size_t start = 0;
	while(start < 7 && ((b[start] == 0x00 && (b[start + 1] & 0x80) == 0) || (b[start] == 0xFF && (b[start + 1] & 0x80) != 0)))
		++start;
	return Bytes(b + start, b + 8);
}

Bytes MinimalUnsigned(uint64_t v)
{
	uint8_t b[9];
	b[0] = 0;
	for(size_t i = 0; i < 8; ++i)
		b[i + 1] = static_cast<uint8_t>(v >> (56 - 8 * i));
	size_t start = 0;
	while(start < 8 && b[start] == 0x00 && (b[start + 1] & 0x80) == 0)
		++start;
	return Bytes(b + start, b + 9);
}

//the complete encoding of an element (header included)
Bytes RawOf(const Element& e)
{
	const size_t header = e.totalLength - e.contentLength - (e.indefinite ? 2 : 0);
	const uint8_t* start = e.content - header;
	return Bytes(start, start + e.totalLength);
}

Element ParseWhole(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("MMS: null data");
	const Element e = ParseElement(data, length);
	if(e.totalLength != length)
		throw DecodeError("MMS: trailing octets after PDU");
	return e;
}

void RequireConstructed(const Element& e)
{
	if(!e.tag.constructed)
		throw DecodeError("MMS: constructed encoding expected");
}

void RequireContext(const Element& e)
{
	if(e.tag.cls != TagClass::ContextSpecific)
		throw DecodeError("MMS: context-specific tag expected");
}

void RequireEmpty(const Element& e)
{
	if(e.tag.constructed || e.contentLength != 0)
		throw DecodeError("MMS: NULL expected");
}

//The content of an EXPLICIT tag: exactly one inner element
Element Unwrap(const Element& wrapper)
{
	RequireConstructed(wrapper);
	Cursor c(wrapper);
	const Element inner = c.Next();
	if(!c.Empty())
		throw DecodeError("MMS: more than one element in an explicit tag");
	return inner;
}

void SkipRest(Cursor& c)
{
	while(!c.Empty())
		c.Next();
}

uint32_t ReadUnsigned32(const Element& e)
{
	const uint64_t v = DecodeUnsigned(e);
	if(v > 0xFFFFFFFFull)
		throw DecodeError("MMS: Unsigned32 out of range");
	return static_cast<uint32_t>(v);
}

int64_t ReadNonNegative(const Element& e)
{
	const int64_t v = DecodeInteger(e);
	if(v < 0)
		throw DecodeError("MMS: negative value");
	return v;
}

std::string ReadIdentifier(const Element& e)
{
	if(e.contentLength > MaxIdentifierLength)
		throw DecodeError("MMS: Identifier too long");
	return DecodeString(e); //throws for constructed
}

std::string ReadUniversalIdentifier(const Element& e)
{
	if(!e.tag.Is(UniversalTag::VisibleString))
		throw DecodeError("MMS: VisibleString expected");
	return ReadIdentifier(e);
}

void WriteBool(Writer& w, bool value, Tag tag)
{
	const uint8_t b = value ? 0x01 : 0x00;
	w.Primitive(tag, &b, 1);
}

void CheckIdentifier(const std::string& s)
{
	if(s.size() > MaxIdentifierLength)
		throw std::invalid_argument("MMS: Identifier too long");
}

void CheckNonNegative(int64_t v, const char* what)
{
	if(v < 0)
		throw std::invalid_argument(std::string("MMS: negative ") + what);
}

// ---------------------------------------------------------------------------------------------
// ObjectName and variable access
// ---------------------------------------------------------------------------------------------

ObjectName ReadObjectName(const Element& e)
{
	RequireContext(e);
	ObjectName n;
	switch(e.tag.number)
	{
		case 0:
			n.scope = ObjectScope::VmdSpecific;
			n.itemId = ReadIdentifier(e);
			break;
		case 1:
		{
			RequireConstructed(e);
			n.scope = ObjectScope::DomainSpecific;
			Cursor c(e);
			n.domainId = ReadUniversalIdentifier(c.Next());
			n.itemId = ReadUniversalIdentifier(c.Next());
			if(!c.Empty())
				throw DecodeError("MMS: extra elements in a domain-specific ObjectName");
			break;
		}
		case 2:
			n.scope = ObjectScope::AaSpecific;
			n.itemId = ReadIdentifier(e);
			break;
		default:
			throw DecodeError("MMS: unknown ObjectName alternative");
	}
	return n;
}

void WriteObjectName(Writer& w, const ObjectName& n)
{
	CheckIdentifier(n.itemId);
	switch(n.scope)
	{
		case ObjectScope::VmdSpecific:
			w.String(n.itemId, Tag::Context(0));
			break;
		case ObjectScope::DomainSpecific:
			CheckIdentifier(n.domainId);
			w.Begin(Tag::Context(1, true));
			w.String(n.domainId);
			w.String(n.itemId);
			w.End();
			break;
		case ObjectScope::AaSpecific:
			w.String(n.itemId, Tag::Context(2));
			break;
		default:
			throw std::invalid_argument("MMS: bad ObjectName scope");
	}
}

VariableSpec ReadVariableSpec(const Element& seq)
{
	if(!seq.tag.Is(UniversalTag::Sequence))
		throw DecodeError("MMS: SEQUENCE expected in listOfVariable");
	RequireConstructed(seq);
	Cursor c(seq);
	const Element spec = c.Next();
	if(!spec.tag.Is(TagClass::ContextSpecific, 0))
		throw DecodeError("MMS: only name variable specifications are supported");
	VariableSpec v;
	v.name = ReadObjectName(Unwrap(spec));
	if(c.PeekIs(TagClass::ContextSpecific, 5))
		v.alternateAccess = RawOf(c.Next());
	SkipRest(c);
	return v;
}

ListOfVariable ReadListOfVariable(const Element& list)
{
	RequireConstructed(list);
	ListOfVariable out;
	Cursor c(list);
	while(!c.Empty())
	{
		if(out.size() >= MaxListEntries)
			throw DecodeError("MMS: too many variables");
		out.push_back(ReadVariableSpec(c.Next()));
	}
	return out;
}

void WriteListOfVariable(Writer& w, const ListOfVariable& list, Tag tag)
{
	if(list.size() > MaxListEntries)
		throw std::invalid_argument("MMS: too many variables");
	w.Begin(tag);
	for(const auto& v : list)
	{
		w.Begin(SequenceTag);
		w.Begin(Tag::Context(0, true));
		WriteObjectName(w, v.name);
		w.End();
		if(v.alternateAccess)
		{
			const Bytes& a = *v.alternateAccess;
			bool ok = false;
			try
			{
				const Element e = ParseElement(a.data(), a.size());
				ok = e.totalLength == a.size() && e.tag.Is(TagClass::ContextSpecific, 5);
			}
			catch(const DecodeError&)
			{}
			if(!ok)
				throw std::invalid_argument("MMS: alternateAccess is not one [5] element");
			w.Raw(a);
		}
		w.End();
	}
	w.End();
}

//the element is the VariableAccessSpecification CHOICE alternative
VariableAccessSpecification ReadSpecification(const Element& e)
{
	RequireContext(e);
	if(e.tag.number == 0)
		return ReadListOfVariable(e);
	if(e.tag.number == 1)
		return ReadObjectName(Unwrap(e));
	throw DecodeError("MMS: unknown VariableAccessSpecification alternative");
}

void WriteSpecification(Writer& w, const VariableAccessSpecification& s)
{
	if(const auto* list = std::get_if<ListOfVariable>(&s))
		WriteListOfVariable(w, *list, Tag::Context(0, true));
	else
	{
		w.Begin(Tag::Context(1, true));
		WriteObjectName(w, std::get<ObjectName>(s));
		w.End();
	}
}

// ---------------------------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------------------------

//Validates scalar content against its type
void ValidateScalar(DataType type, const Element& e)
{
	switch(type)
	{
		case DataType::Boolean:
			DecodeBoolean(e);
			break;
		case DataType::BitString:
		case DataType::BooleanArray:
			DecodeBitString(e);
			break;
		case DataType::Integer:
		case DataType::Bcd:
			DecodeInteger(e);
			break;
		case DataType::Unsigned:
			DecodeUnsigned(e);
			break;
		case DataType::FloatingPoint:
			if(e.tag.constructed || e.contentLength < 2)
				throw DecodeError("MMS: bad FloatingPoint");
			break;
		case DataType::OctetString:
		case DataType::VisibleString:
		case DataType::GeneralizedTime:
		case DataType::MmsString:
			if(e.tag.constructed)
				throw DecodeError("MMS: constructed string not supported");
			break;
		case DataType::BinaryTime:
			if(e.tag.constructed || (e.contentLength != 4 && e.contentLength != 6))
				throw DecodeError("MMS: bad BinaryTime");
			break;
		case DataType::ObjectId:
			DecodeObjectIdentifier(e);
			break;
		case DataType::UtcTime:
			if(e.tag.constructed || e.contentLength != 8)
				throw DecodeError("MMS: bad UtcTime");
			break;
		default:
			throw DecodeError("MMS: not a scalar");
	}
}

bool IsKnownDataTag(uint32_t n)
{
	return (n >= 1 && n <= 7) || (n >= 9 && n <= 17);
}

Data ReadData(const Element& e, unsigned depth, Budget& budget)
{
	if(depth > MaxNesting)
		throw DecodeError("MMS: Data nested too deeply");
	budget.Take();
	RequireContext(e);
	if(!IsKnownDataTag(e.tag.number))
		throw DecodeError("MMS: unknown Data alternative");
	Data d;
	d.type = static_cast<DataType>(e.tag.number);
	if(d.type == DataType::Array || d.type == DataType::Structure)
	{
		RequireConstructed(e);
		Cursor c(e);
		while(!c.Empty())
		{
			if(d.elements.size() >= MaxListEntries)
				throw DecodeError("MMS: too many data elements");
			d.elements.push_back(ReadData(c.Next(), depth + 1, budget));
		}
		return d;
	}
	ValidateScalar(d.type, e);
	d.content.assign(e.content, e.content + e.contentLength);
	return d;
}

void WriteData(Writer& w, const Data& d, unsigned depth)
{
	if(depth > MaxNesting)
		throw std::invalid_argument("MMS: Data nested too deeply");
	const uint32_t number = static_cast<uint32_t>(d.type);
	if(!IsKnownDataTag(number))
		throw std::invalid_argument("MMS: bad Data type");
	if(d.type == DataType::Array || d.type == DataType::Structure)
	{
		if(!d.content.empty())
			throw std::invalid_argument("MMS: constructed Data with content");
		if(d.elements.size() > MaxListEntries)
			throw std::invalid_argument("MMS: too many data elements");
		w.Begin(Tag::Context(number, true));
		for(const auto& m : d.elements)
			WriteData(w, m, depth + 1);
		w.End();
		return;
	}
	if(!d.elements.empty())
		throw std::invalid_argument("MMS: scalar Data with elements");
	Element probe;
	probe.tag = Tag::Context(number);
	probe.content = d.content.data();
	probe.contentLength = d.content.size();
	try
	{
		ValidateScalar(d.type, probe);
	}
	catch(const DecodeError& ex)
	{
		throw std::invalid_argument(ex.what());
	}
	w.Primitive(Tag::Context(number), d.content.data(), d.content.size());
}

AccessResult ReadAccessResult(const Element& e, Budget& budget)
{
	RequireContext(e);
	if(e.tag.number == 0)
		return AccessResult::Failure(ReadNonNegative(e));
	return AccessResult::Success(ReadData(e, 0, budget));
}

std::vector<AccessResult> ReadAccessResults(const Element& list, Budget& budget)
{
	RequireConstructed(list);
	std::vector<AccessResult> out;
	Cursor c(list);
	while(!c.Empty())
	{
		if(out.size() >= MaxListEntries)
			throw DecodeError("MMS: too many results");
		out.push_back(ReadAccessResult(c.Next(), budget));
	}
	return out;
}

void WriteAccessResults(Writer& w, const std::vector<AccessResult>& results, Tag listTag)
{
	if(results.size() > MaxListEntries)
		throw std::invalid_argument("MMS: too many results");
	w.Begin(listTag);
	for(const auto& r : results)
	{
		if(r.success)
			WriteData(w, r.data, 0);
		else
		{
			CheckNonNegative(r.error, "DataAccessError");
			w.Integer(r.error, Tag::Context(0));
		}
	}
	w.End();
}

// ---------------------------------------------------------------------------------------------
// TypeDescription
// ---------------------------------------------------------------------------------------------

TypeDescription ReadTypeDescription(const Element& e, unsigned depth, Budget& budget)
{
	using Kind = TypeDescription::Kind;
	if(depth > MaxNesting)
		throw DecodeError("MMS: TypeDescription nested too deeply");
	budget.Take();
	RequireContext(e);
	TypeDescription t;
	switch(e.tag.number)
	{
		case 0:
			t.kind = Kind::TypeName;
			t.typeName = ReadObjectName(Unwrap(e));
			break;
		case 1:
		{
			RequireConstructed(e);
			t.kind = Kind::Array;
			Cursor c(e);
			if(c.PeekIs(TagClass::ContextSpecific, 0))
				t.packed = DecodeBoolean(c.Next());
			t.size = static_cast<int64_t>(ReadUnsigned32(c.Expect(TagClass::ContextSpecific, 1)));
			t.subtypes.push_back(ReadTypeDescription(Unwrap(c.Expect(TagClass::ContextSpecific, 2)), depth + 1, budget));
			SkipRest(c);
			break;
		}
		case 2:
		{
			RequireConstructed(e);
			t.kind = Kind::Structure;
			Cursor c(e);
			if(c.PeekIs(TagClass::ContextSpecific, 0))
				t.packed = DecodeBoolean(c.Next());
			const Element comps = c.Expect(TagClass::ContextSpecific, 1);
			RequireConstructed(comps);
			Cursor cc(comps);
			while(!cc.Empty())
			{
				if(t.subtypes.size() >= MaxListEntries)
					throw DecodeError("MMS: too many components");
				const Element seq = cc.Next();
				if(!seq.tag.Is(UniversalTag::Sequence))
					throw DecodeError("MMS: SEQUENCE expected for a component");
				RequireConstructed(seq);
				Cursor sc(seq);
				std::optional<std::string> name;
				if(sc.PeekIs(TagClass::ContextSpecific, 0))
					name = ReadIdentifier(sc.Next());
				t.subtypes.push_back(ReadTypeDescription(Unwrap(sc.Expect(TagClass::ContextSpecific, 1)), depth + 1, budget));
				t.componentNames.push_back(std::move(name));
				SkipRest(sc);
			}
			SkipRest(c);
			break;
		}
		case 3: RequireEmpty(e); t.kind = Kind::Boolean; break;
		case 4: t.kind = Kind::BitString; t.size = DecodeInteger(e); break;
		case 5: t.kind = Kind::Integer; t.size = DecodeInteger(e); break;
		case 6: t.kind = Kind::Unsigned; t.size = DecodeInteger(e); break;
		case 7:
		{
			RequireConstructed(e);
			t.kind = Kind::FloatingPoint;
			Cursor c(e);
			const int64_t format = ReadNonNegative(c.Expect(UniversalTag::Integer));
			const int64_t exponent = ReadNonNegative(c.Expect(UniversalTag::Integer));
			if(format > 255 || exponent > 255)
				throw DecodeError("MMS: FloatingPoint widths out of range");
			t.formatWidth = static_cast<uint8_t>(format);
			t.exponentWidth = static_cast<uint8_t>(exponent);
			SkipRest(c);
			break;
		}
		case 9: t.kind = Kind::OctetString; t.size = DecodeInteger(e); break;
		case 10: t.kind = Kind::VisibleString; t.size = DecodeInteger(e); break;
		case 11: RequireEmpty(e); t.kind = Kind::GeneralizedTime; break;
		case 12: t.kind = Kind::BinaryTime; t.timeOfDay = DecodeBoolean(e); break;
		case 13: t.kind = Kind::Bcd; t.size = DecodeInteger(e); break;
		case 15: RequireEmpty(e); t.kind = Kind::ObjectId; break;
		case 16: t.kind = Kind::MmsString; t.size = DecodeInteger(e); break;
		case 17: RequireEmpty(e); t.kind = Kind::UtcTime; break;
		default:
			throw DecodeError("MMS: unknown TypeDescription alternative");
	}
	return t;
}

void WriteTypeDescription(Writer& w, const TypeDescription& t, unsigned depth)
{
	using Kind = TypeDescription::Kind;
	if(depth > MaxNesting)
		throw std::invalid_argument("MMS: TypeDescription nested too deeply");
	const uint32_t number = static_cast<uint32_t>(t.kind);
	switch(t.kind)
	{
		case Kind::TypeName:
			if(!t.typeName)
				throw std::invalid_argument("MMS: TypeName without a name");
			w.Begin(Tag::Context(0, true));
			WriteObjectName(w, *t.typeName);
			w.End();
			break;
		case Kind::Array:
			if(t.subtypes.size() != 1)
				throw std::invalid_argument("MMS: array type needs one element type");
			CheckNonNegative(t.size, "numberOfElements");
			w.Begin(Tag::Context(1, true));
			if(t.packed)
				WriteBool(w, true, Tag::Context(0));
			w.Unsigned(static_cast<uint64_t>(t.size), Tag::Context(1));
			w.Begin(Tag::Context(2, true));
			WriteTypeDescription(w, t.subtypes[0], depth + 1);
			w.End();
			w.End();
			break;
		case Kind::Structure:
			if(t.subtypes.size() != t.componentNames.size() || t.subtypes.size() > MaxListEntries)
				throw std::invalid_argument("MMS: structure components inconsistent");
			w.Begin(Tag::Context(2, true));
			if(t.packed)
				WriteBool(w, true, Tag::Context(0));
			w.Begin(Tag::Context(1, true));
			for(size_t i = 0; i < t.subtypes.size(); ++i)
			{
				w.Begin(SequenceTag);
				if(t.componentNames[i])
				{
					CheckIdentifier(*t.componentNames[i]);
					w.String(*t.componentNames[i], Tag::Context(0));
				}
				w.Begin(Tag::Context(1, true));
				WriteTypeDescription(w, t.subtypes[i], depth + 1);
				w.End();
				w.End();
			}
			w.End();
			w.End();
			break;
		case Kind::Boolean:
		case Kind::GeneralizedTime:
		case Kind::ObjectId:
		case Kind::UtcTime:
			w.Null(Tag::Context(number));
			break;
		case Kind::BitString:
		case Kind::Integer:
		case Kind::Unsigned:
		case Kind::OctetString:
		case Kind::VisibleString:
		case Kind::Bcd:
		case Kind::MmsString:
			w.Integer(t.size, Tag::Context(number));
			break;
		case Kind::FloatingPoint:
			w.Begin(Tag::Context(7, true));
			w.Unsigned(t.formatWidth);
			w.Unsigned(t.exponentWidth);
			w.End();
			break;
		case Kind::BinaryTime:
			WriteBool(w, t.timeOfDay, Tag::Context(12));
			break;
		default:
			throw std::invalid_argument("MMS: bad TypeDescription kind");
	}
}

// ---------------------------------------------------------------------------------------------
// ServiceError
// ---------------------------------------------------------------------------------------------

//e is the element holding the ServiceError SEQUENCE content
ServiceError ReadServiceError(const Element& e)
{
	RequireConstructed(e);
	Cursor c(e);
	ServiceError s;
	const Element alt = Unwrap(c.Expect(TagClass::ContextSpecific, 0));
	RequireContext(alt);
	if(alt.tag.number > static_cast<uint32_t>(ErrorClass::Others))
		throw DecodeError("MMS: unknown errorClass");
	s.errorClass = alt.tag.number;
	s.errorCode = ReadNonNegative(alt);
	if(c.PeekIs(TagClass::ContextSpecific, 1))
		s.additionalCode = DecodeInteger(c.Next());
	if(c.PeekIs(TagClass::ContextSpecific, 2))
		s.additionalDescription = DecodeString(c.Next());
	if(c.PeekIs(TagClass::ContextSpecific, 3))
		s.serviceSpecificInformation = RawOf(c.Next());
	SkipRest(c);
	return s;
}

void WriteServiceError(Writer& w, const ServiceError& s, Tag tag)
{
	if(s.errorClass < ErrorClass::VmdState || s.errorClass > ErrorClass::Others)
		throw std::invalid_argument("MMS: bad errorClass");
	CheckNonNegative(s.errorCode, "errorCode");
	w.Begin(tag);
	w.Begin(Tag::Context(0, true));
	w.Integer(s.errorCode, Tag::Context(static_cast<uint32_t>(s.errorClass)));
	w.End();
	if(s.additionalCode)
		w.Integer(*s.additionalCode, Tag::Context(1));
	if(s.additionalDescription)
		w.String(*s.additionalDescription, Tag::Context(2));
	if(s.serviceSpecificInformation)
	{
		const Bytes& b = *s.serviceSpecificInformation;
		bool ok = false;
		try
		{
			const Element e = ParseElement(b.data(), b.size());
			ok = e.totalLength == b.size() && e.tag.Is(TagClass::ContextSpecific, 3);
		}
		catch(const DecodeError&)
		{}
		if(!ok)
			throw std::invalid_argument("MMS: serviceSpecificInformation is not one [3] element");
		w.Raw(b);
	}
	w.End();
}

// ---------------------------------------------------------------------------------------------
// Confirmed services: decoding
// ---------------------------------------------------------------------------------------------

ConfirmedServiceRequest ReadRequestService(const Element& svc, Budget& budget)
{
	RequireContext(svc);
	switch(svc.tag.number)
	{
		case 1:
		{
			RequireConstructed(svc);
			GetNameListRequest r;
			Cursor c(svc);
			const Element cls = Unwrap(c.Expect(TagClass::ContextSpecific, 0));
			if(!cls.tag.Is(TagClass::ContextSpecific, 0))
				throw DecodeError("MMS: extended object classes are not supported");
			r.objectClass = ReadNonNegative(cls);
			const Element scope = Unwrap(c.Expect(TagClass::ContextSpecific, 1));
			RequireContext(scope);
			switch(scope.tag.number)
			{
				case 0:
					RequireEmpty(scope);
					r.scope = ObjectScope::VmdSpecific;
					break;
				case 1:
					r.scope = ObjectScope::DomainSpecific;
					r.domainId = ReadIdentifier(scope);
					break;
				case 2:
					RequireEmpty(scope);
					r.scope = ObjectScope::AaSpecific;
					break;
				default:
					throw DecodeError("MMS: unknown objectScope");
			}
			if(c.PeekIs(TagClass::ContextSpecific, 2))
				r.continueAfter = ReadIdentifier(c.Next());
			SkipRest(c);
			return r;
		}
		case 2:
			RequireEmpty(svc);
			return IdentifyRequest{};
		case 4:
		{
			RequireConstructed(svc);
			ReadRequest r;
			Cursor c(svc);
			if(c.PeekIs(TagClass::ContextSpecific, 0))
				r.specificationWithResult = DecodeBoolean(c.Next());
			r.specification = ReadSpecification(Unwrap(c.Expect(TagClass::ContextSpecific, 1)));
			SkipRest(c);
			return r;
		}
		case 5:
		{
			RequireConstructed(svc);
			WriteRequest r;
			Cursor c(svc);
			r.specification = ReadSpecification(c.Next());
			const Element list = c.Expect(TagClass::ContextSpecific, 0);
			RequireConstructed(list);
			Cursor lc(list);
			while(!lc.Empty())
			{
				if(r.data.size() >= MaxListEntries)
					throw DecodeError("MMS: too many data elements");
				r.data.push_back(ReadData(lc.Next(), 0, budget));
			}
			SkipRest(c);
			return r;
		}
		case 6:
		{
			RequireConstructed(svc);
			GetVariableAccessAttributesRequest r;
			Cursor c(svc);
			const Element name = c.Next();
			if(!name.tag.Is(TagClass::ContextSpecific, 0))
				throw DecodeError("MMS: only name is supported for GetVariableAccessAttributes");
			r.name = ReadObjectName(Unwrap(name));
			SkipRest(c);
			return r;
		}
		case 11:
		{
			RequireConstructed(svc);
			DefineNamedVariableListRequest r;
			Cursor c(svc);
			r.name = ReadObjectName(c.Next());
			r.variables = ReadListOfVariable(c.Expect(TagClass::ContextSpecific, 0));
			SkipRest(c);
			return r;
		}
		case 12:
		{
			GetNamedVariableListAttributesRequest r;
			r.name = ReadObjectName(Unwrap(svc));
			return r;
		}
		case 13:
		{
			RequireConstructed(svc);
			DeleteNamedVariableListRequest r;
			Cursor c(svc);
			if(c.PeekIs(TagClass::ContextSpecific, 0))
			{
				const int64_t scope = ReadNonNegative(c.Next());
				if(scope > DeleteScope::Vmd)
					throw DecodeError("MMS: bad scopeOfDelete");
				r.scopeOfDelete = scope;
			}
			if(c.PeekIs(TagClass::ContextSpecific, 1))
			{
				const Element list = c.Next();
				RequireConstructed(list);
				std::vector<ObjectName> names;
				Cursor lc(list);
				while(!lc.Empty())
				{
					if(names.size() >= MaxListEntries)
						throw DecodeError("MMS: too many names");
					names.push_back(ReadObjectName(lc.Next()));
				}
				r.listOfVariableListName = std::move(names);
			}
			if(c.PeekIs(TagClass::ContextSpecific, 2))
				r.domainName = ReadIdentifier(c.Next());
			SkipRest(c);
			return r;
		}
		default:
			throw DecodeError("MMS: unsupported confirmed service");
	}
}

ConfirmedServiceResponse ReadResponseService(const Element& svc, Budget& budget)
{
	RequireContext(svc);
	switch(svc.tag.number)
	{
		case 1:
		{
			RequireConstructed(svc);
			GetNameListResponse r;
			Cursor c(svc);
			const Element list = c.Expect(TagClass::ContextSpecific, 0);
			RequireConstructed(list);
			Cursor lc(list);
			while(!lc.Empty())
			{
				if(r.identifiers.size() >= MaxListEntries)
					throw DecodeError("MMS: too many identifiers");
				r.identifiers.push_back(ReadUniversalIdentifier(lc.Next()));
			}
			if(c.PeekIs(TagClass::ContextSpecific, 1))
				r.moreFollows = DecodeBoolean(c.Next());
			SkipRest(c);
			return r;
		}
		case 2:
		{
			RequireConstructed(svc);
			IdentifyResponse r;
			Cursor c(svc);
			r.vendorName = ReadIdentifier(c.Expect(TagClass::ContextSpecific, 0));
			r.modelName = ReadIdentifier(c.Expect(TagClass::ContextSpecific, 1));
			r.revision = ReadIdentifier(c.Expect(TagClass::ContextSpecific, 2));
			if(c.PeekIs(TagClass::ContextSpecific, 3))
			{
				const Element list = c.Next();
				RequireConstructed(list);
				std::vector<Oid> syntaxes;
				Cursor lc(list);
				while(!lc.Empty())
				{
					if(syntaxes.size() >= MaxListEntries)
						throw DecodeError("MMS: too many abstract syntaxes");
					syntaxes.push_back(DecodeObjectIdentifier(lc.Expect(UniversalTag::ObjectIdentifier)));
				}
				r.listOfAbstractSyntaxes = std::move(syntaxes);
			}
			SkipRest(c);
			return r;
		}
		case 4:
		{
			RequireConstructed(svc);
			ReadResponse r;
			Cursor c(svc);
			if(c.PeekIs(TagClass::ContextSpecific, 0))
				r.specification = ReadSpecification(Unwrap(c.Next()));
			r.results = ReadAccessResults(c.Expect(TagClass::ContextSpecific, 1), budget);
			SkipRest(c);
			return r;
		}
		case 5:
		{
			RequireConstructed(svc);
			WriteResponse r;
			Cursor c(svc);
			while(!c.Empty())
			{
				if(r.results.size() >= MaxListEntries)
					throw DecodeError("MMS: too many write results");
				const Element e = c.Next();
				RequireContext(e);
				WriteResult wr;
				if(e.tag.number == 0)
				{
					wr.success = false;
					wr.error = ReadNonNegative(e);
				}
				else if(e.tag.number == 1)
					RequireEmpty(e);
				else
					throw DecodeError("MMS: unknown Write result alternative");
				r.results.push_back(wr);
			}
			return r;
		}
		case 6:
		{
			RequireConstructed(svc);
			GetVariableAccessAttributesResponse r;
			Cursor c(svc);
			r.mmsDeletable = DecodeBoolean(c.Expect(TagClass::ContextSpecific, 0));
			if(c.PeekIs(TagClass::ContextSpecific, 1))
				throw DecodeError("MMS: Address in GetVariableAccessAttributes is not supported");
			r.typeDescription = ReadTypeDescription(Unwrap(c.Expect(TagClass::ContextSpecific, 2)), 0, budget);
			if(c.PeekIs(TagClass::ContextSpecific, 3))
				r.accessControlListName = ReadIdentifier(c.Next());
			SkipRest(c);
			return r;
		}
		case 11:
			RequireEmpty(svc);
			return DefineNamedVariableListResponse{};
		case 12:
		{
			RequireConstructed(svc);
			GetNamedVariableListAttributesResponse r;
			Cursor c(svc);
			r.mmsDeletable = DecodeBoolean(c.Expect(TagClass::ContextSpecific, 0));
			r.variables = ReadListOfVariable(c.Expect(TagClass::ContextSpecific, 1));
			if(c.PeekIs(TagClass::ContextSpecific, 2))
				r.accessControlListName = ReadIdentifier(c.Next());
			SkipRest(c);
			return r;
		}
		case 13:
		{
			RequireConstructed(svc);
			DeleteNamedVariableListResponse r;
			Cursor c(svc);
			r.numberMatched = ReadUnsigned32(c.Expect(TagClass::ContextSpecific, 0));
			r.numberDeleted = ReadUnsigned32(c.Expect(TagClass::ContextSpecific, 1));
			SkipRest(c);
			return r;
		}
		default:
			throw DecodeError("MMS: unsupported confirmed service response");
	}
}

// ---------------------------------------------------------------------------------------------
// Confirmed services: encoding
// ---------------------------------------------------------------------------------------------

void WriteRequestService(Writer& w, const IdentifyRequest&)
{
	w.Null(Tag::Context(2));
}

void WriteRequestService(Writer& w, const GetNameListRequest& r)
{
	CheckNonNegative(r.objectClass, "objectClass");
	w.Begin(Tag::Context(1, true));
	w.Begin(Tag::Context(0, true));
	w.Integer(r.objectClass, Tag::Context(0));
	w.End();
	w.Begin(Tag::Context(1, true));
	switch(r.scope)
	{
		case ObjectScope::VmdSpecific:
			w.Null(Tag::Context(0));
			break;
		case ObjectScope::DomainSpecific:
			CheckIdentifier(r.domainId);
			w.String(r.domainId, Tag::Context(1));
			break;
		case ObjectScope::AaSpecific:
			w.Null(Tag::Context(2));
			break;
		default:
			throw std::invalid_argument("MMS: bad objectScope");
	}
	w.End();
	if(r.continueAfter)
	{
		CheckIdentifier(*r.continueAfter);
		w.String(*r.continueAfter, Tag::Context(2));
	}
	w.End();
}

void WriteRequestService(Writer& w, const ReadRequest& r)
{
	w.Begin(Tag::Context(4, true));
	if(r.specificationWithResult)
		WriteBool(w, true, Tag::Context(0));
	w.Begin(Tag::Context(1, true));
	WriteSpecification(w, r.specification);
	w.End();
	w.End();
}

void WriteRequestService(Writer& w, const WriteRequest& r)
{
	if(r.data.size() > MaxListEntries)
		throw std::invalid_argument("MMS: too many data elements");
	w.Begin(Tag::Context(5, true));
	WriteSpecification(w, r.specification);
	w.Begin(Tag::Context(0, true));
	for(const auto& d : r.data)
		WriteData(w, d, 0);
	w.End();
	w.End();
}

void WriteRequestService(Writer& w, const GetVariableAccessAttributesRequest& r)
{
	w.Begin(Tag::Context(6, true));
	w.Begin(Tag::Context(0, true));
	WriteObjectName(w, r.name);
	w.End();
	w.End();
}

void WriteRequestService(Writer& w, const DefineNamedVariableListRequest& r)
{
	w.Begin(Tag::Context(11, true));
	WriteObjectName(w, r.name);
	WriteListOfVariable(w, r.variables, Tag::Context(0, true));
	w.End();
}

void WriteRequestService(Writer& w, const GetNamedVariableListAttributesRequest& r)
{
	w.Begin(Tag::Context(12, true));
	WriteObjectName(w, r.name);
	w.End();
}

void WriteRequestService(Writer& w, const DeleteNamedVariableListRequest& r)
{
	w.Begin(Tag::Context(13, true));
	if(r.scopeOfDelete)
	{
		if(*r.scopeOfDelete < DeleteScope::Specific || *r.scopeOfDelete > DeleteScope::Vmd)
			throw std::invalid_argument("MMS: bad scopeOfDelete");
		w.Integer(*r.scopeOfDelete, Tag::Context(0));
	}
	if(r.listOfVariableListName)
	{
		if(r.listOfVariableListName->size() > MaxListEntries)
			throw std::invalid_argument("MMS: too many names");
		w.Begin(Tag::Context(1, true));
		for(const auto& n : *r.listOfVariableListName)
			WriteObjectName(w, n);
		w.End();
	}
	if(r.domainName)
	{
		CheckIdentifier(*r.domainName);
		w.String(*r.domainName, Tag::Context(2));
	}
	w.End();
}

void WriteResponseService(Writer& w, const IdentifyResponse& r)
{
	CheckIdentifier(r.vendorName);
	CheckIdentifier(r.modelName);
	CheckIdentifier(r.revision);
	w.Begin(Tag::Context(2, true));
	w.String(r.vendorName, Tag::Context(0));
	w.String(r.modelName, Tag::Context(1));
	w.String(r.revision, Tag::Context(2));
	if(r.listOfAbstractSyntaxes)
	{
		if(r.listOfAbstractSyntaxes->size() > MaxListEntries)
			throw std::invalid_argument("MMS: too many abstract syntaxes");
		w.Begin(Tag::Context(3, true));
		for(const auto& o : *r.listOfAbstractSyntaxes)
			w.ObjectIdentifier(o);
		w.End();
	}
	w.End();
}

void WriteResponseService(Writer& w, const GetNameListResponse& r)
{
	if(r.identifiers.size() > MaxListEntries)
		throw std::invalid_argument("MMS: too many identifiers");
	w.Begin(Tag::Context(1, true));
	w.Begin(Tag::Context(0, true));
	for(const auto& id : r.identifiers)
	{
		CheckIdentifier(id);
		w.String(id);
	}
	w.End();
	if(r.moreFollows)
		WriteBool(w, *r.moreFollows, Tag::Context(1));
	w.End();
}

void WriteResponseService(Writer& w, const ReadResponse& r)
{
	w.Begin(Tag::Context(4, true));
	if(r.specification)
	{
		w.Begin(Tag::Context(0, true));
		WriteSpecification(w, *r.specification);
		w.End();
	}
	WriteAccessResults(w, r.results, Tag::Context(1, true));
	w.End();
}

void WriteResponseService(Writer& w, const WriteResponse& r)
{
	if(r.results.size() > MaxListEntries)
		throw std::invalid_argument("MMS: too many write results");
	w.Begin(Tag::Context(5, true));
	for(const auto& wr : r.results)
	{
		if(wr.success)
			w.Null(Tag::Context(1));
		else
		{
			CheckNonNegative(wr.error, "DataAccessError");
			w.Integer(wr.error, Tag::Context(0));
		}
	}
	w.End();
}

void WriteResponseService(Writer& w, const GetVariableAccessAttributesResponse& r)
{
	w.Begin(Tag::Context(6, true));
	WriteBool(w, r.mmsDeletable, Tag::Context(0));
	w.Begin(Tag::Context(2, true));
	WriteTypeDescription(w, r.typeDescription, 0);
	w.End();
	if(r.accessControlListName)
	{
		CheckIdentifier(*r.accessControlListName);
		w.String(*r.accessControlListName, Tag::Context(3));
	}
	w.End();
}

void WriteResponseService(Writer& w, const DefineNamedVariableListResponse&)
{
	w.Null(Tag::Context(11));
}

void WriteResponseService(Writer& w, const GetNamedVariableListAttributesResponse& r)
{
	w.Begin(Tag::Context(12, true));
	WriteBool(w, r.mmsDeletable, Tag::Context(0));
	WriteListOfVariable(w, r.variables, Tag::Context(1, true));
	if(r.accessControlListName)
	{
		CheckIdentifier(*r.accessControlListName);
		w.String(*r.accessControlListName, Tag::Context(2));
	}
	w.End();
}

void WriteResponseService(Writer& w, const DeleteNamedVariableListResponse& r)
{
	w.Begin(Tag::Context(13, true));
	w.Unsigned(r.numberMatched, Tag::Context(0));
	w.Unsigned(r.numberDeleted, Tag::Context(1));
	w.End();
}

// ---------------------------------------------------------------------------------------------
// PDUs: decoding
// ---------------------------------------------------------------------------------------------

ConfirmedRequest ReadConfirmedRequest(const Element& pdu, Budget& budget)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	ConfirmedRequest r;
	r.invokeId = ReadUnsigned32(c.Expect(UniversalTag::Integer));
	if(c.PeekIs(UniversalTag::Sequence))
		throw DecodeError("MMS: listOfModifier is not supported");
	r.service = ReadRequestService(c.Next(), budget);
	SkipRest(c); //cs-request-detail
	return r;
}

ConfirmedResponse ReadConfirmedResponse(const Element& pdu, Budget& budget)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	ConfirmedResponse r;
	r.invokeId = ReadUnsigned32(c.Expect(UniversalTag::Integer));
	r.service = ReadResponseService(c.Next(), budget);
	SkipRest(c); //cs-request-detail
	return r;
}

ConfirmedError ReadConfirmedError(const Element& pdu)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	ConfirmedError r;
	r.invokeId = ReadUnsigned32(c.Expect(TagClass::ContextSpecific, 0));
	if(c.PeekIs(TagClass::ContextSpecific, 1))
		r.modifierPosition = DecodeInteger(c.Next());
	r.error = ReadServiceError(c.Expect(TagClass::ContextSpecific, 2));
	SkipRest(c);
	return r;
}

InformationReport ReadUnconfirmed(const Element& pdu, Budget& budget)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	const Element svc = c.Next();
	if(!svc.tag.Is(TagClass::ContextSpecific, 0))
		throw DecodeError("MMS: only InformationReport is supported as an unconfirmed service");
	RequireConstructed(svc);
	SkipRest(c);
	Cursor sc(svc);
	InformationReport r;
	r.specification = ReadSpecification(sc.Next());
	r.results = ReadAccessResults(sc.Expect(TagClass::ContextSpecific, 0), budget);
	SkipRest(sc);
	return r;
}

Reject ReadReject(const Element& pdu)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	Reject r;
	if(c.PeekIs(TagClass::ContextSpecific, 0))
		r.originalInvokeId = ReadUnsigned32(c.Next());
	const Element alt = c.Next();
	RequireContext(alt);
	if(alt.tag.number < 1 || alt.tag.number > 11)
		throw DecodeError("MMS: unknown reject type");
	r.type = alt.tag.number;
	r.reason = ReadNonNegative(alt);
	SkipRest(c);
	return r;
}

//the optional BitString fields of the detail sequences; returns the BitString
BitString ReadBits(const Element& e)
{
	return DecodeBitString(e);
}

InitiateRequest ReadInitiateRequest(const Element& pdu)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	InitiateRequest r;
	if(c.PeekIs(TagClass::ContextSpecific, 0))
		r.localDetailCalling = ReadNonNegative(c.Next());
	r.proposedMaxServOutstandingCalling = ReadNonNegative(c.Expect(TagClass::ContextSpecific, 1));
	r.proposedMaxServOutstandingCalled = ReadNonNegative(c.Expect(TagClass::ContextSpecific, 2));
	if(c.PeekIs(TagClass::ContextSpecific, 3))
		r.proposedDataStructureNestingLevel = ReadNonNegative(c.Next());
	const Element detail = c.Expect(TagClass::ContextSpecific, 4);
	RequireConstructed(detail);
	SkipRest(c);
	Cursor d(detail);
	r.proposedVersionNumber = ReadNonNegative(d.Expect(TagClass::ContextSpecific, 0));
	r.proposedParameterCbb = ReadBits(d.Expect(TagClass::ContextSpecific, 1));
	r.servicesSupportedCalling = ReadBits(d.Expect(TagClass::ContextSpecific, 2));
	if(d.PeekIs(TagClass::ContextSpecific, 3))
		r.additionalSupportedCalling = ReadBits(d.Next());
	if(d.PeekIs(TagClass::ContextSpecific, 4))
		r.additionalCbbSupportedCalling = ReadBits(d.Next());
	if(d.PeekIs(TagClass::ContextSpecific, 5))
		r.privilegeClassIdentityCalling = ReadIdentifier(d.Next());
	SkipRest(d);
	return r;
}

InitiateResponse ReadInitiateResponse(const Element& pdu)
{
	RequireConstructed(pdu);
	Cursor c(pdu);
	InitiateResponse r;
	if(c.PeekIs(TagClass::ContextSpecific, 0))
		r.localDetailCalled = ReadNonNegative(c.Next());
	r.negotiatedMaxServOutstandingCalling = ReadNonNegative(c.Expect(TagClass::ContextSpecific, 1));
	r.negotiatedMaxServOutstandingCalled = ReadNonNegative(c.Expect(TagClass::ContextSpecific, 2));
	if(c.PeekIs(TagClass::ContextSpecific, 3))
		r.negotiatedDataStructureNestingLevel = ReadNonNegative(c.Next());
	const Element detail = c.Expect(TagClass::ContextSpecific, 4);
	RequireConstructed(detail);
	SkipRest(c);
	Cursor d(detail);
	r.negotiatedVersionNumber = ReadNonNegative(d.Expect(TagClass::ContextSpecific, 0));
	r.negotiatedParameterCbb = ReadBits(d.Expect(TagClass::ContextSpecific, 1));
	r.servicesSupportedCalled = ReadBits(d.Expect(TagClass::ContextSpecific, 2));
	if(d.PeekIs(TagClass::ContextSpecific, 3))
		r.additionalSupportedCalled = ReadBits(d.Next());
	if(d.PeekIs(TagClass::ContextSpecific, 4))
		r.additionalCbbSupportedCalled = ReadBits(d.Next());
	if(d.PeekIs(TagClass::ContextSpecific, 5))
		r.privilegeClassIdentityCalled = ReadIdentifier(d.Next());
	SkipRest(d);
	return r;
}

// ---------------------------------------------------------------------------------------------
// PDUs: encoding
// ---------------------------------------------------------------------------------------------

void WriteInvokeId(Writer& w, uint32_t id)
{
	w.Unsigned(id);
}

void WritePdu(Writer& w, const ConfirmedRequest& r)
{
	w.Begin(Tag::Context(0, true));
	WriteInvokeId(w, r.invokeId);
	std::visit([&w](const auto& s) { WriteRequestService(w, s); }, r.service);
	w.End();
}

void WritePdu(Writer& w, const ConfirmedResponse& r)
{
	w.Begin(Tag::Context(1, true));
	WriteInvokeId(w, r.invokeId);
	std::visit([&w](const auto& s) { WriteResponseService(w, s); }, r.service);
	w.End();
}

void WritePdu(Writer& w, const ConfirmedError& r)
{
	w.Begin(Tag::Context(2, true));
	w.Unsigned(r.invokeId, Tag::Context(0));
	if(r.modifierPosition)
		w.Integer(*r.modifierPosition, Tag::Context(1));
	WriteServiceError(w, r.error, Tag::Context(2, true));
	w.End();
}

void WritePdu(Writer& w, const InformationReport& r)
{
	w.Begin(Tag::Context(3, true));
	w.Begin(Tag::Context(0, true));
	WriteSpecification(w, r.specification);
	WriteAccessResults(w, r.results, Tag::Context(0, true));
	w.End();
	w.End();
}

void WritePdu(Writer& w, const Reject& r)
{
	if(r.type < 1 || r.type > 11)
		throw std::invalid_argument("MMS: bad reject type");
	CheckNonNegative(r.reason, "reject reason");
	w.Begin(Tag::Context(4, true));
	if(r.originalInvokeId)
		w.Unsigned(*r.originalInvokeId, Tag::Context(0));
	w.Integer(r.reason, Tag::Context(static_cast<uint32_t>(r.type)));
	w.End();
}

void WritePdu(Writer& w, const ConcludeRequest&)
{
	w.Null(Tag::Context(11));
}

void WritePdu(Writer& w, const ConcludeResponse&)
{
	w.Null(Tag::Context(12));
}

void WritePdu(Writer& w, const ConcludeError& r)
{
	WriteServiceError(w, r.error, Tag::Context(13, true));
}

void WriteBits(Writer& w, const BitString& b, uint32_t tag)
{
	if(b.unusedBits > 7 || (b.data.empty() && b.unusedBits != 0))
		throw std::invalid_argument("MMS: bad BitString");
	w.BitString(b, Tag::Context(tag));
}

void WritePdu(Writer& w, const InitiateRequest& r)
{
	if(r.localDetailCalling)
		CheckNonNegative(*r.localDetailCalling, "localDetailCalling");
	CheckNonNegative(r.proposedMaxServOutstandingCalling, "proposedMaxServOutstandingCalling");
	CheckNonNegative(r.proposedMaxServOutstandingCalled, "proposedMaxServOutstandingCalled");
	if(r.proposedDataStructureNestingLevel)
		CheckNonNegative(*r.proposedDataStructureNestingLevel, "proposedDataStructureNestingLevel");
	CheckNonNegative(r.proposedVersionNumber, "proposedVersionNumber");
	w.Begin(Tag::Context(8, true));
	if(r.localDetailCalling)
		w.Integer(*r.localDetailCalling, Tag::Context(0));
	w.Integer(r.proposedMaxServOutstandingCalling, Tag::Context(1));
	w.Integer(r.proposedMaxServOutstandingCalled, Tag::Context(2));
	if(r.proposedDataStructureNestingLevel)
		w.Integer(*r.proposedDataStructureNestingLevel, Tag::Context(3));
	w.Begin(Tag::Context(4, true));
	w.Integer(r.proposedVersionNumber, Tag::Context(0));
	WriteBits(w, r.proposedParameterCbb, 1);
	WriteBits(w, r.servicesSupportedCalling, 2);
	if(r.additionalSupportedCalling)
		WriteBits(w, *r.additionalSupportedCalling, 3);
	if(r.additionalCbbSupportedCalling)
		WriteBits(w, *r.additionalCbbSupportedCalling, 4);
	if(r.privilegeClassIdentityCalling)
	{
		CheckIdentifier(*r.privilegeClassIdentityCalling);
		w.String(*r.privilegeClassIdentityCalling, Tag::Context(5));
	}
	w.End();
	w.End();
}

void WritePdu(Writer& w, const InitiateResponse& r)
{
	if(r.localDetailCalled)
		CheckNonNegative(*r.localDetailCalled, "localDetailCalled");
	CheckNonNegative(r.negotiatedMaxServOutstandingCalling, "negotiatedMaxServOutstandingCalling");
	CheckNonNegative(r.negotiatedMaxServOutstandingCalled, "negotiatedMaxServOutstandingCalled");
	if(r.negotiatedDataStructureNestingLevel)
		CheckNonNegative(*r.negotiatedDataStructureNestingLevel, "negotiatedDataStructureNestingLevel");
	CheckNonNegative(r.negotiatedVersionNumber, "negotiatedVersionNumber");
	w.Begin(Tag::Context(9, true));
	if(r.localDetailCalled)
		w.Integer(*r.localDetailCalled, Tag::Context(0));
	w.Integer(r.negotiatedMaxServOutstandingCalling, Tag::Context(1));
	w.Integer(r.negotiatedMaxServOutstandingCalled, Tag::Context(2));
	if(r.negotiatedDataStructureNestingLevel)
		w.Integer(*r.negotiatedDataStructureNestingLevel, Tag::Context(3));
	w.Begin(Tag::Context(4, true));
	w.Integer(r.negotiatedVersionNumber, Tag::Context(0));
	WriteBits(w, r.negotiatedParameterCbb, 1);
	WriteBits(w, r.servicesSupportedCalled, 2);
	if(r.additionalSupportedCalled)
		WriteBits(w, *r.additionalSupportedCalled, 3);
	if(r.additionalCbbSupportedCalled)
		WriteBits(w, *r.additionalCbbSupportedCalled, 4);
	if(r.privilegeClassIdentityCalled)
	{
		CheckIdentifier(*r.privilegeClassIdentityCalled);
		w.String(*r.privilegeClassIdentityCalled, Tag::Context(5));
	}
	w.End();
	w.End();
}

void WritePdu(Writer& w, const InitiateError& r)
{
	WriteServiceError(w, r.error, Tag::Context(10, true));
}

// ---------------------------------------------------------------------------------------------
// scalar access
// ---------------------------------------------------------------------------------------------

Element ScalarElement(const Data& d, DataType expected, const char* what)
{
	if(d.type != expected)
		throw DecodeError(std::string("MMS: Data is not ") + what);
	Element e;
	e.tag = Tag::Context(static_cast<uint32_t>(d.type));
	e.content = d.content.data();
	e.contentLength = d.content.size();
	e.totalLength = d.content.size();
	return e;
}

void PutBigEndian(Bytes& out, uint64_t v, size_t octets)
{
	for(size_t i = 0; i < octets; ++i)
		out.push_back(static_cast<uint8_t>(v >> (8 * (octets - 1 - i))));
}

uint64_t GetBigEndian(const uint8_t* p, size_t octets)
{
	uint64_t v = 0;
	for(size_t i = 0; i < octets; ++i)
		v = (v << 8) | p[i];
	return v;
}
}

// ---------------------------------------------------------------------------------------------
// Data members
// ---------------------------------------------------------------------------------------------

Data Data::Boolean(bool value)
{
	Data d;
	d.type = DataType::Boolean;
	d.content = Bytes{static_cast<uint8_t>(value ? 0x01 : 0x00)};
	return d;
}

Data Data::Integer(int64_t value)
{
	Data d;
	d.type = DataType::Integer;
	d.content = MinimalInteger(value);
	return d;
}

Data Data::Unsigned(uint64_t value)
{
	Data d;
	d.type = DataType::Unsigned;
	d.content = MinimalUnsigned(value);
	return d;
}

Data Data::Float32(float value)
{
	uint32_t bits;
	static_assert(sizeof(bits) == sizeof(value), "float must be 32 bit");
	std::memcpy(&bits, &value, sizeof(bits));
	Data d;
	d.type = DataType::FloatingPoint;
	d.content.push_back(8);
	PutBigEndian(d.content, bits, 4);
	return d;
}

Data Data::Float64(double value)
{
	uint64_t bits;
	static_assert(sizeof(bits) == sizeof(value), "double must be 64 bit");
	std::memcpy(&bits, &value, sizeof(bits));
	Data d;
	d.type = DataType::FloatingPoint;
	d.content.push_back(11);
	PutBigEndian(d.content, bits, 8);
	return d;
}

Data Data::Bits(const BitString& value)
{
	Data d;
	d.type = DataType::BitString;
	d.content.push_back(value.unusedBits);
	d.content.insert(d.content.end(), value.data.begin(), value.data.end());
	return d;
}

Data Data::BitsArray(const BitString& value)
{
	Data d = Bits(value);
	d.type = DataType::BooleanArray;
	return d;
}

Data Data::OctetString(const Bytes& value)
{
	Data d;
	d.type = DataType::OctetString;
	d.content = value;
	return d;
}

Data Data::VisibleString(const std::string& value)
{
	Data d;
	d.type = DataType::VisibleString;
	d.content.assign(value.begin(), value.end());
	return d;
}

Data Data::MmsString(const std::string& value)
{
	Data d = VisibleString(value);
	d.type = DataType::MmsString;
	return d;
}

Data Data::GeneralizedTime(const std::string& value)
{
	Data d = VisibleString(value);
	d.type = DataType::GeneralizedTime;
	return d;
}

Data Data::Bcd(uint64_t value)
{
	if(value > static_cast<uint64_t>(INT64_MAX))
		throw std::invalid_argument("MMS: BCD value out of range");
	Data d;
	d.type = DataType::Bcd;
	d.content = MinimalInteger(static_cast<int64_t>(value));
	return d;
}

Data Data::ObjectId(const Oid& value)
{
	Writer w;
	w.ObjectIdentifier(value);
	const Bytes enc = w.Release();
	//strip the universal tag and length octets (an OID content here is short enough for a one octet length)
	const Element e = ParseElement(enc.data(), enc.size());
	Data d;
	d.type = DataType::ObjectId;
	d.content.assign(e.content, e.content + e.contentLength);
	return d;
}

Data Data::Utc(const UtcTime& value)
{
	if(value.fraction > 0xFFFFFF)
		throw std::invalid_argument("MMS: UtcTime fraction out of range");
	Data d;
	d.type = DataType::UtcTime;
	PutBigEndian(d.content, value.seconds, 4);
	PutBigEndian(d.content, value.fraction, 3);
	d.content.push_back(value.quality);
	return d;
}

Data Data::Binary(const BinaryTime& value)
{
	Data d;
	d.type = DataType::BinaryTime;
	PutBigEndian(d.content, value.milliseconds, 4);
	if(value.days)
		PutBigEndian(d.content, *value.days, 2);
	return d;
}

Data Data::Structure(std::vector<Data> members)
{
	Data d;
	d.type = DataType::Structure;
	d.elements = std::move(members);
	return d;
}

Data Data::Array(std::vector<Data> members)
{
	Data d;
	d.type = DataType::Array;
	d.elements = std::move(members);
	return d;
}

bool Data::AsBoolean() const
{
	return DecodeBoolean(ScalarElement(*this, DataType::Boolean, "a Boolean"));
}

int64_t Data::AsInteger() const
{
	return DecodeInteger(ScalarElement(*this, DataType::Integer, "an Integer"));
}

uint64_t Data::AsUnsigned() const
{
	return DecodeUnsigned(ScalarElement(*this, DataType::Unsigned, "an Unsigned"));
}

double Data::AsFloat() const
{
	if(type != DataType::FloatingPoint)
		throw DecodeError("MMS: Data is not a FloatingPoint");
	if(content.size() == 5 && content[0] == 8)
	{
		const uint32_t bits = static_cast<uint32_t>(GetBigEndian(content.data() + 1, 4));
		float f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	}
	if(content.size() == 9 && content[0] == 11)
	{
		const uint64_t bits = GetBigEndian(content.data() + 1, 8);
		double f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	}
	throw DecodeError("MMS: unsupported FloatingPoint format");
}

BitString Data::AsBits() const
{
	if(type != DataType::BitString && type != DataType::BooleanArray)
		throw DecodeError("MMS: Data is not a bit string");
	Element e;
	e.tag = Tag::Context(static_cast<uint32_t>(type));
	e.content = content.data();
	e.contentLength = content.size();
	return DecodeBitString(e);
}

Bytes Data::AsOctetString() const
{
	if(type != DataType::OctetString)
		throw DecodeError("MMS: Data is not an octet string");
	return content;
}

std::string Data::AsString() const
{
	if(type != DataType::VisibleString && type != DataType::MmsString && type != DataType::GeneralizedTime)
		throw DecodeError("MMS: Data is not a string");
	return std::string(content.begin(), content.end());
}

uint64_t Data::AsBcd() const
{
	const int64_t v = DecodeInteger(ScalarElement(*this, DataType::Bcd, "a BCD"));
	if(v < 0)
		throw DecodeError("MMS: negative BCD");
	return static_cast<uint64_t>(v);
}

Oid Data::AsObjectId() const
{
	return DecodeObjectIdentifier(ScalarElement(*this, DataType::ObjectId, "an object identifier"));
}

UtcTime Data::AsUtc() const
{
	if(type != DataType::UtcTime || content.size() != 8)
		throw DecodeError("MMS: Data is not a UtcTime");
	UtcTime t;
	t.seconds = static_cast<uint32_t>(GetBigEndian(content.data(), 4));
	t.fraction = static_cast<uint32_t>(GetBigEndian(content.data() + 4, 3));
	t.quality = content[7];
	return t;
}

BinaryTime Data::AsBinary() const
{
	if(type != DataType::BinaryTime || (content.size() != 4 && content.size() != 6))
		throw DecodeError("MMS: Data is not a BinaryTime");
	BinaryTime t;
	t.milliseconds = static_cast<uint32_t>(GetBigEndian(content.data(), 4));
	if(content.size() == 6)
		t.days = static_cast<uint16_t>(GetBigEndian(content.data() + 4, 2));
	return t;
}

// ---------------------------------------------------------------------------------------------
// free functions
// ---------------------------------------------------------------------------------------------

BitString MakeBits(std::initializer_list<size_t> setBits, size_t totalBits)
{
	BitString bs(totalBits);
	for(size_t b : setBits)
		bs.Set(b, true);
	return bs;
}

Pdu Decode(const uint8_t* data, size_t length)
{
	const Element top = ParseWhole(data, length);
	RequireContext(top);
	Budget budget;
	switch(top.tag.number)
	{
		case 0: return ReadConfirmedRequest(top, budget);
		case 1: return ReadConfirmedResponse(top, budget);
		case 2: return ReadConfirmedError(top);
		case 3: return ReadUnconfirmed(top, budget);
		case 4: return ReadReject(top);
		case 8: return ReadInitiateRequest(top);
		case 9: return ReadInitiateResponse(top);
		case 10: return InitiateError{ReadServiceError(top)};
		case 11: RequireEmpty(top); return ConcludeRequest{};
		case 12: RequireEmpty(top); return ConcludeResponse{};
		case 13: return ConcludeError{ReadServiceError(top)};
		default:
			throw DecodeError("MMS: unsupported PDU type");
	}
}

Bytes Encode(const Pdu& pdu)
{
	Writer w;
	std::visit([&w](const auto& p) { WritePdu(w, p); }, pdu);
	return w.Release();
}

std::optional<uint32_t> PeekInvokeId(const uint8_t* data, size_t length)
{
	try
	{
		if(data == nullptr || length == 0)
			return std::nullopt;
		const Element top = ParseElement(data, length);
		if(top.tag.cls != TagClass::ContextSpecific || !top.tag.constructed)
			return std::nullopt;
		Cursor c(top);
		switch(top.tag.number)
		{
			case 0:
			case 1:
				return ReadUnsigned32(c.Expect(UniversalTag::Integer));
			case 2:
				return ReadUnsigned32(c.Expect(TagClass::ContextSpecific, 0));
			case 4:
				if(c.PeekIs(TagClass::ContextSpecific, 0))
					return ReadUnsigned32(c.Next());
				return std::nullopt;
			default:
				return std::nullopt;
		}
	}
	catch(const DecodeError&)
	{
		return std::nullopt;
	}
}

Data DecodeData(const uint8_t* data, size_t length)
{
	const Element e = ParseWhole(data, length);
	Budget budget;
	return ReadData(e, 0, budget);
}

Bytes EncodeData(const Data& data)
{
	Writer w;
	WriteData(w, data, 0);
	return w.Release();
}

// ---------------------------------------------------------------------------------------------
// InvokeIdTracker
// ---------------------------------------------------------------------------------------------

InvokeIdTracker::InvokeIdTracker(size_t maxOutstanding, uint32_t firstId):
	limit(maxOutstanding),
	next(firstId)
{}

std::optional<uint32_t> InvokeIdTracker::Allocate()
{
	if(outstanding.size() >= limit)
		return std::nullopt;
	//limit is far below 2^32, so a free id is found within limit + 1 tries
	for(;;)
	{
		const uint32_t id = next++;
		if(outstanding.insert(id).second)
			return id;
	}
}

bool InvokeIdTracker::Complete(uint32_t id)
{
	return outstanding.erase(id) != 0;
}

bool InvokeIdTracker::IsOutstanding(uint32_t id) const
{
	return outstanding.count(id) != 0;
}

void InvokeIdTracker::Clear()
{
	outstanding.clear();
}

} // namespace mms
} // namespace iccp
