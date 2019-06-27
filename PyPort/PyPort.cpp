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
#include <chrono>
#include <ctime>
#include <time.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

using namespace odc;

std::shared_ptr<asio::io_context::strand> PyPort::python_strand = nullptr;

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

// Get the number contained in this string.
std::string GetNumber(std::string input)
{
	std::string res;
	for (char ch : input)
	{
		if ((ch >= '0') && (ch <= '9'))
			res += ch;
	}
	return res;
}

struct MyException: public std::exception
{
	std::string s;
	MyException(std::string ss): s(ss) {}
	~MyException() throw () {} // Updated
	const char* what() const throw() { return s.c_str(); }
};

// Constructor for PyPort --------------------------------------
PyPort::PyPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	JSONMain(""),
	JSONOverride("")
{
	//the creation of a new PyPortConf will get the point details
	pConf.reset(new PyPortConf(ConfFilename, ConfOverrides));

	// Just to save a lot of dereferencing..
	MyConf = static_cast<PyPortConf*>(this->pConf.get());

	// We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

PyPort::~PyPort()
{
	LOGDEBUG("Destructing PyPort");

	python_strand->dispatch([&]()
		{
			LOGDEBUG("reset pWrapper");
			pWrapper.reset();
			// Only 1 strand per ODC system.
			if (python_strand.use_count() == 1)
			{
			      LOGDEBUG("reset python_strand");
			//			python_strand.reset();
			}
		});
}
/*

            */
// The ASIO IOS instance is up, our config files have been read and parsed, this is the opportunity to kick off connections and scheduled processes
void PyPort::Build()
{
	// Only 1 strand per ODC system. Must wait until build as pIOS is not available in the constructor
	if (python_strand == nullptr)
	{
		LOGDEBUG("Create python_strand");
		python_strand.reset(new asio::io_context::strand(*pIOS));
	}

	// Every call to pWrapper should be strand protected.
	python_strand->dispatch([&]()
		{
			// If first time constructor is called, will instansiate the interpreter.
			// Pass in a pointer to our SetTimer method, so it can be called from Python code - bit circular - I know!
			// Also pass in a PublishEventCall method, so Python can send us Events to Publish.
			pWrapper.reset(new PythonWrapper(this->Name, std::bind(&PyPort::SetTimer, this, std::placeholders::_1, std::placeholders::_2),
				std::bind(&PyPort::PublishEventCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

			// Python code is loaded and class created, __init__ called.
			pWrapper->Build("PyPort", MyConf->pyModuleName, MyConf->pyClassName, this->Name);

			pWrapper->Config(JSONMain, JSONOverride);
		});

	LOGDEBUG("Loaded \"{}\" ", MyConf->pyModuleName);

	pServer = ServerManager::AddConnection(pIOS, MyConf->pyHTTPAddr, MyConf->pyHTTPPort); //Static method - creates a new ServerManager if required

	// Now add all the callbacks that we need - the root handler might be a duplicate, in which case it will be ignored!

	auto roothandler = std::make_shared<http::HandlerCallbackType>([](const std::string& absoluteuri, http::reply& rep)
		{
			rep.status = http::reply::ok;
			rep.content.append("You have reached the PyPort http interface.<br>To talk to a port the url must contain the PyPort name, which is case senstive.<br>Anything beyond this will be passed to the Python code.");
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
		});

	ServerManager::AddHandler(pServer, "GET /", roothandler);

	auto gethandler = std::make_shared<http::HandlerCallbackType>([=](const std::string& absoluteuri, http::reply& rep)
		{
			// So when we hit here, someone has make a Get request of our Port. Pass it to Python, and wiat for a response...
			std::string result = pWrapper->RestHandler(absoluteuri); // Expect no long processing or waits in the python code to handle this.
			std::string contenttype = "application/json";

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
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = contenttype;
		});
	ServerManager::AddHandler(pServer, "GET /" + Name, gethandler);

	auto posthandler = std::make_shared<http::HandlerCallbackType>([=](const std::string& absoluteuri, http::reply& rep)
		{

			rep.status = http::reply::ok;
			rep.content.append("You have reached the PyPort Instance with POST on " + Name);
			rep.headers.resize(2);
			rep.headers[0].name = "Content-Length";
			rep.headers[0].value = std::to_string(rep.content.size());
			rep.headers[1].name = "Content-Type";
			rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
		});
	ServerManager::AddHandler(pServer, "POST /" + Name, posthandler);
}

void PyPort::Enable()
{
	if (enabled) return;
	enabled = true;

	python_strand->dispatch([&]()
		{
			pWrapper->Enable();
		});
}

void PyPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	python_strand->dispatch([&]()
		{
			pWrapper->Disable();
		});
}

bool PyPort::GetQualityFlagsFromStringName(const std::string StrQuality, QualityFlags& QualityResult)
{
#define CHECKFLAGSTRING(X) if (StrQuality.find(#X) != std::string::npos) QualityResult |= QualityFlags::X

	QualityResult = QualityFlags::NONE;

	CHECKFLAGSTRING(ONLINE);
	CHECKFLAGSTRING(RESTART);
	CHECKFLAGSTRING(COMM_LOST);
	CHECKFLAGSTRING(REMOTE_FORCED);
	CHECKFLAGSTRING(LOCAL_FORCED);
	CHECKFLAGSTRING(OVERRANGE);
	CHECKFLAGSTRING(REFERENCE_ERR);
	CHECKFLAGSTRING(ROLLOVER);
	CHECKFLAGSTRING(DISCONTINUITY);
	CHECKFLAGSTRING(CHATTER_FILTER);

	return (QualityResult != QualityFlags::NONE); // Should never be none!
}

bool PyPort::GetEventTypeFromStringName(const std::string StrEventType, EventType& EventTypeResult)
{
#define CHECKEVENTSTRING(X) if (StrEventType.find(ToString(X)) != std::string::npos) EventTypeResult = X

	EventTypeResult = EventType::BeforeRange;

	CHECKEVENTSTRING(EventType::ConnectState);
	CHECKEVENTSTRING(EventType::Binary);
	CHECKEVENTSTRING(EventType::Analog);
	CHECKEVENTSTRING(EventType::Counter);
	CHECKEVENTSTRING(EventType::FrozenCounter);
	CHECKEVENTSTRING(EventType::BinaryOutputStatus);
	CHECKEVENTSTRING(EventType::AnalogOutputStatus);
	CHECKEVENTSTRING(EventType::ControlRelayOutputBlock);
	CHECKEVENTSTRING(EventType::OctetString);
	CHECKEVENTSTRING(EventType::BinaryQuality);
	CHECKEVENTSTRING(EventType::DoubleBitBinaryQuality);
	CHECKEVENTSTRING(EventType::AnalogQuality);
	CHECKEVENTSTRING(EventType::CounterQuality);
	CHECKEVENTSTRING(EventType::BinaryOutputStatusQuality);
	CHECKEVENTSTRING( EventType::FrozenCounterQuality);
	CHECKEVENTSTRING(EventType::AnalogOutputStatusQuality);

	return (EventTypeResult != EventType::BeforeRange);
}
bool PyPort::GetControlCodeFromStringName(const std::string StrControlCode, ControlCode& ControlCodeResult)
{
#define CHECKCONTROLCODESTRING(X) if (StrControlCode.find(ToString(X)) != std::string::npos) ControlCodeResult = X

	ControlCodeResult = ControlCode::UNDEFINED;

	CHECKCONTROLCODESTRING(ControlCode::CLOSE_PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::CLOSE_PULSE_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_OFF);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_OFF_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_ON);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::NUL);
	CHECKCONTROLCODESTRING(ControlCode::NUL_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_OFF);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_OFF_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::TRIP_PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::TRIP_PULSE_ON_CANCEL);

	return (ControlCodeResult != ControlCode::UNDEFINED);
}
bool PyPort::GetConnectStateFromStringName(const std::string StrConnectState, ConnectState& ConnectStateResult)
{
#define CHECKCONNECTSTATESTRING(X) if (StrConnectState.find(ToString(X)) != std::string::npos) {ConnectStateResult = X;return true;}

	CHECKCONNECTSTATESTRING(ConnectState::CONNECTED);
	CHECKCONNECTSTATESTRING(ConnectState::DISCONNECTED);
	CHECKCONNECTSTATESTRING(ConnectState::PORT_DOWN);
	CHECKCONNECTSTATESTRING(ConnectState::PORT_UP);

	return false;
}

