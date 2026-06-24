// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureCreator. PureCreator is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureCreator is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureCreator. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestTextAlign.cpp
 * @brief Unit tests for Render::TextAlign helpers
 *
 * The four helpers translate a CSS-space (box, textWidth, padding) into a
 * physical-px startX. They must:
 *   - multiply the final CSS expression by `scale`
 *   - never drop below box.x + minPadding for the clamped variant
 *   - return finite values for zero-scale and zero-width inputs
 */

#include "ui/backend/gl/textalign.h"

#include <gtest/gtest.h>

using Ui::fpx_t;
using Ui::Backend::Gl::TextAlign::startXCenter;
using Ui::Backend::Gl::TextAlign::startXCenterClamped;
using Ui::Backend::Gl::TextAlign::startXLeft;
using Ui::Backend::Gl::TextAlign::startXRight;
using Ui::Res::Type::bound_t;

// ============================================================================
// startXLeft
// ============================================================================

TEST(TextAlign, Left_BasicScaleAndPadding)
{
    // (10 + 16) * 2 = 52
    const bound_t box { 10.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 16.0F, 2.0F), 52.0F);
}

TEST(TextAlign, Left_ZeroPadding)
{
    const bound_t box { 25.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 0.0F, 2.0F), 50.0F);
}

TEST(TextAlign, Left_ZeroBoxX)
{
    const bound_t box { 0.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 12.0F, 2.0F), 24.0F);
}

TEST(TextAlign, Left_Scale1x)
{
    const bound_t box { 10.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 16.0F, 1.0F), 26.0F);
}

TEST(TextAlign, Left_ZeroScale)
{
    const bound_t box { 10.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 16.0F, 0.0F), 0.0F);
}

TEST(TextAlign, Left_FractionalCssPosition)
{
    // (32.5 + 16) * 2 = 97.0
    const bound_t box { 32.5F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXLeft(box, 16.0F, 2.0F), 97.0F);
}

// ============================================================================
// startXRight
// ============================================================================

TEST(TextAlign, Right_BasicShortcutText)
{
    // box.x=0, w=192 (popup width), padH=16, textW=42 ("Ctrl+R" approx).
    // (0 + 192 - 16 - 42) * 2 = 268
    const bound_t box { 0.0F, 0.0F, 192.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXRight(box, 42.0F, 16.0F, 2.0F), 268.0F);
}

TEST(TextAlign, Right_TextWiderThanBox_ProducesNegativeStart)
{
    // textWidth (200) > box.w (100). Result is negative; helper does not clamp.
    // (0 + 100 - 0 - 200) * 2 = -200
    const bound_t box { 0.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXRight(box, 200.0F, 0.0F, 2.0F), -200.0F);
}

TEST(TextAlign, Right_ZeroTextWidth_RightEdgeMinusPadding)
{
    const bound_t box { 0.0F, 0.0F, 192.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXRight(box, 0.0F, 16.0F, 2.0F), 352.0F);
}

TEST(TextAlign, Right_ZeroPadding_RightEdgeMinusText)
{
    const bound_t box { 0.0F, 0.0F, 192.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXRight(box, 42.0F, 0.0F, 2.0F), 300.0F);
}

TEST(TextAlign, Right_ZeroScale)
{
    const bound_t box { 10.0F, 0.0F, 100.0F, 28.0F };
    EXPECT_FLOAT_EQ(startXRight(box, 30.0F, 8.0F, 0.0F), 0.0F);
}

// ============================================================================
// startXCenter
// ============================================================================

TEST(TextAlign, Center_BasicTitle)
{
    // box.x=0, w=300, textW=80. (0 + (300-80)/2) * 2 = 220
    const bound_t box { 0.0F, 0.0F, 300.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 80.0F, 2.0F), 220.0F);
}

TEST(TextAlign, Center_NonZeroBoxX_OffsetIncluded)
{
    // box.x=50, w=200, textW=40. (50 + (200-40)/2) * 2 = 260
    const bound_t box { 50.0F, 0.0F, 200.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 40.0F, 2.0F), 260.0F);
}

TEST(TextAlign, Center_TextWidthZero_CentersBoxLeftEdge)
{
    // (0 + 200/2) * 2 = 200 (i.e. box center)
    const bound_t box { 0.0F, 0.0F, 200.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 0.0F, 2.0F), 200.0F);
}

TEST(TextAlign, Center_TextWiderThanBox_NegativeSlackPullsLeft)
{
    // textW=300 vs w=200. (0 + (200-300)/2) * 2 = -100. No clamping by design.
    const bound_t box { 0.0F, 0.0F, 200.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 300.0F, 2.0F), -100.0F);
}

TEST(TextAlign, Center_OddSlack_HalvesFractionally)
{
    // (200 - 41)/2 = 79.5. (0 + 79.5) * 2 = 159
    const bound_t box { 0.0F, 0.0F, 200.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 41.0F, 2.0F), 159.0F);
}

TEST(TextAlign, Center_Scale1x)
{
    const bound_t box { 0.0F, 0.0F, 300.0F, 24.0F };
    EXPECT_FLOAT_EQ(startXCenter(box, 80.0F, 1.0F), 110.0F);
}

// ============================================================================
// startXCenterClamped
// ============================================================================

TEST(TextAlign, CenterClamped_FitsCenters)
{
    // textW (80) fits in box (300) with room: behaves like startXCenter.
    const bound_t box { 0.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 80.0F, 8.0F, 2.0F), 220.0F);
}

TEST(TextAlign, CenterClamped_TextTooWide_ClampsToMinPad)
{
    // textW (290) leaves only 5 px slack each side; minPaddingH=8 forces clamp.
    // centered = (0 + (300-290)/2) * 2 = 10
    // minX     = (0 + 8) * 2 = 16
    // result must be 16 (clamped, NOT 10).
    const bound_t box { 0.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 290.0F, 8.0F, 2.0F), 16.0F);
}

TEST(TextAlign, CenterClamped_ExactFitAtMinPad_ReturnsMinXPath)
{
    // centered slack equals minPadding exactly; either branch yields same value.
    // (300-284)/2 = 8 == minPad. centered = 16, minX = 16.
    const bound_t box { 0.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 284.0F, 8.0F, 2.0F), 16.0F);
}

TEST(TextAlign, CenterClamped_NonZeroBoxX_ClampUsesBoxX)
{
    // Box offset by 50 css. minPadding kicks in -> startX = (50 + 8) * 2 = 116.
    const bound_t box { 50.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 290.0F, 8.0F, 2.0F), 116.0F);
}

TEST(TextAlign, CenterClamped_ZeroMinPad_NeverClamps)
{
    // minPaddingH=0 means clamp threshold == box.x; centered branch always wins.
    const bound_t box { 0.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 290.0F, 0.0F, 2.0F), 10.0F);
}

TEST(TextAlign, CenterClamped_NeverDropsBelowMinX)
{
    // Property: result is always >= (box.x + minPaddingH) * scale.
    const bound_t box { 0.0F, 0.0F, 100.0F, 30.0F };
    for (fpx_t textW = 0.0F; textW < 200.0F; textW += 7.5F) {
        const fpx_t result = startXCenterClamped(box, textW, 8.0F, 2.0F);
        EXPECT_GE(result, 16.0F) << "textW=" << textW;
    }
}

TEST(TextAlign, CenterClamped_ZeroScale)
{
    const bound_t box { 0.0F, 0.0F, 300.0F, 30.0F };
    EXPECT_FLOAT_EQ(startXCenterClamped(box, 80.0F, 8.0F, 0.0F), 0.0F);
}
