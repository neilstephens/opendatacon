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
 */


// Python Interface:
// We need to call a function Config(JSONString) when we load and process the config file. We are passing a "Python" section of the config,
// which could be any valid JSON.
// We need to call a method when we get Build() called.
// We need to call a Python method every time we get an event, and pass it a callback that it should call when it is finished. Do we need a timeout on how
// long until it responds, then we just respond with a fail.
// Do we need to call a Python like timer method, that will allow the Python code to set how long it should be until it is next called.
// Kind of like an adjustable timer tick. We could do this through ASIO. We could also then use a strand to control access to the Python instance - can
// we have one per core???


#include "PyPort.h"
#include <Python.h>
#include <chrono>
#include <ctime>
#include <datetime.h>
#include <time.h>
#include <iomanip>

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

PyPort::PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	//the creation of a new CBPortConf will get the point details
	pConf.reset(new PyPortConf(ConfFilename, ConfOverrides));

	// Just to save a lot of dereferencing..
	MyConf = static_cast<PyPortConf*>(this->pConf.get());
//	MyPointConf = MyConf->pPointConf;

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();

	//Py_SetProgramName("opendatacon");
	PyEval_InitThreads();
	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append(\".\")");

	/* create a new module */
	PyObject* module = PyModule_Create(&moduledef); // Py_InitModule("odc", PyPort::PyModuleMethods);
	PyDateTime_IMPORT;

	/* create a new class/type */
	//PyDataPortType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&PyPort::PyDataPortType) < 0)
		return;

	PyModule_AddObject(module, "DataPort", (PyObject*)&PyPort::PyDataPortType);
}

PyPort::~PyPort()
{

	LOGDEBUG("Destructing PyPort");
	Py_XDECREF(pyFuncEventBinary);
	if (pyModule != NULL) { Py_DECREF(pyModule); }

	PyPorts.erase(this->pyInstance);
	Py_Finalize();
}

void PyPort::Enable()
{
	if (enabled) return;
	enabled = true;

	//Manager_->PostPyCall(pyFuncEnable, PyTuple_New(0));
};

void PyPort::Disable()
{
	if (!enabled) return;
	enabled = false;

//	Manager_->PostPyCall(pyFuncDisable, PyTuple_New(0));
};

