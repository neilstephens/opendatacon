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
 *		Extensively modifed: Scott Ellis <scott.ellis@novatex.com.au>
 */


// Python Interface:
// We need to pass the config JSON (both main and overrides) to the Python class, so that it has all the information that we have.
// We need to call a Python method every time we get an event, we assume it will not call network or other long running functions. We wait for it to finish.
// We need to export a function to the Python code, so that it can Post an Event into the ODC event bus.
// Do we need to call a Python like timer method, that will allow the Python code to set how long it should be until it is next called.
// So leave the extension bit out for the moment, just get to the pont where we can load the class and call its methods...

#include "PyPort.h"
#include <opendatacon/MergeJsonConf.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include <whereami++.h>
#ifdef WIN32
#include <direct.h>
#define PathSeparator "\\"
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define PathSeparator "/"
#define GetCurrentDir getcwd
#endif
#include <iostream>

std::string GetCurrentWorkingDir(void)
{
	char buff[FILENAME_MAX];
	std::string current_working_dir(GetCurrentDir(buff, FILENAME_MAX));
	return current_working_dir;
}

//FIXME: This is bad. Any uses of this should use std::filesystem.
bool fileexists(const std::string& filename)
{
	std::ifstream ifile(filename.c_str());
	return (bool)ifile;
}
using namespace odc;

std::vector<std::string> split(const std::string& s, char delim)
{
	std::vector<std::string> result;
	std::stringstream ss(s);
	std::string item;

	while (getline(ss, item, delim))
	{
		result.push_back(item);
	}
	return result;
}

//FIMXE: std lib should be used for this kind of thing
// Get the number contained in this string.
std::string GetNumber(const std::string& input)
{
	std::string res;
	for (char ch : input)
	{
		if ((ch >= '0') && (ch <= '9'))
			res += ch;
	}
	return res;
}

// Constructor for PyPort --------------------------------------
PyPort::PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	SetLog("PyPort");
	//the creation of a new PyPortConf will get the point details
	pConf = std::make_unique<PyPortConf>(ConfFilename, ConfOverrides);

	// Just to save a lot of dereferencing..
	MyConf = static_cast<PyPortConf*>(this->pConf.get());

	// We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

PyPort::~PyPort()
{
	LOGDEBUG("Destructing PyPort");
}

