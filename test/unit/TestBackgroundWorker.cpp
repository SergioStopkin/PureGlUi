// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureGlUi. PureGlUi is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureGlUi is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureGlUi. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestBackgroundWorker.cpp
 * @brief Guard for Common::BackgroundWorker - the CPU-budgeted async task pool.
 *        Completion is checked race-free: the destructor drains every queued
 *        task then joins, so "post N, destroy, assert N ran" needs no sleeps.
 *        The budget invariant (in-flight runners <= budget) holds regardless of
 *        timing. Pure C++ (threads only) - no GL/X11.
 */

#include "common/backgroundworker.h"
#include "common/system.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

TEST(BackgroundWorker, AllPostedTasksRunBeforeShutdown)
{
    std::atomic<int> count { 0 };
    {
        Common::BackgroundWorker worker;
        for (int i = 0; i < 100; ++i) {
            worker.post([&count]() { count.fetch_add(1); });
        }
    } // destructor drains the queue + joins, so every posted task has run
    EXPECT_EQ(count.load(), 100);
}

TEST(BackgroundWorker, SingleArgPostRuns)
{
    std::atomic<bool> ran { false };
    {
        Common::BackgroundWorker worker;
        worker.post([&ran]() { ran.store(true); });
    }
    EXPECT_TRUE(ran.load());
}

TEST(BackgroundWorker, ZeroThreadsRunsAsSingleCore)
{
    std::atomic<bool> ran { false };
    {
        Common::BackgroundWorker worker;
        worker.post(0, [&ran]() { ran.store(true); });
    }
    EXPECT_TRUE(ran.load());
}

TEST(BackgroundWorker, MultiCoreTaskRuns)
{
    std::atomic<bool> ran { false };
    {
        Common::BackgroundWorker worker;
        worker.post(worker.budget(), [&ran]() { ran.store(true); });
    }
    EXPECT_TRUE(ran.load());
}

TEST(BackgroundWorker, OverBudgetRequestIsClampedAndRuns)
{
    // A request larger than the pool is clamped to the budget, so it still gets
    // admitted (never blocks forever asking for more cores than exist).
    std::atomic<bool> ran { false };
    {
        Common::BackgroundWorker worker;
        worker.post(worker.budget() + 100, [&ran]() { ran.store(true); });
    }
    EXPECT_TRUE(ran.load());
}

TEST(BackgroundWorker, BudgetMatchesCoreFormula)
{
    Common::BackgroundWorker worker;
    const unsigned int       cores    = Common::System::cpuCores();
    const unsigned int       expected = (cores > 2) ? cores - 2 : 1;
    EXPECT_EQ(worker.budget(), expected);
    EXPECT_GE(worker.budget(), 1U);
}

TEST(BackgroundWorker, NeverExceedsBudget)
{
    std::atomic<int> inFlight { 0 };
    std::atomic<int> maxInFlight { 0 };
    unsigned int     budget = 0;
    {
        Common::BackgroundWorker worker;
        budget = worker.budget();
        for (int i = 0; i < 64; ++i) {
            worker.post(1, [&inFlight, &maxInFlight]() {
                const int now  = inFlight.fetch_add(1) + 1;
                int       prev = maxInFlight.load();
                while (now > prev && !maxInFlight.compare_exchange_weak(prev, now)) { }
                // Hold the slot briefly so tasks actually overlap.
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                inFlight.fetch_sub(1);
            });
        }
    }
    // The pool hands out at most `budget` single-core slots at once.
    EXPECT_LE(static_cast<unsigned int>(maxInFlight.load()), budget);
}
