/*	opendatacon
 *
 *	Copyright (c) 2014:
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
 * TestHelpers.h
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef GOMODBUS_TESTHELPERS_H
#define GOMODBUS_TESTHELPERS_H

#include "../PortLoader.h"
#include "../ThreadPool.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <opendatacon/DataPort.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>

#include "CAPI/C_Port.h"

// Defined in CatchTestStart.cpp; controls logger verbosity.
extern spdlog::level::level_enum log_level;

// ---------------------------------------------------------------------------
// Test setup / teardown
// ---------------------------------------------------------------------------

// Register the two spdlog loggers required by the Go port and the ODC
// framework.  Call at the top of every test case.
inline void TestSetup()
{
	odc::spdlog_drop_all();
	auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLib = std::make_shared<spdlog::logger>("opendatacon", sink);
	pLib->set_level(log_level);
	odc::spdlog_register_logger(pLib);
	auto pGo = std::make_shared<spdlog::logger>("GoModbusPort", sink);
	pGo->set_level(log_level);
	odc::spdlog_register_logger(pGo);
}

inline void TestTearDown()
{
	odc::spdlog_flush_all();
}

// ---------------------------------------------------------------------------
// WaitCallback / SendEvent
// ---------------------------------------------------------------------------

// Poll the io_service while waiting for an atomic flag, up to timeout.
inline bool WaitCallback(std::atomic_bool& done, std::chrono::milliseconds timeout)
{
	auto deadline = std::chrono::steady_clock::now() + timeout;
	while(!done.load() && std::chrono::steady_clock::now() < deadline)
	{
		odc::asio_service::Get()->poll_one();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return done.load();
}

// Send an ODC event to a port and block (polling the io_service) until the
// status callback fires or the timeout expires.  Returns the command status.
inline odc::CommandStatus SendEvent(odc::C_Port& port,
	std::shared_ptr<odc::EventInfo> event,
	std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
	std::atomic_bool done(false);
	odc::CommandStatus result = odc::CommandStatus::UNDEFINED;
	auto cb = std::make_shared<std::function<void(odc::CommandStatus)>>(
		[&done, &result](odc::CommandStatus s)
		{
			result = s;
			done   = true;
		});
	port.Event(event, "TestHarness", cb);
	WaitCallback(done, timeout);
	return result;
}

// ---------------------------------------------------------------------------
// EventCapturePort
// ---------------------------------------------------------------------------

// Minimal DataPort that records every inbound ODC event in a thread-safe
// vector.  Subscribe it to a publisher before Enable(); the port itself
// does not need Build() or Enable() to receive events.
class EventCapturePort: public odc::DataPort
{
public:
	explicit EventCapturePort(const std::string& name)
		: odc::DataPort(name, "", Json::Value())
	{}

	void Build() override {}
	void Enable() override {}
	void Disable() override {}
	void ProcessElements(const Json::Value&) override {}

	void Event(std::shared_ptr<const odc::EventInfo> event,
		const std::string& /*sender*/,
		odc::SharedStatusCallback_t pCb) override
	{
		std::lock_guard<std::mutex> lk(mtx_);
		events_.push_back(event);
		(*pCb)(odc::CommandStatus::SUCCESS);
	}

	// Name is protected in IOHandler; expose it publicly.
	const std::string& GetName() const { return Name; }

	size_t Count() const
	{
		std::lock_guard<std::mutex> lk(mtx_);
		return events_.size();
	}

	// Return a snapshot of all captured events.
	std::vector<std::shared_ptr<const odc::EventInfo>> Events() const
	{
		std::lock_guard<std::mutex> lk(mtx_);
		return events_;
	}

	// Return the first captured event of the given type, or nullptr.
	std::shared_ptr<const odc::EventInfo> FirstOfType(odc::EventType et) const
	{
		std::lock_guard<std::mutex> lk(mtx_);
		for(const auto& e : events_)
		{
			if(e->GetEventType() == et)
				return e;
		}
		return nullptr;
	}

	// Block (polling io_service) until at least n events arrive or timeout.
	bool WaitFor(size_t n, std::chrono::milliseconds timeout)
	{
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while(std::chrono::steady_clock::now() < deadline)
		{
			if(Count() >= n)
				return true;
			odc::asio_service::Get()->poll_one();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return Count() >= n;
	}

	// Block until any captured event satisfies pred, or timeout expires.
	// Unlike WaitFor(), this ignores events that do not match the predicate
	// (e.g. ConnectState events that precede the data events of interest).
	bool WaitForMatch(std::function<bool(const odc::EventInfo&)> pred,
		std::chrono::milliseconds timeout)
	{
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while(std::chrono::steady_clock::now() < deadline)
		{
			{
				std::lock_guard<std::mutex> lk(mtx_);
				for(const auto& e : events_)
				{
					if(pred(*e))
						return true;
				}
			}
			odc::asio_service::Get()->poll_one();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		// One final scan after timeout.
		std::lock_guard<std::mutex> lk(mtx_);
		for(const auto& e : events_)
		{
			if(pred(*e))
				return true;
		}
		return false;
	}

private:
	mutable std::mutex mtx_;
	std::vector<std::shared_ptr<const odc::EventInfo>> events_;
};

// ---------------------------------------------------------------------------
// Config builders
// ---------------------------------------------------------------------------

// Minimal GoModbusClient config: connects to 127.0.0.1:1502 (offline in most
// tests — the port does not need a real server to test lifecycle/config).
inline Json::Value MakeMinimalClientConfig()
{
	Json::Value conf;
	conf["TCP"]["Address"] = "127.0.0.1:1502";
	conf["UnitID"]         = 1;
	conf["TimeoutMs"]      = 1000;
	conf["PollRateMs"]     = 1000;
	return conf;
}

// GoModbusClient config with a Binary poll point, a range of Analog poll
// points, and a BinaryControl point — used by tests that exercise the full
// point-expansion and control-dispatch paths.
inline Json::Value MakeClientConfigWithPoints()
{
	Json::Value conf = MakeMinimalClientConfig();

	// Single binary input (coil 0 → ODC Binary index 0)
	Json::Value bin;
	bin["Index"]         = 0;
	bin["Modbus"]["Type"]    = "Coil";
	bin["Modbus"]["Address"] = 0;
	conf["Binaries"].append(bin);

	// Range of analogs (HR 100-103 → ODC Analog indices 100-103)
	Json::Value ana;
	ana["Range"]["Start"]    = 100;
	ana["Range"]["Stop"]     = 103;
	ana["Modbus"]["Type"]    = "HoldingRegister";
	ana["Modbus"]["Address"] = 100;
	ana["Endian"]            = "ABCD";
	ana["DataType"]          = "Int16";
	conf["Analogs"].append(ana);

	// Binary control (coil 200 → ODC ControlRelayOutputBlock index 0)
	Json::Value ctl;
	ctl["Index"]         = 0;
	ctl["Modbus"]["Type"]    = "Coil";
	ctl["Modbus"]["Address"] = 200;
	conf["BinaryControls"].append(ctl);

	return conf;
}

// GoModbusServer config that listens on 127.0.0.1:<listenPort>.
// An optional Json::Value of extra point arrays can be merged in.
// UnitID = 0 means respond to any unit ID.
inline Json::Value MakeServerConfig(int listenPort,
	const Json::Value& points = Json::Value())
{
	Json::Value conf;
	conf["TCP"]["Listen"] = "127.0.0.1:" + std::to_string(listenPort);
	conf["UnitID"]        = 0;
	if(points.isObject())
	{
		for(const auto& key : points.getMemberNames())
			conf[key] = points[key];
	}
	return conf;
}

// GoModbusClient config that connects to 127.0.0.1:<connectPort>.
// Used in end-to-end tests where a real server is running locally.
inline Json::Value MakeClientConfig(int connectPort, int pollRateMs = 200)
{
	Json::Value conf;
	conf["TCP"]["Address"] = "127.0.0.1:" + std::to_string(connectPort);
	conf["UnitID"]         = 1;
	conf["TimeoutMs"]      = 2000;
	conf["PollRateMs"]     = pollRateMs;
	return conf;
}

// Subscribe a capture port to a publisher AND establish demand on the publisher.
// In a full DataConcentrator setup, a DataConnector calls publisher.Event(CONNECTED,
// subscriber_name) after subscribing, which updates the publisher's demand map so
// that odc_in_demand() returns true and polling is not suppressed.
inline void SubscribeAndEstablishDemand(odc::C_Port& publisher,
	EventCapturePort* subscriber)
{
	publisher.Subscribe(subscriber, subscriber->GetName());
	// Simulate the DataConnector's demand handshake: the subscriber reports
	// its ConnectState to the publisher, establishing demand.
	// Cast to DataPort& to access Event(ConnectState, string) which is hidden
	// in C_Port by the Event(shared_ptr<EventInfo>, ...) override.
	static_cast<odc::DataPort&>(publisher).Event(
		odc::ConnectState::CONNECTED, subscriber->GetName());
}

#endif // GOMODBUS_TESTHELPERS_H
