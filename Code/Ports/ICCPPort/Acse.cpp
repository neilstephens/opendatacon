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
 * Acse.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8650-1 / ITU-T X.227 association control service element.
 */

#include "Acse.h"
#include <cstdio>
#include <stdexcept>

namespace iccp
{
namespace acse
{

using namespace ber;

namespace
{
constexpr uint32_t ExternalTagNumber = 8;
constexpr uint32_t ObjectDescriptorTagNumber = 7;
constexpr uint32_t TagImplementationInformation = 29;
constexpr uint32_t TagUserInformation = 30;

// ---- decoding ----

Element ParseWhole(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("ACSE: null data");
	const Element e = ParseElement(data, length);
	if(e.totalLength != length)
		throw DecodeError("ACSE: trailing octets after APDU");
	return e;
}

Element ParseApdu(const uint8_t* data, size_t length, ApduType type)
{
	const Element e = ParseWhole(data, length);
	if(!e.tag.Is(TagClass::Application, static_cast<uint32_t>(type)))
		throw DecodeError("ACSE: unexpected APDU type");
	if(!e.tag.constructed)
		throw DecodeError("ACSE: constructed encoding expected");
	return e;
}

void RequireConstructed(const Element& e)
{
	if(!e.tag.constructed)
		throw DecodeError("ACSE: constructed encoding expected");
}

//Each field may appear once; numbers are context tag numbers below 64
void MarkSeen(uint64_t& seen, uint32_t number)
{
	const uint64_t bit = static_cast<uint64_t>(1) << number;
	if(seen & bit)
		throw DecodeError("ACSE: duplicate field");
	seen |= bit;
}

//The content of an EXPLICIT tag: exactly one inner element
Element Unwrap(const Element& wrapper)
{
	RequireConstructed(wrapper);
	Cursor c(wrapper);
	const Element inner = c.Next();
	if(!c.Empty())
		throw DecodeError("ACSE: more than one element in an explicit tag");
	return inner;
}

//The complete encoding of the inner element of an EXPLICIT tag
Bytes InnerEncoding(const Element& wrapper, const Element& inner)
{
	return Bytes(wrapper.content, wrapper.content + inner.totalLength);
}

int64_t ReadExplicitInteger(const Element& wrapper)
{
	const Element inner = Unwrap(wrapper);
	if(!inner.tag.Is(UniversalTag::Integer))
		throw DecodeError("ACSE: INTEGER expected");
	return DecodeInteger(inner);
}

int64_t ReadNonNegative(int64_t v)
{
	if(v < 0)
		throw DecodeError("ACSE: negative value");
	return v;
}

Oid ReadApplicationContextName(const Element& wrapper)
{
	const Element inner = Unwrap(wrapper);
	if(!inner.tag.Is(UniversalTag::ObjectIdentifier))
		throw DecodeError("ACSE: application-context-name is not an OBJECT IDENTIFIER");
	return DecodeObjectIdentifier(inner);
}

ApTitle ReadApTitle(const Element& wrapper)
{
	const Element inner = Unwrap(wrapper);
	ApTitle t;
	if(inner.tag.Is(UniversalTag::ObjectIdentifier))
	{
		t.form2 = true;
		t.oid = DecodeObjectIdentifier(inner);
	}
	else if(inner.tag.Is(UniversalTag::Sequence) && inner.tag.constructed)
	{
		t.form2 = false;
		t.form1 = InnerEncoding(wrapper, inner);
	}
	else
		throw DecodeError("ACSE: unsupported AP-title form");
	return t;
}

AeQualifier ReadAeQualifier(const Element& wrapper)
{
	const Element inner = Unwrap(wrapper);
	AeQualifier q;
	if(inner.tag.Is(UniversalTag::Integer))
	{
		q.form2 = true;
		q.number = DecodeInteger(inner);
	}
	else if((inner.tag.Is(UniversalTag::Sequence) || inner.tag.Is(UniversalTag::Set)) && inner.tag.constructed)
	{
		q.form2 = false;
		q.form1 = InnerEncoding(wrapper, inner);
	}
	else
		throw DecodeError("ACSE: unsupported AE-qualifier form");
	return q;
}

uint8_t ReadBitMask8(const Element& e)
{
	const BitString bs = DecodeBitString(e);
	const size_t n = bs.BitCount() < 8 ? bs.BitCount() : 8;
	uint8_t mask = 0;
	for(size_t i = 0; i < n; ++i)
		if(bs.Get(i))
			mask = static_cast<uint8_t>(mask | (1u << i));
	return mask;
}

//[n] EXPLICIT Authentication-value: kept as the complete encoding of the CHOICE alternative
Bytes ReadAuthenticationValue(const Element& wrapper)
{
	const Element inner = Unwrap(wrapper);
	return InnerEncoding(wrapper, inner);
}

uint8_t ReadProtocolVersion(const Element& e)
{
	return ReadBitMask8(e);
}

uint8_t ReadAcseRequirements(const Element& e)
{
	return ReadBitMask8(e);
}

External ReadExternal(const Element& e)
{
	if(!e.tag.Is(TagClass::Universal, ExternalTagNumber))
		throw DecodeError("ACSE: EXTERNAL expected in user information");
	RequireConstructed(e);

	Cursor c(e);
	External x;
	if(c.PeekIs(UniversalTag::ObjectIdentifier))
		x.directReference = DecodeObjectIdentifier(c.Next());
	if(c.PeekIs(UniversalTag::Integer))
		x.indirectReference = DecodeInteger(c.Next());
	if(c.PeekIs(TagClass::Universal, ObjectDescriptorTagNumber))
		c.Next(); //data-value-descriptor is not used

	const Element enc = c.Next();
	if(!c.Empty())
		throw DecodeError("ACSE: trailing data in EXTERNAL");

	if(enc.tag.Is(TagClass::ContextSpecific, 0))
	{
		RequireConstructed(enc);
		const Element inner = ParseElement(enc.content, enc.contentLength);
		if(inner.totalLength != enc.contentLength)
			throw DecodeError("ACSE: single-ASN.1-type is not exactly one element");
		x.encoding = ExternalEncoding::SingleAsn1Type;
	}
	else if(enc.tag.Is(TagClass::ContextSpecific, 1))
	{
		if(enc.tag.constructed)
			throw DecodeError("ACSE: constructed octet-aligned data not supported");
		x.encoding = ExternalEncoding::OctetAligned;
	}
	else
		throw DecodeError("ACSE: unsupported EXTERNAL encoding");

	x.data = enc.content;
	x.dataLength = enc.contentLength;
	return x;
}

//[30] IMPLICIT Association-information (SEQUENCE OF EXTERNAL)
UserInformation ReadUserInformation(const Element& e)
{
	RequireConstructed(e);
	UserInformation out;
	Cursor c(e);
	while(!c.Empty())
	{
		if(out.size() >= MaxExternals)
			throw DecodeError("ACSE: too many EXTERNALs in user information");
		out.push_back(ReadExternal(c.Next()));
	}
	return out;
}

std::string ReadImplementationInformation(const Element& e)
{
	return DecodeString(e);
}

// ---- encoding ----

void CheckOneElement(const uint8_t* data, size_t length, const char* what)
{
	try
	{
		const Element e = ParseElement(data, length);
		if(e.totalLength != length)
			throw DecodeError("trailing");
	}
	catch(const DecodeError&)
	{
		throw std::invalid_argument(std::string("ACSE: ") + what + " is not exactly one BER element");
	}
}

BitString MaskToBits(uint32_t mask, size_t maxBits)
{
	size_t n = 0;
	for(size_t i = 0; i < maxBits; ++i)
		if(mask & (1u << i))
			n = i + 1;
	BitString bs(n);
	for(size_t i = 0; i < n; ++i)
		if(mask & (1u << i))
			bs.Set(i, true);
	return bs;
}

void WriteApplicationContextName(Writer& w, const Oid& name)
{
	w.Begin(Tag::Context(1, true));
	w.ObjectIdentifier(name);
	w.End();
}

void WriteApTitle(Writer& w, uint32_t number, const ApTitle& t)
{
	w.Begin(Tag::Context(number, true));
	if(t.form2)
		w.ObjectIdentifier(t.oid);
	else
	{
		CheckOneElement(t.form1.data(), t.form1.size(), "AP-title form 1");
		if((t.form1[0] & 0x1F) != 0x10 || (t.form1[0] & 0xC0) != 0)
			throw std::invalid_argument("ACSE: AP-title form 1 is not a SEQUENCE");
		w.Raw(t.form1);
	}
	w.End();
}

void WriteAeQualifier(Writer& w, uint32_t number, const AeQualifier& q)
{
	w.Begin(Tag::Context(number, true));
	if(q.form2)
		w.Integer(q.number);
	else
	{
		CheckOneElement(q.form1.data(), q.form1.size(), "AE-qualifier form 1");
		const uint8_t tagNumber = q.form1[0] & 0x1F;
		if((q.form1[0] & 0xC0) != 0 || (q.form1[0] & 0x20) == 0 || (tagNumber != 0x10 && tagNumber != 0x11))
			throw std::invalid_argument("ACSE: AE-qualifier form 1 is not a SEQUENCE or SET");
		w.Raw(q.form1);
	}
	w.End();
}

void WriteInvocationId(Writer& w, uint32_t number, const std::optional<int64_t>& id)
{
	if(!id)
		return;
	w.Begin(Tag::Context(number, true));
	w.Integer(*id);
	w.End();
}

void WriteAuthentication(Writer& w, const Authentication& a, uint32_t requirementsTag, uint32_t mechanismTag, uint32_t valueTag)
{
	if(a.acseRequirements)
		w.BitString(MaskToBits(*a.acseRequirements, 8), Tag::Context(requirementsTag));
	if(a.mechanismName)
		w.ObjectIdentifier(*a.mechanismName, Tag::Context(mechanismTag));
	if(a.authenticationValue)
	{
		CheckOneElement(a.authenticationValue->data(), a.authenticationValue->size(), "authentication-value");
		w.Begin(Tag::Context(valueTag, true));
		w.Raw(*a.authenticationValue);
		w.End();
	}
}

void WriteProtocolVersion(Writer& w, uint8_t version, bool force)
{
	if(force || version != ProtocolVersion::Version1)
		w.BitString(MaskToBits(version, 8), Tag::Context(0));
}

void WriteImplementationInformation(Writer& w, const std::optional<std::string>& info)
{
	if(info)
		w.String(*info, Tag::Context(TagImplementationInformation));
}

void WriteUserInformation(Writer& w, const UserInformation& ui)
{
	if(ui.empty())
		return;
	if(ui.size() > MaxExternals)
		throw std::invalid_argument("ACSE: too many EXTERNALs");
	w.Begin(Tag::Context(TagUserInformation, true));
	for(const auto& x : ui)
	{
		if(x.dataLength > 0 && x.data == nullptr)
			throw std::invalid_argument("ACSE: null EXTERNAL data");
		w.Begin(Tag{TagClass::Universal, true, ExternalTagNumber});
		if(x.directReference)
			w.ObjectIdentifier(*x.directReference);
		if(x.indirectReference)
			w.Integer(*x.indirectReference);
		if(x.encoding == ExternalEncoding::SingleAsn1Type)
		{
			CheckOneElement(x.data, x.dataLength, "single-ASN.1-type EXTERNAL data");
			w.Begin(Tag::Context(0, true));
			w.Raw(Bytes(x.data, x.data + x.dataLength));
			w.End();
		}
		else
			w.OctetString(x.data, x.dataLength, Tag::Context(1));
		w.End();
	}
	w.End();
}

void CheckNonNegative(const std::optional<int64_t>& v, const char* what)
{
	if(v && *v < 0)
		throw std::invalid_argument(std::string("ACSE: negative ") + what);
}
}

ApduType PeekApduType(const uint8_t* data, size_t length)
{
	const Element e = ParseWhole(data, length);
	if(e.tag.cls != TagClass::Application || e.tag.number > static_cast<uint32_t>(ApduType::Abrt))
		throw DecodeError("ACSE: not an ACSE APDU");
	RequireConstructed(e);
	return static_cast<ApduType>(e.tag.number);
}

Aarq DecodeAarq(const uint8_t* data, size_t length)
{
	const Element top = ParseApdu(data, length, ApduType::Aarq);
	Aarq a;
	uint64_t seen = 0;
	bool haveName = false;

	Cursor c(top);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("ACSE: unexpected element in AARQ");

		switch(e.tag.number)
		{
			case 0:
				MarkSeen(seen, 0);
				a.protocolVersion = ReadProtocolVersion(e);
				a.encodeProtocolVersion = true;
				break;
			case 1:
				MarkSeen(seen, 1);
				a.applicationContextName = ReadApplicationContextName(e);
				haveName = true;
				break;
			case 2:
				MarkSeen(seen, 2);
				a.calledApTitle = ReadApTitle(e);
				break;
			case 3:
				MarkSeen(seen, 3);
				a.calledAeQualifier = ReadAeQualifier(e);
				break;
			case 4:
				MarkSeen(seen, 4);
				a.calledApInvocationId = ReadExplicitInteger(e);
				break;
			case 5:
				MarkSeen(seen, 5);
				a.calledAeInvocationId = ReadExplicitInteger(e);
				break;
			case 6:
				MarkSeen(seen, 6);
				a.callingApTitle = ReadApTitle(e);
				break;
			case 7:
				MarkSeen(seen, 7);
				a.callingAeQualifier = ReadAeQualifier(e);
				break;
			case 8:
				MarkSeen(seen, 8);
				a.callingApInvocationId = ReadExplicitInteger(e);
				break;
			case 9:
				MarkSeen(seen, 9);
				a.callingAeInvocationId = ReadExplicitInteger(e);
				break;
			case 10:
				MarkSeen(seen, 10);
				a.authentication.acseRequirements = ReadAcseRequirements(e);
				break;
			case 11:
				MarkSeen(seen, 11);
				a.authentication.mechanismName = DecodeObjectIdentifier(e);
				break;
			case 12:
				MarkSeen(seen, 12);
				a.authentication.authenticationValue = ReadAuthenticationValue(e);
				break;
			case TagImplementationInformation:
				MarkSeen(seen, TagImplementationInformation);
				a.implementationInformation = ReadImplementationInformation(e);
				break;
			case TagUserInformation:
				MarkSeen(seen, TagUserInformation);
				a.userInformation = ReadUserInformation(e);
				break;
			default:
				break; //unknown optional field (e.g. application-context-name-list)
		}
	}
	if(!haveName)
		throw DecodeError("ACSE: AARQ without application-context-name");
	return a;
}

Aare DecodeAare(const uint8_t* data, size_t length)
{
	const Element top = ParseApdu(data, length, ApduType::Aare);
	Aare a;
	uint64_t seen = 0;
	bool haveName = false, haveResult = false, haveDiagnostic = false;

	Cursor c(top);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("ACSE: unexpected element in AARE");

		switch(e.tag.number)
		{
			case 0:
				MarkSeen(seen, 0);
				a.protocolVersion = ReadProtocolVersion(e);
				a.encodeProtocolVersion = true;
				break;
			case 1:
				MarkSeen(seen, 1);
				a.applicationContextName = ReadApplicationContextName(e);
				haveName = true;
				break;
			case 2:
			{
				MarkSeen(seen, 2);
				const int64_t r = ReadExplicitInteger(e);
				if(r < Result::Accepted || r > Result::RejectedTransient)
					throw DecodeError("ACSE: bad association result");
				a.result = r;
				haveResult = true;
				break;
			}
			case 3:
			{
				MarkSeen(seen, 3);
				//CHOICE { acse-service-user [1] INTEGER, acse-service-provider [2] INTEGER }
				const Element alt = Unwrap(e);
				if(alt.tag.Is(TagClass::ContextSpecific, 1))
					a.diagnostic.fromProvider = false;
				else if(alt.tag.Is(TagClass::ContextSpecific, 2))
					a.diagnostic.fromProvider = true;
				else
					throw DecodeError("ACSE: unknown source-diagnostic choice");
				a.diagnostic.value = ReadNonNegative(ReadExplicitInteger(alt));
				haveDiagnostic = true;
				break;
			}
			case 4:
				MarkSeen(seen, 4);
				a.respondingApTitle = ReadApTitle(e);
				break;
			case 5:
				MarkSeen(seen, 5);
				a.respondingAeQualifier = ReadAeQualifier(e);
				break;
			case 6:
				MarkSeen(seen, 6);
				a.respondingApInvocationId = ReadExplicitInteger(e);
				break;
			case 7:
				MarkSeen(seen, 7);
				a.respondingAeInvocationId = ReadExplicitInteger(e);
				break;
			case 8:
				MarkSeen(seen, 8);
				a.authentication.acseRequirements = ReadAcseRequirements(e);
				break;
			case 9:
				MarkSeen(seen, 9);
				a.authentication.mechanismName = DecodeObjectIdentifier(e);
				break;
			case 10:
				MarkSeen(seen, 10);
				a.authentication.authenticationValue = ReadAuthenticationValue(e);
				break;
			case TagImplementationInformation:
				MarkSeen(seen, TagImplementationInformation);
				a.implementationInformation = ReadImplementationInformation(e);
				break;
			case TagUserInformation:
				MarkSeen(seen, TagUserInformation);
				a.userInformation = ReadUserInformation(e);
				break;
			default:
				break; //unknown optional field (e.g. application-context-name-list)
		}
	}
	if(!haveName || !haveResult || !haveDiagnostic)
		throw DecodeError("ACSE: AARE missing a mandatory field");
	return a;
}

