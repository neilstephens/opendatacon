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
#include <datetime.h>
#include <time.h>
#include <iomanip>


using namespace odc;

msSinceEpoch_t PyDateTime_to_msSinceEpoch_t(PyObject* pyTime)
{
	if (!pyTime || !PyDateTime_Check(pyTime))
		throw std::exception();

	tm timeinfo;
	timeinfo.tm_sec = PyDateTime_DATE_GET_SECOND(pyTime);
	timeinfo.tm_min = PyDateTime_DATE_GET_MINUTE(pyTime);
	timeinfo.tm_hour = PyDateTime_DATE_GET_HOUR(pyTime);
	timeinfo.tm_mday = PyDateTime_GET_DAY(pyTime);
	timeinfo.tm_mon = PyDateTime_GET_MONTH(pyTime) - 1;
	timeinfo.tm_year = PyDateTime_GET_YEAR(pyTime) - 1900;

	return msSinceEpoch_t(0); //TODO DateTime convert (std::chrono::seconds(my_mktime(&timeinfo)) + std::chrono::microseconds(PyDateTime_DATE_GET_MICROSECOND(pyTime)));
}

std::unordered_map<PyObject*, PyPort*> PyPort::PyPorts;
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))

struct module_state
{
	PyObject* error;
};

static PyObject* error_out(PyObject* m)
{
	struct module_state* st = GETSTATE(m);
	PyErr_SetString(st->error, "something bad happened");
	return NULL;
}

static PyMethodDef myextension_methods[] = {
	{"error_out", (PyCFunction)error_out, METH_NOARGS, NULL},
	{NULL, NULL}
};

static int myextension_traverse(PyObject* m, visitproc visit, void* arg)
{
	Py_VISIT(GETSTATE(m)->error);
	return 0;
}

static int myextension_clear(PyObject* m)
{
	Py_CLEAR(GETSTATE(m)->error);
	return 0;
}
static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"ODC",
	NULL,
	sizeof(struct module_state),
	myextension_methods,
	NULL,
	myextension_traverse,
	myextension_clear,
	NULL
};

PyMethodDef PyPort::PyPortMethods[] = {
	{"pyPublishEventBoolean",
	 PyPort::pyPublishEvent, // fn pointer to wrap
	 METH_VARARGS,
	 "Publish ODC event to subscribed ports"},
	{NULL, NULL, 0, NULL} // sentinel.
};

PyTypeObject PyPort::PyDataPortType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"odc.DataPort",                           /* tp_name */
	sizeof(PyDataPortObject),                 /* tp_basicsize */
	0,                                        /* tp_itemsize */
	0,                                        /* tp_dealloc */
	0,                                        /* tp_print */
	0,                                        /* tp_getattr */
	0,                                        /* tp_setattr */
	0,                                        /* tp_reserved */
	0,                                        /* tp_repr */
	0,                                        /* tp_as_number */
	0,                                        /* tp_as_sequence */
	0,                                        /* tp_as_mapping */
	0,                                        /* tp_hash  */
	0,                                        /* tp_call */
	0,                                        /* tp_str */
	0,                                        /* tp_getattro */
	0,                                        /* tp_setattro */
	0,                                        /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"DataPort objects",                       /* tp_doc */
	0,                                        /* tp_traverse */
	0,                                        /* tp_clear */
	0,                                        /* tp_richcompare */
	0,                                        /* tp_weaklistoffset */
	0,                                        /* tp_iter */
	0,                                        /* tp_iternext */
	PyPortMethods,                            /* tp_methods */
	0,                                        /* tp_members */
	0,                                        /* tp_getset */
	0,                                        /* tp_base */
	0,                                        /* tp_dict */
	0,                                        /* tp_descr_get */
	0,                                        /* tp_descr_set */
	0,                                        /* tp_dictoffset */
	(initproc)PyPort::Py_init,                /* tp_init */
	0,                                        /* tp_alloc */
	PyType_GenericNew,                        /* tp_new */
};



