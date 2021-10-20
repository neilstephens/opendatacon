/*	opendatacon
 *
 *	Copyright (c) 2019:
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
 *  Created on:20/6/2019
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef PYPORT_H_
#define PYPORT_H_
#include "PythonWrapper.h"
#include "PyPortConf.h"
#include "../HTTP/HttpServerManager.h"
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <unordered_map>
#include <string>

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;
typedef std::shared_ptr<std::function<void (const std::string response)>> ResponseCallback_t;

using namespace odc;

void CommandLineLoggingSetup(spdlog::level::level_enum log_level);
void CommandLineLoggingCleanup();

enum PointType { Binary = 0, Analog = 1, BinaryControl = 2};

// We have a map of these structures, for each sender to this port. This way we only need one port to handle all inbound data on the event bus.
class PortMapClass
{
public:
	std::unordered_map<size_t, std::string> AnalogMap;
	std::unordered_map<size_t, std::string> BinaryMap;
	std::unordered_map<size_t, std::string> BinaryControlMap;
};

class PyPort: public DataPort
{

public:
	PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~PyPort() override;
	void ProcessElements(const Json::Value& JSONRoot) final;

	void Enable() override;
	void Disable() override;

	void RemoveHTTPHandlers();

	void Build() override;
	void AddHTTPHandlers();
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void RestHandler(const std::string& url, const std::string& content, const ResponseCallback_t& pResponseCallback);
	void PublishEventCall(const std::string &EventTypeStr, size_t ODCIndex, const std::string &QualityStr, const std::string &PayloadStr);

	static std::shared_ptr<odc::EventInfo> CreateEventFromStrParams(const std::string& EventTypeStr, size_t& ODCIndex, const std::string& QualityStr, const std::string& PayloadStr, const std::string &Name);

	// Keep track of each PyPort so static methods can get access to the correct PyPort instance
	static std::unordered_map<PyObject*, PyPort*> PyPorts;

	size_t GetEventQueueSize() { return pWrapper->GetEventQueueSize(); }
	std::string GetTagValue(const std::string& SenderName, EventType Eventt, size_t Index);
	void ProcessPoints(PointType ptype, const Json::Value& JSONNode);
	void SetTimer(uint32_t id, uint32_t delayms);

protected:
	// Worker function to try and clean up the code...
	PyPortConf* MyConf;

private:
	std::unique_ptr<PythonWrapper> pWrapper;
	std::string JSONMain;
	std::string JSONOverride;
	std::unordered_map<std::string, std::shared_ptr<PortMapClass>> PortTagMap;

	ServerTokenType pServer;

	// We need one strand, for ALL python ports, so that we control access to the Python Interpreter to one thread.
	static std::shared_ptr<asio::io_context::strand> python_strand;
	static std::once_flag python_strand_flag;
	std::shared_timed_mutex timer_mutex;
	std::unordered_map<uint32_t, pTimer_t> timers;

	// Worker methods
	void PostCallbackCall(const odc::SharedStatusCallback_t& pStatusCallback, CommandStatus c);
	void PostResponseCallbackCall(const ResponseCallback_t& pResponseCallback, const std::string& response);
	void StoreTimer(uint32_t id, pTimer_t t);
	void CancelTimers();
};

#endif /* PYPORT_H_ */
