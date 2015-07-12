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
#include <opendnp3/master/CommandResponse.h>
#include <opendnp3/app/IterableBuffer.h>

#include <modbus/modbus.h>
#include "ModbusPort.h"
#include <opendatacon/ASIOScheduler.h>

using namespace opendnp3;

#include <utility>

/*
template <typename T, typename F>
class capture_impl
{
    T x;
    F f;
public:
    capture_impl( T && x, F && f )
    : x{std::forward<T>(x)}, f{std::forward<F>(f)}
    {}
    
    template <typename ...Ts> auto operator()( Ts&&...args )
    -> decltype(f( x, std::forward<Ts>(args)... ))
    {
        return f( x, std::forward<Ts>(args)... );
    }
    
    template <typename ...Ts> auto operator()( Ts&&...args ) const
    -> decltype(f( x, std::forward<Ts>(args)... ))
    {
        return f( x, std::forward<Ts>(args)... );
    }
};

template <typename T, typename F>
capture_impl<T,F> capture( T && x, F && f )
{
    return capture_impl<T,F>(
                             std::forward<T>(x), std::forward<F>(f) );
}*/


class ModbusMasterPort: public ModbusPort
{
public:
	ModbusMasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
		ModbusPort(aName, aConfFilename, aConfOverrides),
        mb(nullptr)
	{};
    
    ~ModbusMasterPort();

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
	std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, uint16_t index, const std::string& SenderName);
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);

    
private:
    template<class T>
    opendnp3::CommandStatus WriteObject(const T& command, uint16_t index);
    
    void DoPoll(uint32_t pollgroup);
    
private:
    void HandleError(int errnum, const std::string& source);
    CommandStatus HandleWriteError(int errnum, const std::string& source);
    
    ModbusReadGroup<opendnp3::Binary>* GetRange(uint16_t index);
    
    modbus_t *mb;
    typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
    std::unique_ptr<Timer_t> pTCPRetryTimer;
    std::unique_ptr<ASIOScheduler> PollScheduler;
};

#endif /* ModbusCLIENTPORT_H_ */
