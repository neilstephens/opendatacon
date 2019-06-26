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
 * JSONPort.cpp
 *
 *  Created on: 01/11/2016
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <memory>
#include <chrono>
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>
#include "JSONPort.h"

using namespace odc;

JSONPort::JSONPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool aisServer):
	DataPort(aName, aConfFilename, aConfOverrides),
	isServer(aisServer),
	pSockMan(nullptr)
{
	//the creation of a new PortConf will get the point details
	pConf.reset(new JSONPortConf(ConfFilename, aConfOverrides));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

void JSONPort::Enable()
{
	if(enabled) return;
	try
	{
		if(pSockMan.get() == nullptr)
			throw std::runtime_error("Socket manager uninitilised");
		pSockMan->Open();
		enabled = true;
	}
	catch(std::exception& e)
	{
		if(auto log = odc::spdlog_get("JSONPort"))
			log->error("{}: Problem opening connection:: {}", Name, e.what());
		return;
	}
}

void JSONPort::Disable()
{
	if(!enabled) return;
	enabled = false;
	if(pSockMan.get() == nullptr)
		return;
	pSockMan->Close();
}

void JSONPort::SocketStateHandler(bool state)
{
	std::string msg;
	ConnectState conn_state;
	if(state)
	{
		msg = Name+": Connection established.";
		conn_state = ConnectState::CONNECTED;
	}
	else
	{
		msg = Name+": Connection closed.";
		conn_state = ConnectState::DISCONNECTED;
	}
	if(auto log = odc::spdlog_get("JSONPort"))
		log->info(msg);

	//Send an event out
	PublishEvent(std::move(conn_state));
}

void JSONPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(JSONRoot.isMember("IP"))
		static_cast<JSONPortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();

	if(JSONRoot.isMember("Port"))
		static_cast<JSONPortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	//TODO: document this
	if(JSONRoot.isMember("RetryTimems"))
		static_cast<JSONPortConf*>(pConf.get())->retry_time_ms = JSONRoot["RetryTimems"].asUInt();
	//TODO: document this
	if(JSONRoot.isMember("EventBufferSize"))
		static_cast<JSONPortConf*>(pConf.get())->evt_buffer_size = JSONRoot["EventBufferSize"].asUInt();
	//TODO: document this
	if(JSONRoot.isMember("StyleOutput"))
		static_cast<JSONPortConf*>(pConf.get())->style_output = JSONRoot["StyleOutput"].asBool();
}

void JSONPort::Build()
{
	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

	pSockMan = std::make_unique<TCPSocketManager<std::string>>
		           (pIOS, isServer, pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port),
		           std::bind(&JSONPort::ReadCompletionHandler,this,std::placeholders::_1),
		           std::bind(&JSONPort::SocketStateHandler,this,std::placeholders::_1),
		           1000,
		           true,
		           pConf->retry_time_ms);
}

void JSONPort::ReadCompletionHandler(buf_t& readbuf)
{
	//transfer content between matched braces to get processed as json
	char ch;
	std::string braced;
	size_t count_open_braces = 0, count_close_braces = 0;
	while(readbuf.size() > 0)
	{
		ch = readbuf.sgetc();
		readbuf.consume(1);
		if(ch=='{')
		{
			count_open_braces++;
			if(count_open_braces == 1 && braced.length() > 0)
				braced.clear(); //discard anything before the first brace
		}
		else if(ch=='}')
		{
			count_close_braces++;
			if(count_close_braces > count_open_braces)
			{
				braced.clear(); //discard because it must be outside matched braces
				count_close_braces = count_open_braces = 0;
				if(auto log = odc::spdlog_get("JSONPort"))
					log->warn("Malformed JSON recieved: unmatched closing brace.");
			}
		}
		braced.push_back(ch);
		//check if we've found a match to the first brace
		if(count_open_braces > 0 && count_close_braces == count_open_braces)
		{
			ProcessBraced(braced);
			braced.clear();
			count_close_braces = count_open_braces = 0;
		}
	}
	//put back the leftovers
	for(auto ch : braced)
		readbuf.sputc(ch);
}

