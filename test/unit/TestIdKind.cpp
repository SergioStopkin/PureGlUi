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
 * @file TestIdKind.cpp
 * @brief Unit tests for ui/idkind.h: a kind byte over a 56-bit serial, read back
 *        whole at both ends of the serial; host kinds told from the framework's;
 *        INVALID_ID its own kind; a dock's grip and scrollbar mapping back to the
 *        dock without meeting each other; the fixed ids all different.
 */

#include "ui/idkind.h"
#include "ui/type.h"

#include <array>
#include <gtest/gtest.h>

using Ui::IdKind;

TEST(IdKindTest, KindAndSerialReadBackAtBothEndsOfTheSerial)
{
    for (const Ui::id_t serial : { Ui::id_t { 0 }, Ui::SERIAL_LAST }) {
        const Ui::id_t id = Ui::idOf(IdKind::MenuItem, serial);
        EXPECT_EQ(Ui::kindOf(id), IdKind::MenuItem);
        EXPECT_EQ(Ui::serialOf(id), serial);
    }
}

// The last serial of one kind is not the first of the next: no range runs into another
TEST(IdKindTest, KindsNeverMeet)
{
    EXPECT_NE(Ui::idOf(IdKind::MenuButton, Ui::SERIAL_LAST), Ui::idOf(IdKind::ToolbarButton, 0));
    EXPECT_LT(Ui::idOf(IdKind::MenuButton, Ui::SERIAL_LAST), Ui::idOf(IdKind::ToolbarButton, 0));
}

TEST(IdKindTest, HostIdsAreTheHostKindsOnly)
{
    EXPECT_TRUE(Ui::isHostId(Ui::idOf(IdKind::HostFirst, 0)));
    EXPECT_TRUE(Ui::isHostId(Ui::idOf(IdKind::HostLast, Ui::SERIAL_LAST)));
    EXPECT_FALSE(Ui::isHostId(Ui::idOf(IdKind::FrameworkLast, Ui::SERIAL_LAST)));
    EXPECT_FALSE(Ui::isHostId(Ui::idOf(IdKind::Untagged, 42)));
    EXPECT_FALSE(Ui::isHostId(Ui::INVALID_ID));
}

TEST(IdKindTest, InvalidIdIsItsOwnKind) { EXPECT_EQ(Ui::kindOf(Ui::INVALID_ID), IdKind::Invalid); }

TEST(IdKindTest, DockGripAndScrollMapBackToTheirDock)
{
    const Ui::id_t dock   = Ui::idOf(IdKind::Dock, 3);
    const Ui::id_t grip   = Ui::toDockGripElementId(dock);
    const Ui::id_t scroll = Ui::toDockScrollElementId(dock);
    EXPECT_EQ(Ui::toDockId(grip), dock);
    EXPECT_EQ(Ui::toDockId(scroll), dock);
    EXPECT_NE(grip, scroll);
    EXPECT_NE(grip, dock);
    EXPECT_NE(scroll, dock);
}

TEST(IdKindTest, FixedIdsAllDiffer)
{
    const std::array<Ui::id_t, 5> fixed = { Ui::MAIN_WINDOW_ID,
                                            Ui::APP_ID,
                                            Ui::TAB_ARROW_LEFT,
                                            Ui::TAB_ARROW_RIGHT,
                                            Ui::STATUS_TEXT_ID };
    for (std::size_t first = 0; first < fixed.size(); ++first) {
        for (std::size_t second = first + 1; second < fixed.size(); ++second) {
            EXPECT_NE(fixed.at(first), fixed.at(second)) << first << " vs " << second;
        }
    }
}
