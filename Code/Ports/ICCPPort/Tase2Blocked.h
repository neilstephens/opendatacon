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
 * Tase2Blocked.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 3, blocked transfers: the BlockData attribute of the DSTransmissionPars (IEC 60870-6-503 clause 7.2.5.5.3).
 *
 *  Pure functions over bytes and MMS types: no I/O, no state.
 *
 *  Provenance: [T] the text of the standard, [V] seen in a capture of a real peer's reports (a real peer as the SERVER, rule 0, RealQ points, three special objects at the head
 *  of the data set), [S] this port's reading of what neither gives. What is [V]:
 *   - the report: a list of variables with the special objects as domain-specific variables (domain of the transfer set) in data set order, then ONE VMD-specific variable
 *     named Block_Data; the results are the special objects' values and one octet string. The special objects are therefore BOTH separate variables AND inside the octet string.
 *   - the octet string: the rule octet (0), then EVERY member of the data set in data set order, the special transfer objects included, each in a fixed width storage form:
 *     Transfer_Set_Name 65 octets (scope 1, domain 32 and item 32 octets, zero padded), Transfer_Set_Time_Stamp 4 octets (unsigned seconds, big endian), DSConditions_Detected
 *     1 octet (the content octet of the bit string, bit 0 the most significant), Event_Code_Detected 2 octets (the standard types it Integer16; only the value 0 was seen), a StateQ point
 *     1 octet, a RealQ point 5 octets (a big endian IEEE 754 single and the flags octet). No octet is left over.
 *  What is [V] (continued): a PARTIAL report of an RBE set is RULE 1: the rule octet, then (1 octet position, value) pairs of the changed points ONLY, no special objects in the string; the position
 *  is the 0-based index in the data set's FULL member list, the special objects counted (three special objects: the first point is position 3); a RealQ value is a float and the flags. SEVERAL
 *  changed points make several pairs in one report, in ascending position order. A COMPLETE report of an RBE set (the integrity reports) is RULE 0 like any complete report.
 *  What is [S] (never seen): the byte order and sign of a non zero Event_Code_Detected (big endian two's complement, Integer16), rule 2 (rule octet 2, then the same pairs with 2 octet unsigned
 *  positions, for a position above 255: the standard's text gives the rule number and the width, not the byte order, taken as big endian like every other number here), and the octets of every other
 *  point type (ValueWidth, AppendValue, ReadValue). Each is in ONE place so that a later capture can correct it.
 *  All pairs of a report use the one rule (the standard has one rule octet per string): when one position is above 255 every position of that report is 2 octets (ChooseRule). Positions above 65535
 *  fit no rule (Encode refuses them).
 *  This port's server sends rule 2 unless the option BlockedRule2 is false (see ResponderSession::BuildBlockedValues); the client decodes rules 1 and 2 leniently, a position below the number of
 *  special objects being a special object whose storage form is read and ignored.
 *
 *  The standard [T]: the octet string starts with a one octet RBE blocking rule: 0 (no RBE): the value of every variable of the data set, in order, octet aligned; 1 (Offset1, RBE)
 *  and 2 (Offset2, RBE): for each variable reported, its position in the data set's variable list (1 or 2 octets, unsigned, counted from 0) followed by its value, octet aligned. With
 *  RBE the server may choose rule 1 or 2 and the most compact is recommended.
 */

#ifndef ICCP_TASE2_BLOCKED_H_
#define ICCP_TASE2_BLOCKED_H_

#include "Tase2.h"

namespace iccp
{
namespace tase2
{
namespace blocked
{

//The item id of the variable that carries the blocked octet string [V]. It is a VMD-specific variable (no domain) [V]. The server's option BlockedDataName overrides the id;
//a client takes the octet string whatever the variable is called and in whatever scope.
inline constexpr const char* DefaultVariableName = "Block_Data";

//How many octets of the blocked data the diagnostics show
inline constexpr size_t DiagnosticOctets = 96;

//The RBE blocking rule: the first octet of the blocked data [T]. Rule 0 (a complete report) and rule 1 (what changed, one or several points) have been seen [V].
enum class Rule : uint8_t
{
	NoRbe = 0,   //every member of the data set in order, values only, the special objects included: a complete report [V]
	Offset1 = 1, //(position (1 octet), value) pairs of the changed points only [V, one or several points]
	Offset2 = 2  //the same with 2 octet positions [S]
};
const char* ToString(Rule rule);

//One member of the data set as the layout sees it: a special transfer object or a point of a type
struct Member
{
	std::optional<Special> special;
	PointType type = PointType::RealQ; //a point
	static Member Object(Special s)
	{
		Member m;
		m.special = s;
		return m;
	}
	static Member Point(PointType t)
	{
		Member m;
		m.type = t;
		return m;
	}
	bool operator==(const Member&) const = default;
};

//The storage form of a special transfer object. Transfer_Set_Name [V]: scope (1 octet, 1 for a domain-specific name, 0 for a VMD-specific one), the domain id (32 octets) and the item id
//(32 octets), ASCII, zero padded = 65 octets. Transfer_Set_Time_Stamp [V]: 4 octets, unsigned seconds, big endian. DSConditions_Detected [V]: 1 octet, the content octet of the bit string
//(bit 0, IntervalTimeOut, is the most significant). Event_Code_Detected [V]: 2 octets (the standard types it Integer16); only the value 0 was seen, so [S]: big endian, two's complement.
inline constexpr size_t NameIdWidth = 32;
inline constexpr size_t NameStorageWidth = 1 + 2 * NameIdWidth;
size_t SpecialWidth(Special special);
struct SpecialValue
{
	ObjectRef transferSet;  //Transfer_Set_Name
	int64_t number = 0;     //Transfer_Set_Time_Stamp, Event_Code_Detected
	uint8_t conditions = 0; //DSConditions_Detected: the mask of DsConditions (bit i of the mask is bit i of the string)
	bool operator==(const SpecialValue&) const = default;
};
//Append the storage form. Throws std::invalid_argument if a name part is longer than 32 octets. A time stamp that does not fit 32 bits, or an event code that does not fit 16, is cut to it.
void AppendSpecial(Bytes& out, Special special, const SpecialValue& value);
//Read one special object from exactly SpecialWidth(special) octets. Throws DecodeError for another length or a name whose scope octet is neither 0 nor 1.
SpecialValue ReadSpecial(const uint8_t* data, size_t length, Special special);

//The octets of one point value, by point type [V for RealQ, S for the others]. Numbers are big endian (network order), the components of a structure follow each other in the order this
//port already uses on the wire for the type (time stamp, [milliseconds,] value, flags, [COV]); the flags and the state are the one octet of the 8 bit string (state in the top two bits).
//  Real 4 | RealQ 5 | RealQTimeTag 9 | RealExtended 13 | State 1 | StateQ 1 | StateQTimeTag 5 | StateExtended 9 | Discrete 4 | DiscreteQ 5 | DiscreteQTimeTag 9 | DiscreteExtended 13
//Value: IEEE 754 single (Real types) or two's complement 32 bit (Discrete types). Time stamp: unsigned 32 bit seconds since 1970. Milliseconds and COV: unsigned 16 bit.
size_t ValueWidth(PointType type);
//Append the octets of a value. A value that does not fit its field is cut to the field (time stamp and discrete to 32 bits, milliseconds and COV to 16). Throws std::invalid_argument for a
//value without a part its type needs (as ToData does).
void AppendValue(Bytes& out, const PointValue& value);
//Read one value of the given type from exactly ValueWidth(type) octets. Throws DecodeError if the octets are not that many or a State value has bits set that are not the state.
PointValue ReadValue(const uint8_t* data, size_t length, PointType type);

size_t MemberWidth(const Member& member);

//One member of a report: its position in the data set's FULL member list (0-based, the special objects counted [V]; rule 0 has no positions) and its value:
//a special object (special set, the value in `object`) or a point (the value in `value`)
struct Item
{
	size_t position = 0;
	PointValue value;
	std::optional<Special> special;
	SpecialValue object;
	bool operator==(const Item&) const = default;
};
Item ObjectItem(size_t position, Special special, const SpecialValue& value);

//Rule 1 when every position of a partial report fits one octet (at most 255), else rule 2 [rule 2 S]; rule 0 when `rbe` is false (a complete report). The server calls ChooseRule(true, ...) for a partial report.
Rule ChooseRule(bool rbe, size_t largestPosition);
//The blocked octet string. Rule 0: the items are the whole data set in order, special objects included (their positions are not written). Rules 1 and 2: position and value for each
//item, in the order given. Throws std::invalid_argument if a position does not fit the rule.
Bytes Encode(Rule rule, const std::vector<Item>& items);
//The same, with the rule chosen by ChooseRule from the largest position of the items
Bytes Encode(bool rbe, const std::vector<Item>& items);

struct Decoded
{
	Rule rule = Rule::NoRbe;
	std::vector<Item> items;
};
//Read the octet string given the members of the data set (the special objects and the points, in data set order). Throws DecodeError, with the reason in the text, for: no rule octet,
//an unknown rule, truncated values, leftover octets after the last member (rule 0), a position outside the data set (rules 1 and 2), and more values than the data set has members
//(rules 1 and 2). Nothing is returned for a block that fails: the caller must not apply any of it.
Decoded Decode(const Bytes& data, const std::vector<Member>& members);

//A transfer report seen from the client in its blocked form
struct BlockedReport
{
	ObjectRef transferSet;
	std::optional<int64_t> timeStamp;
	std::optional<DsConditions> conditionsDetected;
	std::optional<int64_t> eventCode;
	std::vector<Special> layout;        //the special objects the report carried
	mms::ObjectName dataName;           //the variable that carried the octet string, whatever its name or scope
	Bytes data;                         //the octet string
	std::vector<std::string> variables; //every variable of the report, in order, as "domain/item" or "item", for the diagnostics
	std::string problem;                //not empty: the special objects could not be read (no Transfer_Set_Name, a failed result, a bad value): the report is not usable
};
//Recognise a blocked report: an InformationReport that lists its variables, with exactly one variable that is not a special transfer object (by item name, in any scope and in any position)
//whose result is an octet string. Lenient on receipt: the name and scope of that variable are not checked (a real peer's is VMD-specific), and the special objects may come in any order.
//nullopt = this is not a blocked report (an ordinary report is handled as before). Never throws.
std::optional<BlockedReport> RecognizeBlockedReport(const mms::InformationReport& report);

//The first `maxOctets` octets as hex, with the count of what is left out, for the diagnostics
std::string HexDump(const Bytes& data, size_t maxOctets = DiagnosticOctets);
//"domain/item" for a domain-specific name, "item" for a vmd-specific one
std::string DisplayName(const mms::ObjectName& name);

} // namespace blocked
} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_BLOCKED_H_ */
