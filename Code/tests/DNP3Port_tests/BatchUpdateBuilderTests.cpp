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
 *
 * Design notes
 * ============
 * BatchUpdateBuilder has two orthogonal flush triggers:
 *
 *   Count ceiling  – when batchCount reaches maxBatchCount the batch is
 *                    flushed immediately, regardless of timing.
 *
 *   Timer          – when the first event of a new batch arrives, a
 *                    one-shot timer is armed for BatchPeriodms().
 *                    BatchPeriodms() = min(smoothedRate/maxBatchCount
 *                                         * maxBatchPeriodMs,
 *                                         maxBatchPeriodMs)
 *                    With a cold EMA this is 0 ms (flush immediately).
 *                    With a warm EMA it grows proportionally to rate.
 *
 * The EMA is updated by a periodic "rate timer" that fires every
 * maxBatchPeriodMs/maxBatchCount (= TICK) milliseconds while events are
 * arriving, and self-cancels when the window falls to 0.
 *
 * Testing strategy
 * ================
 * Rather than sleeping for "generous" fixed intervals and hoping the
 * system catches up, every test uses:
 *
 *   1. MockOutstation::WaitForFlushes() – a condition-variable based
 *      barrier that returns as soon as N flushes have been recorded
 *      (or a generous CI timeout elapses).  This makes tests run as
 *      fast as the implementation delivers them, with no artificial
 *      floor, while still failing cleanly if something is broken.
 *
 *   2. BatchUpdateBuilder::Sync() – drains the internal strand so
 *      tests can safely inspect counts that live there.
 *
 *   3. BatchUpdateBuilder::CurrentBatchWindowMs() – strand-safe read of
 *      the computed batch window so tests can confirm the EMA state.
 *
 *   4. BatchUpdateBuilderTest::CancelFlushTimerSync() – test-only friend
 *      helper that cancels the flush timer from within the strand,
 *      enabling deterministic testing of the destructor flush path.
 *
 *   5. BatchUpdateBuilderTest::GetBatchCount() – strand-safe read of
 *      batchCount, used inside WarmEMA to assert no partial batch pending.
 *
 * Timing assertions are expressed as measured elapsed intervals with
 * generous tolerance bounds (CI_SLACK_MS covers slow CI machines).
 * Negative assertions ("this has NOT happened yet") are replaced by
 * positive ordering assertions ("A happened before B") wherever
 * possible.
 *
 * Stack discipline
 * ================
 * Within each TEST_CASE, variables are declared in this order so that
 * C++ LIFO destruction gives the correct teardown sequence:
 *
 *   1. TestLifecycle  (RAII wrapper, destroyed last → TestTearDown()
 *                      runs after everything else, even on REQUIRE fail)
 *   2. ThreadPool     (destroyed second-to-last → drains asio and joins
 *                      threads before the logger is dropped)
 *   3. Fixtures / BBs (destroyed first → BB destructors run while the
 *                      asio thread pool is still alive)
 */

#include "../Code/Ports/DNP3Port/BatchUpdateBuilder.h"
#include "TestDNP3Helpers.h"
#include "../ThreadPool.h"
#include <catch.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using ms_t = std::chrono::milliseconds;

#define SUITE(name) "BatchUpdateBuilderTests - " name

// ---------------------------------------------------------------------------
// Test parameters
// ---------------------------------------------------------------------------

// Default BatchUpdateBuilder parameters
static constexpr size_t MAX_PERIOD_MS = 200;
static constexpr size_t MAX_COUNT     = 5;
static constexpr double EMA_WEIGHT    = 0.5;

// EMA sampling interval (rate timer period)
static constexpr auto TICK = ms_t(MAX_PERIOD_MS / MAX_COUNT); // 40 ms

// How long to wait for an expected flush before declaring the test hung.
// 10 * MAX_PERIOD_MS is generous enough for any reasonable CI machine.
static constexpr auto FLUSH_TIMEOUT = ms_t(10 * MAX_PERIOD_MS); // 2 000 ms

// Additional slack added to measured-latency upper bounds so that
// transiently slow schedulers don't produce false failures.
static constexpr auto CI_SLACK = ms_t(5 * MAX_PERIOD_MS); // 1 000 ms

// ---------------------------------------------------------------------------
// RAII test lifecycle wrapper
// ---------------------------------------------------------------------------
// Declaring this FIRST in each TEST_CASE means its destructor runs LAST,
// after the ThreadPool has drained and all fixtures have been torn down.
// This guarantees TestTearDown() is called even when REQUIRE fails.
struct TestLifecycle
{
	TestLifecycle()  { TestSetup(); }
	~TestLifecycle() { TestTearDown(); }
};

// ---------------------------------------------------------------------------
// MockOutstation
// ---------------------------------------------------------------------------

