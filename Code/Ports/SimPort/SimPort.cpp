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
 * SimPort.cpp
 *
 *  Created on: 29/07/2015
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "SimPort.h"
#include "SimPortCollection.h"
#include "SimPortConf.h"
#include "sqlite3/sqlite3.h"
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <opendatacon/version.h>
#include <chrono>
#include <limits>
#include <memory>
#include <random>

thread_local std::mt19937 SimPort::RandNumGenerator = std::mt19937(std::random_device()());

inline bool ValidEventType(EventType type)
{
	return type == EventType::Binary || type == EventType::Analog;
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

//Implement DataPort interface
SimPort::SimPort(const std::string& Name, const std::string& File, const Json::Value& Overrides):
	DataPort(Name, File, Overrides),
	SimCollection(nullptr)
{
	static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
	static std::weak_ptr<SimPortCollection> weak_collection;

	//if we're the first/only one on the scene,
	// init the SimPortCollection
	if(!init_flag.test_and_set(std::memory_order_acquire))
	{
		//make a custom deleter for the DNP3Manager that will also clear the init flag
		auto deinit_del = [](SimPortCollection* collection_ptr)
					{init_flag.clear(std::memory_order_release); delete collection_ptr;};
		this->SimCollection = std::shared_ptr<SimPortCollection>(new SimPortCollection(), deinit_del);
		weak_collection = this->SimCollection;
	}
	//otherwise just make sure it's finished initialising and take a shared_ptr
	else
	{
		while (!(this->SimCollection = weak_collection.lock()))
		{} //init happens very seldom, so spin lock is good
	}

	pConf = std::make_unique<SimPortConf>();
	pSimConf = static_cast<SimPortConf*>(this->pConf.get());
	pSimConf->Name(Name);
	ProcessFile();
}

void SimPort::Enable()
{
	pEnableDisableSync->post([this]()
		{
			if(!enabled)
			{
				enabled = true;
				PortUp();
				if(!httpServerToken.ServerID.empty())
					HttpServerManager::StartConnection(httpServerToken);
			}
		});
}

void SimPort::Disable()
{
	pEnableDisableSync->post([this]()
		{
			if(enabled)
			{
				PortDown();
				if(!httpServerToken.ServerID.empty())
					HttpServerManager::StopConnection(httpServerToken);
				enabled = false;
			}
		});
}


// Just a way to keep the BinaryVals and Analog+*Vals up to date...
void SimPort::PostPublishEvent(std::shared_ptr<EventInfo> event, SharedStatusCallback_t pStatusCallback = std::make_shared<std::function<void(CommandStatus status)>>([](CommandStatus status) {}))
{
	// deep copy to store for statefulness
	auto cloned_event = std::make_shared<odc::EventInfo>(*event);
	PublishEvent(event, pStatusCallback);
	pSimConf->Event(cloned_event);
}

void SimPort::PublishBinaryEvents(const std::vector<std::size_t>& indexes, const std::string& payload)
{
	for (std::size_t i = 0; i < indexes.size(); ++i)
	{
		auto event = pSimConf->Event(odc::EventType::Binary, indexes[i]);
		event->SetPayload<odc::EventType::Binary>(std::move(payload[i] - '0'));
		event->SetTimestamp(msSinceEpoch());
		PostPublishEvent(event);
	}
}

std::pair<std::string, const IUIResponder *> SimPort::GetUIResponder()
{
	return std::pair<std::string,const SimPortCollection*>("SimControl",this->SimCollection.get());
}

const Json::Value SimPort::GetCurrentState() const
{
	return pSimConf->CurrentState();
}

const Json::Value SimPort::GetStatistics() const
{
	Json::Value stats;
	return stats;
}
const Json::Value SimPort::GetStatus() const
{
	Json::Value status;
	return status;
}

std::vector<std::size_t> SimPort::IndexesFromString(const std::string& index_str, EventType type)
{
	std::vector<std::size_t> indexes;
	//Check for comma separated list,no white space
	std::regex comma_regx("^[0-9]+(?:,[0-9]+)*$");

	if(std::regex_match(index_str, comma_regx))
	{
		// We have matched the regex. Now split.
		auto idxstrings = split(index_str, ',');
		for(const auto& idxs : idxstrings)
		{
			std::size_t idx;
			try
			{
				idx = std::stoi(idxs);
			}
			catch(std::exception& e)
			{
				continue;
			}
			{
				if(pSimConf->IsIndex(type, idx))
					indexes.emplace_back(idx);
			}
		}
	}
	else //Not comma seaparated list
	{//use it as a regex
		try
		{
			std::regex ind_regex(index_str,std::regex::extended);
			const std::vector<std::size_t> allowed_indexes = pSimConf->Indexes(type);
			for(auto allowed : allowed_indexes)
			{
				if(std::regex_match(std::to_string(allowed),ind_regex))
					indexes.emplace_back(allowed);
			}
		}
		catch(std::exception& e)
		{}
	}

	return indexes;
}

bool SimPort::UILoad(EventType type, const std::string& index, const std::string& value, const std::string& quality, const std::string& timestamp, const bool force)
{
	auto log = odc::spdlog_get("SimPort");
	if(log)
		log->debug("{} : UILoad : {}, {}, {}, {}, {}, {}", Name, ToString(type), index, value, quality, timestamp, force);

	if(!enabled)
	{
		if (log)
			log->error("{} : UILoad() when disabled.", Name);
		return false;
	}

	double val = 0.0f;
	msSinceEpoch_t ts = msSinceEpoch();
	try
	{
		if(value != "StartVal")
			val = std::stod(value);
		if (!timestamp.empty())
			ts = std::stoull(timestamp);
	}
	catch(std::exception& e)
	{
		return false;
	}

	QualityFlags Q = quality.empty() ?
	                 (value == "StartVal" ? QualityFlags::NONE : QualityFlags::ONLINE)
	                 : QualityFlagsFromString(quality);

	if (Q == QualityFlags::NONE && value != "StartVal")
		return false;

	auto indexes = IndexesFromString(index, type);
	if (ValidEventType(type) && (indexes.size() > 0))
	{
		auto point_qual = Q;
		for (std::size_t index : indexes)
		{
			if(force)
			{
				pSimConf->ForcedState(type, index, true);
			}
			else if(pSimConf->ForcedState(type, index))
				continue;

			if(value == "StartVal")
			{
				val = pSimConf->StartValue(type, index);
				if(Q == QualityFlags::NONE)
					point_qual = pSimConf->StartQuality(type, index);
			}
			auto event = std::make_shared<EventInfo>(type, index, Name, point_qual, ts);
			if (type == EventType::Binary)
				event->SetPayload<EventType::Binary>(std::move(val));
			if (type == EventType::Analog)
				event->SetPayload<EventType::Analog>(std::move(val));
			PostPublishEvent(event);
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool SimPort::UISetUpdateInterval(EventType type, const std::string& index, const std::string& period)
{
	auto log = odc::spdlog_get("SimPort");
	if(log)
		log->debug("{} : UISetUpdateInterval : {}, {}, {}", Name, ToString(type), index, period);

	unsigned int delta = 0;
	try
	{
		delta = std::stoi(period);
	}
	catch(std::exception& e)
	{
		return false;
	}

	auto indexes = IndexesFromString(index, type);
	if(!indexes.size())
		return false;

	if (ValidEventType(type))
	{
		for (std::size_t index : indexes)
		{
			pSimConf->UpdateInterval(type, index, delta);
			if(enabled)
			{
				auto ptimer = pSimConf->Timer(ToString(type) + std::to_string(index));
				if (!delta)
				{
					ptimer->cancel();
				}
				else
				{
					auto random_interval = std::uniform_int_distribution<unsigned int>(0, delta << 1)(RandNumGenerator);
					ptimer->expires_from_now(std::chrono::milliseconds(random_interval));
					ptimer->async_wait([=](asio::error_code err_code)
						{
							if(enabled && !err_code)
							{
								if (type == EventType::Binary)
									StartBinaryEvents(index);
								if (type == EventType::Analog)
									StartAnalogEvents(index);
							}
						});
				}
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

void SimPort::UISetStdDevScaling(double scale_factor)
{
	pSimConf->std_dev_scaling = scale_factor;
}
bool SimPort::UIToggleAbsAnalogs()
{
	pSimConf->abs_analogs = !pSimConf->abs_analogs;
	return pSimConf->abs_analogs;
}

bool SimPort::UIRelease(EventType type, const std::string& index)
{
	return SetForcedState(index, type, false);
}

bool SimPort::SetForcedState(const std::string& index, EventType type, bool forced)
{
	auto indexes = IndexesFromString(index, type);
	bool result = true;
	if (!index.empty() && ValidEventType(type))
	{
		for (auto idx : indexes)
			pSimConf->ForcedState(type, idx, forced);
	}
	else
	{
		result = false;
	}
	return result;
}

template<odc::EventType event_type>
void SimPort::ResetPoint(std::size_t index)
{
	auto event = pSimConf->Event(event_type, index);
	event->SetTimestamp(msSinceEpoch());
	event->SetPayload<event_type>(pSimConf->StartValue(event_type, index));
	event->SetQuality(pSimConf->StartQuality(event_type, index));
	PostPublishEvent(event);
}

template<odc::EventType event_type>
void SimPort::ResetPoints()
{
	for(auto index : pSimConf->Indexes(event_type))
		ResetPoint<event_type>(index);
}

void SimPort::PortUp()
{
	auto now = msSinceEpoch();

	for(const auto& type : {odc::EventType::Analog,odc::EventType::Binary})
	{
		std::vector<std::size_t> indexes = pSimConf->Indexes(type);
		for(auto index : indexes)
		{
			ptimer_t ptimer = pSimConf->Timer(ToString(type)+std::to_string(index));

			if(TryStartEventsFromDB(type,index,now,ptimer))
				continue;
			//else do random events

			auto event = pSimConf->Event(type, index);
			event->SetTimestamp(msSinceEpoch());
			if(!pSimConf->ForcedState(type,index))
				PostPublishEvent(event);

			auto interval = pSimConf->UpdateInterval(type, index);
			if (interval)
			{
				auto random_interval = std::uniform_int_distribution<unsigned int>(0, interval << 1)(RandNumGenerator);
				ptimer->expires_from_now(std::chrono::milliseconds(random_interval));
				ptimer->async_wait([=](asio::error_code err_code)
					{
						if (enabled && !err_code)
						{
							if(type == odc::EventType::Analog)
								StartAnalogEvents(index);
							else if(type == odc::EventType::Binary)
								StartBinaryEvents(index, !event->GetPayload<odc::EventType::Binary>());
						}
					});
			}
		}
	}
}

void SimPort::PortDown()
{
	pSimConf->CancelTimers();
}

bool SimPort::TryStartEventsFromDB(const EventType type, const size_t index, const msSinceEpoch_t now, const ptimer_t ptimer)
{
	//Check if we're configured to load this point from DB
	auto db_stat = pSimConf->GetDBStat(type,index);
	if(db_stat)
	{
		auto event = std::make_shared<EventInfo>(type,index,Name);
		NextEventFromDB(event);
		auto time_offset = InitDBTimestampHandling(event, now);

		msSinceEpoch_t delta;
		if(now > event->GetTimestamp())
			delta = 0;
		else
			delta = event->GetTimestamp() - now;
		ptimer->expires_from_now(std::chrono::milliseconds(delta));
		ptimer->async_wait([=](asio::error_code err_code)
			{
				if(enabled && !err_code)
					SpawnEvent(event, ptimer, time_offset);
				//else - break timer cycle
			});

		return true;
	}
	return false;
}

void SimPort::NextEventFromDB(const std::shared_ptr<EventInfo>& event)
{
	auto db_stat = pSimConf->GetDBStat(event->GetEventType(),event->GetIndex());
	auto rv = sqlite3_step(db_stat.get());
	if(rv == SQLITE_ROW)
	{
		auto t = static_cast<msSinceEpoch_t>(sqlite3_column_int64(db_stat.get(),0));
		event->SetTimestamp(t);
		switch(event->GetEventType())
		{
			case EventType::Analog:
			{
				event->SetPayload<EventType::Analog>(sqlite3_column_double(db_stat.get(),1));
				break;
			}
			case EventType::Binary:
			{
				auto val = sqlite3_column_int(db_stat.get(),1);
				event->SetPayload<EventType::Binary>(val != 0);
				break;
			}
			default:
				break;
		}
		return;
	}
	else if(rv == SQLITE_DONE)
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->debug("{} : No more SQL records for {} {}", Name, ToString(event->GetEventType()), event->GetIndex());
	}
	else
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->error("{} : sqlite3_step() error for {} {} : ", Name, ToString(event->GetEventType()), event->GetIndex(), sqlite3_errstr(rv));
	}
	//no more records or error - wait forever
	auto forever = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::duration::max()).count();
	event->SetTimestamp(forever);
}

int64_t SimPort::InitDBTimestampHandling(const std::shared_ptr<EventInfo>& event, const msSinceEpoch_t now)
{
	int64_t time_offset = 0;
	const auto timestamp_handling = pSimConf->TimestampHandling(event->GetEventType(),event->GetIndex());
	if(!(timestamp_handling & TimestampMode::ABSOLUTE_T))
	{
		if(!!(timestamp_handling & TimestampMode::FIRST))
		{
			time_offset = now - event->GetTimestamp();
		}
		else if(!!(timestamp_handling & TimestampMode::TOD))
		{
			auto whole_days_ts = std::chrono::duration_cast<days>(std::chrono::milliseconds(event->GetTimestamp()));
			auto whole_days_now = std::chrono::duration_cast<days>(std::chrono::milliseconds(now));
			time_offset = std::chrono::duration_cast<std::chrono::milliseconds>(whole_days_now).count()
			              - std::chrono::duration_cast<std::chrono::milliseconds>(whole_days_ts).count();
		}
		else
		{
			throw std::runtime_error("Invalid timestamp mode: Not absolute, but not relative either");
		}
	}
	if(!(timestamp_handling & TimestampMode::FASTFORWARD))
	{
		//Find the first event that's not in the past
		while(now > (event->GetTimestamp()+time_offset))
			NextEventFromDB(event);
	}
	return time_offset;
}

void SimPort::PopulateNextEvent(const std::shared_ptr<EventInfo>& event, int64_t time_offset)
{
	//Check if we're configured to load this point from DB
	auto db_stat = pSimConf->GetDBStat(event->GetEventType(),event->GetIndex());
	if(db_stat)
	{
		NextEventFromDB(event);
		event->SetTimestamp(event->GetTimestamp()+time_offset);
		return;
	}

	//Otherwise do random
	unsigned int interval;
	if(event->GetEventType() == EventType::Analog)
	{
		RandomiseAnalog(event);
		interval = pSimConf->UpdateInterval(event->GetEventType(), event->GetIndex());
	}
	else if(event->GetEventType() == EventType::Binary)
	{
		bool val = !event->GetPayload<EventType::Binary>();
		event->SetPayload<EventType::Binary>(std::move(val));
		interval = pSimConf->UpdateInterval(event->GetEventType(), event->GetIndex());
	}
	else if(auto log = odc::spdlog_get("SimPort"))
	{
		log->error("{} : Unsupported EventType : '{}'", Name, ToString(event->GetEventType()));
		return;
	}
	else
		return;

	auto random_interval = std::uniform_int_distribution<unsigned int>(0, interval << 1)(RandNumGenerator);
	event->SetTimestamp(msSinceEpoch()+random_interval);
}

void SimPort::SpawnEvent(const std::shared_ptr<EventInfo>& event, ptimer_t pTimer, int64_t time_offset)
{
	//deep copy event to modify as next event
	auto next_event = std::make_shared<EventInfo>(*event);
	if (!pSimConf->ForcedState(event->GetEventType(), event->GetIndex()))
	{
		event->SetTimestamp(msSinceEpoch());
		PostPublishEvent(event);
	}

	PopulateNextEvent(next_event, time_offset);
	auto now = msSinceEpoch();
	msSinceEpoch_t delta;
	if(now > next_event->GetTimestamp())
		delta = 0;
	else
		delta = next_event->GetTimestamp() - now;
	pTimer->expires_from_now(std::chrono::milliseconds(delta));
	//wait til next time
	if(enabled)
		pTimer->async_wait([=](asio::error_code err_code)
			{
				if(enabled && !err_code)
					SpawnEvent(next_event, pTimer, time_offset);
				//else - break timer cycle
			});
}

/*
  Build is called from the main thread before any thread is spawned
  therefore, it is already synchronous code.
  No need to worry about protection
*/
void SimPort::Build()
{
	pSimConf->ProcessElements(JSONConf);

	pEnableDisableSync = pIOS->make_strand();
	auto shared_this = std::static_pointer_cast<SimPort>(shared_from_this());
	this->SimCollection->insert_or_assign(this->Name,shared_this);

	if ((pSimConf->HttpAddress().size() != 0) && (pSimConf->HttpPort().size() != 0))
	{
		httpServerToken = HttpServerManager::AddConnection(pIOS, pSimConf->HttpAddress(), pSimConf->HttpPort()); //Static method - creates a new HttpServerManager if required

		// Now add all the callbacks that we need - the root handler might be a duplicate, in which case it will be ignored!

		auto roothandler = std::make_shared<http::HandlerCallbackType>([](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
			{
				rep.status = http::reply::ok;
				rep.content.append("ERROR - You have reached the SimPort http interface.<br>To talk to a port the url must contain the SimPort name, which is case senstive.<br>The rest is formatted as if it were a console UI command.");
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "text/html"; // http::server::mime_types::extension_to_type(extension);
			});

		HttpServerManager::AddHandler(httpServerToken, "GET /", roothandler);

		std::string VersionResp = fmt::format("{{\"ODCVersion\":\"{}\",\"ConfigFileVersion\":\"{}\"}}", odc::version_string(), odc::GetConfigVersion());
		auto versionhandler = std::make_shared<http::HandlerCallbackType>([=](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
			{
				rep.status = http::reply::ok;

				rep.content.append(VersionResp);
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "application/json"; // http::server::mime_types::extension_to_type(extension);
			});

		HttpServerManager::AddHandler(httpServerToken, "GET /Version", versionhandler);

		auto gethandler = std::make_shared<http::HandlerCallbackType>([this](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
			{
				// So when we hit here, someone has made a Get request of our Named Port.
				// The parameters are type and index, we return value, quality and timestamp
				// Split the absolute uri into parameters.

				std::string type = "";
				std::string index = "";
				std::string error = "";

				if (parameters.count("type") != 0)
					type = parameters.at("type");
				else
					error = "No 'type' parameter found";

				if (parameters.count("index") != 0)
					index = parameters.at("index");
				else
					error += " No 'index' parameter found";

				std::string result = "";
				std::string contenttype = "application/json";

				if (error.length() == 0)
				{
					if (to_lower(type) == "binary")
					{
						std::vector<std::size_t> indexes = IndexesFromString(index, EventType::Binary);
						result = pSimConf->CurrentState(EventType::Binary, indexes);
					}
					if (to_lower(type) == "analog")
					{
						std::vector<std::size_t> indexes = IndexesFromString(index, EventType::Analog);
						result = pSimConf->CurrentState(EventType::Analog, indexes);
					}
					if (to_lower(type) == "control")
					{
						std::vector<std::size_t> indexes = IndexesFromString(index, EventType::ControlRelayOutputBlock);
						result = pSimConf->CurrentState(EventType::ControlRelayOutputBlock, indexes);
					}
				}
				if (result.length() != 0)
				{
					rep.status = http::reply::ok;
					rep.content.append(result);
				}
				else
				{
					rep.status = http::reply::not_found;
					contenttype = "text/html";
					rep.content.append("ERROR - You have reached the SimPort Instance with GET on " + Name + " Invalid Request " + type + ", " + index + " - " + error);
				}
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = contenttype;
				LOGDEBUG("{} Get Command {}, Resp {}", Name, absoluteuri, rep.content);
			});
		HttpServerManager::AddHandler(httpServerToken, "GET /" + Name, gethandler);

		auto posthandler = std::make_shared<http::HandlerCallbackType>([this](const std::string& absoluteuri, const http::ParameterMapType& parameters, const std::string& content, http::reply& rep)
			{
				// So when we hit here, someone has made a POST request of our Port.
				// The UILoad checks the values past to it and sets sensible defaults if they are missing.

				EventType type = EventType::BeforeRange;
				std::string index = "";
				std::string value = "";
				std::string quality = "";
				std::string timestamp = "";
				std::string period = "";
				std::string error = "";

				if (parameters.count("type") != 0)
					type = EventTypeFromString(parameters.at("type"));
				else
					error = "No 'type' parameter found";

				if (parameters.count("index") != 0)
					index = parameters.at("index");
				else
					error += " No 'index' parameter found";

				if (parameters.count("quality") != 0)
					quality = parameters.at("quality");
				if (parameters.count("timestamp") != 0)
					timestamp = parameters.at("timestamp");

				if (parameters.count("force") != 0)
				{
					bool result = false;
					if (((to_lower(parameters.at("force")) == "true") || (parameters.at("force") == "1")))
					{
						result = SetForcedState(index, type, true);
					}
					if (((to_lower(parameters.at("force")) == "false") || (parameters.at("force") == "0")))
					{
						result = SetForcedState(index, type, false);
					}
					if (result == false)
					{
						error += "  Unable to set forced";
					}
					else
					{
						rep.content.append("Set Period Command Accepted\n");
					}
				}

				if (parameters.count("value") != 0)
					value = parameters.at("value");
				if (parameters.count("period") != 0)
					period = parameters.at("period");

				if ((error.length() == 0) && (value.length() != 0)) // Forced set above
				{
					if (UILoad(type, index, value, quality, timestamp, false))
					{
						rep.status = http::reply::ok;
						rep.content.append("Set Value Command Accepted\n");
					}
					else
						error += " Unable to set value (invalid index?) ";
				}

				if ((error.length() == 0) && (period.length() != 0) )
				{
					if (UISetUpdateInterval(type, index, period))
					{
						rep.status = http::reply::ok;
						rep.content.append("Set Period Command Accepted\n");
					}
					else
						error += " Unable to set Period (invalid index?) ";
				}

				if (!((value.length() > 0) || (period.length() > 0)))
				{
					error += " Missing a value or period (or both) parameter(s). Must have at least one.";
				}

				if (error.length() != 0)
				{
					rep.status = http::reply::not_found;
					rep.content.assign("ERROR - You have reached the SimPort Instance with POST on " + Name + " POST Command Failed - " + error);
				}
				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = std::to_string(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "text/html";
				LOGDEBUG("{} Post/Get Command {}, Resp {}", Name, absoluteuri, rep.content);
			});
		HttpServerManager::AddHandler(httpServerToken, "POST /" + Name, posthandler);
		HttpServerManager::AddHandler(httpServerToken, "GET /post/" + Name, posthandler); // Allow the post functionality but using a get - easier for testing!
		HttpServerManager::StartConnection(httpServerToken);
	}
}

/*
  ProcessElements is called from the main thread before any thread is spawned
  therefore, it is already synchronous code.
  No need to worry about protection
*/
void SimPort::ProcessElements(const Json::Value& json_root)
{
	//This function is called multiple times (includes, overrides etc)
	//Store and override the config, so we can pass it to SimPortConf to process it one hit in Build()
	auto MemberNames = json_root.getMemberNames();
	for(auto mn : MemberNames)
		JSONConf[mn] = json_root[mn];
}

void SimPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	CommandStatus status = CommandStatus::NOT_SUPPORTED;
	std::string message = "Control not supported";
	std::size_t index = 0;
	//if(auto log = odc::spdlog_get("SimPort"))
	//    log->trace("{}: Received control for Name {}", Name);

	if (event->GetEventType() == EventType::ControlRelayOutputBlock)
	{
		index = event->GetIndex();
		if (pSimConf->IsIndex(odc::EventType::ControlRelayOutputBlock, index))
		{
			status = odc::CommandStatus::SUCCESS;
			auto feedbacks = pSimConf->BinaryFeedbacks(index);
			if (!feedbacks.empty())
			{
				auto& command = event->GetPayload<EventType::ControlRelayOutputBlock>();
				status = HandleBinaryFeedback(feedbacks, index, command, message);
			}

			std::shared_ptr<PositionFeedback> bp = pSimConf->GetPositionFeedback(index);
			if (bp)
			{
				status = HandlePositionFeedback(bp, event->GetPayload<EventType::ControlRelayOutputBlock>(), message);
			}

			auto& command = event->GetPayload<EventType::ControlRelayOutputBlock>();
			auto payload = command;
			std::shared_ptr<odc::EventInfo> control_event = std::make_shared<odc::EventInfo>(odc::EventType::ControlRelayOutputBlock, index, event->GetSourcePort(), event->GetQuality());
			control_event->SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(payload));
			control_event->SetTimestamp(event->GetTimestamp());
			pSimConf->SetLatestControlEvent(control_event, index);
		}
	}
	EventResponse(message, index, pStatusCallback, status);
}

std::pair<CommandStatus,std::string>
SimPort::SendOneBinaryFeedback(const std::shared_ptr<BinaryFeedback>& fb, const std::size_t index, const odc::ControlRelayOutputBlock& command)
{
	CommandStatus status = CommandStatus::SUCCESS;
	std::string message = "Binary feedback event processing is a success";
	bool forced = false;
	if(fb->mode == FeedbackMode::PULSE)
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->trace("{}: Control {}: Pulse feedback to Binary {}.", Name, index, fb->on_value->GetIndex());
		switch(command.functionCode)
		{
			case ControlCode::PULSE_ON:
			case ControlCode::LATCH_ON:
			case ControlCode::LATCH_OFF:
			case ControlCode::CLOSE_PULSE_ON:
			case ControlCode::TRIP_PULSE_ON:
			{
				if(!pSimConf->ForcedState(odc::EventType::Binary, fb->on_value->GetIndex()))
				{
					auto event = std::make_shared<odc::EventInfo>(*fb->on_value);
					event->SetTimestamp();
					PostPublishEvent(event);
				}
				else
					forced = true;
				ptimer_t ptimer = pIOS->make_steady_timer();
				ptimer->expires_from_now(std::chrono::milliseconds(command.onTimeMS));
				ptimer->async_wait([ptimer,fb,this](asio::error_code)
					{
						if(!pSimConf->ForcedState(odc::EventType::Binary, fb->off_value->GetIndex()))
						{
							auto event = std::make_shared<odc::EventInfo>(*fb->off_value);
							event->SetTimestamp();
							PostPublishEvent(event);
						}
					});
				//TODO: (maybe) implement multiple pulses - command has count and offTimeMS
				break;
			}
			default:
				message = "Binary feedback is not supported";
				status = CommandStatus::NOT_SUPPORTED;
		}
	}
	else //LATCH
	{
		if (IsOnCommand(command.functionCode))
		{
			if(auto log = odc::spdlog_get("SimPort"))
				log->trace("{}: Control {}: Latch on feedback to Binary {}.",
					Name, index,fb->on_value->GetIndex());
			fb->on_value->SetTimestamp();
			if(!pSimConf->ForcedState(odc::EventType::Binary, fb->on_value->GetIndex()))
			{
				auto event = std::make_shared<odc::EventInfo>(*fb->on_value);
				event->SetTimestamp();
				PostPublishEvent(event);
			}
			else
				forced = true;
		}
		else if (IsOffCommand(command.functionCode))
		{
			if(auto log = odc::spdlog_get("SimPort"))
				log->trace("{}: Control {}: Latch off feedback to Binary {}.",
					Name, index, fb->off_value->GetIndex());
			fb->off_value->SetTimestamp();
			if(!pSimConf->ForcedState(odc::EventType::Binary, fb->off_value->GetIndex()))
			{
				auto event = std::make_shared<odc::EventInfo>(*fb->off_value);
				event->SetTimestamp();
				PostPublishEvent(event);
			}
			else
				forced = true;
		}
		else
		{
			message = "Binary feedback is not supported";
			status = CommandStatus::NOT_SUPPORTED;
		}
	}
	if (forced)
	{
		message = "This binary control point is blocked because it is forced";
		status = CommandStatus::BLOCKED;
	}
	return {status,message};
}

CommandStatus SimPort::HandleBinaryFeedback(const std::vector<std::shared_ptr<BinaryFeedback>>& feedbacks, std::size_t index, const odc::ControlRelayOutputBlock& command, std::string& message)
{
	CommandStatus status = CommandStatus::SUCCESS;
	message = "Binary feedback event processing is a success";
	for(auto& fb : feedbacks)
	{
		if(fb->delay != 0)
		{
			ptimer_t ptimer = pIOS->make_steady_timer();
			ptimer->expires_from_now(std::chrono::milliseconds(fb->delay));
			ptimer->async_wait([this,fb,index,command,ptimer](asio::error_code)
				{
					SendOneBinaryFeedback(fb,index,command);
				});
		}
		else
		{
			auto stat_msg = SendOneBinaryFeedback(fb,index,command);
			if(stat_msg.first != CommandStatus::SUCCESS)
			{
				status = stat_msg.first;
				message = stat_msg.second;
			}
		}
	}
	return status;
}

CommandStatus SimPort::HandlePositionFeedback(const std::shared_ptr<PositionFeedback>& binary_position, const odc::ControlRelayOutputBlock& command, std::string& message)
{
	CommandStatus status = CommandStatus::NOT_SUPPORTED;
	message = "This binary position point is not supported";
	if (binary_position->type == FeedbackType::ANALOG)
		status = HandlePositionFeedbackForAnalog(binary_position, command, message);
	else if (binary_position->type == FeedbackType::BINARY)
		status = HandlePositionFeedbackForBinary(binary_position, command, message);
	else if (binary_position->type == FeedbackType::BCD)
		status = HandlePositionFeedbackForBCD(binary_position, command, message);
	return status;
}

CommandStatus SimPort::HandlePositionFeedbackForAnalog(const std::shared_ptr<PositionFeedback>& binary_position, const odc::ControlRelayOutputBlock& command, std::string& message)
{
	CommandStatus status = CommandStatus::NOT_SUPPORTED;
	message = "this binary position control is not supported";
	const std::size_t index = binary_position->indexes[0];
	if (pSimConf->IsIndex(odc::EventType::Analog, index))
	{
		if (!pSimConf->ForcedState(odc::EventType::Analog, index))
		{
			PositionAction action = binary_position->action[ON];
			if (IsOffCommand(command.functionCode))
				action = binary_position->action[OFF];
			auto event = pSimConf->Event(odc::EventType::Analog, index);
			double payload = event->GetPayload<odc::EventType::Analog>();
			if (action == PositionAction::RAISE)
			{
				if (payload + binary_position->tap_step <= binary_position->raise_limit)
				{
					payload += binary_position->tap_step;
					event->SetPayload<odc::EventType::Analog>(std::move(payload));
					event->SetTimestamp(msSinceEpoch());
					PostPublishEvent(event);
					message = "binary position event processing a success";
					status = CommandStatus::SUCCESS;
				}
				else
				{
					message = "this binary control is out or range";
					status = CommandStatus::OUT_OF_RANGE;
				}
			}
			else if (action == PositionAction::LOWER)
			{
				if (payload - binary_position->tap_step >= binary_position->lower_limit)
				{
					payload -= binary_position->tap_step;
					event->SetPayload<odc::EventType::Analog>(std::move(payload));
					event->SetTimestamp(msSinceEpoch());
					PostPublishEvent(event);
					message = "binary position event processing a success";
					status = CommandStatus::SUCCESS;
				}
				else
				{
					message = "this binary control is out or range";
					status = CommandStatus::OUT_OF_RANGE;
				}
			}
		}
		else
		{
			message = "this binary control position is blocked due to forced state";
			status = CommandStatus::BLOCKED;
		}
	}
	return status;
}

CommandStatus SimPort::HandlePositionFeedbackForBinary(const std::shared_ptr<PositionFeedback>& binary_position, const odc::ControlRelayOutputBlock& command, std::string& message)
{
	CommandStatus status = CommandStatus::NOT_SUPPORTED;
	message = "this binary control position is not supported";

	// check for all the binary indexes present
	std::string binary;
	for (std::size_t index : binary_position->indexes)
	{
		if (!pSimConf->IsIndex(odc::EventType::Binary, index))
			return status;
		if (pSimConf->ForcedState(odc::EventType::Binary, index))
		{
			message = "this binary control position is blocked due to forced state";
			return CommandStatus::BLOCKED;
		}
		binary += std::to_string(static_cast<bool>(pSimConf->Payload(odc::EventType::Binary, index)));
	}

	std::size_t payload = odc::to_decimal(binary);
	PositionAction action = binary_position->action[ON];
	if (IsOffCommand(command.functionCode))
		action = binary_position->action[OFF];
	if (action == PositionAction::RAISE)
	{
		if (payload + binary_position->tap_step <= binary_position->raise_limit)
		{
			payload += int(binary_position->tap_step);
			binary = odc::to_binary(payload, binary_position->indexes.size());
			PublishBinaryEvents(binary_position->indexes, binary);
			message = "the event processing for this binary control position is a success";
			status = CommandStatus::SUCCESS;
		}
		else
		{
			message = "this binary control is out or range";
			status = CommandStatus::OUT_OF_RANGE;
		}
	}
	else if (action == PositionAction::LOWER)
	{
		if (payload - binary_position->tap_step >= binary_position->lower_limit)
		{
			payload -= int(binary_position->tap_step);
			binary = odc::to_binary(payload, binary_position->indexes.size());
			PublishBinaryEvents(binary_position->indexes, binary);
			message = "the event processing for this binary control position is a success";
			status = CommandStatus::SUCCESS;
		}
		else
		{
			message = "this binary control is out or range";
			status = CommandStatus::OUT_OF_RANGE;
		}
	}
	return status;
}

CommandStatus SimPort::HandlePositionFeedbackForBCD(const std::shared_ptr<PositionFeedback>& binary_position, const odc::ControlRelayOutputBlock& command, std::string& message)
{
	CommandStatus status = CommandStatus::NOT_SUPPORTED;
	message = "this binary control position is not supported for bcd type";
	std::string bcd_binary;
	for (std::size_t index : binary_position->indexes)
	{
		if (!pSimConf->IsIndex(odc::EventType::Binary, index))
			return status;
		if (pSimConf->ForcedState(odc::EventType::Binary, index))
		{
			message = "this binary control position is blocked due to forced state";
			return CommandStatus::BLOCKED;
		}
		bcd_binary += std::to_string(static_cast<bool>(pSimConf->Payload(odc::EventType::Binary, index)));
	}

	std::size_t payload = bcd_encoded_to_decimal(bcd_binary);
	PositionAction action = binary_position->action[ON];
	if (IsOffCommand(command.functionCode))
		action = binary_position->action[OFF];
	if (action == PositionAction::RAISE)
	{
		if (payload + binary_position->tap_step <= binary_position->raise_limit)
		{
			payload += int(binary_position->tap_step);
			bcd_binary = odc::decimal_to_bcd_encoded_string(payload, binary_position->indexes.size());
			PublishBinaryEvents(binary_position->indexes, bcd_binary);
			message = "the event processing for this binary control position is a success";
			status = CommandStatus::SUCCESS;
		}
		else
		{
			message = "this binary control is out or range";
			status = CommandStatus::OUT_OF_RANGE;
		}
	}
	else if (action == PositionAction::LOWER)
	{
		if (payload - binary_position->tap_step >= binary_position->lower_limit)
		{
			payload -= int(binary_position->tap_step);
			bcd_binary = odc::decimal_to_bcd_encoded_string(payload, binary_position->indexes.size());
			PublishBinaryEvents(binary_position->indexes, bcd_binary);
			message = "the event processing for this binary control position is a success";
			status = CommandStatus::SUCCESS;
		}
		else
		{
			message = "this binary control is out or range";
			status = CommandStatus::OUT_OF_RANGE;
		}
	}
	return status;
}

void SimPort::EventResponse(const std::string& message, std::size_t index, SharedStatusCallback_t pStatusCallback, CommandStatus status)
{
	if(auto log = odc::spdlog_get("SimPort"))
		log->trace("{} : {} for Index {}", Name, message, index);
	(*pStatusCallback)(status);
}

bool SimPort::IsOffCommand(odc::ControlCode code) const
{
	return code == ControlCode::LATCH_OFF ||
	       code == ControlCode::TRIP_PULSE_ON ||
	       code == ControlCode::PULSE_OFF;
}

bool SimPort::IsOnCommand(odc::ControlCode code) const
{
	return code == ControlCode::LATCH_ON ||
	       code == ControlCode::CLOSE_PULSE_ON ||
	       code == ControlCode::PULSE_ON;
}