std::shared_ptr<odc::EventInfo> PyPort::CreateEventFromStrParams(const std::string& EventTypeStr, uint32_t& ODCIndex, const std::string& QualityStr, const std::string& PayloadStr)
{
	EventType EventTypeResult;
	if (!GetEventTypeFromStringName(EventTypeStr, EventTypeResult))
	{
		LOGERROR("Invalid Event Type String passed from Python Code to ODC - {}", EventTypeStr);
		return nullptr;
	}
	QualityFlags QualityResult;
	if (!GetQualityFlagsFromStringName(QualityStr, QualityResult))
	{
		LOGERROR("No Quality Information Passed from Python Code to ODC - {}", QualityStr);
		return nullptr;
	}

	std::shared_ptr<odc::EventInfo> pubevent;

	switch (EventTypeResult)
	{
		case EventType::ConnectState:
		{
			ConnectState state; // PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN
			if (!GetConnectStateFromStringName(PayloadStr, state))
			{
				LOGERROR("Invalid Connection State passed from Python Code to ODC - {}", PayloadStr);
				return nullptr;
			}
			pubevent = std::make_shared<EventInfo>(EventType::ConnectState, 0, Name);
			pubevent->SetPayload<EventType::ConnectState>(std::move(state));
		}
		break;

		case EventType::Binary:
		{
			pubevent = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, QualityResult);
			bool val = (PayloadStr.find("1") != std::string::npos);
			pubevent->SetPayload<EventType::Binary>(std::move(val));
		}
		break;

		case EventType::Analog:
			try
			{
				pubevent = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, QualityResult);
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
				pubevent = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name, QualityResult);
				// Payload String looks like: "|LATCH_ON|Count 1|ON 100ms|OFF 100ms|"
				EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
				auto Parts = split(PayloadStr, '|');
				if (Parts.size() != 5) throw MyException("Payload for ControlRelayOutputBlock does not have enough sections " + PayloadStr);

				ControlCode ControlCodeResult;
				GetControlCodeFromStringName(Parts[1], ControlCodeResult);
				val.functionCode = ControlCodeResult;

				if (Parts[2].find("Count") == std::string::npos) throw MyException("Count field of ControlRelayOutputBlock not in " + Parts[2]);
				val.count = std::stoi(GetNumber(Parts[2]));

				if (Parts[3].find("ON") == std::string::npos) throw MyException("ON field of ControlRelayOutputBlock not in " + Parts[3]);
				val.onTimeMS = std::stoi(GetNumber(Parts[3]));

				if (Parts[4].find("OFF") == std::string::npos) throw MyException("OFF field of ControlRelayOutputBlock not in " + Parts[4]);
				val.offTimeMS = std::stoi(GetNumber(Parts[4]));

				pubevent->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));
			}
			catch (std::exception& e)
			{
				LOGERROR("ControlRelayOutputBlock Value passed from Python failed to be converted - {}, {}", PayloadStr, e.what());
			}
			break;
		case EventType::OctetString:
			LOGERROR("PublishEvent from Python passed an EventType that is not implemented - {}", ToString(EventTypeResult));
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
void PyPort::PublishEventCall(const std::string &EventTypeStr, uint32_t ODCIndex,  const std::string &QualityStr, const std::string &PayloadStr )
{
	LOGDEBUG("PyPort Publish Event {}, {}, {}, {}", EventTypeStr, ODCIndex, QualityStr, PayloadStr);

	// Separate call to allow testing
	std::shared_ptr<EventInfo> pubevent = CreateEventFromStrParams(EventTypeStr, ODCIndex, QualityStr, PayloadStr);

	if (pubevent)
		PublishEvent(pubevent, nullptr);
}
// So we have received an event from the ODC message bus - it will be Control or Connect events.
// So basically the Event mechanism is agnostinc. You can be a producer of control events and a consumer of data events, or the reverse, or in some odd cases -
// A consumer of both types of events (this means you are just a listener on a conversation between two other devices.)
void PyPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		LOGDEBUG("PyPort {} not enabled, Event from {} ignored", Name, SenderName);
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	python_strand->dispatch([&, event, SenderName, pStatusCallback]()
		{
			CommandStatus result = pWrapper->Event(event, SenderName); // Expect no long processing or waits in the python code to handle this.

			PostCallbackCall(pStatusCallback, result);
		});
}
void PyPort::SetTimer(uint32_t id, uint32_t delayms)
{
	if (!enabled)
	{
		LOGDEBUG("PyPort {} not enabled, SetTimer call ignored", Name);
		return;
	}

	pTimer_t timer(new Timer_t(*pIOS));
	timer->expires_from_now(std::chrono::milliseconds(delayms));
	timer->async_wait(
		[&, id, timer](asio::error_code err_code) // Pass in shared ptr to keep it alive until we are done - time out or aborted
		{
			if (err_code != asio::error::operation_aborted)
			{
			      python_strand->dispatch([&,id]()
					{
						pWrapper->CallTimerHandler(id);
					});

			}
		});
}