// Constructor for PyPort --------------------------------------
PyPort::PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	//the creation of a new CBPortConf will get the point details
	pConf.reset(new PyPortConf(ConfFilename, ConfOverrides));

	// Just to save a lot of dereferencing..
	MyConf = static_cast<PyPortConf*>(this->pConf.get());

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();

	PyEval_InitThreads();
	Py_Initialize();
	if (PyRun_SimpleString("import sys") != 0)
	{
		LOGERROR("Unable to import python sys library");
		return;
	}
	// Append current working directory to the sys.path variable - so we ca find the module.
	if (PyRun_SimpleString("sys.path.append(\".\")") != 0)
	{
		LOGERROR("Unable to append to sys path in python sys library");
		return;
	}

	// create a new module
	PyObject* module = PyModule_Create(&moduledef);
	PyDateTime_IMPORT;

	// create a new class/type
	PyDataPortType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&PyPort::PyDataPortType) != 0)
	{
		LOGERROR("Unable to create the python class");
		return;
	}
	if (PyModule_AddObject(module, "DataPort", (PyObject*)&PyPort::PyDataPortType) != 0)
	{
		LOGERROR("Unable to addobject to the python class");
	}
}

PyPort::~PyPort()
{

	LOGDEBUG("Destructing PyPort");
	Py_XDECREF(pyFuncEvent);
	if (pyModule != NULL) { Py_DECREF(pyModule); }

	PyPorts.erase(this->pyInstance);
	Py_Finalize();
}

// The ASIO IOS instance is up, our config files have been read and parsed, this is the opportunity to kick off connections and scheduled processes
void PyPort::Build()
{
	//asio::io_service::strand pyStrand(IOMgr.);

	//TODO Do we have to make sure we have the GIL before we do anything?
	const auto pyUniCodeModuleName = PyUnicode_FromString(MyConf->pyModuleName.c_str());

	pyModule = PyImport_Import(pyUniCodeModuleName);
	if (pyModule == nullptr)
	{
		LOGERROR("Could not load Python Module - {}", MyConf->pyModuleName);
		if (PyErr_Occurred()) PyErrOutput();
		return;
	}
	Py_DECREF(pyUniCodeModuleName);

	PyObject* pyDict = nullptr, * pyClass = nullptr;

	// pDict and pFunc are borrowed references
	pyDict = PyModule_GetDict(pyModule);
	if (pyDict == nullptr)
	{
		LOGERROR("Could not load Python Dictionary Reference");
		if (PyErr_Occurred()) PyErrOutput();
		return;
	}
	// Build the name of a callable class
	pyClass = PyDict_GetItemString(pyDict, MyConf->pyClassName.c_str());

	if (pyClass == nullptr)
	{
		LOGERROR("Could not load Python Class Reference - {}", MyConf->pyClassName);
		if (PyErr_Occurred()) PyErrOutput();
		return;
	}
	// Create an instance of the class
	if (pyClass && PyCallable_Check(pyClass))
	{
		auto pyArgs = PyTuple_New(1);
		auto pyObjectName = PyUnicode_FromString(this->Name.c_str());
		PyTuple_SetItem(pyArgs, 0, pyObjectName);

		pyInstance = PyObject_CallObject(pyClass, pyArgs);
		LOGDEBUG("pyObject: {}", (uint64_t)pyInstance);

		PyPorts.emplace(pyInstance, this);

		Py_DECREF(pyArgs);

		if (PyErr_Occurred())
		{
			PyErrOutput();
			return;
		}
	}
	else
	{
		PyErrOutput();
		LOGERROR("pyClass not callable");
		return;
	}

	pyFuncEnable = GetFunction(pyInstance, MyConf->pyFuncEnableName);
	pyFuncDisable = GetFunction(pyInstance, MyConf->pyFuncDisableName);
	pyFuncEvent = GetFunction(pyInstance, MyConf->pyFuncEventName);

	LOGDEBUG("Loaded \"{}\"\n", MyConf->pyModuleName);
}

void PyPort::Enable()
{
	if (enabled) return;
	enabled = true;

	PostPyCall(pyFuncEnable, PyTuple_New(0)); // No passed variables
};

void PyPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	PostPyCall(pyFuncDisable, PyTuple_New(0)); // No passed variables
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

	// pValue reference stolen here:
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

	PostPyCall(pyFuncEvent, pyArgs, pStatusCallback); // Callback will be called when done...
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

