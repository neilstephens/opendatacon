/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 * PyPort.h
 *
 *  Created on: 26/03/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef PYWRAPPER_H_
#define PYWRAPPER_H_

#include "Py.h"
#include "SpecialEventQueue.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <opendatacon/util.h>
#include <opendatacon/DataPort.h>


using namespace odc;

typedef std::function<void (uint32_t, uint32_t)> SetTimerFnType;
typedef std::function<void ( const char*, uint32_t, const char*, const char*)> PublishEventCallFnType;

// Class to store the evnt as a stringified version, mainly so that when Python is retreving these records, it does minimal processing.
/*class EventQueueType
{
public:
      EventQueueType(const std::string& _EventType, const size_t _Index, odc::msSinceEpoch_t _TimeStamp,
            const std::string& _Quality, const std::string& _Payload, const std::string& _Sender):
            EventType(_EventType),
            Index(_Index),
            TimeStamp(_TimeStamp),
            Quality(_Quality),
            Payload(_Payload),
            Sender(_Sender)
      {};
      EventQueueType():
            EventType(""),
            Index(0),
            TimeStamp(0),
            Quality(""),
            Payload(""),
            Sender("")
      {};
      std::string EventType;
      size_t Index;
      odc::msSinceEpoch_t TimeStamp;
      std::string Quality;
      std::string Payload;
      std::string Sender;
};*/

class PythonInitWrapper
{
public:
	PythonInitWrapper(bool GlobalUseSystemPython);
	~PythonInitWrapper();
private:
	static PyThreadState* threadState;
};

class PythonWrapper
{

public:
	PythonWrapper(const std::string& aName, std::shared_ptr<odc::asio_service> _pIOS, SetTimerFnType SetTimerFn, PublishEventCallFnType PublishEventCallFn);
	~PythonWrapper();
	void Build(const std::string& modulename, const std::string& pyPathName, const std::string& pyLoadModuleName, const std::string& pyClassName, const std::string& PortName, bool GlobalUseSystemPython);
	void Config(const std::string& JSONMain, const std::string& JSONOverride);
	void PortOperational(); // Called when Build is complete.
	void Enable();
	void Disable();

	CommandStatus Event(const std::shared_ptr<const EventInfo>& odcevent, const std::string& SenderName);
	void QueueEvent(const std::string& jsonevent);

	bool DequeueEvent(std::string& eq);
	size_t GetEventQueueSize()
	{
		return EventQueue->Size();
	}

	void CallTimerHandler(uint32_t id);
	std::string RestHandler(const std::string& url, const std::string& content);

	SetTimerFnType GetPythonPortSetTimerFn() { return PythonPortSetTimerFn; };                         // Protect set access, only allow get.
	PublishEventCallFnType GetPythonPortPublishEventCallFn() { return PythonPortPublishEventCallFn; }; // Protect set access, only allow get.

	static void PyErrOutput();
	static void DumpStackTrace();

	std::string Name;

	// Do this to make sure it is always a valid pointer - the python code could pass anything back...
	static PythonWrapper* GetThisFromPythonSelf(uint64_t guid)
	{
		std::shared_lock<std::shared_timed_mutex> lck(PythonWrapper::WrapperHashMutex);
		if (PyWrappers.count(guid))
		{
			return (PythonWrapper*)guid;
		}
		return nullptr;
	}

private:
	void StoreWrapperMapping()
	{
		std::unique_lock<std::shared_timed_mutex> lck(PythonWrapper::WrapperHashMutex);
		uint64_t guid = reinterpret_cast<uint64_t>(this);
		if (sizeof(uintptr_t) == 4) guid &= 0x00000000FFFFFFFF; // Stop sign extension
		PyWrappers.emplace(guid);
		LOGDEBUG("Stored python wrapper guid into mapping table - {0:#x}", guid);
	}
	void RemoveWrapperMapping()
	{
		std::unique_lock<std::shared_timed_mutex> lck(PythonWrapper::WrapperHashMutex);
		uint64_t guid = reinterpret_cast<uint64_t>(this);
		if (sizeof(uintptr_t) == 4) guid &= 0x00000000FFFFFFFF; // Stop sign extension
		PyWrappers.erase(guid);
	}

	PyObject* GetFunction(PyObject* pyInstance, const std::string& sFunction);
	PyObject* PyCall(PyObject* pyFunction, PyObject* pyArgs);

	// Keep track of each PyWrapper so static methods can get access to the correct PyPort instance
	static std::unordered_set<uint64_t> PyWrappers;
	static std::shared_timed_mutex WrapperHashMutex;

	std::shared_ptr<PythonInitWrapper> PyMgr;
	std::shared_ptr<odc::asio_service> pIOS;

	// We need a hard limit for the number of queued events, after which we start dumping elements. Better than running out of memory?
	const size_t MaximumQueueSize = 1000000; // 1 million

	std::shared_ptr<SpecialEventQueue<std::string>> EventQueue;

	// Keep pointers to the methods in out Python code that we want to be able to call.
	PyObject* pyModule = nullptr;
	PyObject* pyInstance = nullptr;
	PyObject* pyFuncConfig = nullptr;
	PyObject* pyFuncOperational = nullptr;
	PyObject* pyFuncEnable = nullptr;
	PyObject* pyFuncDisable = nullptr;
	PyObject* pyFuncEvent = nullptr;
	PyObject* pyTimerHandler = nullptr;
	PyObject* pyRestHandler = nullptr;

	SetTimerFnType PythonPortSetTimerFn;
	PublishEventCallFnType PythonPortPublishEventCallFn;
	std::atomic_flag QueuePushErrorLogged = ATOMIC_FLAG_INIT;
};

#endif /* PYWRAPPER_H_ */
