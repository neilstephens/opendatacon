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

JSONPort::JSONPort(std::shared_ptr<JSONPortManager> Manager, std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	Manager_(Manager),
	write_queue(),
	WriteQueueStrand(Manager->get_io_service())
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

void JSONPort::BuildOrRebuild()
{
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
		//TODO: implement controls
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
	WriteQueueStrand.post([this,message]()
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
				WriteQueueStrand.wrap(std::bind(&JSONPort::WriteCompletionHandler,this,std::placeholders::_1,std::placeholders::_2))
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
