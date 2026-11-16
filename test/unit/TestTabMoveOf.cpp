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
 * @file TestTabMoveOf.cpp
 * @brief Unit tests for ui/window/tabmoveof.h, where a tab dragged along the strip goes,
 *        by where the drag draws it: drawn half over a neighbour (swap-at 0.5) it jumps that
 *        neighbour, short of it it stays, and at swap-at 1 only once over it whole; a jump
 *        the layout has not redrawn yet is neither made twice nor taken back, and a
 *        neighbour scrolled off the strip is never passed
 */

#include "ui/res/type/bound.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/type.h"
#include "ui/window/tabmoveof.h"

#include <gtest/gtest.h>
#include <optional>
#include <unordered_map>
#include <vector>

using Ui::Res::Type::bound_t;

namespace {

constexpr Ui::fpx_t HALF  = 0.5F; // the shipped swap-at: the leading edge at the neighbour's middle
constexpr Ui::fpx_t WHOLE = 1.0F; // over the neighbour whole

// Tabs in this order, each `tab_t` with only its id
std::vector<Ui::tab_t> tabsOf(const std::vector<Ui::id_t> & ids)
{
    std::vector<Ui::tab_t> tabs;
    for (const Ui::id_t id : ids) {
        tabs.emplace_back(Ui::tab_t { .id = id });
    }
    return tabs;
}

// Where the last layout put them: 1, 2, 3 left to right, 100 wide each
const std::unordered_map<Ui::id_t, bound_t> PLACED = {
    { 1, { .x = 0.0F, .w = 100.0F } },
    { 2, { .x = 100.0F, .w = 100.0F } },
    { 3, { .x = 200.0F, .w = 100.0F } },
};

bound_t placeOf(Ui::id_t id)
{
    const auto it = PLACED.find(id);
    return it == PLACED.end() ? bound_t {} : it->second;
}

// Where `draggedId` goes, drawn with its left edge at `left`
std::optional<Ui::id_t> moveOf(const Ui::TabBar & tabBar, Ui::id_t draggedId, Ui::fpx_t left, Ui::fpx_t swapAt = HALF)
{
    return Ui::Window::tabMoveOf(tabBar, placeOf, draggedId, left, swapAt);
}

} // namespace

TEST(TabMoveOf, ShortOfHalfOverANeighbourItStays)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 1, 2, 3 }));
    EXPECT_FALSE(moveOf(tabBar, 1, 40.0F).has_value());  // its right edge 140, short of 2's middle
    EXPECT_FALSE(moveOf(tabBar, 3, 160.0F).has_value()); // its left edge 160, short of 2's middle
}

TEST(TabMoveOf, HalfOverTheRightNeighbourItGoesAfterIt)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 1, 2, 3 }));
    EXPECT_EQ(moveOf(tabBar, 1, 60.0F), 3U);              // right before 3
    EXPECT_EQ(moveOf(tabBar, 2, 160.0F), Ui::INVALID_ID); // the end
}

TEST(TabMoveOf, HalfOverTheLeftNeighbourItGoesBeforeIt)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 1, 2, 3 }));
    EXPECT_EQ(moveOf(tabBar, 2, 40.0F), 1U);
    EXPECT_EQ(moveOf(tabBar, 3, 140.0F), 2U);
}

TEST(TabMoveOf, AtWholeOnlyOnceOverTheNeighbourWhole)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 1, 2, 3 }));
    EXPECT_FALSE(moveOf(tabBar, 1, 60.0F, WHOLE).has_value());
    EXPECT_EQ(moveOf(tabBar, 1, 110.0F, WHOLE), 3U);
    EXPECT_FALSE(moveOf(tabBar, 2, 10.0F, WHOLE).has_value());
    EXPECT_EQ(moveOf(tabBar, 2, -10.0F, WHOLE), 1U);
}

// 1 moved after 2, the layout not redrawn yet: 3 is still a whole tab off, and 2 - now its
// left neighbour - still stands right of 1's place, so 1 neither jumps again nor back
TEST(TabMoveOf, AJumpNotRedrawnYetIsNeitherMadeTwiceNorTakenBack)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 2, 1, 3 }));
    EXPECT_FALSE(moveOf(tabBar, 1, 60.0F).has_value());
}

// A neighbour scrolled off the strip (4 and 5, never drawn) has no place, and is never passed
TEST(TabMoveOf, ANeighbourOffTheStripIsNeverPassed)
{
    Ui::TabBar tabBar;
    tabBar.setTabs(tabsOf({ 4, 3, 5 }));
    EXPECT_FALSE(moveOf(tabBar, 3, 500.0F).has_value());
    EXPECT_FALSE(moveOf(tabBar, 3, -50.0F).has_value());
}
