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
 * PyPortManager.cpp
 *
 *  Created on: 25/04/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#include "PyPortManager.h"

PyPortManager::PyPortManager(std::shared_ptr<odc::IOManager> pIOMgr): SyncIOManager(pIOMgr)
{
	//Py_SetProgramName("opendatacon");
	PyEval_InitThreads();
	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append(\".\")");

	/* create a new module */
	PyObject *module = Py_InitModule("odc", PyPort::PyModuleMethods);
	PyDateTime_IMPORT;

	/* create a new class/type */
	//PyDataPortType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&PyPort::PyDataPortType) < 0)
		return;

	PyModule_AddObject(module, "DataPort", (PyObject *)&PyPort::PyDataPortType);
}
