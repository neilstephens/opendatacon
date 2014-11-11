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
 * ModbusClientPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ModbusCLIENTPORT_H_
#define ModbusCLIENTPORT_H_

#include <queue>
#include <opendnp3/master/ISOEHandler.h>
#include <opendnp3/master/IPollListener.h>
#include <opendnp3/master/CommandResponse.h>
#include <opendnp3/app/IterableBuffer.h>

#include "ModbusPort.h"
#include "ASIOScheduler.h"

using namespace opendnp3;

class ModbusMasterPort: public ModbusPort
{
public:
	ModbusMasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
		ModbusPort(aName, aConfFilename, aConfOverrides),
        mb(nullptr)
	{};
    
    ~ModbusMasterPort()
    {
        Disable();
        if (mb != nullptr) modbus_free(mb);
    }

    // Implement ModbusPort
	void Enable();
	void Disable();
    void Connect();
    void Disconnect();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	// Implement some IOHandler - parent ModbusPort implements the rest to return NOT_SUPPORTED
	std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(bool connected, uint16_t index, const std::string& SenderName);
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);

    
private:
    template<class T>
    opendnp3::CommandStatus WriteObject(const T& command, uint16_t index);
    
    void DoPoll(uint32_t pollgroup);
    
private:
    void HandleError(int errnum, const std::string& source);
    CommandStatus HandleWriteError(int errnum, const std::string& source);
    
    void StateListener(opendnp3::ChannelState state);
    modbus_t *mb;
    typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
    std::unique_ptr<Timer_t> pTCPRetryTimer;
    std::unique_ptr<ASIOScheduler> PollScheduler;
};

#endif /* ModbusCLIENTPORT_H_ */
