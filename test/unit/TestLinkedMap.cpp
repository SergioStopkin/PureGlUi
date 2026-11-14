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
 * @file TestLinkedMap.cpp
 * @brief Unit tests for Ui::LinkedMap - the keyed collection in an order of its
 *        own that holds the tab orders. Verifies visit, insertion order, moves,
 *        removal, neighbours and ends, and that a value's address and a copy's
 *        lookups hold, independent of any rendering.
 */

#include "ui/index.h"
#include "ui/linkedmap.h"
#include "ui/tab.h"

#include <atomic>
#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <utility>
#include <vector>

// Note: do not `using Ui::id_t` - it clashes with POSIX ::id_t from <sys/types.h>.
using Ui::LinkedMap;

namespace {

// The keys in order, as a vector to compare
template <typename Map>
auto keysOf(const Map & map)
{
    const auto keys = map.keys();
    return std::vector(keys.begin(), keys.end());
}

// The value under key, none while it is not held
std::optional<int> valueOf(const LinkedMap<Ui::id_t, int> & map, Ui::id_t key)
{
    std::optional<int> found;
    map.visit(key, [&found](int value) { found = value; });
    return found;
}

// Where the value under key lives, to tell whether it moved
const void * addressOf(const LinkedMap<Ui::id_t, int> & map, Ui::id_t key)
{
    const void * address = nullptr;
    map.visit(key, [&address](const int & value) { address = &value; });
    return address;
}

// 1, 2, 3 in that order
LinkedMap<Ui::id_t, int> oneTwoThree()
{
    LinkedMap<Ui::id_t, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);
    return map;
}

} // namespace

// ============================================================================
// Read side
// ============================================================================

TEST(LinkedMap, EmptyByDefault)
{
    LinkedMap<Ui::id_t, int> map;
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_FALSE(map.contains(1));
    EXPECT_FALSE(valueOf(map, 1));
    EXPECT_TRUE(map.keys().empty());
    EXPECT_FALSE(map.firstKey());
    EXPECT_FALSE(map.lastKey());
}

TEST(LinkedMap, AddThenVisit)
{
    LinkedMap<Ui::id_t, int> map;
    map.add(10, 100);
    map.add(20, 200);

    EXPECT_EQ(valueOf(map, 10), 100);
    EXPECT_EQ(valueOf(map, 20), 200);
    EXPECT_TRUE(map.contains(20));
    EXPECT_EQ(map.size(), 2U);
}

TEST(LinkedMap, VisitTellsWhetherTheKeyIsHeld)
{
    const LinkedMap<Ui::id_t, int> map    = oneTwoThree();
    int                            called = 0;
    EXPECT_TRUE(map.visit(2, [&called](int) { ++called; }));
    EXPECT_FALSE(map.visit(99, [&called](int) { ++called; }));
    EXPECT_EQ(called, 1);
}

TEST(LinkedMap, AddPreservesInsertionOrder)
{
    LinkedMap<Ui::id_t, int> map;
    map.add(3, 0);
    map.add(1, 0);
    map.add(2, 0);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 3, 1, 2 }));
}

TEST(LinkedMap, DuplicateAddIgnored)
{
    LinkedMap<Ui::id_t, int> map;
    map.add(5, 100);
    map.add(5, 999); // same key - ignored, no duplicate in order
    EXPECT_EQ(map.size(), 1U);
    EXPECT_EQ(valueOf(map, 5), 100);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 5 }));
}

TEST(LinkedMap, NeighboursFollowTheOrder)
{
    const LinkedMap<Ui::id_t, int> map = oneTwoThree();
    EXPECT_EQ(map.keyAfter(1), 2U);
    EXPECT_EQ(map.keyBefore(3), 2U);
    EXPECT_FALSE(map.keyAfter(3)); // the last has none after it
    EXPECT_FALSE(map.keyBefore(1));
    EXPECT_FALSE(map.keyAfter(99)); // nor has an absent key
    EXPECT_FALSE(map.keyBefore(99));
    EXPECT_EQ(map.firstKey(), 1U);
    EXPECT_EQ(map.lastKey(), 3U);
}

