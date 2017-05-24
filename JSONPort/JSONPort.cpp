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

#include <opendnp3/LogLevels.h>
#include <chrono>
#include "JSONPort.h"

using namespace odc;

JSONPort::JSONPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	write_queue()
{
	//the creation of a new PortConf will get the point details
	pConf.reset(new JSONPortConf(ConfFilename, aConfOverrides));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
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
		static_cast<JSONPortConf*>(pConf.get())->evt_buffer_size = JSONRoot["RetryTimems"].asUInt();
	//TODO: document this
	if(JSONRoot.isMember("StyleOutput"))
		static_cast<JSONPortConf*>(pConf.get())->style_output = JSONRoot["StyleOutput"].asBool();
}

void JSONPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	pWriteQueueStrand.reset(new asio::strand(*pIOS));
}
void JSONPort::Read()
{
	asio::async_read(*pSock.get(), readbuf,asio::transfer_at_least(1), std::bind(&JSONPort::ReadCompletionHandler,this,std::placeholders::_1));
}
void JSONPort::ReadCompletionHandler(asio::error_code err_code)
{
	if(err_code)
	{
		if(err_code != asio::error::eof)
		{
			std::string msg = Name+": Read error: '"+err_code.message()+"'";
			auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			return;
		}
		else
		{
			std::string msg = Name+": '"+err_code.message()+"' : Retrying...";
			auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
	}

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
			if(count_open_braces == 1)
				braced.clear(); //discard anything before the first brace
		}
		if(ch=='}')
		{
			count_close_braces++;
			if(count_close_braces > count_open_braces)
			{
				braced.clear(); //discard because it must be outside matched braces
				count_close_braces = count_open_braces = 0;
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

	if(!err_code) //not eof - read more
		Read();
	else
	{
		//remote end closed the connection - reset and try reconnecting
		PublishEvent(ConnectState::DISCONNECTED , 0);
		Disable();
		Enable();
	}
}

void JSONPort::AsyncFuturesPoll(std::vector<std::future<CommandStatus>>&& future_results, size_t index, std::shared_ptr<Timer_t> pTimer, double poll_time_ms, double backoff_factor)
{
	JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
	bool ready = true;
	bool success = true;
	for(auto& future_result : future_results)
	{
		if(future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			ready = false;
			break;
		}
		//first one that isn't a success, we break
		if(future_result.get() != CommandStatus::SUCCESS)
		{
			success = false;
			break;
		}
	}
	if(ready)
	{
		Json::Value result;
		//FIXME:
		result["Command"]["Index"] =(Json::UInt64) index;
		result["Command"]["Status"] = success ? "SUCCESS" : (future_results.size()==1 ? "FAIL" : "UNDEFINED");
		if(pConf->style_output)
		{
			Json::StyledWriter writer;
			QueueWrite(writer.write(result));
		}
		else
		{
			Json::FastWriter writer;
			QueueWrite(writer.write(result));
		}
	}
	else
	{
		pTimer->expires_from_now(std::chrono::milliseconds((unsigned int)poll_time_ms));
		pTimer->async_wait([=,future_results=std::move(future_results)](asio::error_code err_code) mutable
					 {
						AsyncFuturesPoll(std::move(future_results), index, pTimer, backoff_factor*poll_time_ms, backoff_factor);
					 });
	}
}

//At this point we have a whole (hopefully JSON) object - ie. {.*}
//Here we parse it and extract any paths that match our point config
void JSONPort::ProcessBraced(std::string braced)
{
	Json::Value JSONRoot; // will contain the root value after parsing.
	Json::Reader JSONReader;
	bool parsing_success = JSONReader.parse(braced, JSONRoot);
	if (parsing_success)
	{
		JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());

		//little functor to traverse any paths, starting at the root
		//pass a JSON array of nodes representing the path (that's how we store our point config after all)
		auto TraversePath = [&JSONRoot](const Json::Value nodes)
		{
			//val will traverse any paths, starting at the root
			auto val = JSONRoot;
			//traverse
			for(unsigned int n = 0; n < nodes.size(); ++n)
				if((val = val[nodes[n].asCString()]).isNull())
					break;
			return val;
		};

		Json::Value timestamp_val = TraversePath(pConf->pPointConf->TimestampPath);

		for(auto& point_pair : pConf->pPointConf->Analogs)
		{
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				if(val.isNumeric())
					LoadT(Analog(val.asDouble(),static_cast<uint8_t>(AnalogQuality::ONLINE)),point_pair.first, timestamp_val);
				else if(val.isString())
				{
					double value;
					try
					{
						value = std::stod(val.asString());
					}
					catch(std::exception&)
					{
						LoadT(Analog(0,static_cast<uint8_t>(AnalogQuality::COMM_LOST)),point_pair.first, timestamp_val);
						continue;
					}
					LoadT(Analog(value,static_cast<uint8_t>(AnalogQuality::ONLINE)),point_pair.first, timestamp_val);
				}
			}
		}

		for(auto& point_pair : pConf->pPointConf->Binaries)
		{
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				bool true_val = false; BinaryQuality qual = BinaryQuality::ONLINE;
				if(point_pair.second.isMember("TrueVal"))
				{
					true_val = (val == point_pair.second["TrueVal"]);
					if(point_pair.second.isMember("FalseVal"))
						if (!true_val && (val != point_pair.second["FalseVal"]))
							qual = BinaryQuality::COMM_LOST;
				}
				else if(point_pair.second.isMember("FalseVal"))
					true_val = !(val == point_pair.second["FalseVal"]);
				else if(val.isNumeric() || val.isBool())
					true_val = val.asBool();
				else if(val.isString())
				{
					true_val = (val.asString() == "true");
					if(!true_val && (val.asString() != "false"))
						qual = BinaryQuality::COMM_LOST;
				}
				else
					qual = BinaryQuality::COMM_LOST;

				LoadT(Binary(true_val,static_cast<uint8_t>(qual)),point_pair.first,timestamp_val);
			}
		}

		for(auto& point_pair : pConf->pPointConf->Controls)
		{
			if(!point_pair.second.isMember("JSONPath"))
				continue;
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, get the value and send the control
			if(!val.isNull())
			{
				ControlRelayOutputBlock command(ControlCode::PULSE_ON); //default pulse if nothing else specified

				//work out control code to send
				if(point_pair.second.isMember("ControlMode") && point_pair.second["ControlMode"].isString())
				{
					auto check_val = [&](std::string truename, std::string falsename)->bool
					{
						bool ret = true;
						if(point_pair.second.isMember(truename))
						{
							ret = (val == point_pair.second[truename]);
							if(point_pair.second.isMember(falsename))
								if (!ret && (val != point_pair.second[falsename]))
									throw std::runtime_error("unexpected control value");
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
								throw std::runtime_error("unexpected control value");
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
						catch(std::runtime_error e)
						{
							//TODO: log warning
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
						catch(std::runtime_error e)
						{
							//TODO: log warning
							continue;
						}
						if(trip)
							command.functionCode = ControlCode::TRIP_PULSE_ON;
						else
							command.functionCode = ControlCode::CLOSE_PULSE_ON;
					}
					else if(cm != "PULSE")
					{
						//TODO: log warning
						continue;
					}
				}
				if(point_pair.second.isMember("PulseCount"))
					command.count = point_pair.second["PulseCount"].asUInt();
				if(point_pair.second.isMember("OnTimems"))
					command.onTimeMS = point_pair.second["OnTimems"].asUInt();
				if(point_pair.second.isMember("OffTimems"))
					command.offTimeMS = point_pair.second["OffTimems"].asUInt();

				auto future_results = PublishCommand(command,point_pair.first);

				auto pTimer =std::make_shared<Timer_t>(*pIOS);
				pTimer->expires_from_now(std::chrono::milliseconds(10)); //TODO: expose pole time in config
				pTimer->async_wait([=,future_results=std::move(future_results)](asio::error_code err_code) mutable
							 {
								if(!err_code)
									AsyncFuturesPoll(std::move(future_results), point_pair.first, pTimer, 20, 2);
								else
								{
									Json::Value result;
									result["Command"]["Index"] = point_pair.first;
									result["Command"]["Status"] = "UNDEFINED";
									if(pConf->style_output)
									{
										Json::StyledWriter writer;
										QueueWrite(writer.write(result));
									}
									else
									{
										Json::FastWriter writer;
										QueueWrite(writer.write(result));
									}
								}
							 });
			}
		}
	}
	else
	{
		std::string msg = "Error parsing JSON string: '"+braced+"'";
		auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
	}
}
template<typename T>
inline void JSONPort::LoadT(T meas, uint16_t index, Json::Value timestamp_val)
{
	std::string msg = "Measurement Event '"+std::string(typeid(meas).name())+"'";
	auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::DBG,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	if(!timestamp_val.isNull() && timestamp_val.isUInt64())
	{
		meas = T(meas.value, meas.quality, Timestamp(timestamp_val.asUInt64()));
	}

	PublishEvent(meas, index);
}

void JSONPort::QueueWrite(const std::string &message)
{
	pWriteQueueStrand->post([this,message]()
	{
		write_queue.push_back(message);
		if(write_queue.size() == 1) //need to kick off a write
			Write();
		else if(write_queue.size() > static_cast<JSONPortConf*>(this->pConf.get())->evt_buffer_size) //limit buffer size - drop event
			write_queue.pop_front();
	});
}
void JSONPort::Write()
{
	asio::async_write(*pSock.get(),
				asio::buffer(write_queue[0].c_str(),write_queue[0].size()),
				pWriteQueueStrand->wrap(std::bind(&JSONPort::WriteCompletionHandler,this,std::placeholders::_1,std::placeholders::_2))
				);
}
void JSONPort::WriteCompletionHandler(asio::error_code err_code, size_t bytes_written)
{
	if(err_code)
	{
		if(err_code != asio::error::eof)
		{
			std::string msg = Name+": Write error: '"+err_code.message()+"'";
			auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			return;
		}
		else
		{
			std::string msg = Name+": '"+err_code.message()+"'";
			auto log_entry = openpal::LogEntry("JSONPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
	}
	else
	{
		write_queue.pop_front();
	}
	if(!write_queue.empty())
		Write();
}

//Unsupported types - return as such
std::future<CommandStatus> JSONPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

std::future<CommandStatus> JSONPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::ConnectionEvent(ConnectState state, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

std::future<CommandStatus> JSONPort::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
std::future<CommandStatus> JSONPort::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

//Supported types - call templates
std::future<CommandStatus> JSONPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName){return EventT(meas,index,SenderName);}
std::future<CommandStatus> JSONPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName){return EventT(meas,index,SenderName);}
std::future<CommandStatus> JSONPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName){return EventQ(qual,index,SenderName);}
std::future<CommandStatus> JSONPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName){return EventQ(qual,index,SenderName);}

