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
 * Tase2Control.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 5 objects and result mapping, see Tase2Control.h (the model follows IEC 60870-6-503 / -802 and a real peer's behaviour).
 */

#include "Tase2Control.h"
#include <cctype>
#include <cmath>

namespace iccp
{
namespace tase2
{

const char* ToString(ControlType type)
{
	switch(type)
	{
		case ControlType::Command: return "Command";
		case ControlType::DiscreteCommand: return "DiscreteCommand";
		case ControlType::RealSetpoint: return "RealSetpoint";
		case ControlType::DiscreteSetpoint: return "DiscreteSetpoint";
	}
	return "?";
}

ObjectName SelectObject(const ObjectName& device)
{
	ObjectName n = device;
	n.itemId = SelectName(device.itemId);
	return n;
}

namespace
{
int64_t IntegerOf(const Data& d, const char* what)
{
	if(d.type == mms::DataType::Integer)
		return d.AsInteger();
	if(d.type == mms::DataType::Unsigned)
	{
		const uint64_t u = d.AsUnsigned();
		if(u > static_cast<uint64_t>(INT64_MAX))
			throw DecodeError(std::string("TASE.2 control: ") + what + " is out of range");
		return static_cast<int64_t>(u);
	}
	throw DecodeError(std::string("TASE.2 control: ") + what + " is not an integer");
}
}

Data ToData(ControlType type, const ControlValue& value)
{
	return IsRealControl(type) ? Data::Float32(static_cast<float>(value.real)) : Data::Integer(value.integer);
}

ControlValue ParseOperateData(ControlType type, const Data& data)
{
	//the bare value, nothing else: a structure (or any other shape) is not an operate
	ControlValue d;
	if(IsRealControl(type))
	{
		if(data.type != mms::DataType::FloatingPoint)
			throw DecodeError("TASE.2 control: a setpoint value is a floating point number");
		d.real = data.AsFloat();
		if(!std::isfinite(d.real))
			throw DecodeError("TASE.2 control: a setpoint value is not a finite number");
	}
	else
		d.integer = IntegerOf(data, "the value");
	return d;
}

std::string DescribeData(const Data& data)
{
	switch(data.type)
	{
		case mms::DataType::Array: return "an array of " + std::to_string(data.elements.size()) + " members";
		case mms::DataType::Structure: return "a structure of " + std::to_string(data.elements.size()) + " members";
		case mms::DataType::Boolean: return "a boolean";
		case mms::DataType::BitString: return "a bit string";
		case mms::DataType::Integer: return "an integer";
		case mms::DataType::Unsigned: return "an unsigned integer";
		case mms::DataType::FloatingPoint: return "a floating point";
		case mms::DataType::OctetString: return "an octet string";
		case mms::DataType::VisibleString: return "a visible string";
		case mms::DataType::GeneralizedTime: return "a generalized time";
		case mms::DataType::BinaryTime: return "a binary time";
		case mms::DataType::Bcd: return "a BCD value";
		case mms::DataType::BooleanArray: return "a boolean array";
		case mms::DataType::ObjectId: return "an object identifier";
		case mms::DataType::MmsString: return "an MMS string";
		case mms::DataType::UtcTime: return "a UTC time";
	}
	return "unknown data";
}

std::string ExpectedControlData(ControlType type)
{
	return std::string("a bare value (") + (IsRealControl(type) ? "a floating point" : "an integer") + ")";
}

mms::TypeDescription DeviceTypeDescription(ControlType type)
{
	using Kind = mms::TypeDescription::Kind;
	mms::TypeDescription t;
	if(IsRealControl(type))
	{
		t.kind = Kind::FloatingPoint;
		t.formatWidth = 32;
		t.exponentWidth = 8;
	}
	else
	{
		t.kind = Kind::Integer;
		t.size = 16;
	}
	return t;
}

Data SelectToData(int64_t checkBackId)
{
	return Data::Integer(checkBackId);
}

int64_t ParseSelectData(const Data& data)
{
	return IntegerOf(data, "the CheckBackID");
}

mms::TypeDescription SelectTypeDescription()
{
	mms::TypeDescription t;
	t.kind = mms::TypeDescription::Kind::Integer;
	t.size = 16;
	return t;
}

mms::ReadRequest BuildReadCheckBack(const ObjectName& device)
{
	mms::ReadRequest r;
	r.specification = mms::ListOfVariable{mms::VariableSpec{SelectObject(device), std::nullopt}
	};
	return r;
}

mms::WriteRequest BuildOperate(const ObjectName& device, ControlType type, const ControlValue& value)
{
	mms::WriteRequest w;
	w.specification = mms::ListOfVariable{mms::VariableSpec{device, std::nullopt}
	};
	w.data.push_back(ToData(type, value));
	return w;
}

// ---------------------------------------------------------------------------------------------
// Device tags
// ---------------------------------------------------------------------------------------------

const char* ToString(DeviceTag tag)
{
	switch(tag)
	{
		case DeviceTag::NoTag: return "NO_TAG";
		case DeviceTag::OpenAndCloseInhibit: return "OPEN_AND_CLOSE_INHIBIT";
		case DeviceTag::CloseOnlyInhibit: return "CLOSE_ONLY_INHIBIT";
	}
	return "?";
}

std::optional<DeviceTag> DeviceTagFromString(const std::string& text)
{
	std::string t;
	for(char c : text)
		t += c == '-' ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	for(DeviceTag tag : {DeviceTag::NoTag, DeviceTag::OpenAndCloseInhibit, DeviceTag::CloseOnlyInhibit})
		if(t == ToString(tag))
			return tag;
	return std::nullopt;
}

ObjectName TagObject(const ObjectName& device)
{
	ObjectName n = device;
	n.itemId = TagName(device.itemId);
	return n;
}

bool ValidTagReason(const std::string& reason)
{
	if(reason.size() > MaxTagReasonLength)
		return false;
	for(char c : reason)
		if(static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E)
			return false;
	return true;
}

Data TagToData(const TagValue& value)
{
	//tag_hi (bit 0) is the most significant bit of the two bit tag value [S]
	const unsigned v = static_cast<unsigned>(value.tag);
	mms::BitString flags(3);
	flags.Set(0, (v & 2) != 0);
	flags.Set(1, (v & 1) != 0);
	flags.Set(2, value.armed);
	return Data::Structure({Data::Bits(flags), Data::VisibleString(value.reason)});
}

TagValue ParseTagData(const Data& data)
{
	if(data.type != mms::DataType::Structure || data.elements.size() != 2)
		throw DecodeError("TASE.2 tag: a Tag_Value is a structure of the flags and the reason");
	const Data& f = data.elements[0];
	const Data& r = data.elements[1];
	if(f.type != mms::DataType::BitString)
		throw DecodeError("TASE.2 tag: the flags of a Tag_Value are a bit string");
	if(r.type != mms::DataType::VisibleString && r.type != mms::DataType::MmsString)
		throw DecodeError("TASE.2 tag: the reason of a Tag_Value is a visible string");
	const mms::BitString flags = f.AsBits();
	TagValue t;
	const unsigned v = (flags.Get(0) ? 2u : 0u) | (flags.Get(1) ? 1u : 0u);
	if(v > 2)
		throw DecodeError("TASE.2 tag: the tag value 3 (both tag bits set) is not defined");
	t.tag = static_cast<DeviceTag>(v);
	t.armed = flags.Get(2);
	t.reason = r.AsString();
	if(t.reason.size() > MaxTagReasonLength)
		throw DecodeError("TASE.2 tag: the reason is longer than " + std::to_string(MaxTagReasonLength) + " characters");
	return t;
}

mms::TypeDescription TagTypeDescription()
{
	using Kind = mms::TypeDescription::Kind;
	mms::TypeDescription flags;
	flags.kind = Kind::BitString;
	flags.size = 3;
	mms::TypeDescription reason;
	reason.kind = Kind::VisibleString;
	reason.size = -static_cast<int64_t>(MaxTagReasonLength); //varying length, at most 255
	mms::TypeDescription t;
	t.kind = Kind::Structure;
	t.subtypes = {flags, reason};
	t.componentNames = {std::string("Flags"), std::string("Reason")};
	return t;
}

mms::ReadRequest BuildGetTag(const ObjectName& device)
{
	mms::ReadRequest r;
	r.specification = mms::ListOfVariable{mms::VariableSpec{TagObject(device), std::nullopt}
	};
	return r;
}

mms::WriteRequest BuildSetTag(const ObjectName& device, DeviceTag tag, const std::string& reason)
{
	mms::WriteRequest w;
	w.specification = mms::ListOfVariable{mms::VariableSpec{TagObject(device), std::nullopt}
	};
	TagValue v;
	v.tag = tag;
	v.reason = reason;
	w.data.push_back(TagToData(v));
	return w;
}

bool TagPrevents(DeviceTag tag, ControlType type, bool on)
{
	switch(tag)
	{
		case DeviceTag::NoTag: return false;
		case DeviceTag::OpenAndCloseInhibit: return true;
		case DeviceTag::CloseOnlyInhibit: return !IsSetpoint(type) && on;
	}
	return false;
}

int64_t DataAccessErrorOf(ControlError e)
{
	switch(e)
	{
		case ControlError::NotSelected: return mms::DataAccessError::TemporarilyUnavailable;
		case ControlError::Busy: return mms::DataAccessError::TemporarilyUnavailable;
		case ControlError::TypeMismatch: return mms::DataAccessError::TypeInconsistent;
		case ControlError::NotSupported: return mms::DataAccessError::TypeUnsupported;
		case ControlError::OutOfRange: return mms::DataAccessError::ObjectAttributeInconsistent;
		case ControlError::Unknown: return mms::DataAccessError::ObjectNonExistent;
		case ControlError::Refused: return mms::DataAccessError::ObjectAccessDenied;
		case ControlError::Hardware: return mms::DataAccessError::HardwareFault;
		case ControlError::Tagged: return mms::DataAccessError::TemporarilyUnavailable;
	}
	return mms::DataAccessError::HardwareFault;
}

const char* ToString(ControlError e)
{
	switch(e)
	{
		case ControlError::NotSelected: return "not selected";
		case ControlError::Busy: return "busy";
		case ControlError::TypeMismatch: return "wrong type of value";
		case ControlError::NotSupported: return "value not supported";
		case ControlError::OutOfRange: return "out of range";
		case ControlError::Unknown: return "no such device";
		case ControlError::Refused: return "refused";
		case ControlError::Hardware: return "hardware failure";
		case ControlError::Tagged: return "tagged";
	}
	return "?";
}

ControlError ControlErrorOf(odc::CommandStatus status)
{
	using CS = odc::CommandStatus;
	switch(status)
	{
		case CS::NO_SELECT: return ControlError::NotSelected;
		case CS::FORMAT_ERROR: return ControlError::TypeMismatch;
		case CS::NOT_SUPPORTED: return ControlError::NotSupported;
		case CS::OUT_OF_RANGE: return ControlError::OutOfRange;
		case CS::ALREADY_ACTIVE:
		case CS::TOO_MANY_OPS:
		case CS::PROCESSING_LIMITED: return ControlError::Busy;
		case CS::LOCAL:
		case CS::DOWNSTREAM_LOCAL:
		case CS::NOT_AUTHORIZED:
		case CS::AUTOMATION_INHIBIT:
		case CS::BLOCKED:
		case CS::BLOCKED_OTHER_MASTER:
		case CS::NON_PARTICIPATING: return ControlError::Refused;
		default: return ControlError::Hardware; //TIMEOUT, HARDWARE_ERROR, DOWNSTREAM_FAIL, CANCELLED, ALREADY_COMPLETE, UNDEFINED, ...
	}
}

odc::CommandStatus CommandStatusOf(int64_t dataAccessError)
{
	using CS = odc::CommandStatus;
	namespace DA = mms::DataAccessError;
	switch(dataAccessError)
	{
		case DA::ObjectInvalidated: return CS::NO_SELECT;
		case DA::HardwareFault: return CS::HARDWARE_ERROR;
		case DA::TemporarilyUnavailable: return CS::ALREADY_ACTIVE;
		case DA::ObjectAccessDenied: return CS::BLOCKED;
		case DA::ObjectUndefined:
		case DA::InvalidAddress:
		case DA::TypeUnsupported:
		case DA::ObjectAccessUnsupported:
		case DA::ObjectNonExistent: return CS::NOT_SUPPORTED;
		case DA::TypeInconsistent:
		case DA::ObjectValueInvalid: return CS::FORMAT_ERROR;
		case DA::ObjectAttributeInconsistent: return CS::OUT_OF_RANGE;
		default: return CS::UNDEFINED;
	}
}

std::optional<bool> CommandOnOff(odc::ControlCode code)
{
	using CC = odc::ControlCode;
	switch(code)
	{
		case CC::LATCH_ON:
		case CC::PULSE_ON:
		case CC::CLOSE_PULSE_ON: return true;
		case CC::LATCH_OFF:
		case CC::PULSE_OFF:
		case CC::TRIP_PULSE_ON: return false;
		default: return std::nullopt;
	}
}

} // namespace tase2
} // namespace iccp
