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

#include <functional>
#include <unordered_map>
#include <map>
#include <opendatacon/asio.h>
#include <asiopal/LogFanoutHandler.h>
#include <opendatacon/IOTypes.h>

namespace odc
{
	
enum ConnectState {PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN};

typedef enum { ENABLED, DISABLED, DELAYED } InitState_t;

class IOHandler
{
public:
	IOHandler(const std::string& aName);
	virtual ~IOHandler(){}

	//Create an overloaded Event function for every type of event

	// measurement events
	virtual void Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;

	// change of quality events
	virtual void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;

	// control events
	virtual void Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;
	virtual void Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;

	//Connection events:
	virtual void Event(ConnectState state, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) = 0;

	virtual void Enable()=0;
	virtual void Disable()=0;

	void Subscribe(IOHandler* pIOHandler, std::string aName);
	void AddLogSubscriber(openpal::ILogHandler* logger);
	void SetLogLevel(openpal::LogFilters LOG_LEVEL);
	void SetIOS(asio::io_service* ios_ptr);

	std::string Name;
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
	
	template<class T>
	void PublishEvent(const T& meas, uint16_t index, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([](CommandStatus status){}))
	{
		if(pIOS == nullptr)
		{
			throw std::runtime_error("Uninitialised io_service on enabled IOHandler");
		}
		auto multi_callback = SyncMultiCallback(Subscribers.size(),pStatusCallback);
		for(auto IOHandler_pair: Subscribers)
		{
			IOHandler_pair.second->Event(meas, index, Name, multi_callback);
		}
	}

	std::shared_ptr<std::function<void(CommandStatus)>> SyncMultiCallback (const size_t cb_number, std::shared_ptr<std::function<void(CommandStatus)>> pStatusCallback)
	{
		auto pCombinedStatus = std::make_shared<CommandStatus>(CommandStatus::SUCCESS);
		auto pExecCount = std::make_shared<size_t>(0);
		auto pCB_sync = std::make_shared<asio::strand>(*pIOS);
		return std::make_shared<std::function<void (CommandStatus status)>>
		(pCB_sync->wrap([=](CommandStatus status)
		{
			if(*pCombinedStatus == CommandStatus::UNDEFINED)
				return;

			if(*pExecCount == 0)
				*pCombinedStatus = status;
			else if(status != *pCombinedStatus)
			{
				*pCombinedStatus = CommandStatus::UNDEFINED;
				(*pStatusCallback)(*pCombinedStatus);
			}
			else if(++(*pExecCount) >= cb_number)
			{
				(*pStatusCallback)(*pCombinedStatus);
			}
		}));
	}

private:
	std::unordered_map<std::string,IOHandler*> Subscribers;

	// Important that this is private - for inter process memory management
	static std::unordered_map<std::string, IOHandler*> IOHandlers;
};
	
}

#endif /* IOHANDLER_H_ */
