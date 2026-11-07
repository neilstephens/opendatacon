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
 * BER.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ASN.1 Basic Encoding Rules (ITU-T X.690) codec.
 *
 *  Decoding is defensive: every read is bounds checked and any malformed input
 *  results in a DecodeError (the input is untrusted network data).
 *  Decoders supply views into the caller's buffer, which must outlive the views.
 */

#ifndef ICCP_BER_H_
#define ICCP_BER_H_

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace iccp
{
namespace ber
{

using Bytes = std::vector<uint8_t>;

class DecodeError: public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

enum class TagClass : uint8_t
{
	Universal = 0,
	Application = 1,
	ContextSpecific = 2,
	Private = 3
};

enum class UniversalTag : uint32_t
{
	Boolean = 1,
	Integer = 2,
	BitString = 3,
	OctetString = 4,
	Null = 5,
	ObjectIdentifier = 6,
	Enumerated = 10,
	Utf8String = 12,
	Sequence = 16,
	Set = 17,
	UtcTime = 23,
	GeneralizedTime = 24,
	VisibleString = 26
};

struct Tag
{
	TagClass cls = TagClass::Universal;
	bool constructed = false;
	uint32_t number = 0;

	static Tag Universal(UniversalTag n, bool constructed = false)
	{
		return Tag{TagClass::Universal, constructed, static_cast<uint32_t>(n)};
	}
	static Tag Application(uint32_t n, bool constructed = false)
	{
		return Tag{TagClass::Application, constructed, n};
	}
	static Tag Context(uint32_t n, bool constructed = false)
	{
		return Tag{TagClass::ContextSpecific, constructed, n};
	}
	static Tag Private(uint32_t n, bool constructed = false)
	{
		return Tag{TagClass::Private, constructed, n};
	}

	//match on class and number only; implicit/explicit tagging decides primitive vs constructed
	bool Is(TagClass c, uint32_t n) const
	{
		return cls == c && number == n;
	}
	bool Is(UniversalTag n) const
	{
		return Is(TagClass::Universal, static_cast<uint32_t>(n));
	}
	bool operator==(const Tag& rhs) const
	{
		return cls == rhs.cls && constructed == rhs.constructed && number == rhs.number;
	}
	bool operator!=(const Tag& rhs) const
	{
		return !(*this == rhs);
	}
};

//A decoded tag-length-value, viewing the underlying buffer
struct Element
{
	Tag tag;
	const uint8_t* content = nullptr;
	size_t contentLength = 0;
	//header + content (+ end-of-contents octets for indefinite length)
	size_t totalLength = 0;
	bool indefinite = false;
};

//Parse one element from the start of the buffer. Throws DecodeError.
//Supports definite and indefinite (constructed only) lengths.
Element ParseElement(const uint8_t* data, size_t length, unsigned depth = 0);

//Sequential reader over the elements in a buffer (or in a constructed element's content)
class Cursor
{
public:
	Cursor(const uint8_t* data, size_t length):
		pos(data), remaining(length)
	{}
	explicit Cursor(const Element& e):
		pos(e.content), remaining(e.contentLength)
	{}
	explicit Cursor(const Bytes& b):
		pos(b.data()), remaining(b.size())
	{}

	bool Empty() const
	{
		return remaining == 0;
	}
	size_t Remaining() const
	{
		return remaining;
	}

	//Throws if empty or malformed
	Tag PeekTag() const;
	//False if empty or malformed
	bool PeekIs(TagClass c, uint32_t n) const;
	bool PeekIs(UniversalTag n) const
	{
		return PeekIs(TagClass::Universal, static_cast<uint32_t>(n));
	}

	//Consume the next element. Throws if empty or malformed.
	Element Next();
	//Consume the next element, throwing unless it matches the class and number.
	Element Expect(TagClass c, uint32_t n);
	Element Expect(UniversalTag n)
	{
		return Expect(TagClass::Universal, static_cast<uint32_t>(n));
	}

private:
	const uint8_t* pos;
	size_t remaining;
};

//ASN.1 BIT STRING. Bit 0 is the most significant bit of the first octet.
struct BitString
{
	Bytes data;
	uint8_t unusedBits = 0;

	BitString() = default;
	//zero-filled string of the given length in bits
	explicit BitString(size_t bitCount);

	size_t BitCount() const
	{
		return data.size() * 8 - unusedBits;
	}
	bool operator==(const BitString&) const = default;
	//false for indexes beyond BitCount()
	bool Get(size_t index) const;
	//throws std::out_of_range for indexes beyond BitCount()
	void Set(size_t index, bool value);
};

//Primitive value decoders. All throw DecodeError on constructed encodings or bad content.
bool DecodeBoolean(const Element& e);
int64_t DecodeInteger(const Element& e);
uint64_t DecodeUnsigned(const Element& e);
Bytes DecodeOctetString(const Element& e);
std::string DecodeString(const Element& e);
BitString DecodeBitString(const Element& e);
std::vector<uint32_t> DecodeObjectIdentifier(const Element& e);
void DecodeNull(const Element& e);

//Builds a BER encoding. Constructed elements are written with Begin()/End() pairs
//(definite length is back-filled at End()).
//The tag argument on the primitive writers allows implicit tagging.
class Writer
{
public:
	void Begin(Tag tag);
	void End();

	void Boolean(bool value, Tag tag = Tag::Universal(UniversalTag::Boolean));
	void Integer(int64_t value, Tag tag = Tag::Universal(UniversalTag::Integer));
	void Unsigned(uint64_t value, Tag tag = Tag::Universal(UniversalTag::Integer));
	void OctetString(const uint8_t* data, size_t length, Tag tag = Tag::Universal(UniversalTag::OctetString));
	void OctetString(const Bytes& data, Tag tag = Tag::Universal(UniversalTag::OctetString));
	void String(const std::string& value, Tag tag = Tag::Universal(UniversalTag::VisibleString));
	void BitString(const ber::BitString& value, Tag tag = Tag::Universal(UniversalTag::BitString));
	void ObjectIdentifier(const std::vector<uint32_t>& arcs, Tag tag = Tag::Universal(UniversalTag::ObjectIdentifier));
	void Null(Tag tag = Tag::Universal(UniversalTag::Null));
	//primitive with caller-encoded content
	void Primitive(Tag tag, const uint8_t* data, size_t length);
	//append an already encoded element
	void Raw(const Bytes& encoded);

	//The encoding so far. Every Begin() must have been closed with End().
	const Bytes& Data() const;
	Bytes Release();

private:
	Bytes buffer;
	std::vector<size_t> openStarts;
};

} //namespace ber
} //namespace iccp

#endif /* ICCP_BER_H_ */
