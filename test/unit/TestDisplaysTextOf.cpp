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
 * @file TestDisplaysTextOf.cpp
 * @brief Unit tests for ui/window/displaystextof.h: System info's %DISPLAYS%, one line
 *        per display, the rate to a hundredth and left out where unknown.
 */

#include "ui/window/display.h"
#include "ui/window/displaystextof.h"

#include <gtest/gtest.h>

using Ui::Window::display_t;
using Ui::Window::displaysTextOf;

TEST(DisplaysTextOf, OneLinePerDisplayThePrimaryMarked)
{
    EXPECT_EQ(
    displaysTextOf(
    { display_t { .name = "eDP-1", .width = 2560, .height = 1600, .refreshRate = 89.9499F, .isPrimary = true },
      display_t { .name = "HDMI-1", .width = 1920, .height = 1080, .refreshRate = 60.0F } }),
    "eDP-1: 2560x1600, 89.95 Hz, primary\nHDMI-1: 1920x1080, 60 Hz");
}

// A display that gives no rate, or no name, is still listed
TEST(DisplaysTextOf, AnUnknownRateOrNameIsLeftOut)
{
    EXPECT_EQ(displaysTextOf({ display_t { .width = 1024, .height = 768 } }), "Display: 1024x768");
}

// Negative: nothing to list
TEST(DisplaysTextOf, NoDisplayIsNotAvailable) { EXPECT_EQ(displaysTextOf({}), "N/A"); }
