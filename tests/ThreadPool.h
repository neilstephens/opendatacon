/*	opendatacon
*
*	Copyright (c) 2015:
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
 *  Created on: 24/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <opendatacon/asio.h>

class ThreadPool
{
public:
	ThreadPool(size_t num = 1)
	{
		for(auto n = num; n>0; n--)
			threads.emplace_back([this](){pIOS->run();});
	}
	~ThreadPool()
	{
		pWork.reset();
		pIOS->run();
		for(auto& t : threads)
			t.join();
		threads.clear();
	}
private:
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::unique_ptr<asio::io_service::work> pWork = pIOS->make_work();
	std::vector<std::thread> threads;
};

#endif // THREADPOOL_H
