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
 * @file TestIconStore.cpp
 * @brief Unit tests for Ui::Res::Store::IconStore alias resolution: aliases
 *        resolve to their .svg, roles resolve via a single hop, `place` is the
 *        entry's own value (Left default, never inherited across the hop), and
 *        unresolvable / malformed / unknown entries resolve to an empty icon.
 *        Driven by the test/data/iconstore.json fixture.
 */

#include "ui/res/store/iconstore.h"

#include <gtest/gtest.h>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test/data"
#endif

namespace {

using Ui::Res::Store::IconStore;
using Ui::Res::Type::IconPlace;

IconStore loadFixture()
{
    IconStore store;
    (void)store.load(TEST_DATA_DIR "/iconstore.json"); // Changed result asserted in its own test
    return store;
}

// Alias (icon already a .svg) resolves to itself; explicit place is kept.
TEST(IconStore, AliasKeepsSvgAndExplicitPlace)
{
    const IconStore store = loadFixture();
    EXPECT_EQ(store.iconDefault("leaf").icon, "leaf.svg");
    EXPECT_EQ(store.iconDefault("leaf").place, IconPlace::Right);
    EXPECT_EQ(store.iconDefault("alias").icon, "alias.svg");
    EXPECT_EQ(store.iconDefault("alias").place, IconPlace::Left); // default
}

// Role (icon names an alias) resolves via a single hop to the alias's .svg.
TEST(IconStore, RoleResolvesViaAlias)
{
    const IconStore store = loadFixture();
    EXPECT_EQ(store.iconDefault("role").icon, "alias.svg");
    EXPECT_EQ(store.iconDefault("rolePlaced").icon, "alias.svg");
    EXPECT_EQ(store.iconDefault("rolePlaced").place, IconPlace::Right); // own place
}

// `place` is NOT inherited across the hop: roleNoInherit -> leaf (.svg) keeps
// Left even though leaf itself is placed Right.
TEST(IconStore, PlaceNotInheritedFromAlias)
{
    const IconStore store = loadFixture();
    EXPECT_EQ(store.iconDefault("roleNoInherit").icon, "leaf.svg");
    EXPECT_EQ(store.iconDefault("roleNoInherit").place, IconPlace::Left);
}

// A role whose target is missing (or not a .svg) resolves to an empty icon.
TEST(IconStore, UnresolvableRoleIsEmpty)
{
    const IconStore store = loadFixture();
    EXPECT_TRUE(store.iconDefault("broken").icon.empty());
}

// `_comment` and non-object entries are skipped; unknown roles are empty.
TEST(IconStore, SkipsCommentNonObjectAndUnknown)
{
    const IconStore store = loadFixture();
    EXPECT_TRUE(store.iconDefault("_comment").icon.empty());
    EXPECT_TRUE(store.iconDefault("bad").icon.empty());
    EXPECT_TRUE(store.iconDefault("nonexistent").icon.empty());
}

// First load reports Icon changed; an identical reload reports no change.
TEST(IconStore, ChangedOnFirstLoadThenStable)
{
    IconStore store;
    EXPECT_EQ(store.load(TEST_DATA_DIR "/iconstore.json"), Ui::Res::Type::Changed::Icon);
    EXPECT_EQ(store.load(TEST_DATA_DIR "/iconstore.json"), Ui::Res::Type::Changed::None);
}

// role -> role -> alias chains: resolution is a SINGLE hop by contract, so the
// outer role (whose target is itself a role) must resolve to empty - and must
// do so deterministically, independent of map iteration order. The fixture
// holds chains of BOTH alphabetical orientations, so no consistent iteration
// order visits every outer before its mid: order-dependent in-place resolution
// always leaks a mid's freshly resolved .svg through at least one outer.
TEST(IconStore, RoleToRoleChainIsEmptyRegardlessOfOrder)
{
    IconStore store;
    (void)store.load(TEST_DATA_DIR "/iconstorerolechain.json");
    for (const char * mid : { "zMid1", "zMid2", "zMid3", "zMid4", "aMid5", "aMid6", "aMid7", "aMid8" }) {
        EXPECT_FALSE(store.iconDefault(mid).icon.empty()) << mid; // normal single hop resolves
    }
    for (const char * outer : { "aOut1", "aOut2", "aOut3", "aOut4", "zOut5", "zOut6", "zOut7", "zOut8" }) {
        EXPECT_TRUE(store.iconDefault(outer).icon.empty()) << outer; // second hop must not happen
    }
}

// A missing file leaves the store unchanged.
TEST(IconStore, MissingFileIsNoChange)
{
    IconStore store;
    EXPECT_EQ(store.load(TEST_DATA_DIR "/does-not-exist.json"), Ui::Res::Type::Changed::None);
}

} // namespace
