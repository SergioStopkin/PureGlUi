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
 * @file TestRevealedOffsetOf.cpp
 * @brief Unit tests for ui/revealedoffsetof.h, the scroll that brings an item into view the
 *        least it can - the tab strip's and the Ctrl+Tab list's both
 */

#include "ui/revealedoffsetof.h"

#include <gtest/gtest.h>

using Ui::revealedOffsetOf;

TEST(RevealedOffsetOfTest, InViewStays)
{
    EXPECT_EQ(revealedOffsetOf(2, 2, 4), 2U);
    EXPECT_EQ(revealedOffsetOf(2, 5, 4), 2U);
}

TEST(RevealedOffsetOfTest, BeforeTheViewComesFirst) { EXPECT_EQ(revealedOffsetOf(6, 0, 4), 0U); }

TEST(RevealedOffsetOfTest, AfterTheViewComesLast) { EXPECT_EQ(revealedOffsetOf(0, 6, 4), 3U); }

TEST(RevealedOffsetOfTest, NoneShownNeverRunsPastTheItem)
{
    EXPECT_EQ(revealedOffsetOf(0, 2, 0), 0U);
    EXPECT_EQ(revealedOffsetOf(6, 2, 0), 2U);
}
