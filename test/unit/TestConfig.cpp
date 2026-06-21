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

#include <gtest/gtest.h>

using Ui::config_t;

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
