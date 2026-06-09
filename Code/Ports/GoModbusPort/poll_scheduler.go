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
 * poll_scheduler.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

import (
	"context"
	"sync"
	"sync/atomic"
	"time"
)

type PollScheduler interface {
	SchedulePoll(pollFn func() error) bool
	Shutdown()
	Wait()
	Stats() PollSchedulerStats
}

type PollSchedulerStats struct {
	PollsScheduled uint64
	PollsDropped   uint64
	PollsRunning   int32
	MaxConcurrent  int
}

type pollScheduler struct {
	ctx           context.Context
	cancel        context.CancelFunc
	wg            sync.WaitGroup
	ticker        *time.Ticker
	pollFn        func() error
	sem           chan struct{}
	maxConcurrent int

	pollsScheduled atomic.Uint64
	pollsDropped   atomic.Uint64
	pollsRunning   atomic.Int32
}

func newPollScheduler(interval time.Duration, pollFn func() error, maxConcurrent int) *pollScheduler {
	if maxConcurrent <= 0 {
		maxConcurrent = 1
	}
	ctx, cancel := context.WithCancel(context.Background())
	ps := &pollScheduler{
		ctx:           ctx,
		cancel:        cancel,
		pollFn:        pollFn,
		sem:           make(chan struct{}, maxConcurrent),
		maxConcurrent: maxConcurrent,
		ticker:        time.NewTicker(interval),
	}
	ps.wg.Add(1)
	go ps.run()
	return ps
}

func (ps *pollScheduler) run() {
	defer ps.wg.Done()
	for {
		select {
		case <-ps.ctx.Done():
			return
		case <-ps.ticker.C:
			select {
			case ps.sem <- struct{}{}:
				ps.wg.Add(1)
				ps.pollsRunning.Add(1)
				ps.pollsScheduled.Add(1)
				go func() {
					defer func() {
						ps.pollsRunning.Add(-1)
						<-ps.sem
						ps.wg.Done()
					}()
					_ = ps.pollFn()
				}()
			default:
				ps.pollsDropped.Add(1)
			}
		}
	}
}

func (ps *pollScheduler) SchedulePoll(pollFn func() error) bool {
	select {
	case ps.sem <- struct{}{}:
		ps.wg.Add(1)
		ps.pollsRunning.Add(1)
		ps.pollsScheduled.Add(1)
		go func() {
			defer func() {
				ps.pollsRunning.Add(-1)
				<-ps.sem
				ps.wg.Done()
			}()
			_ = pollFn()
		}()
		return true
	default:
		ps.pollsDropped.Add(1)
		return false
	}
}

func (ps *pollScheduler) Shutdown() {
	ps.cancel()
	ps.ticker.Stop()
}

func (ps *pollScheduler) Wait() {
	ps.wg.Wait()
}

func (ps *pollScheduler) Stats() PollSchedulerStats {
	return PollSchedulerStats{
		PollsScheduled: ps.pollsScheduled.Load(),
		PollsDropped:   ps.pollsDropped.Load(),
		PollsRunning:   ps.pollsRunning.Load(),
		MaxConcurrent:  ps.maxConcurrent,
	}
}