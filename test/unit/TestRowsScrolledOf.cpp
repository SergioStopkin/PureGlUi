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
 * @file TestRowsScrolledOf.cpp
 * @brief Unit tests for ui/render/rowsscrolledof.h, how many rows a wheel's turn scrolls a dock
 *        or the Ctrl+Tab list: by the scroll speed, in the user's direction, a notch at least a
 *        row however coarse the row, none for no turn or rows of no height
 */

#include "ui/render/rowsscrolledof.h"
#include "ui/res/type/input.h"
#include "ui/type.h"

#include <gtest/gtest.h>

using Ui::Render::rowsScrolledOf;

namespace {

constexpr Ui::fpx_t ROW = 32.0F;

// The wheel at `speed` px a notch, the content following the fingers when `isNatural`
Ui::Res::Type::input_t wheelOf(Ui::fpx_t speed, bool isNatural)
{
    Ui::Res::Type::input_t input;
    input.scrollSpeed   = speed;
    input.scrollNatural = isNatural;
    return input;
}

} // namespace

TEST(RowsScrolledOfTest, NoTurnNoRows) { EXPECT_EQ(rowsScrolledOf(0.0F, ROW, wheelOf(40.0F, false)), 0); }

TEST(RowsScrolledOfTest, RowsOfNoHeightNoRows) { EXPECT_EQ(rowsScrolledOf(1.0F, 0.0F, wheelOf(40.0F, false)), 0); }

// 2 notches at 40 px: 80 px, two whole rows of 32
TEST(RowsScrolledOfTest, ATurnScrollsItsWholeRows) { EXPECT_EQ(rowsScrolledOf(2.0F, ROW, wheelOf(40.0F, true)), 2); }

TEST(RowsScrolledOfTest, NaturalScrollingFlipsTheWay)
{
    EXPECT_EQ(rowsScrolledOf(2.0F, ROW, wheelOf(40.0F, false)), -2);
}

// A notch of 10 px is under a row: still one, either way
TEST(RowsScrolledOfTest, ANotchIsAtLeastARow)
{
    EXPECT_EQ(rowsScrolledOf(1.0F, ROW, wheelOf(10.0F, true)), 1);
    EXPECT_EQ(rowsScrolledOf(1.0F, ROW, wheelOf(10.0F, false)), -1);
}
