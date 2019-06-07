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
 * PyPortPythonSupport.cpp
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


#include "PythonWrapper.h"
#include <chrono>
#include <ctime>
#include <datetime.h> //PyDateTime
#include <time.h>
#include <iomanip>
#include <exception>
#include "PyPort.h"


using namespace odc;

msSinceEpoch_t PyDateTime_to_msSinceEpoch_t(PyObject* pyTime);

std::atomic_uint PythonWrapper::InterpreterUseCount = 0;
std::unordered_map<uint64_t, int> PythonWrapper::PyWrappers;

// Wrap getting/releasing the GIL to be bullet proof.
class GetPythonGIL
{
	PyGILState_STATE gstate;

public:
	GetPythonGIL()
	{
		gstate = PyGILState_Ensure();
	}
	~GetPythonGIL()
	{
		PyGILState_Release(gstate);
	}
};

#pragma region odc support module
// This is where we expose ODC methods to our script, so they can be called by the script when needed.

// This is an extension method that we have provided to our embedded Python. It will feed a message into our logging framework
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_log(PyObject* self, PyObject* args)
{
	uint32_t logtype;
	uint64_t guid;
	const char* message;

	// Now parse the arguments provided, one Unsigned int (I) and a string (s) and the function name. DO NOT GET THIS WRONG!
	if (!PyArg_ParseTuple(args, "LIs:log",&guid, &logtype, &message))
	{
		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		return NULL;
	}

	std::string WholeMessage = "pyLog - ";

	// Work out which instance of our PyWrapper is talking to us.
	PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

	LOGDEBUG("Lookup pyObject: {:#x}, {:#x}", guid, (uint64_t)thisPyWrapper);

	if (thisPyWrapper != nullptr)
	{
		WholeMessage += thisPyWrapper->Name + " - ";
	}
	else
	{
		LOGDEBUG("odc.Log called from Python code for unknown PyPort object");
	}

	WholeMessage += message;

	// Take appropriate action
	if (auto log = odc::spdlog_get("PyPort"))
	{
		// Ordinals match spdlog values - spdlog::level::level_enum::
		switch (logtype)
		{
			case 0: log->trace(WholeMessage);
				break;
			case 1:
				log->debug(WholeMessage);
				break;
			case 2:
				log->info(WholeMessage);
				break;
			case 3:
				log->warn(WholeMessage);
				break;
			case 4:
				log->error(WholeMessage);
				break;
			default:
				log->critical(WholeMessage);
		}
	}

	// Return a PyObject return value, in this case "none".
	return Py_BuildValue("");
}

// This is an extension method that we have provided to our embedded Python. It will post an event into the ODC bus.
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_PublishEvent(PyObject* self, PyObject* args)
{
	//TODO Update to new general event type, so we pass any event. Then decode in Python
	uint32_t index;
	uint32_t value;
	uint32_t quality;
	uint64_t guid;
	PyObject* pyTime = nullptr;

	// Now parse the arguments provided, three Unsigned ints (I) and a pyObject (O) and the function name.
	if (!PyArg_ParseTuple(args, "LIIIO:PublishEvent", &guid, &index, &value, &quality, &pyTime))
	{
		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		return NULL;
	}
	LOGDEBUG("Python Publish Event Index: {}, Value {}, Quality {}", index, value, quality);
	// Take appropriate action

	//     Timestamp time = PyDateTime_to_Timestamp(pyTime);

	//    Binary newmeas(value, quality, time);

	// Update to new format     thisPyPort->PublishEvent(newmeas, index);

	// Return a PyObject return value.
	return Py_BuildValue("i", 0);
}

static PyMethodDef odcMethods[] = {
	{"log", odc_log, METH_VARARGS, "Process a log message, level and string message"},
	{"PublishEvent", odc_PublishEvent, METH_VARARGS, "Publish ODC event to subscribed ports"},
	{NULL, NULL, 0, NULL}
};

static PyModuleDef odcModule = {
	PyModuleDef_HEAD_INIT, "odc", NULL, -1, odcMethods,   NULL, NULL, NULL, NULL
};

