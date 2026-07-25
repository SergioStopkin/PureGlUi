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
 * @file TestSig.cpp
 * @brief sig.h - the stop-callback store + signal-name mapping, headless.
 *
 * sig is inline (header-only) so the framework ships no translation unit; these
 * cases pin the two behaviours a consumer depends on. on_signal() itself is
 * deliberately NOT called: it is one-shot by design (a second signal _Exit()s the
 * process), so invoking it from a test would take the whole binary down. What it
 * does with the callback is covered by driving g_stop directly, which is the same
 * variable it reads.
 */

#include "sig.h"

#include <csignal>
#include <gtest/gtest.h>
#include <string>

namespace {

TEST(Sig, AppInitStoresTheStopCallback)
{
    bool stopped = false;
    g_app_init([&stopped]() { stopped = true; });

    ASSERT_TRUE(static_cast<bool>(g_stop));
    g_stop();
    EXPECT_TRUE(stopped);
}

TEST(Sig, AppInitReplacesAPreviousCallback)
{
    int first  = 0;
    int second = 0;
    g_app_init([&first]() { ++first; });
    g_app_init([&second]() { ++second; });

    g_stop();
    EXPECT_EQ(first, 0); // replaced, not chained
    EXPECT_EQ(second, 1);
}

TEST(Sig, SignalNameMapsTheHandledSignals)
{
    EXPECT_EQ(signal_name(SIGINT), "SIGINT");
    EXPECT_EQ(signal_name(SIGTERM), "SIGTERM");
}

TEST(Sig, SignalNameIsUnknownForAnythingElse) { EXPECT_EQ(signal_name(-1), "UNKNOWN"); }

} // namespace
