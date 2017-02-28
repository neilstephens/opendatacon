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
#include <opendatacon/IOTypes.h>

namespace odc
{
	
enum ConnectState {PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN};

typedef enum { ENABLED, DISABLED, DELAYED } InitState_t;

class IOHandler
{
public:
	IOHandler(std::string aName);
	virtual ~IOHandler(){}

	static std::future<CommandStatus> CommandFutureSuccess()
	{
		auto Promise = std::promise<CommandStatus>();
		Promise.set_value(CommandStatus::SUCCESS);
		return Promise.get_future();
	}

	static std::future<CommandStatus> CommandFutureUndefined()
	{
		auto Promise = std::promise<CommandStatus>();
		Promise.set_value(CommandStatus::UNDEFINED);
		return Promise.get_future();
	}

	static std::future<CommandStatus> CommandFutureNotSupported()
	{
		auto Promise = std::promise<CommandStatus>();
		Promise.set_value(CommandStatus::NOT_SUPPORTED);
		return Promise.get_future();
	}

	//Create an overloaded Event function for every type of event

	// measurement events
	virtual std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) = 0;

	// change of quality events
	virtual std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) = 0;

	// control events
	virtual std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) = 0;
	virtual std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) = 0;

	//Connection events:
	virtual std::future<CommandStatus> Event(ConnectState state, uint16_t index, const std::string& SenderName) = 0;

	virtual void Enable()=0;
	virtual void Disable()=0;

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
	InitState_t InitState;
	uint16_t EnableDelayms;

	static std::unordered_map<std::string, IOHandler*>& GetIOHandlers();

protected:
	bool InDemand();
	std::map<std::string,bool> connection_demands;
	bool MuxConnectionEvents(ConnectState state, const std::string& SenderName);

private:
	// Important that this is private - for inter process memory management
	static std::unordered_map<std::string, IOHandler*> IOHandlers;
};
	
}

#endif /* IOHANDLER_H_ */
