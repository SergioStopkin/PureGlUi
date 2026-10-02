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
 * @file TestTabsShownOf.cpp
 * @brief Unit tests for ui/render/tabstripwidthof.h (the strip is the room between
 *        the toolbars) and ui/render/tabsshownof.h (how many tabs it shows however
 *        it is scrolled: all while they fit at their narrowest, else as many as fit
 *        between both arrows, never fewer than one).
 */

#include "ui/render/tabsshownof.h"
#include "ui/render/tabstripwidthof.h"
#include "ui/res/type/layout.h"

#include <gtest/gtest.h>

using Ui::Render::tabsShownOf;
using Ui::Render::tabStripWidthOf;

namespace {

constexpr Ui::fpx_t MIN_TAB = 100.0F;
constexpr Ui::fpx_t ARROW   = 20.0F;

} // namespace

TEST(TabStripWidthOfTest, TheRoomBetweenTheToolbars)
{
    Ui::Res::Type::layout_t layout;
    layout.leftToolbar.width  = 54.0F;
    layout.rightToolbar.width = 40.0F;
    EXPECT_FLOAT_EQ(tabStripWidthOf(layout, 1280.0F), 1186.0F);
}

TEST(TabsShownOfTest, AllWhileTheyFitAtTheirNarrowest)
{
    EXPECT_EQ(tabsShownOf(500.0F, MIN_TAB, ARROW, 5), 5U);
    EXPECT_EQ(tabsShownOf(500.0F, MIN_TAB, ARROW, 3), 3U);
}

TEST(TabsShownOfTest, AsManyAsFitBetweenBothArrowsOnceTheyOverflow)
{
    EXPECT_EQ(tabsShownOf(500.0F, MIN_TAB, ARROW, 6), 4U); // (500 - 40) / 100
}

TEST(TabsShownOfTest, NeverFewerThanOne)
{
    EXPECT_EQ(tabsShownOf(90.0F, MIN_TAB, ARROW, 6), 1U);
    EXPECT_EQ(tabsShownOf(10.0F, MIN_TAB, ARROW, 6), 1U); // narrower than both arrows
}

TEST(TabsShownOfTest, NoneForNoTabs) { EXPECT_EQ(tabsShownOf(500.0F, MIN_TAB, ARROW, 0), 0U); }

TEST(TabsShownOfTest, NoMinimumWidthShowsAll) { EXPECT_EQ(tabsShownOf(500.0F, 0.0F, ARROW, 50), 50U); }
