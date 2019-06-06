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
 * PyPort.cpp
 *
 *  Created on: 26/03/2017
 *      Author: Alan Murray <alan@atmurray.net>
 *		Extensively modifed: Scott Ellis <scott.ellis@novatex.com.au>
 */


// Python Interface:
// We need to pass the config JSON (both main and overrides) to the Python class, so that it has all the information that we have.
// We need to call a Python method every time we get an event, we assume it will not call network or other long running functions. We wait for it to finish.
// We need to export a function to the Python code, so that it can Post an Event into the ODC event bus.
// Do we need to call a Python like timer method, that will allow the Python code to set how long it should be until it is next called.
// So leave the extension bit out for the moment, just get to the pont where we can load the class and call its methods...

#include "PyPort.h"
#include <chrono>
#include <ctime>
#include <time.h>
#include <iomanip>


using namespace odc;


// Constructor for PyPort --------------------------------------
PyPort::PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	//the creation of a new CBPortConf will get the point details
	pConf.reset(new PyPortConf(ConfFilename, ConfOverrides));

	// Just to save a lot of dereferencing..
	MyConf = static_cast<PyPortConf*>(this->pConf.get());

	// We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();

	pWrapper.reset(new PythonWrapper(this->Name));
}

PyPort::~PyPort()
{
	LOGDEBUG("Destructing PyPort");
	pWrapper.reset();
}

// The ASIO IOS instance is up, our config files have been read and parsed, this is the opportunity to kick off connections and scheduled processes
void PyPort::Build()
{
	//asio::io_service::strand pyStrand(IOMgr.);

	pWrapper->Build("DataPort", MyConf->pyModuleName, MyConf->pyClassName, this->Name);

	LOGDEBUG("Loaded \"{}\" ", MyConf->pyModuleName);
}

void PyPort::Enable()
{
	if (enabled) return;
	enabled = true;

	pWrapper->Enable();
};

void PyPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	pWrapper->Disable();
};

// So we have received an event from the ODC message bus - it will be Control or Connect events.
// We will be sending back values (PublishEvent), from the simulator, as if we were a connected master scanning a live RTU.
// That is assuming our Python script is a simulator (RTU) It could also be accepting point data values sent out by someone else.
// So basically the Event mechanism is agnostinc. You can be a producer of control events and a consumer of data events, or the reverse, or in some odd cases -
// A consumer of both types of events (this means you are just a listener on a conversation between two other devices.)
void PyPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	// Try and send all events through to Python without modification, return value will be success or failure - using our predefined values.

	auto pyArgs = PyTuple_New(5);
	auto pyEventType = PyLong_FromUnsignedLong(static_cast<unsigned long>(event->GetEventType()));
	auto pyIndex = PyLong_FromUnsignedLong(event->GetIndex());
	auto pyTime = PyLong_FromUnsignedLong(event->GetTimestamp());
	auto pyQuality = PyLong_FromUnsignedLong(static_cast<unsigned long>(event->GetQuality()));
	auto pySender = PyUnicode_FromString(SenderName.c_str());

	// The py values above are stolen into the pyArgs structure - I think, so only need to release pyArgs
	PyTuple_SetItem(pyArgs, 0, pyEventType);
	PyTuple_SetItem(pyArgs, 1, pyIndex);
	PyTuple_SetItem(pyArgs, 2, pyTime);
	PyTuple_SetItem(pyArgs, 3, pyQuality);

	/*
	typedef std::pair<bool, bool> DBB;
	typedef std::tuple<msSinceEpoch_t, uint32_t, uint8_t> TAI;
	typedef std::pair<uint16_t, uint32_t> SS;
	typedef std::pair<int16_t, CommandStatus> AO16;
	typedef std::pair<int32_t, CommandStatus> AO32;
	typedef std::pair<float, CommandStatus> AOF;
	typedef std::pair<double, CommandStatus> AOD;

	EVENTPAYLOAD(EventType::Binary, bool)
	      EVENTPAYLOAD(EventType::DoubleBitBinary, DBB)
	      EVENTPAYLOAD(EventType::Analog, double)
	      EVENTPAYLOAD(EventType::Counter, uint32_t)
	      EVENTPAYLOAD(EventType::FrozenCounter, uint32_t)
	      EVENTPAYLOAD(EventType::BinaryOutputStatus, bool)
	      EVENTPAYLOAD(EventType::AnalogOutputStatus, double)
	      EVENTPAYLOAD(EventType::BinaryCommandEvent, CommandStatus)
	      EVENTPAYLOAD(EventType::AnalogCommandEvent, CommandStatus)
	      EVENTPAYLOAD(EventType::OctetString, std::string)
	      EVENTPAYLOAD(EventType::TimeAndInterval, TAI)
	      EVENTPAYLOAD(EventType::SecurityStat, SS)
	      EVENTPAYLOAD(EventType::ControlRelayOutputBlock, ControlRelayOutputBlock)
	      EVENTPAYLOAD(EventType::AnalogOutputInt16, AO16)
	      EVENTPAYLOAD(EventType::AnalogOutputInt32, AO32)
	      EVENTPAYLOAD(EventType::AnalogOutputFloat32, AOF)
	      EVENTPAYLOAD(EventType::AnalogOutputDouble64, AOD)
	      EVENTPAYLOAD(EventType::BinaryQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::DoubleBitBinaryQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::AnalogQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::CounterQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::BinaryOutputStatusQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::FrozenCounterQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::AnalogOutputStatusQuality, QualityFlags)
	      EVENTPAYLOAD(EventType::ConnectState, ConnectState)
	      */
	//PyTuple_SetItem(pyArgs, 4, event->GetPayload()); //TODO Can be many types - how best to handle??

	PyTuple_SetItem(pyArgs, 4, pySender);

