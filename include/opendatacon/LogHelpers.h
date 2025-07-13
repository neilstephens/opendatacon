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
 * LogHelpers.h
 *
 *  Created on: 12/07/2025
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#ifndef LOGHELPERS_H
#define LOGHELPERS_H

#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <atomic>

namespace odc
{

/* The idea of these log helpers is to enable classes to log conveniently, but also efficiently,
 * simply by adding a LogHelpers static data member
 *	- avoids having to fetch log pointers from the central spdlog registry every time (locking bottle-neck)
 *	- but since logs can be reloaded dynamically, and this is for static lifetime, we can't simply store a shared_ptr
 *	- so this uses a weak_ptr and refreshes it when needed
 *	- also optimises 'should log' queries by caching the result for trace level
 *		- the main datacon signals that the cache is invalid by bumping the global sequence number
 */

class LogHelpers final
{
public:
	LogHelpers(const std::string& log_name):
		logname(log_name),
		plog(odc::make_shared(std::weak_ptr<spdlog::logger>())),
		log(*plog)
	{}
	virtual ~LogHelpers() = default;

	inline std::shared_ptr<spdlog::logger> GetLog() const
	{
		if(auto l = log.lock())
			return l;
		auto l = odc::spdlog_get(logname);
		if(l) l->debug("Refreshing expired log weak_ptr. RefreshSequenceNum {}", GetLogRefreshSequenceNum());
		log = l;
		return l;
	}
	inline bool ShouldLog(const spdlog::level::level_enum& lvl) const
	{
		//trace gets special treatment - only check the underlying log if the global log refresh sequence has changed
		if(!lvl)
		{
			auto seq = GetLogRefreshSequenceNum();
			if(refreshSeq == seq)
				return trace_should_log;
			refreshSeq = seq;
			if(auto l = GetLog())
			{
				l->debug("Refreshing 'trace_should_log' cache due to sequence bump. RefreshSequenceNum {}", seq);
				trace_should_log = l->should_log(lvl);
				return trace_should_log;
			}
			return false;
		}
		if(auto l = GetLog()) return l->should_log(lvl);
		return false;
	}

	//Expose the convenience log functions one-for-one
	#define CONVENIENCE(Level,level)\
		template<typename ... Args> inline bool Level(Args&&... args) const \
		{                                                                   \
			if(auto l = GetLog())                                         \
			{                                                             \
				l->level(std::forward<Args>(args)...);                  \
				return true;                                            \
			}                                                             \
			return false;                                                 \
		}
	CONVENIENCE(Trace,trace)
	CONVENIENCE(Debug,debug)
	CONVENIENCE(Info,info)
	CONVENIENCE(Warn,warn)
	CONVENIENCE(Error,error)
	CONVENIENCE(Critical,critical)
	#undef CONVENIENCE //don't let the macro escape to the outside world

private:
	std::string logname;
	//manage the weak_ptr with a shared_ptr, so I can force the destructor to run in libODC
	std::shared_ptr<std::weak_ptr<spdlog::logger>> plog;
	std::weak_ptr<spdlog::logger>& log;
	mutable std::atomic_bool trace_should_log;
	mutable std::atomic_size_t refreshSeq = 9999;
};

} //namespace odc

#endif // LOGHELPERS_H