// The ASIO IOS instance is up, our config files have been read and parsed, this is the opportunity to kick off connections and scheduled processes
void PyPort::Build()
{
	LOGDEBUG("PyPort Build called for {}", Name);

	// Check that the Python Module is available to load in either the current path, or the path to the executing program
	std::string CurrentPath(GetCurrentWorkingDir());
	std::string PyModPath;
	std::string FullModuleFilename(CurrentPath + PathSeparator + MyConf->pyModuleName+".py");

	if (fileexists(FullModuleFilename))
	{
		LOGDEBUG("Found Python Module in current directory {}", FullModuleFilename);
		PyModPath = CurrentPath;
	}
	else
	{
		std::string ExePath = whereami::getExecutablePath().dirname();
		FullModuleFilename = ExePath + PathSeparator + MyConf->pyModuleName + ".py";

		if (fileexists(FullModuleFilename))
		{
			LOGDEBUG("Found Python Module in exe directory {}", FullModuleFilename);
			PyModPath = ExePath;
		}
		else
		{
			LOGERROR("Could not find Python Module {} in {} or {}", MyConf->pyModuleName + ".py", CurrentPath, ExePath);
			return;
		}
	}

	// Build is single threaded, no ASIO tasks fire up until all the builds are finished

	//FIXME: use lambdas instead of std::bind
	// If first time constructor is called, will instansiate the interpreter.
	// Pass in a pointer to our SetTimer method, so it can be called from Python code - bit circular - I know!
	// Also pass in a PublishEventCall method, so Python can send us Events to Publish.
	pWrapper = std::make_unique<PythonWrapper>(this->Name, pIOS, std::bind(&PyPort::SetTimer, this, std::placeholders::_1, std::placeholders::_2),
		std::bind(&PyPort::PublishEventCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	LOGDEBUG("pWrapper Created #####");
	try
	{
		// Python code is loaded and class created, __init__ called.
		pWrapper->Build(PyModPath, this->Name, MyConf);

		Json::StreamWriterBuilder wbuilder;
		wbuilder["commentStyle"] = "None"; // No comments - python doesn't like them
		pWrapper->Config(Json::writeString(wbuilder, JSONConf), "");

		LOGDEBUG("Loaded Python Module \"{}\" ", MyConf->pyModuleName);
	}
	catch (std::exception& e)
	{
		LOGERROR("Exception Importing Module and Creating Class instance - {}", e.what());
	}

	pServer = HttpServerManager::AddConnection(pIOS, MyConf->pyHTTPAddr, MyConf->pyHTTPPort); //Static method - creates a new HttpServerManager if required
	AddHTTPHandlers();
}

void PyPort::Enable()
{
	// If already true, return - otherwise set to true
	if (enabled.exchange(true))
		return;

	// We need to not enable until build has pWrapper created..
	LOGDEBUG("About to Wait for pWrapper to be created!");
	while (!pWrapper)
	{
		if (pIOS->stopped())
			return; // Shutting down!

		pIOS->poll_one();
	}
	LOGDEBUG("pWrapper is good!");

	HttpServerManager::StartConnection(pServer);

	auto promise = std::make_shared<std::promise<bool>>();
	auto future = promise->get_future(); // You can only call get_future ONCE!!!! Otherwise throws an assert exception!

	pWrapper->GlobalPythonStrand()->post([this,promise]()
		{
			LOGSTRAND("Entered Strand on Enable");
			pWrapper->Enable();
			promise->set_value(true);
			pWrapper->PortOperational();
			LOGDEBUG("Port enabled and  operational 1 {}", Name);
			LOGSTRAND("Exit Strand");
		});
	// Synchronously wait for promise to be fulfilled - pWrapper to be created, we need to poll the ASIO threadpool to do that
	LOGDEBUG("Entering Port Wait {}", Name);
	while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
	{
		if (pIOS->stopped() == true)
		{
			// If we are closing get out of here. Dont worry about the result.
			return;
		}
		// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
		pIOS->poll_one();
	}
	LOGDEBUG("Port enabled and  operational 2 {}", Name);
}

void PyPort::Disable()
{
	// If already false, return - otherwise set to false.
	if (!enabled.exchange(false))
		return;

	CancelTimers();
	RemoveHTTPHandlers();
	// Leaves the connection running, someone else might be using it? If another joins will work fine.
	// Used to be: HttpServerManager::StopConnection(pServer);

	pWrapper->GlobalPythonStrand()->post([this]()
		{
			LOGSTRAND("Entered Strand on Disable");
			pWrapper->Disable();
			LOGSTRAND("Exit Strand");
		});
}

void PyPort::AddHTTPHandlers()
{
	// Now add all the callbacks that we need - the root handler might be a duplicate, in which case it will be ignored!

	auto roothandler = std::make_shared<http::HandlerCallbackType>([](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
		{
			rep.status = http::reply::ok;
			rep.content.append("You have reached the PyPort http interface.<br>To talk to a port the url must contain the PyPort name, which is case senstive.<br>Anything beyond this will be passed to the Python code.");
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
		});

	HttpServerManager::AddHandler(pServer, "GET /", roothandler);

	auto gethandler = std::make_shared<http::HandlerCallbackType>([this](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
		{
			// So when we hit here, someone has made a Get request of our Port. Pass it to Python, and wait for a response...
			std::string contenttype = "application/json";
			std::string result = "";

			if (!pWrapper)
			{
				LOGERROR("Tried to handle a http callback, but pWrapper is null {} {}", absoluteuri, content);
				rep.status = http::reply::not_found;
				rep.content.append("You have reached the PyPort Instance with GET on " + Name + " Port has been destructed!!");
				contenttype = "text/html";
			}
			else
			{
				result = pWrapper->RestHandler(absoluteuri, content); // Expect no long processing or waits in the python code to handle this.

				if (result.length() > 0)
				{
					rep.status = http::reply::ok;
					rep.content.append(result);
				}
				else
				{
					rep.status = http::reply::not_found;
					rep.content.append("You have reached the PyPort Instance with GET on " + Name + " No reponse from Python Code");
					contenttype = "text/html";
				}
			}
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = contenttype;
		});
	HttpServerManager::AddHandler(pServer, "GET /" + Name, gethandler);

	auto posthandler = std::make_shared<http::HandlerCallbackType>([=](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
		{
			// So when we hit here, someone has made a Get request of our Port. Pass it to Python, and wait for a response...
			std::string result = pWrapper->RestHandler(absoluteuri, content); // Expect no long processing or waits in the python code to handle this.
			std::string contenttype = "application/json";

			if (result.length() > 0)
			{
				rep.status = http::reply::ok;
				rep.content.append(result);
			}
			else
			{
				rep.status = http::reply::not_found;
				rep.content.append("You have reached the PyPort Instance with POST on " + Name + " No reponse from Python Code");
				contenttype = "text/html";
			}
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = contenttype;
		});
	HttpServerManager::AddHandler(pServer, "POST /" + Name, posthandler);
}

void PyPort::RemoveHTTPHandlers()
{
	HttpServerManager::RemoveHandler(pServer, "GET /");
	HttpServerManager::RemoveHandler(pServer, "GET /" + Name);
	size_t remaininghandlers = HttpServerManager::RemoveHandler(pServer, "POST /" + Name);
	if (remaininghandlers == 0)
	{
		HttpServerManager::StopConnection(pServer); // Only do this if no one else (other PyPort instances) is listening on the port.
	}
}

std::shared_ptr<odc::EventInfo> PyPort::CreateEventFromStrParams(const std::string& EventTypeStr, size_t& ODCIndex, const std::string& QualityStr, const std::string& PayloadStr, const std::string& SourcePort)
{
	EventType EventTypeResult = EventTypeFromString(EventTypeStr);
	if (EventTypeResult >= EventType::AfterRange)
	{
		LOGERROR("Invalid Event Type String passed from Python Code to ODC - {}", EventTypeStr);
		return nullptr;
	}
	QualityFlags QualityResult = QualityFlagsFromString(QualityStr);
	if (QualityResult == QualityFlags::NONE)
	{
		LOGERROR("No Quality Information Passed from Python Code to ODC - {}", QualityStr);
		return nullptr;
	}

	std::shared_ptr<odc::EventInfo> pubevent;

	switch (EventTypeResult)
	{
		case EventType::ConnectState:
		{
			ConnectState state = ConnectStateFromString(PayloadStr);
			if (state == ConnectState::UNDEFINED)
			{
				LOGERROR("Invalid Connection State passed from Python Code to ODC - {}", PayloadStr);
				return nullptr;
			}
			pubevent = std::make_shared<EventInfo>(EventType::ConnectState, 0, SourcePort);
			pubevent->SetPayload<EventType::ConnectState>(std::move(state));
		}
		break;

		case EventType::Binary:
		{
			pubevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, SourcePort, QualityResult);
			bool val = (PayloadStr.find('1') != std::string::npos);
			pubevent->SetPayload<EventType::Binary>(std::move(val));
		}
		break;

		case EventType::Analog:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, SourcePort, QualityResult);
				double dval = std::stod(PayloadStr);
				pubevent->SetPayload<EventType::Analog>(std::move(dval));
			}
			catch (std::exception& e)
			{
				LOGERROR("Analog Value passed from Python failed to be converted to a double - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::Counter:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::FrozenCounter:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::BinaryOutputStatus:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::AnalogOutputStatus:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::ControlRelayOutputBlock:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, SourcePort, QualityResult);
				// Payload String looks like: "|LATCH_ON|Count 1|ON 100ms|OFF 100ms|"
				EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
				auto Parts = split(PayloadStr, '|');
				if (Parts.size() != 5) throw std::runtime_error("Payload for ControlRelayOutputBlock does not have enough sections " + PayloadStr);

				ControlCode ControlCodeResult = ControlCodeFromString(Parts[1]);
				if(ControlCodeResult == ControlCode::UNDEFINED) throw std::runtime_error("ControlCode field of ControlRelayOutputBlock not in " + Parts[1]);
				val.functionCode = ControlCodeResult;

				if (Parts[2].find("Count") == std::string::npos) throw std::runtime_error("Count field of ControlRelayOutputBlock not in " + Parts[2]);
				val.count = std::stoi(GetNumber(Parts[2]));

				if (Parts[3].find("ON") == std::string::npos) throw std::runtime_error("ON field of ControlRelayOutputBlock not in " + Parts[3]);
				val.onTimeMS = std::stoi(GetNumber(Parts[3]));

				if (Parts[4].find("OFF") == std::string::npos) throw std::runtime_error("OFF field of ControlRelayOutputBlock not in " + Parts[4]);
				val.offTimeMS = std::stoi(GetNumber(Parts[4]));

				pubevent->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("ControlRelayOutputBlock Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::AnalogOutputInt16:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ODCIndex, SourcePort, QualityResult);
				EventTypePayload<EventType::AnalogOutputInt16>::type val;
				int16_t ival = std::stoi(PayloadStr);
				val.first = ival;
				// val.second = odc::CommandStatus::SUCCESS;	// Should this be set at all - NO
				pubevent->SetPayload<EventType::AnalogOutputInt16>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("AnalogOutputInt16 Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::AnalogOutputInt32:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::AnalogOutputInt32, ODCIndex, SourcePort, QualityResult);
				EventTypePayload<EventType::AnalogOutputInt32>::type val;
				int32_t ival = std::stoi(PayloadStr);
				val.first = ival;
				pubevent->SetPayload<EventType::AnalogOutputInt32>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("AnalogOutputInt32 Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::AnalogOutputFloat32:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::AnalogOutputFloat32, ODCIndex, SourcePort, QualityResult);
				EventTypePayload<EventType::AnalogOutputFloat32>::type val;
				float fval = std::stof(PayloadStr);
				val.first = fval;
				pubevent->SetPayload<EventType::AnalogOutputFloat32>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("AnalogOutputFloat32 Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::AnalogOutputDouble64:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::AnalogOutputDouble64, ODCIndex, SourcePort, QualityResult);
				EventTypePayload<EventType::AnalogOutputDouble64>::type val;
				double dval = std::stod(PayloadStr);
				val.first = dval;
				pubevent->SetPayload<EventType::AnalogOutputDouble64>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("AnalogOutputDouble64 Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::OctetString:
		{
			pubevent = std::make_shared<EventInfo>(EventType::OctetString, ODCIndex, SourcePort, QualityResult);
			pubevent->SetPayload<EventType::OctetString>(std::string(PayloadStr));
		}
		break;
		case EventType::BinaryQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::DoubleBitBinaryQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::AnalogQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::CounterQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::BinaryOutputStatusQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::FrozenCounterQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		case EventType::AnalogOutputStatusQuality:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
			break;
		default:
			LOGERROR("PublishEvent from Python passed an EventType that we can't handle - {}", ToString(EventTypeResult));
			break;
	}
	return pubevent;
}

// We pass this method string values for fields (from Python) parse them and create the ODC event then send it.
// A pointer to this method is passed to the Wrapper class so it can be called when we get information from the Python code.
// We are not passing a callback - just nullstr. So dont expect feedback.
void PyPort::PublishEventCall(const std::string &EventTypeStr, size_t ODCIndex,  const std::string &QualityStr, const std::string &PayloadStr, const std::string &SourcePort )
{
	//LOGDEBUG("PyPort Publish Event {}, {}, {}, {}", EventTypeStr, ODCIndex, QualityStr, PayloadStr);
	std::string SrcPort = SourcePort;
	if (SourcePort.length() == 0)
	{
		SrcPort = Name; // If not provided, use default
	}
	// Separate call to allow testing
	std::shared_ptr<EventInfo> pubevent = CreateEventFromStrParams(EventTypeStr, ODCIndex, QualityStr, PayloadStr, SrcPort);
	if (pubevent)
	{
		if(MyConf->pyEnablePublishCallbackHandler)
		{
			auto callback = std::make_shared<std::function<void (CommandStatus)>>(pWrapper->GlobalPythonStrand()->wrap(
				[this, EventTypeStr, ODCIndex, QualityStr, PayloadStr, Time{pubevent->GetTimestamp()}](CommandStatus result)
				{
					pWrapper->CallPublishCallback(EventTypeStr, ODCIndex, QualityStr, PayloadStr, Time, ToString(result));
				}));
			PublishEvent(pubevent,callback);
		}
		else
			PublishEvent(pubevent);
	}
}
std::string getISOCurrentTimestampUTC_from_msSinceEpoch_t(const odc::msSinceEpoch_t& ts)
{
	// TimeDate Needs to be "2019-07-17T01:34:20.072Z" ISO8601 format.
	time_t tp = ts / 1000; // time_t is normally seconds since epoch. We deal in msec!
	int msec = ts % 1000;

	std::tm* t = std::gmtime(&tp); // So UTC
	if (t != nullptr)
	{
		return fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour, t->tm_min, t->tm_sec, msec);
	}
	return "Error";
}

std::string PyPort::GetTagValue(const std::string & SenderName, EventType Eventt, size_t Index)
{
	// The unordered_maps lookup reading do not need to be protected, they are threadsafe for this
	// The Event handler calling this is multithreaded, so there will be concurrent access.

	std::string Tag = "";

	auto searchport = PortTagMap.find(SenderName);
	if (searchport != PortTagMap.end())
	{
		auto foundport = searchport->second;
		switch(Eventt)
		{
			case EventType::Analog:
			{
				auto search = foundport->AnalogMap.find(Index);
				if (search != foundport->AnalogMap.end())
					Tag = search->second;
			}
			break;
			case EventType::Binary:
			{
				auto search = foundport->BinaryMap.find(Index);
				if (search != foundport->BinaryMap.end())
					Tag = search->second;
			}
			break;
			case EventType::OctetString:
			{
				auto search = foundport->OctetStringMap.find(Index);
				if (search != foundport->OctetStringMap.end())
					Tag = search->second;
			}
			break;
			case EventType::ControlRelayOutputBlock:
			{
				auto search = foundport->BinaryControlMap.find(Index);
				if (search != foundport->BinaryControlMap.end())
					Tag = search->second;
			}
			break;
			case EventType::AnalogOutputDouble64:
			case EventType::AnalogOutputFloat32:
			case EventType::AnalogOutputInt16:
			case EventType::AnalogOutputInt32:
			{
				auto search = foundport->AnalogControlMap.find(Index);
				if (search != foundport->AnalogControlMap.end())
					Tag = search->second;
			}
			break;
			default:
				break;
		}
	}
	LOGTRACE("PyPort {} GetTagValue {} {} {} {}", Name, SenderName, Index, ToString(Eventt), Tag);
	return Tag;
}

// So we have received an event from the ODC message bus - it will be Control or Connect events.
// So basically the Event mechanism is agnostinc. You can be a producer of control events and a consumer of data events, or the reverse, or in some odd cases -
// A consumer of both types of events (this means you are just a listener on a conversation between two other devices.)
void PyPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		LOGTRACE("PyPort {} not enabled, Event from {} ignored", Name, SenderName);
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	if (MyConf->pyEventsAreQueued)
	{
		// Use a concurrent queue to buffer the events into Python. If the events are queued, then the python code is responsible
		// for periodically processing them and emptying the queue. Send as a json string

		// So in the config file a point can have a "Tag" value. If this is present, we save it in a hash with an ODCIndex.
		// We do a lookup on the index to get the tag, which is then part of the queued event.

		// Also probably need an event filter defined in the conf file - events not to queue?

		std::string isotimestamp = getISOCurrentTimestampUTC_from_msSinceEpoch_t(event->GetTimestamp());
		try
		{
			if (MyConf->pyQueueFormatString == "")
				return;

			// Could use event->SourcePort (for the sending port, instead of the the SenderName, which will be the connector name.
			std::string TagValue = GetTagValue(SenderName, event->GetEventType(),event->GetIndex());

			if (MyConf->pyOnlyQueueEventsWithTags && (TagValue == ""))
				return; // If only queue events that have tags, and we dont have a tag, return

			auto raw_quality_mask = static_cast<std::underlying_type<QualityFlags>::type>(event->GetQuality());

			std::string jsonevent = fmt::format(MyConf->pyQueueFormatString, // How the string will be formatted - can leave values below out if desired!
				odc::ToString(event->GetEventType()),                      // 0
				event->GetIndex(),                                         // 1
				isotimestamp,                                              // 2
				ToString(event->GetQuality()),                             // 3
				event->GetPayloadString(MyConf->pyOctetStringFormat),      // 4
				SenderName,                                                // 5
				TagValue,                                                  // 6
				MyConf->pyTagPrefixString,                                 // 7
				raw_quality_mask);                                         // 8
			pWrapper->QueueEvent(jsonevent);
			LOGTRACE("Queued Event {}", jsonevent);
			PostCallbackCall(pStatusCallback, CommandStatus::SUCCESS);
		}
		catch(std::exception& e)
		{
			LOGCRITICAL("Queue Formatting String Parsing Failure {}, {}", e.what(), MyConf->pyQueueFormatString);
		}
	}
	else
	{
		pWrapper->GlobalPythonStrand()->post([this, event, SenderName, pStatusCallback]()
			{
				LOGSTRAND("Entered Strand on Event");
				CommandStatus result = pWrapper->Event(event, SenderName); // Expect no long processing or waits in the python code to handle this.

				PostCallbackCall(pStatusCallback, result);
				LOGSTRAND("Exit Strand");
			});
	}
}
void PyPort::SetTimer(uint32_t id, uint32_t delayms)
{
	if (!enabled)
	{
		LOGDEBUG("PyPort {} not enabled, SetTimer call ignored", Name);
		return;
	}
	LOGTRACE("SetTimer call {}, {}, {}", Name, id, delayms);

	pTimer_t timer = pIOS->make_steady_timer();
	StoreTimer(id, timer);
	timer->expires_from_now(std::chrono::milliseconds(delayms));
	timer->async_wait(pWrapper->GlobalPythonStrand()->wrap(
		[this, id, timer](asio::error_code err_code) // Pass in shared ptr to keep it alive until we are done - time out or aborted
		{
			if (!err_code)
			{
				if (!enabled)
				{
					LOGDEBUG("PyPort {} not enabled, Timer callback ignored", Name);
					return;
				}
				LOGSTRAND("Entered Strand on SetTimer");
				pWrapper->CallTimerHandler(id);
				LOGSTRAND("Exit Strand");
			}
		}));
}

void PyPort::StoreTimer(uint32_t id, pTimer_t t)
{
	std::unique_lock<std::shared_timed_mutex> lck(timer_mutex);
	timers[id] = t;
}

void PyPort::CancelTimers()
{
	std::unique_lock<std::shared_timed_mutex> lck(timer_mutex);
	for (const auto& timer : timers)
		timer.second->cancel();
	timers.clear();
}

// This is called when we have decoded a restful request, to the point where we know which instance it should be passed to. We give it a callback, which will
// be used to actually send the response back to the caller.
void PyPort::RestHandler(const std::string& url, const std::string& content, const ResponseCallback_t& pResponseCallback)
{
	if (!enabled)
	{
		LOGDEBUG("PyPort {} not enabled, Restful Request ignored", Name);
		PostResponseCallbackCall(pResponseCallback, "Error Port not enabled");
		return;
	}

	pWrapper->GlobalPythonStrand()->post([this, url, content, pResponseCallback]()
		{
			LOGSTRAND("Entered Strand on RestHandler");
			std::string result = pWrapper->RestHandler(url,content); // Expect no long processing or waits in the python code to handle this.

			PostResponseCallbackCall(pResponseCallback, result);
			LOGSTRAND("Exit Strand");
		});
}

// Just schedule the callback, don't want to do it in a strand protected section.
void PyPort::PostCallbackCall(const odc::SharedStatusCallback_t& pStatusCallback, CommandStatus c)
{
	if (pStatusCallback)
	{
		pIOS->post([pStatusCallback, c]()
			{
				(*pStatusCallback)(c);
			});
	}
}
void PyPort::PostResponseCallbackCall(const ResponseCallback_t & pResponseCallback, const std::string& response)
{
	if (pResponseCallback)
	{
		pIOS->post([pResponseCallback, response]()
			{
				(*pResponseCallback)(response);
			});
	}
}

// This should be called twice, once for the config file setion, and the second for config overrides.
void PyPort::ProcessElements(const Json::Value& JSONRoot)
{
	MergeJsonConf(JSONConf, JSONRoot);

	if (JSONRoot.isMember("ModuleName"))
		MyConf->pyModuleName = JSONRoot["ModuleName"].asString();
	if (JSONRoot.isMember("ClassName"))
		MyConf->pyClassName = JSONRoot["ClassName"].asString();
	if (JSONRoot.isMember("IP"))
		MyConf->pyHTTPAddr = JSONRoot["IP"].asString();
	if (JSONRoot.isMember("Port"))
		MyConf->pyHTTPPort = JSONRoot["Port"].asString();

	if (JSONRoot.isMember("QueueFormatString"))
		MyConf->pyQueueFormatString = JSONRoot["QueueFormatString"].asString();
	if (JSONRoot.isMember("TagPrefix"))
		MyConf->pyTagPrefixString = JSONRoot["TagPrefix"].asString();
	if (JSONRoot.isMember("EventsAreQueued"))
		MyConf->pyEventsAreQueued = JSONRoot["EventsAreQueued"].asBool();
	if (JSONRoot.isMember("OnlyQueueEventsWithTags"))
		MyConf->pyOnlyQueueEventsWithTags = JSONRoot["OnlyQueueEventsWithTags"].asBool();
	if (JSONRoot.isMember("EnablePublishCallbackHandler"))
		MyConf->pyEnablePublishCallbackHandler = JSONRoot["EnablePublishCallbackHandler"].asBool();
	if (JSONRoot.isMember("OctetStringFormat"))
	{
		auto fmt = JSONRoot["OctetStringFormat"].asString();
		if(fmt == "Hex")
			MyConf->pyOctetStringFormat = DataToStringMethod::Hex;
		else if(fmt == "Base64")
			MyConf->pyOctetStringFormat = DataToStringMethod::Base64;
		else if(fmt == "Raw")
			MyConf->pyOctetStringFormat = DataToStringMethod::Raw;
		else
			throw std::invalid_argument("OctetStringFormat should be Raw or Hex, not " + fmt);
	}

	//TODO: The following parameter should always be set to the same value. If different throw an exception as the conf file is wrong!
	if (JSONRoot.isMember("GlobalUseSystemPython"))
		MyConf->GlobalUseSystemPython = JSONRoot["GlobalUseSystemPython"].asBool(); // Defaults to OFF

	if (JSONRoot.isMember("Analogs"))
	{
		const auto Analogs = JSONRoot["Analogs"];
		LOGDEBUG("Conf processed - Analog Points");
		ProcessPoints(EventType::Analog, Analogs);
	}
	if (JSONRoot.isMember("Binaries"))
	{
		const auto Binaries = JSONRoot["Binaries"];
		LOGDEBUG("Conf processed - Binary Points");
		ProcessPoints(EventType::Binary, Binaries);
	}
	if (JSONRoot.isMember("OctetStrings"))
	{
		const auto OctetStrings = JSONRoot["OctetStrings"];
		LOGDEBUG("Conf processed - OctetString Points");
		ProcessPoints(EventType::OctetString, OctetStrings);
	}
	if (JSONRoot.isMember("BinaryControls"))
	{
		const auto BinaryControls = JSONRoot["BinaryControls"];
		LOGDEBUG("Conf processed - Binary Controls");
		ProcessPoints(EventType::ControlRelayOutputBlock, BinaryControls);
	}
	if (JSONRoot.isMember("AnalogControls"))
	{
		const auto AnalogControls = JSONRoot["AnalogControls"];
		LOGDEBUG("Conf processed - Analog Controls");
		ProcessPoints(EventType::AnalogOutputDouble64, AnalogControls);
	}
}

// This method loads both Analog and Counter/Timers. They look functionally similar in CB
void PyPort::ProcessPoints(EventType ptype, const Json::Value& JSONNode)
{
	std::string Name("None");

	switch(ptype)
	{
		case EventType::Analog:
			Name = "Analog";
			break;
		case EventType::Binary:
			Name = "Binary";
			break;
		case EventType::OctetString:
			Name = "OctetString";
			break;
		case EventType::ControlRelayOutputBlock:
			Name = "Control";
			break;
		case EventType::AnalogOutputDouble64:
		case EventType::AnalogOutputFloat32:
		case EventType::AnalogOutputInt16:
		case EventType::AnalogOutputInt32:
			Name = "AnalogControl";
			break;
		default:
			break;
	}

	LOGDEBUG("Conf processing - {}", Name);
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		size_t index = 0;
		std::string sender = "";

		if (JSONNode[n].isMember("Index"))
		{
			index = JSONNode[n]["Index"].asUInt();
		}

		if (JSONNode[n].isMember("Sender"))
		{
			sender = JSONNode[n]["Sender"].asString();
		}

		if (JSONNode[n].isMember("Tag"))
		{
			std::string Tag = JSONNode[n]["Tag"].asString();

			if (sender == "")
			{
				throw std::invalid_argument("A point needs an \"Sender\" to match the PortName the event is coming from for a Tag to be valid : " + JSONNode[n].toStyledString());
			}
			// We have an index, tag and a sender, so add to the hash.
			auto searchport = PortTagMap.find(sender);
			if (searchport == PortTagMap.end())
			{
				PortTagMap.emplace(std::make_pair(sender, std::make_shared<PortMapClass>() ));
			}
			searchport = PortTagMap.find(sender);
			if (searchport == PortTagMap.end())
			{
				throw std::invalid_argument("We could not find a PortTagMap entry that we just added? Bad things are happening! " + JSONNode[n].toStyledString());
			}

			auto foundport = searchport->second;
			switch(ptype)
			{
				case EventType::Analog:
					foundport->AnalogMap.emplace(std::make_pair(index, Tag));
					break;
				case EventType::Binary:
					foundport->BinaryMap.emplace(std::make_pair(index, Tag));
					break;
				case EventType::OctetString:
					foundport->OctetStringMap.emplace(std::make_pair(index, Tag));
					break;
				case EventType::ControlRelayOutputBlock:
					foundport->BinaryControlMap.emplace(std::make_pair(index, Tag));
					break;
				case EventType::AnalogOutputDouble64:
				case EventType::AnalogOutputFloat32:
				case EventType::AnalogOutputInt16:
				case EventType::AnalogOutputInt32:
					foundport->AnalogControlMap.emplace(std::make_pair(index, Tag));
					break;
				default:
					LOGDEBUG("Conf Processing {} - found a Tag for a Type that does not support Tag - {}", Name, JSONNode[n].toStyledString());
					break;
			}
			LOGTRACE("Sender - {}, Type - {}, Tag - {}, Index - {}", sender, Name, Tag, index);
		}
	}
	LOGDEBUG("Conf processing - {} - Finished",Name);
}
