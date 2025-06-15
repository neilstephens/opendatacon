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

#include "JSONPort.h"
#include <chrono>
#include <memory>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>

using namespace odc;

JSONPort::JSONPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool aisServer):
	DataPort(aName, aConfFilename, aConfOverrides),
	isServer(aisServer),
	pSockMan(nullptr)
{
	//the creation of a new PortConf will get the point details
	pConf = std::make_unique<JSONPortConf>(ConfFilename, aConfOverrides);

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

void JSONPort::Enable()
{
	if(enabled) return;
	try
	{
		if(!pSockMan)
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
	if(!pSockMan)
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
	//TODO: document this
	if(JSONRoot.isMember("PrintAllEvents"))
		static_cast<JSONPortConf*>(pConf.get())->print_all = JSONRoot["PrintAllEvents"].asBool();
}

void JSONPort::Build()
{
	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

	pSockMan = std::make_unique<TCPSocketManager>
		           (pIOS, isServer, pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port),
		           std::bind(&JSONPort::ReadCompletionHandler,this,std::placeholders::_1),
		           std::bind(&JSONPort::SocketStateHandler,this,std::placeholders::_1),
		           1000,
		           true,
		           pConf->retry_time_ms);

	std::vector<std::shared_ptr<const EventInfo>> init_events;
	init_events.reserve(pConf->pPointConf->Analogs.size()+
		pConf->pPointConf->Binaries.size()+
		pConf->pPointConf->OctetStrings.size()+
		pConf->pPointConf->Controls.size()+
		pConf->pPointConf->AnalogControls.size());

	for(const auto& point : pConf->pPointConf->Analogs)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Analog,point.first,"",QualityFlags::RESTART,0));
	for(const auto& point : pConf->pPointConf->Binaries)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::Binary,point.first,"",QualityFlags::RESTART,0));
	for(const auto& point : pConf->pPointConf->OctetStrings)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::OctetString,point.first,"",QualityFlags::RESTART,0));
	for(const auto& point : pConf->pPointConf->Controls)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::ControlRelayOutputBlock,point.first,"",QualityFlags::RESTART,0));
	for(const auto& point : pConf->pPointConf->AnalogControls)
		init_events.emplace_back(std::make_shared<const EventInfo>(EventType::AnalogOutputDouble64,point.first,"",QualityFlags::RESTART,0));

	pDB = std::make_unique<EventDB>(init_events);
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
					log->warn("Malformed JSON received: unmatched closing brace.");
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

