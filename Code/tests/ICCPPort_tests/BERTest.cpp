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
 * BERTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the BER codec
 */

#include "BER.h"
#include <catch.hpp>
#include <cstdint>
#include <limits>
#include <string>

#define SUITE(name) "ICCP BER - " name

namespace
{
using namespace iccp::ber;

Bytes Hex(const std::string& s)
{
	Bytes out;
	int hi = -1;
	for(char c : s)
	{
		int v;
		if(c >= '0' && c <= '9') v = c - '0';
		else if(c >= 'a' && c <= 'f') v = c - 'a' + 10;
		else if(c >= 'A' && c <= 'F') v = c - 'A' + 10;
		else continue;
		if(hi < 0) hi = v;
		else
		{
			out.push_back(static_cast<uint8_t>(hi * 16 + v));
			hi = -1;
		}
	}
	return out;
}

Bytes EncodeInt(int64_t v)
{
	Writer w;
	w.Integer(v);
	return w.Release();
}

Bytes EncodeUnsigned(uint64_t v)
{
	Writer w;
	w.Unsigned(v);
	return w.Release();
}

//Parse a whole buffer and recurse into every constructed element
void Walk(const uint8_t* data, size_t len, unsigned depth = 0)
{
	if(depth > 100)
		return;
	Cursor c(data, len);
	while(!c.Empty())
	{
		const Element e = c.Next();
		if(e.tag.constructed)
			Walk(e.content, e.contentLength, depth + 1);
	}
}
}

TEST_CASE(SUITE("Integer encoding"))
{
	REQUIRE(EncodeInt(0) == Hex("02 01 00"));
	REQUIRE(EncodeInt(127) == Hex("02 01 7F"));
	REQUIRE(EncodeInt(128) == Hex("02 02 00 80"));
	REQUIRE(EncodeInt(256) == Hex("02 02 01 00"));
	REQUIRE(EncodeInt(-1) == Hex("02 01 FF"));
	REQUIRE(EncodeInt(-128) == Hex("02 01 80"));
	REQUIRE(EncodeInt(-129) == Hex("02 02 FF 7F"));
	REQUIRE(EncodeInt(std::numeric_limits<int64_t>::min()) == Hex("02 08 80 00 00 00 00 00 00 00"));
	REQUIRE(EncodeInt(std::numeric_limits<int64_t>::max()) == Hex("02 08 7F FF FF FF FF FF FF FF"));
}