// ============================================================================
// Mutate side
// ============================================================================

TEST(LinkedMap, RemoveDropsFromMapAndOrder)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.remove(2);
    EXPECT_FALSE(map.contains(2));
    EXPECT_FALSE(valueOf(map, 2));
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 3 }));
    EXPECT_EQ(map.keyAfter(1), 3U); // the neighbours closed up
}

TEST(LinkedMap, RemoveMissingIsNoop)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.remove(99);
    EXPECT_EQ(map.size(), 3U);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(LinkedMap, MoveBeforeReorders)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.moveBefore(3, 1); // last to front
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 3, 1, 2 }));
    map.moveBefore(3, 2); // into the middle
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 3, 2 }));
    map.moveBefore(3, 2); // already there
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 3, 2 }));
    EXPECT_EQ(map.firstKey(), 1U);
    EXPECT_EQ(map.lastKey(), 2U);
}

TEST(LinkedMap, MoveBeforeItselfOrMissingIsNoop)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.moveBefore(2, 2);
    map.moveBefore(99, 1);
    map.moveBefore(1, 99);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(LinkedMap, MoveToBackReorders)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.moveToBack(1);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 2, 3, 1 }));
    map.moveToBack(1); // already last
    map.moveToBack(99);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 2, 3, 1 }));
    EXPECT_EQ(map.lastKey(), 1U);
}

TEST(LinkedMap, VisitChangesInPlace)
{
    LinkedMap<Ui::id_t, int> map;
    map.add(1, 100);
    EXPECT_TRUE(map.visit(1, [](int & value) { value = 555; }));
    EXPECT_EQ(valueOf(map, 1), 555);
    EXPECT_FALSE(map.visit(99, [](int & value) { value = 0; }));
}

// What a caller holding a visited value's address relies on: only its own remove ends it
TEST(LinkedMap, ValueAddressHoldsAcrossChanges)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    const void *             two = addressOf(map, 2);
    map.moveToBack(2);
    map.moveBefore(2, 1);
    map.remove(1);
    map.remove(3);
    map.add(4, 400);
    EXPECT_EQ(addressOf(map, 2), two);
    EXPECT_EQ(valueOf(map, 2), 200);
}

// ============================================================================
// Copy and move - the lookups must point at the holder's own nodes
// ============================================================================

TEST(LinkedMap, CopyIsIndependent)
{
    const LinkedMap<Ui::id_t, int> original = oneTwoThree();
    LinkedMap<Ui::id_t, int>       copy     = original;
    EXPECT_NE(addressOf(copy, 2), addressOf(original, 2));

    copy.visit(2, [](int & value) { value = 222; });
    copy.moveToBack(1);
    copy.remove(3);

    EXPECT_EQ(keysOf(copy), (std::vector<Ui::id_t> { 2, 1 }));
    EXPECT_EQ(valueOf(copy, 2), 222);
    EXPECT_EQ(keysOf(original), (std::vector<Ui::id_t> { 1, 2, 3 }));
    EXPECT_EQ(valueOf(original, 2), 200);
}

