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
 * SimPort.h
 *
 *  Created on: 29/07/2015
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "SimPort.h"

//Implement DataPort interface
SimPort::SimPort(std::string Name, std::string File, const Json::Value Overrides):
	DataPort(Name, File, Overrides)
{}
void SimPort::Enable()
{}
void SimPort::Disable()
{}
void SimPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{}
void SimPort::ProcessElements(const Json::Value& JSONRoot)
{}
std::future<opendnp3::CommandStatus> SimPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	return CommandFutureSuccess();
}

//Implement Event handlers from IOHandler - All not supported because SimPort is just a source.

// measurement events
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

// change of quality Events
std::future<opendnp3::CommandStatus> SimPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

// control events
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