TEST_CASE(SUITE("Integer round trip"))
{
	const int64_t values[] = {0, 1, -1, 127, 128, -128, -129, 255, 256, 32767, 32768, -32768, -32769,
		                    2147483647, -2147483648LL, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
	for(auto v : values)
	{
		const Bytes enc = EncodeInt(v);
		REQUIRE(DecodeInteger(ParseElement(enc.data(), enc.size())) == v);
	}
}

TEST_CASE(SUITE("Unsigned encoding and round trip"))
{
	REQUIRE(EncodeUnsigned(0) == Hex("02 01 00"));
	REQUIRE(EncodeUnsigned(127) == Hex("02 01 7F"));
	REQUIRE(EncodeUnsigned(128) == Hex("02 02 00 80"));
	REQUIRE(EncodeUnsigned(0xFFFFFFFFFFFFFFFFULL) == Hex("02 09 00 FF FF FF FF FF FF FF FF"));

	const uint64_t values[] = {0, 1, 127, 128, 255, 256, 0x7FFFFFFFULL, 0x80000000ULL, 0xFFFFFFFFULL,
		                     0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL};
	for(auto v : values)
	{
		const Bytes enc = EncodeUnsigned(v);
		REQUIRE(DecodeUnsigned(ParseElement(enc.data(), enc.size())) == v);
	}
}

TEST_CASE(SUITE("Integer decode rejects bad input"))
{
	Bytes empty = Hex("02 00");
	REQUIRE_THROWS_AS(DecodeInteger(ParseElement(empty.data(), empty.size())), DecodeError);

	Bytes tooLong = Hex("02 09 01 00 00 00 00 00 00 00 00");
	REQUIRE_THROWS_AS(DecodeInteger(ParseElement(tooLong.data(), tooLong.size())), DecodeError);

	Bytes negative = Hex("02 01 FF");
	REQUIRE_THROWS_AS(DecodeUnsigned(ParseElement(negative.data(), negative.size())), DecodeError);

	Bytes tooBig = Hex("02 09 01 00 00 00 00 00 00 00 00");
	REQUIRE_THROWS_AS(DecodeUnsigned(ParseElement(tooBig.data(), tooBig.size())), DecodeError);

	//constructed INTEGER
	Bytes constructed = Hex("22 03 02 01 05");
	REQUIRE_THROWS_AS(DecodeInteger(ParseElement(constructed.data(), constructed.size())), DecodeError);
}

TEST_CASE(SUITE("Boolean"))
{
	Writer w;
	w.Boolean(true);
	w.Boolean(false);
	REQUIRE(w.Data() == Hex("01 01 FF 01 01 00"));

	Cursor c(w.Data());
	REQUIRE(DecodeBoolean(c.Next()) == true);
	REQUIRE(DecodeBoolean(c.Next()) == false);

	Bytes bad = Hex("01 02 00 00");
	REQUIRE_THROWS_AS(DecodeBoolean(ParseElement(bad.data(), bad.size())), DecodeError);
}

TEST_CASE(SUITE("Octet and visible strings"))
{
	Writer w;
	w.OctetString(Hex("01 02 03"));
	w.String("hi");
	REQUIRE(w.Data() == Hex("04 03 01 02 03 1A 02 68 69"));

	Cursor c(w.Data());
	REQUIRE(DecodeOctetString(c.Next()) == Hex("01 02 03"));
	REQUIRE(DecodeString(c.Next()) == "hi");
}

TEST_CASE(SUITE("Length forms"))
{
	{
		Writer w;
		w.OctetString(Bytes(127, 0xAA));
		REQUIRE(w.Data().size() == 2 + 127);
		REQUIRE(w.Data()[1] == 0x7F);
	}
	{
		Writer w;
		w.OctetString(Bytes(128, 0xAA));
		REQUIRE(w.Data().size() == 3 + 128);
		REQUIRE(w.Data()[1] == 0x81);
		REQUIRE(w.Data()[2] == 0x80);
	}
	{
		Writer w;
		w.OctetString(Bytes(300, 0xAA));
		REQUIRE(w.Data().size() == 4 + 300);
		REQUIRE(w.Data()[1] == 0x82);
		REQUIRE(w.Data()[2] == 0x01);
		REQUIRE(w.Data()[3] == 0x2C);
		const Element e = ParseElement(w.Data().data(), w.Data().size());
		REQUIRE(e.contentLength == 300);
		REQUIRE(e.totalLength == 304);
	}
	{
		//non-minimal long form lengths are accepted on decode
		Bytes enc = Hex("04 82 00 02 AA BB");
		const Element e = ParseElement(enc.data(), enc.size());
		REQUIRE(e.contentLength == 2);
		REQUIRE(e.totalLength == 6);
	}
}

TEST_CASE(SUITE("Bit string"))
{
	BitString bs(10);
	REQUIRE(bs.BitCount() == 10);
	bs.Set(0, true);
	bs.Set(9, true);
	REQUIRE(bs.Get(0));
	REQUIRE_FALSE(bs.Get(1));
	REQUIRE(bs.Get(9));
	REQUIRE_FALSE(bs.Get(10));
	REQUIRE_THROWS_AS(bs.Set(10, true), std::out_of_range);

	Writer w;
	w.BitString(bs);
	REQUIRE(w.Data() == Hex("03 03 06 80 40"));

	const Element e = ParseElement(w.Data().data(), w.Data().size());
	const BitString back = DecodeBitString(e);
	REQUIRE(back.BitCount() == 10);
	REQUIRE(back.Get(0));
	REQUIRE(back.Get(9));
	REQUIRE_FALSE(back.Get(5));

	Bytes badUnused = Hex("03 02 08 00");
	REQUIRE_THROWS_AS(DecodeBitString(ParseElement(badUnused.data(), badUnused.size())), DecodeError);
	Bytes noOctet = Hex("03 00");
	REQUIRE_THROWS_AS(DecodeBitString(ParseElement(noOctet.data(), noOctet.size())), DecodeError);
	Bytes unusedOnEmpty = Hex("03 01 03");
	REQUIRE_THROWS_AS(DecodeBitString(ParseElement(unusedOnEmpty.data(), unusedOnEmpty.size())), DecodeError);
}

TEST_CASE(SUITE("Object identifier"))
{
	{
		//MMS abstract syntax {1 0 9506 2 1}
		Writer w;
		w.ObjectIdentifier({1, 0, 9506, 2, 1});
		REQUIRE(w.Data() == Hex("06 05 28 CA 22 02 01"));
		REQUIRE(DecodeObjectIdentifier(ParseElement(w.Data().data(), w.Data().size())) == std::vector<uint32_t>({1, 0, 9506, 2, 1}));
	}
	{
		//BER transfer syntax {2 1 1}
		Writer w;
		w.ObjectIdentifier({2, 1, 1});
		REQUIRE(w.Data() == Hex("06 02 51 01"));
	}
	{
		//first subidentifier needing more than one octet
		Bytes enc = Hex("06 03 81 34 03");
		REQUIRE(DecodeObjectIdentifier(ParseElement(enc.data(), enc.size())) == std::vector<uint32_t>({2, 100, 3}));
		Writer w;
		w.ObjectIdentifier({2, 100, 3});
		REQUIRE(w.Data() == enc);
	}
	{
		Writer w;
		REQUIRE_THROWS_AS(w.ObjectIdentifier({1}), std::invalid_argument);
		REQUIRE_THROWS_AS(w.ObjectIdentifier({3, 1}), std::invalid_argument);
		REQUIRE_THROWS_AS(w.ObjectIdentifier({1, 40}), std::invalid_argument);
	}

	Bytes empty = Hex("06 00");
	REQUIRE_THROWS_AS(DecodeObjectIdentifier(ParseElement(empty.data(), empty.size())), DecodeError);
	Bytes truncated = Hex("06 02 2B 86");
	REQUIRE_THROWS_AS(DecodeObjectIdentifier(ParseElement(truncated.data(), truncated.size())), DecodeError);
	Bytes nonMinimal = Hex("06 03 2B 80 01");
	REQUIRE_THROWS_AS(DecodeObjectIdentifier(ParseElement(nonMinimal.data(), nonMinimal.size())), DecodeError);
	Bytes huge = Hex("06 07 2B FF FF FF FF FF 7F");
	REQUIRE_THROWS_AS(DecodeObjectIdentifier(ParseElement(huge.data(), huge.size())), DecodeError);
}

TEST_CASE(SUITE("Null"))
{
	Writer w;
	w.Null();
	REQUIRE(w.Data() == Hex("05 00"));
	DecodeNull(ParseElement(w.Data().data(), w.Data().size()));

	Bytes bad = Hex("05 01 00");
	REQUIRE_THROWS_AS(DecodeNull(ParseElement(bad.data(), bad.size())), DecodeError);
}

TEST_CASE(SUITE("Tag forms"))
{
	{
		Writer w;
		w.Begin(Tag::Context(0, true));
		w.End();
		REQUIRE(w.Data() == Hex("A0 00"));
	}
	{
		Writer w;
		w.Integer(5, Tag::Context(2));
		REQUIRE(w.Data() == Hex("82 01 05"));
		const Element e = ParseElement(w.Data().data(), w.Data().size());
		REQUIRE(e.tag.Is(TagClass::ContextSpecific, 2));
		REQUIRE_FALSE(e.tag.constructed);
	}
	{
		Writer w;
		w.Begin(Tag::Application(1, true));
		w.End();
		REQUIRE(w.Data() == Hex("61 00"));
	}
	{
		//high tag numbers
		Writer w;
		w.Begin(Tag::Context(31, true));
		w.End();
		w.Begin(Tag::Context(128, true));
		w.End();
		w.Begin(Tag::Private(0x0FFFFFFF, true));
		w.End();
		REQUIRE(w.Data().size() == 3 + 4 + 6);
		Cursor c(w.Data());
		Element e = c.Next();
		REQUIRE(e.tag.Is(TagClass::ContextSpecific, 31));
		e = c.Next();
		REQUIRE(e.tag.Is(TagClass::ContextSpecific, 128));
		e = c.Next();
		REQUIRE(e.tag.Is(TagClass::Private, 0x0FFFFFFF));
		REQUIRE(c.Empty());
	}
	{
		Bytes nonMinimal = Hex("BF 80 01 00");
		REQUIRE_THROWS_AS(ParseElement(nonMinimal.data(), nonMinimal.size()), DecodeError);
		Bytes lowInHighForm = Hex("BF 05 00");
		REQUIRE_THROWS_AS(ParseElement(lowInHighForm.data(), lowInHighForm.size()), DecodeError);
		Bytes overflow = Hex("BF FF FF FF FF 7F 00");
		REQUIRE_THROWS_AS(ParseElement(overflow.data(), overflow.size()), DecodeError);
		Bytes truncated = Hex("BF 81");
		REQUIRE_THROWS_AS(ParseElement(truncated.data(), truncated.size()), DecodeError);
	}
}

TEST_CASE(SUITE("Nested construction"))
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	w.Integer(5);
	w.String("hi");
	w.End();
	REQUIRE(w.Data() == Hex("30 07 02 01 05 1A 02 68 69"));

	Cursor outer(w.Data());
	const Element seq = outer.Expect(UniversalTag::Sequence);
	REQUIRE(seq.tag.constructed);
	REQUIRE(outer.Empty());

	Cursor inner(seq);
	REQUIRE(DecodeInteger(inner.Expect(UniversalTag::Integer)) == 5);
	REQUIRE(DecodeString(inner.Expect(UniversalTag::VisibleString)) == "hi");
	REQUIRE(inner.Empty());
}

