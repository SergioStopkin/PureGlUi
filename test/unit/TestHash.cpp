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
 * @file TestHash.cpp
 * @brief Unit tests for Common::hashOf - the one hash_combine every
 *        multi-field key hashes through (border_t, font_t, the SVG cache key)
 */

#include "common/hash.h"
#include "ui/res/type/border.h"
#include "ui/res/type/font.h"

#include <functional>
#include <gtest/gtest.h>

TEST(Hash, OneValueIsItsOwnHash) { EXPECT_EQ(Common::hashOf(42), std::hash<int> {}(42)); }

TEST(Hash, OrderMatters) { EXPECT_NE(Common::hashOf(1, 2), Common::hashOf(2, 1)); }

TEST(Hash, EveryValueCounts)
{
    EXPECT_NE(Common::hashOf(7, 7), Common::hashOf(7));
    EXPECT_NE(Common::hashOf(1, 2, 0), Common::hashOf(1, 2));
}

TEST(Hash, BorderHashesItsCornersInOrder)
{
    const Ui::Res::Type::border_t border { 1.0F, 2.0F, 3.0F, 4.0F };
    const Ui::Res::Type::border_t swapped { 2.0F, 1.0F, 3.0F, 4.0F };
    EXPECT_EQ(std::hash<Ui::Res::Type::border_t> {}(border), Common::hashOf(1.0F, 2.0F, 3.0F, 4.0F));
    EXPECT_NE(std::hash<Ui::Res::Type::border_t> {}(border), std::hash<Ui::Res::Type::border_t> {}(swapped));
}

TEST(Hash, FontHashesEveryField)
{
    // A hand fold of hash(family) ^ (size << 1) ^ (weight << 2) hashes these two alike
    const Ui::Res::Type::font_t regular { "Sans", 2, Ui::Res::Type::FontWeight::Regular };
    const Ui::Res::Type::font_t bold { "Sans", 0, Ui::Res::Type::FontWeight::Bold };
    EXPECT_EQ(std::hash<Ui::Res::Type::font_t> {}(regular),
              Common::hashOf(regular.family, regular.size, regular.weight));
    EXPECT_NE(std::hash<Ui::Res::Type::font_t> {}(regular), std::hash<Ui::Res::Type::font_t> {}(bold));
}
