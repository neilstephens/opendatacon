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
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

#define SUITE(name) "BatchUpdateBuilderTests - " name

// Default parameters. EMA tick = maxBatchPeriodms / maxBatchCount = 200 / 5 = 40ms.
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

inline void WaitFor(size_t ms)
{
	auto pIOS = odc::asio_service::Get(4);
	auto start_time = odc::msSinceEpoch();
	while((odc::msSinceEpoch() - start_time) < ms)
		pIOS->poll_one();
}

struct Fixture
{
	std::shared_ptr<MockOutstation>     os;
	std::shared_ptr<BatchUpdateBuilder> bb;

	Fixture(size_t maxPeriodMs = MAX_PERIOD_MS,
		size_t maxCount    = MAX_COUNT,
		double emaWeight   = EMA_WEIGHT)
		:     os(std::make_shared<MockOutstation>()),
		bb(std::make_shared<BatchUpdateBuilder>(os, maxPeriodMs, maxCount, emaWeight))
	{}

	void sendEvents(int n, std::chrono::milliseconds spacing = 0ms)
	{
		for(int i = 0; i < n; i++)
		{
			bb->Event(i, 0, opendnp3::EventMode::Detect);
			if(spacing > 0ms)
				WaitFor(spacing.count());
		}
	}
};

// ---------------------------------------------------------------------------
// Basic tests
// ---------------------------------------------------------------------------