namespace
{
//RLRQ and RLRE have the same shape: reason [0] OPTIONAL, user-information [30] OPTIONAL
template<typename T>
T DecodeRelease(const uint8_t* data, size_t length, ApduType type)
{
	const Element top = ParseApdu(data, length, type);
	T r;
	uint64_t seen = 0;
	Cursor c(top);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("ACSE: unexpected element in release APDU");
		if(e.tag.number == 0)
		{
			MarkSeen(seen, 0);
			r.reason = ReadNonNegative(DecodeInteger(e));
		}
		else if(e.tag.number == TagUserInformation)
		{
			MarkSeen(seen, TagUserInformation);
			r.userInformation = ReadUserInformation(e);
		}
	}
	return r;
}

template<typename T>
void WriteRelease(Writer& w, const T& r, ApduType type)
{
	CheckNonNegative(r.reason, "release reason");
	w.Begin(Tag::Application(static_cast<uint32_t>(type), true));
	if(r.reason)
		w.Integer(*r.reason, Tag::Context(0));
	WriteUserInformation(w, r.userInformation);
	w.End();
}
}

Rlrq DecodeRlrq(const uint8_t* data, size_t length)
{
	return DecodeRelease<Rlrq>(data, length, ApduType::Rlrq);
}

Rlre DecodeRlre(const uint8_t* data, size_t length)
{
	return DecodeRelease<Rlre>(data, length, ApduType::Rlre);
}