//	PostPyCall(pyFuncEvent, pyArgs, pStatusCallback); // Callback will be called when done...

	Py_DECREF(pyArgs);
}
/*
switch (event->GetEventType())
{

    case EventType::ControlRelayOutputBlock:
    return WriteObject(event->GetPayload<EventType::ControlRelayOutputBlock>(), numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
    case EventType::AnalogOutputInt16:
    return WriteObject(event->GetPayload<EventType::AnalogOutputInt16>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
    case EventType::AnalogOutputInt32:
    return WriteObject(event->GetPayload<EventType::AnalogOutputInt32>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
    case EventType::AnalogOutputFloat32:
    return WriteObject(event->GetPayload<EventType::AnalogOutputFloat32>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
    case EventType::AnalogOutputDouble64:
    return WriteObject(event->GetPayload<EventType::AnalogOutputDouble64>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);


    case EventType::Analog:
    {
          // ODC Analog is a double by default...
          uint16_t analogmeas = static_cast<uint16_t>(event->GetPayload<EventType::Analog>());

          LOGDEBUG("OS - Received Event - Analog - Index {}  Value 0x{}", std::to_string(ODCIndex), std::to_string(analogmeas));
          return (*pStatusCallback)(CommandStatus::SUCCESS);
    }
    case EventType::Counter:
    {
          return (*pStatusCallback)(CommandStatus::SUCCESS);
    }
    case EventType::Binary:
    {
          uint8_t meas = event->GetPayload<EventType::Binary>();

          LOGDEBUG("OS - Received Event - Binary - Index {}, Bit Value {}", std::to_string(ODCIndex), std::to_string(meas));

          return (*pStatusCallback)(CommandStatus::SUCCESS);
    }
    case EventType::AnalogQuality:
    {
          return (*pStatusCallback)(CommandStatus::SUCCESS);
    }
    case EventType::CounterQuality:
    {
          return (*pStatusCallback)(CommandStatus::SUCCESS);
    }
    case EventType::ConnectState:
    {
          auto state = event->GetPayload<EventType::ConnectState>();
          // This will be fired by (typically) an CBOutStation port on the "other" side of the ODC Event bus. blockindex.e. something upstream has connected
          // We should probably send all the points to the Outstation as we don't know what state the OutStation point table will be in.

          if (state == ConnectState::CONNECTED)
          {
                LOGDEBUG("Got a connected event - Triggering sending of current data ");
          }
          else if (state == ConnectState::DISCONNECTED)
          {
                LOGDEBUG("Got a disconnected event - cant send events data ");
          }

          return PostCallbackCall(pStatusCallback, CommandStatus::SUCCESS);
    }
    default:
          return PostCallbackCall(pStatusCallback, CommandStatus::NOT_SUPPORTED);
}
*/


// Just schedule the callback, don't want to do it in a strand protected section.
void PyPort::PostCallbackCall(const odc::SharedStatusCallback_t& pStatusCallback, CommandStatus c)
{
	if (pStatusCallback != nullptr)
	{
		pIOS->post([&, pStatusCallback, c]()
			{
				(*pStatusCallback)(c);
			});
	}
}


// Post a call to a python method, that also has a callback attached. As we are not expecting the Python code to take long (not expecting network ops)
// wait for it to complete, and then call any passed callback function.
void PyPort::PostPyCall(PyObject* pyFunction, PyObject* pyArgs, SharedStatusCallback_t pStatusCallback)
{
//	pIOS->post([&, pyArgs, pyFunction, pStatusCallback]
	{
		if (pyFunction && PyCallable_Check(pyFunction))
		{
			PyObject* result = PyObject_CallObject(pyFunction, pyArgs);

			if (PyErr_Occurred())
			{
				pWrapper->PyErrOutput();
			}
			//TODO: Unpack the PyObject result, and call our callback.

			if (true)
				PostCallbackCall(pStatusCallback, CommandStatus::SUCCESS);
			else
				PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		}
		else
		{
			LOGERROR("Python Method is not valid");
			PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		}
	} //);
}
void PyPort::ProcessElements(const Json::Value& JSONRoot)
{
	if (JSONRoot.isMember("ModuleName"))
		MyConf->pyModuleName = JSONRoot["ModuleName"].asString();
	if (JSONRoot.isMember("ClassName"))
		MyConf->pyClassName = JSONRoot["ClassName"].asString();
}