PyMethodDef PyPort::PyModuleMethods[] = {
	{NULL}
};
PyMethodDef PyPort::PyPortMethods[] = {
	{"PublishEventBoolean",
	 PyPort::PublishEventBoolean, // fn pointer to wrap
	 METH_VARARGS,
	 "Publish boolean event to subscribed ports"},
	{"PublishEventInteger",
	 PyPort::PublishEventInteger, // fn pointer to wrap
	 METH_VARARGS,
	 "Publish integer event to subscribed ports"},
	{"PublishEventFloat",
	 PyPort::PublishEventFloat, // fn pointer to wrap
	 METH_VARARGS,
	 "Publish floating point event to subscribed ports"},
	{"PublishEventConnectState",
	 PyPort::PublishEventConnectState, // fn pointer to wrap
	 METH_VARARGS,
	 "Publish connection state change event to subscribed ports"},
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

PyObject* GetFunction(PyObject* pyInstance, std::string &sFunction)
{
	PyObject* pyFunc = PyObject_GetAttrString(pyInstance, sFunction.c_str());

	if (!pyFunc || PyErr_Occurred() || !PyCallable_Check(pyFunc))
	{
		PyErr_Print();
		fprintf(stderr, "Cannot resolve function \"%s\"\n", sFunction.c_str());
		return nullptr;
	}
	return pyFunc;
}

using namespace odc;

PyObject* PyPort::Py_init(PyObject *self, PyObject *args)
{
	printf("PyPort.__init__ called\n");
	Py_INCREF(Py_None);
	PyDateTime_IMPORT;
	return Py_None;
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
	size_t ODCIndex = event->GetIndex();

	// Do we try and send all events through to Python without modification, register a callback, where we will call the callback that we have?
	// Or do we break it up into specific cases.
	switch (event->GetEventType())
	{
		/*
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
		*/

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
}

/*
void CBMasterPort::WriteObject(const ControlRelayOutputBlock& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
      uint8_t Group = 0;
      uint8_t Channel = 0;
      bool exists = MyPointConf->PointTable.GetBinaryControlCBIndexUsingODCIndex(index, Group, Channel);

      if (!exists)
      {
            LOGDEBUG("Master received a Binary Control ODC Change Command on a point that is not defined {}", std::to_string(index));
            PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
            return;
      }

      std::string OnOffString = "ON";

      if ((command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
      {
            OnOffString = "OFF";

            SendDigitalControlOffCommand(MyConf->mAddrConf.OutstationAddr, Group, Channel, pStatusCallback);
      }
      else
      {
            OnOffString = "ON";

            SendDigitalControlOnCommand(MyConf->mAddrConf.OutstationAddr, Group, Channel, pStatusCallback);
      }
      LOGDEBUG("Master received a Binary Control Output Command - Index: {} - {} Group/Channel {}/{}", OnOffString,std::to_string(index), std::to_string(Group), std::to_string(Channel));
}

void CBMasterPort::WriteObject(const int16_t &command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
      LOGDEBUG("Master received an ANALOG CONTROL Change Command {}", std::to_string(index));

      uint8_t Group = 0;
      uint8_t Channel = 0;
      bool exists = MyPointConf->PointTable.GetAnalogControlCBIndexUsingODCIndex(index, Group, Channel);

      if (!exists)
      {
            LOGDEBUG("Master received an Analog Control ODC Change Command on a point that is not defined {}",std::to_string(index));
            PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
            return;
      }

      assert((Channel >= 1) && (Channel <= 2));

      uint16_t BData = numeric_cast<uint16_t>(command);
      uint8_t FunctionCode = FUNC_SETPOINT_A;

      if (Channel == 2)
            FunctionCode = FUNC_SETPOINT_B;

      CBBlockData commandblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, Group, FunctionCode, BData, true);
      QueueCBCommand(commandblock, nullptr);

      // Then queue the execute command - if the previous one does not work, then this one will not either.
      // Bit of a question about how to  feedback failure.
      // Only do the callback on the second - EXECUTE - command.

      CBBlockData executeblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, Group, FUNC_EXECUTE_COMMAND, 0, true);
      QueueCBCommand(executeblock, pStatusCallback);
}


void CBMasterPort::WriteObject(const int32_t & command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
      LOGDEBUG("Master received unknown AnalogOutputInt32 ODC Event - Index {}, Value {}",index,command);
      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::WriteObject(const float& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
      LOGERROR("On Master float Type is not implemented - Index {}, Value {}",index,command);
      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::WriteObject(const double& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{

      LOGDEBUG("Master received unknown double ODC Event - Index {}, Value {}",index,command);
      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}
PyObject* PyPort::PublishEventBoolean(PyObject *self, PyObject *args)
{
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

      if(!PyArg_ParseTuple(args, "IIIO:PublishEventBoolean", &index, &value, &quality, &pyTime))
      {
            if (PyErr_Occurred())
            {
                  PyErr_Print();
            }
            return NULL;
      }

      Timestamp time = PyDateTime_to_Timestamp(pyTime);

      Binary newmeas(value, quality, time);

      thisPyPort->PublishEvent(newmeas, index);

      return Py_BuildValue("i", 0);
}

PyObject* PyPort::PublishEventInteger(PyObject *self, PyObject *args)
{
      uint32_t index;
      double value;
      PyObject *pyValue = nullptr;
      uint32_t quality;
      PyObject *pyTime = nullptr;

      if(!PyPorts.count(self))
      {
            fprintf(stderr, "Callback for unknown PyPort object\n");
            return NULL;
      }
      auto thisPyPort = PyPorts.at(self);

      if(!PyArg_ParseTuple(args, "IOIO:PublishEventInteger", &index, &pyValue, &quality, &pyTime))
      {
            if (PyErr_Occurred())
            {
                  PyErr_Print();

            }
            return NULL;
      }

      Timestamp time = PyDateTime_to_Timestamp(pyTime);

      if (PyFloat_Check(pyValue))
      {
            value = PyFloat_AsDouble(pyValue);
      }
      else if (PyInt_Check(pyValue))
      {
            value = PyInt_AsLong(pyValue);
      }
      else
      {
            return NULL;
      }

      Analog newmeas(value, quality, time);

      thisPyPort->PublishEvent(newmeas, index);

      return Py_BuildValue("i", 0);
}

PyObject* PyPort::PublishEventFloat(PyObject *self, PyObject *args)
{
      uint32_t index;
      double value;
      PyObject *pyValue = nullptr;
      uint32_t quality;
      PyObject *pyTime = nullptr;

      if(!PyPorts.count(self))
      {
            fprintf(stderr, "Callback for unknown PyPort object\n");
            return NULL;
      }
      auto thisPyPort = PyPorts.at(self);

      if(!PyArg_ParseTuple(args, "IOIO:PublishEventInteger", &index, &pyValue, &quality, &pyTime))
      {
            if (PyErr_Occurred())
            {
                  PyErr_Print();

            }
            return NULL;
      }

      Timestamp time = PyDateTime_to_Timestamp(pyTime);

      if (PyFloat_Check(pyValue))
      {
            value = PyFloat_AsDouble(pyValue);
      }
      else if (PyInt_Check(pyValue))
      {
            value = PyInt_AsLong(pyValue);
      }
      else
      {
            return NULL;
      }

      Analog newmeas(value, quality, time);

      thisPyPort->PublishEvent(newmeas, index);

      return Py_BuildValue("i", 0);
}

PyObject* PyPort::PublishEventConnectState(PyObject *self, PyObject *args)
{
      uint32_t index;
      long value;
      PyObject *pyValue = nullptr;
      PyObject *pyTime = nullptr;

      if(!PyPorts.count(self))
      {
            fprintf(stderr, "Callback for unknown PyPort object\n");
            return NULL;
      }
      auto thisPyPort = PyPorts.at(self);

      if(!PyArg_ParseTuple(args, "IOO:PublishEventInteger", &index, &pyValue, &pyTime))
      {
            if (PyErr_Occurred())
            {
                  PyErr_Print();
            }
            return NULL;
      }

      Timestamp time = PyDateTime_to_Timestamp(pyTime);

      if (PyInt_Check(pyValue))
      {
            value = PyInt_AsLong(pyValue);
      }
      else
      {
            return NULL;
      }

      ConnectState newmeas;
      switch (value)
      {
            case 1:
                  newmeas = ConnectState::PORT_UP;
            case 2:
                  newmeas = ConnectState::CONNECTED;
            case 3:
                  newmeas = ConnectState::DISCONNECTED;
            case 4:
                  newmeas = ConnectState::PORT_DOWN;
                  break;
            default:
                  // TODO: error message here
                  return Py_BuildValue("i", 0);
                  break;
      }

      PublishEvent(newmeas, index);

      // call some fn on PyPort
      return Py_BuildValue("i", 0);
}
*/

void PyPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(JSONRoot.isMember("ModuleName"))
		static_cast<PyPortConf*>(pConf.get())->ModuleName = JSONRoot["ModuleName"].asString();
	if(JSONRoot.isMember("ClassName"))
		static_cast<PyPortConf*>(pConf.get())->ClassName = JSONRoot["ClassName"].asString();
	if(JSONRoot.isMember("FuncEnable"))
		static_cast<PyPortConf*>(pConf.get())->FuncEnable = JSONRoot["FuncEnable"].asString();
	if(JSONRoot.isMember("FuncDisable"))
		static_cast<PyPortConf*>(pConf.get())->FuncDisable = JSONRoot["FuncDisable"].asString();
	if(JSONRoot.isMember("FuncEventConnectState"))
		static_cast<PyPortConf*>(pConf.get())->FuncEventConnectState = JSONRoot["FuncEventConnectState"].asString();
	if(JSONRoot.isMember("FuncEventBinary"))
		static_cast<PyPortConf*>(pConf.get())->FuncEventBinary = JSONRoot["FuncEventBinary"].asString();
	if(JSONRoot.isMember("FuncEventAnalog"))
		static_cast<PyPortConf*>(pConf.get())->FuncEventAnalog = JSONRoot["FuncEventAnalog"].asString();
	if(JSONRoot.isMember("FuncEventControlBinary"))
		static_cast<PyPortConf*>(pConf.get())->FuncEventControlBinary = JSONRoot["FuncEventControlBinary"].asString();
	if(JSONRoot.isMember("FuncEventControlAnalog"))
		static_cast<PyPortConf*>(pConf.get())->FuncEventControlAnalog = JSONRoot["FuncEventControlAnalog"].asString();
}

// The ASIO IOS instance is up, our config files have been read and parsed, this is the opportunity to kick off connections and scheduled processes
void PyPort::Build()
{
	PyPortConf* conf = static_cast<PyPortConf*>(pConf.get());

	//asio::io_service::strand pyStrand(IOMgr.);

	const auto pyModuleName = PyUnicode_FromString(conf->ModuleName.c_str());

	pyModule = PyImport_Import(pyModuleName);
	Py_DECREF(pyModuleName);

	PyObject *pyDict = nullptr, *pyClass = nullptr;

	// pDict and pFunc are borrowed references
	if (pyModule)
	{
		pyDict = PyModule_GetDict(pyModule);
	}

	// Build the name of a callable class
	if (pyDict)
	{
		pyClass = PyDict_GetItemString(pyDict, conf->ClassName.c_str());
	}

	// Create an instance of the class
	if (pyClass && PyCallable_Check(pyClass))
	{
		auto pyArgs = PyTuple_New(1);
		auto pyObjectName = PyUnicode_FromString(this->Name.c_str());
		PyTuple_SetItem(pyArgs, 0, pyObjectName); // string SenderName

		pyInstance = PyObject_CallObject(pyClass, pyArgs);
		printf("Object: %li\n", (ssize_t)pyInstance);

		PyPorts.emplace(pyInstance, this);

		Py_DECREF(pyArgs);

		if (PyErr_Occurred())
		{
			PyErr_Print();
			return;
		}
	}
	else
	{
		PyErr_Print();
		fprintf(stderr, "pyClass not callable\n");
		return;
	}

	if (pyInstance)
	{
		pyFuncEnable = GetFunction(pyInstance, conf->FuncEnable);
		pyFuncDisable = GetFunction(pyInstance, conf->FuncDisable);
		pyFuncEventConnectState = GetFunction(pyInstance, conf->FuncEventBinary);
		pyFuncEventBinary = GetFunction(pyInstance, conf->FuncEventBinary);
		pyFuncEventAnalog = GetFunction(pyInstance, conf->FuncEventAnalog);
		pyFuncEventControlBinary = GetFunction(pyInstance, conf->FuncEventControlBinary);
		pyFuncEventControlAnalog = GetFunction(pyInstance, conf->FuncEventControlAnalog);
	}
	else
	{
		PyErr_Print();
		fprintf(stderr, "Failed to load \"%s\"\n", conf->ModuleName.c_str());
		return;
	}
}

/*
//Supported types - call templates
std::future<CommandStatus> PyPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName)
{

      auto pyArgs = PyTuple_New(5);
      auto pyQuality = PyInt_FromLong(meas.quality);
      auto pyTime = PyInt_FromLong(meas.time);
      auto pyIndex = PyInt_FromLong(index);
      auto pySender = PyUnicode_FromString(SenderName.c_str());
      // pValue reference stolen here:
      PyTuple_SetItem(pyArgs, 0, (meas.value) ? Py_True : Py_False); // bool value
      PyTuple_SetItem(pyArgs, 1, pyQuality);                         // uint8_t quality
      PyTuple_SetItem(pyArgs, 2, pyTime);                            // Timestamp
      PyTuple_SetItem(pyArgs, 3, pyIndex);                           // uint16_t index
      PyTuple_SetItem(pyArgs, 4, pySender);                          // string SenderName


      //Manager_->PostPyCall(pyFuncEventBinary, pyArgs);

      return IOHandler::CommandFutureSuccess();
}
*/


void PyPort::PyErrOutput()
{
	PyErr_Print();
}

void PyPort::PostPyCall(PyObject* pyFunction, PyObject* pyArgs)
{
	pIOS->post([&,pyArgs, pyFunction]
		{
			if (pyFunction && PyCallable_Check(pyFunction))
			{
			      PyObject_CallObject(pyFunction, pyArgs);

			      if (PyErr_Occurred())
			      {
			            PyErr_Print();
				}

			}
			else
			{
			// TODO: output error the pyFunction isn't valid
			}
			Py_DECREF(pyArgs);
		});
}

void PyPort::PostPyCall(PyObject* pyFunction, PyObject* pyArgs, std::function<void(PyObject*)> callback)
{
	pIOS->post([&, pyArgs, pyFunction, callback]
		{
			if (pyFunction && PyCallable_Check(pyFunction))
			{
			      PyObject* result = PyObject_CallObject(pyFunction, pyArgs);

			      if (PyErr_Occurred())
			      {
			            PyErrOutput();
				}
			      callback(result);

			}
			else
			{
			      callback(nullptr);
			}
			Py_DECREF(pyArgs);
		});
}