TEST_CASE(SUITE("Nested construction with long content"))
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	w.Begin(Tag::Context(0, true));
	w.OctetString(Bytes(200, 0x55));
	w.End();
	w.End();

	//inner: 04 81 C8 + 200 = 203 content -> A0 81 CB ; outer content 206 -> 30 81 CE
	REQUIRE(w.Data().size() == 3 + 3 + 3 + 200);
	REQUIRE(w.Data()[0] == 0x30);
	REQUIRE(w.Data()[1] == 0x81);
	REQUIRE(w.Data()[2] == 0xCE);

	Cursor c(w.Data());
	const Element seq = c.Expect(UniversalTag::Sequence);
	Cursor c2(seq);
	const Element ctx = c2.Expect(TagClass::ContextSpecific, 0);
	Cursor c3(ctx);
	REQUIRE(DecodeOctetString(c3.Expect(UniversalTag::OctetString)).size() == 200);
}

TEST_CASE(SUITE("Writer misuse"))
{
	Writer w;
	REQUIRE_THROWS_AS(w.End(), std::logic_error);
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	REQUIRE_THROWS_AS(w.Data(), std::logic_error);
	w.End();
	REQUIRE(w.Data() == Hex("30 00"));
}

TEST_CASE(SUITE("Cursor"))
{
	Bytes enc = Hex("02 01 01 02 01 02");
	Cursor c(enc);
	REQUIRE(c.PeekIs(UniversalTag::Integer));
	REQUIRE_FALSE(c.PeekIs(UniversalTag::Boolean));
	REQUIRE_THROWS_AS(c.Expect(UniversalTag::Boolean), DecodeError);
	//Expect consumes
	REQUIRE(c.Remaining() == 3);
	REQUIRE(DecodeInteger(c.Next()) == 2);
	REQUIRE(c.Empty());
	REQUIRE_FALSE(c.PeekIs(UniversalTag::Integer));
	REQUIRE_THROWS_AS(c.PeekTag(), DecodeError);
	REQUIRE_THROWS_AS(c.Next(), DecodeError);
}