//Templates for supported types
template<typename T>
inline std::future<CommandStatus> JSONPort::EventQ(const T& meas, uint16_t index, const std::string& SenderName)
{
	return IOHandler::CommandFutureUndefined();
}

template<typename T>
inline std::future<CommandStatus> JSONPort::EventT(const T& meas, uint16_t index, const std::string& SenderName)
{
	if(!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}
	auto pConf = static_cast<JSONPortConf*>(this->pConf.get());

	auto ToJSON = [pConf,index,&meas](std::map<uint16_t, Json::Value>& PointMap)->Json::Value
	{
		if(PointMap.count(index))
		{
			Json::Value output = pConf->pPointConf->pJOT->Instantiate(meas, index, PointMap[index]["Name"].asString());
			return std::move(output);
		}
		return Json::Value::null;
	};
	auto output = std::is_same<T,Analog>::value ? ToJSON(pConf->pPointConf->Analogs) :
			  std::is_same<T,Binary>::value ? ToJSON(pConf->pPointConf->Binaries) :
										  Json::Value::null;
	if(output.isNull())
		return IOHandler::CommandFutureNotSupported();

	if(pConf->style_output)
	{
		Json::StyledWriter writer;
		QueueWrite(writer.write(output));
	}
	else
	{
		Json::FastWriter writer;
		QueueWrite(writer.write(output));
	}
	return IOHandler::CommandFutureSuccess();
}
