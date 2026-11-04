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
 * @file TestConfig.cpp
 * @brief Unit tests for Ui::config_t - the fw's CSS <-> physical pixel math,
 *        decoupled from any process global. Verifies scale conversions and the
 *        floor/round modes that the renderer and popup layout depend on.
 */

#include "ui/config.h"
#include "ui/type.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>

using Ui::config_t;

namespace {

// Every scale an edge has to hold at: the fractional ones are what toPhysEdge is for, and CI
// only ever runs 1x and 1.25x
constexpr std::array SCALES { 1.0F, 1.25F, 1.5F, 1.75F, 2.0F, 2.5F, 3.0F };

// CSS px either way of the window's corner, in half steps: a value off the window, and one
// that is not whole, are both handed to it
constexpr int MOST_HALF_STEPS = 1600;

[[nodiscard]] config_t configAt(Ui::fpx_t scale) { return config_t { Ui::roundToInt(96.0F * scale), scale, false }; }

} // namespace

TEST(Config, Defaults)
{
    const config_t cfg;
    EXPECT_EQ(cfg.dpi, 96);
    EXPECT_FLOAT_EQ(cfg.scale, 1.0F);
    EXPECT_FALSE(cfg.isCompositing);
}

TEST(Config, IdentityAtScaleOne)
{
    const config_t cfg; // scale 1.0
    EXPECT_FLOAT_EQ(cfg.toPhys(10), 10.0F);
    EXPECT_FLOAT_EQ(cfg.toCss(10), 10.0F);
    EXPECT_EQ(cfg.toPhysFloor(10), 10);
    EXPECT_EQ(cfg.toCssFloor(10), 10);
}

TEST(Config, ScalesAtTwoX)
{
    const config_t cfg { 192, 2.0F, false };
    EXPECT_FLOAT_EQ(cfg.toPhys(10), 20.0F);
    EXPECT_EQ(cfg.toPhysFloor(10), 20);
    EXPECT_FLOAT_EQ(cfg.toCss(20), 10.0F);
    EXPECT_EQ(cfg.toCssFloor(20), 10);
}

TEST(Config, FloorTruncatesFraction)
{
    const config_t cfg { 144, 1.5F, false };
    // 3 * 1.5 = 4.5 -> floor 4
    EXPECT_EQ(cfg.toPhysFloor(3), 4);
    // 7 / 1.5 = 4.66 -> floor 4
    EXPECT_EQ(cfg.toCssFloor(7), 4);
}

TEST(Config, RoundRoundsCssBeforeScaling)
{
    const config_t cfg { 192, 2.0F, false };
    // round(2.4) = 2 -> 2 * 2 = 4
    EXPECT_FLOAT_EQ(cfg.toPhysRound(2.4F), 4.0F);
    // round(2.6) = 3 -> 3 * 2 = 6
    EXPECT_FLOAT_EQ(cfg.toPhysRound(2.6F), 6.0F);
    // int overload scales directly: 3 * 2 = 6
    EXPECT_FLOAT_EQ(cfg.toPhysRound(3), 6.0F);
}

// GL fills a pixel whose centre is inside the edge, so one on a fraction starts on the first
// centre past it, and one exactly on a centre starts there
TEST(Config, AnEdgeAtAFractionalScaleIsThePixelGlDrawsItFrom)
{
    const config_t at125 = configAt(1.25F);
    EXPECT_FLOAT_EQ(at125.toPhysEdge(1.0F), 1.0F);   // 1.25
    EXPECT_FLOAT_EQ(at125.toPhysEdge(2.0F), 2.0F);   // 2.5, on a centre
    EXPECT_FLOAT_EQ(at125.toPhysEdge(3.0F), 4.0F);   // 3.75
    EXPECT_FLOAT_EQ(at125.toPhysEdge(10.0F), 12.0F); // 12.5, on a centre

    const config_t at150 = configAt(1.5F);
    EXPECT_FLOAT_EQ(at150.toPhysEdge(1.0F), 1.0F); // 1.5, on a centre
    EXPECT_FLOAT_EQ(at150.toPhysEdge(2.0F), 3.0F); // 3
    EXPECT_FLOAT_EQ(at150.toPhysEdge(3.0F), 4.0F); // 4.5, on a centre

    const config_t at175 = configAt(1.75F);
    EXPECT_FLOAT_EQ(at175.toPhysEdge(1.0F), 2.0F); // 1.75
    EXPECT_FLOAT_EQ(at175.toPhysEdge(2.0F), 3.0F); // 3.5, on a centre
    EXPECT_FLOAT_EQ(at175.toPhysEdge(4.0F), 7.0F); // 7

    const config_t at250 = configAt(2.5F);
    EXPECT_FLOAT_EQ(at250.toPhysEdge(1.0F), 2.0F); // 2.5, on a centre
    EXPECT_FLOAT_EQ(at250.toPhysEdge(3.0F), 7.0F); // 7.5, on a centre
}

