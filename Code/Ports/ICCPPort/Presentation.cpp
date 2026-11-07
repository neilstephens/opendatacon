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
 * Presentation.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8823-1 / ITU-T X.226 presentation protocol, normal mode, kernel only.
 */

#include "Presentation.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace iccp
{
namespace presentation
{

using namespace ber;

namespace
{
constexpr int64_t MaxContextId = std::numeric_limits<int32_t>::max();
constexpr int64_t ModeNormal = 1;

enum class Kind
{
	Cp,
	Cpa,
	Cpr
};

//everything the three connect-family PPDUs can hold
struct Params
{
	std::optional<Bytes> calling;
	std::optional<Bytes> called;
	std::optional<Bytes> responding;
	bool haveContexts = false;
	std::vector<ContextDefinition> contexts;
	bool haveResults = false;
	std::vector<ContextResult> results;
	std::optional<DefaultContextName> defaultName;
	std::optional<int64_t> defaultResult;
	std::optional<int64_t> providerReason;
	std::optional<uint8_t> presentationRequirements;
	std::optional<uint16_t> userSessionRequirements;
	bool haveVersion = false;
	UserData userData;
	bool haveUserData = false;
};

// ---- decoding ----

Element ParseWhole(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("Presentation: null data");
	const Element e = ParseElement(data, length);
	if(e.totalLength != length)
		throw DecodeError("Presentation: trailing octets after PPDU");
	return e;
}

void RequireConstructed(const Element& e)
{
	if(!e.tag.constructed)
		throw DecodeError("Presentation: constructed encoding expected");
}

template<typename T>
void SetOnce(std::optional<T>& field, T value)
{
	if(field)
		throw DecodeError("Presentation: duplicate parameter");
	field = std::move(value);
}

Bytes ReadSelector(const Element& e)
{
	Bytes b = DecodeOctetString(e);
	if(b.size() > MaxSelectorLength)
		throw DecodeError("Presentation: selector too long");
	return b;
}

int64_t ReadContextId(const Element& e)
{
	const int64_t id = DecodeInteger(e);
	if(id < 1 || id > MaxContextId)
		throw DecodeError("Presentation: bad presentation context identifier");
	return id;
}

//Bit i of the string maps to mask bit i; bits beyond maxBits are ignored
uint32_t ReadBits(const Element& e, size_t maxBits)
{
	const BitString bs = DecodeBitString(e);
	const size_t n = bs.BitCount() < maxBits ? bs.BitCount() : maxBits;
	uint32_t mask = 0;
	for(size_t i = 0; i < n; ++i)
		if(bs.Get(i))
			mask |= (1u << i);
	return mask;
}

void ReadVersion(const Element& e)
{
	if((ReadBits(e, 8) & 0x01) == 0)
		throw DecodeError("Presentation: protocol version 1 not offered");
}

std::vector<ContextDefinition> ReadContextDefinitions(const Element& list)
{
	RequireConstructed(list);
	std::vector<ContextDefinition> out;
	Cursor items(list);
	while(!items.Empty())
	{
		const Element item = items.Expect(UniversalTag::Sequence);
		RequireConstructed(item);
		if(out.size() >= MaxContexts)
			throw DecodeError("Presentation: too many presentation contexts");

		Cursor c(item);
		ContextDefinition d;
		d.id = ReadContextId(c.Expect(UniversalTag::Integer));
		d.abstractSyntax = DecodeObjectIdentifier(c.Expect(UniversalTag::ObjectIdentifier));
		const Element ts = c.Expect(UniversalTag::Sequence);
		RequireConstructed(ts);
		if(!c.Empty())
			throw DecodeError("Presentation: trailing data in context definition");
		Cursor tc(ts);
		while(!tc.Empty())
		{
			if(d.transferSyntaxes.size() >= MaxTransferSyntaxes)
				throw DecodeError("Presentation: too many transfer syntaxes");
			d.transferSyntaxes.push_back(DecodeObjectIdentifier(tc.Expect(UniversalTag::ObjectIdentifier)));
		}
		out.push_back(std::move(d));
	}
	return out;
}

std::vector<ContextResult> ReadContextResults(const Element& list)
{
	RequireConstructed(list);
	std::vector<ContextResult> out;
	Cursor items(list);
	while(!items.Empty())
	{
		const Element item = items.Expect(UniversalTag::Sequence);
		RequireConstructed(item);
		if(out.size() >= MaxContexts)
			throw DecodeError("Presentation: too many presentation contexts");

		Cursor c(item);
		ContextResult r;
		r.result = DecodeInteger(c.Expect(TagClass::ContextSpecific, 0));
		if(r.result < Result::Acceptance || r.result > Result::ProviderRejection)
			throw DecodeError("Presentation: bad context definition result");
		if(c.PeekIs(TagClass::ContextSpecific, 1))
			r.transferSyntax = DecodeObjectIdentifier(c.Next());
		if(c.PeekIs(TagClass::ContextSpecific, 2))
			r.providerReason = DecodeInteger(c.Next());
		if(!c.Empty())
			throw DecodeError("Presentation: trailing data in context result");
		out.push_back(std::move(r));
	}
	return out;
}

DefaultContextName ReadDefaultContextName(const Element& e)
{
	RequireConstructed(e);
	Cursor c(e);
	DefaultContextName n;
	n.abstractSyntax = DecodeObjectIdentifier(c.Expect(TagClass::ContextSpecific, 0));
	n.transferSyntax = DecodeObjectIdentifier(c.Expect(TagClass::ContextSpecific, 1));
	if(!c.Empty())
		throw DecodeError("Presentation: trailing data in default context name");
	return n;
}

Pdv ReadPdv(const Element& e)
{
	RequireConstructed(e);
	Cursor c(e);
	Pdv p;
	if(c.PeekIs(UniversalTag::ObjectIdentifier))
		p.transferSyntax = DecodeObjectIdentifier(c.Next());
	p.contextId = ReadContextId(c.Expect(UniversalTag::Integer));
	const Element v = c.Next();
	if(!c.Empty())
		throw DecodeError("Presentation: trailing data in PDV-list");

	if(v.tag.Is(TagClass::ContextSpecific, 0))
	{
		//single-ASN1-type is EXPLICITly tagged: the content is one complete element
		RequireConstructed(v);
		const Element inner = ParseElement(v.content, v.contentLength);
		if(inner.totalLength != v.contentLength)
			throw DecodeError("Presentation: single-ASN.1-type is not exactly one element");
		p.kind = PdvKind::SingleAsn1Type;
	}
	else if(v.tag.Is(TagClass::ContextSpecific, 1))
	{
		if(v.tag.constructed)
			throw DecodeError("Presentation: constructed octet-aligned data not supported");
		p.kind = PdvKind::OctetAligned;
	}
	else
		throw DecodeError("Presentation: unsupported presentation-data-values choice");

	p.data = v.content;
	p.dataLength = v.contentLength;
	return p;
}

//the content of a [APPLICATION 1] fully-encoded-data element
UserData ReadUserData(const Element& e)
{
	if(e.tag.Is(TagClass::Application, 0))
		throw DecodeError("Presentation: simply-encoded-data not supported");
	if(!e.tag.Is(TagClass::Application, 1))
		throw DecodeError("Presentation: unexpected user data encoding");
	RequireConstructed(e);

	UserData out;
	Cursor c(e);
	while(!c.Empty())
	{
		if(out.size() >= MaxPdvs)
			throw DecodeError("Presentation: too many PDV-lists");
		out.push_back(ReadPdv(c.Expect(UniversalTag::Sequence)));
	}
	if(out.empty())
		throw DecodeError("Presentation: empty fully-encoded-data");
	return out;
}

//Which normal-mode-parameters are allowed in each PPDU
bool Allowed(Kind kind, uint32_t number)
{
	switch(number)
	{
		case 0:  return true;              //protocol-version
		case 1:                            //calling-presentation-selector
		case 2:                            //called-presentation-selector
		case 4:                            //presentation-context-definition-list
		case 6:  return kind == Kind::Cp;  //default-context-name
		case 3:                            //responding-presentation-selector
		case 5:  return kind != Kind::Cp;  //presentation-context-definition-result-list
		case 7:                            //default-context-result
		case 10: return kind == Kind::Cpr; //provider-reason
		case 8:                            //presentation-requirements
		case 9:  return kind != Kind::Cpr; //user-session-requirements
		default: return true;              //unknown: skipped by the caller
	}
}

void ParseNormalMode(const Element& params, Kind kind, Params& out)
{
	RequireConstructed(params);
	Cursor c(params);
	while(!c.Empty())
	{
		const Element p = c.Next();

		if(p.tag.cls == TagClass::Application)
		{
			if(out.haveUserData)
				throw DecodeError("Presentation: duplicate user data");
			out.haveUserData = true;
			out.userData = ReadUserData(p);
			continue;
		}
		if(p.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("Presentation: unexpected parameter class");
		if(!Allowed(kind, p.tag.number))
			throw DecodeError("Presentation: parameter not valid in this PPDU");

		switch(p.tag.number)
		{
			case 0:
				if(out.haveVersion)
					throw DecodeError("Presentation: duplicate parameter");
				out.haveVersion = true;
				ReadVersion(p);
				break;
			case 1:
				SetOnce<Bytes>(out.calling, ReadSelector(p));
				break;
			case 2:
				SetOnce<Bytes>(out.called, ReadSelector(p));
				break;
			case 3:
				SetOnce<Bytes>(out.responding, ReadSelector(p));
				break;
			case 4:
				if(out.haveContexts)
					throw DecodeError("Presentation: duplicate parameter");
				out.haveContexts = true;
				out.contexts = ReadContextDefinitions(p);
				break;
			case 5:
				if(out.haveResults)
					throw DecodeError("Presentation: duplicate parameter");
				out.haveResults = true;
				out.results = ReadContextResults(p);
				break;
			case 6:
				SetOnce<DefaultContextName>(out.defaultName, ReadDefaultContextName(p));
				break;
			case 7:
			{
				const int64_t r = DecodeInteger(p);
				if(r < Result::Acceptance || r > Result::ProviderRejection)
					throw DecodeError("Presentation: bad default context result");
				SetOnce<int64_t>(out.defaultResult, r);
				break;
			}
			case 8:
				SetOnce<uint8_t>(out.presentationRequirements, static_cast<uint8_t>(ReadBits(p, 8)));
				break;
			case 9:
				SetOnce<uint16_t>(out.userSessionRequirements, static_cast<uint16_t>(ReadBits(p, 16)));
				break;
			case 10:
				SetOnce<int64_t>(out.providerReason, DecodeInteger(p));
				break;
			default:
				break; //unknown context-specific parameter
		}
	}
}

//CP / CPA outer SET: mode-selector and normal-mode-parameters
void ParseConnectSet(const uint8_t* data, size_t length, Kind kind, Params& out)
{
	const Element top = ParseWhole(data, length);
	if(!top.tag.Is(UniversalTag::Set))
		throw DecodeError("Presentation: PPDU is not a SET");
	RequireConstructed(top);

	bool haveMode = false, haveNormal = false;
	Cursor c(top);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.Is(TagClass::ContextSpecific, 0))
		{
			if(haveMode)
				throw DecodeError("Presentation: duplicate mode-selector");
			haveMode = true;
			RequireConstructed(e);
			Cursor mc(e);
			const int64_t mode = DecodeInteger(mc.Expect(TagClass::ContextSpecific, 0));
			if(!mc.Empty())
				throw DecodeError("Presentation: trailing data in mode-selector");
			if(mode != ModeNormal)
				throw DecodeError("Presentation: only normal mode is supported");
		}
		else if(e.tag.Is(TagClass::ContextSpecific, 1))
			throw DecodeError("Presentation: X.410 mode is not supported");
		else if(e.tag.Is(TagClass::ContextSpecific, 2))
		{
			if(haveNormal)
				throw DecodeError("Presentation: duplicate normal-mode-parameters");
			haveNormal = true;
			ParseNormalMode(e, kind, out);
		}
		else if(e.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("Presentation: unexpected element in PPDU");
		//other context-specific elements are skipped
	}
	if(!haveMode || !haveNormal)
		throw DecodeError("Presentation: missing mode-selector or normal-mode-parameters");
}

// ---- encoding ----

void CheckContextId(int64_t id)
{
	if(id < 1 || id > MaxContextId)
		throw std::invalid_argument("Presentation: bad presentation context identifier");
}

void WriteSelector(Writer& w, const Bytes& selector, uint32_t number)
{
	if(selector.size() > MaxSelectorLength)
		throw std::invalid_argument("Presentation: selector longer than 16 octets");
	w.OctetString(selector, Tag::Context(number));
}

//Highest set bit decides the string length, as for a named bit list, but never fewer than minBits
BitString MaskToBits(uint32_t mask, size_t maxBits, size_t minBits = 0)
{
	size_t n = minBits;
	for(size_t i = 0; i < maxBits; ++i)
		if(mask & (1u << i))
			n = i + 1 > n ? i + 1 : n;
	BitString bs(n);
	for(size_t i = 0; i < n; ++i)
		if(mask & (1u << i))
			bs.Set(i, true);
	return bs;
}

void WriteModeSelector(Writer& w)
{
	w.Begin(Tag::Context(0, true));
	w.Integer(ModeNormal, Tag::Context(0));
	w.End();
}

void WriteContextDefinitions(Writer& w, const std::vector<ContextDefinition>& list)
{
	if(list.size() > MaxContexts)
		throw std::invalid_argument("Presentation: too many presentation contexts");
	w.Begin(Tag::Context(4, true));
	for(const auto& d : list)
	{
		CheckContextId(d.id);
		if(d.transferSyntaxes.size() > MaxTransferSyntaxes)
			throw std::invalid_argument("Presentation: too many transfer syntaxes");
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		w.Integer(d.id);
		w.ObjectIdentifier(d.abstractSyntax);
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		for(const auto& ts : d.transferSyntaxes)
			w.ObjectIdentifier(ts);
		w.End();
		w.End();
	}
	w.End();
}

void WriteContextResults(Writer& w, const std::vector<ContextResult>& list)
{
	if(list.size() > MaxContexts)
		throw std::invalid_argument("Presentation: too many presentation contexts");
	w.Begin(Tag::Context(5, true));
	for(const auto& r : list)
	{
		if(r.result < Result::Acceptance || r.result > Result::ProviderRejection)
			throw std::invalid_argument("Presentation: bad context definition result");
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		w.Integer(r.result, Tag::Context(0));
		if(r.transferSyntax)
			w.ObjectIdentifier(*r.transferSyntax, Tag::Context(1));
		if(r.providerReason)
			w.Integer(*r.providerReason, Tag::Context(2));
		w.End();
	}
	w.End();
}

void WriteUserData(Writer& w, const UserData& ud)
{
	if(ud.empty())
		return;
	if(ud.size() > MaxPdvs)
		throw std::invalid_argument("Presentation: too many PDV-lists");
	w.Begin(Tag::Application(1, true));
	for(const auto& p : ud)
	{
		CheckContextId(p.contextId);
		if(p.dataLength > 0 && p.data == nullptr)
			throw std::invalid_argument("Presentation: null PDV data");

		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		if(p.transferSyntax)
			w.ObjectIdentifier(*p.transferSyntax);
		w.Integer(p.contextId);
		if(p.kind == PdvKind::SingleAsn1Type)
		{
			try
			{
				const Element inner = ParseElement(p.data, p.dataLength);
				if(inner.totalLength != p.dataLength)
					throw DecodeError("trailing");
			}
			catch(const DecodeError&)
			{
				throw std::invalid_argument("Presentation: single-ASN.1-type data is not exactly one BER element");
			}
			w.Begin(Tag::Context(0, true));
			w.Raw(Bytes(p.data, p.data + p.dataLength));
			w.End();
		}
		else
		{
			w.OctetString(p.data, p.dataLength, Tag::Context(1));
		}
		w.End();
	}
	w.End();
}

void WriteRequirements(Writer& w, const std::optional<uint8_t>& presentation, const std::optional<uint16_t>& session)
{
	//both defined bits are always written, as real-world peers do ("88 02 06 00" for none): a named bit list may be
	//trimmed but peers send the full 2 bits and byte-identical output makes interop traces comparable
	if(presentation)
		w.BitString(MaskToBits(*presentation, 8, 2), Tag::Context(8));
	if(session)
		w.BitString(MaskToBits(*session, 16), Tag::Context(9));
}
}

ConnectRequest DecodeConnectRequest(const uint8_t* data, size_t length)
{
	Params p;
	ParseConnectSet(data, length, Kind::Cp, p);
	ConnectRequest cp;
	cp.callingSelector = std::move(p.calling);
	cp.calledSelector = std::move(p.called);
	cp.contexts = std::move(p.contexts);
	cp.defaultContextName = std::move(p.defaultName);
	cp.presentationRequirements = p.presentationRequirements;
	cp.userSessionRequirements = p.userSessionRequirements;
	cp.userData = std::move(p.userData);
	return cp;
}

ConnectAccept DecodeConnectAccept(const uint8_t* data, size_t length)
{
	Params p;
	ParseConnectSet(data, length, Kind::Cpa, p);
	ConnectAccept cpa;
	cpa.respondingSelector = std::move(p.responding);
	cpa.results = std::move(p.results);
	cpa.presentationRequirements = p.presentationRequirements;
	cpa.userSessionRequirements = p.userSessionRequirements;
	cpa.userData = std::move(p.userData);
	return cpa;
}

ConnectRefuse DecodeConnectRefuse(const uint8_t* data, size_t length)
{
	const Element top = ParseWhole(data, length);
	//the normal-mode choice is a plain SEQUENCE; the X.410 choice is a SET
	if(!top.tag.Is(UniversalTag::Sequence))
		throw DecodeError("Presentation: CPR is not a SEQUENCE (X.410 mode is not supported)");

	Params p;
	ParseNormalMode(top, Kind::Cpr, p);
	ConnectRefuse cpr;
	cpr.respondingSelector = std::move(p.responding);
	cpr.results = std::move(p.results);
	cpr.defaultContextResult = p.defaultResult;
	cpr.providerReason = p.providerReason;
	cpr.userData = std::move(p.userData);
	return cpr;
}

Abort DecodeAbort(const uint8_t* data, size_t length)
{
	const Element top = ParseWhole(data, length);
	RequireConstructed(top);
	Abort a;
	Cursor c(top);

	if(top.tag.Is(TagClass::ContextSpecific, 0))
	{
		//ARU normal mode: [0] SEQUENCE { context identifier list [0] OPTIONAL, user-data OPTIONAL }
		bool haveList = false, haveUserData = false;
		while(!c.Empty())
		{
			const Element e = c.Next();
			if(e.tag.cls == TagClass::Application)
			{
				if(haveUserData)
					throw DecodeError("Presentation: duplicate user data");
				haveUserData = true;
				a.userData = ReadUserData(e);
			}
			else if(e.tag.Is(TagClass::ContextSpecific, 0))
			{
				if(haveList)
					throw DecodeError("Presentation: duplicate context identifier list");
				haveList = true;
				RequireConstructed(e);
				//the list is not used; only its structure is checked
				Cursor lc(e);
				size_t count = 0;
				while(!lc.Empty())
				{
					if(++count > MaxContexts)
						throw DecodeError("Presentation: too many presentation contexts");
					const Element item = lc.Expect(UniversalTag::Sequence);
					RequireConstructed(item);
					Cursor ic(item);
					ReadContextId(ic.Expect(UniversalTag::Integer));
					DecodeObjectIdentifier(ic.Expect(UniversalTag::ObjectIdentifier));
					if(!ic.Empty())
						throw DecodeError("Presentation: trailing data in context identifier");
				}
			}
			else if(e.tag.cls != TagClass::ContextSpecific)
				throw DecodeError("Presentation: unexpected element in ARU");
		}
		return a;
	}

	if(top.tag.Is(UniversalTag::Sequence))
	{
		//ARP: SEQUENCE { provider-reason [0] OPTIONAL, event-identifier [1] OPTIONAL }
		a.providerAbort = true;
		while(!c.Empty())
		{
			const Element e = c.Next();
			if(e.tag.Is(TagClass::ContextSpecific, 0))
				SetOnce<int64_t>(a.providerReason, DecodeInteger(e));
			else if(e.tag.Is(TagClass::ContextSpecific, 1))
				SetOnce<int64_t>(a.eventIdentifier, DecodeInteger(e));
			else if(e.tag.cls != TagClass::ContextSpecific)
				throw DecodeError("Presentation: unexpected element in ARP");
		}
		return a;
	}

	throw DecodeError("Presentation: not an ARU or ARP PPDU");
}

Pdv DecodeData(const uint8_t* data, size_t length)
{
	const Element top = ParseWhole(data, length);
	UserData ud = ReadUserData(top);
	if(ud.size() != 1)
		throw DecodeError("Presentation: data PPDU must hold exactly one PDV-list");
	return ud[0];
}

Bytes Encode(const ConnectRequest& cp)
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Set, true));
	WriteModeSelector(w);
	w.Begin(Tag::Context(2, true));
	if(cp.callingSelector)
		WriteSelector(w, *cp.callingSelector, 1);
	if(cp.calledSelector)
		WriteSelector(w, *cp.calledSelector, 2);
	if(!cp.contexts.empty())
		WriteContextDefinitions(w, cp.contexts);
	if(cp.defaultContextName)
	{
		w.Begin(Tag::Context(6, true));
		w.ObjectIdentifier(cp.defaultContextName->abstractSyntax, Tag::Context(0));
		w.ObjectIdentifier(cp.defaultContextName->transferSyntax, Tag::Context(1));
		w.End();
	}
	WriteRequirements(w, cp.presentationRequirements, cp.userSessionRequirements);
	WriteUserData(w, cp.userData);
	w.End();
	w.End();
	return w.Release();
}