// Records every Apply() call with its batch size and wall-clock arrival time.
// WaitForFlushes() blocks the calling thread until at least N flushes have
// been recorded, or the timeout elapses.
class MockOutstation: public opendnp3::IOutstation
{
public:
	struct FlushRecord
	{
		size_t batchSize;
		std::chrono::steady_clock::time_point arrivedAt;
	};

	mutable std::mutex mtx;
	mutable std::condition_variable cv;
	std::vector<FlushRecord> records;

	void Apply(const opendnp3::Updates& updates) override
	{
		std::lock_guard lock(mtx);
		records.push_back({updates.size(), std::chrono::steady_clock::now()});
		cv.notify_all();
	}

	// Block until at least n flushes have been recorded.
	// Returns true if the condition was met within the timeout.
	bool WaitForFlushes(size_t n, ms_t timeout = FLUSH_TIMEOUT) const
	{
		std::unique_lock lock(mtx);
		return cv.wait_for(lock, timeout, [&]{ return records.size() >= n; });
	}

	// Snapshot accessors (call after WaitForFlushes / Sync to avoid races)
	size_t FlushCount()   const { std::lock_guard lock(mtx); return records.size(); }
	size_t TotalEvents()  const
	{
		std::lock_guard lock(mtx);
		size_t n = 0;
		for(auto& r : records) n += r.batchSize;
		return n;
	}
	size_t MaxBatchSize() const
	{
		std::lock_guard lock(mtx);
		size_t m = 0;
		for(auto& r : records) m = std::max(m, r.batchSize);
		return m;
	}
	// Returns batch sizes in order (copy, so safe to inspect after the lock)
	std::vector<size_t> BatchSizes() const
	{
		std::lock_guard lock(mtx);
		std::vector<size_t> v;
		v.reserve(records.size());
		for(auto& r : records) v.push_back(r.batchSize);
		return v;
	}
};

// ---------------------------------------------------------------------------
// BatchUpdateBuilderTest – friend accessor for test-only introspection
// ---------------------------------------------------------------------------
// The BatchUpdateBuilder header grants this struct friend access so that
// tests can manipulate private state that cannot be reached via the public
// API (e.g., cancelling the flush timer to force the destructor path).
// This struct exists only in the test binary.
struct BatchUpdateBuilderTest
{
	// Cancel the flush timer from within the strand and block until done.
	// Increments flushSeq first so the (already-queued) aborted callback
	// will see seq != flushSeq and will not call Flush() itself.
	// After this returns, the destructor's Flush() is the only remaining
	// delivery mechanism.
	static void CancelFlushTimerSync(BatchUpdateBuilder& bb)
	{
		std::promise<void> p;
		bb.pSyncStrand->post([&bb, &p]
			{
				bb.flushSeq++; // invalidate any pending timer callback
				bb.pFlushTimer->cancel();
				p.set_value();
			});
		p.get_future().wait();
	}

	// Read batchCount from within the strand and return it.
	// Use after a count-ceiling flush to assert no partial batch is pending.
	static size_t GetBatchCount(const BatchUpdateBuilder& bb)
	{
		std::promise<size_t> p;
		bb.pSyncStrand->post([&bb, &p]{ p.set_value(bb.batchCount); });
		return p.get_future().get();
	}

	// Block until all work previously posted to the internal strand has
	// completed. Equivalent to the old public Sync() on BatchUpdateBuilder.
	static void Sync(const BatchUpdateBuilder& bb)
	{
		std::promise<void> p;
		bb.pSyncStrand->post([&p]{ p.set_value(); });
		p.get_future().wait();
	}

	// Strand-safe read of BatchPeriodms() (the current EMA-derived batch
	// window in milliseconds). Returns 0 when the EMA is cold/decayed.
	static size_t CurrentBatchWindowMs(const BatchUpdateBuilder& bb)
	{
		std::promise<size_t> p;
		bb.pSyncStrand->post([&bb, &p]{ p.set_value(bb.BatchPeriodms()); });
		return p.get_future().get();
	}
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

struct Fixture
{
	std::shared_ptr<MockOutstation>     os;
	std::shared_ptr<BatchUpdateBuilder> bb;

	Fixture(size_t maxPeriodMs = MAX_PERIOD_MS,
		size_t maxCount    = MAX_COUNT,
		double emaWeight   = EMA_WEIGHT)
		: os(std::make_shared<MockOutstation>()),
		bb(std::make_shared<BatchUpdateBuilder>(os, maxPeriodMs, maxCount, emaWeight))
	{}

	// Send n events spaced by the given interval.
	// Spacing is honoured by sleeping the calling thread, which is fine
	// since the strand runs in the ThreadPool's background threads.
	void SendEvents(size_t n, ms_t spacing = 0ms)
	{
		for(size_t i = 0; i < n; i++)
		{
			bb->Event(static_cast<int>(i), 0, opendnp3::EventMode::Detect);
			if(spacing > 0ms)
				std::this_thread::sleep_for(spacing);
		}
	}

