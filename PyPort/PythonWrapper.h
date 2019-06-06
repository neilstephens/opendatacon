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

// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
//#define NONVSTESTING

#include <Python.h>
#include <unordered_map>
#include <opendatacon/util.h>


// Hide some of the code to make Logging cleaner
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->debug(__VA_ARGS__);
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->error(__VA_ARGS__);
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("PyPort"))  \
		log->warn(__VA_ARGS__);
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->info(__VA_ARGS__);

using namespace odc;


class PythonWrapper
{

public:
	PythonWrapper(const std::string& aName);
	~PythonWrapper();
	void Build(const std::string& modulename, std::string& pyLoadModuleName, std::string& pyClassName, std::string& PortName);

	void Enable();
	void Disable();

/*	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
*/
	void PyErrOutput();

	std::string Name;
	static PythonWrapper* GetThisFromPythonSelf(PyObject* self)
	{
		if (PyWrappers.count(self))
		{
			return PyWrappers.at(self);
		}
		return nullptr;
	}

protected:
	// Structures used to pass our extension methods to our Python code. Do these need to be port local???
//	static PyMethodDef PyModuleMethods[];
//	static PyMethodDef PyPortMethods[];
//	static PyTypeObject PyDataPortType;

private:
	// Keep track of each PyWrapper so static methods can get access to the correct PyPort instance
	static std::unordered_map<PyObject*, PythonWrapper*> PyWrappers;
	static std::atomic_uint PythonWrapper::InterpreterUseCount; // Used to keep track of Interpreter Setup/Tear down.

	void InitialisePyInterpreter();
	void CreateBasePyModule(const std::string& modulename);
	void ImportModuleAndCreateClassInstance(const std::string& pyModuleName, const std::string& pyClassName, const std::string& PortName);

	// Keep pointers to the methods in out Python code that we want to be able to call.
	PyObject* pyModule = nullptr;
	PyObject* pyInstance = nullptr;
	PyObject* pyFuncEnable = nullptr;
	PyObject* pyFuncDisable = nullptr;
	PyObject* pyFuncEvent = nullptr;

	PyObject* GetFunction(PyObject* pyInstance, const std::string& sFunction);
	void PostPyCall(PyObject* pyFunction, PyObject* pyArgs);
};

#endif /* PYWRAPPER_H_ */
