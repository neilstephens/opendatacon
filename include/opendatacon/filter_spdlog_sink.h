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
 * filter_spdlog_sink.h
 *
 *  Created on: 5/7/2023
 *      Author: Neil Stephens
 */
#ifndef FILTER_SPDLOG_SINK_H
#define FILTER_SPDLOG_SINK_H

#include <spdlog/sinks/base_sink.h>
#include <regex>
#include <string>
#include <map>

namespace odc
{

struct filter_sink
{
	virtual void AddFilter(const std::string& regx_str, bool whitelist = false) = 0;
	virtual size_t RemoveFilter(const std::string& regx_str) = 0;
};

template <class T, typename Mutex>
class filter_spdlog_sink: public filter_sink, public spdlog::sinks::base_sink<Mutex>
{
	static_assert(std::is_base_of<spdlog::sinks::sink, T>::value);
public:
	template<typename ... A>
	filter_spdlog_sink(A&& ... args):
		BaseSink(std::forward<A>(args)...)
	{}
	void AddFilter(const std::string& regx_str, bool whitelist) override
	{
		std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
		std::regex filt(regx_str,std::regex::extended);
		auto& List = whitelist ? WhiteList : BlackList;
		List[regx_str] = std::move(filt);
	}
	size_t RemoveFilter(const std::string& regx_str) override
	{
		size_t removed = 0;
		std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
		removed += WhiteList.erase(regx_str);
		removed += BlackList.erase(regx_str);
		return removed;
	}
protected:
	void sink_it_(const spdlog::details::log_msg &msg) override
	{
		//Default is log everything.
		//Blacklist overrides Default.
		//Whitelist overrides Blacklist.
		using p = std::pair<bool,std::map<std::string,std::regex>&>;
		for(const auto& [white,List] : {p{true,WhiteList},p{false,BlackList}})
		{
			for(const auto& [str,rgx] : List)
			{
				if(std::regex_match(msg.logger_name.begin(),msg.logger_name.end(),rgx)
				   || std::regex_match(msg.payload.begin(),msg.payload.end(),rgx))
				{
					if(white)
						BaseSink.log(msg);
					return;
				}
			}
		}
		BaseSink.log(msg);
	}
	void flush_() override
	{
		BaseSink.flush();
	}
	void set_pattern_(const std::string &pattern) override
	{
		BaseSink.set_pattern(pattern);
	}
	void set_formatter_(std::unique_ptr<spdlog::formatter> sink_formatter) override
	{
		BaseSink.set_formatter(std::move(sink_formatter));
	}
private:
	T BaseSink;
	std::map<std::string,std::regex> WhiteList;
	std::map<std::string,std::regex> BlackList;
};

#include "spdlog/details/null_mutex.h"
#include <mutex>
template <class T>
using filter_spdlog_sink_mt = filter_spdlog_sink<T,std::mutex>;
template <class T>
using filter_spdlog_sink_st = filter_spdlog_sink<T,spdlog::details::null_mutex>;

} //namespace odc

#endif // FILTER_SPDLOG_SINK_H