//At this point we have a whole (hopefully JSON) object - ie. {.*}
//Here we parse it and extract any paths that match our point config
void JSONPort::ProcessBraced(const std::string& braced)
{
	//TODO: make this a reusable reader (class member)
	Json::CharReaderBuilder rbuilder;
	std::unique_ptr<Json::CharReader> const JSONReader(rbuilder.newCharReader());

	char const* start = braced.c_str();
	char const* stop = start + braced.size();

	Json::Value JSONRoot; // will contain the root value after parsing.
	std::string err_str;

	bool parsing_success = JSONReader->parse(start, stop, &JSONRoot, &err_str);
	if (parsing_success)
	{
		JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());

		//little functor to traverse any paths, starting at the root
		//pass a JSON array of nodes representing the path (that's how we store our point config after all)
		auto TraversePath = [&JSONRoot](const Json::Value& nodes)
					  {
						  //val will traverse any paths, starting at the root
						  auto val = JSONRoot;
						  //traverse
						  for (unsigned int n = 0; n < nodes.size(); ++n)
							  if ((val = val[nodes[n].asCString()]).isNull())
								  break;
						  return val;
					  };

		msSinceEpoch_t timestamp = 0;
		if (!pConf->pPointConf->TimestampPath.isNull())
		{
			try
			{
				timestamp = TraversePath(pConf->pPointConf->TimestampPath).asUInt64();
				if (timestamp == 0)
					throw std::runtime_error("Null timestamp");
			}
			catch (std::runtime_error e)
			{
				if (auto log = odc::spdlog_get("JSONPort"))
					log->error("Error decoding timestamp as Uint64: '{}'", e.what());
			}
		}

		//vector to store any events we find contained in this Json object
		std::vector<std::shared_ptr<EventInfo>> events;

		for (auto& point_pair : pConf->pPointConf->Analogs)
		{
			if (!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if (!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::Analog, point_pair.first, Name, QualityFlags::ONLINE, timestamp);
				if (val.isNumeric())
					event->SetPayload<EventType::Analog>(val.asDouble());
				else if (val.isString())
				{
					double value;
					try
					{
						value = std::stod(val.asString());
						event->SetPayload<EventType::Analog>(std::move(value));
					}
					catch (std::exception&)
					{
						if (auto log = odc::spdlog_get("JSONPort"))
							log->error("Error decoding Analog from string '{}', for index {}", val.asString(), point_pair.first);
						event->SetPayload<EventType::Analog>(0);
						event->SetQuality(QualityFlags::OVERRANGE);
					}
				}
				else
				{
					if (auto log = odc::spdlog_get("JSONPort"))
						log->error("Error decoding Analog for index {}", point_pair.first);
					event->SetPayload<EventType::Analog>(0);
					event->SetQuality(QualityFlags::OVERRANGE);
				}
				events.push_back(event);
			}
		}

		for (auto& point_pair : pConf->pPointConf->Binaries)
		{
			if (!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if (!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::Binary, point_pair.first, Name, QualityFlags::ONLINE, timestamp);
				bool true_val = false;
				if (point_pair.second.isMember("TrueVal"))
				{
					true_val = (val == point_pair.second["TrueVal"]);
					if (point_pair.second.isMember("FalseVal"))
						if (!true_val && (val != point_pair.second["FalseVal"]))
							event->SetQuality(QualityFlags::COMM_LOST);
				}
				else if (point_pair.second.isMember("FalseVal"))
					true_val = !(val == point_pair.second["FalseVal"]);
				else if (val.isNumeric() || val.isBool())
					true_val = val.asBool();
				else if (val.isString())
				{
					true_val = (val.asString() == "true");
					if (!true_val && (val.asString() != "false"))
						event->SetQuality(QualityFlags::COMM_LOST);
				}
				else
					event->SetQuality(QualityFlags::COMM_LOST);

				event->SetPayload<EventType::Binary>(std::move(true_val));
				events.push_back(event);
			}
		}

		//Publish any analog and binary events from above
		for (auto& event : events)
		{
			PublishEvent(event);
		}
		//We'll publish any controls separately below, because they each have a callback

		for (auto& point_pair : pConf->pPointConf->Controls)
		{
			if (!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, get the value and send the control
			if (!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, point_pair.first, Name, QualityFlags::NONE, timestamp);

				ControlRelayOutputBlock command;
				command.functionCode = ControlCode::PULSE_ON; //default pulse if nothing else specified

				//work out control code to send
				if (point_pair.second.isMember("ControlMode") && point_pair.second["ControlMode"].isString())
				{
					auto check_val = [&](std::string truename, std::string falsename) -> bool
							     {
								     bool ret = true;
								     if (point_pair.second.isMember(truename))
								     {
									     ret = (val == point_pair.second[truename]);
									     if (point_pair.second.isMember(falsename))
										     if (!ret && (val != point_pair.second[falsename]))
											     throw std::runtime_error("Unexpected control value");
								     }
								     else if (point_pair.second.isMember(falsename))
									     ret = !(val == point_pair.second[falsename]);
								     else if (val.isNumeric() || val.isBool())
									     ret = val.asBool();
								     else if (val.isString()) //Guess some sensible default on/off/trip/close values
								     {
									     //TODO: replace with regex?
									     ret = (val.asString() == "true" ||
									            val.asString() == "True" ||
									            val.asString() == "TRUE" ||
									            val.asString() == "on" ||
									            val.asString() == "On" ||
									            val.asString() == "ON" ||
									            val.asString() == "close" ||
									            val.asString() == "Close" ||
									            val.asString() == "CLOSE");
									     if (!ret && (val.asString() != "false" &&
									                  val.asString() != "False" &&
									                  val.asString() != "FALSE" &&
									                  val.asString() != "off" &&
									                  val.asString() != "Off" &&
									                  val.asString() != "OFF" &&
									                  val.asString() != "trip" &&
									                  val.asString() != "Trip" &&
									                  val.asString() != "TRIP"))
										     throw std::runtime_error("Unexpected control value");
								     }
								     return ret;
							     };

					auto cm = point_pair.second["ControlMode"].asString();
					if (cm == "LATCH")
					{
						bool on;
						try
						{
							on = check_val("OnVal", "OffVal");
						}
						catch (std::runtime_error e)
						{
							if (auto log = odc::spdlog_get("JSONPort"))
								log->error("'{}', for index {}", e.what(), point_pair.first);
							continue;
						}
						if (on)
							command.functionCode = ControlCode::LATCH_ON;
						else
							command.functionCode = ControlCode::LATCH_OFF;
					}
					else if (cm == "TRIPCLOSE")
					{
						bool trip;
						try
						{
							trip = check_val("TripVal", "CloseVal");
						}
						catch (std::runtime_error e)
						{
							if (auto log = odc::spdlog_get("JSONPort"))
								log->error("'{}', for index {}", e.what(), point_pair.first);
							continue;
						}
						if (trip)
							command.functionCode = ControlCode::TRIP_PULSE_ON;
						else
							command.functionCode = ControlCode::CLOSE_PULSE_ON;
					}
					else if (cm != "PULSE")
					{
						if (auto log = odc::spdlog_get("JSONPort"))
							log->error("Unrecongnised ControlMode '{}', recieved for index {}", cm, point_pair.first);
						continue;
					}
				}
				if (point_pair.second.isMember("PulseCount"))
					command.count = point_pair.second["PulseCount"].asUInt();
				if (point_pair.second.isMember("OnTimems"))
					command.onTimeMS = point_pair.second["OnTimems"].asUInt();
				if (point_pair.second.isMember("OffTimems"))
					command.offTimeMS = point_pair.second["OffTimems"].asUInt();

				auto pStatusCallback =
					std::make_shared<std::function<void(CommandStatus)>>([=](CommandStatus command_stat)
						{
							Json::Value result;
							result["Command"]["Index"] = point_pair.first;

							if (command_stat == CommandStatus::SUCCESS)
								result["Command"]["Status"] = "SUCCESS";
							else
								result["Command"]["Status"] = "UNDEFINED";

							//TODO: make this writer reusable (class member)
							//WARNING: Json::StreamWriter isn't threadsafe - maybe just share the StreamWriterBuilder for now...
							Json::StreamWriterBuilder wbuilder;
							if (!pConf->style_output)
								wbuilder["indentation"] = "";
							std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());

							std::ostringstream oss;
							pWriter->write(result, &oss); oss << std::endl;
							pSockMan->Write(oss.str());
						});
				event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(command));
				PublishEvent(event, pStatusCallback);
			}
		}

		for (auto& point_pair : pConf->pPointConf->AnalogControls)
		{
			if (!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, get the value and send the control

			// Now decode the val JSON string to get the index and value and process that
			if (!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, point_pair.first, Name, QualityFlags::ONLINE, timestamp);
				AO16 analogpayload;
				analogpayload.second = CommandStatus::SUCCESS;

				if (auto log = odc::spdlog_get("JSONPort"))
					log->debug("JSNOn AnalogControl Command - {}", val.asString());

				if (val.isNumeric())
					analogpayload.first = val.asUInt();
				else if (val.isString())
				{
					try
					{
						analogpayload.first = std::stoul(val.asString());
					}
					catch (std::exception&)
					{
						if (auto log = odc::spdlog_get("JSONPort"))
							log->error("Error decoding AnalogControl from string '{}', for index {}", val.asString(), point_pair.first);
					}
				}
				else
				{
					if (auto log = odc::spdlog_get("JSONPort"))
						log->error("Error decoding AnalogControl value for index {}", point_pair.first);
					return;
				}
				event->SetPayload<EventType::AnalogOutputInt16>(move(analogpayload));

				auto pStatusCallback =
					std::make_shared<std::function<void(CommandStatus)>>([=](CommandStatus command_stat)
						{
							Json::Value result;
							result["Command"]["Index"] = point_pair.first;

							if (command_stat == CommandStatus::SUCCESS)
								result["Command"]["Status"] = "SUCCESS";
							else
								result["Command"]["Status"] = "UNDEFINED";

							//TODO: make this writer reusable (class member)
							//WARNING: Json::StreamWriter isn't threadsafe - maybe just share the StreamWriterBuilder for now...
							Json::StreamWriterBuilder wbuilder;
							if (!pConf->style_output)
								wbuilder["indentation"] = "";
							std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());

							std::ostringstream oss;
							pWriter->write(result, &oss); oss << std::endl;
							pSockMan->Write(oss.str());
						});

				PublishEvent(event, pStatusCallback);
			}
		}
	}
	else
	{
		if (auto log = odc::spdlog_get("JSONPort"))
			log->warn("Error parsing JSON string: '{}' : '{}'", braced, err_str);
	}
}

void JSONPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
	}

	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

	auto i = event->GetIndex();
	auto q = ToString(event->GetQuality());
	auto t = event->GetTimestamp();
	auto& sp = event->GetSourcePort();
	auto& s = SenderName;
	Json::Value output;
	switch(event->GetEventType())
	{
		case EventType::Analog:
		{
			auto v = event->GetPayload<EventType::Analog>();
			auto& m = pConf->pPointConf->Analogs;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		case EventType::Binary:
		{
			auto v = event->GetPayload<EventType::Binary>();
			auto& m = pConf->pPointConf->Binaries;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		case EventType::ControlRelayOutputBlock:
		{
			auto v = std::string(event->GetPayload<EventType::ControlRelayOutputBlock>());
			auto& m = pConf->pPointConf->Controls;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		default:
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
	}

	if(output.isNull())
	{
		(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
		return;
	}

	//TODO: make this writer reusable (class member)
	//WARNING: Json::StreamWriter isn't threadsafe - maybe just share the StreamWriterBuilder for now...
	Json::StreamWriterBuilder wbuilder;
	if(!pConf->style_output)
		wbuilder["indentation"] = "";
	std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());

	std::ostringstream oss;
	pWriter->write(output, &oss); oss<<std::endl;
	pSockMan->Write(oss.str());

	(*pStatusCallback)(CommandStatus::SUCCESS);
}