Abrt DecodeAbrt(const uint8_t* data, size_t length)
{
	const Element top = ParseApdu(data, length, ApduType::Abrt);
	Abrt a;
	uint64_t seen = 0;
	bool haveSource = false;
	Cursor c(top);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.cls != TagClass::ContextSpecific)
			throw DecodeError("ACSE: unexpected element in ABRT");
		switch(e.tag.number)
		{
			case 0:
			{
				MarkSeen(seen, 0);
				const int64_t s = DecodeInteger(e);
				if(s < AbortSource::AcseServiceUser || s > AbortSource::AcseServiceProvider)
					throw DecodeError("ACSE: bad abort source");
				a.source = s;
				haveSource = true;
				break;
			}
			case 1:
				MarkSeen(seen, 1);
				a.diagnostic = ReadNonNegative(DecodeInteger(e));
				break;
			case TagUserInformation:
				MarkSeen(seen, TagUserInformation);
				a.userInformation = ReadUserInformation(e);
				break;
			default:
				break;
		}
	}
	if(!haveSource)
		throw DecodeError("ACSE: ABRT without abort-source");
	return a;
}

Bytes Encode(const Aarq& a)
{
	Writer w;
	w.Begin(Tag::Application(0, true));
	WriteProtocolVersion(w, a.protocolVersion, a.encodeProtocolVersion);
	WriteApplicationContextName(w, a.applicationContextName);
	if(a.calledApTitle)
		WriteApTitle(w, 2, *a.calledApTitle);
	if(a.calledAeQualifier)
		WriteAeQualifier(w, 3, *a.calledAeQualifier);
	WriteInvocationId(w, 4, a.calledApInvocationId);
	WriteInvocationId(w, 5, a.calledAeInvocationId);
	if(a.callingApTitle)
		WriteApTitle(w, 6, *a.callingApTitle);
	if(a.callingAeQualifier)
		WriteAeQualifier(w, 7, *a.callingAeQualifier);
	WriteInvocationId(w, 8, a.callingApInvocationId);
	WriteInvocationId(w, 9, a.callingAeInvocationId);
	WriteAuthentication(w, a.authentication, 10, 11, 12);
	WriteImplementationInformation(w, a.implementationInformation);
	WriteUserInformation(w, a.userInformation);
	w.End();
	return w.Release();
}

