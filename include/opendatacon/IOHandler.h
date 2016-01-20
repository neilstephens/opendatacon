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
 * IOHandler.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef IOHANDLER_H_
#define IOHANDLER_H_

#include <future>
#include <unordered_map>
#include <map>
#include <asio.hpp>
#include <asiopal/LogFanoutHandler.h>
#include <asiodnp3/DNP3Manager.h>
#include "opendnp3/app/MeasurementTypes.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"
#include "opendnp3/app/AnalogOutput.h"

enum ConnectState {PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN};

typedef opendnp3::BinaryQuality BinaryQuality;
typedef opendnp3::DoubleBitBinaryQuality DoubleBitBinaryQuality;
typedef opendnp3::AnalogQuality AnalogQuality;
typedef opendnp3::CounterQuality CounterQuality;
typedef opendnp3::BinaryOutputStatusQuality BinaryOutputStatusQuality;

enum class FrozenCounterQuality: uint8_t
{
	/// set when the data is "good", meaning that rest of the system can trust the value
	ONLINE = 0x1,
	/// the quality all points get before we have established communication (or populated) the point
	RESTART = 0x2,
	/// set if communication has been lost with the source of the data (after establishing contact)
	COMM_LOST = 0x4,
	/// set if the value is being forced to a "fake" value somewhere in the system
	REMOTE_FORCED = 0x8,
	/// set if the value is being forced to a "fake" value on the original device
	LOCAL_FORCED = 0x10,
	/// Deprecated flag that indicates value has rolled over
	ROLLOVER = 0x20,
	/// indicates an unusual change in value
	DISCONTINUITY = 0x40,
	/// reserved bit
	RESERVED = 0x80
};

/**
Quality field bitmask for AnalogOutputStatus values
*/
enum class AnalogOutputStatusQuality: uint8_t
{
	/// set when the data is "good", meaning that rest of the system can trust the value
	ONLINE = 0x1,
	/// the quality all points get before we have established communication (or populated) the point
	RESTART = 0x2,
	/// set if communication has been lost with the source of the data (after establishing contact)
	COMM_LOST = 0x4,
	/// set if the value is being forced to a "fake" value somewhere in the system
	REMOTE_FORCED = 0x8,
	/// set if the value is being forced to a "fake" value on the original device
	LOCAL_FORCED = 0x10,
	/// set if a hardware input etc. is out of range and we are using a place holder value
	OVERRANGE = 0x20,
	/// set if calibration or reference voltage has been lost meaning readings are questionable
	REFERENCE_ERR = 0x40,
	/// reserved bit
	RESERVED = 0x80
};

class IOHandler
{
public:
	IOHandler(std::string aName);
	virtual ~IOHandler(){}

	static std::future<opendnp3::CommandStatus> CommandFutureSuccess()
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::SUCCESS);
		return Promise.get_future();
	}

	static std::future<opendnp3::CommandStatus> CommandFutureUndefined()
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return Promise.get_future();
	}

	static std::future<opendnp3::CommandStatus> CommandFutureNotSupported()
	{
		auto Promise = std::promise<opendnp3::CommandStatus>();
		Promise.set_value(opendnp3::CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	}

	//Create an overloaded Event function for every type of event

	// measurement events
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) = 0;

	// change of quality events
	virtual std::future<opendnp3::CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) = 0;

	// control events
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) = 0;

	//Connection events:
	virtual std::future<opendnp3::CommandStatus> Event(ConnectState state, uint16_t index, const std::string& SenderName) = 0;

	void Subscribe(IOHandler* pIOHandler, std::string aName);
	void AddLogSubscriber(openpal::ILogHandler* logger);
	void SetLogLevel(openpal::LogFilters LOG_LEVEL);
	void SetIOS(asio::io_service* ios_ptr);

	std::string Name;
	std::unordered_map<std::string,IOHandler*> Subscribers;
	std::unique_ptr<asiopal::LogFanoutHandler> pLoggers;
	openpal::LogFilters LOG_LEVEL;
	asio::io_service* pIOS;
	bool enabled;

	// Don't access the following from outside this dll under Windows
	// due to static issues use GetIOHandlers to return the right reference instead
	static std::unordered_map<std::string, IOHandler*> IOHandlers;

protected:
	bool InDemand();
	std::map<std::string,bool> connection_demands;
	bool MuxConnectionEvents(ConnectState state, const std::string& SenderName);
};

std::unordered_map<std::string, IOHandler*>& GetIOHandlers();

#endif /* IOHANDLER_H_ */
