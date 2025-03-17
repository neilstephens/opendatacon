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
 * OneShotFunc.h
 *
 *  Created on: 16th March 2025
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ONESHOTFUNC_H
#define ONESHOTFUNC_H

#include <opendatacon/util.h>
#include <memory>
#include <utility>
#include <atomic>
#include <functional>

namespace odc
{

// class that simply wraps a shared_ptr to a std::function and warns if it doesn't get called exactly once
//
//	Main use case is for callbacks that are designed to be called once and only once
//	Highlights cases where user-defined Ports/Transforms neglect to call an Event callback,
//		because PublishEvent, DataConnector, and the Transform base class use this wrapper

template <typename FnT> class OneShotFunc; //FnT is the same style template param as a std::function. Eg OneShotFunc<void(CommandStatus)>

template <typename FnR, typename ... FnArgs>
class OneShotFunc<FnR(FnArgs...)> //Use an explicit specialisation to break down into return and arg types
{
private:
	using FnT = FnR(FnArgs...);
	std::shared_ptr<std::function<FnT>> pFn;
	std::atomic_bool called = false;

public:

	//the only public interface is the static wrapping factory function
	static inline std::shared_ptr<std::function<FnT>> Wrap(const std::shared_ptr<std::function<FnT>>& wrapee)
	{
		auto pOneShot = std::make_shared<OneShotFuncBuilder<FnT>>(wrapee);
		return std::make_shared<std::function<FnT>>([pOneShot](FnArgs... args) -> auto
			{
				return (*pOneShot)(std::forward<FnArgs>(args)...);
			});
	}

	//explicitly delete any default ctors
	OneShotFunc() = delete;
	OneShotFunc(const OneShotFunc&) = delete;
	OneShotFunc(OneShotFunc&&) = delete;
	OneShotFunc& operator=(const OneShotFunc&) = delete;
	OneShotFunc& operator=(OneShotFunc&&) = delete;

protected:

	//protected ctor/dtor so only the factory can use
	explicit OneShotFunc(const std::shared_ptr<std::function<FnT>>& wrapee)
		:pFn(wrapee)
	{}

	//dtor also logs if the fn was never called
	~OneShotFunc()
	{
		if(!called.load())
		{
			if(auto log = odc::spdlog_get("opendatacon"))
			{
				log->error("Callback not called before destruction. Dumping trace backlog if configured.");
				log->dump_backtrace();
			}
		}
	}

private:

	//call operator - calls the wrapped fn after checking if it's been called before
	template <typename ... Args>
	auto operator()(Args&&... args)
	{
		if(called.exchange(true))
		{
			if(auto log = odc::spdlog_get("opendatacon"))
			{
				log->error("Callback called more than once. Dumping trace backlog if configured.");
				log->dump_backtrace();
			}
		}
		return (*pFn)(std::forward<Args>(args)...);
	}

	//otherwise useless derrived class for the sole purpose of enabling make_shared to access protected ctor/dtor from within the factory
	template <typename T>
	struct OneShotFuncBuilder: public OneShotFunc<T>
	{
		OneShotFuncBuilder(const std::shared_ptr<std::function<T>>& wrapee)
			: OneShotFunc<T>(wrapee)
		{};
	};
};

} //namespace odc

#endif // ONESHOTFUNC_H