Bytes Encode(const Aare& a)
{
	if(a.result < Result::Accepted || a.result > Result::RejectedTransient)
		throw std::invalid_argument("ACSE: bad association result");
	if(a.diagnostic.value < 0)
		throw std::invalid_argument("ACSE: negative diagnostic");

	Writer w;
	w.Begin(Tag::Application(1, true));
	WriteProtocolVersion(w, a.protocolVersion, a.encodeProtocolVersion);
	WriteApplicationContextName(w, a.applicationContextName);
	w.Begin(Tag::Context(2, true));
	w.Integer(a.result);
	w.End();
	w.Begin(Tag::Context(3, true));
	w.Begin(Tag::Context(a.diagnostic.fromProvider ? 2u : 1u, true));
	w.Integer(a.diagnostic.value);
	w.End();
	w.End();
	if(a.respondingApTitle)
		WriteApTitle(w, 4, *a.respondingApTitle);
	if(a.respondingAeQualifier)
		WriteAeQualifier(w, 5, *a.respondingAeQualifier);
	WriteInvocationId(w, 6, a.respondingApInvocationId);
	WriteInvocationId(w, 7, a.respondingAeInvocationId);
	WriteAuthentication(w, a.authentication, 8, 9, 10);
	WriteImplementationInformation(w, a.implementationInformation);
	WriteUserInformation(w, a.userInformation);
	w.End();
	return w.Release();
}

