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
 * @file TestConvert.cpp
 * @brief Unit tests for Ui::Convert - the CSS-string -> number parsing backbone
 *        the layout and theme stores rely on. Covers unit handling (px/%/vw/vh),
 *        malformed-input fallbacks, and the border-radius 1/2/3/4-value
 *        shorthand expansion.
 */

#include "ui/convert.h"

#include <gtest/gtest.h>

using Ui::Convert;
using Ui::Res::Type::border_t;

TEST(Convert, Str2IntUnits)
{
    EXPECT_EQ(Convert::str2int("48px"), 48);
    EXPECT_EQ(Convert::str2int("100"), 100);
    EXPECT_EQ(Convert::str2int("  12px "), 12); // whitespace stripped
    EXPECT_EQ(Convert::str2int("0"), 0);
}

TEST(Convert, Str2IntPercentAndViewport)
{
    // No ref: the raw number is returned.
    EXPECT_EQ(Convert::str2int("50%"), 50);
    EXPECT_EQ(Convert::str2int("100vw"), 100);
    EXPECT_EQ(Convert::str2int("100vh"), 100);
    // With ref: percent/viewport resolve against it.
    EXPECT_EQ(Convert::str2int("50%", 0, 200), 100);
    EXPECT_EQ(Convert::str2int("25vw", 0, 800), 200);
    EXPECT_EQ(Convert::str2int("50vh", 0, 600), 300);
}

TEST(Convert, Str2IntEmptyAndInvalidReturnZero)
{
    EXPECT_EQ(Convert::str2int(""), 0);
    EXPECT_EQ(Convert::str2int("   "), 0);
    EXPECT_EQ(Convert::str2int("garbage"), 0);
}

TEST(Convert, Str2FpxDelegates)
{
    EXPECT_FLOAT_EQ(Convert::str2fpx("14px"), 14.0F);
    EXPECT_FLOAT_EQ(Convert::str2fpx(""), 0.0F);
}

TEST(Convert, Str2Uint32Units)
{
    EXPECT_EQ(Convert::str2uint32("32px"), 32U);
    EXPECT_EQ(Convert::str2uint32("10%", 0, 300U), 30U);
    EXPECT_EQ(Convert::str2uint32(""), 0U);
    EXPECT_EQ(Convert::str2uint32("bad"), 0U);
}

TEST(Convert, ParseCssNumber)
{
    EXPECT_FLOAT_EQ(Convert::parseCssNumber("0.15"), 0.15F);
    EXPECT_FLOAT_EQ(Convert::parseCssNumber("1.38"), 1.38F);
    EXPECT_FLOAT_EQ(Convert::parseCssNumber("45"), 45.0F);
    EXPECT_FLOAT_EQ(Convert::parseCssNumber(""), 0.0F);     // fallback
    EXPECT_FLOAT_EQ(Convert::parseCssNumber("junk"), 0.0F); // fallback
}

TEST(Convert, ParseCssInt)
{
    EXPECT_EQ(Convert::parseCssInt("14px"), 14); // stoi stops at the unit
    EXPECT_EQ(Convert::parseCssInt("400"), 400);
    EXPECT_EQ(Convert::parseCssInt(""), 0);
    EXPECT_EQ(Convert::parseCssInt("bad"), 0);
}

TEST(Convert, BorderRadiusOneValueAllCorners)
{
    const border_t b = Convert::parseCssBorderRadius("6");
    EXPECT_FLOAT_EQ(b.topLeft, 6.0F);
    EXPECT_FLOAT_EQ(b.topRight, 6.0F);
    EXPECT_FLOAT_EQ(b.bottomRight, 6.0F);
    EXPECT_FLOAT_EQ(b.bottomLeft, 6.0F);
}

TEST(Convert, BorderRadiusTwoValues)
{
    // top-left+bottom-right = v0 ; top-right+bottom-left = v1
    const border_t b = Convert::parseCssBorderRadius("2 8");
    EXPECT_FLOAT_EQ(b.topLeft, 2.0F);
    EXPECT_FLOAT_EQ(b.topRight, 8.0F);
    EXPECT_FLOAT_EQ(b.bottomRight, 2.0F);
    EXPECT_FLOAT_EQ(b.bottomLeft, 8.0F);
}

TEST(Convert, BorderRadiusThreeValues)
{
    // v0, v1 (top-right+bottom-left), v2
    const border_t b = Convert::parseCssBorderRadius("1 2 3");
    EXPECT_FLOAT_EQ(b.topLeft, 1.0F);
    EXPECT_FLOAT_EQ(b.topRight, 2.0F);
    EXPECT_FLOAT_EQ(b.bottomRight, 3.0F);
    EXPECT_FLOAT_EQ(b.bottomLeft, 2.0F);
}

TEST(Convert, BorderRadiusFourValues)
{
    const border_t b = Convert::parseCssBorderRadius("1 2 3 4");
    EXPECT_FLOAT_EQ(b.topLeft, 1.0F);
    EXPECT_FLOAT_EQ(b.topRight, 2.0F);
    EXPECT_FLOAT_EQ(b.bottomRight, 3.0F);
    EXPECT_FLOAT_EQ(b.bottomLeft, 4.0F);
}

TEST(Convert, BorderRadiusEmptyIsZero)
{
    const border_t b = Convert::parseCssBorderRadius("");
    EXPECT_TRUE((b == border_t {}));
    EXPECT_FALSE(b.anyNonZero());
}
