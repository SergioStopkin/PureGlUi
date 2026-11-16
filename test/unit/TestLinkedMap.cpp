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
 *        own that holds the tab orders. Verifies visit, forEach, insertion order,
 *        moves, removal, neighbours and ends, and that a value's address and a
 *        copy's lookups hold, independent of any rendering.
 */

#include "thrower.h"
#include "ui/index.h"
#include "ui/linkedmap.h"
#include "ui/tab.h"

#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Note: do not `using Ui::id_t` - it clashes with POSIX ::id_t from <sys/types.h>.
using TestSupport::thrower_t;
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

// Every key with its value as forEach hands them, in order, to compare
std::vector<std::pair<Ui::id_t, int>> walkOf(const LinkedMap<Ui::id_t, int> & map)
{
    std::vector<std::pair<Ui::id_t, int>> walked;
    map.forEach([&walked](Ui::id_t key, int value) { walked.emplace_back(key, value); });
    return walked;
}

// The values an Index visits under key, in order, to compare
std::vector<Ui::id_t> groupOf(const Ui::Index<Ui::id_t, Ui::id_t> & index, Ui::id_t key)
{
    std::vector<Ui::id_t> group;
    index.visit(key, [&group](Ui::id_t value) { group.emplace_back(value); });
    return group;
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

// Cleared, nothing is left to find, walk or neighbour
TEST(LinkedMap, ClearLeavesNothing)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_FALSE(map.contains(2));
    EXPECT_FALSE(valueOf(map, 2));
    EXPECT_TRUE(map.keys().empty());
    EXPECT_TRUE(walkOf(map).empty());
    EXPECT_FALSE(map.firstKey());
    EXPECT_FALSE(map.lastKey());
    EXPECT_FALSE(map.keyAfter(1));
}

// A key held before a clear goes in again fresh, at the end of what is added after it
TEST(LinkedMap, AKeyAddedAfterAClearIsNew)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.clear();
    map.add(3, 30);
    map.add(1, 10);
    EXPECT_EQ(walkOf(map), (std::vector<std::pair<Ui::id_t, int>> { { 3, 30 }, { 1, 10 } }));
}

// Values that can be neither copied nor moved are cleared as any are
TEST(LinkedMap, ClearTakesValuesThatCannotMove)
{
    LinkedMap<Ui::id_t, std::atomic<int>> map;
    map.emplace(1, 7);
    map.emplace(2, 8);
    map.clear();
    EXPECT_TRUE(map.empty());
    map.emplace(2, 9);
    int two = 0;
    ASSERT_TRUE(map.visit(2, [&two](const std::atomic<int> & value) { two = value.load(); }));
    EXPECT_EQ(two, 9);
}

// Negative: clearing an empty map, or one already cleared, changes nothing
TEST(LinkedMap, ClearOfAnEmptyMapIsNoop)
{
    LinkedMap<Ui::id_t, int> map;
    map.clear();
    EXPECT_TRUE(map.empty());
    map.add(1, 100);
    map.clear();
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_TRUE(map.keys().empty());
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
// forEach - every key with its value, in order, one pass
// ============================================================================

TEST(LinkedMap, ForEachWalksEveryKeyWithItsValueInOrder)
{
    const LinkedMap<Ui::id_t, int> map = oneTwoThree();
    EXPECT_EQ(walkOf(map), (std::vector<std::pair<Ui::id_t, int>> { { 1, 100 }, { 2, 200 }, { 3, 300 } }));
}

// The order walked is the order kept, whatever moved or went
TEST(LinkedMap, ForEachFollowsMovesAndRemoves)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.moveBefore(3, 1);
    map.moveToBack(1);
    map.remove(2);
    map.add(4, 400);
    EXPECT_EQ(walkOf(map), (std::vector<std::pair<Ui::id_t, int>> { { 3, 300 }, { 1, 100 }, { 4, 400 } }));
}

// Once per key, however often a key was added
TEST(LinkedMap, ForEachCallsOncePerKey)
{
    LinkedMap<Ui::id_t, int> map;
    map.add(5, 100);
    map.add(5, 999);
    map.add(6, 600);
    int calls = 0;
    map.forEach([&calls](Ui::id_t /*key*/, int /*value*/) { ++calls; });
    EXPECT_EQ(calls, 2);
}

// The mutate side changes each value where it stands, the order as it was
TEST(LinkedMap, ForEachChangesValuesInPlace)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    const void *             two = addressOf(map, 2);
    map.forEach([](Ui::id_t /*key*/, int & value) { ++value; });
    EXPECT_EQ(walkOf(map), (std::vector<std::pair<Ui::id_t, int>> { { 1, 101 }, { 2, 201 }, { 3, 301 } }));
    EXPECT_EQ(addressOf(map, 2), two);
}