Bytes Encode(const Rlrq& r)
{
	Writer w;
	WriteRelease(w, r, ApduType::Rlrq);
	return w.Release();
}

Bytes Encode(const Rlre& r)
{
	Writer w;
	WriteRelease(w, r, ApduType::Rlre);
	return w.Release();
}

Bytes Encode(const Abrt& a)
{
	if(a.source < AbortSource::AcseServiceUser || a.source > AbortSource::AcseServiceProvider)
		throw std::invalid_argument("ACSE: bad abort source");
	CheckNonNegative(a.diagnostic, "abort diagnostic");

	Writer w;
	w.Begin(Tag::Application(4, true));
	w.Integer(a.source, Tag::Context(0));
	if(a.diagnostic)
		w.Integer(*a.diagnostic, Tag::Context(1));
	WriteUserInformation(w, a.userInformation);
	w.End();
	return w.Release();
}

// ---- MACE authentication value

Authentication MakeMaceAuthentication(const Oid& mechanismName, const Bytes& authenticationValue)
{
	Authentication a;
	a.acseRequirements = AcseRequirement::Authentication;
	a.mechanismName = mechanismName;
	a.authenticationValue = authenticationValue;
	return a;
}

namespace
{
constexpr size_t GeneralizedTimeLength = 15; //YYYYMMDDHHMMSSZ

//days since 1970-01-01 of a proleptic Gregorian date (H. Hinnant's algorithm)
int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d)
{
	y -= m <= 2;
	const int64_t era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400);
	const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDays(int64_t z, int64_t& y, unsigned& m, unsigned& d)
{
	z += 719468;
	const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	const unsigned doe = static_cast<unsigned>(z - era * 146097);
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = static_cast<int64_t>(yoe) + era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const unsigned mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	y += m <= 2;
}

