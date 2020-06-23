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
#include "PyPort.h"
#include <Python.h>
#include <chrono>
#include <ctime>
#include <ctime>
#include <datetime.h> //PyDateTime
#include <exception>
#include <exception>
#include <frameobject.h>
#include <iomanip>
#include <opendatacon/IOTypes.h>
#include <opendatacon/Platform.h>
#include <utility>
#include <whereami++.h>


using namespace odc;

PyThreadState* PythonInitWrapper::threadState;

std::unordered_set<uint64_t> PythonWrapper::PyWrappers;
std::shared_timed_mutex PythonWrapper::WrapperHashMutex;

// Wrap getting/releasing the GIL to be bullet proof.
class GetPythonGIL
{
	PyGILState_STATE gstate;

public:
	GetPythonGIL()
	{
		#ifdef PYTHON3_7
		if (!_Py_IsFinalizing())
		#endif
		gstate = PyGILState_Ensure();
	}
	bool OkToContinue()
	{
		#ifdef PYTHON3_7
		return !_Py_IsFinalizing(); // If the interpreter is shutting down, alert our code
		#else
		return true;
		#endif
	}
	~GetPythonGIL()
	{
		#ifdef PYTHON3_7
		if (!_Py_IsFinalizing())
		#endif
		PyGILState_Release(gstate);
	}
};

#ifdef _MSC_VER
#pragma region odc support module
#endif
// This is where we expose ODC methods to our script, so they can be called by the script when needed.

