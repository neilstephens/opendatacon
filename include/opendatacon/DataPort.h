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
 * DataPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DATAPORT_H_
#define DATAPORT_H_

#include "DataPortConf.h"
#include "IOHandler.h"
#include "IOManager.h"
#include "ConfigParser.h"
#include "IUIResponder.h"

namespace odc {
	typedef opendnp3::ChannelState ChannelState;
	
	class DataPort: public IOHandler, public ConfigParser
	{
	public:
		DataPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
		IOHandler(aName),
		ConfigParser(aConfFilename, aConfOverrides),
		pConf(nullptr)
		{}
		virtual ~DataPort(){}
		
		virtual void Enable()=0;
		virtual void Disable()=0;
		virtual void BuildOrRebuild()=0;
		virtual void ProcessElements(const Json::Value& JSONRoot)=0;
		
		virtual const Json::Value GetStatistics() const
		{
			return Json::Value();
		}
		
		virtual const Json::Value GetCurrentState() const
		{
			return Json::Value();
		}
		
		virtual const Json::Value GetStatus() const
		{
			return Json::Value();
		}
		
		// Default to unsupported
		virtual std::future<CommandStatus> Event(const ConnectState& state, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		
		virtual std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		
		virtual std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		
		virtual std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		virtual std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
		
	protected:
		std::unique_ptr<DataPortConf> pConf;
	};
	
}

#endif /* DATAPORT_H_ */