bool IsLeap(int64_t y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

unsigned DaysInMonth(int64_t y, unsigned m)
{
	static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return m == 2 && IsLeap(y) ? 29u : days[m - 1];
}
}

std::string FormatGeneralizedTime(int64_t unixSeconds)
{
	const int64_t days = unixSeconds >= 0 ? unixSeconds / 86400 : -((-unixSeconds + 86399) / 86400);
	const int64_t secs = unixSeconds - days * 86400;
	int64_t y;
	unsigned m, d;
	CivilFromDays(days, y, m, d);
	if(y < 0 || y > 9999)
		throw std::invalid_argument("ACSE: time outside the years 0000 to 9999");
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d%02u%02u%02d%02d%02dZ", static_cast<int>(y), m, d, static_cast<int>(secs / 3600),
		static_cast<int>(secs % 3600 / 60), static_cast<int>(secs % 60));
	return buf;
}

bool ParseGeneralizedTime(const std::string& text, int64_t& unixSeconds)
{
	if(text.size() != GeneralizedTimeLength || text.back() != 'Z')
		return false;
	for(size_t i = 0; i + 1 < text.size(); ++i)
		if(text[i] < '0' || text[i] > '9')
			return false;
	auto num = [&](size_t pos, size_t n)
		     {
			     int v = 0;
			     for(size_t i = 0; i < n; ++i)
				     v = v * 10 + (text[pos + i] - '0');
			     return v;
		     };
	const int y = num(0, 4), mo = num(4, 2), d = num(6, 2), h = num(8, 2), mi = num(10, 2), s = num(12, 2);
	if(mo < 1 || mo > 12 || d < 1 || d > static_cast<int>(DaysInMonth(y, static_cast<unsigned>(mo))) || h > 23 || mi > 59 || s > 59)
		return false;
	unixSeconds = DaysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400 + h * 3600 + mi * 60 + s;
	return true;
}

