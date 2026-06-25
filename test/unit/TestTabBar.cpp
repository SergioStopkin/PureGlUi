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
 * @file TestTabBar.cpp
 * @brief Unit tests for Ui::TabBar - the chrome's tab model (ordered tab_t set
 *        plus scroll offset). Verifies projection via setTabs, identity lookup,
 *        scroll bounds, the in-place loading refresh, and the scroll-clamp on
 *        shrink. Pure C++ - no rendering, no WorkspaceManager.
 */

#include "ui/tab.h"
#include "ui/tabbar.h"

#include <gtest/gtest.h>

// Note: do not `using Ui::id_t` - it clashes with POSIX ::id_t from <sys/types.h>.
using Ui::TabBar;

namespace {

// Build a plain tab_t; callers override only the fields a test cares about.
Ui::tab_t tab(Ui::id_t    id,
              std::string label      = "t",
              bool        isActive   = false,
              bool        hasContent = false,
              bool        isLoading  = false,
              int         progress   = 0)
{
    return Ui::tab_t { id, std::move(label), isActive, hasContent, isLoading, progress };
}

// N tabs with ids 0..N-1 in order.
std::vector<Ui::tab_t> tabs(std::size_t n)
{
    std::vector<Ui::tab_t> out;
    for (std::size_t i = 0; i < n; ++i) {
        out.emplace_back(tab(static_cast<Ui::id_t>(i)));
    }
    return out;
}

} // namespace

// ============================================================================
// Read side / empty state
// ============================================================================

TEST(TabBar, EmptyByDefault)
{
    TabBar bar;
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.count(), 0U);
    EXPECT_EQ(bar.scrollOffset(), 0U);
    EXPECT_EQ(bar.find(0), nullptr);
    EXPECT_TRUE(bar.order().empty());
}

TEST(TabBar, FindMissingReturnsNull)
{
    TabBar bar;
    bar.setTabs(tabs(2));
    EXPECT_NE(bar.find(0), nullptr);
    EXPECT_EQ(bar.find(99), nullptr); // unknown id
}

// ============================================================================
// setTabs projection
// ============================================================================

TEST(TabBar, SetTabsPopulatesInOrderWithFields)
{
    TabBar bar;
    bar.setTabs({ tab(7, "Model A", true, true, false, 0), tab(8, "new 1") });

    EXPECT_FALSE(bar.empty());
    EXPECT_EQ(bar.count(), 2U);
    EXPECT_EQ(bar.order(), (std::vector<Ui::id_t> { 7, 8 }));

    ASSERT_NE(bar.find(7), nullptr);
    EXPECT_EQ(bar.find(7)->label, "Model A");
    EXPECT_TRUE(bar.find(7)->isActive);
    EXPECT_TRUE(bar.find(7)->hasContent);
    EXPECT_FALSE(bar.find(8)->hasContent);
}

TEST(TabBar, SetTabsReplacesPreviousContents)
{
    TabBar bar;
    bar.setTabs({ tab(1), tab(2), tab(3) });
    bar.setTabs({ tab(10), tab(11) });

    EXPECT_EQ(bar.count(), 2U);
    EXPECT_EQ(bar.order(), (std::vector<Ui::id_t> { 10, 11 }));
    EXPECT_EQ(bar.find(1), nullptr); // old ids gone
    EXPECT_EQ(bar.find(2), nullptr);
    EXPECT_NE(bar.find(10), nullptr);
}

TEST(TabBar, SetTabsEmptyClears)
{
    TabBar bar;
    bar.setTabs(tabs(3));
    bar.setTabs({});
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.count(), 0U);
    EXPECT_EQ(bar.find(0), nullptr);
}

TEST(TabBar, SetTabsDuplicateIdKeepsFirst)
{
    TabBar bar;
    bar.setTabs({ tab(5, "first"), tab(5, "second") }); // same id - second ignored
    EXPECT_EQ(bar.count(), 1U);
    EXPECT_EQ(bar.order(), (std::vector<Ui::id_t> { 5 }));
    ASSERT_NE(bar.find(5), nullptr);
    EXPECT_EQ(bar.find(5)->label, "first");
}

