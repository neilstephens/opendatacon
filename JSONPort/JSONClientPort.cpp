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
 * JSONClientDataPort.cpp
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <thread>
#include <chrono>
#include <opendnp3/LogLevels.h>
#include "JSONClientPort.h"

JSONClientPort::JSONClientPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	JSONPort(aName, aConfFilename, aConfOverrides)
{};

void JSONClientPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{

}

void JSONClientPort::Enable()
{
	if(enabled) return;
	JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
	try
	{
		asio::ip::tcp::resolver resolver(*pIOS);
		asio::ip::tcp::resolver::query query(pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port));
		auto endpoint_iterator = resolver.resolve(query);
		pSock.reset(new asio::ip::tcp::socket(*pIOS));
		asio::async_connect(*pSock.get(), endpoint_iterator,std::bind(&JSONClientPort::ConnectCompletionHandler,this,std::placeholders::_1));
	}
	catch(std::exception& e)
	{
		std::string msg = "Problem opening connection: "+Name+": "+e.what();
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
};
void JSONClientPort::ConnectCompletionHandler(asio::error_code err_code)
{
	if(err_code)
	{
		std::string msg = Name+": Connect error: '"+err_code.message()+"'";
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::INFO,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		//try again later
		//TODO: add the timeout as a config item
		pTCPRetryTimer.reset(new Timer_t(*pIOS, std::chrono::seconds(20)));
		pTCPRetryTimer->async_wait(
				[this](asio::error_code err_code)
				{
					if(err_code != asio::error::operation_aborted)
						Enable();
				});
		return;
	}
	std::string msg = Name+": Connect success!";
	auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::INFO,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	enabled = true;
	Read();
}
void JSONClientPort::Read()
{
	asio::async_read(*pSock.get(), buf,asio::transfer_at_least(1), std::bind(&JSONClientPort::ReadCompletionHandler,this,std::placeholders::_1));
}
void JSONClientPort::ReadCompletionHandler(asio::error_code err_code)
{
	if(err_code)
	{
		if(err_code != asio::error::eof)
		{
			std::string msg = Name+": Read error: '"+err_code.message()+"'";
			auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			return;
		}
		else
		{
			std::string msg = Name+": '"+err_code.message()+"' : Retrying...";
			auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
	}

	//transfer content between matched braces to get processed as json
	char ch;
	std::string braced;
	size_t count_open_braces = 0, count_close_braces = 0;
	while(buf.size() > 0)
	{
		ch = buf.sgetc();
		buf.consume(1);
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
		buf.sputc(ch);

	if(!err_code)//not eof - read more
		Read();
	else
	{
		//remote end closed the connection - reset and try reconnecting
		Disable();
		Enable();
	}
}

//At this point we have a whole (hopefully JSON) object - ie. {.*}
//Here we parse it and extract any paths that match our point config
void JSONClientPort::ProcessBraced(std::string braced)
{
	Json::Value JSONRoot;   // will contain the root value after parsing.
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
				if((val = val[nodes[n].asCString()]).isNull())break;
			return val;
		};

		for(auto& point_pair : pConf->pPointConf->Analogs)
		{
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				if(val.isNumeric())
					LoadT(opendnp3::Analog(val.asDouble(),static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE)),point_pair.first);
				else if(val.isString())
				{
					double value;
					try
					{
						value = std::stod(val.asString());
					}
					catch(std::exception&)
					{
						LoadT(opendnp3::Analog(0,static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST)),point_pair.first);
						continue;
					}
					LoadT(opendnp3::Analog(value,static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE)),point_pair.first);
				}
			}
		}

		for(auto& point_pair : pConf->pPointConf->Binaries)
		{
			Json::Value val = TraversePath(point_pair.second["JSONPath"]);
			//if the path existed, load up the point
			if(!val.isNull())
			{
				bool true_val = false; opendnp3::BinaryQuality qual = opendnp3::BinaryQuality::ONLINE;
				if(!point_pair.second["TrueVal"].isNull())
				{
					true_val = (val == point_pair.second["TrueVal"]);
					if(!point_pair.second["FalseVal"].isNull())
						if (!true_val && (val != point_pair.second["FalseVal"]))
							qual = opendnp3::BinaryQuality::COMM_LOST;
				}
				else if(!point_pair.second["FlaseVal"].isNull())
					true_val = !(val == point_pair.second["FalseVal"]);
				else if(val.isNumeric() || val.isBool())
					true_val = val.asBool();
				else if(val.isString())
				{
					true_val = (val.asString() == "true");
					if(!true_val && (val.asString() != "false"))
						qual = opendnp3::BinaryQuality::COMM_LOST;
				}
				else
					qual = opendnp3::BinaryQuality::COMM_LOST;

				LoadT(opendnp3::Binary(true_val,static_cast<uint8_t>(qual)),point_pair.first);
			}
		}
		//TODO: implement controls
	}
	else
	{
		std::string msg = "Error parsing JSON string: '"+braced+"'";
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
	}
}
template<typename T>
inline void JSONClientPort::LoadT(T meas, uint16_t index)
{
	std::string msg = "Measurement Event '"+std::string(typeid(meas).name())+"'";
	auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::DBG,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	for(auto IOHandler_pair : Subscribers)
		IOHandler_pair.second->Event(meas, index, this->Name);
}
void JSONClientPort::Disable()
{
	//cancel the retry timer (otherwise it would tie up the io_service on shutdown)
	if(pTCPRetryTimer.get() != nullptr)
		pTCPRetryTimer->cancel();
	//shutdown and close socket by using destructor
	pSock.reset(nullptr);
	enabled = false;
};
