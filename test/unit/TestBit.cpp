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
 * @brief Unit tests for Common::Bit - the bit helpers for platform masks (GL, X11,
 *        EGL) and the hash. Covers Or/And/Xor/Shl/Shr on plain ints; an enum's
 *        flags combine through Common::flags_t instead (TestFlags.cpp).
 */

#include "common/bit.h"

#include <gtest/gtest.h>

using Common::Bit;

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
