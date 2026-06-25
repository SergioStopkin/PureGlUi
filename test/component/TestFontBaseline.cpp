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
 * @file TestFontBaseline.cpp
 * @brief Unit tests for font_metrics_t::baseline() and baselineCap()
 *
 * baseline()    centers the full font line box (ascent + descent) in the CSS
 *               box - correct for strings that may contain descenders.
 * baselineCap() centers the cap block (~ (ascent + x_height) / 2) in the CSS
 *               box - matches CSS `text-box-edge: cap alphabetic`. Lowercase
 *               ascenders and descenders are allowed to extend outside.
 *
 * Both must:
 *   - return std::floor((... ) * scale) so output is integer physical px
 *   - degrade gracefully on edge inputs (zero scale, zero box, negative cssY)
 */

#include "ui/gl/fonttypes.h"

#include <gtest/gtest.h>

using Ui::fpx_t;
using Ui::Gl::font_metrics_t;

// Real Nunito Regular @ 14 px CSS, captured from FreeType at runtime.
static font_metrics_t nunito14()
{
    font_metrics_t m {};
    m.ascent      = 14;
    m.descent     = 5;
    m.height      = m.ascent + m.descent;
    m.x_height    = 7;
    m.draw_spaces = true;
    return m;
}

// Real LiberationMono Regular @ 14 px CSS.
static font_metrics_t mono14()
{
    font_metrics_t m {};
    m.ascent      = 12;
    m.descent     = 4;
    m.height      = m.ascent + m.descent;
    m.x_height    = 7;
    m.draw_spaces = true;
    return m;
}

// ============================================================================
// baseline() - line-box centered
// ============================================================================

TEST(FontBaseline, LineBox_PopupItem_Nunito14_Scale2x)
{
    // Popup item: cssY=32.32, cssH=31.32 (matches "Cobalt" runtime values).
    // baseline = floor((32.32 + (31.32 - 19) / 2 + 14) * 2) = floor(104.96) = 104
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(32.32F, 31.32F, 2.0F), 104.0F);
}

TEST(FontBaseline, LineBox_TopMenu_Nunito14_Scale2x)
{
    // Top menu item: cssY=0, cssH=30.
    // baseline = floor((0 + (30 - 19) / 2 + 14) * 2) = floor(39.0) = 39
    // (Runtime log captured 40 due to slight float drift in different inputs;
    // the exact integer-input case here yields 39 deterministically.)
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 30.0F, 2.0F), 39.0F);
}

TEST(FontBaseline, LineBox_StatusBar_Mono14_Scale2x)
{
    // Status bar: cssY=0, cssH=18 (font mono 14, ascent=12, descent=4).
    // baseline = floor((0 + (18 - 16) / 2 + 12) * 2) = floor(26.0) = 26
    const auto m = mono14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 18.0F, 2.0F), 26.0F);
}

TEST(FontBaseline, LineBox_Scale1x_HalfDpi)
{
    // 1x scale: physical px == CSS px. baseline = floor(0 + (32-19)/2 + 14) = floor(20.5) = 20
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 32.0F, 1.0F), 20.0F);
}

TEST(FontBaseline, LineBox_Scale3x_RetinaXL)
{
    // 3x scale path: floor((0 + (32 - 19)/2 + 14) * 3) = floor(61.5) = 61
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 32.0F, 3.0F), 61.0F);
}

TEST(FontBaseline, LineBox_BoxShorterThanFont_NoCrashNegativeAdjustment)
{
    // Pathological: cssH (10) < height (19). Inner = 0 + (10-19)/2 + 14 = 9.5.
    // baseline = floor(9.5 * 2) = 19. Function must not assert/crash.
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 10.0F, 2.0F), 19.0F);
}

TEST(FontBaseline, LineBox_ZeroScale_ReturnsZero)
{
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(20.0F, 30.0F, 0.0F), 0.0F);
}

TEST(FontBaseline, LineBox_ZeroBox_StillFinite)
{
    // cssH=0: baseline = floor((0 + (0-19)/2 + 14) * 2) = floor(9.0) = 9
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 0.0F, 2.0F), 9.0F);
}

TEST(FontBaseline, LineBox_ZeroMetrics_ReturnsScaledTop)
{
    // Empty metrics (height=0, ascent=0): baseline = floor((cssY + cssH/2) * scale).
    // For cssY=0, cssH=20, scale=2: floor(20.0) = 20.
    font_metrics_t m {};
    EXPECT_FLOAT_EQ(m.baseline(0.0F, 20.0F, 2.0F), 20.0F);
}

TEST(FontBaseline, LineBox_Idempotent_SameInputsSameOutput)
{
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baseline(32.32F, 31.32F, 2.0F), m.baseline(32.32F, 31.32F, 2.0F));
}