// A whole scale leaves nothing to decide
TEST(Config, AnEdgeAtAWholeScaleIsItsRoundedPosition)
{
    for (const Ui::fpx_t scale : { 1.0F, 2.0F, 3.0F }) {
        const config_t cfg = configAt(scale);
        for (int css = -20; css <= 200; ++css) {
            const auto value = static_cast<Ui::fpx_t>(css);
            EXPECT_FLOAT_EQ(cfg.toPhysEdge(value), cfg.toPhysRound(value)) << "at " << scale << "x, " << css;
        }
    }
}

TEST(Config, AnEdgeMakesItsCssValueWholeFirst)
{
    const config_t at200 = configAt(2.0F);
    EXPECT_FLOAT_EQ(at200.toPhysEdge(1.5F), 4.0F); // round(1.5) = 2
    EXPECT_FLOAT_EQ(at200.toPhysEdge(2.4F), 4.0F); // round(2.4) = 2
    EXPECT_FLOAT_EQ(at200.toPhysEdge(2.6F), 6.0F); // round(2.6) = 3

    const config_t at125 = configAt(1.25F);
    EXPECT_FLOAT_EQ(at125.toPhysEdge(9.6F), 12.0F);  // round(9.6) = 10
    EXPECT_FLOAT_EQ(at125.toPhysEdge(10.4F), 12.0F); // round(10.4) = 10
}

// Negative: the window's corner stays where it is at any scale
TEST(Config, TheWindowsCornerIsItsOwnEdge)
{
    for (const Ui::fpx_t scale : SCALES) {
        EXPECT_FLOAT_EQ(configAt(scale).toPhysEdge(0.0F), 0.0F) << "at " << scale << "x";
    }
}

// Negative: a list or a dialog can start off the window's top or left
TEST(Config, AnEdgeOffTheWindowIsStillThePixelGlDrawsItFrom)
{
    EXPECT_FLOAT_EQ(configAt(1.25F).toPhysEdge(-1.0F), -1.0F); // -1.25
    EXPECT_FLOAT_EQ(configAt(1.5F).toPhysEdge(-1.0F), -2.0F);  // -1.5, on a centre
    EXPECT_FLOAT_EQ(configAt(2.0F).toPhysEdge(-1.0F), -2.0F);  // -2
    EXPECT_FLOAT_EQ(configAt(2.0F).toPhysEdge(-1.5F), -4.0F);  // round(-1.5) = -2
}

// Whatever the scale or the value: a whole pixel, the nearest one or the half-way one past it,
// so a platform has no fraction left to cut off
TEST(Config, AnEdgeIsAlwaysAWholePixelWithinHalfOfItsPosition)
{
    for (const Ui::fpx_t scale : SCALES) {
        const config_t cfg = configAt(scale);
        for (int step = -MOST_HALF_STEPS; step <= MOST_HALF_STEPS; ++step) {
            const Ui::fpx_t css  = static_cast<Ui::fpx_t>(step) / 2.0F;
            const Ui::fpx_t edge = cfg.toPhysEdge(css);
            EXPECT_FLOAT_EQ(edge, std::floor(edge)) << "at " << scale << "x, " << css;
            EXPECT_LE(std::abs(edge - cfg.toPhysRound(css)), 0.5F) << "at " << scale << "x, " << css;
        }
    }
}

// Two things side by side never overlap, nor swap: at 1x and up each CSS px moves an edge on
TEST(Config, AnEdgeMovesOnWithEveryCssPx)
{
    for (const Ui::fpx_t scale : SCALES) {
        const config_t cfg = configAt(scale);
        for (int css = -MOST_HALF_STEPS / 2; css < MOST_HALF_STEPS / 2; ++css) {
            EXPECT_GT(cfg.toPhysEdge(static_cast<Ui::fpx_t>(css + 1)), cfg.toPhysEdge(static_cast<Ui::fpx_t>(css)))
            << "at " << scale << "x, " << css;
        }
    }
}

// What a dialog takes as its size, the pixels between its two edges: whole, and within a pixel
// of its size however its corner fell
TEST(Config, TheSpanBetweenTwoEdgesIsWithinAPixelOfItsSize)
{
    constexpr Ui::fpx_t WIDTH = 300.0F;
    for (const Ui::fpx_t scale : SCALES) {
        const config_t cfg = configAt(scale);
        for (int css = -100; css <= 400; ++css) {
            const auto      left = static_cast<Ui::fpx_t>(css);
            const Ui::fpx_t span = cfg.toPhysEdge(left + WIDTH) - cfg.toPhysEdge(left);
            EXPECT_FLOAT_EQ(span, std::floor(span)) << "at " << scale << "x, " << css;
            EXPECT_LE(std::abs(span - (WIDTH * scale)), 1.0F) << "at " << scale << "x, " << css;
        }
    }
}

// The free function every caller uses reads the scale the process runs at
TEST(Config, TheFreeEdgeReadsTheProcessScale)
{
    const config_t before = Ui::g_config;
    Ui::g_config          = configAt(1.25F);
    const Ui::fpx_t edge  = Ui::toPhysEdge(10.0F);
    Ui::g_config          = before;
    EXPECT_FLOAT_EQ(edge, 12.0F);
}
