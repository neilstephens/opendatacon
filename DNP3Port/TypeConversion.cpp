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
 * DNP3Port/TypeConversion.cpp
 *
 *  Created on: 24/06/2018
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "TypeConversion.h"
#include <opendatacon/IOTypes.h>
#include <opendnp3/app/AnalogOutput.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>
#include <opendnp3/app/MeasurementTypes.h>

namespace odc
{

CommandStatus ToODC(const opendnp3::CommandStatus dnp3)
{
	CommandStatus stat;
	switch(dnp3)
	{
		case opendnp3::CommandStatus::SUCCESS:
			stat = CommandStatus::SUCCESS;
			break;
		case opendnp3::CommandStatus::TIMEOUT:
			stat = CommandStatus::TIMEOUT;
			break;
		case opendnp3::CommandStatus::NO_SELECT:
			stat = CommandStatus::NO_SELECT;
			break;
		case opendnp3::CommandStatus::FORMAT_ERROR:
			stat = CommandStatus::FORMAT_ERROR;
			break;
		case opendnp3::CommandStatus::NOT_SUPPORTED:
			stat = CommandStatus::NOT_SUPPORTED;
			break;
		case opendnp3::CommandStatus::ALREADY_ACTIVE:
			stat = CommandStatus::ALREADY_ACTIVE;
			break;
		case opendnp3::CommandStatus::HARDWARE_ERROR:
			stat = CommandStatus::HARDWARE_ERROR;
			break;
		case opendnp3::CommandStatus::LOCAL:
			stat = CommandStatus::LOCAL;
			break;
		case opendnp3::CommandStatus::TOO_MANY_OPS:
			stat = CommandStatus::TOO_MANY_OPS;
			break;
		case opendnp3::CommandStatus::NOT_AUTHORIZED:
			stat = CommandStatus::NOT_AUTHORIZED;
			break;
		case opendnp3::CommandStatus::AUTOMATION_INHIBIT:
			stat = CommandStatus::AUTOMATION_INHIBIT;
			break;
		case opendnp3::CommandStatus::PROCESSING_LIMITED:
			stat = CommandStatus::PROCESSING_LIMITED;
			break;
		case opendnp3::CommandStatus::OUT_OF_RANGE:
			stat = CommandStatus::OUT_OF_RANGE;
			break;
		case opendnp3::CommandStatus::DOWNSTREAM_LOCAL:
			stat = CommandStatus::DOWNSTREAM_LOCAL;
			break;
		case opendnp3::CommandStatus::ALREADY_COMPLETE:
			stat = CommandStatus::ALREADY_COMPLETE;
			break;
		case opendnp3::CommandStatus::BLOCKED:
			stat = CommandStatus::BLOCKED;
			break;
		case opendnp3::CommandStatus::CANCELLED:
			stat = CommandStatus::CANCELLED;
			break;
		case opendnp3::CommandStatus::BLOCKED_OTHER_MASTER:
			stat = CommandStatus::BLOCKED_OTHER_MASTER;
			break;
		case opendnp3::CommandStatus::DOWNSTREAM_FAIL:
			stat = CommandStatus::DOWNSTREAM_FAIL;
			break;
		case opendnp3::CommandStatus::NON_PARTICIPATING:
			stat = CommandStatus::NON_PARTICIPATING;
			break;
		case opendnp3::CommandStatus::UNDEFINED:
		default:
			stat = CommandStatus::UNDEFINED;
			break;
	}
	return stat;
}

opendnp3::CommandStatus FromODC(const CommandStatus stat)
{
	opendnp3::CommandStatus dnp3;
	switch(stat)
	{
		case CommandStatus::SUCCESS:
			dnp3 = opendnp3::CommandStatus::SUCCESS;
			break;
		case CommandStatus::TIMEOUT:
			dnp3 = opendnp3::CommandStatus::TIMEOUT;
			break;
		case CommandStatus::NO_SELECT:
			dnp3 = opendnp3::CommandStatus::NO_SELECT;
			break;
		case CommandStatus::FORMAT_ERROR:
			dnp3 = opendnp3::CommandStatus::FORMAT_ERROR;
			break;
		case CommandStatus::NOT_SUPPORTED:
			dnp3 = opendnp3::CommandStatus::NOT_SUPPORTED;
			break;
		case CommandStatus::ALREADY_ACTIVE:
			dnp3 = opendnp3::CommandStatus::ALREADY_ACTIVE;
			break;
		case CommandStatus::HARDWARE_ERROR:
			dnp3 = opendnp3::CommandStatus::HARDWARE_ERROR;
			break;
		case CommandStatus::LOCAL:
			dnp3 = opendnp3::CommandStatus::LOCAL;
			break;
		case CommandStatus::TOO_MANY_OPS:
			dnp3 = opendnp3::CommandStatus::TOO_MANY_OPS;
			break;
		case CommandStatus::NOT_AUTHORIZED:
			dnp3 = opendnp3::CommandStatus::NOT_AUTHORIZED;
			break;
		case CommandStatus::AUTOMATION_INHIBIT:
			dnp3 = opendnp3::CommandStatus::AUTOMATION_INHIBIT;
			break;
		case CommandStatus::PROCESSING_LIMITED:
			dnp3 = opendnp3::CommandStatus::PROCESSING_LIMITED;
			break;
		case CommandStatus::OUT_OF_RANGE:
			dnp3 = opendnp3::CommandStatus::OUT_OF_RANGE;
			break;
		case CommandStatus::DOWNSTREAM_LOCAL:
			dnp3 = opendnp3::CommandStatus::DOWNSTREAM_LOCAL;
			break;
		case CommandStatus::ALREADY_COMPLETE:
			dnp3 = opendnp3::CommandStatus::ALREADY_COMPLETE;
			break;
		case CommandStatus::BLOCKED:
			dnp3 = opendnp3::CommandStatus::BLOCKED;
			break;
		case CommandStatus::CANCELLED:
			dnp3 = opendnp3::CommandStatus::CANCELLED;
			break;
		case CommandStatus::BLOCKED_OTHER_MASTER:
			dnp3 = opendnp3::CommandStatus::BLOCKED_OTHER_MASTER;
			break;
		case CommandStatus::DOWNSTREAM_FAIL:
			dnp3 = opendnp3::CommandStatus::DOWNSTREAM_FAIL;
			break;
		case CommandStatus::NON_PARTICIPATING:
			dnp3 = opendnp3::CommandStatus::NON_PARTICIPATING;
			break;
		case CommandStatus::UNDEFINED:
		default:
			dnp3 = opendnp3::CommandStatus::UNDEFINED;
			break;
	}
	return dnp3;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::Binary& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::Binary, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::CHATTER_FILTER))
		qual |= QualityFlags::CHATTER_FILTER;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::BinaryQuality::RESTART))
		qual |= QualityFlags::RESTART;

	event->SetPayload<EventType::Binary>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::DoubleBitBinary& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::DoubleBitBinary, ind, source);

	EventTypePayload<EventType::DoubleBitBinary>::type val;

	switch(dnp3.value)
	{
		case opendnp3::DoubleBit::INTERMEDIATE: //00
			val.first = false; val.second = false; break;
		case opendnp3::DoubleBit::DETERMINED_OFF: //01
			val.first = false; val.second = true; break;
		case opendnp3::DoubleBit::DETERMINED_ON: //10
			val.first = true; val.second = false; break;
		case opendnp3::DoubleBit::INDETERMINATE: //11
			val.first = true; val.second = true; break;
	}

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER))
		qual |= QualityFlags::CHATTER_FILTER;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.value & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART))
		qual |= QualityFlags::RESTART;

	event->SetPayload<EventType::DoubleBitBinary>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::Analog& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::Analog, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::OVERRANGE))
		qual |= QualityFlags::OVERRANGE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::REFERENCE_ERR))
		qual |= QualityFlags::REFERENCE_ERR;

	event->SetPayload<EventType::Analog>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::Counter& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::Counter, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::ROLLOVER))
		qual |= QualityFlags::ROLLOVER;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::DISCONTINUITY))
		qual |= QualityFlags::DISCONTINUITY;

	event->SetPayload<EventType::Counter>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::FrozenCounter& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::FrozenCounter, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::ROLLOVER))
		qual |= QualityFlags::ROLLOVER;
	if(dnp3.flags.IsSet(opendnp3::CounterQuality::DISCONTINUITY))
		qual |= QualityFlags::DISCONTINUITY;

	event->SetPayload<EventType::FrozenCounter>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryOutputStatus& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::BinaryOutputStatus, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::BinaryOutputStatusQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::BinaryOutputStatusQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::BinaryOutputStatusQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::BinaryOutputStatusQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::BinaryOutputStatusQuality::RESTART))
		qual |= QualityFlags::RESTART;

	event->SetPayload<EventType::BinaryOutputStatus>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputStatus& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputStatus, ind, source);

	auto val = dnp3.value;

	QualityFlags qual = QualityFlags::NONE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::OVERRANGE))
		qual |= QualityFlags::OVERRANGE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(dnp3.flags.IsSet(opendnp3::AnalogQuality::REFERENCE_ERR))
		qual |= QualityFlags::REFERENCE_ERR;

	event->SetPayload<EventType::AnalogOutputStatus>(std::move(val));
	event->SetQuality(qual);
	event->SetTimestamp(dnp3.time);

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryQuality& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::BinaryQuality, ind, source);

	QualityFlags qual = QualityFlags::NONE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER))
		qual |= QualityFlags::CHATTER_FILTER;

	event->SetPayload<EventType::BinaryQuality>(std::move(qual));
	event->SetQuality(qual);
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::DoubleBitBinaryQuality& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::DoubleBitBinaryQuality, ind, source);

	QualityFlags qual = QualityFlags::NONE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER))
		qual |= QualityFlags::CHATTER_FILTER;

	event->SetPayload<EventType::DoubleBitBinaryQuality>(std::move(qual));
	event->SetQuality(qual);
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogQuality& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogQuality, ind, source);

	QualityFlags qual = QualityFlags::NONE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::REFERENCE_ERR))
		qual |= QualityFlags::REFERENCE_ERR;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::AnalogQuality::OVERRANGE))
		qual |= QualityFlags::OVERRANGE;

	event->SetPayload<EventType::AnalogQuality>(std::move(qual));
	event->SetQuality(qual);
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::CounterQuality& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::CounterQuality, ind, source);

	QualityFlags qual = QualityFlags::NONE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::ROLLOVER))
		qual |= QualityFlags::ROLLOVER;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::CounterQuality::DISCONTINUITY))
		qual |= QualityFlags::DISCONTINUITY;

	event->SetPayload<EventType::CounterQuality>(std::move(qual));
	event->SetQuality(qual);
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::BinaryOutputStatusQuality& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::BinaryOutputStatusQuality, ind, source);

	QualityFlags qual = QualityFlags::NONE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::ONLINE))
		qual |= QualityFlags::ONLINE;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::RESTART))
		qual |= QualityFlags::RESTART;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::COMM_LOST))
		qual |= QualityFlags::COMM_LOST;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::REMOTE_FORCED))
		qual |= QualityFlags::REMOTE_FORCED;
	if(static_cast<uint8_t>(dnp3) & static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::LOCAL_FORCED))
		qual |= QualityFlags::LOCAL_FORCED;

	event->SetPayload<EventType::BinaryOutputStatusQuality>(std::move(qual));
	event->SetQuality(qual);
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::ControlRelayOutputBlock& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ind, source);

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;

	switch(dnp3.functionCode)
	{
		case opendnp3::ControlCode::NUL:
			val.functionCode = ControlCode::NUL;
			break;
		case opendnp3::ControlCode::NUL_CANCEL:
			val.functionCode = ControlCode::NUL_CANCEL;
			break;
		case opendnp3::ControlCode::PULSE_ON:
			val.functionCode = ControlCode::PULSE_ON;
			break;
		case opendnp3::ControlCode::PULSE_ON_CANCEL:
			val.functionCode = ControlCode::PULSE_ON_CANCEL;
			break;
		case opendnp3::ControlCode::PULSE_OFF:
			val.functionCode = ControlCode::PULSE_OFF;
			break;
		case opendnp3::ControlCode::PULSE_OFF_CANCEL:
			val.functionCode = ControlCode::PULSE_OFF_CANCEL;
			break;
		case opendnp3::ControlCode::LATCH_ON:
			val.functionCode = ControlCode::LATCH_ON;
			break;
		case opendnp3::ControlCode::LATCH_ON_CANCEL:
			val.functionCode = ControlCode::LATCH_ON_CANCEL;
			break;
		case opendnp3::ControlCode::LATCH_OFF:
			val.functionCode = ControlCode::LATCH_OFF;
			break;
		case opendnp3::ControlCode::LATCH_OFF_CANCEL:
			val.functionCode = ControlCode::LATCH_OFF_CANCEL;
			break;
		case opendnp3::ControlCode::CLOSE_PULSE_ON:
			val.functionCode = ControlCode::CLOSE_PULSE_ON;
			break;
		case opendnp3::ControlCode::CLOSE_PULSE_ON_CANCEL:
			val.functionCode = ControlCode::CLOSE_PULSE_ON_CANCEL;
			break;
		case opendnp3::ControlCode::TRIP_PULSE_ON:
			val.functionCode = ControlCode::TRIP_PULSE_ON;
			break;
		case opendnp3::ControlCode::TRIP_PULSE_ON_CANCEL:
			val.functionCode = ControlCode::TRIP_PULSE_ON_CANCEL;
			break;
		case opendnp3::ControlCode::UNDEFINED:
		default:
			val.functionCode = ControlCode::UNDEFINED;
			break;
	}
	val.count = dnp3.count;
	val.onTimeMS = dnp3.onTimeMS;
	val.offTimeMS = dnp3.offTimeMS;
	val.status = ToODC(dnp3.status);

	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));
	event->SetQuality(QualityFlags::NONE); //controls don't have quality
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputInt16& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ind, source);

	EventTypePayload<EventType::AnalogOutputInt16>::type val;

	val.first = dnp3.value;
	val.second = ToODC(dnp3.status);

	event->SetPayload<EventType::AnalogOutputInt16>(std::move(val));
	event->SetQuality(QualityFlags::NONE); //controls don't have quality
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputInt32& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt32, ind, source);

	EventTypePayload<EventType::AnalogOutputInt32>::type val;

	val.first = dnp3.value;
	val.second = ToODC(dnp3.status);

	event->SetPayload<EventType::AnalogOutputInt32>(std::move(val));
	event->SetQuality(QualityFlags::NONE); //controls don't have quality
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputFloat32& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputFloat32, ind, source);

	EventTypePayload<EventType::AnalogOutputFloat32>::type val;

	val.first = dnp3.value;
	val.second = ToODC(dnp3.status);

	event->SetPayload<EventType::AnalogOutputFloat32>(std::move(val));
	event->SetQuality(QualityFlags::NONE); //controls don't have quality
	event->SetTimestamp(msSinceEpoch());

	return event;
}