TEST(FontBaseline, LineBox_OutputIsInteger_FlooredAfterScale)
{
    // The result must be std::floor(...) of (... * scale), so always integer-valued.
    const auto  m   = nunito14();
    const fpx_t out = m.baseline(7.5F, 23.5F, 2.0F);
    EXPECT_FLOAT_EQ(out, std::floor(out));
}

// ============================================================================
// baselineCap() - cap-height centered
// ============================================================================

TEST(FontBaseline, Cap_PopupItem_Nunito14_Scale2x)
{
    // capHeight = (14 + 7) / 2 = 10.5
    // baseline = floor((32.32 + (31.32 + 10.5) / 2) * 2) = floor((32.32 + 20.91) * 2)
    //         = floor(106.46) = 106
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(32.32F, 31.32F, 2.0F), 106.0F);
}

TEST(FontBaseline, Cap_PopupItem_Emerald_Nunito14_Scale2x)
{
    // "Emerald" item at cssY=63.64, cssH=31.32.
    // baseline = floor((63.64 + (31.32 + 10.5) / 2) * 2) = floor(169.1) = 169
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(63.64F, 31.32F, 2.0F), 169.0F);
}

TEST(FontBaseline, Cap_ThemeSwatch_Mono14_Scale2x)
{
    // Theme swatch box: cssY=0, cssH=20, mono14 (ascent=12, x_height=7 -> capH=9.5).
    // baseline = floor((0 + (20 + 9.5) / 2) * 2) = floor(29.5) = 29
    const auto m = mono14();
    EXPECT_FLOAT_EQ(m.baselineCap(0.0F, 20.0F, 2.0F), 29.0F);
}

TEST(FontBaseline, Cap_TopMenu_Nunito14_Scale2x)
{
    // cssY=0, cssH=30. baseline = floor((0 + (30 + 10.5) / 2) * 2) = floor(40.5) = 40
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(0.0F, 30.0F, 2.0F), 40.0F);
}

TEST(FontBaseline, Cap_BaselineBelowLineBox_NoDescentReservation)
{
    // For typical metrics where descent > 0, baselineCap drops the baseline
    // *lower* (larger Y) than baseline() because it doesn't reserve descent
    // space at the bottom of the box. The cap block centers symmetrically
    // instead of being pushed up by reserved descender slack.
    const auto m = nunito14();
    EXPECT_GT(m.baselineCap(32.32F, 31.32F, 2.0F), m.baseline(32.32F, 31.32F, 2.0F));
}

TEST(FontBaseline, Cap_Scale1x_HalfDpi)
{
    // 1x scale: floor(0 + (30 + 10.5)/2) = floor(20.25) = 20
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(0.0F, 30.0F, 1.0F), 20.0F);
}

TEST(FontBaseline, Cap_ZeroScale_ReturnsZero)
{
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(20.0F, 30.0F, 0.0F), 0.0F);
}

TEST(FontBaseline, Cap_ZeroBox_StillFinite)
{
    // cssH=0: baseline = floor((0 + (0+10.5)/2) * 2) = floor(10.5) = 10
    const auto m = nunito14();
    EXPECT_FLOAT_EQ(m.baselineCap(0.0F, 0.0F, 2.0F), 10.0F);
}

TEST(FontBaseline, Cap_ZeroMetrics_DegradesToBoxCenter)
{
    // ascent=0, x_height=0 => capHeight=0; baseline = floor((cssY + cssH/2) * scale).
    // Same as line-box with zero metrics: cssY=0, cssH=20, scale=2 => 20.
    font_metrics_t m {};
    EXPECT_FLOAT_EQ(m.baselineCap(0.0F, 20.0F, 2.0F), 20.0F);
}

TEST(FontBaseline, Cap_OutputIsInteger_FlooredAfterScale)
{
    const auto  m   = nunito14();
    const fpx_t out = m.baselineCap(7.5F, 23.5F, 2.0F);
    EXPECT_FLOAT_EQ(out, std::floor(out));
}

TEST(FontBaseline, Cap_VisualMiddleNearBoxMiddle_Emerald)
{
    // Sanity check that "Emerald"-style box centers the cap visual middle
    // close to the box middle. Cap top = baseline - capHeight (in physical px).
    // box_middle_phys = (63.64 + 31.32/2) * 2 = 158.6
    // baseline = 169
    // capHeight_phys = 10.5 * 2 = 21
    // cap_top = 169 - 21 = 148
    // cap_middle = (148 + 169) / 2 = 158.5
    // Off by 0.1 phys px. Tolerance: <= 1 phys px.
    const auto  m             = nunito14();
    const fpx_t baseline      = m.baselineCap(63.64F, 31.32F, 2.0F);
    const fpx_t capHeightPhys = (m.ascent + m.x_height) / 2.0F * 2.0F;
    const fpx_t capMiddle     = baseline - capHeightPhys / 2.0F;
    const fpx_t boxMiddle     = (63.64F + 31.32F / 2.0F) * 2.0F;
    EXPECT_NEAR(capMiddle, boxMiddle, 1.0F);
}
