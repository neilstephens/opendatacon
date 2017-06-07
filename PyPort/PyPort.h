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

#ifndef PYPORT_H_
#define PYPORT_H_

#include <unordered_map>
#include <opendatacon/DataPort.h>

#include <Python.h>

#include "PyPortConf.h"
#include "PyPortFactory.h"
#include "PyPortManager.h"

using namespace odc;

class PyPort: public DataPort
{
	friend PyPortManager;
public:
	PyPort(std::shared_ptr<PyPortManager> Manager, std::string Name, std::string ConfFilename, const Json::Value ConfOverrides);
	~PyPort();

	void ProcessElements(const Json::Value& JSONRoot);
	
	virtual void Enable() override;
	virtual void Disable() override;

	void BuildOrRebuild();

	template<typename T> std::future<CommandStatus> EventT(const T& meas, uint16_t index, const std::string& SenderName);
	template<typename T> std::future<CommandStatus> EventQ(const T& qual, uint16_t index, const std::string& SenderName);

	//virtual std::future<CommandStatus> Event(const ConnectState& state, uint16_t index, const std::string& SenderName);
	
	virtual std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName);
	virtual std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName);

	virtual std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName);
	virtual std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName);

protected:
	static PyMethodDef PyModuleMethods[];
	static PyMethodDef PyPortMethods[];
	static PyTypeObject PyDataPortType;

	typedef struct {
		PyObject_HEAD
		/* Type-specific fields go here. */
	} PyDataPortObject;

private:
	std::shared_ptr<PyPortManager> Manager_;

	/// Python interface definition
	static PyObject* Py_init(PyObject *self, PyObject *args);	
	static PyObject* PublishEventBoolean(PyObject *self, PyObject *args);
	static PyObject* PublishEventInteger(PyObject *self, PyObject *args);
	static PyObject* PublishEventFloat(PyObject *self, PyObject *args);
	static PyObject* PublishEventConnectState(PyObject *self, PyObject *args);

	PyObject *pyModule = nullptr;
	PyObject *pyInstance = nullptr;
	PyObject *pyFuncEnable = nullptr;
	PyObject *pyFuncDisable = nullptr;
	PyObject *pyFuncEventConnectState = nullptr;
	PyObject *pyFuncEventBinary = nullptr;
	PyObject *pyFuncEventAnalog = nullptr;
	PyObject *pyFuncEventControlBinary = nullptr;
	PyObject *pyFuncEventControlAnalog = nullptr;
	
	static std::unordered_map<PyObject*, PyPort*> PyPorts;
};

#endif /* PYPORT_H_ */
