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
#include <optional>

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

// The tab under id, none while it is not held
std::optional<Ui::tab_t> tabOf(const TabBar & bar, Ui::id_t id)
{
    std::optional<Ui::tab_t> found;
    bar.visit(id, [&found](const Ui::tab_t & held) { found = held; });
    return found;
}

// The tabs' ids in order, as a vector to compare
std::vector<Ui::id_t> orderOf(const TabBar & bar)
{
    const auto order = bar.order();
    return std::vector<Ui::id_t>(order.begin(), order.end());
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
    EXPECT_FALSE(tabOf(bar, 0));
    EXPECT_TRUE(bar.order().empty());
}

TEST(TabBar, FindMissingReturnsNull)
{
    TabBar bar;
    bar.setTabs(tabs(2));
    EXPECT_TRUE(tabOf(bar, 0));
    EXPECT_FALSE(tabOf(bar, 99)); // unknown id
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
    EXPECT_EQ(orderOf(bar), (std::vector<Ui::id_t> { 7, 8 }));

    ASSERT_TRUE(tabOf(bar, 7));
    EXPECT_EQ(tabOf(bar, 7).value().label, "Model A");
    EXPECT_TRUE(tabOf(bar, 7).value().isActive);
    EXPECT_TRUE(tabOf(bar, 7).value().hasContent);
    EXPECT_FALSE(tabOf(bar, 8).value().hasContent);
}

TEST(TabBar, SetTabsReplacesPreviousContents)
{
    TabBar bar;
    bar.setTabs({ tab(1), tab(2), tab(3) });
    bar.setTabs({ tab(10), tab(11) });

    EXPECT_EQ(bar.count(), 2U);
    EXPECT_EQ(orderOf(bar), (std::vector<Ui::id_t> { 10, 11 }));
    EXPECT_FALSE(tabOf(bar, 1)); // old ids gone
    EXPECT_FALSE(tabOf(bar, 2));
    EXPECT_TRUE(tabOf(bar, 10));
}

TEST(TabBar, SetTabsEmptyClears)
{
    TabBar bar;
    bar.setTabs(tabs(3));
    bar.setTabs({});
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.count(), 0U);
    EXPECT_FALSE(tabOf(bar, 0));
}

TEST(TabBar, SetTabsDuplicateIdKeepsFirst)
{
    TabBar bar;
    bar.setTabs({ tab(5, "first"), tab(5, "second") }); // same id - second ignored
    EXPECT_EQ(bar.count(), 1U);
    EXPECT_EQ(orderOf(bar), (std::vector<Ui::id_t> { 5 }));
    ASSERT_TRUE(tabOf(bar, 5));
    EXPECT_EQ(tabOf(bar, 5).value().label, "first");
}

// ============================================================================
// The active tab and a tab's neighbours
// ============================================================================

TEST(TabBar, ActiveIdIsTheFirstProjectedActive)
{
    TabBar bar;
    EXPECT_EQ(bar.activeId(), Ui::INVALID_ID);
    bar.setTabs({ tab(1), tab(2, "t", true), tab(3, "t", true) });
    EXPECT_EQ(bar.activeId(), 2U);
    bar.setTabs({ tab(1), tab(2) }); // none active
    EXPECT_EQ(bar.activeId(), Ui::INVALID_ID);
}

// A repeated id is dropped whole, its isActive with it
TEST(TabBar, ActiveIdIgnoresARepeatedId)
{
    TabBar bar;
    bar.setTabs({ tab(5, "first"), tab(5, "second", true) });
    EXPECT_EQ(bar.activeId(), Ui::INVALID_ID);
}

TEST(TabBar, NeighboursWrapAtTheEnds)
{
    TabBar bar;
    bar.setTabs({ tab(7), tab(3), tab(9) });
    EXPECT_EQ(bar.idAfter(7), 3U);
    EXPECT_EQ(bar.idAfter(9), 7U); // the first after the last
    EXPECT_EQ(bar.idBefore(3), 7U);
    EXPECT_EQ(bar.idBefore(7), 9U); // the last before the first
}

TEST(TabBar, ALoneTabIsItsOwnNeighbour)
{
    TabBar bar;
    bar.setTabs({ tab(4) });
    EXPECT_EQ(bar.idAfter(4), 4U);
    EXPECT_EQ(bar.idBefore(4), 4U);
}

// The strip's own neighbours, with no wrapping: a drag stops at either end
TEST(TabBar, RightAndLeftOfStopAtTheEnds)
{
    TabBar bar;
    bar.setTabs({ tab(7), tab(3), tab(9) });
    EXPECT_EQ(bar.rightIdOf(7), 3U);
    EXPECT_EQ(bar.leftIdOf(9), 3U);
    EXPECT_FALSE(bar.rightIdOf(9).has_value());
    EXPECT_FALSE(bar.leftIdOf(7).has_value());
    EXPECT_FALSE(bar.rightIdOf(99).has_value());
}

