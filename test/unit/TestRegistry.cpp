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
 * @file TestRegistry.cpp
 * @brief Unit tests for Ui::Registry - the ordered, id-keyed collection that
 *        backs dynamic UI element lists (tabs today). Verifies identity lookup,
 *        insertion order, reorder, and removal independent of any rendering.
 */

#include "ui/index.h"
#include "ui/registry.h"
#include "ui/tab.h"

#include <gtest/gtest.h>
#include <vector>

// Note: do not `using Ui::id_t` - it clashes with POSIX ::id_t from <sys/types.h>.
using Ui::Registry;

namespace {

// Collect the ids in visual order for terse comparisons.
std::vector<Ui::id_t> ids(const Registry<Ui::id_t, int> & reg) { return reg.order(); }

} // namespace

// ============================================================================
// Read side
// ============================================================================

TEST(Registry, EmptyByDefault)
{
    Registry<Ui::id_t, int> reg;
    EXPECT_TRUE(reg.empty());
    EXPECT_EQ(reg.size(), 0U);
    EXPECT_FALSE(reg.contains(1));
    EXPECT_EQ(reg.find(1), nullptr);
}

TEST(Registry, AddThenFind)
{
    Registry<Ui::id_t, int> reg;
    reg.add(10, 100);
    reg.add(20, 200);

    ASSERT_NE(reg.find(10), nullptr);
    EXPECT_EQ(*reg.find(10), 100);
    EXPECT_EQ(*reg.find(20), 200);
    EXPECT_TRUE(reg.contains(20));
    EXPECT_EQ(reg.size(), 2U);
}

TEST(Registry, AddPreservesInsertionOrder)
{
    Registry<Ui::id_t, int> reg;
    reg.add(3, 0);
    reg.add(1, 0);
    reg.add(2, 0);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 3, 1, 2 }));
}

TEST(Registry, DuplicateAddIgnored)
{
    Registry<Ui::id_t, int> reg;
    reg.add(5, 100);
    reg.add(5, 999); // same id - ignored, no duplicate in order
    EXPECT_EQ(reg.size(), 1U);
    EXPECT_EQ(*reg.find(5), 100);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 5 }));
}

// ============================================================================
// Mutate side
// ============================================================================

TEST(Registry, RemoveDropsFromMapAndOrder)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.add(2, 0);
    reg.add(3, 0);
    reg.remove(2);
    EXPECT_FALSE(reg.contains(2));
    EXPECT_EQ(reg.find(2), nullptr);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 3 }));
}

TEST(Registry, RemoveMissingIsNoop)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.remove(99);
    EXPECT_EQ(reg.size(), 1U);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1 }));
}

TEST(Registry, InsertAtIndex)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.add(3, 0);
    reg.insert(1, 2, 0); // between 1 and 3
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(Registry, InsertIndexClampedToEnd)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.insert(99, 2, 0); // out-of-range index clamps to append
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2 }));
}

TEST(Registry, MoveReorders)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.add(2, 0);
    reg.add(3, 0);
    reg.move(3, 0); // bring last to front
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 3, 1, 2 }));
    reg.move(3, 99); // clamp to end
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(Registry, MoveMissingIsNoop)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 0);
    reg.add(2, 0);
    reg.move(99, 0);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2 }));
}

TEST(Registry, EditMutatesInPlace)
{
    Registry<Ui::id_t, int> reg;
    reg.add(1, 100);
    int * value = reg.edit(1);
    ASSERT_NE(value, nullptr);
    *value = 555;
    EXPECT_EQ(*reg.find(1), 555);
    EXPECT_EQ(reg.edit(99), nullptr);
}

// ============================================================================
// With the first real value type (tab_t)
// ============================================================================

TEST(Registry, HoldsTabViewModels)
{
    Registry<Ui::id_t, Ui::tab_t> tabs;
    tabs.add(7, Ui::tab_t { 7, "Model A", true, true, false, 0 });
    tabs.add(8, Ui::tab_t { 8, "new 1", false, false, false, 0 });

    ASSERT_NE(tabs.find(7), nullptr);
    EXPECT_EQ(tabs.find(7)->label, "Model A");
    EXPECT_TRUE(tabs.find(7)->isActive);
    EXPECT_FALSE(tabs.find(8)->hasContent);

    // Switch active tab via edit.
    tabs.edit(7)->isActive = false;
    tabs.edit(8)->isActive = true;
    EXPECT_FALSE(tabs.find(7)->isActive);
    EXPECT_TRUE(tabs.find(8)->isActive);

    EXPECT_EQ(tabs.order(), (std::vector<Ui::id_t> { 7, 8 }));
}

// ============================================================================
// String keys - the reason the key is a template parameter. Numeric ids are
// reassigned when resources reload; a key_t survives that.
// ============================================================================

TEST(Registry, HoldsStringKeys)
{
    Registry<Ui::key_t, int> reg;
    reg.add("view:displayMode:shaded", 1);
    reg.add("view:displayMode:realistic", 2);

    ASSERT_NE(reg.find("view:displayMode:shaded"), nullptr);
    EXPECT_EQ(*reg.find("view:displayMode:shaded"), 1);
    EXPECT_EQ(reg.find("view:missing"), nullptr);
    EXPECT_TRUE(reg.contains("view:displayMode:realistic"));
    EXPECT_EQ(reg.order(), (std::vector<Ui::key_t> { "view:displayMode:shaded", "view:displayMode:realistic" }));
}

TEST(Registry, FindOrAddCreatesOnceAndJoinsOrder)
{
    Registry<Ui::id_t, std::vector<int>> reg;
    reg.findOrAdd(5).emplace_back(1);
    reg.findOrAdd(5).emplace_back(2); // same key: appends, does not replace

    ASSERT_NE(reg.find(5), nullptr);
    EXPECT_EQ(*reg.find(5), (std::vector<int> { 1, 2 }));
    EXPECT_EQ(reg.order(), (std::vector<Ui::id_t> { 5 })); // joined the order exactly once
}

// ============================================================================
// Ui::Index - group-by over Registry
// ============================================================================

TEST(Index, GroupsValuesByKeyInInsertionOrder)
{
    Ui::Index<Ui::id_t, Ui::id_t> children;
    children.add(1, 10);
    children.add(2, 20);
    children.add(1, 11);

    EXPECT_EQ(children.group(1), (std::vector<Ui::id_t> { 10, 11 }));
    EXPECT_EQ(children.group(2), (std::vector<Ui::id_t> { 20 }));
    EXPECT_EQ(children.size(), 2U); // distinct keys, not values
    EXPECT_EQ(children.keys(), (std::vector<Ui::id_t> { 1, 2 }));
}

// The ergonomic contract: a caller walks group() without first checking
// whether the key exists.
TEST(Index, AbsentKeyYieldsEmptyGroup)
{
    Ui::Index<Ui::id_t, Ui::id_t> children;
    children.add(1, 10);

    EXPECT_TRUE(children.group(999).empty());
    EXPECT_FALSE(children.contains(999));

    std::size_t walked = 0;
    for (const Ui::id_t child : children.group(999)) {
        (void)child;
        ++walked;
    }
    EXPECT_EQ(walked, 0U);
}