DataToStringMethod GetOctetStringFormat(const Json::Value& point_json)
{
	auto format = DataToStringMethod::Raw;
	if(point_json.isMember("Format"))
	{
		if(point_json["Format"].asString() == "Hex")
			format = DataToStringMethod::Hex;
		else if(point_json["Format"].asString() == "Base64")
			format = DataToStringMethod::Base64;
		else if(point_json["Format"].asString() != "Raw")
		{
			if(auto log = odc::spdlog_get("JSONPort"))
				log->warn("Unknown format for OctetString specified '{}'. Using raw text", point_json["Format"].asString());
		}
	}
	return format;
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

	bool parsing_success = JSONReader->parse(start,stop,&JSONRoot,&err_str);
	if (parsing_success)
	{
		auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

		//little functor to traverse any paths, starting at the root
		//pass a JSON array of nodes representing the path (that's how we store our point config after all)
		auto TraversePath = [&JSONRoot](const Json::Value& nodes)
					  {
						  //val will traverse any paths, starting at the root
						  auto val = JSONRoot;
						  //traverse
						  for(unsigned int n = 0; n < nodes.size(); ++n)
							  if((val = val[nodes[n].asCString()]).isNull())
								  break;
						  return val;
					  };

		msSinceEpoch_t timestamp = msSinceEpoch();
		if(!pConf->pPointConf->TimestampPath.isNull())
		{
			try
			{
				timestamp = TraversePath(pConf->pPointConf->TimestampPath).asUInt64();
				if(timestamp == 0)
					throw std::runtime_error("Null timestamp");
			}
			catch(std::runtime_error& e)
			{
				if(auto log = odc::spdlog_get("JSONPort"))
					log->error("Error decoding timestamp as Uint64: '{}'",e.what());
			}
		}

		//vector to store any events we find contained in this Json object
		std::vector<std::shared_ptr<EventInfo>> events;

		for(auto& point_pair : pConf->pPointConf->Analogs)
		{
			if(!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::Analog,point_pair.first,Name,QualityFlags::ONLINE,timestamp);
				if(val.isNumeric())
					event->SetPayload<EventType::Analog>(val.asDouble());
				else if(val.isString())
				{
					double value;
					try
					{
						value = std::stod(val.asString());
						event->SetPayload<EventType::Analog>(std::move(value));
					}
					catch(std::exception&)
					{
						if(auto log = odc::spdlog_get("JSONPort"))
							log->error("Error decoding Analog from string '{}', for index {}",val.asString(),point_pair.first);
						event->SetPayload<EventType::Analog>(0);
						event->SetQuality(QualityFlags::OVERRANGE);
					}
				}
				else
				{
					if(auto log = odc::spdlog_get("JSONPort"))
						log->error("Error decoding Analog for index {}",point_pair.first);
					event->SetPayload<EventType::Analog>(0);
					event->SetQuality(QualityFlags::OVERRANGE);
				}
				events.push_back(event);
			}
		}

		for(auto& point_pair : pConf->pPointConf->Binaries)
		{
			if(!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::Binary,point_pair.first,Name,QualityFlags::ONLINE,timestamp);
				bool true_val = false;
				if(point_pair.second.isMember("TrueVal"))
				{
					true_val = (val == point_pair.second["TrueVal"]);
					if(point_pair.second.isMember("FalseVal"))
						if (!true_val && (val != point_pair.second["FalseVal"]))
							event->SetQuality(QualityFlags::COMM_LOST);
				}
				else if(point_pair.second.isMember("FalseVal"))
					true_val = !(val == point_pair.second["FalseVal"]);
				else if(val.isNumeric() || val.isBool())
					true_val = val.asBool();
				else if(val.isString())
				{
					true_val = (val.asString() == "true");
					if(!true_val && (val.asString() != "false"))
						event->SetQuality(QualityFlags::COMM_LOST);
				}
				else
					event->SetQuality(QualityFlags::COMM_LOST);

				event->SetPayload<EventType::Binary>(std::move(true_val));
				events.push_back(event);
			}
		}

		for(auto& point_pair : pConf->pPointConf->OctetStrings)
		{
			if(!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::OctetString,point_pair.first,Name,QualityFlags::ONLINE,timestamp);
				auto format = GetOctetStringFormat(point_pair.second);

				std::string unmodified_string = val.asString();
				switch(format)
				{
					case DataToStringMethod::Raw:
						event->SetPayload<EventType::OctetString>(std::move(unmodified_string));
						break;
					case DataToStringMethod::Hex:
						try
						{
							event->SetPayload<EventType::OctetString>(hex2buf(unmodified_string));
						}
						catch(const std::exception& e)
						{
							auto msg = fmt::format("Invalid hex string '{}': {}", unmodified_string, e.what());
							if(auto log = odc::spdlog_get("JSONPort"))
								log->warn(msg);
							event->SetPayload<EventType::OctetString>(std::move(msg));
						}
						break;
					case DataToStringMethod::Base64:
						try
						{
							//FIXME: b64decode doesn't check validity and throw. It likely just crashes.
							// do the check here instead of try/catch
							event->SetPayload<EventType::OctetString>(b64decode(unmodified_string));
						}
						catch(const std::exception& e)
						{
							auto msg = fmt::format("Invalid base64 string '{}': {}", unmodified_string, e.what());
							if(auto log = odc::spdlog_get("JSONPort"))
								log->warn(msg);
							event->SetPayload<EventType::OctetString>(std::move(msg));
						}
					default:
						if(auto log = odc::spdlog_get("JSONPort"))
							log->warn("Unsupported string to data method {}",format);
						event->SetPayload<EventType::OctetString>(std::move(unmodified_string));
						break;
				}
				events.push_back(event);
			}
		}

		//Publish any events from above
		for(auto& event : events)
		{
			pDB->Set(event);
			PublishEvent(event);
		}
		//We'll publish any controls separately below, because they each have a callback

		for(auto& point_pair : pConf->pPointConf->Controls)
		{
			if(!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, get the value and send the control
			if(!val.isNull())
			{
				auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock,point_pair.first,Name,QualityFlags::NONE,timestamp);

				ControlRelayOutputBlock command;
				command.functionCode = ControlCode::PULSE_ON; //default pulse if nothing else specified

				//work out control code to send
				if(point_pair.second.isMember("ControlMode") && point_pair.second["ControlMode"].isString())
				{
					auto check_val = [&point_pair,&val](const std::string& truename, const std::string& falsename) -> bool
							     {
								     bool ret = true;
								     if(point_pair.second.isMember(truename))
								     {
									     ret = (val == point_pair.second[truename]);
									     if(point_pair.second.isMember(falsename))
										     if (!ret && (val != point_pair.second[falsename]))
											     throw std::runtime_error("Unexpected control value");
								     }
								     else if(point_pair.second.isMember(falsename))
									     ret = !(val == point_pair.second[falsename]);
								     else if(val.isNumeric() || val.isBool())
									     ret = val.asBool();
								     else if(val.isString()) //Guess some sensible default on/off/trip/close values
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
									     if(!ret && (val.asString() != "false" &&
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
					if(cm == "LATCH")
					{
						bool on;
						try
						{
							on = check_val("OnVal","OffVal");
						}
						catch(std::runtime_error& e)
						{
							if(auto log = odc::spdlog_get("JSONPort"))
								log->error("'{}', for index {}",e.what(),point_pair.first);
							continue;
						}
						if(on)
							command.functionCode = ControlCode::LATCH_ON;
						else
							command.functionCode = ControlCode::LATCH_OFF;
					}
					else if(cm == "TRIPCLOSE")
					{
						bool trip;
						try
						{
							trip = check_val("TripVal","CloseVal");
						}
						catch(std::runtime_error& e)
						{
							if(auto log = odc::spdlog_get("JSONPort"))
								log->error("'{}', for index {}",e.what(),point_pair.first);
							continue;
						}
						if(trip)
							command.functionCode = ControlCode::TRIP_PULSE_ON;
						else
							command.functionCode = ControlCode::CLOSE_PULSE_ON;
					}
					else if(cm != "PULSE")
					{
						if(auto log = odc::spdlog_get("JSONPort"))
							log->error("Unrecongnised ControlMode '{}', received for index {}",cm,point_pair.first);
						continue;
					}
				}
				if(point_pair.second.isMember("PulseCount"))
					command.count = point_pair.second["PulseCount"].asUInt();
				if(point_pair.second.isMember("OnTimems"))
					command.onTimeMS = point_pair.second["OnTimems"].asUInt();
				if(point_pair.second.isMember("OffTimems"))
					command.offTimeMS = point_pair.second["OffTimems"].asUInt();

				auto pStatusCallback =
					std::make_shared<std::function<void(CommandStatus)>>([=](CommandStatus command_stat)
						{
							Json::Value result;
							result["Command"]["Index"] = point_pair.first;
							result["Command"]["Status"] = odc::ToString(command_stat);

							//TODO: make this writer reusable (class member)
							//WARNING: Json::StreamWriter isn't threadsafe - maybe just share the StreamWriterBuilder for now...
							Json::StreamWriterBuilder wbuilder;
							if(!pConf->style_output)
								wbuilder["indentation"] = "";
							std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());

							std::ostringstream oss;
							pWriter->write(result, &oss); oss<<std::endl;
							pSockMan->Write(oss.str());
						});
				event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(command));
				pDB->Set(event);
				PublishEvent(event,pStatusCallback);
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
				event->SetPayload<EventType::AnalogOutputInt16>(std::move(analogpayload));

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
				pDB->Set(event);
				PublishEvent(event, pStatusCallback);
			}
		}
	}
	else
	{
		if(auto log = odc::spdlog_get("JSONPort"))
			log->warn("Error parsing JSON string: '{}' : '{}'", braced, err_str);
	}
}

