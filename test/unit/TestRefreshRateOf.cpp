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
 * @file TestRefreshRateOf.cpp
 * @brief Unit tests for ui/window/refreshrateof.h: frames are paced at the primary
 *        display's rate, the first known one without it, and 0 - the default kept -
 *        where no display gives one.
 */

#include "ui/window/display.h"
#include "ui/window/refreshrateof.h"

#include <gtest/gtest.h>

using Ui::Window::display_t;
using Ui::Window::refreshRateOf;

TEST(RefreshRateOf, ThePrimaryDisplaysRateWins)
{
    EXPECT_FLOAT_EQ(refreshRateOf({ display_t { .name = "HDMI-1", .refreshRate = 60.0F },
                                    display_t { .name = "eDP-1", .refreshRate = 90.0F, .isPrimary = true } }),
                    90.0F);
}

// With none primary, the first that says
TEST(RefreshRateOf, WithoutAPrimaryTheFirstKnownRate)
{
    EXPECT_FLOAT_EQ(refreshRateOf({ display_t { .name = "VIRTUAL-1" },
                                    display_t { .name = "HDMI-1", .refreshRate = 144.0F },
                                    display_t { .name = "DP-1", .refreshRate = 60.0F } }),
                    144.0F);
}

// A primary with no rate of its own does not hide the others'
TEST(RefreshRateOf, APrimaryWithoutARateLeavesTheOthers)
{
    EXPECT_FLOAT_EQ(refreshRateOf({ display_t { .name = "eDP-1", .isPrimary = true },
                                    display_t { .name = "HDMI-1", .refreshRate = 75.0F } }),
                    75.0F);
}

// Negative: no display, or none with a rate, is no answer
TEST(RefreshRateOf, NoRateAnywhereIsZero)
{
    EXPECT_FLOAT_EQ(refreshRateOf({}), 0.0F);
    EXPECT_FLOAT_EQ(refreshRateOf({ display_t { .name = "VIRTUAL-1", .isPrimary = true } }), 0.0F);
}