TEST_CASE(SUITE("Indefinite length"))
{
	{
		Bytes enc = Hex("30 80 02 01 05 00 00 AA");
		const Element e = ParseElement(enc.data(), enc.size());
		REQUIRE(e.indefinite);
		REQUIRE(e.contentLength == 3);
		REQUIRE(e.totalLength == 7);
		Cursor c(e);
		REQUIRE(DecodeInteger(c.Next()) == 5);
		REQUIRE(c.Empty());
	}
	{
		//nested
		Bytes enc = Hex("30 80 A0 80 02 01 05 00 00 04 01 FF 00 00");
		const Element e = ParseElement(enc.data(), enc.size());
		REQUIRE(e.totalLength == enc.size());
		Walk(enc.data(), enc.size());
	}
	{
		Bytes missingEoc = Hex("30 80 02 01 05");
		REQUIRE_THROWS_AS(ParseElement(missingEoc.data(), missingEoc.size()), DecodeError);
		Bytes onPrimitive = Hex("04 80 01 00 00");
		REQUIRE_THROWS_AS(ParseElement(onPrimitive.data(), onPrimitive.size()), DecodeError);
	}
	{
		//excessive nesting is refused rather than recursing without bound
		Bytes enc;
		for(int i = 0; i < 100; ++i)
		{
			enc.push_back(0x30);
			enc.push_back(0x80);
		}
		for(int i = 0; i < 100; ++i)
		{
			enc.push_back(0x00);
			enc.push_back(0x00);
		}
		REQUIRE_THROWS_AS(ParseElement(enc.data(), enc.size()), DecodeError);
	}
}

