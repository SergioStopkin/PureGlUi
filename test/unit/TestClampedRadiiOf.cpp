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
 * @file TestClampedRadiiOf.cpp
 * @brief Unit tests for ui/window/clampedradiiof.h: a popup's corner radii rounded to
 *        whole px and never past half its shorter side - the squares Underlay reads
 *        under its corners, which the corner-capture component test now expects too.
 */

#include "ui/res/type/border.h"
#include "ui/window/clampedradiiof.h"

#include <gtest/gtest.h>

using Ui::Res::Type::border_t;
using Ui::Window::clampedRadiiOf;

TEST(ClampedRadiiOfTest, KeepsWholeRadiiWithinHalfTheShorterSide)
{
    const border_t radii { .topLeft = 12.0F, .topRight = 12.0F, .bottomRight = 12.0F, .bottomLeft = 12.0F };
    EXPECT_EQ(clampedRadiiOf(radii, 100.0F, 60.0F), radii);
}

TEST(ClampedRadiiOfTest, ClampsARadiusPastHalfTheShorterSide)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 40.0F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 30.0F);
}

// Half the shorter side is the most a corner may take, not past it
TEST(ClampedRadiiOfTest, KeepsARadiusOfExactlyHalfTheShorterSide)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 30.0F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 30.0F);
}

// Width the shorter side: the clamp follows whichever side is shorter
TEST(ClampedRadiiOfTest, ClampsByTheWidthWhereItIsTheShorterSide)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 40.0F }, 60.0F, 100.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 30.0F);
}

TEST(ClampedRadiiOfTest, RoundsAHalfPixelUp)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 7.5F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 8.0F);
}

TEST(ClampedRadiiOfTest, RoundsLessThanAHalfPixelDown)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 7.4F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 7.0F);
}

// Rounded before the clamp: 30.6 would round to 31, past the 30 the rect allows
TEST(ClampedRadiiOfTest, ClampsAfterRounding)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 30.6F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 30.0F);
}

// An odd shorter side leaves half a pixel at the clamp, the clamp coming after the rounding
TEST(ClampedRadiiOfTest, AnOddShorterSideClampsToHalfItsLength)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 40.0F }, 100.0F, 61.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 30.5F);
}

// A square corner stays square, so Underlay reads nothing under it
TEST(ClampedRadiiOfTest, KeepsAZeroRadiusZero) { EXPECT_EQ(clampedRadiiOf({}, 100.0F, 60.0F), border_t {}); }

// A radius under half a pixel rounds away: drawn, it would be no corner at all
TEST(ClampedRadiiOfTest, RoundsARadiusUnderHalfAPixelToZero)
{
    const border_t clamped = clampedRadiiOf({ .topLeft = 0.4F }, 100.0F, 60.0F);
    EXPECT_FLOAT_EQ(clamped.topLeft, 0.0F);
}

// Each corner on its own: one past the clamp, one rounded, one square, one kept
TEST(ClampedRadiiOfTest, ClampsEachCornerOnItsOwn)
{
    const border_t clamped = clampedRadiiOf(
        { .topLeft = 50.0F, .topRight = 7.5F, .bottomRight = 0.0F, .bottomLeft = 12.0F },
        100.0F,
        60.0F);
    EXPECT_EQ(clamped, (border_t { .topLeft = 30.0F, .topRight = 8.0F, .bottomRight = 0.0F, .bottomLeft = 12.0F }));
}

// A rect of no size leaves no room for any corner
TEST(ClampedRadiiOfTest, ARectOfNoSizeTakesNoCorner)
{
    const border_t radii { .topLeft = 12.0F, .topRight = 12.0F, .bottomRight = 12.0F, .bottomLeft = 12.0F };
    EXPECT_EQ(clampedRadiiOf(radii, 0.0F, 60.0F), border_t {});
}
