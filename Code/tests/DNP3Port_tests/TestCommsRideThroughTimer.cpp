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
/**
 */
#include "TestDNP3Helpers.h"
#include "../ThreadPool.h"
#include "../../Ports/DNP3Port/CommsRideThroughTimer.h"
#include "../../Ports/DNP3Port/CommsRideThroughTimer.cpp"
#include <opendatacon/asio.h>
#include <catch.hpp>
#include <atomic>

#define SUITE(name) "CommsRideThroughTimer - " name

const size_t testTimeout = 10000;

const size_t msRideTime = 1000;
std::atomic_bool CommsIsBad = false;
std::atomic_bool CommsToggled = false;
const auto SetCommsBad = [](){ CommsIsBad = true; };
const auto SetCommsGood = [](){ CommsIsBad = false; CommsToggled = true; };

inline void WaitFor(const std::atomic_bool& var, const bool state)
{
	auto pIOS = odc::asio_service::Get();
	auto start_time = odc::msSinceEpoch();
	while((odc::msSinceEpoch() - start_time) < testTimeout && var != state)
		pIOS->poll_one();
	CHECK(var == state);
}

inline void WaitFor(size_t ms)
{
	auto pIOS = odc::asio_service::Get();
	auto start_time = odc::msSinceEpoch();
	while((odc::msSinceEpoch() - start_time) < ms)
		pIOS->poll_one();
}

TEST_CASE(SUITE("Simple Timeout"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	auto trigger_time = odc::msSinceEpoch();
	pCRTT->Trigger();

	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - trigger_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Cancel Trigger"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msCancel = msRideTime*0.7;

	auto trigger_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(msCancel);
	pCRTT->Cancel();
	pCRTT->Trigger();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - trigger_time;
	CHECK(measured_duration > 0.95*(msRideTime+msCancel));
	CHECK(measured_duration < 1.05*(msRideTime+msCancel));

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Trigger Pause Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msPause = msRideTime*0.7;

	auto trigger_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(msRideTime*0.5);
	pCRTT->Pause();
	WaitFor(msPause);
	pCRTT->Resume();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - trigger_time;
	CHECK(measured_duration > 0.95*(msRideTime+msPause));
	CHECK(measured_duration < 1.05*(msRideTime+msPause));
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Trigger Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msPause = msRideTime*0.5;

	auto start_time = odc::msSinceEpoch();
	pCRTT->Pause();
	WaitFor(msPause*0.5);
	pCRTT->Trigger();
	WaitFor(msPause*0.5);
	pCRTT->Resume();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*(msRideTime+msPause));
	CHECK(measured_duration < 1.05*(msRideTime+msPause));
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Trigger Resume Cancel"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msPause = msRideTime*0.2;

	pCRTT->Pause();
	WaitFor(msPause*0.5);
	pCRTT->Trigger();
	WaitFor(msPause*0.5);
	pCRTT->Resume();
	pCRTT->Cancel();
	WaitFor(msRideTime*1.2);

	CHECK_FALSE(CommsIsBad);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Cancel Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msPause = msRideTime*0.5;

	pCRTT->Trigger();
	pCRTT->Pause();
	WaitFor(msPause*0.5);
	pCRTT->Cancel();
	pCRTT->Trigger();
	WaitFor(msPause*0.5);
	auto start_time = odc::msSinceEpoch();
	pCRTT->Resume();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Resume Trigger"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);


	pCRTT->Pause();
	pCRTT->Resume();
	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Resume Repeat"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	for(int i = 0; i < 100; i++)
	{
		pCRTT->Pause();
		pCRTT->Resume();
	}
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause Trigger Cancel Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	const size_t msPause = msRideTime*0.5;

	pCRTT->Pause();
	WaitFor(msPause*0.5);
	pCRTT->Trigger();
	pCRTT->Cancel();
	WaitFor(msPause*0.5);
	pCRTT->Resume();
	pCRTT->Trigger();
	auto start_time = odc::msSinceEpoch();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Random"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	std::random_device rd;
	std::mt19937 g(rd());
	auto log = odc::spdlog_get("DNP3Port");

	auto pPause = std::make_shared<std::function<void(void)>>([pCRTT,log](){pCRTT->Pause(); log->debug("Pause");});
	auto pTrigger = std::make_shared<std::function<void(void)>>([pCRTT,log](){pCRTT->Trigger(); log->debug("Trigger");});
	auto pCancel = std::make_shared<std::function<void(void)>>([pCRTT,log](){pCRTT->Cancel(); log->debug("Cancel");});
	auto pResume = std::make_shared<std::function<void(void)>>([pCRTT,log](){pCRTT->Resume(); log->debug("Resume");});

	std::vector<std::shared_ptr<std::function<void(void)>>> random_actions = {pPause,pTrigger,pCancel,pResume};

	for(int i=0; i<10; i++)
	{
		CommsIsBad = false; CommsToggled = false;
		for(int j=0; j<20; j++)
		{
			std::shuffle(random_actions.begin(), random_actions.end(), g);
			for(auto& action : random_actions)
				(*action)();
		}

		auto start_time = odc::msSinceEpoch();
		if(random_actions.back() == pTrigger)
		{
			WaitFor(CommsIsBad,true);
			auto measured_duration = odc::msSinceEpoch() - start_time;
			CHECK(measured_duration > 0.95*msRideTime);
			CHECK(measured_duration < 1.05*msRideTime);
		}
		else if(random_actions.back() == pCancel)
		{
			WaitFor(msRideTime*1.2);
			CHECK_FALSE(CommsIsBad);
		}
		else
		{
			pCRTT->Resume();
			pCRTT->Cancel();
			pCRTT->Trigger();
			WaitFor(CommsIsBad,true);
			auto measured_duration = odc::msSinceEpoch() - start_time;
			CHECK(measured_duration > 0.95*msRideTime);
			CHECK(measured_duration < 1.05*msRideTime);
		}
		pCRTT->Cancel();
		WaitFor(CommsIsBad,false);
	}

	//sleep for a bit to make sure logs finish
	log->flush();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	TestTearDown();
}

