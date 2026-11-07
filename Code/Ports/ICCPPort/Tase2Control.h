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
 * Tase2Control.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 5 (device control) objects and their mapping to ODC: pure functions over iccp::mms types, no I/O and no timers.
 *
 *  The device model follows IEC 60870-6-503 (clauses 5.3.12 and 7.2.6) and IEC 60870-6-802 (clauses 5.2.3, 6.2.2 and 7.1.2), and the way a
 *  real peer operates devices (a capture). Only the parts that were read in the standards (T) or seen on the wire (V) are used; the few
 *  deliberate deviations are named where they are made.
 *
 *  A control device D lives in an ICC domain (or the VCC) and is two MMS named variables:
 *
 *    D       write only. The type is an INTEGER of width 16 (Command, DiscreteCommand, DiscreteSetpoint) or a FLOATING POINT of 32 bits
 *            with an exponent width of 8 (RealSetpoint). A Write is the OPERATE and its data is the BARE value, nothing else. A Read is
 *            not defined: it is refused as access denied.
 *    D_SBO   INTEGER of width 16. A Read is the SELECT: the device goes from IDLE to ARMED for its timeout and the answer is the
 *            CheckBackID that the bilateral table has for the device (a fixed, configured value that the client may check; it is not
 *            generated per select). A Write is not defined: refused as access denied.
 *
 *  Select before operate: Read D_SBO, then Write D with the value before the select timeout. A device that needs no select (non SBO)
 *  is written at any time.
 *
 *  Results are MMS data access errors of the Read (select) or the Write (operate), mapped below, never service errors.
 *
 *  Device tags (optional, IEC 60870-6-503 clauses 5.3.12.5, 5.3.12.6, 7.2.6.8 and 7.2.6.9, IEC 60870-6-802 clauses 6.2.2 and 7.1.2). A taggable device D
 *  also has the variable D_TAG (same scope and domain as D) of the type Tag_Value = STRUCTURE { Flags: bit string (tag_hi [0], tag_lo [1], tag_state [2]),
 *  Reason: VisibleString of up to 255 characters }. A Read of it is Get Tag, a Write is Set Tag. The tag value is carried in bits 0 and 1 (NO-TAG 0,
 *  OPEN-AND-CLOSE-INHIBIT 1, CLOSE-ONLY-INHIBIT 2) and the state in bit 2 (IDLE 0, ARMED 1) [T]; that tag_hi is the most significant bit of the two bit value is
 *  an assumption [S] (the text does not say). No real peer has been seen using tags: everything here is [T] or [S], never [V].
 */

#ifndef ICCP_TASE2_CONTROL_H_
#define ICCP_TASE2_CONTROL_H_

#include "Tase2.h"
#include <opendatacon/IOTypes.h>

