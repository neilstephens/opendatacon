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
 * BatchUpdateBuilderTests.cpp
 *
 *  Created on: 11/05/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "../Code/Ports/DNP3Port/BatchUpdateBuilder.h"
#include "TestDNP3Helpers.h"
#include "../ThreadPool.h"
#include <catch.hpp>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

#define SUITE(name) "BatchUpdateBuilderTests - " name

// Parameters chosen for fast tests with comfortable timing margins.
//   EMA tick = maxBatchPeriodms / maxBatchCount = 200 / 5 = 40ms
static constexpr size_t MAX_PERIOD_MS = 200;
static constexpr size_t MAX_COUNT     = 5;
static constexpr double EMA_WEIGHT    = 0.5;
static constexpr auto TICK          = 40ms;
static constexpr auto GENEROUS_WAIT = 400ms; // 2x MAX_PERIOD_MS

class MockOutstation: public opendnp3::IOutstation
{
public:
	mutable std::mutex mtx;
	std::vector<size_t> batches;

	void Apply(const opendnp3::Updates& updates) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		batches.push_back(updates.size());
	}

	size_t FlushCount() const { std::lock_guard<std::mutex> lock(mtx); return batches.size(); }
	size_t TotalEvents() const { std::lock_guard<std::mutex> lock(mtx); size_t n=0; for(auto b: batches) n+=b;return n; }
	size_t MaxBatchSize() const { std::lock_guard<std::mutex> lock(mtx); size_t m=0; for(auto b: batches) m=std::max(m,b);return m; }
};

struct Fixture
{
	std::shared_ptr<MockOutstation>     os = std::make_shared<MockOutstation>();
	std::shared_ptr<BatchUpdateBuilder> bb = std::make_shared<BatchUpdateBuilder>(os, MAX_PERIOD_MS, MAX_COUNT, EMA_WEIGHT);

	void sendEvents(int n, std::chrono::milliseconds spacing = 0ms)
	{
		for(int i = 0; i < n; i++)
		{
			bb->Event(i, 0, opendnp3::EventMode::Detect);
			if(spacing > 0ms) std::this_thread::sleep_for(spacing);
		}
	}
};

// Trickle: events spaced well beyond maxBatchPeriodms — window stays 0,
// each event flushes immediately on its own.
TEST_CASE(SUITE("trickle_immediate_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(4, GENEROUS_WAIT);

	CHECK(f.os->FlushCount()   == 4);
	CHECK(f.os->TotalEvents()  == 4);
	CHECK(f.os->MaxBatchSize() == 1);

	TestTearDown();
}

// Count ceiling: burst of exactly maxBatchCount events triggers an
// immediate count-based flush before any timer fires.
TEST_CASE(SUITE("count_ceiling_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(MAX_COUNT);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	REQUIRE(f.os->FlushCount()  >= 1);
	CHECK(f.os->batches.front() == MAX_COUNT);
	CHECK(f.os->TotalEvents()   == MAX_COUNT);

	TestTearDown();
}

// Two count-ceiling flushes: 2x maxBatchCount events, each batch exactly full.
TEST_CASE(SUITE("two_count_ceiling_flushes"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(MAX_COUNT * 2);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->TotalEvents() == MAX_COUNT * 2);
	std::lock_guard<std::mutex> lock(f.os->mtx);
	for(auto b : f.os->batches)
		CHECK(b <= MAX_COUNT);

	TestTearDown();
}

// Timer flush: small burst below count ceiling at enough rate to open
// a non-zero window. Should arrive as one batch via the flush timer.
TEST_CASE(SUITE("timer_batch_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(3, 10ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->FlushCount()   == 1);
	CHECK(f.os->TotalEvents()  == 3);
	CHECK(f.os->MaxBatchSize() == 3);

	TestTearDown();
}

// Sustained high rate: EMA builds up, downstream flush count should be
// well below the number of events sent.
TEST_CASE(SUITE("sustained_rate_reduces_flush_count"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(60, 5ms); // 60 events over 300ms ≈ 200 msg/s
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->TotalEvents() == 60);
	CHECK(f.os->FlushCount()  < 30);
	CHECK(f.os->MaxBatchSize() > 1);

	TestTearDown();
}

// Silence decay: after a sustained burst the EMA builds; after silence
// longer than maxBatchPeriodms the next lone event should flush immediately.
TEST_CASE(SUITE("silence_resets_window"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(20, 5ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);
	const size_t flushesBeforeSilence = f.os->FlushCount();

	std::this_thread::sleep_for(GENEROUS_WAIT * 3); // EMA decays to zero

	f.bb->Event(0, 0, opendnp3::EventMode::Detect);
	std::this_thread::sleep_for(TICK * 3);

	CHECK(f.os->FlushCount()    == flushesBeforeSilence + 1);
	CHECK(f.os->batches.back()  == 1);

	TestTearDown();
}

// Destructor flush: pending events not yet flushed by a timer must be
// delivered when the BatchUpdateBuilder is destroyed.
TEST_CASE(SUITE("destructor_flushes_pending"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	auto os = std::make_shared<MockOutstation>();
	{
		auto bb = std::make_shared<BatchUpdateBuilder>(os, MAX_PERIOD_MS, MAX_COUNT, EMA_WEIGHT);
		bb->Event(1, 0, opendnp3::EventMode::Detect);
		bb->Event(2, 0, opendnp3::EventMode::Detect);
		std::this_thread::sleep_for(TICK); // let strand process posts
	} // destructor runs here

	CHECK(os->TotalEvents() == 2);

	TestTearDown();
}

// Zero-dt guard: two events posted with no sleep between them may share
// the same clock tick. Should not crash or produce inf/NaN in the EMA.
TEST_CASE(SUITE("zero_dt_no_crash"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get();
	ThreadPool thread_pool(4);

	Fixture f;
	f.bb->Event(1, 0, opendnp3::EventMode::Detect);
	f.bb->Event(2, 0, opendnp3::EventMode::Detect);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->TotalEvents() == 2);

	TestTearDown();
}
