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

#include <opendatacon/IOTypes.h>
#include <memory>
#include <utility>
#include <atomic>
#include <functional>
#include <tuple>

namespace odc
{

// class that simply wraps a shared_ptr to a callable object and warns if it doesn't get called exactly once
//
//	Main use case is for callbacks that are designed to be called once and only once
//	Highlights cases where user-defined Ports/Transforms neglect to call an Event callback,
//		because PublishEvent, DataConnector, and the Transform base class use this wrapper

template <typename FnT>
class OneShotFunc
{
private:

	std::shared_ptr<std::function<FnT>> pFn;
	std::atomic_bool called = false;

	//extract the args that the function expects to a tuple type
	template <typename T> struct ArgsTuple;

	template <typename R, typename ... Args>
	struct ArgsTuple<std::function<R(Args...)>> { using type = std::tuple<Args...>; };

	using FnArgs = typename ArgsTuple<std::function<FnT>>::type;

public:

	//ctor
	explicit OneShotFunc(const std::shared_ptr<std::function<FnT>>& wrapee)
		:pFn(wrapee)
	{}

	//call operator - the whole reason for this class - warn if it's called more than once
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

	static inline std::shared_ptr<std::function<FnT>> Wrap(const std::shared_ptr<std::function<FnT>>& wrapee)
	{
		auto pOneShot = std::make_shared<OneShotFunc<FnT>>(wrapee);
		return std::make_shared<std::function<FnT>>([pOneShot](FnArgs args) -> auto
			{
				return std::apply((*pOneShot),args);
			});
	}

	//dtor - also warn if it wasn't called
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
};

} //namespace odc

#endif // ONESHOTFUNC_H
