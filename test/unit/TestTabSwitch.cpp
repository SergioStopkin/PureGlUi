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
 * @file TestTabSwitch.cpp
 * @brief Unit tests for ui/tabswitch.h, the Ctrl+Tab list while Ctrl is held: the tab it
 *        opens on, its highlight wrapping both ways, the pointer's row, and when it goes
 *        up - only once its delay has run out, so a quick tap switches with no list. And
 *        ui/window/iscontrolkey.h, what a Ctrl going up is named on every platform
 */

#include "common/time.h"
#include "ui/tabswitch.h"
#include "ui/type.h"
#include "ui/window/iscontrolkey.h"

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

using Ui::TabSwitch;

namespace {

// Tabs 3, 1, 2 by when last active, 3 the active one
const std::vector<Ui::id_t> RECENT = { 3, 1, 2 };

} // namespace

TEST(TabSwitch, NextOpensOnTheTabViewedBefore)
{
    TabSwitch tabSwitch;
    tabSwitch.open(RECENT, 1, Common::Time::now());
    EXPECT_TRUE(tabSwitch.isOpen());
    EXPECT_EQ(tabSwitch.selectedId(), 1U);
}

TEST(TabSwitch, PriorOpensOnTheLeastRecent)
{
    TabSwitch tabSwitch;
    tabSwitch.open(RECENT, -1, Common::Time::now());
    EXPECT_EQ(tabSwitch.selectedId(), 2U);
}

TEST(TabSwitch, CycleWrapsBothWays)
{
    TabSwitch tabSwitch;
    tabSwitch.open(RECENT, 1, Common::Time::now());
    tabSwitch.cycle(1);
    EXPECT_EQ(tabSwitch.selectedId(), 2U);
    tabSwitch.cycle(1); // past the last, the first - the active one
    EXPECT_EQ(tabSwitch.selectedId(), 3U);
    tabSwitch.cycle(-1); // before the first, the last
    EXPECT_EQ(tabSwitch.selectedId(), 2U);
}

TEST(TabSwitch, ALoneTabSelectsItself)
{
    TabSwitch tabSwitch;
    tabSwitch.open({ 5 }, 1, Common::Time::now());
    EXPECT_EQ(tabSwitch.selectedId(), 5U);
}

TEST(TabSwitch, NoTabLeavesItClosed)
{
    TabSwitch tabSwitch;
    tabSwitch.open({}, 1, Common::Time::now());
    EXPECT_FALSE(tabSwitch.isOpen());
    EXPECT_EQ(tabSwitch.selectedId(), Ui::INVALID_ID);
}

// The pointer's row is the one Ctrl going up takes; one past the last changes nothing
TEST(TabSwitch, SelectTakesTheRowUnderThePointer)
{
    TabSwitch tabSwitch;
    tabSwitch.open(RECENT, 1, Common::Time::now());
    tabSwitch.select(0);
    EXPECT_EQ(tabSwitch.selectedId(), 3U);
    tabSwitch.select(99);
    EXPECT_EQ(tabSwitch.selectedRow(), 0U);
}

// Up only once Ctrl has been held past its delay, and once
TEST(TabSwitch, DueOnlyOnceItsDelayHasRun)
{
    const Common::Time::point_t opened = Common::Time::now();
    TabSwitch                   tabSwitch;
    tabSwitch.open(RECENT, 1, opened + std::chrono::milliseconds(200));
    EXPECT_FALSE(tabSwitch.isDue(opened));
    EXPECT_TRUE(tabSwitch.isDue(opened + std::chrono::milliseconds(200)));

    tabSwitch.setShown();
    EXPECT_TRUE(tabSwitch.isShown());
    EXPECT_FALSE(tabSwitch.isDue(opened + std::chrono::milliseconds(500)));
}

TEST(TabSwitch, CloseForgetsIt)
{
    TabSwitch tabSwitch;
    tabSwitch.open(RECENT, 1, Common::Time::now());
    tabSwitch.setShown();
    tabSwitch.close();
    EXPECT_FALSE(tabSwitch.isOpen());
    EXPECT_FALSE(tabSwitch.isShown());
    EXPECT_FALSE(tabSwitch.isDue(Common::Time::now()));
}

// Either Ctrl, named as X11 names it - and on macOS Cmd too, folded into Control
TEST(IsControlKey, EitherCtrlAndNothingElse)
{
    EXPECT_TRUE(Ui::Window::isControlKey("Control_L"));
    EXPECT_TRUE(Ui::Window::isControlKey("Control_R"));
    EXPECT_FALSE(Ui::Window::isControlKey("Tab"));
    EXPECT_FALSE(Ui::Window::isControlKey(""));
}
