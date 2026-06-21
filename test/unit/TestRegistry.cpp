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

#include "ui/registry.h"
#include "ui/type/tab.h"

#include <gtest/gtest.h>

// Note: do not `using Ui::id_t` - it clashes with POSIX ::id_t from <sys/types.h>.
using Ui::Registry;

namespace {

// Collect the ids in visual order for terse comparisons.
std::vector<Ui::id_t> ids(const Registry<int> & reg) { return reg.order(); }

} // namespace

// ============================================================================
// Read side
// ============================================================================

TEST(Registry, EmptyByDefault)
{
    Registry<int> reg;
    EXPECT_TRUE(reg.empty());
    EXPECT_EQ(reg.size(), 0U);
    EXPECT_FALSE(reg.contains(1));
    EXPECT_EQ(reg.find(1), nullptr);
}

TEST(Registry, AddThenFind)
{
    Registry<int> reg;
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
    Registry<int> reg;
    reg.add(3, 0);
    reg.add(1, 0);
    reg.add(2, 0);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 3, 1, 2 }));
}

TEST(Registry, DuplicateAddIgnored)
{
    Registry<int> reg;
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
    Registry<int> reg;
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
    Registry<int> reg;
    reg.add(1, 0);
    reg.remove(99);
    EXPECT_EQ(reg.size(), 1U);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1 }));
}

TEST(Registry, InsertAtIndex)
{
    Registry<int> reg;
    reg.add(1, 0);
    reg.add(3, 0);
    reg.insert(1, 2, 0); // between 1 and 3
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(Registry, InsertIndexClampedToEnd)
{
    Registry<int> reg;
    reg.add(1, 0);
    reg.insert(99, 2, 0); // out-of-range index clamps to append
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2 }));
}

TEST(Registry, MoveReorders)
{
    Registry<int> reg;
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
    Registry<int> reg;
    reg.add(1, 0);
    reg.add(2, 0);
    reg.move(99, 0);
    EXPECT_EQ(ids(reg), (std::vector<Ui::id_t> { 1, 2 }));
}

TEST(Registry, EditMutatesInPlace)
{
    Registry<int> reg;
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
    Registry<Ui::Type::tab_t> tabs;
    tabs.add(7, Ui::Type::tab_t { 7, "Model A", true, true, false, 0 });
    tabs.add(8, Ui::Type::tab_t { 8, "new 1", false, false, false, 0 });

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