//TODO: Static method, that then needs to work out which instance it is so it can call the correct PublishEvent. Might be better to use Bind in passing
// the function call into the array passed to Python??
PyObject* PyPort::pyPublishEvent(PyObject *self, PyObject *args)
{
	//TODO Update to new general event type, so we pass any event. Then decode in Python
	uint32_t index;
	uint32_t value;
	uint32_t quality;
	PyObject *pyTime = nullptr;

	if(!PyPorts.count(self))
	{
		fprintf(stderr, "Callback for unknown PyPort object\n");
		return NULL;
	}
	auto thisPyPort = PyPorts.at(self);

	if(!PyArg_ParseTuple(args, "IIIO:PublishEvent", &index, &value, &quality, &pyTime))
	{
		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		return NULL;
	}

	//     Timestamp time = PyDateTime_to_Timestamp(pyTime);

	//    Binary newmeas(value, quality, time);

	// Update to new format     thisPyPort->PublishEvent(newmeas, index);

	return Py_BuildValue("i", 0);
}

PyObject* PyPort::GetFunction(PyObject* pyInstance, std::string& sFunction)
{
	PyObject* pyFunc = PyObject_GetAttrString(pyInstance, sFunction.c_str());

	if (!pyFunc || PyErr_Occurred() || !PyCallable_Check(pyFunc))
	{
		PyErrOutput();
		LOGERROR("Cannot resolve function \"{}\"\n", sFunction);
		return nullptr;
	}
	return pyFunc;
}

PyObject* PyPort::Py_init(PyObject* self, PyObject* args)
{
	LOGDEBUG("PyPort.__init__ called");
	Py_INCREF(Py_None);
	PyDateTime_IMPORT;
	return Py_None;
}

void PyPort::PyErrOutput()
{
	//TODO Look at https://stackoverflow.com/questions/1796510/accessing-a-python-traceback-from-the-c-api

	PyObject* ptype, * pvalue, * ptraceback;
	PyErr_Fetch(&ptype, &pvalue, &ptraceback);
	//pvalue contains error message
	//ptraceback contains stack snapshot and many other information
	//(see python traceback structure)

	if (pvalue)
	{
		PyObject* pstr = PyObject_Str(pvalue);
		if (pstr)
		{
			const char* err_msg = PyUnicode_AsUTF8(pstr);
			if (err_msg)
			{
				std::cout << "Python Error " << err_msg << std::endl;
				LOGERROR("Python Error {}", err_msg);
			}
		}
		PyErr_Restore(ptype, pvalue, ptraceback);
	}
}

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

// Post a call to a python method, that will be executed by ASIO.
//TODO: Think that we may need to strand protect this to prevent problems. There is also the Python GIL (global interpreter lock) to consider..
void PyPort::PostPyCall(PyObject* pyFunction, PyObject* pyArgs)
{
	pIOS->post([&,pyArgs, pyFunction]
		{
			if (pyFunction && PyCallable_Check(pyFunction))
			{
			      PyObject_CallObject(pyFunction, pyArgs);
			      Py_DECREF(pyArgs);

			      if (PyErr_Occurred())
			      {
			            PyErrOutput();
				}
			}
			else
			{
			      LOGERROR("Python Method is not valid");
			}
		});
}
// Post a call to a python method, that also has a callback attached. As we are not expecting the Python code to take long (not expecting network ops)
// wait for it to complete, and then call any passed callback function.
void PyPort::PostPyCall(PyObject* pyFunction, PyObject* pyArgs, SharedStatusCallback_t pStatusCallback)
{
	pIOS->post([&, pyArgs, pyFunction, pStatusCallback]
		{
			if (pyFunction && PyCallable_Check(pyFunction))
			{
			      PyObject* result = PyObject_CallObject(pyFunction, pyArgs);

			      Py_DECREF(pyArgs);

			      if (PyErr_Occurred())
			      {
			            PyErrOutput();
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
		});
}
void PyPort::ProcessElements(const Json::Value& JSONRoot)
{
	if (JSONRoot.isMember("ModuleName"))
		MyConf->pyModuleName = JSONRoot["ModuleName"].asString();
	if (JSONRoot.isMember("ClassName"))
		MyConf->pyClassName = JSONRoot["ClassName"].asString();
	if (JSONRoot.isMember("FuncEnable"))
		MyConf->pyFuncEnableName = JSONRoot["FuncEnable"].asString();
	if (JSONRoot.isMember("FuncDisable"))
		MyConf->pyFuncDisableName = JSONRoot["FuncDisable"].asString();
	if (JSONRoot.isMember("FuncEventHandler"))
		MyConf->pyFuncEventName = JSONRoot["FuncEventHandler"].asString();
}
