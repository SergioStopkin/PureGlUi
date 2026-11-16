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
 * @file TestRowCloseBoundOf.cpp
 * @brief Unit tests for ui/render/rowcloseboundof.h, where a row's close button stands in it:
 *        its icon with its margin round it, a square `right` in from the row's right edge,
 *        centred on the row's height, and with the row wherever it stands
 */

#include "ui/render/rowcloseboundof.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/region.h"
#include "ui/type.h"

#include <gtest/gtest.h>

using Ui::Render::rowCloseBoundOf;
using Ui::Res::Type::bound_t;
using Ui::Res::Type::region_t;

namespace {

// A 14 px icon, `margin` round it, 6 px in from the right
region_t closeOf(Ui::fpx_t margin)
{
    region_t close;
    close.height = 14.0F;
    close.margin = margin;
    close.right  = 6.0F;
    return close;
}

const bound_t ROW = { .x = 0.0F, .y = 64.0F, .w = 300.0F, .h = 32.0F };

} // namespace

// 4 px round a 14 px icon: a 22 px square
TEST(RowCloseBoundOfTest, ItsIconAndItsMarginASquare)
{
    const bound_t close = rowCloseBoundOf(closeOf(4.0F), ROW);
    EXPECT_FLOAT_EQ(close.w, 22.0F);
    EXPECT_FLOAT_EQ(close.h, 22.0F);
}

TEST(RowCloseBoundOfTest, RightInFromTheRowsRightEdge)
{
    EXPECT_FLOAT_EQ(rowCloseBoundOf(closeOf(4.0F), ROW).x, 300.0F - 6.0F - 22.0F);
}

// 32 px row, 22 px square: 5 px over and under it
TEST(RowCloseBoundOfTest, CentredOnTheRowsHeight)
{
    EXPECT_FLOAT_EQ(rowCloseBoundOf(closeOf(4.0F), ROW).y, 64.0F + 5.0F);
}

TEST(RowCloseBoundOfTest, MovesWithItsRow)
{
    const bound_t home  = rowCloseBoundOf(closeOf(4.0F), ROW);
    const bound_t moved = rowCloseBoundOf(closeOf(4.0F), { .x = 10.0F, .y = 96.0F, .w = 300.0F, .h = 32.0F });
    EXPECT_FLOAT_EQ(moved.x - home.x, 10.0F);
    EXPECT_FLOAT_EQ(moved.y - home.y, 32.0F);
}

// A narrower row - one left of a scrollbar's gutter - takes it nearer, still `right` in
TEST(RowCloseBoundOfTest, FollowsTheRowsRightEdge)
{
    EXPECT_FLOAT_EQ(rowCloseBoundOf(closeOf(4.0F), { .x = 0.0F, .y = 64.0F, .w = 280.0F, .h = 32.0F }).x,
                    280.0F - 6.0F - 22.0F);
}

// A row shorter than the square: still centred, standing over its top and bottom alike
TEST(RowCloseBoundOfTest, ARowShorterThanTheSquareStaysCentred)
{
    EXPECT_FLOAT_EQ(rowCloseBoundOf(closeOf(4.0F), { .x = 0.0F, .y = 64.0F, .w = 300.0F, .h = 12.0F }).y, 64.0F - 5.0F);
}

TEST(RowCloseBoundOfTest, NoMarginIsTheIconAlone)
{
    const bound_t close = rowCloseBoundOf(closeOf(0.0F), ROW);
    EXPECT_FLOAT_EQ(close.w, 14.0F);
    EXPECT_FLOAT_EQ(close.x, 300.0F - 6.0F - 14.0F);
}