// This is called when we have decoded a restful request, to the point where we know which instance it should be passed to. We give it a callback, which will
// be used to actually send the response back to the caller.
void PyPort::RestHandler(const std::string& url, ResponseCallback_t pResponseCallback)
{
	if (!enabled)
	{
		LOGDEBUG("PyPort {} not enabled, Restful Request ignored", Name);
		PostResponseCallbackCall(pResponseCallback, "Error Port not enambled");
		return;
	}

	python_strand->dispatch([&, url, pResponseCallback]()
		{
			std::string result = pWrapper->RestHandler(url); // Expect no long processing or waits in the python code to handle this.

			PostResponseCallbackCall(pResponseCallback, result);
		});
}

// Just schedule the callback, don't want to do it in a strand protected section.
void PyPort::PostCallbackCall(const odc::SharedStatusCallback_t& pStatusCallback, CommandStatus c)
{
	if (pStatusCallback != nullptr)
	{
		pIOS->post([&, pStatusCallback, c]()
			{
				(*pStatusCallback)(c);
			});
	}
}
void PyPort::PostResponseCallbackCall(const ResponseCallback_t & pResponseCallback, const std::string& response)
{
	if (pResponseCallback != nullptr)
	{
		pIOS->post([&, pResponseCallback, response]()
			{
				(*pResponseCallback)(response);
			});
	}
}

// This should be called twice, once for the config file setion, and the second for config overrides.
void PyPort::ProcessElements(const Json::Value& JSONRoot)
{
// We need to strip comments from the JSON here, as Python JSON handling libraries throw on finding comments.
	if (JSONMain.length() == 0)
	{
		Json::StreamWriterBuilder wbuilder;
		wbuilder["commentStyle"] = "None";                // No comments
		JSONMain = Json::writeString(wbuilder, JSONRoot); // Spit the root out as string, so we can pass to Python in build.
	}
	else if (JSONOverride.length() == 0)
	{
		Json::StreamWriterBuilder wbuilder;
		wbuilder["commentStyle"] = "None";                    // No comments
		JSONOverride = Json::writeString(wbuilder, JSONRoot); // Spit the root out as string, so we can pass to Python in build.
	}

	if (JSONRoot.isMember("ModuleName"))
		MyConf->pyModuleName = JSONRoot["ModuleName"].asString();
	if (JSONRoot.isMember("ClassName"))
		MyConf->pyClassName = JSONRoot["ClassName"].asString();
}