TEST(LinkedMap, CopyAssignReplaces)
{
    const LinkedMap<Ui::id_t, int> original = oneTwoThree();
    LinkedMap<Ui::id_t, int>       target;
    target.add(9, 900);
    target = original;
    target.remove(1);

    EXPECT_FALSE(target.contains(9));
    EXPECT_EQ(keysOf(target), (std::vector<Ui::id_t> { 2, 3 }));
    EXPECT_EQ(keysOf(original), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

TEST(LinkedMap, MovedKeepsLookups)
{
    LinkedMap<Ui::id_t, int> original = oneTwoThree();
    const void *             two      = addressOf(original, 2);
    LinkedMap<Ui::id_t, int> moved    = std::move(original);
    EXPECT_EQ(addressOf(moved, 2), two);

    LinkedMap<Ui::id_t, int> assigned;
    assigned = std::move(moved);
    assigned.moveToBack(1);
    assigned.remove(2);
    EXPECT_EQ(keysOf(assigned), (std::vector<Ui::id_t> { 3, 1 }));
}

// ============================================================================
// With the first real value type (tab_t)
// ============================================================================

TEST(LinkedMap, HoldsTabViewModels)
{
    LinkedMap<Ui::id_t, Ui::tab_t> tabs;
    tabs.add(7, Ui::tab_t { 7, "Model A", true, true, false, 0 });
    tabs.add(8, Ui::tab_t { 8, "new 1", false, false, false, 0 });

    std::optional<Ui::tab_t> seven;
    ASSERT_TRUE(tabs.visit(7, [&seven](const Ui::tab_t & tab) { seven = tab; }));
    EXPECT_EQ(seven.value().label, "Model A");
    EXPECT_TRUE(seven.value().isActive);

    // Switch active tab in place
    tabs.visit(7, [](Ui::tab_t & tab) { tab.isActive = false; });
    tabs.visit(8, [](Ui::tab_t & tab) { tab.isActive = true; });
    bool isSevenActive = true;
    bool isEightActive = false;
    tabs.visit(7, [&isSevenActive](const Ui::tab_t & tab) { isSevenActive = tab.isActive; });
    tabs.visit(8, [&isEightActive](const Ui::tab_t & tab) { isEightActive = tab.isActive; });
    EXPECT_FALSE(isSevenActive);
    EXPECT_TRUE(isEightActive);

    EXPECT_EQ(keysOf(tabs), (std::vector<Ui::id_t> { 7, 8 }));
}

// ============================================================================
// String keys - the reason the key is a template parameter. Numeric ids are
// reassigned when resources reload; a key_t survives that.
// ============================================================================

TEST(LinkedMap, HoldsStringKeys)
{
    LinkedMap<Ui::key_t, int> map;
    map.add("view:displayMode:shaded", 1);
    map.add("view:displayMode:realistic", 2);

    int shaded = 0;
    EXPECT_TRUE(map.visit("view:displayMode:shaded", [&shaded](int value) { shaded = value; }));
    EXPECT_EQ(shaded, 1);
    EXPECT_FALSE(map.contains("view:missing"));
    EXPECT_EQ(map.keyAfter("view:displayMode:shaded"), "view:displayMode:realistic");
    EXPECT_EQ(keysOf(map), (std::vector<Ui::key_t> { "view:displayMode:shaded", "view:displayMode:realistic" }));
}

TEST(LinkedMap, FindOrAddCreatesOnceAndJoinsOrder)
{
    LinkedMap<Ui::id_t, std::vector<int>> map;
    map.findOrAdd(5).emplace_back(1);
    map.findOrAdd(5).emplace_back(2); // same key: appends, does not replace

    std::vector<int> five;
    ASSERT_TRUE(map.visit(5, [&five](const std::vector<int> & held) { five = held; }));
    EXPECT_EQ(five, (std::vector<int> { 1, 2 }));
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 5 })); // joined the order exactly once
}

// Built in its node, so a value that cannot move is held directly
TEST(LinkedMap, FindOrAddHoldsAValueThatCannotMove)
{
    LinkedMap<Ui::id_t, std::atomic<int>> map;
    map.findOrAdd(1).store(7);
    map.findOrAdd(2).store(8);
    int one = 0;
    map.visit(1, [&one](const std::atomic<int> & value) { one = value.load(); });
    EXPECT_EQ(one, 7);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2 }));
}

// ============================================================================
// Ui::Index - group-by over LinkedMap
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
    EXPECT_EQ(keysOf(children), (std::vector<Ui::id_t> { 1, 2 }));
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