	// Convenience wrappers so tests can write f.Sync() / f.CurrentBatchWindowMs()
	// instead of spelling out BatchUpdateBuilderTest:: every time.
	void   Sync()                const { BatchUpdateBuilderTest::Sync(*bb); }
	size_t CurrentBatchWindowMs() const { return BatchUpdateBuilderTest::CurrentBatchWindowMs(*bb); }

	// Warm the EMA by sending count-ceiling-sized rapid bursts separated by
	// one TICK of silence.  Each burst triggers a count-ceiling flush
	// (batchCount → 0, flushSeq++, flush timer cancelled), leaving no
	// partial batch or stray timer pending after WarmEMA returns.
	// The rate timer fires during each inter-burst sleep and integrates
	// the (very high) instantRate into smoothedArrivalRate, driving
	// BatchPeriodms() positive after the first or second burst.
	//
	// Invariants on return:
	//   • CurrentBatchWindowMs() > 0
	//   • batchCount == 0  (no pending events in pBuilder)
	//   • flush timer is not armed  (last burst was count-ceiling)
	//
	// Returns the flush count after Sync() for use as a stable baseline.
	size_t WarmEMA(size_t maxPeriodMs = MAX_PERIOD_MS,
		size_t maxCount    = MAX_COUNT)
	{
		// TICK: EMA sampling interval — sleep this long between bursts so
		// the rate timer has time to fire and update smoothedArrivalRate.
		const auto tick = ms_t(static_cast<long long>(maxPeriodMs / maxCount));
		const auto warmupTimeout = ms_t(maxPeriodMs * 10) + CI_SLACK;

		const size_t maxBursts = 20; // bounded to fail fast on broken implementation
		for(size_t burst = 0; burst < maxBursts; burst++)
		{
			const size_t flushBefore = os->FlushCount();
			SendEvents(maxCount); // rapid — no spacing → count-ceiling flush guaranteed
			REQUIRE(os->WaitForFlushes(flushBefore + 1, warmupTimeout));

			// After a count-ceiling flush the strand invariant holds:
			// batchCount==0, flushSeq incremented, flush timer cancelled.
			REQUIRE(BatchUpdateBuilderTest::GetBatchCount(*bb) == 0);

			// Sleep one TICK so the rate timer fires and updates smoothedArrivalRate.
			std::this_thread::sleep_for(tick + ms_t(10));

			if(CurrentBatchWindowMs() > 0)
				break;
		}

		Sync();
		REQUIRE(CurrentBatchWindowMs() > 0);
		return os->FlushCount();
	}
};

// ===========================================================================
//  Group 1 – Event delivery correctness
//  The most fundamental invariant: every event posted reaches the outstation
//  exactly once, regardless of which flush path delivers it.
// ===========================================================================

// Send N events in rapid bursts (triggering only count-ceiling flushes
// when maxBatchCount=1).  Verify exact event count and no duplication.
TEST_CASE(SUITE("AllEventsDelivered_CountCeilingOnly"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	// maxBatchCount=1 means every event is a count-ceiling flush;
	// no timer involvement, completely deterministic.
	Fixture f(MAX_PERIOD_MS, 1, EMA_WEIGHT);

	constexpr size_t N = 50;
	f.SendEvents(N);

	REQUIRE(f.os->WaitForFlushes(N, FLUSH_TIMEOUT));
	f.Sync();

	CHECK(f.os->TotalEvents()  == N);
	CHECK(f.os->FlushCount()   == N);
	CHECK(f.os->MaxBatchSize() == 1);
}

// Send a large burst that spans many count-ceiling flushes.
// Every event must be accounted for and no batch must exceed the ceiling.
TEST_CASE(SUITE("AllEventsDelivered_ManyCeilingFlushes"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;

	// 4 complete ceiling-sized bursts plus a partial remainder
	constexpr size_t N = MAX_COUNT * 4 + 2;
	f.SendEvents(N);

	// Wait for at least the 4 count flushes
	REQUIRE(f.os->WaitForFlushes(4, FLUSH_TIMEOUT));
	// Then wait for the remainder (timer-flushed), up to 1 extra flush
	REQUIRE(f.os->WaitForFlushes(5, ms_t(MAX_PERIOD_MS) + CI_SLACK));
	f.Sync();

	CHECK(f.os->TotalEvents() == N);
	for(auto bs : f.os->BatchSizes())
		CHECK(bs <= MAX_COUNT);
}

// Send events slowly (cold EMA → 0 ms window → immediate timer flush per event).
// Every event must produce exactly one flush.
TEST_CASE(SUITE("AllEventsDelivered_TrickleRate"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;

	// Space events well beyond the EMA silence threshold so the window
	// stays at zero between events.
	constexpr size_t N = 5;
	f.SendEvents(N, ms_t(MAX_PERIOD_MS * 2));

	REQUIRE(f.os->WaitForFlushes(N, ms_t(N * MAX_PERIOD_MS * 3)));
	f.Sync();

	CHECK(f.os->TotalEvents() == N);
	CHECK(f.os->FlushCount()  == N);
}

// Verify the all-events-delivered invariant under a mixed workload:
// high-rate bursts interleaved with pauses.
TEST_CASE(SUITE("AllEventsDelivered_MixedLoad"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;

	constexpr size_t BURST   = MAX_COUNT * 3;
	constexpr size_t TRICKLE = 3;

	// Burst 1 (cold start – mostly count-ceiling flushes)
	f.SendEvents(BURST);
	REQUIRE(f.os->WaitForFlushes(1, FLUSH_TIMEOUT));

	// Pause to let EMA decay
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS * 4));
	f.Sync();

	// Trickle with cold EMA: each event should flush promptly (window = 0)
	// Capture the baseline BEFORE sending so we know how many to wait for
	const size_t baseTrickle = f.os->FlushCount();
	f.SendEvents(TRICKLE, ms_t(MAX_PERIOD_MS * 3));
	// Total time for trickle = TRICKLE * MAX_PERIOD_MS * 3; give generous extra
	REQUIRE(f.os->WaitForFlushes(baseTrickle + TRICKLE,
		ms_t(static_cast<long long>(TRICKLE) * MAX_PERIOD_MS * 5)));

	// Burst 2 (warm-ish EMA by now)
	const size_t beforeBurst2 = f.os->FlushCount();
	f.SendEvents(BURST);
	REQUIRE(f.os->WaitForFlushes(beforeBurst2 + 1, FLUSH_TIMEOUT));

	// Wait for any residual timer flush
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);
	f.Sync();

	CHECK(f.os->TotalEvents() == BURST + TRICKLE + BURST);
	for(auto bs : f.os->BatchSizes())
		CHECK(bs <= MAX_COUNT);
}

// ===========================================================================
//  Group 2 – Count-ceiling flush behaviour
// ===========================================================================

// When exactly maxBatchCount events arrive in rapid succession (with a warm
// EMA so the timer is slow), they must be delivered in a single flush of
// exactly maxBatchCount before the timer period elapses.
TEST_CASE(SUITE("CountCeiling_SingleFlushOfExactSize"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseTotalEvents = f.os->TotalEvents();
	const size_t baseFlushes     = f.os->FlushCount();

	const auto t_send = std::chrono::steady_clock::now();
	f.SendEvents(MAX_COUNT); // exactly the ceiling

	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, FLUSH_TIMEOUT));
	const auto elapsed = std::chrono::duration_cast<ms_t>(
		std::chrono::steady_clock::now() - t_send);
	f.Sync();

	// All MAX_COUNT events arrived in one batch
	CHECK(f.os->TotalEvents() == baseTotalEvents + MAX_COUNT);
	CHECK(f.os->BatchSizes().back() == MAX_COUNT);

	// The ceiling fires before the timer period (which is MAX_PERIOD_MS when warm)
	const auto window = ms_t(static_cast<long long>(f.CurrentBatchWindowMs()));
	CAPTURE(elapsed.count(), window.count());
	CHECK(elapsed < window + CI_SLACK);
}

// After a count-ceiling flush the stale-flush guard (flushSeq) must prevent
// the timer that was armed for the first event of that batch from issuing a
// second, empty flush.
TEST_CASE(SUITE("CountCeiling_NoStaleTimerFlushAfterCeiling"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseFlushes = f.os->FlushCount();

	// Send exactly one ceiling-worth of events
	f.SendEvents(MAX_COUNT);
	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, FLUSH_TIMEOUT));

