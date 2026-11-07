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
 * BER.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ASN.1 Basic Encoding Rules (ITU-T X.690) codec.
 */

#include "BER.h"
#include <limits>
#include <utility>

namespace iccp
{
namespace ber
{

namespace
{
//Bounds the nesting of indefinite-length elements, which are scanned recursively
constexpr unsigned MaxIndefiniteDepth = 64;
//Largest length we will write (and the largest we will read is bounded by the buffer)
constexpr size_t MaxEncodableLength = 0x7FFFFFFF;

Tag DecodeTag(const uint8_t* data, size_t length, size_t& pos)
{
	if(length == 0)
		throw DecodeError("BER: truncated tag");

	const uint8_t first = data[0];
	Tag tag;
	tag.cls = static_cast<TagClass>(first >> 6);
	tag.constructed = (first & 0x20) != 0;
	tag.number = first & 0x1F;
	pos = 1;

	if(tag.number == 0x1F)
	{
		//high tag number form: base 128, continuation bit in the MSB
		uint32_t number = 0;
		unsigned count = 0;
		for(;;)
		{
			if(pos >= length)
				throw DecodeError("BER: truncated high tag number");
			const uint8_t b = data[pos++];
			if(count == 0 && b == 0x80)
				throw DecodeError("BER: non-minimal high tag number");
			if(++count > 5 || number > (std::numeric_limits<uint32_t>::max() >> 7))
				throw DecodeError("BER: tag number too large");
			number = (number << 7) | (b & 0x7F);
			if((b & 0x80) == 0)
				break;
		}
		if(number < 0x1F)
			throw DecodeError("BER: high tag form used for low tag number");
		tag.number = number;
	}
	return tag;
}

void AppendTag(Bytes& out, const Tag& tag, bool constructed)
{
	const uint8_t lead = static_cast<uint8_t>((static_cast<uint8_t>(tag.cls) << 6) | (constructed ? 0x20 : 0));
	if(tag.number < 0x1F)
	{
		out.push_back(static_cast<uint8_t>(lead | tag.number));
		return;
	}
	out.push_back(static_cast<uint8_t>(lead | 0x1F));
	uint8_t groups[5];
	size_t n = 0;
	uint32_t v = tag.number;
	do
	{
		groups[n++] = static_cast<uint8_t>(v & 0x7F);
		v >>= 7;
	} while(v != 0);
	while(n > 1)
		out.push_back(static_cast<uint8_t>(groups[--n] | 0x80));
	out.push_back(groups[0]);
}

Bytes EncodeLength(size_t length)
{
	if(length > MaxEncodableLength)
		throw std::length_error("BER: element too long to encode");
	Bytes out;
	if(length < 0x80)
	{
		out.push_back(static_cast<uint8_t>(length));
		return out;
	}
	uint8_t octets[4];
	size_t n = 0;
	while(length != 0)
	{
		octets[n++] = static_cast<uint8_t>(length & 0xFF);
		length >>= 8;
	}
	out.push_back(static_cast<uint8_t>(0x80 | n));
	while(n > 0)
		out.push_back(octets[--n]);
	return out;
}

void AppendBase128(Bytes& out, uint64_t v)
{
	uint8_t groups[10];
	size_t n = 0;
	do
	{
		groups[n++] = static_cast<uint8_t>(v & 0x7F);
		v >>= 7;
	} while(v != 0);
	while(n > 1)
		out.push_back(static_cast<uint8_t>(groups[--n] | 0x80));
	out.push_back(groups[0]);
}
} //namespace

Element ParseElement(const uint8_t* data, size_t length, unsigned depth)
{
	if(depth > MaxIndefiniteDepth)
		throw DecodeError("BER: indefinite length nesting too deep");

	size_t pos = 0;
	Element e;
	e.tag = DecodeTag(data, length, pos);

	if(e.tag.cls == TagClass::Universal && !e.tag.constructed && e.tag.number == 0)
		throw DecodeError("BER: unexpected end-of-contents");

	if(pos >= length)
		throw DecodeError("BER: truncated length");
	const uint8_t lenByte = data[pos++];

	if(lenByte < 0x80)
	{
		e.contentLength = lenByte;
	}
	else if(lenByte == 0x80)
	{
		if(!e.tag.constructed)
			throw DecodeError("BER: indefinite length on primitive element");
		e.indefinite = true;
	}
	else if(lenByte == 0xFF)
	{
		throw DecodeError("BER: reserved length octet");
	}
	else
	{
		const size_t n = lenByte & 0x7F;
		if(n > 4)
			throw DecodeError("BER: length too large");
		if(length - pos < n)
			throw DecodeError("BER: truncated length");
		size_t len = 0;
		for(size_t i = 0; i < n; ++i)
			len = (len << 8) | data[pos++];
		e.contentLength = len;
	}

	if(!e.indefinite)
	{
		if(e.contentLength > length - pos)
			throw DecodeError("BER: content exceeds buffer");
		e.content = data + pos;
		e.totalLength = pos + e.contentLength;
		return e;
	}

	//indefinite: walk the children to find the end-of-contents octets
	size_t p = pos;
	for(;;)
	{
		if(length - p < 2)
			throw DecodeError("BER: missing end-of-contents");
		if(data[p] == 0 && data[p + 1] == 0)
			break;
		const Element child = ParseElement(data + p, length - p, depth + 1);
		p += child.totalLength;
	}
	e.content = data + pos;
	e.contentLength = p - pos;
	e.totalLength = p + 2;
	return e;
}

Tag Cursor::PeekTag() const
{
	if(remaining == 0)
		throw DecodeError("BER: no more elements");
	size_t pos_unused = 0;
	return DecodeTag(pos, remaining, pos_unused);
}

bool Cursor::PeekIs(TagClass c, uint32_t n) const
{
	if(remaining == 0)
		return false;
	try
	{
		return PeekTag().Is(c, n);
	}
	catch(const DecodeError&)
	{
		return false;
	}
}

Element Cursor::Next()
{
	if(remaining == 0)
		throw DecodeError("BER: no more elements");
	const Element e = ParseElement(pos, remaining);
	pos += e.totalLength;
	remaining -= e.totalLength;
	return e;
}

Element Cursor::Expect(TagClass c, uint32_t n)
{
	const Element e = Next();
	if(!e.tag.Is(c, n))
		throw DecodeError("BER: unexpected tag " + std::to_string(static_cast<unsigned>(e.tag.cls)) + "/" + std::to_string(e.tag.number)
			+ ", expected " + std::to_string(static_cast<unsigned>(c)) + "/" + std::to_string(n));
	return e;
}

BitString::BitString(size_t bitCount):
	data((bitCount + 7) / 8, 0),
	unusedBits(static_cast<uint8_t>((8 - bitCount % 8) % 8))
{}

bool BitString::Get(size_t index) const
{
	if(index >= BitCount())
		return false;
	return (data[index / 8] & (0x80 >> (index % 8))) != 0;
}

void BitString::Set(size_t index, bool value)
{
	if(index >= BitCount())
		throw std::out_of_range("BitString index out of range");
	const uint8_t mask = static_cast<uint8_t>(0x80 >> (index % 8));
	if(value)
		data[index / 8] |= mask;
	else
		data[index / 8] &= static_cast<uint8_t>(~mask);
}

namespace
{
void RequirePrimitive(const Element& e, const char* what)
{
	if(e.tag.constructed)
		throw DecodeError(std::string("BER: constructed encoding of ") + what + " not supported");
}
}

bool DecodeBoolean(const Element& e)
{
	RequirePrimitive(e, "BOOLEAN");
	if(e.contentLength != 1)
		throw DecodeError("BER: BOOLEAN must be one octet");
	return e.content[0] != 0;
}

int64_t DecodeInteger(const Element& e)
{
	RequirePrimitive(e, "INTEGER");
	if(e.contentLength == 0 || e.contentLength > 8)
		throw DecodeError("BER: INTEGER length unsupported");
	uint64_t v = (e.content[0] & 0x80) ? ~uint64_t(0) : 0; //sign extend
	for(size_t i = 0; i < e.contentLength; ++i)
		v = (v << 8) | e.content[i];
	return static_cast<int64_t>(v);
}

uint64_t DecodeUnsigned(const Element& e)
{
	RequirePrimitive(e, "INTEGER");
	if(e.contentLength == 0 || e.contentLength > 9)
		throw DecodeError("BER: unsigned INTEGER length unsupported");
	if(e.content[0] & 0x80)
		throw DecodeError("BER: unsigned INTEGER is negative");
	size_t i = 0;
	if(e.contentLength == 9)
	{
		if(e.content[0] != 0)
			throw DecodeError("BER: unsigned INTEGER exceeds 64 bits");
		i = 1;
	}
	uint64_t v = 0;
	for(; i < e.contentLength; ++i)
		v = (v << 8) | e.content[i];
	return v;
}

Bytes DecodeOctetString(const Element& e)
{
	RequirePrimitive(e, "OCTET STRING");
	return Bytes(e.content, e.content + e.contentLength);
}

std::string DecodeString(const Element& e)
{
	RequirePrimitive(e, "string");
	return std::string(reinterpret_cast<const char*>(e.content), e.contentLength);
}

BitString DecodeBitString(const Element& e)
{
	RequirePrimitive(e, "BIT STRING");
	if(e.contentLength == 0)
		throw DecodeError("BER: BIT STRING missing unused-bits octet");
	BitString bs;
	bs.unusedBits = e.content[0];
	if(bs.unusedBits > 7 || (e.contentLength == 1 && bs.unusedBits != 0))
		throw DecodeError("BER: BIT STRING bad unused-bits count");
	bs.data.assign(e.content + 1, e.content + e.contentLength);
	return bs;
}

std::vector<uint32_t> DecodeObjectIdentifier(const Element& e)
{
	RequirePrimitive(e, "OBJECT IDENTIFIER");
	if(e.contentLength == 0)
		throw DecodeError("BER: empty OBJECT IDENTIFIER");

	std::vector<uint64_t> subs;
	uint64_t v = 0;
	bool inSub = false;
	for(size_t i = 0; i < e.contentLength; ++i)
	{
		const uint8_t b = e.content[i];
		if(!inSub && b == 0x80)
			throw DecodeError("BER: non-minimal OID subidentifier");
		if(v > (std::numeric_limits<uint64_t>::max() >> 7))
			throw DecodeError("BER: OID subidentifier too large");
		v = (v << 7) | (b & 0x7F);
		inSub = true;
		if((b & 0x80) == 0)
		{
			subs.push_back(v);
			v = 0;
			inSub = false;
		}
	}
	if(inSub)
		throw DecodeError("BER: truncated OID subidentifier");

	std::vector<uint32_t> arcs;
	const uint64_t first = subs[0];
	uint64_t a0 = first < 40 ? 0 : (first < 80 ? 1 : 2);
	uint64_t a1 = first - a0 * 40;
	if(a1 > std::numeric_limits<uint32_t>::max())
		throw DecodeError("BER: OID arc too large");
	arcs.push_back(static_cast<uint32_t>(a0));
	arcs.push_back(static_cast<uint32_t>(a1));
	for(size_t i = 1; i < subs.size(); ++i)
	{
		if(subs[i] > std::numeric_limits<uint32_t>::max())
			throw DecodeError("BER: OID arc too large");
		arcs.push_back(static_cast<uint32_t>(subs[i]));
	}
	return arcs;
}

void DecodeNull(const Element& e)
{
	RequirePrimitive(e, "NULL");
	if(e.contentLength != 0)
		throw DecodeError("BER: NULL must be empty");
}

void Writer::Begin(Tag tag)
{
	AppendTag(buffer, tag, true);
	openStarts.push_back(buffer.size());
}

void Writer::End()
{
	if(openStarts.empty())
		throw std::logic_error("BER Writer: End() without Begin()");
	const size_t start = openStarts.back();
	openStarts.pop_back();
	const Bytes len = EncodeLength(buffer.size() - start);
	buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(start), len.begin(), len.end());
}

void Writer::Primitive(Tag tag, const uint8_t* data, size_t length)
{
	AppendTag(buffer, tag, false);
	const Bytes len = EncodeLength(length);
	buffer.insert(buffer.end(), len.begin(), len.end());
	if(length != 0)
		buffer.insert(buffer.end(), data, data + length);
}

void Writer::Boolean(bool value, Tag tag)
{
	const uint8_t b = value ? 0xFF : 0x00;
	Primitive(tag, &b, 1);
}

void Writer::Integer(int64_t value, Tag tag)
{
	uint8_t octets[8];
	const uint64_t u = static_cast<uint64_t>(value);
	for(size_t i = 0; i < 8; ++i)
		octets[i] = static_cast<uint8_t>(u >> (56 - 8 * i));
	size_t start = 0;
	//strip redundant sign octets
	while(start < 7 &&
	      ((octets[start] == 0x00 && (octets[start + 1] & 0x80) == 0) ||
	       (octets[start] == 0xFF && (octets[start + 1] & 0x80) != 0)))
		++start;
	Primitive(tag, octets + start, 8 - start);
}

void Writer::Unsigned(uint64_t value, Tag tag)
{
	uint8_t octets[9];
	octets[0] = 0;
	for(size_t i = 0; i < 8; ++i)
		octets[i + 1] = static_cast<uint8_t>(value >> (56 - 8 * i));
	size_t start = 0;
	while(start < 8 && octets[start] == 0x00 && (octets[start + 1] & 0x80) == 0)
		++start;
	Primitive(tag, octets + start, 9 - start);
}

void Writer::OctetString(const uint8_t* data, size_t length, Tag tag)
{
	Primitive(tag, data, length);
}

void Writer::OctetString(const Bytes& data, Tag tag)
{
	Primitive(tag, data.data(), data.size());
}

void Writer::String(const std::string& value, Tag tag)
{
	Primitive(tag, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

void Writer::BitString(const ber::BitString& value, Tag tag)
{
	Bytes content;
	content.reserve(value.data.size() + 1);
	content.push_back(value.data.empty() ? 0 : value.unusedBits);
	content.insert(content.end(), value.data.begin(), value.data.end());
	Primitive(tag, content.data(), content.size());
}

void Writer::ObjectIdentifier(const std::vector<uint32_t>& arcs, Tag tag)
{
	if(arcs.size() < 2 || arcs[0] > 2 || (arcs[0] < 2 && arcs[1] > 39))
		throw std::invalid_argument("BER Writer: invalid OBJECT IDENTIFIER");
	Bytes content;
	AppendBase128(content, static_cast<uint64_t>(arcs[0]) * 40 + arcs[1]);
	for(size_t i = 2; i < arcs.size(); ++i)
		AppendBase128(content, arcs[i]);
	Primitive(tag, content.data(), content.size());
}

void Writer::Null(Tag tag)
{
	Primitive(tag, nullptr, 0);
}

void Writer::Raw(const Bytes& encoded)
{
	buffer.insert(buffer.end(), encoded.begin(), encoded.end());
}

const Bytes& Writer::Data() const
{
	if(!openStarts.empty())
		throw std::logic_error("BER Writer: unclosed Begin()");
	return buffer;
}

Bytes Writer::Release()
{
	if(!openStarts.empty())
		throw std::logic_error("BER Writer: unclosed Begin()");
	openStarts.clear();
	return std::move(buffer);
}

} //namespace ber
} //namespace iccp