TEST(TabBar, AnUnknownIdHasNoNeighbours)
{
    TabBar bar;
    bar.setTabs(tabs(2));
    EXPECT_EQ(bar.idAfter(99), Ui::INVALID_ID);
    EXPECT_EQ(bar.idBefore(99), Ui::INVALID_ID);
    EXPECT_EQ(TabBar {}.idAfter(Ui::INVALID_ID), Ui::INVALID_ID);
}

// ============================================================================
// The recent order - the Ctrl+Tab list's
// ============================================================================

TEST(TabBar, RecentIdsAreAsProjected)
{
    TabBar bar;
    bar.setTabs({ tab(1), tab(2), tab(3, "t", true) }, { 3, 1, 2 });
    EXPECT_EQ(bar.recentIds(), (std::vector<Ui::id_t> { 3, 1, 2 }));
}

// A host that keeps no such order gets the strip's
TEST(TabBar, RecentIdsDefaultToTheStrip)
{
    TabBar bar;
    bar.setTabs({ tab(7), tab(3), tab(9) });
    EXPECT_EQ(bar.recentIds(), (std::vector<Ui::id_t> { 7, 3, 9 }));
}

// The active one first, the list opening on it and counting its rows from there; the others
// keep their order. The strip's order has it anywhere, and so may a host's
TEST(TabBar, RecentIdsStartWithTheActiveOne)
{
    TabBar bar;
    bar.setTabs({ tab(7), tab(3), tab(9, "t", true) });
    EXPECT_EQ(bar.recentIds(), (std::vector<Ui::id_t> { 9, 7, 3 }));
    bar.setTabs({ tab(1), tab(2), tab(3, "t", true) }, { 1, 3, 2 });
    EXPECT_EQ(bar.recentIds(), (std::vector<Ui::id_t> { 3, 1, 2 }));
}

// Only tabs it holds, so a row of the list always names one
TEST(TabBar, RecentIdsDropAnUnknownOne)
{
    TabBar bar;
    bar.setTabs({ tab(1), tab(2) }, { 2, 99, 1 });
    EXPECT_EQ(bar.recentIds(), (std::vector<Ui::id_t> { 2, 1 }));
    EXPECT_TRUE(bar.contains(2));
    EXPECT_FALSE(bar.contains(99));
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
    ASSERT_TRUE(tabOf(bar, 1));
    EXPECT_TRUE(tabOf(bar, 1).value().isLoading);
    EXPECT_EQ(tabOf(bar, 1).value().progress, 2);
}

TEST(TabBar, SetLoadingClearsOnCompletion)
{
    TabBar bar;
    bar.setTabs({ tab(1, "m", false, true, true, 1) });
    bar.setLoading(1, false, 3); // load finished
    EXPECT_FALSE(tabOf(bar, 1).value().isLoading);
    EXPECT_EQ(tabOf(bar, 1).value().progress, 3);
}

TEST(TabBar, SetLoadingLeavesStructuralFieldsIntact)
{
    TabBar bar;
    bar.setTabs({ tab(1, "Model A", true, true, false, 0) });
    bar.setLoading(1, true, 1);
    const std::optional<Ui::tab_t> kept = tabOf(bar, 1);
    ASSERT_TRUE(kept);
    EXPECT_EQ(kept.value().label, "Model A"); // untouched by the volatile path
    EXPECT_TRUE(kept.value().isActive);
    EXPECT_TRUE(kept.value().hasContent);
}

TEST(TabBar, SetLoadingMissingIdIsNoop)
{
    TabBar bar;
    bar.setTabs({ tab(1, "keep", false, false, false, 0) });
    bar.setLoading(99, true, 5); // unknown id - must not crash or touch others
    EXPECT_EQ(bar.count(), 1U);
    EXPECT_FALSE(tabOf(bar, 1).value().isLoading);
    EXPECT_EQ(tabOf(bar, 1).value().progress, 0);
}

// ============================================================================
// reveal - a tab activated other than by a click comes into view
// ============================================================================

TEST(TabBar, RevealInViewDoesNotScroll)
{
    TabBar bar;
    bar.setTabs(tabs(10));
    bar.scrollRight();
    bar.scrollRight(); // tabs 2..5 shown, 4 at a time
    bar.reveal(2, 4);
    EXPECT_EQ(bar.scrollOffset(), 2U);
    bar.reveal(5, 4);
    EXPECT_EQ(bar.scrollOffset(), 2U);
}

TEST(TabBar, RevealPastTheRightEdgeShowsItLast)
{
    TabBar bar;
    bar.setTabs(tabs(10));
    bar.reveal(6, 4); // 0..3 shown -> 3..6
    EXPECT_EQ(bar.scrollOffset(), 3U);
}

TEST(TabBar, RevealLeftOfTheOffsetShowsItFirst)
{
    TabBar bar;
    bar.setTabs(tabs(10));
    for (int step = 0; step < 6; ++step) {
        bar.scrollRight();
    }
    bar.reveal(0, 4); // wrapping round from the last tab
    EXPECT_EQ(bar.scrollOffset(), 0U);
}

TEST(TabBar, RevealOneShownPutsItAtTheOffset)
{
    TabBar bar;
    bar.setTabs(tabs(10));
    bar.reveal(7, 1);
    EXPECT_EQ(bar.scrollOffset(), 7U);
}
