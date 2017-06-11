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
 * PyPortManager.h
 *
 *  Created on: 07/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

class PyPortManager;

#ifndef opendatacon_suite_PyPortManager_h
#define opendatacon_suite_PyPortManager_h

#include <Python.h>
#include <datetime.h>                   // needed for struct tm

#include <opendatacon/IOManager.h>

#include <asio.hpp>
#include <unordered_map>
#include <iostream>
#include "PyPort.h"

class PyPortManager : public odc::SyncIOManager<odc::IOManager>
{
public:
	PyPortManager(std::shared_ptr<odc::IOManager> pIOMgr);
	
	virtual ~PyPortManager() {
		std::cout << "Destructing PyPortManager" << std::endl;
		Py_Finalize();
	};
	
	virtual void Shutdown() {
		
	}
	
	void PyErrOutput() {
		PyErr_Print();
	}
	
	void PostPyCall(PyObject* pyFunction, PyObject* pyArgs) {
		post([pyArgs,pyFunction]{
			if (pyFunction && PyCallable_Check(pyFunction)) {
				PyObject_CallObject(pyFunction, pyArgs);
				
				if (PyErr_Occurred())
				{
					PyErr_Print();
				}
				
			} else {
				// TODO: output error the pyFunction isn't valid
			}
			Py_DECREF(pyArgs);
		});
	}
	
	void PostPyCall(PyObject* pyFunction, PyObject* pyArgs, std::function<void(PyObject*)> callback) {
		post([this,pyArgs,pyFunction,callback]{
			if (pyFunction && PyCallable_Check(pyFunction)) {
				PyObject* result = PyObject_CallObject(pyFunction, pyArgs);
				
				if (PyErr_Occurred())
				{
					PyErrOutput();
				}
				callback(result);
				
			} else {
				callback(nullptr);
			}
			Py_DECREF(pyArgs);
		});
	}
	
private:
	PyPortManager(const PyPortManager &other) = delete;
};

#endif
