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
 * @file TestMenuDepthCap.cpp
 * @brief Pins the menu depth cap: --menu-max-depth N counts the menu bar as
 *        level 1, so N allows N-1 nesting levels below a bar entry (deeper
 *        children are dropped, no crash), and a malformed nonpositive config
 *        value falls back to the default instead of gutting every submenu.
 *        Pure C++ - builds menu JSON in memory.
 */

#include "menustorefixture.h"
#include "nlohmann/json.hpp"
#include "ui/res/store/layoutstore.h"
#include "ui/res/type/layout.h"
#include "ui/res/type/menu.h"

#include <functional>
#include <gtest/gtest.h>
#include <string>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test/data"
#endif

namespace {

// A single-child chain nested `levels` deep: L1 -> L2 -> ... -> Llevels.
nlohmann::json nestedMenuJson(int levels)
{
    nlohmann::json item = { { "label", "L" + std::to_string(levels) } };
    for (int i = levels - 1; i >= 1; --i) {
        nlohmann::json parent = { { "label", "L" + std::to_string(i) } };
        parent["submenus"]    = nlohmann::json::array({ item });
        item                  = parent;
    }
    return item;
}

// Length of the single-child chain actually parsed.
int chainDepth(const Ui::Res::Type::menu_t & root)
{
    int                                                 depth = 1;
    std::reference_wrapper<const Ui::Res::Type::menu_t> node  = root;
    while (!node.get().items.empty()) {
        node = std::cref(node.get().items.front());
        ++depth;
    }
    return depth;
}

// The cap counts the bar, so "--menu-max-depth: 3" leaves 2 levels for the chain
// below a bar entry: a 12-deep chain parses to the dropdown row and its submenu,
// and the child that would sit at level 4 is dropped (no crash).
TEST(MenuDepthCap, CapCountsTheBarAsLevelOne)
{
    TestSupport::menu_store_fixture_t fx;
    ASSERT_EQ(fx.layout.layout().menuMaxDepth, 3); // default under test
    const Ui::Res::Type::menu_t root = fx.store.parseMenuItem(nestedMenuJson(12),
                                                              Ui::Res::Store::MenuStore::DROPDOWN_LEVEL);
    EXPECT_EQ(chainDepth(root), fx.layout.layout().menuMaxDepth - 1);
}

// A chain that fits under the cap is untouched.
TEST(MenuDepthCap, BelowCapParsesFully)
{
    TestSupport::menu_store_fixture_t fx;
    const Ui::Res::Type::menu_t       root = fx.store.parseMenuItem(nestedMenuJson(2),
                                                              Ui::Res::Store::MenuStore::DROPDOWN_LEVEL);
    EXPECT_EQ(chainDepth(root), 2);
}

// Malformed "--menu-max-depth: 0" must not gut every submenu: nonpositive
// parse falls back to the default cap.
TEST(MenuDepthCap, ZeroConfigFallsBackToDefault)
{
    Ui::Res::Store::LayoutStore layout;
    (void)layout.load(TEST_DATA_DIR "/layoutdepthzero.json");
    EXPECT_EQ(layout.layout().menuMaxDepth, 3);
}

// A sane configured value still applies.
TEST(MenuDepthCap, ConfiguredValueParses)
{
    Ui::Res::Store::LayoutStore layout;
    (void)layout.load(TEST_DATA_DIR "/layoutdepthfive.json");
    EXPECT_EQ(layout.layout().menuMaxDepth, 5);
}

} // namespace