// ============================================================================
// Scroll - bounds and clamping
// ============================================================================

TEST(TabBar, ScrollRightAdvancesUpToLastIndex)
{
    TabBar bar;
    bar.setTabs(tabs(3));
    EXPECT_EQ(bar.scrollOffset(), 0U);
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 1U);
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 2U);
    bar.scrollRight(); // already at last index - capped
    EXPECT_EQ(bar.scrollOffset(), 2U);
}

TEST(TabBar, ScrollLeftStopsAtZero)
{
    TabBar bar;
    bar.setTabs(tabs(3));
    bar.scrollLeft(); // already at 0 - no underflow
    EXPECT_EQ(bar.scrollOffset(), 0U);
    bar.scrollRight();
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 2U);
    bar.scrollLeft();
    EXPECT_EQ(bar.scrollOffset(), 1U);
}

TEST(TabBar, ScrollRightOnEmptyIsNoop)
{
    TabBar bar;
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 0U);
}

TEST(TabBar, ScrollRightOnSingleTabIsNoop)
{
    TabBar bar;
    bar.setTabs(tabs(1));
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 0U);
}

TEST(TabBar, SetTabsClampsScrollWhenShrinking)
{
    TabBar bar;
    bar.setTabs(tabs(5));
    bar.scrollRight();
    bar.scrollRight();
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 3U);
    bar.setTabs(tabs(2)); // offset 3 now out of range -> clamp to last index (1)
    EXPECT_EQ(bar.scrollOffset(), 1U);
}

TEST(TabBar, SetTabsToEmptyResetsScrollToZero)
{
    TabBar bar;
    bar.setTabs(tabs(5));
    bar.scrollRight();
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 2U);
    bar.setTabs({});
    EXPECT_EQ(bar.scrollOffset(), 0U);
}

TEST(TabBar, SetTabsKeepsScrollWhenStillInRange)
{
    TabBar bar;
    bar.setTabs(tabs(5));
    bar.scrollRight();
    bar.scrollRight();
    EXPECT_EQ(bar.scrollOffset(), 2U);
    bar.setTabs(tabs(5)); // offset 2 still valid -> unchanged
    EXPECT_EQ(bar.scrollOffset(), 2U);
}

// ============================================================================
// setLoading - in-place volatile refresh
// ============================================================================

TEST(TabBar, SetLoadingUpdatesVolatileFields)
{
    TabBar bar;
    bar.setTabs({ tab(1) });
    bar.setLoading(1, true, 2);
    ASSERT_NE(bar.find(1), nullptr);
    EXPECT_TRUE(bar.find(1)->isLoading);
    EXPECT_EQ(bar.find(1)->progress, 2);
}

TEST(TabBar, SetLoadingClearsOnCompletion)
{
    TabBar bar;
    bar.setTabs({ tab(1, "m", false, true, true, 1) });
    bar.setLoading(1, false, 3); // load finished
    EXPECT_FALSE(bar.find(1)->isLoading);
    EXPECT_EQ(bar.find(1)->progress, 3);
}

TEST(TabBar, SetLoadingLeavesStructuralFieldsIntact)
{
    TabBar bar;
    bar.setTabs({ tab(1, "Model A", true, true, false, 0) });
    bar.setLoading(1, true, 1);
    const Ui::tab_t * t = bar.find(1);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->label, "Model A"); // untouched by the volatile path
    EXPECT_TRUE(t->isActive);
    EXPECT_TRUE(t->hasContent);
}

TEST(TabBar, SetLoadingMissingIdIsNoop)
{
    TabBar bar;
    bar.setTabs({ tab(1, "keep", false, false, false, 0) });
    bar.setLoading(99, true, 5); // unknown id - must not crash or touch others
    EXPECT_EQ(bar.count(), 1U);
    EXPECT_FALSE(bar.find(1)->isLoading);
    EXPECT_EQ(bar.find(1)->progress, 0);
}