	// Wait well beyond the batch window; the stale timer must NOT produce
	// a second flush.
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);
	f.Sync();

	// Still exactly one new flush (the count-ceiling one)
	CHECK(f.os->FlushCount() == baseFlushes + 1);
}

// Multiple back-to-back ceiling-sized bursts: every event lands in exactly
// one flush, no event is duplicated or dropped.
TEST_CASE(SUITE("CountCeiling_MultipleConsecutiveBursts"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseTotalEvents = f.os->TotalEvents();
	const size_t baseFlushes     = f.os->FlushCount();

	constexpr size_t BURSTS = 6;
	for(size_t b = 0; b < BURSTS; b++)
	{
		const size_t before = f.os->FlushCount();
		f.SendEvents(MAX_COUNT);
		REQUIRE(f.os->WaitForFlushes(before + 1, FLUSH_TIMEOUT));
	}
	f.Sync();

	CHECK(f.os->TotalEvents() == baseTotalEvents + BURSTS * MAX_COUNT);
	CHECK(f.os->FlushCount()  == baseFlushes     + BURSTS);
	for(auto bs : f.os->BatchSizes())
		CHECK(bs <= MAX_COUNT);
}

// ===========================================================================
//  Group 3 – Timer flush and cold-EMA behaviour
// ===========================================================================

// With a cold EMA (no prior activity) the batch window is 0 ms.
// A single event should therefore produce a flush almost immediately
// (within CI_SLACK).
TEST_CASE(SUITE("Timer_ColdEmaFlushesImmediately"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;

	const auto t_send = std::chrono::steady_clock::now();
	f.bb->Event(0, 0, opendnp3::EventMode::Detect);

	REQUIRE(f.os->WaitForFlushes(1, CI_SLACK));
	const auto elapsed = std::chrono::steady_clock::now() - t_send;
	f.Sync();

	CHECK(f.os->TotalEvents()        == 1);
	CHECK(f.os->FlushCount()         == 1);
	CHECK(f.os->BatchSizes().front() == 1);
	INFO("Cold-EMA flush latency: " << std::chrono::duration_cast<ms_t>(elapsed).count() << " ms");
	CHECK(elapsed < CI_SLACK);
}

// With a warm EMA (window = maxBatchPeriodMs), a batch of events smaller
// than the ceiling should be held for roughly the batch window and then
// flushed by the timer, not the ceiling.
TEST_CASE(SUITE("Timer_WarmEmaHoldsBatchForWindow"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseFlushes     = f.os->FlushCount();
	const size_t baseTotalEvents = f.os->TotalEvents();

	// Send fewer events than the ceiling so only the timer can flush them
	const size_t FEW = MAX_COUNT - 1;
	const auto t_send = std::chrono::steady_clock::now();
	f.SendEvents(FEW);

	// Wait for the timer flush
	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, ms_t(MAX_PERIOD_MS) + CI_SLACK));
	const auto elapsed_ms =
		std::chrono::duration_cast<ms_t>(std::chrono::steady_clock::now() - t_send).count();
	f.Sync();

	const size_t window = f.CurrentBatchWindowMs();
	CAPTURE(window, elapsed_ms);

	// All FEW events arrived in one batch
	CHECK(f.os->TotalEvents() == baseTotalEvents + FEW);
	CHECK(f.os->BatchSizes().back() == FEW);
	// Flush latency is bounded above by the window plus generous CI slack
	CHECK(elapsed_ms < static_cast<long long>(window + CI_SLACK.count()));
}

// Confirm that the timer flush delivers the events in a single batch
// (not split across multiple calls to Apply).
TEST_CASE(SUITE("Timer_BatchArrivesAsOneApplyCall"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseFlushes     = f.os->FlushCount();
	const size_t baseTotalEvents = f.os->TotalEvents();

	// Send a few events (below ceiling) with tiny spacing to ensure they
	// share the same batch timer
	constexpr size_t FEW = 3;
	f.SendEvents(FEW, 2ms);

	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, ms_t(MAX_PERIOD_MS) + CI_SLACK));
	f.Sync();

	// All FEW events in a single Apply call
	CHECK(f.os->TotalEvents() == baseTotalEvents + FEW);
	CHECK(f.os->BatchSizes().back() == FEW);
}

