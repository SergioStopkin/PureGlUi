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
 * @file TestTime.cpp
 * @brief Unit tests for common/time.h: a sleep lasts its span and little more on every
 *        platform - Windows rounds a plain one up to its 15.6 ms timer tick, which paced
 *        the shell's frames and every test's wait by that tick instead.
 */

#include "common/time.h"
#include "common/warnifslow.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>

using Common::Time;
using Common::warnIfSlow;

namespace {

constexpr std::chrono::milliseconds ONE_MS { 1 };

// Sleeps of one millisecond: enough for a median a stray wake-up does not move
constexpr std::size_t SLEEPS = 9;

// Well under a Windows timer tick, well over a sleep that wakes when asked
constexpr double MOST_MS = 5.0;

} // namespace

TEST(Time, ASleepLastsAtLeastItsSpan)
{
    const Time::point_t start = Time::now();
    Time::sleepFor(ONE_MS);
    EXPECT_GE(Time::msSince(start), 1.0);
}

TEST(Time, AMillisecondSleepIsNotATimerTick)
{
    std::array<double, SLEEPS> sleptMs {};
    for (double & slept : sleptMs) {
        const Time::point_t start = Time::now();
        Time::sleepFor(ONE_MS);
        slept = Time::msSince(start);
    }
    std::ranges::sort(sleptMs);
    warnIfSlow("a 1 ms sleep's median", sleptMs.at(SLEEPS / 2), MOST_MS);
}

TEST(Time, ASleepUntilWakesAtItsPoint)
{
    const Time::point_t start = Time::now();
    Time::sleepUntil(start + ONE_MS);
    EXPECT_GE(Time::msSince(start), 1.0);
}

TEST(Time, ASleepUntilAPassedPointReturnsAtOnce)
{
    const Time::point_t start = Time::now();
    Time::sleepUntil(start - std::chrono::seconds(1));
    warnIfSlow("a sleep until a passed point", Time::msSince(start), MOST_MS);
}