std::shared_ptr<EventInfo> ToODC(const opendnp3::AnalogOutputDouble64& dnp3, const size_t ind, const std::string& source)
{
	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputDouble64, ind, source);

	EventTypePayload<EventType::AnalogOutputDouble64>::type val;

	val.first = dnp3.value;
	val.second = ToODC(dnp3.status);

	event->SetPayload<EventType::AnalogOutputDouble64>(std::move(val));
	event->SetQuality(QualityFlags::NONE); //controls don't have quality
	event->SetTimestamp(msSinceEpoch());

	return event;
}

template<> opendnp3::BinaryQuality FromODC<opendnp3::BinaryQuality>(const QualityFlags& qual)
{
	uint8_t dnp3 = 0;

	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryQuality::CHATTER_FILTER);

	return static_cast<opendnp3::BinaryQuality>(dnp3);
}
template<> opendnp3::DoubleBitBinaryQuality FromODC<opendnp3::DoubleBitBinaryQuality>(const QualityFlags& qual)
{
	uint8_t dnp3 = 0;

	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::LOCAL_FORCED);
	if((qual & QualityFlags::CHATTER_FILTER) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::CHATTER_FILTER);

	return static_cast<opendnp3::DoubleBitBinaryQuality>(dnp3);
}
template<> opendnp3::AnalogQuality FromODC<opendnp3::AnalogQuality>(const QualityFlags& qual)
{
	uint8_t dnp3 = 0;

	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::LOCAL_FORCED);
	if((qual & QualityFlags::OVERRANGE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::OVERRANGE);
	if((qual & QualityFlags::REFERENCE_ERR) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::AnalogQuality::REFERENCE_ERR);

	return static_cast<opendnp3::AnalogQuality>(dnp3);
}
template<> opendnp3::CounterQuality FromODC<opendnp3::CounterQuality>(const QualityFlags& qual)
{
	uint8_t dnp3 = 0;

	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::LOCAL_FORCED);
	if((qual & QualityFlags::ROLLOVER) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::ROLLOVER);
	if((qual & QualityFlags::DISCONTINUITY) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::CounterQuality::DISCONTINUITY);

	return static_cast<opendnp3::CounterQuality>(dnp3);
}
template<> opendnp3::BinaryOutputStatusQuality FromODC<opendnp3::BinaryOutputStatusQuality>(const QualityFlags& qual)
{
	uint8_t dnp3 = 0;

	if((qual & QualityFlags::ONLINE) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::ONLINE);
	if((qual & QualityFlags::RESTART) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::RESTART);
	if((qual & QualityFlags::COMM_LOST) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::COMM_LOST);
	if((qual & QualityFlags::REMOTE_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::REMOTE_FORCED);
	if((qual & QualityFlags::LOCAL_FORCED) != QualityFlags::NONE)
		dnp3 |= static_cast<uint8_t>(opendnp3::BinaryOutputStatusQuality::LOCAL_FORCED);

	return static_cast<opendnp3::BinaryOutputStatusQuality>(dnp3);
}

template<> opendnp3::Binary FromODC<opendnp3::Binary>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::Binary dnp3;
	dnp3.value = event->GetPayload<EventType::Binary>();

	auto qual = FromODC<opendnp3::BinaryQuality>(event->GetQuality());
	dnp3.flags.value = static_cast<uint8_t>(qual);

	//re-set state bit of the quality, just in case
	if(dnp3.value)
		dnp3.flags.Set(opendnp3::BinaryQuality::STATE);

	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::DoubleBitBinary FromODC<opendnp3::DoubleBitBinary>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::DoubleBitBinary dnp3;
	auto val = event->GetPayload<EventType::DoubleBitBinary>();
	if((val.first != val.second) && val.first)
		dnp3.value = opendnp3::DoubleBit::DETERMINED_ON;
	else if((val.first != val.second) && val.second)
		dnp3.value = opendnp3::DoubleBit::DETERMINED_OFF;
	else
		dnp3.value = opendnp3::DoubleBit::INDETERMINATE;

	auto qual = FromODC<opendnp3::DoubleBitBinaryQuality>(event->GetQuality());
	dnp3.flags.value = static_cast<uint8_t>(qual);

	//re-set state bits of the quality, just in case
	if(val.first)
		dnp3.flags.value |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE1);
	if(val.second)
		dnp3.flags.value |= static_cast<uint8_t>(opendnp3::DoubleBitBinaryQuality::STATE2);

	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::Analog FromODC<opendnp3::Analog>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::Analog dnp3(event->GetPayload<EventType::Analog>());

	auto qual = FromODC<opendnp3::AnalogQuality>(event->GetQuality());
	dnp3.flags.Set(qual);
	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::Counter FromODC<opendnp3::Counter>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::Counter dnp3(event->GetPayload<EventType::Counter>());

	auto qual = FromODC<opendnp3::CounterQuality>(event->GetQuality());
	dnp3.flags.Set(qual);
	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::FrozenCounter FromODC<opendnp3::FrozenCounter>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::FrozenCounter dnp3(event->GetPayload<EventType::FrozenCounter>());

	auto qual = FromODC<opendnp3::CounterQuality>(event->GetQuality());
	dnp3.flags.Set(qual);
	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::BinaryOutputStatus FromODC<opendnp3::BinaryOutputStatus>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::BinaryOutputStatus dnp3(event->GetPayload<EventType::BinaryOutputStatus>());

	auto qual = FromODC<opendnp3::BinaryOutputStatusQuality>(event->GetQuality());
	dnp3.flags.Set(qual);
	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::AnalogOutputStatus FromODC<opendnp3::AnalogOutputStatus>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::AnalogOutputStatus dnp3(event->GetPayload<EventType::AnalogOutputStatus>());

	auto qual = FromODC<opendnp3::AnalogQuality>(event->GetQuality());
	dnp3.flags.Set(qual);
	dnp3.time = opendnp3::DNPTime(event->GetTimestamp());

	return dnp3;
}
template<> opendnp3::BinaryQuality FromODC<opendnp3::BinaryQuality>(const std::shared_ptr<const EventInfo>& event)
{
	switch(event->GetEventType())
	{
		case EventType::BinaryQuality:
			return FromODC<opendnp3::BinaryQuality>(event->GetPayload<EventType::BinaryQuality>());
		case EventType::Binary:
			return FromODC<opendnp3::BinaryQuality>(event->GetQuality());
		default:
			throw std::runtime_error("Wrong quality type requested for selected odc::EventInfo");
	}
}
template<> opendnp3::DoubleBitBinaryQuality FromODC<opendnp3::DoubleBitBinaryQuality>(const std::shared_ptr<const EventInfo>& event)
{
	switch(event->GetEventType())
	{
		case EventType::DoubleBitBinaryQuality:
			return FromODC<opendnp3::DoubleBitBinaryQuality>(event->GetPayload<EventType::DoubleBitBinaryQuality>());
		case EventType::DoubleBitBinary:
			return FromODC<opendnp3::DoubleBitBinaryQuality>(event->GetQuality());
		default:
			throw std::runtime_error("Wrong quality type requested for selected odc::EventInfo");
	}
}
template<> opendnp3::AnalogQuality FromODC<opendnp3::AnalogQuality>(const std::shared_ptr<const EventInfo>& event)
{
	switch(event->GetEventType())
	{
		case EventType::AnalogQuality:
			return FromODC<opendnp3::AnalogQuality>(event->GetPayload<EventType::AnalogQuality>());
		case EventType::Analog:
			return FromODC<opendnp3::AnalogQuality>(event->GetQuality());
		default:
			throw std::runtime_error("Wrong quality type requested for selected odc::EventInfo");
	}
}
template<> opendnp3::CounterQuality FromODC<opendnp3::CounterQuality>(const std::shared_ptr<const EventInfo>& event)
{
	switch(event->GetEventType())
	{
		case EventType::CounterQuality:
			return FromODC<opendnp3::CounterQuality>(event->GetPayload<EventType::CounterQuality>());
		case EventType::Counter:
			return FromODC<opendnp3::CounterQuality>(event->GetQuality());
		default:
			throw std::runtime_error("Wrong quality type requested for selected odc::EventInfo");
	}
}
template<> opendnp3::BinaryOutputStatusQuality FromODC<opendnp3::BinaryOutputStatusQuality>(const std::shared_ptr<const EventInfo>& event)
{
	switch(event->GetEventType())
	{
		case EventType::BinaryOutputStatusQuality:
			return FromODC<opendnp3::BinaryOutputStatusQuality>(event->GetPayload<EventType::BinaryOutputStatusQuality>());
		case EventType::BinaryOutputStatus:
			return FromODC<opendnp3::BinaryOutputStatusQuality>(event->GetQuality());
		default:
			throw std::runtime_error("Wrong quality type requested for selected odc::EventInfo");
	}
}
template<> opendnp3::ControlRelayOutputBlock FromODC<opendnp3::ControlRelayOutputBlock>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::ControlRelayOutputBlock dnp3;

	auto control = event->GetPayload<EventType::ControlRelayOutputBlock>();

	switch(control.functionCode)
	{
		case ControlCode::NUL:
			dnp3.functionCode = opendnp3::ControlCode::NUL;
			break;
		case ControlCode::NUL_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::NUL_CANCEL;
			break;
		case ControlCode::PULSE_ON:
			dnp3.functionCode = opendnp3::ControlCode::PULSE_ON;
			break;
		case ControlCode::PULSE_ON_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::PULSE_ON_CANCEL;
			break;
		case ControlCode::PULSE_OFF:
			dnp3.functionCode = opendnp3::ControlCode::PULSE_OFF;
			break;
		case ControlCode::PULSE_OFF_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::PULSE_OFF_CANCEL;
			break;
		case ControlCode::LATCH_ON:
			dnp3.functionCode = opendnp3::ControlCode::LATCH_ON;
			break;
		case ControlCode::LATCH_ON_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::LATCH_ON_CANCEL;
			break;
		case ControlCode::LATCH_OFF:
			dnp3.functionCode = opendnp3::ControlCode::LATCH_OFF;
			break;
		case ControlCode::LATCH_OFF_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::LATCH_OFF_CANCEL;
			break;
		case ControlCode::CLOSE_PULSE_ON:
			dnp3.functionCode = opendnp3::ControlCode::CLOSE_PULSE_ON;
			break;
		case ControlCode::CLOSE_PULSE_ON_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::CLOSE_PULSE_ON_CANCEL;
			break;
		case ControlCode::TRIP_PULSE_ON:
			dnp3.functionCode = opendnp3::ControlCode::TRIP_PULSE_ON;
			break;
		case ControlCode::TRIP_PULSE_ON_CANCEL:
			dnp3.functionCode = opendnp3::ControlCode::TRIP_PULSE_ON_CANCEL;
			break;
		case ControlCode::UNDEFINED:
		default:
			dnp3.functionCode = opendnp3::ControlCode::UNDEFINED;
			break;
	}
	dnp3.count = control.count;
	dnp3.onTimeMS = control.onTimeMS;
	dnp3.offTimeMS = control.offTimeMS;
	dnp3.status = FromODC(control.status);

	return dnp3;
}
template<> opendnp3::AnalogOutputInt16 FromODC<opendnp3::AnalogOutputInt16>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::AnalogOutputInt16 dnp3;

	auto control = event->GetPayload<EventType::AnalogOutputInt16>();
	dnp3.value = control.first;
	dnp3.status = FromODC(control.second);

	return dnp3;
}
template<> opendnp3::AnalogOutputInt32 FromODC<opendnp3::AnalogOutputInt32>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::AnalogOutputInt32 dnp3;

	auto control = event->GetPayload<EventType::AnalogOutputInt32>();
	dnp3.value = control.first;
	dnp3.status = FromODC(control.second);

	return dnp3;
}
template<> opendnp3::AnalogOutputFloat32 FromODC<opendnp3::AnalogOutputFloat32>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::AnalogOutputFloat32 dnp3;

	auto control = event->GetPayload<EventType::AnalogOutputFloat32>();
	dnp3.value = control.first;
	dnp3.status = FromODC(control.second);

	return dnp3;
}
template<> opendnp3::AnalogOutputDouble64 FromODC<opendnp3::AnalogOutputDouble64>(const std::shared_ptr<const EventInfo>& event)
{
	opendnp3::AnalogOutputDouble64 dnp3;

	auto control = event->GetPayload<EventType::AnalogOutputDouble64>();
	dnp3.value = control.first;
	dnp3.status = FromODC(control.second);

	return dnp3;
}

} //namespace odc
