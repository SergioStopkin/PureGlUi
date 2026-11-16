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
 * @file TestTabSwitcherBoundOf.cpp
 * @brief Unit tests for ui/window/tabswitcherboundof.h, where the Ctrl+Tab list stands: over
 *        the window's middle, as many rows as fit clear of its edges, as few as there are,
 *        and no list at all for rows of no height.
 */

#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/tabswitcherboundof.h"

#include <gtest/gtest.h>
#include <optional>

using Ui::Res::Type::bound_t;
using Ui::Window::tabSwitcherBoundOf;

namespace {

constexpr Ui::fpx_t WINDOW_W = 800.0F;
constexpr Ui::fpx_t WINDOW_H = 500.0F;
constexpr Ui::fpx_t WIDTH    = 300.0F; // the rows fitted
constexpr Ui::fpx_t MARGIN   = 20.0F;
constexpr Ui::fpx_t ROW      = 32.0F;

} // namespace

// 460 px clear of the edges hold 14 rows of 32: 448 px, centred
TEST(TabSwitcherBoundOfTest, AsManyRowsAsFitCentred)
{
    const std::optional<bound_t> place = tabSwitcherBoundOf(WINDOW_W, WINDOW_H, WIDTH, MARGIN, ROW, 30);
    ASSERT_TRUE(place.has_value());
    EXPECT_EQ(place.value(), (bound_t { .x = 250.0F, .y = 26.0F, .w = WIDTH, .h = 448.0F }));
}

TEST(TabSwitcherBoundOfTest, FewRowsAsTallAsThey)
{
    const std::optional<bound_t> place = tabSwitcherBoundOf(WINDOW_W, WINDOW_H, WIDTH, MARGIN, ROW, 3);
    ASSERT_TRUE(place.has_value());
    EXPECT_FLOAT_EQ(place.value().h, ROW * 3.0F);
    EXPECT_FLOAT_EQ(place.value().y, (WINDOW_H - (ROW * 3.0F)) / 2.0F);
}

// A layout naming no row height gives no list, never a window of no height
TEST(TabSwitcherBoundOfTest, NoRowHeightNoList)
{
    EXPECT_FALSE(tabSwitcherBoundOf(WINDOW_W, WINDOW_H, WIDTH, MARGIN, 0.0F, 3).has_value());
}

TEST(TabSwitcherBoundOfTest, NoRowsNoList)
{
    EXPECT_FALSE(tabSwitcherBoundOf(WINDOW_W, WINDOW_H, WIDTH, MARGIN, ROW, 0).has_value());
}

// A window too low for a row still shows one: never none while there are rows
TEST(TabSwitcherBoundOfTest, ATinyWindowStillShowsARow)
{
    const std::optional<bound_t> place = tabSwitcherBoundOf(WINDOW_W, 20.0F, WIDTH, MARGIN, ROW, 3);
    ASSERT_TRUE(place.has_value());
    EXPECT_FLOAT_EQ(place.value().h, ROW);
}