static PyObject* PyInit_odc(void)
{
	return PyModule_Create(&odcModule);
}

// Load the module into the python interpreter before we initialise it.
void ImportODCModule()
{
	if (PyImport_AppendInittab("odc", &PyInit_odc) != 0)
	{
		LOGERROR("Unable to import odc module to Python Interpreter");
	}
}

#pragma endregion

#pragma region Startup/Setup Functions

PythonWrapper::PythonWrapper(const std::string& aName):
	Name(aName)
{
	if (InterpreterUseCount == 0)
	{
		InterpreterUseCount++;
		InitialisePyInterpreter(); // We only need one for a running ODC instance...
	}
}

PythonWrapper::~PythonWrapper()
{
	LOGDEBUG("Destructing PythonWrapper");

	Py_XDECREF(pyFuncConfig);
	Py_XDECREF(pyFuncEvent);
	Py_XDECREF(pyFuncEnable);
	Py_XDECREF(pyFuncDisable);

	RemoveWrapperMapping();
	Py_XDECREF(pyInstance);

	if (pyModule != nullptr)
	{
		Py_DECREF(pyModule);
	}
	// Only shut down the interpreter when there are no more users left.
	InterpreterUseCount--;
	if (InterpreterUseCount == 0)
	{
		LOGDEBUG("Py_Finalize");
		if (PyGILState_Check() == 1)
		{
			Py_Finalize();
		}
		else
		{
			GetPythonGIL g;
			Py_Finalize();
		}
	}
}
// Startup the interpreter - need to have matching tear down in destructor.
void PythonWrapper::InitialisePyInterpreter()
{
	PyEval_InitThreads(); // Setup interpreter to support threading. We are using a strand to sync access to the interpreter, but still need this.

	LOGDEBUG("Py_Initialize");

	// Load our odc module exposing our internal methods to python (i.e. loggin commands)
	ImportODCModule();

	Py_Initialize(); // Get the Python interpreter running

	// Now execute some commands to get the environment ready.
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
	PyDateTime_IMPORT;
}


void PythonWrapper::ImportModuleAndCreateClassInstance(const std::string& pyModuleName, const std::string& pyClassName, const std::string& PortName)
{
	GetPythonGIL g;

	const auto pyUniCodeModuleName = PyUnicode_FromString(pyModuleName.c_str());

	pyModule = PyImport_Import(pyUniCodeModuleName);
	if (pyModule == nullptr)
	{
		LOGERROR("Could not load Python Module - {}", pyModuleName);
		PyErrOutput();
		throw std::runtime_error::runtime_error("Could not load Python Module");
	}
	Py_DECREF(pyUniCodeModuleName);

	PyObject* pyDict = PyModule_GetDict(pyModule);
	if (pyDict == nullptr)
	{
		LOGERROR("Could not load Python Dictionary Reference");
		PyErrOutput();
		throw std::runtime_error::runtime_error("Could not load Python Dictionary Reference");
	}

	// Build the name of a callable class
	PyObject* pyClass = PyDict_GetItemString(pyDict, pyClassName.c_str());

	// Py_XDECREF(pyDict);	// Borrowed reference, dont destruct

	if (pyClass == nullptr)
	{
		LOGERROR("Could not load Python Class Reference - {}", pyClassName);
		PyErrOutput();
		throw std::runtime_error::runtime_error("Could not load Python Class");
	}
	// Create an instance of the class, with a name matching the port name
	if (pyClass && PyCallable_Check(pyClass))
	{
		auto pyArgs = PyTuple_New(2);
		auto pyguid = PyLong_FromUnsignedLongLong((uint64_t)this); // Pass a this pointer into our constructor, so we can idenify ourselves on calls back into C land
		auto pyObjectName = PyUnicode_FromString(PortName.c_str());
		PyTuple_SetItem(pyArgs, 0, pyguid);
		PyTuple_SetItem(pyArgs, 1, pyObjectName);

		pyInstance = PyObject_CallObject(pyClass, pyArgs);
		if (pyInstance == nullptr)
		{
			LOGDEBUG("Could not get instance pointer for Python Class");
			PyErrOutput();

			throw std::runtime_error::runtime_error("Could not get instance pointer for Python Class");
		}

		StoreWrapperMapping();

		Py_DECREF(pyArgs); // pyObjectName, pyguid is stolen into pyArgs, so dealt with in this call
	}
	else
	{
		PyErrOutput();
		LOGERROR("pyClass not callable");
		throw std::runtime_error::runtime_error("pyClass not callable");
	}
	// Py_XDECREF(pyClass);	// Borrowed reference, dont destruct

	pyFuncConfig = GetFunction(pyInstance, "Config");
	pyFuncEnable = GetFunction(pyInstance, "Enable");
	pyFuncDisable = GetFunction(pyInstance, "Disable");
	pyFuncEvent = GetFunction(pyInstance, "EventHandler");
	//TODO: Call the config function in script ProcessJSONConfig(self, MainJSON, OverrideJSON)
}