// What forEach hands is the value held, not a copy of it: the one visit reaches
TEST(LinkedMap, ForEachHandsTheValueVisitReaches)
{
    const LinkedMap<Ui::id_t, int> map = oneTwoThree();
    std::vector<const void *>      walked;
    map.forEach([&walked](Ui::id_t /*key*/, const int & value) { walked.emplace_back(&value); });
    EXPECT_EQ(walked, (std::vector<const void *> { addressOf(map, 1), addressOf(map, 2), addressOf(map, 3) }));
}

// Negative: an empty map calls nothing, on either side
TEST(LinkedMap, ForEachOnAnEmptyMapCallsNothing)
{
    LinkedMap<Ui::id_t, int>         map;
    const LinkedMap<Ui::id_t, int> & readOnly = map;
    int                              calls    = 0;
    map.forEach([&calls](Ui::id_t /*key*/, int & /*value*/) { ++calls; });
    readOnly.forEach([&calls](Ui::id_t /*key*/, const int & /*value*/) { ++calls; });
    EXPECT_EQ(calls, 0);
}

// Negative: a removed key is walked no more, nor one removed down to none
TEST(LinkedMap, ForEachSkipsWhatWasRemoved)
{
    LinkedMap<Ui::id_t, int> map = oneTwoThree();
    map.remove(1);
    map.remove(3);
    EXPECT_EQ(walkOf(map), (std::vector<std::pair<Ui::id_t, int>> { { 2, 200 } }));
    map.remove(2);
    EXPECT_TRUE(walkOf(map).empty());
}

// A string key comes with its value as an id does
TEST(LinkedMap, ForEachHandsStringKeys)
{
    LinkedMap<Ui::key_t, int> map;
    map.add("b", 2);
    map.add("a", 1);
    std::vector<std::pair<Ui::key_t, int>> walked;
    map.forEach([&walked](const Ui::key_t & key, int value) { walked.emplace_back(key, value); });
    EXPECT_EQ(walked, (std::vector<std::pair<Ui::key_t, int>> { { "b", 2 }, { "a", 1 } }));
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

// A copy walks nodes of its own: a change through it leaves the source's walk as it was
TEST(LinkedMap, ACopyWalksItsOwnValues)
{
    const LinkedMap<Ui::id_t, int> original = oneTwoThree();
    LinkedMap<Ui::id_t, int>       copy     = original;
    copy.forEach([](Ui::id_t /*key*/, int & value) { value = 0; });
    EXPECT_EQ(walkOf(copy), (std::vector<std::pair<Ui::id_t, int>> { { 1, 0 }, { 2, 0 }, { 3, 0 } }));
    EXPECT_EQ(walkOf(original), (std::vector<std::pair<Ui::id_t, int>> { { 1, 100 }, { 2, 200 }, { 3, 300 } }));
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
    tabs.add(7, Ui::tab_t { .id = 7, .label = "Model A", .isActive = true });
    tabs.add(8, Ui::tab_t { .id = 8, .label = "new 1" });

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

TEST(LinkedMap, VisitOrAddCreatesOnceAndJoinsOrder)
{
    LinkedMap<Ui::id_t, std::vector<int>> map;
    map.visitOrAdd(5, [](std::vector<int> & held) { held.emplace_back(1); });
    map.visitOrAdd(5, [](std::vector<int> & held) { held.emplace_back(2); }); // same key: appends, does not replace

    std::vector<int> five;
    ASSERT_TRUE(map.visit(5, [&five](const std::vector<int> & held) { five = held; }));
    EXPECT_EQ(five, (std::vector<int> { 1, 2 }));
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 5 })); // joined the order exactly once
}

// A key held already: its value changed in place, and the key kept where it stands
TEST(LinkedMap, VisitOrAddVisitsTheHeldValue)
{
    LinkedMap<Ui::id_t, int> map    = oneTwoThree();
    const void *             before = addressOf(map, 1);
    map.visitOrAdd(1, [](int & value) { value = 100; });
    EXPECT_EQ(valueOf(map, 1), 100);
    EXPECT_EQ(addressOf(map, 1), before);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2, 3 }));
}

// A value that throws as its node is built leaves no key behind: the map as it was, and the
// key free to add again
TEST(LinkedMap, AFailedAddLeavesNoKey)
{
    LinkedMap<Ui::id_t, thrower_t> map;
    EXPECT_THROW(map.add(1, thrower_t { true }), std::runtime_error);
    EXPECT_FALSE(map.contains(1));
    EXPECT_TRUE(map.empty());
    map.add(1, thrower_t {});
    EXPECT_TRUE(map.contains(1));
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1 }));
}

