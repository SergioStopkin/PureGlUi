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
 * @file TestTabCloseBoundOf.cpp
 * @brief Unit tests for ui/render/tabcloseboundof.h, where a tab's close glyph stands in
 *        it: a square inset by its margin, `right` in from the tab's right edge, and with
 *        the tab wherever it is drawn
 */

#include "ui/render/tabcloseboundof.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/layout.h"

#include <gtest/gtest.h>

using Ui::Render::tabCloseBoundOf;
using Ui::Res::Type::bound_t;

namespace {

Ui::Res::Type::layout_t closeLayout()
{
    Ui::Res::Type::layout_t layout;
    layout.tabCloseMargin = 4.0F;
    layout.tabCloseRight  = 8.0F;
    return layout;
}

} // namespace

TEST(TabCloseBoundOfTest, SquareInsetFromTheRightEdge)
{
    const bound_t close = tabCloseBoundOf(closeLayout(), { .x = 100.0F, .y = 20.0F, .w = 144.0F, .h = 32.0F });
    EXPECT_FLOAT_EQ(close.w, 24.0F);
    EXPECT_FLOAT_EQ(close.h, 24.0F);
    EXPECT_FLOAT_EQ(close.x, 100.0F + 144.0F - 24.0F - 8.0F);
    EXPECT_FLOAT_EQ(close.y, 24.0F);
}

TEST(TabCloseBoundOfTest, MovesWithItsTab)
{
    const bound_t home  = tabCloseBoundOf(closeLayout(), { .x = 100.0F, .y = 20.0F, .w = 144.0F, .h = 32.0F });
    const bound_t moved = tabCloseBoundOf(closeLayout(), { .x = 150.0F, .y = 20.0F, .w = 144.0F, .h = 32.0F });
    EXPECT_FLOAT_EQ(moved.x - home.x, 50.0F);
    EXPECT_FLOAT_EQ(moved.y, home.y);
}
