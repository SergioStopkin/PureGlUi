// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureCreator. PureCreator is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureCreator is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureCreator. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestClickCounter.cpp
 * @brief Unit tests for the platform-event-layer multi-click burst counter.
 *
 * ClickCounter is the back-end the X11/Wayland/Win32 event translators use
 * to populate Event.mouse.clickCount. macOS bypasses it (NSEvent supplies
 * clickCount natively), so these tests focus on the timing + position +
 * button predicate that defines a click burst.
 */

#include "ui/backend/window/clickcounter.h"

#include <gtest/gtest.h>

using Ui::Backend::Window::ClickCounter;

namespace {

constexpr uint32_t LMB = 1; // arbitrary button id, content-agnostic to the counter
constexpr uint32_t RMB = 3;

constexpr uint32_t kIntervalMs = 400;
constexpr int      kDistancePx = 5;

ClickCounter makeCounter()
{
    ClickCounter c;
    c.configure(kIntervalMs, kDistancePx);
    return c;
}

} // namespace

// ============================================================================
// Positive cases
// ============================================================================

TEST(ClickCounter, FirstClickReturnsOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
}

TEST(ClickCounter, DoubleClickWithinAllBounds)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1200, 101, 101, LMB), 2);
}

TEST(ClickCounter, TripleClickWithinAllBounds)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1200, 100, 100, LMB), 2);
    EXPECT_EQ(c.next(1400, 100, 100, LMB), 3);
}

TEST(ClickCounter, IntervalBoundaryInclusive)
{
    // Elapsed == intervalMs counts as "within"
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1000 + kIntervalMs, 100, 100, LMB), 2);
}

TEST(ClickCounter, DistanceBoundaryInclusive)
{
    // dx^2 + dy^2 == distancePx^2 counts as "near"
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1100, 100 + kDistancePx, 100, LMB), 2);
}

TEST(ClickCounter, DistanceDiagonalWithin)
{
    // 3-4-5 triangle: distancePx=5 admits a (3,4) offset
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1100, 103, 104, LMB), 2);
}

TEST(ClickCounter, ConfigureUpdatesThresholds)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    // After widening the interval, a normally-too-slow second press pairs up.
    c.configure(1000, kDistancePx);
    EXPECT_EQ(c.next(1900, 100, 100, LMB), 2);
}

TEST(ClickCounter, IndependentBursts)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1200, 100, 100, LMB), 2);
    // Long gap -> burst resets
    EXPECT_EQ(c.next(5000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(5200, 100, 100, LMB), 2);
}

TEST(ClickCounter, DefaultConfigUses400ms5px)
{
    ClickCounter c; // no configure() call
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1399, 100, 100, LMB), 2); // 399ms < 400ms default
    // 401ms would normally exceed the default; verify the boundary:
    ClickCounter c2;
    EXPECT_EQ(c2.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c2.next(1401, 100, 100, LMB), 1);
}

// ============================================================================
// Negative cases
// ============================================================================

TEST(ClickCounter, ExceedsIntervalResetsToOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1000 + kIntervalMs + 1, 100, 100, LMB), 1);
}

TEST(ClickCounter, ExceedsDistanceResetsToOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1100, 100 + kDistancePx + 1, 100, LMB), 1);
}

TEST(ClickCounter, DifferentButtonResetsToOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1100, 100, 100, RMB), 1);
}

TEST(ClickCounter, TripleBrokenByIntervalGoesOneTwoOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1200, 100, 100, LMB), 2);
    EXPECT_EQ(c.next(1200 + kIntervalMs + 1, 100, 100, LMB), 1);
}

TEST(ClickCounter, TripleBrokenByDistanceGoesOneTwoOne)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1100, 100, 100, LMB), 2);
    EXPECT_EQ(c.next(1200, 100 + kDistancePx + 1, 100, LMB), 1);
}

TEST(ClickCounter, ZeroIntervalNeverPairs)
{
    ClickCounter c;
    c.configure(0, kDistancePx);
    // First press always returns 1 regardless of thresholds; we expect the
    // second to stay at 1 because any positive elapsed > 0 fails the test.
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(1001, 100, 100, LMB), 1);
}

TEST(ClickCounter, ZeroDistanceRequiresExactPosition)
{
    ClickCounter c;
    c.configure(kIntervalMs, 0);
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    // 1 px off -> dist^2 = 1 > 0^2, fails the near test.
    EXPECT_EQ(c.next(1100, 101, 100, LMB), 1);
    // Exact same position pairs up.
    EXPECT_EQ(c.next(1200, 101, 100, LMB), 2);
}

TEST(ClickCounter, NegativeDeltaSameMagnitude)
{
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    // dx = -3, dy = -4 -> dist 5, still within boundary.
    EXPECT_EQ(c.next(1100, 97, 96, LMB), 2);
}

TEST(ClickCounter, BurstIndependentFromEarlierWindow)
{
    // Make sure a stale "lastTime != 0" doesn't haunt a new session after
    // the burst is broken by interval; the second burst's first click must
    // still report 1 and start fresh.
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(1000, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(9999, 100, 100, LMB), 1); // long gap
    EXPECT_EQ(c.next(10100, 100, 100, LMB), 2);
    EXPECT_EQ(c.next(10200, 100, 100, LMB), 3);
}

TEST(ClickCounter, TimeZeroFirstClickPairsWithNext)
{
    // Regression: a sentinel-based "lastTime == 0 means no prior" check would
    // misclassify a real time=0 first click, causing the second click to
    // restart the burst at 1. The explicit m_hasPrior flag fixes this.
    ClickCounter c = makeCounter();
    EXPECT_EQ(c.next(0, 100, 100, LMB), 1);
    EXPECT_EQ(c.next(100, 100, 100, LMB), 2);
}
