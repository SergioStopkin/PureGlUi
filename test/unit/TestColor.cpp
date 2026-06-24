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
 * @file TestColor.cpp
 * @brief Unit tests for Ui::Color::Lighter() and Ui::Color::Darker() HSL-based methods
 *
 * Verifies that adjusting lightness in HSL space preserves hue and saturation.
 * Each unordered_map maps lightness% -> expected hex color for a given base HSL.
 */

#include "ui/color.h"

#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

using Ui::Color;

// hsl(240, 20%, L%) - base at 50% = rgb(102, 102, 153) = #666699
static const std::unordered_map<int, uint32_t> hsl240s20 = {
    { 100, 0xffffff }, { 95, 0xf0f0f5 }, { 90, 0xe0e0eb }, { 85, 0xd1d1e0 }, { 80, 0xc2c2d6 }, { 75, 0xb3b3cc },
    { 70, 0xa3a3c2 },  { 65, 0x9494b8 }, { 60, 0x8585ad }, { 55, 0x7575a3 }, { 50, 0x666699 }, { 45, 0x5c5c8a },
    { 40, 0x52527a },  { 35, 0x47476b }, { 30, 0x3d3d5c }, { 25, 0x33334d }, { 20, 0x29293d }, { 15, 0x1f1f2e },
    { 10, 0x14141f },  { 5, 0x0a0a0f },  { 0, 0x000000 },
};

// hsl(210, 50%, L%) - base at 50% = rgb(64, 128, 191) = #4080bf
static const std::unordered_map<int, uint32_t> hsl210s50 = {
    { 100, 0xffffff }, { 95, 0xecf2f9 }, { 90, 0xd9e6f2 }, { 85, 0xc6d9ec }, { 80, 0xb3cce6 }, { 75, 0x9fbfdf },
    { 70, 0x8cb3d9 },  { 65, 0x79a6d2 }, { 60, 0x6699cc }, { 55, 0x538cc6 }, { 50, 0x4080bf }, { 45, 0x3973ac },
    { 40, 0x336699 },  { 35, 0x2d5986 }, { 30, 0x264d73 }, { 25, 0x204060 }, { 20, 0x19334d }, { 15, 0x132639 },
    { 10, 0x0d1a26 },  { 5, 0x060d13 },  { 0, 0x000000 },
};

// hsl(330, 50%, L%) - base at 50% = rgb(191, 64, 128) = #bf4080
static const std::unordered_map<int, uint32_t> hsl330s50 = {
    { 100, 0xffffff }, { 95, 0xf9ecf2 }, { 90, 0xf2d9e6 }, { 85, 0xecc6d9 }, { 80, 0xe6b3cc }, { 75, 0xdf9fbf },
    { 70, 0xd98cb3 },  { 65, 0xd279a6 }, { 60, 0xcc6699 }, { 55, 0xc6538c }, { 50, 0xbf4080 }, { 45, 0xac3973 },
    { 40, 0x993366 },  { 35, 0x862d59 }, { 30, 0x73264d }, { 25, 0x602040 }, { 20, 0x4d1933 }, { 15, 0x391326 },
    { 10, 0x260d1a },  { 5, 0x13060d },  { 0, 0x000000 },
};

// hsl(180, 20%, L%) - base at 50% = rgb(102, 153, 153) = #669999
static const std::unordered_map<int, uint32_t> hsl180s20 = {
    { 100, 0xffffff }, { 95, 0xf0f5f5 }, { 90, 0xe0ebeb }, { 85, 0xd1e0e0 }, { 80, 0xc2d6d6 }, { 75, 0xb3cccc },
    { 70, 0xa3c2c2 },  { 65, 0x94b8b8 }, { 60, 0x85adad }, { 55, 0x75a3a3 }, { 50, 0x669999 }, { 45, 0x5c8a8a },
    { 40, 0x527a7a },  { 35, 0x476b6b }, { 30, 0x3d5c5c }, { 25, 0x334d4d }, { 20, 0x293d3d }, { 15, 0x1f2e2e },
    { 10, 0x141f1f },  { 5, 0x0a0f0f },  { 0, 0x000000 },
};

