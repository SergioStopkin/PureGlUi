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
 * @file TestTooltipBoundOf.cpp
 * @brief Unit tests for ui/window/tooltipboundof.h: a tooltip stands beside its
 *        anchor, flips before it where after would run past the window, and is
 *        never cut off at the window's edge.
 */

#include "ui/res/type/bound.h"
#include "ui/res/type/region.h"
#include "ui/type.h"
#include "ui/window/tooltipboundof.h"

#include <gtest/gtest.h>

using Ui::fpx_t;
using Ui::Res::Type::bound_t;
using Ui::Res::Type::region_t;
using Ui::Window::tooltipBoundOf;

namespace {

constexpr fpx_t WINDOW_W = 800.0F;
constexpr fpx_t WINDOW_H = 600.0F;
constexpr fpx_t WIDTH    = 100.0F; // the tooltip's

region_t box()
{
    region_t region;
    region.height = 32.0F;
    region.margin = 8.0F;
    return region;
}

} // namespace

TEST(TooltipBoundOfTest, StandsAfterItsAnchorCentredOnIt)
{
    const bound_t bound = tooltipBoundOf({ 100.0F, 200.0F, 40.0F, 40.0F }, box(), WIDTH, WINDOW_W, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.x, 148.0F);
    EXPECT_FLOAT_EQ(bound.y, 204.0F);
    EXPECT_FLOAT_EQ(bound.w, WIDTH);
    EXPECT_FLOAT_EQ(bound.h, 32.0F);
}

// As on the right toolbar
TEST(TooltipBoundOfTest, FlipsBeforeItsAnchorWhereAfterWouldRunPastTheWindow)
{
    const bound_t bound = tooltipBoundOf({ 700.0F, 200.0F, 40.0F, 40.0F }, box(), WIDTH, WINDOW_W, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.x, 592.0F);
}

// Ending exactly at the window's edge is not past it: 652 + 40 + 8 + 100 = 800
TEST(TooltipBoundOfTest, StaysAfterItsAnchorEndingAtTheWindowsEdge)
{
    const bound_t bound = tooltipBoundOf({ 652.0F, 200.0F, 40.0F, 40.0F }, box(), WIDTH, WINDOW_W, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.x, 700.0F);
}

// Flipped in a window too narrow for either side, it would start at -48
TEST(TooltipBoundOfTest, NeverLeavesTheWindowOnTheLeft)
{
    const bound_t bound = tooltipBoundOf({ 60.0F, 200.0F, 40.0F, 40.0F }, box(), WIDTH, 150.0F, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.x, 0.0F);
}

// A snap on the window's top edge: centred, it would start at -11
TEST(TooltipBoundOfTest, NeverLeavesTheWindowAtTheTop)
{
    const bound_t bound = tooltipBoundOf({ 100.0F, 0.0F, 10.0F, 10.0F }, box(), WIDTH, WINDOW_W, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.y, 0.0F);
}

// Centred, it would end at 616
TEST(TooltipBoundOfTest, NeverLeavesTheWindowAtTheBottom)
{
    const bound_t bound = tooltipBoundOf({ 100.0F, 595.0F, 10.0F, 10.0F }, box(), WIDTH, WINDOW_W, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.y, WINDOW_H - 32.0F);
}

// No room for it at all: pinned to the left edge rather than anywhere past it
TEST(TooltipBoundOfTest, AWindowNarrowerThanItPinsItToTheLeftEdge)
{
    const bound_t bound = tooltipBoundOf({ 10.0F, 200.0F, 20.0F, 20.0F }, box(), WIDTH, 80.0F, WINDOW_H);
    EXPECT_FLOAT_EQ(bound.x, 0.0F);
}