Bytes Encode(const ConnectAccept& cpa)
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Set, true));
	WriteModeSelector(w);
	w.Begin(Tag::Context(2, true));
	if(cpa.respondingSelector)
		WriteSelector(w, *cpa.respondingSelector, 3);
	if(!cpa.results.empty())
		WriteContextResults(w, cpa.results);
	WriteRequirements(w, cpa.presentationRequirements, cpa.userSessionRequirements);
	WriteUserData(w, cpa.userData);
	w.End();
	w.End();
	return w.Release();
}

Bytes Encode(const ConnectRefuse& cpr)
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	if(cpr.respondingSelector)
		WriteSelector(w, *cpr.respondingSelector, 3);
	if(!cpr.results.empty())
		WriteContextResults(w, cpr.results);
	if(cpr.defaultContextResult)
	{
		if(*cpr.defaultContextResult < Result::Acceptance || *cpr.defaultContextResult > Result::ProviderRejection)
			throw std::invalid_argument("Presentation: bad default context result");
		w.Integer(*cpr.defaultContextResult, Tag::Context(7));
	}
	if(cpr.providerReason)
		w.Integer(*cpr.providerReason, Tag::Context(10));
	WriteUserData(w, cpr.userData);
	w.End();
	return w.Release();
}

Bytes Encode(const Abort& ab)
{
	Writer w;
	if(ab.providerAbort)
	{
		if(!ab.userData.empty())
			throw std::invalid_argument("Presentation: ARP carries no user data");
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		if(ab.providerReason)
			w.Integer(*ab.providerReason, Tag::Context(0));
		if(ab.eventIdentifier)
			w.Integer(*ab.eventIdentifier, Tag::Context(1));
		w.End();
	}
	else
	{
		if(ab.providerReason || ab.eventIdentifier)
			throw std::invalid_argument("Presentation: ARU has no reason or event identifier");
		w.Begin(Tag::Context(0, true));
		WriteUserData(w, ab.userData);
		w.End();
	}
	return w.Release();
}

Bytes EncodeData(const Pdv& pdv)
{
	Writer w;
	WriteUserData(w, UserData{pdv});
	return w.Release();
}

} // namespace presentation
} // namespace iccp
