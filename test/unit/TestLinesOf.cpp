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
 * @file TestLinesOf.cpp
 * @brief Unit tests for ui/io/platform/linesof.h: the paths zenity or kdialog wrote, one a line
 */

#include "ui/io/platform/linesof.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using lines_t = std::vector<std::string>;

TEST(LinesOf, EachLineIsAPath)
{
    EXPECT_EQ(Ui::Io::Platform::linesOf("/models/one.step\n/models/two.iges\n"),
              (lines_t { "/models/one.step", "/models/two.iges" }));
}

// The end of the last line is no path of its own, whether or not the dialog wrote it
TEST(LinesOf, TheLastLinesEndIsNoPath)
{
    EXPECT_EQ(Ui::Io::Platform::linesOf("/models/one.step\n"), (lines_t { "/models/one.step" }));
    EXPECT_EQ(Ui::Io::Platform::linesOf("/models/one.step"), (lines_t { "/models/one.step" }));
}

// Spaces are the user's names, never a split
TEST(LinesOf, APathWithSpacesIsOnePath)
{
    EXPECT_EQ(Ui::Io::Platform::linesOf("/my models/part one.step\n"), (lines_t { "/my models/part one.step" }));
}

// Negative: a cancelled dialog writes nothing, which is no path, nor is a line end alone
TEST(LinesOf, NothingWrittenIsNoPath)
{
    EXPECT_TRUE(Ui::Io::Platform::linesOf("").empty());
    EXPECT_TRUE(Ui::Io::Platform::linesOf("\n").empty());
}
