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
 * MD3ClientPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef MD3CLIENTPORT_H_
#define MD3CLIENTPORT_H_

#include <queue>
#include <opendnp3/master/ISOEHandler.h>

//#include <MD3/MD3.h>
#include "MD3Port.h"
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


class MD3MasterPort: public MD3Port
{
public:
	MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
		MD3Port(aName, aConfFilename, aConfOverrides),
	//	mb(nullptr),
		MD3_read_buffer(nullptr),
		MD3_read_buffer_size(0)
	{}

	~MD3MasterPort() override;

	// Implement MD3Port
	void Enable() override;
	void Disable() override;
	void Connect();
	void Disconnect();
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	// Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
	std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) override;
	template<typename T> std::future<CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);


private:
	template<class T>
	CommandStatus WriteObject(const T& command, uint16_t index);

	void DoPoll(uint32_t pollgroup);

private:
	void HandleError(int errnum, const std::string& source);
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	MD3ReadGroup<Binary>* GetRange(uint16_t index);

	//MD3_t *mb;
	void* MD3_read_buffer;
	size_t MD3_read_buffer_size;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTCPRetryTimer;
	std::unique_ptr<ASIOScheduler> PollScheduler;
};

#endif /* MD3CLIENTPORT_H_ */