// Trickle: events spaced well beyond maxBatchPeriodms — window stays 0,
// each event flushes immediately on its own.
TEST_CASE(SUITE("trickle_immediate_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
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
	auto pIOS = odc::asio_service::Get(4);
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
	auto pIOS = odc::asio_service::Get(4);
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

// Timer flush: send a warmup burst to build the EMA, then 3 rapid events
// below the count ceiling — they should batch into one timer-triggered flush.
TEST_CASE(SUITE("timer_batch_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(20, 5ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);
	const size_t warmupFlushes = f.os->FlushCount();
	const size_t warmupEvents  = f.os->TotalEvents();

	f.sendEvents(3, 5ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->FlushCount()   == warmupFlushes + 1);
	CHECK(f.os->TotalEvents()  == warmupEvents + 3);
	CHECK(f.os->batches.back() == 3);

	TestTearDown();
}

// Sustained high rate: EMA builds up, downstream flush count should be
// well below the number of events sent.
TEST_CASE(SUITE("sustained_rate_reduces_flush_count"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(60, 5ms); // 60 events over 300ms ≈ 200 msg/s
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->TotalEvents()  == 60);
	CHECK(f.os->FlushCount()   < 30);
	CHECK(f.os->MaxBatchSize() > 1);

	TestTearDown();
}

// Silence decay: after a sustained burst the EMA builds; after silence
// longer than maxBatchPeriodms the next lone event should flush immediately.
TEST_CASE(SUITE("silence_resets_window"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f;
	f.sendEvents(20, 5ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);
	const size_t flushesBeforeSilence = f.os->FlushCount();

	std::this_thread::sleep_for(GENEROUS_WAIT * 3); // EMA decays to zero

	f.bb->Event(0, 0, opendnp3::EventMode::Detect);
	std::this_thread::sleep_for(TICK * 3);

	CHECK(f.os->FlushCount()   == flushesBeforeSilence + 1);
	CHECK(f.os->batches.back() == 1);

	TestTearDown();
}

// Destructor flush: pending events not yet flushed by a timer must be
// delivered when the BatchUpdateBuilder is destroyed.
TEST_CASE(SUITE("destructor_flushes_pending"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
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
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f;
	f.bb->Event(1, 0, opendnp3::EventMode::Detect);
	f.bb->Event(2, 0, opendnp3::EventMode::Detect);
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->TotalEvents() == 2);

	TestTearDown();
}

// ---------------------------------------------------------------------------
// Advanced tests — tuning parameter behaviour
// ---------------------------------------------------------------------------

// maxBatchCount=1 disables batching entirely: every event triggers a count
// flush regardless of rate, so FlushCount always equals TotalEvents.
TEST_CASE(SUITE("max_count_one_no_batching"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f(MAX_PERIOD_MS, 1, EMA_WEIGHT);
	f.sendEvents(8, 5ms); // rapid burst — would batch heavily with maxCount > 1
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f.os->FlushCount()   == 8);
	CHECK(f.os->TotalEvents()  == 8);
	CHECK(f.os->MaxBatchSize() == 1);

	TestTearDown();
}

// Larger maxBatchCount allows larger count-triggered batches.
// At the same rapid burst rate, maxCount=10 produces a batch twice as large
// as maxCount=5 before the count ceiling fires.
TEST_CASE(SUITE("larger_max_count_larger_ceiling_batches"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f5 (MAX_PERIOD_MS, 5,  EMA_WEIGHT);
	Fixture f10(MAX_PERIOD_MS, 10, EMA_WEIGHT);

	f5.sendEvents(10);  // triggers 2 count flushes of 5
	f10.sendEvents(10); // triggers 1 count flush  of 10
	std::this_thread::sleep_for(GENEROUS_WAIT);

	CHECK(f5.os->TotalEvents()  == 10);
	CHECK(f10.os->TotalEvents() == 10);
	CHECK(f5.os->MaxBatchSize() == 5);
	CHECK(f10.os->MaxBatchSize()== 10);

	TestTearDown();
}

// Larger maxBatchPeriodms holds batches longer, trading latency for fewer packets.
// After warmup, 3 rapid events (below count ceiling) are sent to both fixtures.
// The short-period fixture flushes them well within 300ms; the long-period
// fixture has not yet flushed at 300ms but has by 700ms.
TEST_CASE(SUITE("larger_max_period_delays_flush"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	// tick_short=40ms, tick_long=100ms; both saturate after warmup
	Fixture f_short(200, MAX_COUNT, EMA_WEIGHT);
	Fixture f_long (500, MAX_COUNT, EMA_WEIGHT);

	// Warmup — use count-ceiling bursts so we don't wait on timers
	f_short.sendEvents(20, 5ms);
	f_long.sendEvents(20, 5ms);
	// f_long window is 500ms so drain needs longer than GENEROUS_WAIT
	std::this_thread::sleep_for(800ms);
	const size_t shortBase = f_short.os->TotalEvents();
	const size_t longBase  = f_long.os->TotalEvents();

	f_short.sendEvents(3);
	f_long.sendEvents(3);

	// After 300ms: short-period (200ms window) has flushed; long-period (500ms) has not
	std::this_thread::sleep_for(300ms);
	CHECK(f_short.os->TotalEvents() == shortBase + 3);
	CHECK(f_long.os->TotalEvents() == longBase); // still buffered

	// After another 400ms (700ms total): long-period has also flushed
	std::this_thread::sleep_for(400ms);
	CHECK(f_long.os->TotalEvents() == longBase + 3);

	TestTearDown();
}

// High emaWeight recovers low latency faster after a burst.
// After warmup, both fixtures see silence of 5 EMA ticks (200ms).
// With emaWeight=0.9: smoothedRate decays to ~0 (0.1^5 factor) -> window=0 -> immediate flush.
// With emaWeight=0.1: smoothedRate decays to ~59% of warm value -> window still clamped -> probe waits.
// Checked at 2 ticks (80ms) after the probe — fast weight is done, slow weight is not.
TEST_CASE(SUITE("high_ema_weight_recovers_latency_faster"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	Fixture f_fast(MAX_PERIOD_MS, MAX_COUNT, 0.9);
	Fixture f_slow(MAX_PERIOD_MS, MAX_COUNT, 0.1);

	// Saturate both EMAs
	f_fast.sendEvents(20, 5ms);
	f_slow.sendEvents(20, 5ms);
	std::this_thread::sleep_for(GENEROUS_WAIT);
	const size_t fastBase = f_fast.os->TotalEvents();
	const size_t slowBase = f_slow.os->TotalEvents();

	// Silence for 5 EMA ticks (200ms = MAX_PERIOD_MS):
	//   emaWeight=0.9 -> smoothedRate * 0.1^5 ≈ 0  -> window = 0
	//   emaWeight=0.1 -> smoothedRate * 0.9^5 ≈ 59% -> window = MAX_PERIOD_MS (clamped)
	std::this_thread::sleep_for(std::chrono::milliseconds(MAX_PERIOD_MS));

	f_fast.bb->Event(0, 0, opendnp3::EventMode::Detect);
	f_slow.bb->Event(0, 0, opendnp3::EventMode::Detect);

	// After 2 ticks (80ms): fast has flushed immediately, slow is still in its window
	std::this_thread::sleep_for(TICK * 2);
	CHECK(f_fast.os->TotalEvents() == fastBase + 1);
	CHECK(f_slow.os->TotalEvents() == slowBase); // still buffered

	// After full period (another MAX_PERIOD_MS): slow has also flushed
	std::this_thread::sleep_for(std::chrono::milliseconds(MAX_PERIOD_MS));
	CHECK(f_slow.os->TotalEvents() == slowBase + 1);

	TestTearDown();
}

// Steady-state average batch sizes
// Send steady streams at various rates below the caps
//   check that the average batch size/time matches the expected
struct steady_result
{
	size_t totalEvents;
	size_t flushCount;
	size_t maxBatchSize;
};
steady_result run_steady(size_t max_period, size_t max_count, double weight, size_t n, size_t delta)
{

	Fixture f(max_period,max_count,weight);
	f.sendEvents(n,std::chrono::milliseconds(delta));
	WaitFor(max_period);
	return {f.os->TotalEvents(),f.os->FlushCount(),f.os->MaxBatchSize()};
}

TEST_CASE(SUITE("steady_rate_batch_size"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(6);

	auto rates = {5.0, 6.0, 7.0, 9.0};
	auto mx_periods = {700, 1000, 1500};
	auto mx_counts = {7, 10, 15};
	std::vector<std::future<steady_result>> future_results;
	for(const auto& rate : rates)
		for(const auto& mx_period : mx_periods)
			for(const auto& mx_count : mx_counts)
				future_results.push_back(std::async(std::launch::async, run_steady, mx_period, mx_count, 0.2, 10*rate, 1000/rate));

	int i=0;
	for(const auto& rate : rates)
		for(const auto& mx_period : mx_periods)
			for(const auto& mx_count : mx_counts)
			{
				const auto expected_batch_period_ms = static_cast<size_t>(mx_period*(rate/mx_count));
				const auto expected_batch_size = 1 + std::floor(expected_batch_period_ms/(1000/rate));
				const auto result = future_results[i++].get();

				CAPTURE(rate);
				CAPTURE(mx_period);
				CAPTURE(mx_count);
				CHECK(result.totalEvents == 10*rate);
				if(result.maxBatchSize != mx_count)
					CHECK(result.maxBatchSize >= expected_batch_size - 1);
				CHECK(result.maxBatchSize <= expected_batch_size + 1);
			}

	TestTearDown();
}

// Higher arrival rate should produce fewer flushes for the same number of
// events — the window grows proportionally to rate, so more events batch
// together before each flush.
//
// Three rates run in parallel (maxPeriodMs=200, maxCount=10):
//   slow  : 100ms spacing (10 msg/s) — at saturation, window=200ms, ~2 events/batch
//   medium:  40ms spacing (25 msg/s) — above saturation, window=200ms, ~5 events/batch
//   fast  :  10ms spacing (100 msg/s) — count ceiling fires, batch=10 every time
//
// Expected flush ordering: slow > medium > fast
// Wall time: max(50*100ms) + GENEROUS_WAIT = 5.4s.
TEST_CASE(SUITE("parallel_higher_rate_more_batching"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	constexpr int N = 50;
	auto f_slow   = std::async(std::launch::async, run_steady, 200, 10, EMA_WEIGHT, N, 100);
	auto f_medium = std::async(std::launch::async, run_steady, 200, 10, EMA_WEIGHT, N,  40);
	auto f_fast   = std::async(std::launch::async, run_steady, 200, 10, EMA_WEIGHT, N,  10);

	const auto r_slow   = f_slow.get();
	const auto r_medium = f_medium.get();
	const auto r_fast   = f_fast.get();

	CHECK(r_slow.totalEvents == N);
	CHECK(r_medium.totalEvents == N);
	CHECK(r_fast.totalEvents == N);

	// Higher rate → more batching → fewer flushes
	CHECK(r_slow.flushCount   > r_medium.flushCount);
	CHECK(r_medium.flushCount > r_fast.flushCount);

	// Fast rate hits count ceiling
	CHECK(r_fast.maxBatchSize == 10);

	TestTearDown();
}

// Higher emaWeight adapts faster: with a warm EMA the window opens sooner
// after a cold start, so more of the N events are batched → fewer total
// flushes for the same event stream.
//
// Two weights run in parallel at the same rate (30ms, ~33 msg/s):
//   weight=0.9 reaches full window after ~1 EMA tick (20ms)
//   weight=0.1 reaches full window after ~5 EMA ticks (100ms)
// With N=20 events the warm-up difference is visible in flushCount.
// Wall time: 20*30ms + GENEROUS_WAIT ≈ 1s.
TEST_CASE(SUITE("parallel_higher_ema_weight_faster_adaptation"))
{
	TestSetup();
	auto pIOS = odc::asio_service::Get(4);
	ThreadPool thread_pool(4);

	constexpr int N = 20;
	auto f_fast_w = std::async(std::launch::async, run_steady, 200, 10, 0.9, N, 30);
	auto f_slow_w = std::async(std::launch::async, run_steady, 200, 10, 0.1, N, 30);

	const auto r_fast_w = f_fast_w.get();
	const auto r_slow_w = f_slow_w.get();

	CHECK(r_fast_w.totalEvents == N);
	CHECK(r_slow_w.totalEvents == N);

	// Faster-adapting weight batches more → fewer flushes
	CHECK(r_fast_w.flushCount < r_slow_w.flushCount);

	TestTearDown();
}