TEST_CASE(SUITE("FastForward"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(msRideTime*0.5);
	pCRTT->FastForward();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime*0.5);
	CHECK(measured_duration < 1.05*msRideTime*0.5);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Pause FastForward Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(msRideTime*0.5);
	pCRTT->Pause();
	WaitFor(msRideTime*0.1);
	pCRTT->FastForward();
	WaitFor(msRideTime*0.1);
	pCRTT->Resume();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime*0.7);
	CHECK(measured_duration < 1.05*msRideTime*0.7);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("Trigger Cancel FastForward"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	pCRTT->Trigger();
	pCRTT->Cancel();
	pCRTT->FastForward();
	WaitFor(msRideTime*0.2);
	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration > 0.95*msRideTime);
	CHECK(measured_duration < 1.05*msRideTime);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}

TEST_CASE(SUITE("FastForward Pause Resume"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(1);
	CommsIsBad = false; CommsToggled = false;

	auto pCRTT = std::make_shared<CommsRideThroughTimer>(*pIOS, msRideTime, SetCommsGood, SetCommsBad);

	auto start_time = odc::msSinceEpoch();
	pCRTT->Trigger();
	pCRTT->FastForward();
	pCRTT->Pause();
	pCRTT->Trigger();
	pCRTT->Resume();
	WaitFor(CommsIsBad,true);

	auto measured_duration = odc::msSinceEpoch() - start_time;
	CHECK(measured_duration < 0.05*msRideTime);
	CHECK_FALSE(CommsToggled);

	pCRTT->Cancel();
	WaitFor(CommsIsBad,false);

	TestTearDown();
}
