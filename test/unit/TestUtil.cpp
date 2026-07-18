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
 * @file TestUtil.cpp
 * @brief Pure string helpers: Ui::Res::Util::splitMenuKey (hierarchical menu-key
 *        path split, anchors menu-state restore) and Ui::Action::fileExtension
 *        (extension dispatch key for OpenFile handlers). Both GL-free.
 */

#include "ui/action/fileextension.h"
#include "ui/res/util.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

using Segments = std::vector<std::string>;

// -- Ui::Res::Util::splitMenuKey -------------------------------------------
TEST(SplitMenuKey, MultipleSegments)
{
    EXPECT_EQ(Ui::Res::Util::splitMenuKey("View:Theme:default"), (Segments { "View", "Theme", "default" }));
}

TEST(SplitMenuKey, SingleSegment) { EXPECT_EQ(Ui::Res::Util::splitMenuKey("View"), (Segments { "View" })); }

TEST(SplitMenuKey, TrailingColonYieldsEmptyTail)
{
    EXPECT_EQ(Ui::Res::Util::splitMenuKey("A:"), (Segments { "A", "" }));
}

TEST(SplitMenuKey, ConsecutiveColonsYieldEmptyMiddle)
{
    EXPECT_EQ(Ui::Res::Util::splitMenuKey("A::B"), (Segments { "A", "", "B" }));
}

// Documented invariant: never empty - an empty key is one empty segment.
TEST(SplitMenuKey, EmptyKeyIsOneEmptySegment) { EXPECT_EQ(Ui::Res::Util::splitMenuKey(""), (Segments { "" })); }

// -- Ui::Action::fileExtension ---------------------------------------------
TEST(FileExtension, LowercasesAndStripsLeadingDot) { EXPECT_EQ(Ui::Action::fileExtension("/p/Model.STEP"), "step"); }

TEST(FileExtension, LastExtensionOnly) { EXPECT_EQ(Ui::Action::fileExtension("archive.tar.gz"), "gz"); }

TEST(FileExtension, NoExtensionIsEmpty) { EXPECT_EQ(Ui::Action::fileExtension("README"), ""); }

// A leading-dot filename is a dotfile, not an extension (std::filesystem rule).
TEST(FileExtension, DotfileHasNoExtension) { EXPECT_EQ(Ui::Action::fileExtension("/home/u/.bashrc"), ""); }

TEST(FileExtension, EmptyPathIsEmpty) { EXPECT_EQ(Ui::Action::fileExtension(""), ""); }

} // namespace