// hsl(36, 100%, L%) - base at 50% = rgb(255, 153, 0) = #ff9900
static const std::unordered_map<int, uint32_t> hsl36s100 = {
    { 100, 0xffffff }, { 95, 0xfff5e6 }, { 90, 0xffebcc }, { 85, 0xffe0b3 }, { 80, 0xffd699 }, { 75, 0xffcc80 },
    { 70, 0xffc266 },  { 65, 0xffb84d }, { 60, 0xffad33 }, { 55, 0xffa31a }, { 50, 0xff9900 }, { 45, 0xe68a00 },
    { 40, 0xcc7a00 },  { 35, 0xb36b00 }, { 30, 0x995c00 }, { 25, 0x804d00 }, { 20, 0x663d00 }, { 15, 0x4d2e00 },
    { 10, 0x331f00 },  { 5, 0x1a0f00 },  { 0, 0x000000 },
};

static void testHslTable(const Ui::Color & base, int baseLightness, const std::unordered_map<int, uint32_t> & table)
{
    for (const auto & [lightness, hex] : table) {
        const uint8_t expR = (hex >> 16) & 0xff;
        const uint8_t expG = (hex >> 8) & 0xff;
        const uint8_t expB = hex & 0xff;

        const int       delta  = lightness - baseLightness;
        const Ui::Color result = (delta >= 0) ? base.Lighter(delta) : base.Darker(-delta);

        const std::string label = "L" + std::to_string(lightness) + "%";
        EXPECT_NEAR(result.r(), expR, 1) << label;
        EXPECT_NEAR(result.g(), expG, 1) << label;
        EXPECT_NEAR(result.b(), expB, 1) << label;
    }
}

// hsl(240, 20%, 50%) = #666699
TEST(ColorHsl, Table_H240_S20) { testHslTable(Ui::Color(0x66, 0x66, 0x99), 50, hsl240s20); }

// hsl(210, 50%, 50%) = #4080bf
TEST(ColorHsl, Table_H210_S50) { testHslTable(Ui::Color(0x40, 0x80, 0xbf), 50, hsl210s50); }

// hsl(330, 50%, 50%) = #bf4080
TEST(ColorHsl, Table_H330_S50) { testHslTable(Ui::Color(0xbf, 0x40, 0x80), 50, hsl330s50); }

// hsl(180, 20%, 50%) = #669999
TEST(ColorHsl, Table_H180_S20) { testHslTable(Ui::Color(0x66, 0x99, 0x99), 50, hsl180s20); }

// hsl(36, 100%, 50%) = #ff9900
TEST(ColorHsl, Table_H36_S100) { testHslTable(Ui::Color(0xff, 0x99, 0x00), 50, hsl36s100); }

// -- Edge cases --

TEST(ColorHsl, LighterPreservesAlpha)
{
    const Ui::Color base(102, 102, 153, 128);
    EXPECT_EQ(base.Lighter(10).a(), 128);
}

TEST(ColorHsl, DarkerPreservesAlpha)
{
    const Ui::Color base(102, 102, 153, 128);
    EXPECT_EQ(base.Darker(10).a(), 128);
}

TEST(ColorHsl, DarkerClampsToBlack)
{
    const Ui::Color result = Ui::Color(102, 102, 153).Darker(100);
    EXPECT_EQ(result.r(), 0);
    EXPECT_EQ(result.g(), 0);
    EXPECT_EQ(result.b(), 0);
}

TEST(ColorHsl, LighterClampsToWhite)
{
    const Ui::Color result = Ui::Color(102, 102, 153).Lighter(100);
    EXPECT_EQ(result.r(), 255);
    EXPECT_EQ(result.g(), 255);
    EXPECT_EQ(result.b(), 255);
}

TEST(ColorHsl, GrayscaleLighter)
{
    const Ui::Color result = Ui::Color(102, 102, 102).Lighter(10);
    EXPECT_NEAR(result.r(), 128, 1);
    EXPECT_NEAR(result.g(), 128, 1);
    EXPECT_NEAR(result.b(), 128, 1);
}

TEST(ColorHsl, GrayscaleDarker)
{
    const Ui::Color result = Ui::Color(102, 102, 102).Darker(10);
    EXPECT_NEAR(result.r(), 77, 1);
    EXPECT_NEAR(result.g(), 77, 1);
    EXPECT_NEAR(result.b(), 77, 1);
}