inline Json::Value JSONPort::ToJSON(std::shared_ptr<const EventInfo> event, const std::string& SenderName) const
{
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
			typename EventTypePayload<EventType::Analog>::type v;
			try
			{
				v = event->GetPayload<EventType::Analog>();
			}
			catch(std::runtime_error&)
			{}
			auto& m = pConf->pPointConf->Analogs;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : (pConf->print_all) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,"UNKNOWN",sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		case EventType::Binary:
		{
			typename EventTypePayload<EventType::Binary>::type v;
			try
			{
				v = event->GetPayload<EventType::Binary>();
			}
			catch(std::runtime_error&)
			{}
			auto& m = pConf->pPointConf->Binaries;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : (pConf->print_all) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,"UNKNOWN",sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		case EventType::OctetString:
		{
			auto& m = pConf->pPointConf->OctetStrings;
			auto format = m.count(i) ? GetOctetStringFormat(m[i]) : DataToStringMethod::Hex;
			std::string v;
			try
			{
				v = ToString(event->GetPayload<EventType::OctetString>(),format);
			}
			catch(std::runtime_error&)
			{}
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : (pConf->print_all) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,"UNKNOWN",sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		case EventType::ControlRelayOutputBlock:
		{
			std::string v = "";
			try
			{
				v = event->GetPayloadString();
			}
			catch(std::runtime_error&)
			{}
			auto& m = pConf->pPointConf->Controls;
			output = (m.count(i) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,m[i]["Name"].asString(),sp,s)
			          : (pConf->print_all) ? pConf->pPointConf->pJOT->Instantiate(i,v,q,t,"UNKNOWN",sp,s)
			          : Json::Value::nullSingleton());
			break;
		}
		default:
			return Json::Value::nullSingleton();
	}
	return output;
}

void JSONPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}
	pDB->Set(event);

	auto output = ToJSON(event,SenderName);

	if(output.isNull())
	{
		(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
		return;
	}

	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

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

const Json::Value JSONPort::GetCurrentState() const
{
	if(!pDB)
		return Json::Value();

	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

	auto time_str = since_epoch_to_datetime(msSinceEpoch());
	Json::Value ret;
	ret[time_str]["Analogs"] = Json::arrayValue;
	ret[time_str]["Binaries"] = Json::arrayValue;
	ret[time_str]["Controls"] = Json::arrayValue;
	ret[time_str]["AnalogControls"] = Json::arrayValue;

	for(const auto& point : pConf->pPointConf->Analogs)
		ret[time_str]["Analogs"].append(ToJSON(pDB->Get(EventType::Analog,point.first)));
	for(const auto& point : pConf->pPointConf->Binaries)
		ret[time_str]["Binaries"].append(ToJSON(pDB->Get(EventType::Binary,point.first)));
	for(const auto& point : pConf->pPointConf->Controls)
		ret[time_str]["Controls"].append(ToJSON(pDB->Get(EventType::ControlRelayOutputBlock,point.first)));
	for(const auto& point : pConf->pPointConf->AnalogControls)
		ret[time_str]["AnalogControls"].append(ToJSON(pDB->Get(EventType::AnalogOutputDouble64,point.first)));

	return ret;
}
