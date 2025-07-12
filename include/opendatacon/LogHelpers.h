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

/* The idea of these log helpers is to enable derrived classes to log conveniently, but also efficiently
 *	- avoids having to fetch log pointers from the central spdlog registry every time (locking bottle-neck)
 *	- but since logs can be reloaded dynamically, we can't keep pointers forever
 *		- so this uses weak_ptrs and tries to refresh them when they expire
 */

class LogHelpers
{
protected:
	LogHelpers() = default;

	// SetLog needs to be called by the derrived classes that know their logger name
	inline void SetLog(const std::string& log_name) { logname = log_name; }

	inline std::shared_ptr<spdlog::logger> GetLog() const
	{
		if(auto l = log.lock())
			return l;
		auto l = odc::spdlog_get(logname);
		log = l;
		return l;
	}
	inline std::shared_ptr<spdlog::logger> MainLog() const
	{
		if(auto l = mainlog.lock())
			return l;
		auto l = odc::spdlog_get("opendatacon");
		mainlog = l;
		return l;
	}
	inline bool ShouldLog(const spdlog::level::level_enum& lvl) const
	{
		//trace gets special treatment - only check the underlying log if the global log refresh sequence has changed
		if(!lvl)
		{
			if(refreshSeq == RefreshSequenceNum)
				return trace_should_log;
			refreshSeq.store(RefreshSequenceNum);
			if(auto l = GetLog())
			{
				trace_should_log = l->should_log(lvl);
				return trace_should_log;
			}
			return false;
		}
		if(auto l = GetLog()) return l->should_log(lvl);
		return false;
	}
	inline bool MainShouldLog(const spdlog::level::level_enum& lvl) const
	{
		//trace gets special treatment - only check the underlying log if the global log refresh sequence has changed
		if(!lvl)
		{
			if(refreshSeqMain == RefreshSequenceNum)
				return trace_should_log_main;
			refreshSeqMain.store(RefreshSequenceNum);
			if(auto l = MainLog())
			{
				trace_should_log_main = l->should_log(lvl);
				return trace_should_log_main;
			}
			return false;
		}
		if(auto l = MainLog()) return l->should_log(lvl);
		return false;
	}

	//And for our great convenience!
	template<typename ... Args> void LogTrace(Args&&... args) const { if(auto l = GetLog())l->trace(std::forward<Args>(args)...);}
	template<typename ... Args> void LogDebug(Args&&... args) const { if(auto l = GetLog())l->debug(std::forward<Args>(args)...);}
	template<typename ... Args> void LogInfo(Args&&... args) const { if(auto l = GetLog())l->info(std::forward<Args>(args)...);}
	template<typename ... Args> void LogWarn(Args&&... args) const { if(auto l = GetLog())l->warn(std::forward<Args>(args)...);}
	template<typename ... Args> void LogError(Args&&... args) const { if(auto l = GetLog())l->error(std::forward<Args>(args)...);}
	template<typename ... Args> void LogCritical(Args&&... args) const { if(auto l = GetLog())l->critical(std::forward<Args>(args)...);}

private:
	std::string logname;
	mutable std::weak_ptr<spdlog::logger> log;
	mutable std::weak_ptr<spdlog::logger> mainlog;
	mutable std::atomic_bool trace_should_log;
	mutable std::atomic_bool trace_should_log_main;
	mutable std::atomic_size_t refreshSeq = 9999;
	mutable std::atomic_size_t refreshSeqMain = 9999;

public:
	inline static std::atomic_size_t RefreshSequenceNum = 0;
	virtual ~LogHelpers() = default;
};

} //namespace odc

#endif // LOGHELPERS_H