Bytes EncodeMaceAuthValue(const uint8_t* certificate, size_t certificateLength, const std::string& signingTime,
	const uint8_t* signature, size_t signatureLength, std::optional<int64_t> identification)
{
	if(certificate == nullptr || certificateLength == 0 || signature == nullptr || signatureLength == 0)
		throw std::invalid_argument("ACSE: MACE authentication value needs a certificate and a signature");
	int64_t unused;
	if(!ParseGeneralizedTime(signingTime, unused))
		throw std::invalid_argument("ACSE: MACE signing time is not a GeneralizedTime YYYYMMDDHHMMSSZ");
	Writer w;
	w.Begin(Tag::Context(2, true));
	if(identification)
		w.Integer(*identification);
	w.Begin(Tag::Context(0, true));
	w.Begin(Tag::Context(0, true));
	w.OctetString(certificate, certificateLength, Tag::Context(0));
	w.OctetString(reinterpret_cast<const uint8_t*>(signingTime.data()), signingTime.size(), Tag::Context(1));
	w.OctetString(signature, signatureLength, Tag::Context(2));
	w.End();
	w.End();
	w.End();
	return w.Release();
}

MaceAuthValue DecodeMaceAuthValue(const uint8_t* data, size_t length)
{
	if(length == 0 || data == nullptr)
		throw DecodeError("MACE: empty authentication value");
	const Element outer = ParseElement(data, length);
	if(outer.totalLength != length)
		throw DecodeError("MACE: trailing octets after the authentication value");
	if(!outer.tag.Is(TagClass::ContextSpecific, 2) || !outer.tag.constructed)
		throw DecodeError("MACE: the authentication value is not the external [2] alternative");

	//[2] -> [0] single-ASN1-type -> [0] -> members; each level holds exactly one element
	Element level = outer;
	for(int i = 0; i < 2; ++i)
	{
		Cursor c(level);
		if(i == 0 && c.PeekIs(UniversalTag::Integer))
			DecodeInteger(c.Next()); //the identification (the presentation context of the A-security abstract syntax): not needed
		const Element inner = c.Next();
		if(!c.Empty())
			throw DecodeError("MACE: more than one element inside the authentication value");
		if(!inner.tag.Is(TagClass::ContextSpecific, 0) || !inner.tag.constructed)
			throw DecodeError("MACE: unexpected element inside the authentication value");
		level = inner;
	}

	MaceAuthValue v;
	bool haveCert = false, haveTime = false, haveSig = false;
	Cursor c(level);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.cls != TagClass::ContextSpecific || e.tag.constructed || e.tag.number > 2)
			continue; //unknown further member
		bool& seen = e.tag.number == 0 ? haveCert : (e.tag.number == 1 ? haveTime : haveSig);
		if(seen)
			throw DecodeError("MACE: duplicate member in the authentication value");
		seen = true;
		if(e.tag.number == 0)
		{
			v.certificate = e.content;
			v.certificateLength = e.contentLength;
		}
		else if(e.tag.number == 1)
		{
			v.signingTime = e.content;
			v.signingTimeLength = e.contentLength;
		}
		else
		{
			v.signature = e.content;
			v.signatureLength = e.contentLength;
		}
	}
	if(!haveCert || !haveTime || !haveSig)
		throw DecodeError("MACE: the authentication value lacks the certificate, the signing time or the signature");
	if(v.certificateLength == 0 || v.signatureLength == 0)
		throw DecodeError("MACE: empty certificate or signature");
	return v;
}

} // namespace acse
} // namespace iccp