namespace iccp
{
namespace tase2
{

//The kinds of control device (IEC 60870-6-802 clause 6.2.2 and 7.1.2: the type of the device variable)
enum class ControlType : uint8_t
{
	Command,         //Control_Command: an integer of width 16, here ON / OFF as two configured integers
	DiscreteCommand, //an integer command, here also ON / OFF (the same mapping, a different TASE.2 type name)
	RealSetpoint,    //Control_Setpoint_Real: a 32 bit floating point value
	DiscreteSetpoint //Control_Setpoint_Discrete: an integer of width 16
};
const char* ToString(ControlType type);
inline bool IsSetpoint(ControlType type)
{
	return type == ControlType::RealSetpoint || type == ControlType::DiscreteSetpoint;
}
inline bool IsRealControl(ControlType type)
{
	return type == ControlType::RealSetpoint;
}

inline constexpr const char* SelectSuffix = "_SBO"; //the select object of a device D is "D_SBO" (IEC 60870-6-503 clause 7.2.6.2.1)
inline constexpr int64_t MinCheckBackId = -32768;   //the CheckBackID is an integer of width 16 (IEC 60870-6-802 clause 6.2.2); a client may read it as signed or unsigned,
inline constexpr int64_t MaxCheckBackId = 65535;    //so any value of that width is accepted from the configuration, and no range is imposed beyond it
inline constexpr int64_t DefaultCheckBackId = 1;    //what a served device returns when none is configured

//The value of a command or a discrete setpoint is an integer of width 16 too (IEC 60870-6-802 clause 6.2.2: Control_Command, Control_Setpoint_Discrete). What we write is a
//signed integer, so the range we send is the signed one; a peer may encode the value as an unsigned integer, so the range we accept is the signed one and the unsigned one together.
inline constexpr int64_t MinControlValue = -32768;
inline constexpr int64_t MaxControlValue = 32767;
inline constexpr int64_t MaxAcceptedControlValue = 65535;

inline std::string SelectName(const std::string& device)
{
	return device + SelectSuffix;
}
//the select object next to a device (same scope and domain)
ObjectName SelectObject(const ObjectName& device);

//The value of an operate: integer for every type but RealSetpoint, real for that one
struct ControlValue
{
	int64_t integer = 0;
	double real = 0;
	bool operator==(const ControlValue&) const = default;
};

//The data of an operate Write (IEC 60870-6-503 clause 7.2.6.3.2 "List Of Data = the value of the Command"): the BARE value: Integer for Command, DiscreteCommand and
//DiscreteSetpoint, FloatingPoint of 32 bits for RealSetpoint (V: also what a real peer writes, 87 05 08 + IEEE 754 single).
Data ToData(ControlType type, const ControlValue& value);
//Parses the data of an operate: for RealSetpoint a floating point (32 or 64 bit on read), for the other types an integer or an unsigned. Throws DecodeError otherwise
//(a structure, a value of another kind, a number that is not finite or does not fit).
ControlValue ParseOperateData(ControlType type, const Data& data);
//The type of the device variable: integer of width 16, or floating point of 32 bits with an exponent width of 8 (IEC 60870-6-802 clause 6.2.2)
mms::TypeDescription DeviceTypeDescription(ControlType type);
//What the data is, for a refusal log: "a floating point", "an integer", "a structure of 3 members", ...
std::string DescribeData(const Data& data);
//What an operate of the device type takes, for a refusal log
std::string ExpectedControlData(ControlType type);

//The select object: an INTEGER CheckBackID (width 16)
Data SelectToData(int64_t checkBackId);
int64_t ParseSelectData(const Data& data);
mms::TypeDescription SelectTypeDescription();

//Requests (the client side)
//the select: a Read of the select object of the device
mms::ReadRequest BuildReadCheckBack(const ObjectName& device);
//the operate: a Write of the device with the bare value
mms::WriteRequest BuildOperate(const ObjectName& device, ControlType type, const ControlValue& value);

// ---------------------------------------------------------------------------------------------
// Device tags (optional): the Tag attribute, the variable D_TAG, Get Tag and Set Tag
// ---------------------------------------------------------------------------------------------

inline constexpr const char* TagSuffix = "_TAG";  //the tag object of a device D is "D_TAG" (IEC 60870-6-503 clause 7.2.6.8.2) [T]
inline constexpr size_t MaxTagReasonLength = 255; //Reason is a VisibleString of width 255 (IEC 60870-6-802 clause 6.2.2) [T]

//The Tag attribute of a device (IEC 60870-6-802 clause 7.1.2)
enum class DeviceTag : uint8_t
{
	NoTag = 0,               //never prevents an operation
	OpenAndCloseInhibit = 1, //prevents the select and every operate
	CloseOnlyInhibit = 2     //prevents the operate of a Close or Raise command only
};
const char* ToString(DeviceTag tag); //NO_TAG, OPEN_AND_CLOSE_INHIBIT, CLOSE_ONLY_INHIBIT
//The name of a tag as written in the configuration and in the UI commands: case blind, with '_' or '-' (NO_TAG, no-tag, ...)
std::optional<DeviceTag> DeviceTagFromString(const std::string& text);

inline std::string TagName(const std::string& device)
{
	return device + TagSuffix;
}
//the tag object next to a device (same scope and domain)
ObjectName TagObject(const ObjectName& device);

//What a Read of D_TAG returns and a Write carries. `armed` is the tag_state bit: the device is ARMED (a Write ignores it).
struct TagValue
{
	DeviceTag tag = DeviceTag::NoTag;
	bool armed = false;
	std::string reason;
	bool operator==(const TagValue&) const = default;
};
//Tag_Value as an MMS structure: { Flags: a bit string of 3 bits (tag_hi = bit 0, tag_lo = bit 1, tag_state = bit 2), Reason: a visible string }
Data TagToData(const TagValue& value);
//Throws DecodeError if the data is not a Tag_Value: not a structure of 2 members, a flags member that is not a bit string, the tag value 3 (undefined), a
//reason that is not a string or is longer than 255 characters. A bit string shorter or longer than 3 bits is accepted (missing bits are 0, extra bits ignored).
TagValue ParseTagData(const Data& data);
//The type of D_TAG: a structure of Flags (a bit string of 3 bits) and Reason (a visible string of at most 255 characters)
mms::TypeDescription TagTypeDescription();
//Get Tag: a Read of the tag object of the device
mms::ReadRequest BuildGetTag(const ObjectName& device);
//Set Tag: a Write of the tag object with the Tag_Value (the state bit is written as IDLE)
mms::WriteRequest BuildSetTag(const ObjectName& device, DeviceTag tag, const std::string& reason);
//A reason is at most 255 characters of visible ASCII (0x20 to 0x7E)
bool ValidTagReason(const std::string& reason);
//Does the tag prevent the operate of the device with this command? NO_TAG never; OPEN_AND_CLOSE_INHIBIT always; CLOSE_ONLY_INHIBIT only a Close or Raise command.
//The standard does not say which of this port's command values is a Close or Raise [S]: the configured ON value of a command device is, the OFF value is not, and a
//setpoint device is never prevented by CLOSE_ONLY_INHIBIT (a setpoint has no direction). `on` is only used for a command device.
bool TagPrevents(DeviceTag tag, ControlType type, bool on);

// ---------------------------------------------------------------------------------------------
// Results: what went wrong, as a data access error (server) and as an ODC command status (client)
// ---------------------------------------------------------------------------------------------

//Why a control request is refused (IEC 60870-6-503 clauses 7.2.6.2.3 and 7.2.6.3.3)
enum class ControlError : uint8_t
{
	NotSelected,  //an operate of a device that needs a select and is not armed (by this association): temporarily-unavailable
	Busy,         //an operation is in flight, or the device is armed already: temporarily-unavailable
	TypeMismatch, //the data does not have the device's type: type-inconsistent
	NotSupported, //the value is not one the device takes (a command value that is neither ON nor OFF): type-unsupported
	OutOfRange,   //the value is out of range: object-attribute-inconsistent
	Unknown,      //no such device: object-non-existent
	Refused,      //blocked, local, not authorised, inhibited, or an access that is not defined (a Read of the device, a Write of the select object): object-access-denied
	Hardware,     //the device or the downstream port failed or timed out, or cannot be selected: hardware-fault
	Tagged        //the select or the operate is prevented by the device's tag, or Set Tag by another client's tag: temporarily-unavailable
};
int64_t DataAccessErrorOf(ControlError e);
const char* ToString(ControlError e);
//Server side: the ODC command status that came back from downstream for an operate, as the control error (not for SUCCESS)
ControlError ControlErrorOf(odc::CommandStatus status);
//Client side: the ODC command status for a data access error of a Write or a Read (see the README for the table)
odc::CommandStatus CommandStatusOf(int64_t dataAccessError);

//Command devices: ON / OFF for an ODC control code (nullopt: not supported)
std::optional<bool> CommandOnOff(odc::ControlCode code);

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_CONTROL_H_ */
