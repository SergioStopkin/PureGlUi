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
 * @file TestBit.cpp
 * @brief Unit tests for Common::Bit - the bitflag helpers the res loaders use to
 *        combine Changed flags. Covers Or/And/Xor/Shl/Shr on both plain ints and
 *        the Changed enum (the actual mixed enum/int use in the stores).
 */

#include "common/bit.h"
#include "ui/res/type/changed.h"

#include <cstdint>
#include <gtest/gtest.h>

using Common::Bit;
using Ui::Res::Type::Changed;

TEST(Bit, OrAndXorInts)
{
    EXPECT_EQ(Bit::Or(1, 2), 3U);
    EXPECT_EQ(Bit::And(6, 3), 2U);
    EXPECT_EQ(Bit::Xor(5, 3), 6U);
}

TEST(Bit, ShiftInts)
{
    EXPECT_EQ(Bit::Shl(1, 4), 16U);
    EXPECT_EQ(Bit::Shr(16, 2), 4U);
    EXPECT_EQ(Bit::Shl(0, 3), 0U);
}

TEST(Bit, OrCombinesChangedFlags)
{
    // Layout = 0x0004, Popup = 0x0008 -> 0x000C. This is exactly how LayoutStore
    // aggregates the two bits it derives from one parse.
    const auto combined = Bit::Or(Changed::Layout, Changed::Popup);
    EXPECT_EQ(combined, static_cast<std::uint16_t>(0x000C));
    // Casting back through Changed keeps both bits set.
    const auto asChanged = static_cast<Changed>(combined);
    EXPECT_EQ(Bit::And(asChanged, Changed::Layout), static_cast<std::uint16_t>(Changed::Layout));
    EXPECT_EQ(Bit::And(asChanged, Changed::Popup), static_cast<std::uint16_t>(Changed::Popup));
}

TEST(Bit, AndDetectsAbsentFlag)
{
    const auto only = Bit::Or(Changed::Theme, Changed::None);
    EXPECT_EQ(Bit::And(static_cast<Changed>(only), Changed::Layout), 0U);
    EXPECT_NE(Bit::And(static_cast<Changed>(only), Changed::Theme), 0U);
}
