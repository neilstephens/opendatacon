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
 * Tase2Blocked.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 3, blocked transfers, see Tase2Blocked.h.
 */

#include "Tase2Blocked.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace iccp
{
namespace tase2
{
namespace blocked
{

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

void Put16(Bytes& out, uint64_t v)
{
	out.push_back(static_cast<uint8_t>(v >> 8));
	out.push_back(static_cast<uint8_t>(v));
}
void Put32(Bytes& out, uint64_t v)
{
	for(int shift = 24; shift >= 0; shift -= 8)
		out.push_back(static_cast<uint8_t>(v >> shift));
}
int64_t Required(const std::optional<int64_t>& v, const char* what)
{
	if(!v)
		throw std::invalid_argument(std::string("TASE.2 blocked data: point value is missing its ") + what);
	return *v;
}

//reads from a bounded buffer: every read is checked
struct Reader
{
	const uint8_t* data;
	size_t length;
	size_t pos = 0;
	size_t Left() const
	{
		return length - pos;
	}
	uint8_t U8()
	{
		if(Left() < 1)
			throw DecodeError("TASE.2 blocked data: truncated");
		return data[pos++];
	}
	uint32_t U16()
	{
		const uint32_t hi = U8();
		return (hi << 8) | U8();
	}
	uint32_t U32()
	{
		uint32_t v = 0;
		for(int i = 0; i < 4; ++i)
			v = (v << 8) | U8();
		return v;
	}
};

//the conditions octet is the content octet of the bit string: bit i of the mask is bit i of the string, bit 0 the most significant
uint8_t ConditionsOctet(uint8_t mask)
{
	uint8_t octet = 0;
	for(int i = 0; i < 8; ++i)
		if(mask & (1u << i))
			octet = static_cast<uint8_t>(octet | (0x80u >> i));
	return octet;
}
uint8_t ConditionsMask(uint8_t octet)
{
	uint8_t mask = 0;
	for(int i = 0; i < 8; ++i)
		if(octet & (0x80u >> i))
			mask = static_cast<uint8_t>(mask | (1u << i));
	return mask;
}

void PutId(Bytes& out, const std::string& id, const char* what)
{
	if(id.size() > NameIdWidth)
		throw std::invalid_argument(std::string("TASE.2 blocked data: the ") + what + " \"" + id + "\" is longer than " + std::to_string(NameIdWidth) + " octets, the width of its storage form");
	out.insert(out.end(), id.begin(), id.end());
	out.insert(out.end(), NameIdWidth - id.size(), 0);
}
std::string TakeId(const uint8_t* p)
{
	size_t n = 0;
	while(n < NameIdWidth && p[n] != 0)
		++n;
	return std::string(reinterpret_cast<const char*>(p), n);
}
}

const char* ToString(Rule rule)
{
	switch(rule)
	{
		case Rule::NoRbe: return "rule 0 (no RBE)";
		case Rule::Offset1: return "rule 1 (Offset1)";
		case Rule::Offset2: return "rule 2 (Offset2)";
	}
	return "unknown rule";
}

size_t SpecialWidth(Special special)
{
	switch(special)
	{
		case Special::TransferSetName: return NameStorageWidth;
		case Special::TimeStamp: return 4;
		case Special::ConditionsDetected: return 1;
		case Special::EventCode: return 2;
	}
	return 0;
}

void AppendSpecial(Bytes& out, Special special, const SpecialValue& v)
{
	switch(special)
	{
		case Special::TransferSetName:
		{
			Bytes name;
			name.push_back(static_cast<uint8_t>(v.transferSet.scope));
			PutId(name, v.transferSet.domain, "domain");
			PutId(name, v.transferSet.name, "transfer set name");
			out.insert(out.end(), name.begin(), name.end());
			break;
		}
		case Special::TimeStamp:
			Put32(out, static_cast<uint64_t>(v.number));
			break;
		case Special::EventCode:
			Put16(out, static_cast<uint64_t>(v.number));
			break;
		case Special::ConditionsDetected:
			out.push_back(ConditionsOctet(v.conditions));
			break;
	}
}

SpecialValue ReadSpecial(const uint8_t* data, size_t length, Special special)
{
	if(length != SpecialWidth(special))
		throw DecodeError("TASE.2 blocked data: this special object is " + std::to_string(SpecialWidth(special)) + " octets, " + std::to_string(length) + " given");
	SpecialValue v;
	switch(special)
	{
		case Special::TransferSetName:
			if(data[0] > 1)
				throw DecodeError("TASE.2 blocked data: the scope octet of a Transfer_Set_Name is " + std::to_string(data[0]) + ", not 0 (VMD) or 1 (domain)");
			v.transferSet.scope = data[0] == 1 ? Scope::Icc : Scope::Vcc;
			v.transferSet.domain = TakeId(data + 1);
			v.transferSet.name = TakeId(data + 1 + NameIdWidth);
			break;
		case Special::TimeStamp:
		{
			Reader r{data, length};
			v.number = static_cast<int64_t>(r.U32());
			break;
		}
		case Special::EventCode:
		{
			Reader r{data, length};
			v.number = static_cast<int16_t>(r.U16());
			break;
		}
		case Special::ConditionsDetected:
			v.conditions = ConditionsMask(data[0]);
			break;
	}
	return v;
}

size_t ValueWidth(PointType type)
{
	switch(type)
	{
		case PointType::Real: return 4;
		case PointType::RealQ: return 5;
		case PointType::RealQTimeTag: return 9;
		case PointType::RealExtended: return 13;
		case PointType::State: return 1;
		case PointType::StateQ: return 1;
		case PointType::StateQTimeTag: return 5;
		case PointType::StateExtended: return 9;
		case PointType::Discrete: return 4;
		case PointType::DiscreteQ: return 5;
		case PointType::DiscreteQTimeTag: return 9;
		case PointType::DiscreteExtended: return 13;
	}
	return 0;
}

size_t MemberWidth(const Member& m)
{
	return m.special ? SpecialWidth(*m.special) : ValueWidth(m.type);
}

void AppendValue(Bytes& out, const PointValue& p)
{
	const PointType t = p.type;
	if(t == PointType::State)
	{
		out.push_back(static_cast<uint8_t>(static_cast<uint8_t>(p.state) << 6));
		return;
	}
	if(HasTimeTag(t))
		Put32(out, static_cast<uint64_t>(Required(p.timeStamp, "time stamp")));
	else if(IsExtended(t))
	{
		Put32(out, static_cast<uint64_t>(Required(p.timeStamp, "time stamp")));
		Put16(out, static_cast<uint64_t>(Required(p.milliseconds, "milliseconds")));
	}
	if(IsRealType(t))
	{
		const float f = static_cast<float>(p.real);
		uint32_t bits = 0;
		static_assert(sizeof(bits) == sizeof(f));
		std::memcpy(&bits, &f, sizeof(bits));
		Put32(out, bits);
	}
	else if(IsDiscreteType(t))
		Put32(out, static_cast<uint64_t>(p.discrete));
	if(t != PointType::Real && t != PointType::Discrete)
		out.push_back(p.flags);
	if(IsExtended(t))
		Put16(out, static_cast<uint64_t>(Required(p.cov, "COV")));
}

PointValue ReadValue(const uint8_t* data, size_t length, PointType type)
{
	if(length != ValueWidth(type))
		throw DecodeError("TASE.2 blocked data: a value of this point type is " + std::to_string(ValueWidth(type)) + " octets, " + std::to_string(length) + " given");
	Reader r{data, length};
	PointValue p;
	p.type = type;
	if(type == PointType::State)
	{
		const uint8_t octet = r.U8();
		if((octet & 0x3F) != 0)
			throw DecodeError("TASE.2 blocked data: a State value has bits set below the 2 bit state");
		p.state = static_cast<StateValue>(octet >> 6);
		p.flags = octet;
		return p;
	}
	if(HasTimeTag(type))
		p.timeStamp = static_cast<int64_t>(r.U32());
	else if(IsExtended(type))
	{
		p.timeStamp = static_cast<int64_t>(r.U32());
		p.milliseconds = static_cast<int64_t>(r.U16());
	}
	if(IsRealType(type))
	{
		const uint32_t bits = r.U32();
		float f = 0;
		std::memcpy(&f, &bits, sizeof(f));
		p.real = f;
	}
	else if(IsDiscreteType(type))
		p.discrete = static_cast<int32_t>(r.U32());
	if(type != PointType::Real && type != PointType::Discrete)
	{
		p.flags = r.U8();
		if(IsStateType(type))
			p.state = UnpackFlags(p.flags).state;
	}
	if(IsExtended(type))
		p.cov = static_cast<int64_t>(r.U16());
	return p;
}

Item ObjectItem(size_t position, Special special, const SpecialValue& value)
{
	Item item;
	item.position = position;
	item.special = special;
	item.object = value;
	return item;
}

Rule ChooseRule(bool rbe, size_t largestPosition)
{
	if(!rbe)
		return Rule::NoRbe;
	return largestPosition <= 0xFF ? Rule::Offset1 : Rule::Offset2;
}

Bytes Encode(Rule rule, const std::vector<Item>& items)
{
	Bytes out;
	out.push_back(static_cast<uint8_t>(rule));
	for(const auto& item : items)
	{
		switch(rule)
		{
			case Rule::NoRbe:
				break;
			case Rule::Offset1:
				if(item.position > 0xFF)
					throw std::invalid_argument("TASE.2 blocked data: position " + std::to_string(item.position) + " does not fit rule 1 (one octet)");
				out.push_back(static_cast<uint8_t>(item.position));
				break;
			case Rule::Offset2:
				if(item.position > 0xFFFF)
					throw std::invalid_argument("TASE.2 blocked data: position " + std::to_string(item.position) + " does not fit rule 2 (two octets)");
				Put16(out, item.position);
				break;
		}
		if(item.special)
			AppendSpecial(out, *item.special, item.object);
		else
			AppendValue(out, item.value);
	}
	return out;
}

Bytes Encode(bool rbe, const std::vector<Item>& items)
{
	size_t largest = 0;
	for(const auto& item : items)
		largest = std::max(largest, item.position);
	return Encode(ChooseRule(rbe, largest), items);
}

Decoded Decode(const Bytes& data, const std::vector<Member>& members)
{
	if(data.empty())
		throw DecodeError("TASE.2 blocked data: empty, the RBE blocking rule octet is missing");
	Decoded out;
	const uint8_t ruleOctet = data[0];
	if(ruleOctet > static_cast<uint8_t>(Rule::Offset2))
		throw DecodeError("TASE.2 blocked data: unknown RBE blocking rule " + std::to_string(ruleOctet) + " (the rules are 0, 1 and 2)");
	out.rule = static_cast<Rule>(ruleOctet);
	size_t pos = 1;
	auto valueAt = [&](size_t position, const char* what)
			   {
				   const Member& m = members[position];
				   const size_t width = MemberWidth(m);
				   if(data.size() - pos < width)
					   throw DecodeError(std::string("TASE.2 blocked data: truncated, ") + what + " " + std::to_string(position) + " needs " + std::to_string(width) + " octets and " +
						   std::to_string(data.size() - pos) + " are left");
				   Item item;
				   item.position = position;
				   if(m.special)
				   {
					   item.special = m.special;
					   item.object = ReadSpecial(data.data() + pos, width, *m.special);
				   }
				   else
					   item.value = ReadValue(data.data() + pos, width, m.type);
				   pos += width;
				   out.items.push_back(std::move(item));
			   };
	if(out.rule == Rule::NoRbe)
	{
		for(size_t i = 0; i < members.size(); ++i)
			valueAt(i, "the value of member");
		if(pos != data.size())
			throw DecodeError("TASE.2 blocked data: " + std::to_string(data.size() - pos) + " leftover octets after the last of the " + std::to_string(members.size()) + " members");
		return out;
	}
	const size_t positionWidth = out.rule == Rule::Offset1 ? 1 : 2;
	while(pos < data.size())
	{
		if(data.size() - pos < positionWidth)
			throw DecodeError("TASE.2 blocked data: truncated, a position of " + std::to_string(positionWidth) + " octets needs more than the " + std::to_string(data.size() - pos) + " left");
		size_t position = data[pos];
		if(positionWidth == 2)
			position = (position << 8) | data[pos + 1];
		pos += positionWidth;
		if(position >= members.size())
			throw DecodeError("TASE.2 blocked data: position " + std::to_string(position) + " is outside the data set, which has " + std::to_string(members.size()) + " members");
		if(out.items.size() >= members.size())
			throw DecodeError("TASE.2 blocked data: count mismatch, more values than the data set's " + std::to_string(members.size()) + " members");
		valueAt(position, "the value at position");
	}
	return out;
}

std::string DisplayName(const mms::ObjectName& name)
{
	return name.scope == mms::ObjectScope::DomainSpecific ? name.domainId + "/" + name.itemId : name.itemId;
}

std::string HexDump(const Bytes& data, size_t maxOctets)
{
	static const char digits[] = "0123456789ABCDEF";
	std::string out;
	const size_t n = std::min(data.size(), maxOctets);
	for(size_t i = 0; i < n; ++i)
	{
		if(i != 0)
			out += ' ';
		out += digits[data[i] >> 4];
		out += digits[data[i] & 0x0F];
	}
	if(data.size() > n)
		out += " ... (" + std::to_string(data.size() - n) + " more)";
	return out;
}

namespace
{
std::optional<Special> SpecialOfItem(const std::string& item)
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

std::optional<BlockedReport> RecognizeBlockedReport(const mms::InformationReport& report)
{
	const auto* list = std::get_if<mms::ListOfVariable>(&report.specification);
	if(list == nullptr || list->empty() || list->size() != report.results.size())
		return std::nullopt;
	size_t others = 0;
	size_t dataIndex = 0;
	for(size_t i = 0; i < list->size(); ++i)
		if(!SpecialOfItem((*list)[i].name.itemId))
		{
			++others;
			dataIndex = i;
		}
	if(others != 1)
		return std::nullopt;
	const mms::AccessResult& data = report.results[dataIndex];
	if(!data.success || data.data.type != mms::DataType::OctetString)
		return std::nullopt;

	BlockedReport out;
	out.dataName = (*list)[dataIndex].name;
	try
	{
		out.data = data.data.AsOctetString();
	}
	catch(const DecodeError&)
	{
		return std::nullopt;
	}
	for(const auto& v : *list)
		out.variables.push_back(DisplayName(v.name));
	bool haveName = false;
	try
	{
		for(size_t i = 0; i < list->size(); ++i)
		{
			if(i == dataIndex)
				continue;
			const auto special = SpecialOfItem((*list)[i].name.itemId);
			if(!special || std::find(out.layout.begin(), out.layout.end(), *special) != out.layout.end())
				continue; //a special object listed twice: the first counts
			if(!report.results[i].success)
				throw DecodeError("the special object " + (*list)[i].name.itemId + " failed with a data access error");
			const Data& d = report.results[i].data;
			switch(*special)
			{
				case Special::TransferSetName:
					out.transferSet = ParseObjectRef(d);
					haveName = true;
					break;
				case Special::TimeStamp: out.timeStamp = ParseTransferSetTimeStamp(d); break;
				case Special::ConditionsDetected: out.conditionsDetected = ParseDsConditions(d); break;
				case Special::EventCode: out.eventCode = ParseEventCode(d); break;
			}
			out.layout.push_back(*special);
		}
		if(!haveName)
			throw DecodeError("the report has no Transfer_Set_Name, so its transfer set is not known");
	}
	catch(const DecodeError& e)
	{
		out.problem = e.what();
	}
	return out;
}

} // namespace blocked
} // namespace tase2
} // namespace iccp
