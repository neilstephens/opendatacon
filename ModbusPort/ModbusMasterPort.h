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
#include "ModbusPort.h"
#include <queue>
#include <opendatacon/ASIOScheduler.h>
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
	ModbusMasterPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		ModbusPort(aName, aConfFilename, aConfOverrides),
		modbus_read_buffer(nullptr),
		modbus_read_buffer_size(0)
	{}

	~ModbusMasterPort() override;

	// Implement ModbusPort
	void Enable() override;
	void Disable() override final;
	void Connect(modbus_t *mb);
	void Disconnect();
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

private:
	CommandStatus WriteObject(modbus_t* mb, const ControlRelayOutputBlock& output, uint16_t index);
	CommandStatus WriteObject(modbus_t* mb, const int16_t output, uint16_t index);
	CommandStatus WriteObject(modbus_t* mb, const int32_t output, uint16_t index);
	CommandStatus WriteObject(modbus_t* mb, const double output, uint16_t index);
	CommandStatus WriteObject(modbus_t* mb, const float output, uint16_t index);

	void DoPoll(uint32_t pollgroup, modbus_t *mb);

private:
	void HandleError(int errnum, const std::string& source);
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	template<EventType t>
	ModbusReadGroup* GetRange(uint16_t index);

	void* modbus_read_buffer;
	size_t modbus_read_buffer_size;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTCPRetryTimer;
	std::unique_ptr<ASIOScheduler> PollScheduler;
};

#endif /* ModbusCLIENTPORT_H_ */