void PythonWrapper::Build(const std::string& modulename, std::string& pyLoadModuleName, std::string& pyClassName, std::string& PortName)
{
	// Throws exceptions on fail
	ImportModuleAndCreateClassInstance(pyLoadModuleName, pyClassName, PortName);
}

void PythonWrapper::Config(const std::string& JSONMain, const std::string& JSONOverride)
{
	GetPythonGIL g;

	auto pyArgs = PyTuple_New(2);
	auto pyJSONMain = PyUnicode_FromString(JSONMain.c_str());
	auto pyJSONOverride = PyUnicode_FromString(JSONOverride.c_str());
	PyTuple_SetItem(pyArgs, 0, pyJSONMain);
	PyTuple_SetItem(pyArgs, 1, pyJSONOverride);

	PyObject* pyResult = PyCall(pyFuncConfig, pyArgs); // No passed variables, assume no delayed return
	if (pyResult) Py_DECREF(pyResult);
	Py_DECREF(pyArgs);
}

void PythonWrapper::Enable()
{
	GetPythonGIL g;
	auto pyArgs = PyTuple_New(0);
	PyObject* pyResult = PyCall(pyFuncEnable, pyArgs); // No passed variables, assume no delayed return
	if (pyResult) Py_DECREF(pyResult);
	Py_DECREF(pyArgs);
};

void PythonWrapper::Disable()
{
	GetPythonGIL g;
	auto pyArgs = PyTuple_New(0);
	PyObject* pyResult = PyCall(pyFuncDisable, pyArgs); // No passed variables, assume no delayed return
	if (pyResult) Py_DECREF(pyResult);
	Py_DECREF(pyArgs);
};

// When we get an event, we expect the Python code to act on it, and we get back a response straight away. PyPort will Post the result from us.
CommandStatus PythonWrapper::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName)
{
	// Try and send all events through to Python without modification, return value will be success or failure - using our predefined values.
	GetPythonGIL g;

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
	PyObject* pyResult = PyCall(pyFuncEvent, pyArgs); // No passed variables

	if (pyResult) Py_DECREF(pyResult);
	Py_DECREF(pyArgs);

	return CommandStatus::SUCCESS;
}
#pragma region


#pragma region WorkerFunctions

// Get a PyObject handle for the function name given, in the Python instance given.
PyObject* PythonWrapper::GetFunction(PyObject* pyInstance, const std::string& sFunction)
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

void PythonWrapper::PyErrOutput()
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
			Py_DECREF(pstr);

			if (err_msg)
			{
				std::cout << "Python Error " << err_msg << std::endl;
				LOGERROR("Python Error {}", err_msg);
			}
		}
		PyErr_Restore(ptype, pvalue, ptraceback); //TODO: Do we need to do this or DECREF the variables?
	}
}

PyObject* PythonWrapper::PyCall(PyObject* pyFunction, PyObject* pyArgs)
{
	if (pyFunction && PyCallable_Check(pyFunction))
	{
		PyObject* Result = PyObject_CallObject(pyFunction, pyArgs);
		PyErrOutput();
		return Result;
	}
	else
	{
		LOGERROR("Python Method is not valid");
		return nullptr;
	}
}

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

#pragma endregion

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
