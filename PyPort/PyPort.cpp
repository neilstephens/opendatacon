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

#include "PyPort.h"
#include <opendnp3/LogLevels.h>
#include <Python.h>
#include <chrono>
#include <ctime>
#include <datetime.h>
#include <time.h>
#include <iomanip>

std::unordered_map<PyObject*, PyPort*> PyPort::PyPorts;

PyPort::PyPort(std::shared_ptr<PyPortManager> Manager, std::string Name, std::string ConfFilename, const Json::Value ConfOverrides):
DataPort(Name, ConfFilename, ConfOverrides),
Manager_(Manager)
{
	//the creation of a new PortConf will get the point details
	pConf.reset(new PyPortConf(ConfFilename, ConfOverrides));
	
	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

PyPort::~PyPort()
{
	std::cout << "Destructing PyPort" << std::endl;
	Py_XDECREF(pyFuncEventBinary);
	if (pyModule != NULL) { Py_DECREF(pyModule); }
	
	PyPorts.erase(this->pyInstance);
}

void PyPort::Enable(){
	if (enabled) return;
	enabled = true;
	
	Manager_->PostPyCall(pyFuncEnable, PyTuple_New(0));
};

void PyPort::Disable(){
	if (!enabled) return;
	enabled = false;
	
	Manager_->PostPyCall(pyFuncDisable, PyTuple_New(0));
};

PyMethodDef PyPort::PyModuleMethods[] = { {NULL} };
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
	"odc.DataPort",             /* tp_name */
	sizeof(PyDataPortObject), /* tp_basicsize */
	0,                         /* tp_itemsize */
	0,                         /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
	"DataPort objects",           /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	PyPortMethods,     /* tp_methods */
	0,                         /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)PyPort::Py_init, /* tp_init */
	0,                         /* tp_alloc */
	PyType_GenericNew,         /* tp_new */
};


Timestamp PyDateTime_to_Timestamp(PyObject* pyTime)
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

	return Timestamp(std::chrono::seconds(my_mktime(&timeinfo)) + std::chrono::microseconds(PyDateTime_DATE_GET_MICROSECOND(pyTime)));
}

PyObject* GetFunction(PyObject* pyInstance, std::string &sFunction) {
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

PyObject* PyPort::Py_init(PyObject *self, PyObject *args) {
	printf("PyPort.__init__ called\n");
	Py_INCREF(Py_None);
	PyDateTime_IMPORT;
	return Py_None;
}

PyObject* PyPort::PublishEventBoolean(PyObject *self, PyObject *args) {
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

PyObject* PyPort::PublishEventInteger(PyObject *self, PyObject *args) {
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
	
	if (PyFloat_Check(pyValue)) {
		value = PyFloat_AsDouble(pyValue);
	} else if (PyInt_Check(pyValue)) {
		value = PyInt_AsLong(pyValue);
	} else {
		return NULL;
	}
	
	Analog newmeas(value, quality, time);
	
	thisPyPort->PublishEvent(newmeas, index);
	
	return Py_BuildValue("i", 0);
}

PyObject* PyPort::PublishEventFloat(PyObject *self, PyObject *args) {
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
	
	if (PyFloat_Check(pyValue)) {
		value = PyFloat_AsDouble(pyValue);
	} else if (PyInt_Check(pyValue)) {
		value = PyInt_AsLong(pyValue);
	} else {
		return NULL;
	}
	
	Analog newmeas(value, quality, time);
	
	thisPyPort->PublishEvent(newmeas, index);
	
	return Py_BuildValue("i", 0);
}

PyObject* PyPort::PublishEventConnectState(PyObject *self, PyObject *args) {
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
	
	if (PyInt_Check(pyValue)) {
		value = PyInt_AsLong(pyValue);
	} else {
		return NULL;
	}
	
	ConnectState newmeas;
	switch (value) {
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
	
	thisPyPort->PublishEvent(newmeas, index);
	
	// call some fn on PyPort
	return Py_BuildValue("i", 0);
}

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

void PyPort::BuildOrRebuild()
{
	PyPortConf* conf = static_cast<PyPortConf*>(pConf.get());
	
	//asio::io_service::strand pyStrand(IOMgr.);
	
	const auto pyModuleName = PyString_FromString(conf->ModuleName.c_str());

	pyModule = PyImport_Import(pyModuleName);
	Py_DECREF(pyModuleName);
	
	PyObject *pyDict = nullptr, *pyClass = nullptr;
	
	// pDict and pFunc are borrowed references
	if (pyModule) {
		pyDict = PyModule_GetDict(pyModule);
	}
	
	// Build the name of a callable class
	if (pyDict) {
		pyClass = PyDict_GetItemString(pyDict, conf->ClassName.c_str());
	}
	
	// Create an instance of the class
	if (pyClass && PyCallable_Check(pyClass))
	{
		auto pyArgs = PyTuple_New(1);
		auto pyObjectName = PyString_FromString(this->Name.c_str());
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
	
	if (pyInstance) {
		pyFuncEnable = GetFunction(pyInstance, conf->FuncEnable);
		pyFuncDisable = GetFunction(pyInstance, conf->FuncDisable);		
		pyFuncEventConnectState = GetFunction(pyInstance, conf->FuncEventBinary);
		pyFuncEventBinary = GetFunction(pyInstance, conf->FuncEventBinary);
		pyFuncEventAnalog = GetFunction(pyInstance, conf->FuncEventAnalog);
		pyFuncEventControlBinary = GetFunction(pyInstance, conf->FuncEventControlBinary);
		pyFuncEventControlAnalog = GetFunction(pyInstance, conf->FuncEventControlAnalog);
	}
	else {
		PyErr_Print();
		fprintf(stderr, "Failed to load \"%s\"\n", conf->ModuleName.c_str());
		return;
	}
}

//Supported types - call templates
std::future<CommandStatus> PyPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName){

	auto pyArgs = PyTuple_New(5);
	auto pyQuality = PyInt_FromLong(meas.quality);
	auto pyTime = PyInt_FromLong(meas.time);
	auto pyIndex = PyInt_FromLong(index);
	auto pySender = PyString_FromString(SenderName.c_str());
	/* pValue reference stolen here: */
	PyTuple_SetItem(pyArgs, 0, (meas.value) ? Py_True : Py_False); // bool value
	PyTuple_SetItem(pyArgs, 1, pyQuality); // uint8_t quality
	PyTuple_SetItem(pyArgs, 2, pyTime); // Timestamp
	PyTuple_SetItem(pyArgs, 3, pyIndex); // uint16_t index
	PyTuple_SetItem(pyArgs, 4, pySender); // string SenderName

	
	Manager_->PostPyCall(pyFuncEventBinary, pyArgs);

	return IOHandler::CommandFutureSuccess();
}

std::future<CommandStatus> PyPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName){return EventT(meas,index,SenderName);}

std::future<CommandStatus> PyPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName){return EventQ(qual,index,SenderName);}
std::future<CommandStatus> PyPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName){return EventQ(qual,index,SenderName);}

//Templates for supported types
template<typename T>
inline std::future<CommandStatus> PyPort::EventQ(const T& meas, uint16_t index, const std::string& SenderName)
{
	return IOHandler::CommandFutureUndefined();
}

template<typename T>
inline std::future<CommandStatus> PyPort::EventT(const T& meas, uint16_t index, const std::string& SenderName)
{
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}
	auto pConf = static_cast<PyPortConf*>(this->pConf.get());

	return IOHandler::CommandFutureNotSupported();
}
