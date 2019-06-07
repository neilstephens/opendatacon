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

// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
//#define NONVSTESTING

#include <Python.h>
#include <unordered_map>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>

#include "PythonWrapper.h"
#include "PyPortConf.h"


typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;

using namespace odc;

class PyPort: public DataPort
{

public:
	PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~PyPort() override;
	void ProcessElements(const Json::Value& JSONRoot) final;

	void Enable() override;
	void Disable() override;
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	// Keep track of each PyPort so static methods can get access to the correct PyPort instance
	static std::unordered_map<PyObject*, PyPort*> PyPorts;

protected:
	// Worker function to try and clean up the code...
	PyPortConf* MyConf;

private:
	std::unique_ptr<PythonWrapper> pWrapper;
	std::string JSONMain;
	std::string JSONOverride;

	// We need one strand, for ALL python ports, so that we control access to the Python Interpreter to one thread.
	static std::shared_ptr<asio::strand> python_strand;

	// Worker methods
	void PostCallbackCall(const odc::SharedStatusCallback_t& pStatusCallback, CommandStatus c);
};

#endif /* PYPORT_H_ */