TEST_CASE(SUITE("Malformed input"))
{
	Bytes empty;
	REQUIRE_THROWS_AS(ParseElement(empty.data(), empty.size()), DecodeError);

	Bytes noLength = Hex("04");
	REQUIRE_THROWS_AS(ParseElement(noLength.data(), noLength.size()), DecodeError);

	Bytes shortContent = Hex("04 05 01 02");
	REQUIRE_THROWS_AS(ParseElement(shortContent.data(), shortContent.size()), DecodeError);

	Bytes reserved = Hex("04 FF");
	REQUIRE_THROWS_AS(ParseElement(reserved.data(), reserved.size()), DecodeError);

	Bytes lengthOfLength = Hex("04 85 00 00 00 00 01 AA");
	REQUIRE_THROWS_AS(ParseElement(lengthOfLength.data(), lengthOfLength.size()), DecodeError);

	Bytes hugeLength = Hex("04 84 FF FF FF FF AA");
	REQUIRE_THROWS_AS(ParseElement(hugeLength.data(), hugeLength.size()), DecodeError);

	Bytes truncatedLength = Hex("04 82 01");
	REQUIRE_THROWS_AS(ParseElement(truncatedLength.data(), truncatedLength.size()), DecodeError);

	Bytes strayEoc = Hex("00 00");
	REQUIRE_THROWS_AS(ParseElement(strayEoc.data(), strayEoc.size()), DecodeError);
}

TEST_CASE(SUITE("Truncation of a valid encoding never succeeds and never crashes"))
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	w.Integer(123456);
	w.String("some string");
	w.Begin(Tag::Context(3, true));
	w.OctetString(Bytes(150, 0x11));
	w.ObjectIdentifier({1, 0, 9506, 2, 1});
	w.End();
	w.End();
	const Bytes enc = w.Release();

	REQUIRE(ParseElement(enc.data(), enc.size()).totalLength == enc.size());
	for(size_t n = 0; n < enc.size(); ++n)
		REQUIRE_THROWS_AS(ParseElement(enc.data(), n), DecodeError);
}

TEST_CASE(SUITE("Mutated encodings only ever raise DecodeError"))
{
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Sequence, true));
	w.Integer(-42);
	w.Begin(Tag::Context(1, true));
	w.String("abcdef");
	w.Boolean(true);
	w.End();
	w.OctetString(Bytes(20, 0x22));
	w.End();
	const Bytes original = w.Release();

	//deterministic pseudo random mutations
	uint32_t state = 12345;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};

	for(int iter = 0; iter < 5000; ++iter)
	{
		Bytes m = original;
		const int changes = 1 + static_cast<int>(next() % 3);
		for(int c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		if(next() % 4 == 0)
			m.resize(next() % (m.size() + 1));
		try
		{
			Walk(m.data(), m.size());
		}
		catch(const DecodeError&)
		{}
	}
}