// ===========================================================================
//  Group 4 – EMA window adaptation
// ===========================================================================

// Higher event rate → larger smoothedArrivalRate → larger window → the same
// number of events produces fewer, larger flushes.
// Two fixtures run at different rates; the faster one must produce fewer flushes.
TEST_CASE(SUITE("EMA_HigherRateFewerFlushes"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f_slow, f_fast;

	// Slow: 2× TICK between events; Fast: TICK/4 between events.
	// Both send the same total number of events.
	constexpr size_t N = MAX_COUNT * 10;
	f_slow.SendEvents(N, TICK * 2);
	f_fast.SendEvents(N, TICK / 4);

	// Wait for all events to be delivered on both sides
	const auto timeout = ms_t(static_cast<long long>(N) * TICK.count() * 2 + CI_SLACK.count());
	REQUIRE(f_slow.os->WaitForFlushes(1, timeout));
	REQUIRE(f_fast.os->WaitForFlushes(1, timeout));

	// Drain any remaining timer flush
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);
	f_slow.Sync();
	f_fast.Sync();

	REQUIRE(f_slow.os->TotalEvents() == N);
	REQUIRE(f_fast.os->TotalEvents() == N);

	// The faster stream must batch more aggressively (fewer flushes)
	CAPTURE(f_slow.os->FlushCount(), f_fast.os->FlushCount());
	CHECK(f_fast.os->FlushCount() < f_slow.os->FlushCount());
}

// After sustained-rate events warm the EMA, silence longer than
// maxBatchPeriodMs causes the EMA to decay to zero.
// Verified by checking that CurrentBatchWindowMs() returns 0 after silence.
TEST_CASE(SUITE("EMA_DecaysToZeroAfterSilence"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();

	// Confirm the window is non-zero after warm-up
	REQUIRE(f.CurrentBatchWindowMs() > 0);

	// Silence for several EMA ticks (each tick at most zeroes instantRate
	// once; after maxBatchPeriodMs all ticks that can fire have fired)
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS * 4) + CI_SLACK);

	// Window should now be zero
	CHECK(f.CurrentBatchWindowMs() == 0);
}