// This is an extension method that we have provided to our embedded Python. It will feed a message into our logging framework
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_log(PyObject* self, PyObject* args)
{
	try
	{
		uint32_t logtype;
		uint64_t guid;
		const char* message;

		// Now parse the arguments provided, one Unsigned int (I) and a string (s) and the function name. DO NOT GET THIS WRONG!
		if (!PyArg_ParseTuple(args, "LIs:log", &guid, &logtype, &message))
		{
			PythonWrapper::PyErrOutput();
			Py_RETURN_NONE;
		}

		std::string WholeMessage = "pyLog - ";

		// Work out which instance of our PyWrapper is talking to us.
		PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

		if (thisPyWrapper)
		{
			WholeMessage += thisPyWrapper->Name + " - ";
		}
		else
		{
			LOGDEBUG("odc.Log called from Python code for unknown PyPort object - {0:#x}", guid);
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
	}
	catch (std::exception& e)
	{
		LOGERROR("Excception Caught in odc_log - {}", e.what());
	}
	// Return a PyObject return value, in this case "none".
	Py_RETURN_NONE;
}

// This is an extension method that we have provided to our embedded Python. It will post an event into the ODC bus.
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_PublishEvent(PyObject* self, PyObject* args)
{
	try
	{
		uint32_t ODCIndex;
		const char* EventType;
		const char* Payload;
		const char* Quality;
		uint64_t guid;

		// Now parse the arguments provided, three Unsigned ints (I) and a pyObject (O) and the function name.
		if (!PyArg_ParseTuple(args, "LsIss:PublishEvent", &guid, &EventType, &ODCIndex, &Quality, &Payload))
		{
			PythonWrapper::PyErrOutput();
			Py_RETURN_NONE; // This will throw an execption in the python code.
		}

		// Work out which instance of our PyWrapper is talking to us.
		PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

		if (thisPyWrapper)
		{
			// Will create an async wait and call the Python code at the correct time.
			// At constrution, we have passed in a pointer to the PyPort SetTimer method, so we can call it
			// The PyPort ensures that pyWrapper is managed within a strand
			LOGDEBUG("Python Publish Event {}, {}, {}, {}", EventType, ODCIndex, Quality, Payload);
			auto fn = thisPyWrapper->GetPythonPortPublishEventCallFn();
			fn(EventType, ODCIndex, Quality, Payload);
		}
		else
		{
			LOGDEBUG("odc.PublishEvent called from Python code for unknown PyPort object - ignored");
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Excception Caught in odc_PublishEvent() - {}", e.what());
	}
	// Return a PyObject return value. True is 1
	return Py_BuildValue("i", 1);
}

// This is an extension method that we have provided to our embedded Python. It get the current EventQueue length.
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_GetEventQueueSize(PyObject* self, PyObject* args)
{
	try
	{
		uint64_t guid;

		// Now parse the arguments provided, three Unsigned ints (I) and a pyObject (O) and the function name.
		if (!PyArg_ParseTuple(args, "L:PublishEvent", &guid))
		{
			PythonWrapper::PyErrOutput();
			Py_RETURN_NONE; // This will throw an execption in the python code.
		}

		// Work out which instance of our PyWrapper is talking to us.
		PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

		if (thisPyWrapper)
		{
			// At constrution, we have passed in a pointer to the PyPort SetTimer method, so we can call it
			// The PyPort ensures that pyWrapper is managed within a strand
			size_t QueueSize = thisPyWrapper->GetEventQueueSize();
			return Py_BuildValue("i", QueueSize);
		}
		else
		{
			LOGDEBUG("odc.GetEventQueueSize called from Python code for unknown PyPort object - ignored");
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Excception Caught in odc_GetEventQueueSize() - {}", e.what());
	}
	// Return a PyObject return value. True is 1
	return Py_BuildValue("i",0);
}
// This is an extension method that we have provided to our embedded Python. It will post an event into the ODC bus.
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_SetTimer(PyObject* self, PyObject* args)
{
	try
	{
		uint32_t id;
		uint32_t delay;
		uint64_t guid;

		// Now parse the arguments provided, Long (L) Unsigned ints (I) and the function name.
		if (!PyArg_ParseTuple(args, "LII:SetTimer", &guid, &id, &delay))
		{
			PythonWrapper::PyErrOutput();
			Py_RETURN_NONE;
		}

		// Work out which instance of our PyWrapper is talking to us.
		PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

		if (thisPyWrapper)
		{
			// Will create an async wait and call the Python code at the correct time.
			// At constrution, we have passed in a pointer to the PyPort SetTimer method, so we can call it
			// The PyPort ensures that pyWrapper is managed within a strand
			//LOGTRACE("Python SetTimer ID: {}, Delay {}", id, delay);
			auto fn = thisPyWrapper->GetPythonPortSetTimerFn();
			fn(id, delay);
		}
		else
		{
			LOGDEBUG("odc.setTimer called from Python code for unknown PyPort object - Ignored");
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Excception Caught in odc_SetTimer - {}", e.what());
	}
	// Return a PyObject return value, in this case "none".
	Py_RETURN_NONE;
}

// This is an extension method that we have provided to our embedded Python. It will post an event into the ODC bus.
// It is static, so we have to work out which instance of the PythonWrapper class should handle it.
static PyObject* odc_GetNextEvent(PyObject* self, PyObject* args)
{
	try
	{
		uint64_t guid;

		// Now parse the arguments provided
		if (!PyArg_ParseTuple(args, "L:GetNextEvent", &guid))
		{
			PythonWrapper::PyErrOutput();
			// Will cause Pyhton exception
			Py_RETURN_NONE;
		}

		// Work out which instance of our PyWrapper is talking to us.
		PythonWrapper* thisPyWrapper = PythonWrapper::GetThisFromPythonSelf(guid);

		if (thisPyWrapper)
		{
			// Will create an async wait and call the Python code at the correct time.
			// At constrution, we have passed in a pointer to the PyPort SetTimer method, so we can call it
			// The PyPort ensures that pyWrapper is managed within a strand
			std::string eq = "";
			bool empty = !thisPyWrapper->DequeueEvent(eq);
			//LOGDEBUG("GetNextEvent {}", eq);

			auto pyArgs = PyTuple_New(2);
			auto pyJSONType = PyUnicode_FromString(eq.c_str()); // json string
			auto pyEmpty = PyBool_FromLong(empty);
			// The py values above are stolen into the pyArgs structure - so only need to release pyArgs
			PyTuple_SetItem(pyArgs, 0, pyJSONType);
			PyTuple_SetItem(pyArgs, 1, pyEmpty);

			return pyArgs;
		}
		else
		{
			LOGDEBUG("odc.GetNextEvent called from Python code for unknown PyPort object - ignored");
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Excception Caught in odc_GetNextEvent() - {}", e.what());
	}

	Py_RETURN_NONE; // This will throw an execption in the python code.
}

static PyMethodDef odcMethods[] = {
	{"log", odc_log, METH_VARARGS, "Process a log message, level and string message"},
	{"PublishEvent", odc_PublishEvent, METH_VARARGS, "Publish ODC event to subscribed ports"},
	{"SetTimer", odc_SetTimer, METH_VARARGS, "Set a Timer Callback up"},
	{"GetNextEvent", odc_GetNextEvent, METH_VARARGS, "Get the next event from the queue - return None if empty"},
	{"GetEventQueueSize", odc_GetEventQueueSize, METH_VARARGS, "How many elements are there in the event queue - return None if empty"},
	{nullptr, nullptr, 0, nullptr}
};

static PyModuleDef odcModule = {
	PyModuleDef_HEAD_INIT, "odc", nullptr, -1, odcMethods,   nullptr, nullptr, nullptr, nullptr
};

static PyObject* PyInit_odc(void)
{
	return PyModule_Create(&odcModule);
}



#ifdef _MSC_VER
#pragma endregion
#pragma region Startup/Setup Functions
#endif

PythonWrapper::PythonWrapper(const std::string& aName, std::shared_ptr<odc::asio_service> _pIOS, SetTimerFnType SetTimerFn, PublishEventCallFnType PublishEventCallFn):
	Name(aName),
	pIOS(std::move(_pIOS)),
	PythonPortSetTimerFn(std::move(SetTimerFn)),
	PythonPortPublishEventCallFn(std::move(PublishEventCallFn))
{
	EventQueue = std::make_shared<SpecialEventQueue<std::string>>(pIOS, MaximumQueueSize);
}

// Load the module into the python interpreter before we initialise it.
void ImportODCModule()
{
	if (PyImport_AppendInittab("odc", &PyInit_odc) != 0)
	{
		LOGERROR("Unable to import odc module to Python Interpreter");
	}
}

// Startup the interpreter - need to have matching tear down in destructor.
PythonInitWrapper::PythonInitWrapper(bool GlobalUseSystemPython)
{
	try
	{
		LOGDEBUG("Py_Initialize");

		// Load our odc module exposing our internal methods to python (i.e. loggin commands)
		ImportODCModule();

		if (!GlobalUseSystemPython) // Use ODC built python instead - what we do for the tests in CI
		{
			#ifdef PYTHON_LIBDIR
			//We've packaged python locally - so we need to enable the python code to find the files
			//whereami::getModulePath() returns the path to libODC (because that's where the code is compiled)
			auto pythonhome = whereami::getModulePath().dirname() + "/" + PYTHON_LIBDIR;
			#ifdef PYTHON_LIBDIRPLAT
			pythonhome += OSPATHSEP + pythonhome + "/" + PYTHON_LIBDIRPLAT;
			#endif
			PlatformSetEnv("PYTHONHOME", pythonhome.c_str(), 1);
			LOGDEBUG("Set PYTHONHOME env var to: '{}'", pythonhome);

			std::string newpythonpath;
			if (auto pythonpath = getenv("PYTHONPATH"))
				newpythonpath = pythonhome + OSPATHSEP + pythonpath;
			else
				newpythonpath = pythonhome;

			PlatformSetEnv("PYTHONPATH", newpythonpath.c_str(), 1);
			LOGDEBUG("Set PYTHONPATH env var to: '{}'", newpythonpath);
			#else
			LOGERROR("OpenDataCon was built without linked in Python support, must be run in UseSystemPython mode - use the follwing in the Python Port config file - GlobalUseSystemPython = true");
			#endif
		}

		/* If you run into troubles with 32/64 bit both installed, replace the path in the debugger option is VS with the path to the required Pythn libs and it will work
		      like this:
		      Environment  path=c:\Python37-32;c:\Python37-32\dlls;c:\python37-32\scripts\;$(LocalDebuggerEnvironment)
		*/

		Py_Initialize(); // Get the Python interpreter running
		LOGDEBUG("Initialised Python");

		#ifndef PYTHON_34_ORLESS
		LOGDEBUG("Python platform independant path prefix: '{}'",Py_EncodeLocale(Py_GetPrefix(),nullptr));
		#endif

		// Now execute some commands to get the environment ready.
		if (PyRun_SimpleString("import sys") != 0)
		{
			LOGERROR("Unable to import python sys library");
			return;
		}

		// Append current working directory to the sys.path variable - so we can find the module.
		if (PyRun_SimpleString("sys.path.append(\".\")") != 0)
		{
			LOGERROR("Unable to append to sys path in python sys library");
			return;
		}

		// Log the Python path for debugging (also write to a small file for running test code)
		std::wstring path = Py_GetPath();
		std::string spath(path.begin(), path.end());
		LOGCRITICAL("Current Python sys.path - {}",spath);

		PyDateTime_IMPORT;

		/* Kafka load test
		if (PyRun_SimpleString("import confluent_kafka") != 0)
		{
		      LOGERROR("Unable to import python confluent_kafka library");
		      PythonWrapper::PyErrOutput();
		      return;
		}
		*/
		// Initialize threads and release GIL (saving it as well):
		PyEval_InitThreads(); // Not needed from 3.7 onwards, done in PyInitialize()
		if (!PyGILState_Check())
			LOGERROR("About to release and save our GIL state - but apparently we dont have a GIL lock...");

		threadState = PyEval_SaveThread(); // save the GIL, which also releases it.
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught during pyInitialize() - {}", e.what());
	}
}

// This one seems to be excatly what we need, and we are doing it, but it is not working...
// https://stackoverflow.com/questions/15470367/pyeval-initthreads-in-python-3-how-when-to-call-it-the-saga-continues-ad-naus
PythonInitWrapper::~PythonInitWrapper()
{
	LOGDEBUG("Py_Finalize");
	// Restore the state as it was after we called Initialize()
	LOGDEBUG("About to Finalize - Have GIL {} ", PyGILState_Check());
	// Supposed to acquire the GIL and restore the state...
	PyEval_RestoreThread(threadState);
	LOGDEBUG("About to Finalize - Have GIL {} ", PyGILState_Check());

	//	GetPythonGIL g; //TODO If we do this we hang, if we dont we get an error saying we dont have the GIL...
	try
	{
		//TODO This try/catch does not work, as when Py_Finalize() fails, it calls abort - which we cannot catch.
		//			if (!Py_FinalizeEx())
		LOGERROR("Python Py_Finalize() Failed");
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling Py_FinalizeEx() - {}", e.what());
	}
}


PythonWrapper::~PythonWrapper()
{
	LOGDEBUG("Destructing PythonWrapper");

	// Scope the GIL lock
	{
		GetPythonGIL g; // Need the GIL to release the module and the instance - and - potentially the Interpreter

		Py_XDECREF(pyFuncConfig);
		Py_XDECREF(pyFuncEvent);
		Py_XDECREF(pyFuncOperational);
		Py_XDECREF(pyFuncEnable);
		Py_XDECREF(pyFuncDisable);
		Py_XDECREF(pyTimerHandler);
		Py_XDECREF(pyRestHandler);

		RemoveWrapperMapping();
		Py_XDECREF(pyInstance);

		if (pyModule)
		{
			Py_DECREF(pyModule);
		}
	}
}

void PythonWrapper::Build(const std::string& modulename, const std::string& pyPathName, const std::string& pyLoadModuleName,
	const std::string& pyClassName, const std::string& PortName, bool GlobalUseSystemPython)
{
	// First we make sure there is a gobal instance of PythonInitWrapper
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<PythonInitWrapper> weak_mgr;

	// If we're the first/only one on the scene, init the PythonInitWrapper
	if (!init_flag.test_and_set(std::memory_order_acquire))
	{
		//make a custom deleter for the PythonInitWrapper that will also clear the init flag
		auto deinit_del = [](PythonInitWrapper* mgr_ptr)
					{init_flag.clear(); delete mgr_ptr; };
		this->PyMgr = std::shared_ptr<PythonInitWrapper>(
			new PythonInitWrapper(GlobalUseSystemPython),
			deinit_del);
		weak_mgr = this->PyMgr;
	}
	else
	{
		//otherwise just make sure it's finished initialising and take a shared_ptr
		while (!(this->PyMgr = weak_mgr.lock()))
		{} //init happens very seldom, so spin lock is good
	}

	// Throws exceptions on fail
	// ImportModuleAndCreateClassInstance
	GetPythonGIL g;

	if (!g.OkToContinue())
	{
		LOGERROR("Error - Interpreter Closing Down in ImportModuleAndCreateClassInstance");
		return;
	}

	// Make sure the path to where the module is, is known to Python
	PyObject* sysPath = PySys_GetObject("path");
	PyObject* programName = PyUnicode_FromString(pyPathName.c_str());
	PyList_Append(sysPath, programName);
	Py_DECREF(programName);

	const auto pyUniCodeModuleName = PyUnicode_FromString(pyLoadModuleName.c_str());

	pyModule = PyImport_Import(pyUniCodeModuleName);
	if (pyModule == nullptr)
	{
		LOGERROR("Could not load Python Module - {} from {}", pyLoadModuleName, pyPathName);
		PyErrOutput();
		throw std::runtime_error("Could not load Python Module");
	}
	Py_DECREF(pyUniCodeModuleName);

	PyObject* pyDict = PyModule_GetDict(pyModule);
	if (pyDict == nullptr)
	{
		LOGERROR("Could not load Python Dictionary Reference");
		PyErrOutput();
		throw std::runtime_error("Could not load Python Dictionary Reference");
	}

	// Build the name of a callable class
	PyObject* pyClass = PyDict_GetItemString(pyDict, pyClassName.c_str());

	// Py_XDECREF(pyDict);	// Borrowed reference, dont destruct

	if (pyClass == nullptr)
	{
		LOGERROR("Could not load Python Class Reference - {}", pyClassName);
		PyErrOutput();
		throw std::runtime_error("Could not load Python Class");
	}
	// Create an instance of the class, with a name matching the port name
	if (pyClass && PyCallable_Check(pyClass))
	{
		auto pyArgs = PyTuple_New(2);
		auto pyguid = PyLong_FromVoidPtr(this); // Pass a this pointer into our constructor, so we can idenify ourselves on calls back into C land
		auto pyObjectName = PyUnicode_FromString(PortName.c_str());
		PyTuple_SetItem(pyArgs, 0, pyguid);
		PyTuple_SetItem(pyArgs, 1, pyObjectName);

		pyInstance = PyObject_CallObject(pyClass, pyArgs);
		if (pyInstance == nullptr)
		{
			LOGDEBUG("Could not get instance pointer for Python Class");
			PyErrOutput();

			throw std::runtime_error("Could not get instance pointer for Python Class");
		}

		StoreWrapperMapping();

		Py_DECREF(pyArgs); // pyObjectName, pyguid is stolen into pyArgs, so dealt with in this call
	}
	else
	{
		PyErrOutput();
		LOGERROR("pyClass not callable");
		throw std::runtime_error("pyClass not callable");
	}
	// Py_XDECREF(pyClass);	// Borrowed reference, dont destruct

	pyFuncConfig = GetFunction(pyInstance, "Config");
	pyFuncOperational = GetFunction(pyInstance, "Operational");
	pyFuncEnable = GetFunction(pyInstance, "Enable");
	pyFuncDisable = GetFunction(pyInstance, "Disable");
	pyFuncEvent = GetFunction(pyInstance, "EventHandler");
	pyTimerHandler = GetFunction(pyInstance, "TimerHandler");
	pyRestHandler = GetFunction(pyInstance, "RestRequestHandler");
}

void PythonWrapper::Config(const std::string& JSONMain, const std::string& JSONOverride)
{
	GetPythonGIL g;
	if (!g.OkToContinue())
	{
		LOGERROR("Error - Interpreter Closing Down in Config");
		return;
	}

	auto pyArgs = PyTuple_New(2);
	auto pyJSONMain = PyUnicode_FromString(JSONMain.c_str());
	auto pyJSONOverride = PyUnicode_FromString(JSONOverride.c_str());
	PyTuple_SetItem(pyArgs, 0, pyJSONMain);
	PyTuple_SetItem(pyArgs, 1, pyJSONOverride);

	PyObject* pyResult = PyCall(pyFuncConfig, pyArgs); // No passed variables, assume no delayed return
	if (pyResult) Py_DECREF(pyResult);
	Py_DECREF(pyArgs);
}
void PythonWrapper::PortOperational()
{
	try
	{
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error - Interpreter Closing Down in PortOperational");
			return;
		}
		auto pyArgs = PyTuple_New(0);
		PyObject* pyResult = PyCall(pyFuncOperational, pyArgs); // No passed variables, assume no delayed return
		if (pyResult) Py_DECREF(pyResult);
		Py_DECREF(pyArgs);
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyPortOperational() - {}", e.what());
	}
}
void PythonWrapper::Enable()
{
	try
	{
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error - Interpreter Closing Down in Enable");
			return;
		}
		auto pyArgs = PyTuple_New(0);
		PyObject* pyResult = PyCall(pyFuncEnable, pyArgs); // No passed variables, assume no delayed return
		if (pyResult) Py_DECREF(pyResult);
		Py_DECREF(pyArgs);
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyEnable() - {}", e.what());
	}
}

void PythonWrapper::Disable()
{
	try
	{
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error - Interpreter Closing Down in Disable");
			return;
		}
		auto pyArgs = PyTuple_New(0);
		PyObject* pyResult = PyCall(pyFuncDisable, pyArgs); // No passed variables, assume no delayed return
		if (pyResult) Py_DECREF(pyResult);
		Py_DECREF(pyArgs);
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyDisable() - {}", e.what());
	}
}

// these two methods are the only ones that touch the event queue. So to change the queue, do it here.
// This is not synced with the strand when called. So the queue needs to be multi-producer capable
void PythonWrapper::QueueEvent(const std::string& jsonevent)
{
	bool result = EventQueue->async_push(jsonevent);

	if (!result)
	{
		if (!QueuePushErrorLogged.test_and_set())
		{
			size_t qsize = EventQueue->Size();
			LOGERROR("Failed to enqueue item into Event queue - insufficient memory or queue full. Queue Size {}", qsize);
		}
	}
	else
	{
		QueuePushErrorLogged.clear();
	}
}

bool PythonWrapper::DequeueEvent(std::string& eq)
{
	std::shared_ptr<std::string> data = EventQueue->pop();
	if (data == nullptr)
		return false;
	eq = *data;
	return true;
}

// When we get an event, we expect the Python code to act on it, and we get back a response straight away. PyPort will Post the result from us.
// This method is synced with the asio strand in PyPort
CommandStatus PythonWrapper::Event(const std::shared_ptr<const EventInfo>& odcevent, const std::string& SenderName)
{
	try
	{
		// Try and send all events through to Python without modification, return value will be success or failure - using our predefined values.
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error - Interpreter Closing Down in Event");
			return CommandStatus::UNDEFINED;
		}
		auto pyArgs = PyTuple_New(6);
		auto pyEventType = PyUnicode_FromString(odc::ToString(odcevent->GetEventType()).c_str()); // String Event Type
		auto pyIndex = PyLong_FromSize_t(odcevent->GetIndex());
		auto pyTime = PyLong_FromUnsignedLongLong(odcevent->GetTimestamp());             // msSinceEpoch
		auto pyQuality = PyUnicode_FromString(ToString(odcevent->GetQuality()).c_str()); // String quality flags
		auto pyPayload = PyUnicode_FromString(odcevent->GetPayloadString().c_str());
		auto pySender = PyUnicode_FromString(SenderName.c_str());

		// The py values above are stolen into the pyArgs structure - so only need to release pyArgs
		PyTuple_SetItem(pyArgs, 0, pyEventType);
		PyTuple_SetItem(pyArgs, 1, pyIndex);
		PyTuple_SetItem(pyArgs, 2, pyTime);
		PyTuple_SetItem(pyArgs, 3, pyQuality);
		PyTuple_SetItem(pyArgs, 4, pyPayload);
		PyTuple_SetItem(pyArgs, 5, pySender);

		//	PostPyCall(pyFuncEvent, pyArgs, pStatusCallback); // Callback will be called when done...
		PyObject* pyResult = PyCall(pyFuncEvent, pyArgs); // No passed variables

		Py_DECREF(pyArgs);

		if (pyResult) // Non nullptr is a result
		{
			long res = PyLong_AsLong(pyResult);
			PyErrOutput();
			Py_DECREF(pyResult);

			return res ? CommandStatus::SUCCESS : CommandStatus::UNDEFINED;
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyFuncEvent() - {}", e.what());
	}
	return CommandStatus::UNDEFINED;
}

// We have to call this from the PyPort as an asyncwait on the strand. We just pass in the ID, as the delay will have been used.
void PythonWrapper::CallTimerHandler(uint32_t id)
{
	try
	{
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error - Interpreter Closing Down in SetTimer");
			return;
		}
		auto pyArgs = PyTuple_New(1);
		auto pyID = PyLong_FromUnsignedLong(id);

		// The py values above are stolen into the pyArgs structure - I think, so only need to release pyArgs
		PyTuple_SetItem(pyArgs, 0, pyID);
		PyObject* pyResult = PyCall(pyTimerHandler, pyArgs); // No passed variables, assume no delayed return
		if (pyResult) Py_DECREF(pyResult);
		Py_DECREF(pyArgs);
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyTimerHandler() - {}", e.what());
	}
}

// This is a handler for a Rest request received by the (global to all PythonPorts) handler, which will pass it to us.
// We will get the url, and return a JSON formatted response. We do not prune the url in any way, just pass it all through.
std::string PythonWrapper::RestHandler(const std::string& url, const std::string& content)
{
	// Try and send all events through to Python without modification, return value will be success or failure - using our predefined values.
	try
	{
		GetPythonGIL g;
		if (!g.OkToContinue())
		{
			LOGERROR("Error in Python Wrapper handling of Restful Request");
			return "Error in Python Wrapper";
		}
		auto pyArgs = PyTuple_New(2);
		auto pyurl = PyUnicode_FromString(url.c_str());
		auto pycontent = PyUnicode_FromString(content.c_str());

		// The py values above are stolen into the pyArgs structure - I think, so only need to release pyArgs
		PyTuple_SetItem(pyArgs, 0, pyurl);
		PyTuple_SetItem(pyArgs, 1, pycontent);

		PyObject* pyResult = PyCall(pyRestHandler, pyArgs);

		Py_DECREF(pyArgs);

		if (pyResult) // Non nullptr is a result
		{
			std::string res = std::string(PyUnicode_AsUTF8(pyResult));
			PyErrOutput();
			Py_DECREF(pyResult);

			return res;
		}
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Caught calling pyRestHandler() - {}", e.what());
	}
	return "Did not get a response from Python RestHandler";
}

#ifdef _MSC_VER
#pragma region
#pragma region WorkerFunctions
#endif

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

void PythonWrapper::DumpStackTrace()
{
	PyThreadState* tstate = PyThreadState_GET();
	if (nullptr != tstate && nullptr != tstate->frame)
	{
		PyFrameObject* frame = tstate->frame;

		LOGERROR("Python stack trace:");
		while (nullptr != frame)
		{
			// int line = frame->f_lineno;
			/*
			 frame->f_lineno will not always return the correct line number
			 you need to call PyCode_Addr2Line().
			*/
			int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
			const char* filename = PyUnicode_AsUTF8(frame->f_code->co_filename);
			const char* funcname = PyUnicode_AsUTF8(frame->f_code->co_name);
			printf("    %s(%d): %s\n", filename, line, funcname);
			LOGERROR(fmt::format("Trace : {}({}): {}", filename, line, funcname));
			frame = frame->f_back;
		}
	}
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
				LOGERROR("Python Error {}", err_msg);
				PythonWrapper::DumpStackTrace();
			}
		}
		PyErr_Restore(ptype, pvalue, ptraceback); // Do we need to do this or DECREF the variables? This seems to work.
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

#ifdef _MSC_VER
#pragma endregion
#endif