// ============================================================================
// emplace - built in its node from constructor arguments
// ============================================================================

// Built from its arguments, at the end
TEST(LinkedMap, EmplaceBuildsFromItsArgumentsAtTheEnd)
{
    LinkedMap<Ui::id_t, std::string> map;
    map.add(1, std::string("a"));
    map.emplace(2, 3U, 'x');
    std::string two;
    ASSERT_TRUE(map.visit(2, [&two](const std::string & value) { two = value; }));
    EXPECT_EQ(two, "xxx");
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2 }));
}

// Built where it is held and never moved there: a value whose move throws comes in whole
TEST(LinkedMap, EmplaceNeverMovesTheValue)
{
    LinkedMap<Ui::id_t, thrower_t> map;
    EXPECT_NO_THROW(map.emplace(1, true));
    bool isThrowing = false;
    ASSERT_TRUE(map.visit(1, [&isThrowing](const thrower_t & value) { isThrowing = value.isThrowing; }));
    EXPECT_TRUE(isThrowing);
}

// A value that can be neither copied nor moved is held, removed, and emptied away whole
TEST(LinkedMap, EmplaceHoldsAValueThatCannotMove)
{
    LinkedMap<Ui::id_t, std::atomic<int>> map;
    map.emplace(1, 7);
    map.emplace(2, 8);
    int two = 0;
    ASSERT_TRUE(map.visit(2, [&two](const std::atomic<int> & value) { two = value.load(); }));
    EXPECT_EQ(two, 8);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2 }));
    map.remove(1);
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 2 }));
    map.clear();
    EXPECT_TRUE(map.empty());
}

// Negative: a key already held keeps its value and its place, the arguments unused
TEST(LinkedMap, EmplaceOfAHeldKeyChangesNothing)
{
    LinkedMap<Ui::id_t, std::string> map;
    map.emplace(1, 1U, 'a');
    map.emplace(2, 1U, 'b');
    map.emplace(1, 5U, 'z');
    std::string one;
    ASSERT_TRUE(map.visit(1, [&one](const std::string & value) { one = value; }));
    EXPECT_EQ(one, "a");
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1, 2 }));
}

// Negative: a value that throws as it is built leaves no key behind, and the key free to
// build again
TEST(LinkedMap, AFailedEmplaceLeavesNoKey)
{
    LinkedMap<Ui::id_t, std::bitset<8>> map;
    EXPECT_THROW(map.emplace(1, std::string("2")), std::invalid_argument);
    EXPECT_FALSE(map.contains(1));
    EXPECT_TRUE(map.empty());
    map.emplace(1, std::string("101"));
    std::uint64_t one = 0;
    ASSERT_TRUE(map.visit(1, [&one](const std::bitset<8> & value) { one = value.to_ullong(); }));
    EXPECT_EQ(one, std::uint64_t { 5 });
    EXPECT_EQ(keysOf(map), (std::vector<Ui::id_t> { 1 }));
}

// Built in its node, so a value that cannot move is held directly
TEST(LinkedMap, VisitOrAddHoldsAValueThatCannotMove)
{
    LinkedMap<Ui::id_t, std::atomic<int>> map;
    map.visitOrAdd(1, [](std::atomic<int> & value) { value.store(7); });
    map.visitOrAdd(2, [](std::atomic<int> & value) { value.store(8); });
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

    EXPECT_EQ(groupOf(children, 1), (std::vector<Ui::id_t> { 10, 11 }));
    EXPECT_EQ(groupOf(children, 2), (std::vector<Ui::id_t> { 20 }));
    EXPECT_EQ(children.size(), 2U); // distinct keys, not values
    EXPECT_EQ(keysOf(children), (std::vector<Ui::id_t> { 1, 2 }));
}

// A caller walks a group without asking first whether the key is held: none, nothing walked
TEST(Index, AbsentKeyVisitsNothing)
{
    Ui::Index<Ui::id_t, Ui::id_t> children;
    children.add(1, 10);

    std::size_t walked = 0;
    EXPECT_FALSE(children.visit(999, [&walked](Ui::id_t /*child*/) { ++walked; }));
    EXPECT_EQ(walked, 0U);
    EXPECT_FALSE(children.contains(999));
}

// What visit returns is whether the key is held, as contains says
TEST(Index, VisitSaysTheKeyIsHeld)
{
    Ui::Index<Ui::id_t, Ui::id_t> children;
    children.add(1, 10);
    EXPECT_TRUE(children.visit(1, [](Ui::id_t /*child*/) { }));
    EXPECT_TRUE(children.contains(1));
}