// After EMA decays to zero, the next event must flush almost immediately
// (batch window = 0, so timer fires with 0 ms delay).
TEST_CASE(SUITE("EMA_SingleEventFlushesImmediatelyAfterSilence"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();
	const size_t baseFlushes = f.os->FlushCount();

	// Wait for EMA to decay
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS * 4) + CI_SLACK);
	REQUIRE(f.CurrentBatchWindowMs() == 0);

	// Now send a single event and measure how long until it flushes
	const auto t_send = std::chrono::steady_clock::now();
	f.bb->Event(0, 0, opendnp3::EventMode::Detect);

	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, CI_SLACK));
	const auto elapsed = std::chrono::steady_clock::now() - t_send;
	INFO("Post-silence flush latency: " << std::chrono::duration_cast<ms_t>(elapsed).count() << " ms");

	// Should flush much faster than a full batch window
	CHECK(elapsed < CI_SLACK);
}

// Larger maxBatchPeriodMs → larger window → flush latency is proportionally
// longer.  We verify this by measuring the actual latency of each fixture
// and asserting a strict ordering.
TEST_CASE(SUITE("EMA_LargerMaxPeriodGivesLongerLatency"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	// Two fixtures: short period = 200 ms, long period = 600 ms.
	// maxBatchCount and emaWeight are identical so the EMA adapts at the
	// same relative rate; only the absolute period differs.
	constexpr size_t SHORT_PERIOD = 200;
	constexpr size_t LONG_PERIOD  = 600;

	Fixture f_short(SHORT_PERIOD, MAX_COUNT, EMA_WEIGHT);
	Fixture f_long (LONG_PERIOD,  MAX_COUNT, EMA_WEIGHT);

	// Warm both EMAs independently
	f_short.WarmEMA(SHORT_PERIOD);
	f_long.WarmEMA (LONG_PERIOD);

	const size_t baseShort       = f_short.os->FlushCount();
	const size_t baseLong        = f_long.os->FlushCount();
	const size_t baseTotalShort  = f_short.os->TotalEvents();
	const size_t baseTotalLong   = f_long.os->TotalEvents();

	// Send the same sub-ceiling probe to both
	const size_t FEW = MAX_COUNT - 1;

	const auto t_send = std::chrono::steady_clock::now();
	f_short.SendEvents(FEW);
	f_long.SendEvents (FEW);

	// Wait for both flushes with per-fixture generous timeouts
	REQUIRE(f_short.os->WaitForFlushes(baseShort + 1,
		ms_t(static_cast<long long>(SHORT_PERIOD + CI_SLACK.count()))));
	const auto short_elapsed = std::chrono::duration_cast<ms_t>(
		std::chrono::steady_clock::now() - t_send);

	REQUIRE(f_long.os->WaitForFlushes(baseLong + 1,
		ms_t(static_cast<long long>(LONG_PERIOD + CI_SLACK.count()))));
	const auto long_elapsed = std::chrono::duration_cast<ms_t>(
		std::chrono::steady_clock::now() - t_send);

	CAPTURE(short_elapsed.count(), long_elapsed.count());

	// Correctness: all events delivered
	CHECK(f_short.os->TotalEvents() == baseTotalShort + FEW);
	CHECK(f_long.os->TotalEvents()  == baseTotalLong  + FEW);

	// The probe on the longer-period fixture must take longer to flush
	CHECK(long_elapsed > short_elapsed);

	// Upper bounds: each must flush within its window plus CI slack
	CHECK(short_elapsed.count() < static_cast<long long>(SHORT_PERIOD + CI_SLACK.count()));
	CHECK(long_elapsed.count()  < static_cast<long long>(LONG_PERIOD  + CI_SLACK.count()));
}

// Higher emaWeight adapts faster.
// After warming both fixtures, silence for 5 EMA ticks.
// The fast-weight fixture (0.9) decays its smoothedRate to near-zero in
// ~5 ticks (0.1^5 ≈ 0); the slow-weight fixture (0.1) retains ~59%
// of its warm value (0.9^5) → window still positive.
// Verified via CurrentBatchWindowMs() rather than indirect timing.
TEST_CASE(SUITE("EMA_HigherWeightDecaysFaster"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f_fast(MAX_PERIOD_MS, MAX_COUNT, 0.9);
	Fixture f_slow(MAX_PERIOD_MS, MAX_COUNT, 0.1);

	f_fast.WarmEMA();
	f_slow.WarmEMA();

	REQUIRE(f_fast.CurrentBatchWindowMs() > 0);
	REQUIRE(f_slow.CurrentBatchWindowMs() > 0);

	// Silence for exactly 5 EMA ticks (= MAX_PERIOD_MS) plus generous slack.
	// weight=0.9: smoothedRate * 0.1^5 ≈ 0    → window decays to 0
	// weight=0.1: smoothedRate * 0.9^5 ≈ 59%  → window remains positive
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);

	const size_t fastWindow = f_fast.CurrentBatchWindowMs();
	const size_t slowWindow = f_slow.CurrentBatchWindowMs();
	CAPTURE(fastWindow, slowWindow);

	CHECK(fastWindow == 0); // fast weight has decayed to zero
	CHECK(slowWindow > 0);  // slow weight still retains history
}

// Higher emaWeight adapts faster.
// After sending a single event to each fixture, the EMA machinery starts.
// The constructor primes lastArrivalTime to (now - maxBatchPeriodms), so
// instantRate for the first event = 1/(maxBatchPeriodms_s) = 5 Hz.
// After one rate-timer tick (TICK ms) the EMA is updated:
//   fast (w=0.9): smoothedRate = 0.9 * 5 = 4.5  → window = 4.5/5 * 200 = 180 ms
//   slow (w=0.1): smoothedRate = 0.1 * 5 = 0.5  → window = 0.5/5 * 200 =  20 ms
// The fast-weight fixture must have a strictly larger window than slow.
TEST_CASE(SUITE("EMA_HigherWeightWarmsUpFaster"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f_fast(MAX_PERIOD_MS, MAX_COUNT, 0.9);
	Fixture f_slow(MAX_PERIOD_MS, MAX_COUNT, 0.1);

	// Seed the EMA machinery with one event on each fixture.
	// With a cold EMA the 0-ms timer fires almost immediately.
	f_fast.bb->Event(0, 0, opendnp3::EventMode::Detect);
	f_slow.bb->Event(0, 0, opendnp3::EventMode::Detect);
	REQUIRE(f_fast.os->WaitForFlushes(1, FLUSH_TIMEOUT));
	REQUIRE(f_slow.os->WaitForFlushes(1, FLUSH_TIMEOUT));
	f_fast.Sync();
	f_slow.Sync();

	// Sleep slightly longer than one TICK so the rate timer fires exactly
	// once on each fixture, incorporating instantRate into smoothedRate.
	// The second rate timer fires at 2*TICK; with 10ms margin we read the
	// window strictly between the first and second ticks.
	std::this_thread::sleep_for(TICK + ms_t(10));

	const size_t fastWindow = f_fast.CurrentBatchWindowMs();
	const size_t slowWindow = f_slow.CurrentBatchWindowMs();
	CAPTURE(fastWindow, slowWindow);

	// Higher weight converges to instantRate faster after one EMA tick.
	CHECK(fastWindow > slowWindow);
	CHECK(fastWindow > 0);
	CHECK(slowWindow > 0); // slow weight still responds, just less aggressively
}

// The batch window can never exceed maxBatchPeriodMs, regardless of arrival rate.
TEST_CASE(SUITE("EMA_WindowCappedAtMaxBatchPeriod"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	// Drive at a very high rate to push the EMA as high as possible
	f.WarmEMA(MAX_PERIOD_MS, MAX_COUNT);

	const size_t window = f.CurrentBatchWindowMs();
	CAPTURE(window, MAX_PERIOD_MS);
	CHECK(window <= MAX_PERIOD_MS);
}

// ===========================================================================
//  Group 5 – Parameter boundary behaviour
// ===========================================================================

// maxBatchCount = 1: the count ceiling fires for every single event,
// so no timer-based batching can ever occur.
TEST_CASE(SUITE("Params_MaxCountOneMeansNoTimer"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f(MAX_PERIOD_MS, 1, EMA_WEIGHT);

	constexpr size_t N = 20;
	f.SendEvents(N, 5ms); // high rate — would batch heavily with maxCount > 1

	REQUIRE(f.os->WaitForFlushes(N, ms_t(static_cast<long long>(N) * 5 + CI_SLACK.count())));
	f.Sync();

	CHECK(f.os->TotalEvents()  == N);
	CHECK(f.os->FlushCount()   == N);
	CHECK(f.os->MaxBatchSize() == 1);
}

// Larger maxBatchCount allows larger count-ceiling batches.
// A burst of 2×10 events with maxCount=10 should produce
// fewer, larger flushes than the same events with maxCount=5.
TEST_CASE(SUITE("Params_LargerMaxCountAllowsLargerBatches"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f5 (MAX_PERIOD_MS, 5,  EMA_WEIGHT);
	Fixture f10(MAX_PERIOD_MS, 10, EMA_WEIGHT);

	f5.WarmEMA (MAX_PERIOD_MS, 5);
	f10.WarmEMA(MAX_PERIOD_MS, 10);

	const size_t base5       = f5.os->FlushCount();
	const size_t base10      = f10.os->FlushCount();
	const size_t baseTotal5  = f5.os->TotalEvents();
	const size_t baseTotal10 = f10.os->TotalEvents();

	// Send enough events for 2 count flushes at each ceiling
	f5.SendEvents (10); // 2 flushes of 5
	f10.SendEvents(10); // 1 flush of 10

	REQUIRE(f5.os->WaitForFlushes (base5  + 2, FLUSH_TIMEOUT));
	REQUIRE(f10.os->WaitForFlushes(base10 + 1, FLUSH_TIMEOUT));
	f5.Sync();
	f10.Sync();

	CHECK(f5.os->TotalEvents()  == baseTotal5  + 10);
	CHECK(f10.os->TotalEvents() == baseTotal10 + 10);
	// f10 batches the same events into fewer, larger flushes
	CHECK(f10.os->FlushCount() - base10 < f5.os->FlushCount() - base5);
}

// ===========================================================================
//  Group 6 – Edge cases and defensive behaviour
// ===========================================================================

// Two events posted with effectively zero inter-arrival time (same timestamp
// tick) should not crash, produce NaN/Inf in the EMA, or lose events.
TEST_CASE(SUITE("Edge_ZeroDtDoesNotCrash"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;

	// Post both events before the strand gets a chance to process either,
	// so they share the same arrival-time sample on the strand.
	f.bb->Event(0, 0, opendnp3::EventMode::Detect);
	f.bb->Event(1, 0, opendnp3::EventMode::Detect);

	// All events must eventually be delivered
	REQUIRE(f.os->WaitForFlushes(1, FLUSH_TIMEOUT));
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);
	f.Sync();

	CHECK(f.os->TotalEvents() == 2);
}

// If the IOutstation is destroyed before the BatchUpdateBuilder, Apply()
// must not be called (weak_ptr returns nullptr).  The BatchUpdateBuilder
// itself must not crash.
TEST_CASE(SUITE("Edge_OutstationDestroyedBeforeBB"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	auto os = std::make_shared<MockOutstation>();
	auto bb = std::make_shared<BatchUpdateBuilder>(os, MAX_PERIOD_MS, MAX_COUNT, EMA_WEIGHT);

	// Destroy the outstation while events are in-flight
	os.reset();

	// Sending events after the outstation is gone must not crash
	bb->Event(0, 0, opendnp3::EventMode::Detect);
	bb->Event(1, 0, opendnp3::EventMode::Detect);

	// Drain and destroy BB without crashing
	std::this_thread::sleep_for(ms_t(MAX_PERIOD_MS) + CI_SLACK);
	bb.reset(); // destructor should not crash

	// No assertions beyond "we got here without a crash"
}

// The destructor must flush any events that are buffered in the UpdateBuilder
// but have not yet been delivered (i.e. the timer has not fired).
//
// To guarantee the timer has NOT fired when the destructor runs:
//   1. Warm the EMA so BatchPeriodms() = MAX_PERIOD_MS (timer is slow).
//   2. Send a sub-ceiling batch of events and Sync() to ensure they are
//      in pBuilder.
//   3. Use the test-only friend helper to atomically cancel the flush timer
//      AND increment flushSeq, so the already-queued (or soon-to-fire)
//      timer callback will see seq != flushSeq and will NOT call Flush().
//   4. Destroy the BB (reset the shared_ptr).
//      The destructor calls Flush() directly, which calls os->Apply().
TEST_CASE(SUITE("Edge_DestructorFlushesWithoutTimer"))
{
	TestLifecycle lifecycle;
	ThreadPool thread_pool(4);

	Fixture f;
	f.WarmEMA();

	// Confirm the window is non-zero (timer is armed for MAX_PERIOD_MS,
	// long enough that it won't fire before our explicit cancel below)
	REQUIRE(f.CurrentBatchWindowMs() > 0);

	const size_t baseTotalEvents = f.os->TotalEvents();
	const size_t baseFlushes     = f.os->FlushCount();

	constexpr size_t FEW = 2;
	f.SendEvents(FEW);

	// Sync to ensure both events are in pBuilder (not just in the strand queue)
	f.Sync();

	// Atomically: cancel timer + invalidate seq → timer callback is now a no-op
	BatchUpdateBuilderTest::CancelFlushTimerSync(*f.bb);

	// Destroy the BB.  Destructor calls Flush() synchronously on this thread.
	f.bb.reset();

	// Flush() is synchronous – MockOutstation::Apply() was called from inside
	// the destructor, so the cv notification has already fired by now.
	// WaitForFlushes() should return immediately.
	REQUIRE(f.os->WaitForFlushes(baseFlushes + 1, CI_SLACK));

	// The destructor's Flush() must have delivered exactly FEW events
	CHECK(f.os->TotalEvents() == baseTotalEvents + FEW);
	CHECK(f.os->BatchSizes().back() == FEW);
}
