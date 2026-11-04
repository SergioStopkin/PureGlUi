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
 * @file TestScaleFactorOf.cpp
 * @brief Unit tests for ui/window/scalefactorof.h: PUREGLUI_SCALE multiplies the OS's DPI
 *        by a number from 0.25 to 8, and anything else leaves the OS's scale as it is.
 */

#include "ui/window/scalefactorof.h"

#include <gtest/gtest.h>

using Ui::Window::scaleFactorOf;

TEST(ScaleFactorOf, ANumberIsTheFactor)
{
    EXPECT_FLOAT_EQ(scaleFactorOf("1.25"), 1.25F);
    EXPECT_FLOAT_EQ(scaleFactorOf("2"), 2.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("0.5"), 0.5F);
    EXPECT_FLOAT_EQ(scaleFactorOf("0.25"), 0.25F);
    EXPECT_FLOAT_EQ(scaleFactorOf("8"), 8.0F);
}

// Unset, the environment reads empty
TEST(ScaleFactorOf, UnsetLeavesTheOsScale) { EXPECT_FLOAT_EQ(scaleFactorOf(""), 1.0F); }

// Negative: a value that is no factor leaves the OS's scale rather than breaking the UI
TEST(ScaleFactorOf, NoFactorLeavesTheOsScale)
{
    EXPECT_FLOAT_EQ(scaleFactorOf("large"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("0"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("-1.5"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("inf"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("nan"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("1.5x"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("2 px"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("2,5"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("2%"), 1.0F);
}

// Negative: too small rounds the DPI to 0, which every toCss then divides by, and too large
// overflows the DPI's cast
TEST(ScaleFactorOf, AFactorOutOfRangeLeavesTheOsScale)
{
    EXPECT_FLOAT_EQ(scaleFactorOf("0.001"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("1000"), 1.0F);
    EXPECT_FLOAT_EQ(scaleFactorOf("1e30"), 1.0F);
}
